#include "watcher.h"
#include "worker.h"
#include <iostream>
#include <sys/inotify.h>
#include <cstdlib>
#include <string>
#include <vector>

using namespace std;

#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (1024 * (EVENT_SIZE + 16))

Watcher::Watcher(Worker * w)
{
    worker = w;
    worker->writeLog("Watcher constructor");
}
Watcher::~Watcher()
{
    worker->writeLog("Watcher deconstructor");
}
void Watcher::start()
{
    //setup watches
    int length, i = 0, n = 0, fd;
    char buffer[EVENT_BUF_LEN];
    fd = inotify_init(); //create inotify instance
    if (fd < 0)
    {
        worker->writeLog("ERROR: watcher > could not create inotify instance");
        exit(EXIT_FAILURE);
    }

    //add watches
    int w1,w2; //TODO: use array of folders from config file
    vector<xfolder_t>::iterator it;
    settings_t * s = worker->getSettings();
    for(it = s->xFolders.begin(); it != s->xFolders.end(); it++)
    {
        it->watchdescriptor = inotify_add_watch(fd, (char * )it->path.c_str(), 
            IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO);
    }

    // watch folders
    while(1)
    {
        length = read(fd, buffer, EVENT_BUF_LEN); //read inotify events, blocks until event
        if (length < 0)
        {
            worker->writeLog("ERROR: watcher > could not read from inotify");
            exit(EXIT_FAILURE);
        }
        while (i < length) //loop through events
        {
            struct inotify_event * event = (struct inotify_event *) &buffer[i];
            n = i + EVENT_SIZE + event->len; //index of next event
            struct inotify_event * next = (struct inotify_event *) &buffer[n];
            if (event->len)
            {
                //TODO: find xfolder from event->wd
                if ((event->cookie == next->cookie) && (event->wd == next->wd)) //is rename
                {
                    worker->writeLog("RENAME EVENT: from: " +(string) event->name + 
                        " to: " + (string) next->name);
                    //TODO: send rename task to worker
                    i += EVENT_SIZE + next->len; //skip next line
                }
                else if ((event->mask & IN_CREATE) || (event->mask & IN_MOVED_TO)) //is add
                {
                    worker->writeLog("ADD EVENT: file: " +(string) event->name);
                    //TODO: send add task to worker
                }
                else if ((event->mask & IN_DELETE) || (event->mask & IN_MOVED_FROM)) 
                    //is delete
                {
                    worker->writeLog("DELETE EVENT: file: " + (string)  event->name);
                }
            }
            i += EVENT_SIZE + event->len;
        }
        i = 0;
    }
    //TODO: use array of folders from config file
    //TODO: code will never reach this line
    inotify_rm_watch(fd, w1);
    inotify_rm_watch(fd, w2);
    close(fd);
}
