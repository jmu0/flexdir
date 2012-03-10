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
    settings_t * s = worker->getSettings();
    Worker * w = worker;
    vector<flexdir_t>::iterator it;
    vector<flexfile_t>::iterator fl;
    vector<poolfile_t> poolfiles;
    vector<poolfile_t>::iterator pfi;
    for (it = s->flexdirs.begin(); it != s->flexdirs.end(); it++)
    {
        for(fl = (*it).files.begin(); fl != (*it).files.end(); fl++)
        {
            string p = (*it).path + "/" + (*fl).name;
            if (w->getIsLink((char*)p.c_str()))
            {
                string target = w->getLinkTarget((char*)p.c_str());
                poolfiles = worker->getPoolFiles(&(*fl));
                for(pfi = poolfiles.begin(); pfi != poolfiles.end(); pfi++)
                {
                   string pp = (*pfi).p_path + (*pfi).x_path + "/" + (*pfi).name;
                   if (pp == target){
                       setPoolfileRole((*pfi), PRIMARY);
                   }
                   else
                   { 
                       setPoolfileRole((*pfi), SECONDARY);
                   }
                }
                (*fl).role = FLEXFILE;
                if (w->getFileExists((char*)target.c_str()))
                {
                    if (poolfiles.size() != (*it).copies)
                    {
                        (*fl).role = COPIES;
                    }
                }
                else
                {
                    (*fl).role = ORPHAN;
                }


            }
            else
            {
                (*fl).role = NEW;
            }
        }
    }
}

int Checker::setPoolfileRole(poolfile_t poolfile, role_t role)
{
    vector<pooldir_t>::iterator pit;
    vector<poolfile_t>::iterator fit;
    settings_t * s = worker->getSettings();
    for (pit = s->pooldirs.begin(); pit != s->pooldirs.end(); pit++)
    {
        if ((*pit).path == poolfile.p_path)
        {
            for (fit = (*pit).files.begin(); fit != (*pit).files.end(); fit++)
            {
                if (((*fit).x_path == poolfile.x_path) && ((*fit).name == poolfile.name))
                {
                    (*fit).role = role;
                    return 0;
                }
            }
        }
    }
    return 1;
}
