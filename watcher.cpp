#include "watcher.h"
#include "worker.h"
#include <iostream>
#include <sys/inotify.h>
#include <cstdlib>
#include <string>
#include <pthread.h>
#include <vector>

using namespace std;

#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (1024 * (EVENT_SIZE + 16))

Watcher::Watcher(Worker * w)
{
    worker = w;
}

Watcher::~Watcher()
{
}

void Watcher::start(pthread_mutex_t * m, pthread_cond_t * c)
{
    mutex = m;
    condition = c;
    //setup watches
    int length, i = 0, n = 0, fd;
    char buffer[EVENT_BUF_LEN];
    fd = inotify_init(); //create inotify instance
    if (fd < 0)
    {
        worker->writeLog("WATCHER ERROR: could not create inotify instance");
        exit(EXIT_FAILURE);
    }
    //add watches
    vector<flexdir_t>::iterator it;
    settings_t * s = worker->getSettings();
    for(it = s->flexdirs.begin(); it != s->flexdirs.end(); it++)
    {
        it->watchdescriptor = inotify_add_watch(fd, (char * )it->path.c_str(), 
            IN_CLOSE_WRITE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO );
    }
    // watch dirs
    while(1)
    {
        //DEBUG: worker->writeLog("WATCHER waiting...");
        length = read(fd, buffer, EVENT_BUF_LEN); //read inotify events, blocks until event
        if (length < 0)
        {
            worker->writeLog("WATCHER ERROR: could not read from inotify");
            exit(EXIT_FAILURE);
        }
        while (i < length) //loop through events
        {
            struct inotify_event * event = (struct inotify_event *) &buffer[i];
            n = i + EVENT_SIZE + event->len; //index of next event
            struct inotify_event * next = (struct inotify_event *) &buffer[n];
            if (event->len)
            {
                string fPath = getPathFromWatchDescriptor(event->wd);
                string fdPath = fPath + "/" + (string)event->name;
                if (fPath != "NOT FOUND")
                {
                    if ((event->cookie == next->cookie) && (event->wd == next->wd)
                            && (event->mask & IN_MOVED_FROM) && (next->mask & IN_MOVED_TO)) //is rename
                    {
                        i += EVENT_SIZE + next->len; //skip next line
                        string toPath = fPath + "/" + (string) next->name;
                        worker->writeLog("WATCHER EVENT: rename from: " +(string) event->name + " to: " + (string) next->name);
                        pthread_mutex_lock(mutex);
                        worker->addTask(RENAME, fdPath, toPath);
                        pthread_mutex_unlock(mutex);
                    }
                    else if ((event->mask & IN_CLOSE_WRITE) || (event->mask & IN_MOVED_TO)) //is add
                    {
                        if (worker->getIsLink((char*)fdPath.c_str()) == true) //symlink is created by worker during ADD-task
                        {
                            //DEBUG: worker->writeLog("WATCHER EVENT: add  SKIPPED: file is symlink");
                        }
                        else
                        {
                            worker->writeLog("WATCHER EVENT: add file: " +(string) event->name);
                            pthread_mutex_lock(mutex);
                            worker->addTask(ADD, fdPath, " " );
                            pthread_mutex_unlock(mutex);
                        }                        
                    }
                    else if ((event->mask & IN_DELETE) || (event->mask & IN_MOVED_FROM)) //is delete
                    {
                        settings_t * s = worker->getSettings();
                        while(s->deleteWaitFlag == true) 
                            //check if wait-flag is set by worker (during ADD/RENAME action, a DELETE event occurs)
                        {
                            sleep(0);
                        }
                        if ((worker->getFileExists((char*)fdPath.c_str()) == true) && (worker->getIsLink((char*)fdPath.c_str()) == true))
                        {
                            //DEBUG: worker->writeLog("WATCHER EVENT: delete SKIPPED file deleted by worker");
                        }
                        else
                        {
                            worker->writeLog("WATCHER EVENT: remove file: " + (string)  event->name);
                            pthread_mutex_lock(mutex);
                            worker->addTask(REMOVE, fdPath, " ");
                            pthread_mutex_unlock(mutex);
                        }
                    }
                }
                else
                {
                    worker->writeLog("WATCHER ERROR: could not find path from watch descriptor");
                }
            }
            i += EVENT_SIZE + event->len;
        }
        pthread_cond_signal(condition);
        i = 0;
    }
    //TODO: code will never reach this line
    for(it = s->flexdirs.begin(); it != s->flexdirs.end(); it++)
    {
        inotify_rm_watch(fd, it->watchdescriptor);
    }
    close(fd);
}

string Watcher::getPathFromWatchDescriptor(int wd)
{
    vector<flexdir_t>::iterator it;
    settings_t * s = worker->getSettings();
    for (it = s->flexdirs.begin(); it != s->flexdirs.end(); it++)
    {
        if (it->watchdescriptor == wd)
        {
            return it->path;
        }
    }
    return "NOT FOUND";
}
