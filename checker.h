#ifndef CHECKER_H
#define CHECKER_H
#include "worker.h"
#include <pthread.h>

using namespace std;

class Checker
{
    private:
        Worker * worker;
        pthread_mutex_t * mutex;
        pthread_cond_t * condition;
        int setPoolfileRole(poolfile_t poolfile, role_t role);
        void getNextJob(string * job, int * timeToNextJob);
        void repairCopies(flexfile_t * ff, flexdir_t * fd);
        void doRepair();
    public:
        Checker(Worker * w);
        ~Checker();
        void debug(); //TODO: remove this function
        void start(pthread_mutex_t * m, pthread_cond_t * c);
        void analyze();
        int getErrorCount();
        int repair(bool prompt);
        void resync();
};

#endif
