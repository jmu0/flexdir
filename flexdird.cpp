#include <iostream>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <pthread.h>
#include "worker.h"
#include "watcher.h"
#include "checker.h"

using namespace std;

void startDaemon();
void startThreads();
void * watcherThread(void * id);
void * checkerThread(void * id);
void * workerThread(void * id);
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition = PTHREAD_COND_INITIALIZER;

Worker w;
Watcher wa(&w);
Checker ch(&w);

int main()
{
    startDaemon();
    return 0;
}

void startDaemon()
{
    pid_t pid, sid;
    //fork process
    pid=fork();
    if (pid < 0)
    {
        exit(EXIT_FAILURE);
    }
    if (pid > 0)
    {
        exit(EXIT_SUCCESS);
    }
    //umask veranderen
    umask(0);
    //create unique session id voor child proces
    sid = setsid();
    if (sid < 0)
    {
        exit(EXIT_FAILURE);
    }
    //working directory veranderen
    if ((chdir("/")) < 0) 
    {
        exit(EXIT_FAILURE);
    }
    //standaard file descriptors sluiten
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    //start threads
    startThreads();
    exit(EXIT_SUCCESS);
}

void startThreads()
{
    pthread_t wat, cht, wot;
    int retwa, retch, retwo;
    retwa = pthread_create(&wat, NULL, watcherThread, (void *) 1);
    retch = pthread_create(&cht, NULL, checkerThread, (void *) 2);
    retwo = pthread_create(&wot, NULL, workerThread, (void *) 3);
    pthread_join(wat, NULL);
    pthread_join(cht, NULL);
    pthread_join(wot, NULL);
}

void * watcherThread(void * id)
{
    wa.start(&mutex, &condition);   
}

void * checkerThread(void * id)
{
    ch.start(&mutex, &condition);
}

void * workerThread(void * id)
{
    w.startWorker(&mutex, &condition);    
}

