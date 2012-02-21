#include <iostream>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <pthread.h>
#include "worker.h"
#include "watcher.h"

using namespace std;

void startThreads();
void testWatcher();
void * watcherThread(void * id);
void * workerThread(void * id);
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition = PTHREAD_COND_INITIALIZER;

Worker w;
Watcher wa(&w);

int main()
{
    startThreads();
    return 0;
}

void startThreads()
{
    pthread_t wat, cht, wot;
    int retwa, retch, retwo;
    retwa = pthread_create(&wat, NULL, watcherThread, (void *) 1);
    retwo = pthread_create(&wot, NULL, workerThread, (void *) 3);
//    testWatcher();
    pthread_join(wat, NULL);
    pthread_join(wot, NULL);
}

void testWatcher()
{
    sleep(1);
    cout << "create testfile" << endl;
    system("touch /home/jos/tmp/testWatcher.txt");
    sleep(1);
    cout << "move testfile" << endl;
    w.actionMoveFile((char*)"/home/jos/tmp/testWatcher.txt", (char*)"/home/jos/tmp/testWatcherRename.txt");
    sleep(1);
    cout << "delete testfile" << endl;
    w.actionDeleteFile((char*)"/home/jos/tmp/testWatcherRename.txt");
    sleep(1);
    exit(0);
}

void * watcherThread(void * id)
{
    wa.start(&mutex, &condition);   
}

void * workerThread(void * id)
{
    w.startWorker(&mutex, &condition);    
}

