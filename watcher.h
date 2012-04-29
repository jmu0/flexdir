#ifndef WATCHER_H
#define WATCHER_H

#include <string>
#include <pthread.h>
#include "worker.h"

using namespace std;

class Watcher
{
    private:
        Worker * worker;
        pthread_mutex_t * mutex;
        pthread_cond_t * condition;
        string getPathFromWatchDescriptor(int wd);
    public:
        Watcher(Worker * w);
        ~Watcher();
        void start(pthread_mutex_t * m, pthread_cond_t * condition);
};

#endif
