#include "checker.h"
#include "worker.h"
#include <iostream>
#include <string>


using namespace std;

Checker::Checker(Worker * w)
{
    worker = w;
    worker->writeLog("Checker constructor");
}
Checker::~Checker()
{
    worker->writeLog("Checker deconstructor");
}
void Checker::start()
{
    while(1)
    {
//        worker->writeLog("checker thread!");
        sleep(10);
    }
}
