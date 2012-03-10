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
    vector<pooldir_t>::iterator pdi;
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
    for (pdi = s->pooldirs.begin(); pdi != s->pooldirs.end(); pdi++)
    {
        for (pfi = pdi->files.begin(); pfi != pdi->files.end(); pfi++)
        {
            if (pfi->role == NONE)
            {
                pfi->role = ORPHAN;
            }
        }
    }
}

int Checker::getErrorCount()
{
    vector<flexdir_t>::iterator fit;
    vector<pooldir_t>::iterator pit;
    vector<flexfile_t>::iterator ffit;
    vector<poolfile_t>::iterator pfit;
    settings_t * s = worker->getSettings();
    int errors = 0;
    for (fit = s->flexdirs.begin(); fit != s->flexdirs.end(); fit++)
    {
        for (ffit = fit->files.begin(); ffit != fit->files.end(); ffit++)
        {
            if ((ffit->role == NONE) || (ffit->role == NEW) || (ffit->role == COPIES) || (ffit->role == ORPHAN))
            {
                errors++;
            }
        }
    }
    for (pit = s->pooldirs.begin(); pit != s->pooldirs.end(); pit++)
    {
        for (pfit = pit->files.begin(); pfit != pit->files.end(); pfit++)
        {
            if ((pfit->role == NONE) || (pfit->role == ORPHAN))
            {
                errors++;
            }
        }
    }
    return errors;
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

void Checker::repair(bool prompt)
{
    cout << "REPAIR !!" << endl;
}

void Checker::resync(bool verbose)
{
    vector<flexdir_t>::iterator fit;
    vector<poolfile_t> poolfiles;
    vector<poolfile_t>::iterator pfit;
    vector<flexfile_t>::iterator ffit;
    settings_t * s = worker->getSettings();
    Worker * w = worker;
    if (verbose == true)
    {
        cout << "start synchronizing copies..." << endl;
    }
    for (fit = s->flexdirs.begin(); fit != s->flexdirs.end(); fit++)
    {
        for (ffit = fit->files.begin(); ffit != fit->files.end(); ffit++)
        {
            string path = ffit->x_path + "/" + ffit->name;
            string ppath = "";
            if (w->getIsLink((char*)path.c_str()))
            {
                string linktarget = w->getLinkTarget((char*)path.c_str());
                if (w->getFileExists((char*)linktarget.c_str()))
                {
                    poolfiles = w->getPoolFiles(&(*ffit));
                    if (poolfiles.size() > 0)
                    {
                        for (pfit = poolfiles.begin(); pfit != poolfiles.end(); pfit++)
                        {
                            ppath = pfit->p_path + pfit->x_path + "/" + pfit->name;
                            if (linktarget != ppath)
                            {
                                if (w->actionSyncFile((char*)linktarget.c_str(), (char*)ppath.c_str()) == 0)
                                {
                                    if (verbose == true)
                                    {
                                        cout << "syncronized " << linktarget << " to " << ppath << endl;
                                    }
                                }
                                else
                                {
                                    if (verbose == true)
                                    {
                                        cout << "FAILED synchrinizing " << linktarget << " to  " << ppath << endl;
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        if (verbose == true)
                        {
                            cout << "FAILED no poolfiles found for: " << path << endl;
                        }
                    }

                }
                else
                {
                    if (verbose == true)
                    {
                        cout << "FAILED linktarget does not exist: " << linktarget << endl;
                    }
                }
            }
            else
            {
                if (verbose == true)
                {
                    cout << "not a symlink: " << path << endl;
                }
            }

        }
    }
    if (verbose == true)
    {
        cout << "finished synchronizing copies !" << endl;
    }
}
