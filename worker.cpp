#include "worker.h"
#include <iostream>
#include <string>
#include <fstream>//filestream
#include <sstream>//stringstream
#include <time.h>//getTime
#include <vector> //list dirs
#include <queue>//task queue
#include <sys/stat.h>//for file-exist
#include <sys/statvfs.h>//for disk sizes
#include <dirent.h>//for reading filenames
#include <cstdlib> //system calls
#include <string.h> //strcpy
#include <algorithm> //sort vectors
#include <pthread.h> //posix threads

#define SETTINGS "/home/jos/git/flexdir/flexdir.conf"
#define DEF_LOGFILE "/home/jos/git/flexdir/flexdir.log"
#define MAX_LOAD_AVG 1
#define MAX_LOAD_SLEEP 5
#define MAX_WORKER_THREADS 5
#define DAILY_CHECK false
#define DAILY_CHECK_TIME "0:00"
#define ALARM_THRESHOLD 100
#define VERBOSE true

using namespace std;


Worker::Worker()
{   
    logFile = DEF_LOGFILE;
    loadSettings();
    if (settings.tasks.size() > 0)
    {
        stringstream str;
        str << settings.tasks.size();
        writeLog(str.str() + " tasks loaded from file.");
    }
    else
    {
        working=false;
    }
}

Worker::~Worker()
{
}

void Worker::loadSettings()
{
    //defaults
    settings.maxLoadAverage = MAX_LOAD_AVG;
    settings.maxLoadSleep = MAX_LOAD_SLEEP;
    settings.maxWorkerThreads = MAX_WORKER_THREADS;
    settings.verbose = VERBOSE;
    settings.deleteWaitFlag = false;
    settings.dailyCheck = DAILY_CHECK; 
    settings.dailyCheckTime = DAILY_CHECK_TIME; 
    settings.alarmThreshold = ALARM_THRESHOLD;
    //load settings file
    ifstream file;
    file.open(SETTINGS);
    if (file.is_open())
    {
        string line;
        size_t lineNo = 0;
        while (getline(file, line))
        {
            lineNo++;
            string temp = line;
            //remove comment
            if (temp.find('#') != temp.npos)
            {
                temp.erase(temp.find('#'));
            }
            if (temp.empty())
            {
                continue;
            }
            if (!settingsValidLine(temp))
            {
                continue;
            }
            temp.erase(0, temp.find_first_not_of("\t "));
            size_t sepPos = temp.find('=');
            string key, value;
            key = temp.substr(0, sepPos);
            if (key.find('\t') != temp.npos || key.find(' ') != temp.npos)
            {
                key.erase(key.find_first_of("\t "));
            }
            value = temp.substr(sepPos + 1);
            value.erase(0, value.find_first_not_of("\t "));
            value.erase(value.find_last_not_of("\t ") + 1);
            if (key == "maxLoadAverage")
            {
                istringstream str(value);
                if (!(str >> settings.maxLoadAverage))
                {
                    settings.maxLoadAverage = MAX_LOAD_AVG;
                    writeLog("ERROR: settings: could not read maxLoadAverage, set default");
                }
            }
            else if(key == "maxLoadSleep")
            {
                istringstream str(value);
                if (!(str >> settings.maxLoadSleep))
                {
                    settings.maxLoadSleep = MAX_LOAD_SLEEP;
                    writeLog("ERROR: settings; could not read maxLoadSleep, set default");
                }
            }
            else if(key ==  "maxWorkerThreads") //TODO: multiple worker threads?
            {
                istringstream str(value);
                if (!(str >> settings.maxWorkerThreads))
                {
                    settings.maxWorkerThreads = MAX_WORKER_THREADS;
                    writeLog("ERROR: settings: could not read maxWorkerThreads, set default");
                }
            }
            else if(key == "dailyCheck")
            {
                if (value == "true")
                {
                    settings.dailyCheck = true;
                }
                else
                {
                    settings.dailyCheck = false;
                }
            }
            else if(key == "dailyCheckTime")
            {
                settings.dailyCheckTime = value;
            }
            else if(key == "alarmThreshold")
            {
                istringstream str(value);
                if (!(str >> settings.alarmThreshold))
                {
                    settings.alarmThreshold = ALARM_THRESHOLD;
                    writeLog("ERROR: settings: could not read alarmThreshold, set default");
                }

            }
            else if(key == "flexdir")
            {
                flexdir_t f;
                f.path = value.substr(0, value.find_last_of(' '));
                if (getFileExists((char *) f.path.c_str()))
                {
                    istringstream str(value.substr(value.find_last_of(' ') + 1));
                    if (!(str >> f.copies))
                    {
                        f.copies=1;
                        writeLog("ERROR: settings: could not read flexdir copies, set 1");
                    }
                    f.watchdescriptor = -1;
                    settings.flexdirs.push_back(f);
                }
                else
                {
                    writeLog("ERROR: settings: flexdir " + (string)f.path + " doesn't exist");
                }
            }
            else if(key == "pooldir")
            {
                pooldir_t f;
                if (getFileExists((char*) value.c_str()))
                {
                    f.path = value;
                    f.sizeMB = -1;
                    f.freeMB = -1;
                    f.usedPerc = -1;
                    settings.pooldirs.push_back(f);
                }
                else
                {
                    writeLog("ERROR: settings: pooldir " + (string)value + " doesn't exist");
                }
            }
        }
        file.close();
    }
    else
    {
        writeLog("ERROR: couldn't load settings file: " + (string)SETTINGS);
    }
}

bool Worker::settingsValidLine(const string &line) const
{
    string temp = line;
    if (temp.find_first_not_of(' ') == temp.npos)//is only whitespace, continue
    {
        return false;
    }
    if (temp.find('=') == temp.npos)//no separator, continue
    {
        return false;
    }
    temp.erase(0, temp.find_first_not_of("\t"));
    if (temp[0] == '=')//key is missing
    {
        return false;
    }
    for (size_t i = temp.find('=') +1; i < temp.length(); i++)
    {
        if (temp[i] != ' ')
        {
            return true; //value found
        }
    }
    return false; //no value found
}

settings_t * Worker::getSettings()
{
    return &settings;
}

void Worker::printSettings()
{
    getPoolSizes();
    cout << "<settings>" << endl;
    cout << "  <maxLoadAverage>" << settings.maxLoadAverage << "</maxLoadAverage>" << endl;
    cout << "  <maxLoadSleep>" << settings.maxLoadSleep << "</maxLoadSleep>" << endl;
    cout << "  <maxWorkerThreads>" << settings.maxWorkerThreads << "</maxWorkerThreads>" << endl;
    cout << "  <dailyCheck>" << settings.dailyCheck << "</dailyCheck>" << endl;
    cout << "  <dailyCheckTime>" << settings.dailyCheckTime << "</dailyCheckTime>" << endl;
    cout << "  <alarmThreshold>" << settings.alarmThreshold << "</alarmThreshold>" << endl;
    vector<flexdir_t>::iterator ixf;
    cout << "  <flexdirs>" << endl;
    for(ixf = settings.flexdirs.begin(); ixf != settings.flexdirs.end(); ixf++)
    {
        cout << "    <flexdir>\n";
        cout << "      <path>" <<  ixf->path << "</path>\n";
        cout << "      <copies>" << ixf->copies << "</copies>\n"; 
        cout << "      <watchdescriptor>" << ixf->watchdescriptor << "</watchdescriptor>\n";
        cout << "    </flexdir>" << endl;
    }
    cout << "  </flexdirs>\n  <pooldirs>" << endl;
    vector<pooldir_t>::iterator ipf;
    for(ipf = settings.pooldirs.begin(); ipf != settings.pooldirs.end(); ipf++)
    {
        cout << "    <pooldir>\n";
        cout << "      <path>" << ipf->path << "</path>\n";
        cout << "      <sizeMB>" << ipf->sizeMB << "</sizeMB>\n";
        cout << "      <freeMB>" << ipf->freeMB << "</freeMB>\n";
        cout << "      <usedPerc>" << ipf->usedPerc << "</usedPerc>\n";
        cout << "    </pooldir>" << endl;
    }
    cout << "  </pooldirs>\n</settings>" << endl;
}

void Worker::getPoolSizes()
{
    struct statvfs buffer;
    vector<pooldir_t>::iterator iter;
    for(iter = settings.pooldirs.begin(); iter != settings.pooldirs.end(); iter++)
    {
        if (!statvfs((char*)iter->path.c_str(), &buffer))
        {
            long bs = buffer.f_bsize;
            long bl = buffer.f_blocks;
            long fb = buffer.f_bfree;
            iter->sizeMB = (((bl*bs)/1024)/1024);
            iter->freeMB = (((fb*bs)/1024)/1024);
            //TODO: df - h > used% calculated in bytes (=higher)??
            iter->usedPerc = ((((double)iter->sizeMB - (double)iter->freeMB) 
                        / iter->sizeMB) * 100);
        }
        else
        {
            writeLog("ERROR: getPoolSizes: could not read " + (string) iter->path);
        }
    }
}

void Worker::loadFileStructure()
{
    loadFlexFiles();
    loadPoolFiles();
}

void Worker::loadFlexFiles()
{
    vector<flexdir_t>::iterator ix;
    for(ix = settings.flexdirs.begin(); ix != settings.flexdirs.end(); ix++)
    {
        DIR* dir = opendir((char*)ix->path.c_str());
        if (dir)
        {
            struct dirent* entry;
            while((entry = readdir(dir)) != NULL)
            {
                if ((string)entry->d_name != "." && (string)entry->d_name != "..")
                {
                    flexfile_t f;
                    f.name = entry->d_name;
                    f.x_path = ix->path;
                    f.role = NONE;
                    f.actualCopies = 0;
                    ix->files.push_back(f);
                }
            }
        }
        else
        {
            writeLog("ERROR: loadFileStructure: could not open flexfile: "+ (string)ix->path);
        }
    }
}

void Worker::loadPoolFiles()
{
    vector<flexdir_t>::iterator ix;
    vector<pooldir_t>::iterator ip;
    for(ip = settings.pooldirs.begin(); ip != settings.pooldirs.end(); ip++)
    {
        for(ix = settings.flexdirs.begin(); ix != settings.flexdirs.end(); ix++)
        {
            string pad = ip->path + ix->path;
            if (getFileExists((char*)pad.c_str()))
            {
                DIR* dir = opendir((char*)pad.c_str());
                if (dir)
                {
                    struct dirent* entry;
                    while((entry = readdir(dir)) != NULL)
                    {
                        if ((string)entry->d_name != "." && (string)entry->d_name != "..")
                        {
                            poolfile_t f;
                            f.name = entry->d_name;
                            f.x_path = ix->path;
                            f.p_path = ip->path;
                            f.role = NONE;
                            ip->files.push_back(f);
                        }
                    }
                }
                else
                {
                    writeLog("ERROR: loadFileStructure: could not open poolfile: "+ (string)ix->path);
                }
            }
        }
    }
}

void Worker::printFileStructure()
{
    cout << "<fileStructure>" << endl;
    vector<flexdir_t>::iterator ixf;
    vector<flexfile_t>::iterator xf;
    cout << "  <flexdirs>" << endl;
    for(ixf = settings.flexdirs.begin(); ixf != settings.flexdirs.end(); ixf++)
    {
        cout << "    <flexdir>\n";
        cout << "      <path>" <<  ixf->path << "</path>\n";
        cout << "      <copies>" << ixf->copies << "</copies>\n"; 
        cout << "      <watchdescriptor>" << ixf->watchdescriptor << "</watchdescriptor>\n";
        cout << "      <files>" << endl;
        for(xf = ixf->files.begin(); xf != ixf->files.end(); xf++)
        {
            cout << "        <flexfile>" << endl;
            cout << "          <name>" << xf->name << "</name>\n";
            cout << "          <x_path>" << xf->x_path << "</x_path>\n";
            cout << "          <role>" << role2string(xf->role) << "</role>\n";
            cout << "        </flexfile>" << endl;
        }
        cout << "      </files>" << endl;
        cout << "    </flexdir>" << endl;
    }
    cout << "  </flexdirs>\n  <pooldirs>" << endl;
    vector<pooldir_t>::iterator ipf;
    vector<poolfile_t>::iterator pf;
    for(ipf = settings.pooldirs.begin(); ipf != settings.pooldirs.end(); ipf++)
    {
        cout << "    <pooldir>\n";
        cout << "      <path>" << ipf->path << "</path>\n";
        cout << "      <sizeMB>" << ipf->sizeMB << "</sizeMB>\n";
        cout << "      <freeMB>" << ipf->freeMB << "</freeMB>\n";
        cout << "      <usedPerc>" << ipf->usedPerc << "</usedPerc>\n";
        cout << "      <files>" << endl;
        for(pf = ipf->files.begin(); pf != ipf->files.end(); pf++)
        {
            cout << "        <poolfile>" << endl;
            cout << "          <name>" << pf->name << "</name>\n";
            cout << "          <p_path>" << pf->p_path << "</p_path>\n";
            cout << "          <x_path>" << pf->x_path << "</x_path>\n";
            cout << "          <role>" << role2string(pf->role) << "</role>\n";
            cout << "        </poolfile>" << endl;
        }
        cout << "      </files>" << endl;
        cout << "    </pooldir>" << endl;
    }
    cout << "  </pooldirs>\n</fileStructure>" << endl;
}

string Worker::role2string(role_t role)
{
    switch(role)
    {
        case NONE:
            return "NONE";
            break;
        case FLEXFILE:
            return "FLEXFILE";
            break;
        case NEW:
            return "NEW";
            break;
        case PRIMARY:
            return "PRIMARY";
            break;
        case SECONDARY:
            return "SECONDARY";
            break;
        case ORPHAN:
            return "ORPHAN";
            break;
        case COPIES:
            return "COPIES";
            break;
        default:
            return "UNKNOWN";
            break;
    }
}

void Worker::writeLog(string txt)
{
    string entry = getTime("%d-%m-%Y %H:%M:%S") + " > " + txt;
    if (settings.verbose == true)
    {
        cout << entry << endl;
    }
    fstream log;
    log.open(DEF_LOGFILE, fstream::in | fstream::out | fstream::app);
    log << entry << "\n";
    log.close();
}

string Worker::getTime(const char * format)
{
    time_t rawtime;
    struct tm * timeinfo;
    char buffer [80];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer, 80, format, timeinfo);    
    string ret(buffer);
    return ret;
}

double Worker::getLoadAverage()
{
    ifstream la("/proc/loadavg");
    double loadavg = -1;
    la >> loadavg;
    la.close();
    return loadavg;
}

bool pooldirSort(pooldir_t d1, pooldir_t d2)
{
    return d1.usedPerc < d2.usedPerc;
}

vector<pooldir_t> Worker::getNdirs(int n)
{
    vector<pooldir_t> ret;
    int teller = 0;
    vector<pooldir_t>::iterator i;
    getPoolSizes();
    sort(settings.pooldirs.begin(), settings.pooldirs.end(), pooldirSort);
    for(i=settings.pooldirs.begin(); i != settings.pooldirs.end(); i++)
    {
        ret.push_back(*i);
        teller++;
        if (teller >= n)
        {
            return ret;
        }
    }
    return ret;
}

vector<poolfile_t> Worker::getPoolFiles(flexfile_t * flexfile)
{
    vector<poolfile_t> files;
    vector<pooldir_t>::iterator pit;
    string ppath;
    for(pit = settings.pooldirs.begin(); pit != settings.pooldirs.end(); pit++)
    {
        ppath = pit->path + flexfile->x_path + "/" + flexfile->name;
        if (getFileExists((char*)ppath.c_str()) == true)
        {
            poolfile_t pf;
            pf.name = flexfile->name;
            pf.p_path = pit->path;
            pf.x_path = flexfile->x_path;
            pf.role = NONE;
            files.push_back(pf);
        }
    }
    return files;
}

bool Worker::getFileExists(const char * path)
{
    struct stat st;
    if (stat(path, &st) == 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool Worker::getIsLink(char * path)
{
    struct stat sb;
    if (lstat(path, &sb) == 0) { 
        if (S_ISLNK(sb.st_mode)) {
            return true;
        }
        else
        {
            return false;
        }
    } 
    else { 
        writeLog("FAILED: getIsLink > lstat(" + (string)path+ ")");
        return false;
    }
}

string Worker::getLinkTarget(char * path)
{
    char buf[512];
    int len = readlink(path, buf, sizeof(buf));
    if (len < 0)
    {
        writeLog("No link target found");
        buf[0] = '\0';
    }
    else 
    {
        buf[len] = '\0';
    }
    return (string)buf;
}

int Worker::actionMoveFile(char * from, char * to)
{    
    return rename(from, to);
}

int Worker::actionDeleteFile(char * path)
{
    return remove(path);
}

int Worker::actionCopyFile(char * from, char * to)//TODO: use rsync
{
    string com = "cp -ruf '" + (string)from + "' '" + (string)to + "'"; 
    //TODO: find alternative for system call
    return system((char *) com.c_str());
}

int Worker::actionSyncFile(char * from, char * to)
{
    string com = "rsync --recursive --perms --delete --update \"" + (string)from + "\" \"" + (string)to +"\"";
    //TODO: find alternative for system call
    return system((char *) com.c_str());
}

int Worker::actionCreateLink(char * target, char * linkname)
{
    string com = "cp -rsf '" + (string)target + "' '" + (string)linkname + "'";
    //TODO: find alternative for system call
    return system((char *) com.c_str()); 
}

int Worker::actionChangeLink(char * link, char * newTarget)
{
    int ret = 0;
    ret = actionDeleteFile(link);
    if (ret != 0)
    {
        return ret;
    }
    ret = actionCreateLink(newTarget, link);
    return ret;
}

int Worker::actionCreatedir(char * path)
{
    string cmd = "mkdir -p '" + (string)path + "'";
    //TODO: find alternative for system call (mkdir not recursive)
    return system((char*)cmd.c_str());
}

int Worker::actionRemovePoolFile(char * path)
{
    pooldir_t pd;
    poolfile_t pf;
    //get pooldir/file structures from path <from>
    if(getPoolStructFromPath(&pd, &pf, path) == 0)
    {
        //check for .deleted dir, create if needed
        bool pathError = false;
        string delPath = pd.path + "/.deleted" + pf.x_path;
        if (getFileExists((char*)delPath.c_str()) == false)
        {
            if(actionCreatedir((char*)delPath.c_str()) != 0)
            {
                pathError = true;
            }
        }
        if (pathError == false)
        {
            //move file to .deleted dir in pooldir root
            delPath += "/" + pf.name;
            if (actionMoveFile(path, (char*)delPath.c_str()) == 0)
            {
                return 0;
            }
            else
            {
                return -1;
            }
        }
        else
        {
            return -1;
        }
    }
    else
    {
        return -1;
    }
}

void Worker::startWorker(pthread_mutex_t * mutex, pthread_cond_t * condition)
{
    bool stop = false;
    while(stop == false)
    {
        pthread_mutex_lock(mutex);
        if (settings.tasks.size() == 0)
        {
            //DEBUG: writeLog("WORKER: waiting...");
            pthread_cond_wait(condition, mutex);
        }
        pthread_mutex_unlock(mutex);
        while(settings.tasks.size() > 0)
        {
            pthread_mutex_lock(mutex);
            task_t task = settings.tasks.front();
            settings.tasks.pop();
            pthread_mutex_unlock(mutex);
            double lavg = getLoadAverage();
            while (lavg > settings.maxLoadAverage)
            {
                stringstream la, mla, mls;
                la << lavg;
                mla << settings.maxLoadAverage;
                mls << settings.maxLoadSleep;
                writeLog("WORKER: load "+la.str()+" > "+mla.str()+", sleeping "+mls.str()+" seconds");
                sleep(settings.maxLoadSleep);
                lavg=getLoadAverage();
            }
            doTask(&task);
        }
    }
}

int Worker::addTask(taskID_t ID, string from, string to)
{
    task_t task;
    task.ID=ID;
    task.from = from;
    task.to = to;
    settings.tasks.push(task);
    return 0;
}

int Worker::doTask(task_t * task)
{
    flexdir_t fd;
    flexfile_t ff;
    vector<pooldir_t>::iterator pit;
    vector<pooldir_t> dirs; 
    vector<poolfile_t> poolfiles;
    vector<poolfile_t>::iterator pfit;
    string path, logEntry;
    bool copyError = false;
    stringstream ss, sn;
    sn << (settings.tasks.size() + 1);
    ss << task->ID;
    logEntry = "WORKER: " + sn.str() + " tasks, ";
    switch (task->ID)
    {
        case ADD:
            logEntry += "ADD task: ";
            //get flexdir/file structures from path <from>
            if (getFlexStructFromPath(&fd, &ff, task->from) == 0)
            {
                //get n poolfolders
                dirs = getNdirs(fd.copies);
                if (dirs.size() > 0)
                {
                    //create destination dirs and rsync file to folders
                    for (pit = dirs.begin(); pit != dirs.end(); pit++)
                    {
                        string toPath = pit->path + fd.path;
                        string to = toPath + "/" + ff.name;
                        if ( actionCreatedir((char*)toPath.c_str()) == 0)
                        {
                            if ( actionSyncFile((char*)task->from.c_str(), (char*)to.c_str()) != 0)
                            {
                                copyError = true;
                            }
                        }
                        else
                        {
                            copyError = true;
                        }
                    }
                    if (copyError == false)
                    {
                        //remove file / create link
                        settings.deleteWaitFlag = true; //set flag, prevents trigger delete task by watcher
                        if (actionDeleteFile((char*)task->from.c_str()) == 0)
                        {
                            string target = dirs.begin()->path + fd.path + "/" +ff.name;
                            if (actionCreateLink((char*)target.c_str(), (char*)task->from.c_str()) == 0)
                            {
                                logEntry += "OK ";
                            }
                            else 
                            {
                                logEntry += "FAILED could not create symlink ";
                            }
                        }
                        settings.deleteWaitFlag = false;
                    }
                    else
                    {
                        logEntry += "FAILED error copying file(s) to poolfolder(s) ";
                    }
                }
                else
                {
                    logEntry += "FAILED could not get N poolfolders ";
                }
            }
            else
            {
                logEntry += "FAILED could not get flexfile structure ";
            }
            break;
        case SYNC:
            //sync <from> -> <to>
            logEntry += "SYNC task: ";
            if (actionSyncFile((char*)task->from.c_str(), (char*)task->to.c_str()) != 0)
            {
                logEntry += "FAILED could not sync file ";
            }
            else
            {
                logEntry += "OK ";
            }
            break;
        case DELETE:
            logEntry += "DELETE task: ";
            //delete file <from>
            if (actionDeleteFile((char*)task->from.c_str()) != 0)
            {
                logEntry += "FAILED could not delete file ";
            }
            else
            {
                logEntry += "OK ";
            }
            break;
        case REMOVE: //remove all poolfiles for given flexfile <from>
            logEntry += "REMOVE task: ";
            //find all files in pool
            if (getFlexStructFromPath(&fd, &ff, task->from) == 0)
            {
                poolfiles = getPoolFiles(&ff);
                if (poolfiles.size() > 0)
                {
                    //remove files in pool to .deleted 
                    bool removeError = false;
                    string path;
                    for (pfit = poolfiles.begin(); pfit != poolfiles.end(); pfit++)
                    {
                        path = pfit->p_path + task->from;
                        if (actionRemovePoolFile((char*)path.c_str()) != 0)
                        {
                            removeError = true;
                        }
                    }
                    if (removeError == false)
                    {
                        logEntry += "OK ";
                    }
                    else
                    {
                        logEntry += "FAILED error removing poolfiles ";
                    }
                }
                else
                {
                    logEntry += "FAILED no poolfiles found ";
                }
            }
            else
            {
                logEntry += "FAILED could not get flexdir structure ";
            }
            break;
        case RENAME: //rename all poolfiles for given flexfile and update link target
            logEntry += "RENAME task: ";
            //find all files in pool
            if (getFlexStructFromPath(&fd, &ff, task->from) == 0)
            {
                poolfiles = getPoolFiles(&ff);
                if (poolfiles.size() > 0)
                {
                    //rename files in pool to <to>
                    bool renameError = false;
                    string fromPath, toPath;
                    for (pfit = poolfiles.begin(); pfit != poolfiles.end(); pfit++)
                    {
                        fromPath = pfit->p_path + task->from;
                        toPath = pfit->p_path + task->to;
                        if (actionMoveFile((char*)fromPath.c_str(), (char*)toPath.c_str()) != 0)
                        {
                            renameError = true;
                        }
                    }
                    //change link target 
                    if (renameError == false)
                    {
                        settings.deleteWaitFlag = true;
                        if (actionChangeLink((char*)task->to.c_str(), (char*)toPath.c_str()) == 0)
                        {
                            logEntry += "OK ";
                        }
                        else
                        {
                            logEntry += "FAILED could not change link target ";
                        }
                        settings.deleteWaitFlag = false;
                    }
                    else
                    {
                        logEntry += "FAILED error renaming poolfiles ";
                    }
                }
                else
                {
                    logEntry += "FAILED no poolfiles found ";
                }
            }
            else
            {
                logEntry += "FAILED could not get flexdir structure ";
            }
            break;
        default:
            logEntry += "ERROR: unknown task-id: ";
            break;
    }
    logEntry +=  ", from: " + task->from + ", to:" + task->to;
    writeLog(logEntry);
    return 0;
}

int Worker::getFlexStructFromPath(flexdir_t * flexdir, flexfile_t * flexfile, string path)
{
    vector<flexdir_t>::iterator it;
    for(it = settings.flexdirs.begin(); it != settings.flexdirs.end(); it++)
    {
        string fpath = (*it).path;
        int len = fpath.length();
        if (path.substr(0, len) == fpath)
        {
            *flexdir = *it;
            (*flexfile).name = path.substr(len + 1);
            (*flexfile).x_path = fpath;
            (*flexfile).role=NONE;
            (*flexfile).actualCopies = 0;
            return 0;
        }

    }
    return 1;
}

int Worker::getPoolStructFromPath(pooldir_t * pooldir, poolfile_t * poolfile, string path)
{
    vector<flexdir_t>::iterator fit;
    vector<pooldir_t>::iterator pit;
    for(pit = settings.pooldirs.begin(); pit != settings.pooldirs.end(); pit++)
    {
        string fpath = (*pit).path;
        int len = fpath.length();
        if (path.substr(0, len) == fpath)
        {
            *pooldir = *pit;
            (*poolfile).p_path = fpath;
            string rest = path.substr(len);
            for(fit = settings.flexdirs.begin(); fit != settings.flexdirs.end(); fit++)
            {
                fpath = (*fit).path;

                len = fpath.length();
                if (rest.substr(0, len) == fpath)
                {
                    (*poolfile).name = rest.substr(len + 1);
                    (*poolfile).x_path = fpath;
                    (*poolfile).role=NONE;
                    return 0;
                }
            }
        }
    }
    return 1;
}
