#include "checker.h"
#include "worker.h"
#include <iostream>
#include <string>
#include <pthread.h>

using namespace std;

Checker::Checker(Worker * w)
{
    worker = w;
}
Checker::~Checker()
{
}
void Checker::start(pthread_mutex_t * m, pthread_cond_t * c)
{
    mutex = m;
    condition = c;
    while(1)
    {
        sleep(10);
    }
}
