//nog een test
//test push
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
    pthread_t wt, ct;
    int retw, retc;
    retw = pthread_create(&wt, NULL, watcherThread, (void *) 1);
    retc = pthread_create(&ct, NULL, checkerThread, (void *) 2);
    pthread_join(wt, NULL);
    pthread_join(ct, NULL);

}
void * watcherThread(void * id)
{
    wa.start();   
}
void * checkerThread(void * id)
{
    ch.start();
}

