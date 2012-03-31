#include "checker.h"
#include "worker.h"
#include <iostream>
#include <sstream> //stringstream
#include <string>
#include <pthread.h>
#include <time.h> //getNextJob

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
    string job = "nojob";
    int timeToNextJob = 1;
    while(1)
    {
        getNextJob(&job, &timeToNextJob);
        sleep(timeToNextJob);
        if (job == "repair")
        {
            worker->writeLog("CHECKER: start repair...");
            doRepair();
            worker->writeLog("CHECKER: finished repair...");
        }
        else if (job == "resync")
        {
            worker->loadFileStructure();
            resync();
        }
    }
}

void Checker::doRepair()
{
    worker->loadFileStructure();
    analyze();
    settings_t * s = worker->getSettings();
    int errors = getErrorCount();
    if(errors > 0)
    {
        if (errors <= s->alarmThreshold)
        {
            if(repair(false)==0)
            {
                doRepair();
            }
        }
        else
        {
            worker->writeLog("CHECKER: ALARM number of errors larger then alarm threshhold!");
        }
    }
}

void Checker::debug()
{
    //TODO: remove this function, used for testing new functions
    string job = "no";
    int t = 0;
    getNextJob(&job, &t);
    cout << "job: " << job << endl;
    cout << "time: " << t << endl;
}

void Checker::getNextJob(string * job, int * timeToNextJob)
{
    *job = "nojob";
    *timeToNextJob = 86400;
    time_t now;
    time_t next;
    int repTime;
    struct tm * nowInfo;
    struct tm * tInfo;
    string t, h, m;
    settings_t * settings = worker->getSettings();
    time(&now);
    nowInfo = localtime(&now);
    if (settings->dailySync == true)
    {
        *job = "resync";
        tInfo = localtime(&now);
        t = settings->dailySyncTime;
        h = t.substr(0, t.find_first_of(':'));
        m = t.substr(t.find_first_of(':') + 1);
        istringstream hss(h);
        istringstream mss(m);
        if (!(hss >> tInfo->tm_hour))
        {
            *job = "nojob";
        }
        if (!(mss >> tInfo->tm_min))
        {
            *job = "nojob";
        }
        next = mktime(tInfo);
        *timeToNextJob = next - now;
        if (now > next)
        {
            *timeToNextJob += 86400;
        }
    }
    if (settings->dailyRepair == true)
    {
        string repJob = "repair";
        tInfo = localtime(&now);
        t = settings->dailyRepairTime;
        h = t.substr(0, t.find_first_of(':'));
        m = t.substr(t.find_first_of(':') + 1);
        istringstream hsr(h);
        istringstream msr(m);
        if (!(hsr >> tInfo->tm_hour))
        {
            repJob = "nojob";
        }
        if (!(msr >> tInfo->tm_min))
        {
            repJob = "nojob";
        }
        next = mktime(tInfo);
        repTime = next - now;
        if (now > next)
        {
            repTime += 86400;
        }
        if (repTime < *timeToNextJob)
        {
            *job = repJob;
            *timeToNextJob = repTime;
        }
    }
    if (*timeToNextJob < 1)
    {
        *timeToNextJob = 1;
        *job = "nojob";
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
                        answer = "y";
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
                        answer = "y";
                    }
                    if (answer == "y" || answer == "Y")
                    {
                        repairCopies(&(*ffit), &(*fit));
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
                        answer = "y";
                    }
                    if (answer == "y" || answer == "Y")
                    {
                        //link target does not exist, check for secondary copies
                        poolfiles = worker->getPoolFiles(&(*ffit));
                        ffpath = ffit->x_path + "/" + ffit->name;
                        if (poolfiles.size() > 0)
                        {
                            //change link target and check number of copies
                            pfit = poolfiles.begin();
                            pfpath = pfit->p_path + pfit->x_path + "/" + pfit->name;
                            if (mutex != NULL)
                            {
                                pthread_mutex_lock(mutex);
                            }
                            worker->addTask(DELETE, ffpath, " ");
                            worker->addTask(LINK, ffpath, pfpath);
                            if (mutex != NULL)
                            {
                                pthread_mutex_unlock(mutex);
                            } 
                        }
                        else
                        {
                            //delete link
                            if (mutex != NULL)
                            {
                                pthread_mutex_lock(mutex);
                            }
                            worker->addTask(DELETE, ffpath, " ");
                            if (mutex != NULL)
                            {
                                pthread_mutex_unlock(mutex);
                            }
                        }
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
                //TODO: possibly more copies of the same file flagged as orphan
                //don't ask for every copy
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
                        answer = "y";
                    }
                    if (answer == "y" || answer == "Y")
                    {
                        if (mutex != NULL)
                        {
                            pthread_mutex_lock(mutex);
                        }
                        rmpath = pfit->p_path + pfit->x_path + "/" + pfit->name;
                        worker->addTask(LINK, linkPath, pfPath);
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
    return 0;
}

void Checker::repairCopies(flexfile_t * ff, flexdir_t * fd)
{
    vector<poolfile_t> poolfiles;
    vector<poolfile_t>::iterator pfit;
    vector<pooldir_t> pooldirs;
    vector<pooldir_t>::iterator pit;
    string rmpath, pfpath, ffpath, from;
    poolfiles = worker->getPoolFiles(ff);
    if (poolfiles.size() > 0)
    {
        if (fd->copies < ff->actualCopies)
        {
            //delete copies
            for (int i = 0; i < (ff->actualCopies - fd->copies); i++)
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
            pooldirs = worker->getNdirs(fd->copies);
            pfit = poolfiles.begin();
            from = pfit->p_path + pfit->x_path + "/" + pfit->name;
            for (pit = pooldirs.begin(); pit != pooldirs.end(); pit++)
            {
                if (ff->actualCopies < fd->copies)
                {
                    pfpath = pit->path + fd->path + "/" + ff->name;
                    if (worker->getFileExists((char*)pfpath.c_str()) == false)
                    {
                        ffpath = ff->x_path + "/" + ff->name;
                        if (mutex != NULL)
                        {
                            pthread_mutex_lock(mutex);
                        }
                        worker->addTask(SYNC, from, pfpath);
                        if (mutex != NULL)
                        {
                            pthread_mutex_unlock(mutex);
                        }
                        ff->actualCopies++;
                    }
                }
                else
                {
                    break;
                }
            }
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
    worker->writeLog("CHECKER: start synchronizing copies");
    for (fit = s->flexdirs.begin(); fit != s->flexdirs.end(); fit++)
    {
        for (ffit = fit->files.begin(); ffit != fit->files.end(); ffit++)
        {
            string path = ffit->x_path + "/" + ffit->name;
            string ppath = "";
            if (worker->getIsLink((char*)path.c_str()))
            {
                string linktarget = worker->getLinkTarget((char*)path.c_str());
                if (worker->getFileExists((char*)linktarget.c_str()))
                {
                    poolfiles = worker->getPoolFiles(&(*ffit));
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
                                worker->addTask(SYNC, linktarget, ppath);
                                if (mutex != NULL)
                                {
                                    pthread_mutex_unlock(mutex);
                                }
                            }
                        }
                    }
                    else
                    {
                        worker->writeLog("CHECKER: FAILED no poolfiles found for: " + path);
                    }
                }
                else
                {
                    worker->writeLog("CHECKER: FAILED linktarget does not exist: " + linktarget);
                }
            }
            else
            {
                worker->writeLog("CHECKER: not a symlink: " + path);
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
    worker->writeLog("CHECKER: finished synchronizing copies");
}
