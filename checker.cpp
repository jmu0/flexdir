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
void Checker::analyze()
{
    cout << "Analyzing..." << endl;
    settings_t * s = worker->getSettings();
    vector<flexdir_t>::iterator it;
    vector<flexfile_t>::iterator fl;
    vector<poolfile_t> poolfiles;
    vector<poolfile_t>::iterator pfi;
    for (it = s->flexdirs.begin(); it != s->flexdirs.end(); it++)
    {
        cout << (*it).path << endl;
        for(fl = (*it).files.begin(); fl != (*it).files.end(); fl++)
        {
            cout << "  " << (*fl).name << endl;
            poolfiles = worker->getPoolFiles(&(*fl));
            for(pfi = poolfiles.begin(); pfi != poolfiles.end(); pfi++)
            {
                cout << "    " << (*pfi).name << endl;
            }

            
        }
    }
    cout << "done!" << endl;
}
