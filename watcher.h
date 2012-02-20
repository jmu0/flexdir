#ifndef WATCHER_H
#define WATCHER_H

#include <string>
#include "worker.h"

using namespace std;

class Watcher
{
    private:
        Worker * worker;
    public:
        Watcher(Worker * w);
        ~Watcher();
        void start();
};

#endif
