#ifndef CHECKER_H
#define CHECKER_H
#include "worker.h"

using namespace std;

class Checker
{
    private:
        Worker * worker;
    public:
        Checker(Worker * w);
        ~Checker();
        void start();
};

#endif
