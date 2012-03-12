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
                        (*fl).actualCopies = poolfiles.size();
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

int Checker::repair(bool prompt)
{
    vector<flexdir_t>::iterator fit;
    vector<flexfile_t>::iterator ffit;
    vector<pooldir_t> pooldirs;
    vector<pooldir_t>::iterator pit;
    vector<poolfile_t>::iterator pfit;
    vector<poolfile_t> poolfiles;
    settings_t * s = worker->getSettings();
    string rmpath, linktarget, pfpath, ffpath, answer = "y";
    for (fit = s->flexdirs.begin(); fit != s->flexdirs.end(); fit++)    
    {
        for (ffit = fit->files.begin(); ffit != fit->files.end(); ffit++)
        {
            string path = ffit->x_path + "/" + ffit->name;
            switch (ffit->role)
            {
                case NEW:
                    answer = "y";
                    if (prompt == true)
                    {
                        cout << "\nFile: " << path << " is not a symbolic link."<< endl;
                        cout << "Add to pool? (y=yes, n=no, a=all, q=quit) ";
                        cin >> answer;
                    }
                    if (answer == "q" || answer == "Q")
                    {
                        return 1;
                    }
                    if (answer == "a" || answer == "A")
                    {
                        prompt = false;
                    }
                    if (answer == "y" || answer == "Y")
                    {
                        if (mutex != NULL)
                        {
                            pthread_mutex_lock(mutex);
                        }
                        worker->addTask(ADD, path, " ");
                        if (mutex != NULL)
                        {
                            pthread_mutex_unlock(mutex);
                        }
                    }
                    break;
                case COPIES:
                    answer = "y";
                    if (prompt == true)
                    {
                        cout << "\nFile: " << path << " has "<< ffit->actualCopies;
                        cout << " instead of " << fit->copies << " copies" << endl;
                        cout << "Repair? (y=yes, n=no, a=all, q=quit) ";
                        cin >> answer;
                    }
                    if (answer == "q" || answer == "Q")
                    {
                        return 1;
                    }
                    if (answer == "a" || answer == "A")
                    {
                        prompt = false;
                    }
                    if (answer == "y" || answer == "Y")
                    {
                        if (fit->copies < ffit->actualCopies)
                        {
                            //delete copies
                            poolfiles = worker->getPoolFiles(&(*ffit));
                            for (int i = 0; i < (ffit->actualCopies - fit->copies); i++)
                            {
                                for (pfit = poolfiles.begin(); pfit != poolfiles.end(); pfit++)
                                {
                                    if (pfit->role != PRIMARY)
                                    {
                                        if (mutex != NULL)
                                        {
                                            pthread_mutex_lock(mutex);
                                        }
                                        rmpath = pfit->p_path + pfit->x_path + "/" + pfit->name;
                                        worker->addTask(DELETE, rmpath, " ");
                                        if (mutex != NULL)
                                        {
                                            pthread_mutex_unlock(mutex);
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                        else
                        {
                            //add copies
                            pooldirs = worker->getNdirs(fit->copies);
                            for (pit = pooldirs.begin(); pit != pooldirs.end(); pit++)
                            {
                                if (ffit->actualCopies < fit->copies)
                                {
                                    pfpath = pit->path + fit->path + "/" + ffit->name;
                                    if (worker->getFileExists((char*)pfpath.c_str()) == false)
                                    {
                                        ffpath = ffit->x_path + "/" + ffit->name;
                                        linktarget = worker->getLinkTarget((char*)ffpath.c_str());
                                        if (mutex != NULL)
                                        {
                                            pthread_mutex_lock(mutex);
                                        }
                                        worker->addTask(SYNC, linktarget, pfpath);
                                        if (mutex != NULL)
                                        {
                                            pthread_mutex_unlock(mutex);
                                        }
                                        ffit->actualCopies++;
                                    }
                                }
                                else
                                {
                                    break;
                                }
                            }
                        }
                    }                  
                    break;
                case ORPHAN:
                    answer = "y";
                    if (prompt == true)
                    {
                        cout << "\nFile: " << path << " link target does not exist."<< endl;
                        cout << "Repair? (y=yes, n=no, a=all, q=quit) ";
                        cin >> answer;
                    }
                    if (answer == "q" || answer == "Q")
                    {
                        return 1;
                    }
                    if (answer == "a" || answer == "A")
                    {
                        prompt = false;
                    }
                    if (answer == "y" || answer == "Y")
                    {
                        //link target does not exist, check for secondary copies
                        poolfiles = worker->getPoolFiles(&(*ffit));
                        hier was ik
                    }
                    break;
                default:
                    break;
            }
        }
    }
    for (pit = s->pooldirs.begin(); pit != s->pooldirs.end(); pit++)
    {
        for (pfit = pit->files.begin(); pfit != pit->files.end(); pfit++)
        {
            if (pfit->role == ORPHAN)
            {
                string linkPath = pfit->x_path + "/" + pfit->name;
                string pfPath = pit->path + pfit->x_path + "/" + pfit->name;
                if (worker->getFileExists((char *)linkPath.c_str()) == false)
                {
                    answer = "y";
                    if (prompt == true)
                    {
                        cout << "\nPoolfile: " << pfPath << " is not linked."<< endl;
                        cout << "Create link? (y=yes, n=no, a=all, q=quit) ";
                        cin >> answer;
                    }
                    if (answer == "q" || answer == "Q")
                    {
                        return 1;
                    }
                    if (answer == "a" || answer == "A")
                    {
                        prompt = false;
                    }
                    if (answer == "y" || answer == "Y")
                    {
                        if (mutex != NULL)
                        {
                            pthread_mutex_lock(mutex);
                        }
                        rmpath = pfit->p_path + pfit->x_path + "/" + pfit->name;
                        worker->addTask(LINK, linkPath, pfPath);
                        //TODO: check if number of copies are correct
                        if (mutex != NULL)
                        {
                            pthread_mutex_unlock(mutex);
                        }
                    }
                }
            }
        }
    }
    if (condition != NULL)
    {
        pthread_cond_signal(condition);
    }
    else
    {
        settings_t * s = worker->getSettings();
        while(s->tasks.size() > 0)
        {
            task_t task = s->tasks.front();
            s->tasks.pop();
            worker->doTask(&task);
        }
   }
}

void Checker::resync()
{
    vector<flexdir_t>::iterator fit;
    vector<poolfile_t> poolfiles;
    vector<poolfile_t>::iterator pfit;
    vector<flexfile_t>::iterator ffit;
    settings_t * s = worker->getSettings();
    Worker * w = worker;
    w->writeLog("RESYNC: start synchronizing copies");
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
                                if (mutex != NULL)
                                {
                                    pthread_mutex_lock(mutex);
                                }
                                w->addTask(SYNC, linktarget, ppath);
                                if (mutex != NULL)
                                {
                                    pthread_mutex_unlock(mutex);
                                }
                            }
                        }
                    }
                    else
                    {
                        w->writeLog("RESYNC: FAILED no poolfiles found for: " + path);
                    }
                }
                else
                {
                    w->writeLog("RESYNC: FAILED linktarget does not exist: " + linktarget);
                }
            }
            else
            {
                w->writeLog("RESYNC: not a symlink: " + path);
            }
        }
    }
    if (condition != NULL)
    {
        pthread_cond_signal(condition);
    }
    else
    {
        settings_t * s = w->getSettings();
        while(s->tasks.size() > 0)
        {
            task_t task = s->tasks.front();
            s->tasks.pop();
            w->doTask(&task);
        }
    }
    w->writeLog("RESYNC: finished synchronizing copies");
}
