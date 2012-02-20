#include "worker.h"
#include <iostream>
#include <string>
#include <fstream>//filestream
#include <sstream>//stringstream
#include <time.h>//getTime
#include <vector> //list folders
#include <queue>//task queue
#include <sys/stat.h>//for file-exist
#include <sys/statvfs.h>//for disk sizes
#include <dirent.h>//for reading filenames
#include <cstdlib> //system calls
#include <string.h> //strcpy
#include <algorithm> //sort vectors
#include <pthread.h> //posix threads

#define SETTINGS "/home/jos/Git/flexdir/flexdir.conf"
#define DEF_LOGFILE "/home/jos/Git/flexdir/flexdir.log"
#define MAX_LOAD_AVG 1
#define MAX_LOAD_SLEEP 5
#define MAX_WORKER_THREADS 5
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
    writeLog("Worker constructor");
}

Worker::~Worker()
{
    writeLog("Worker deconstructor");
}

void Worker::loadSettings()
{
    //defaults
    settings.maxLoadAverage = MAX_LOAD_AVG;
    settings.maxLoadSleep = MAX_LOAD_SLEEP;
    settings.maxWorkerThreads = MAX_WORKER_THREADS;
    settings.verbose = VERBOSE;
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
            else if(key == "xFolder")
            {
                xfolder_t f;
                f.path = value.substr(0, value.find_last_of(' '));
                if (getFileExists((char *) f.path.c_str()))
                {
                    istringstream str(value.substr(value.find_last_of(' ') + 1));
                    if (!(str >> f.copies))
                    {
                        f.copies=1;
                        writeLog("ERROR: settings: could not read xfolder copies, set 1");
                    }
                    f.watchdescriptor = -1;
                    settings.xFolders.push_back(f);
                }
                else
                {
                    writeLog("ERROR: settings: xfolder " + (string)f.path + " doesn't exist");
                }
            }
            else if(key == "poolFolder")
            {
                poolfolder_t f;
                if (getFileExists((char*) value.c_str()))
                {
                    f.path = value;
                    f.sizeMB = -1;
                    f.freeMB = -1;
                    f.usedPerc = -1;
                    settings.poolFolders.push_back(f);
                }
                else
                {
                    writeLog("ERROR: settings: poolfolder " + (string)value + " doesn't exist");
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
    cout << "  <maxWorkerThreads>" << settings.maxWorkerThreads 
        << "</maxWorkerThreads>" << endl;
    vector<xfolder_t>::iterator ixf;
    cout << "  <xfolders>" << endl;
    for(ixf = settings.xFolders.begin(); ixf != settings.xFolders.end(); ixf++)
    {
        cout << "    <xfolder>\n";
        cout << "      <path>" <<  ixf->path << "</path>\n";
        cout << "      <copies>" << ixf->copies << "</copies>\n"; 
        cout << "      <watchdescriptor>" << ixf->watchdescriptor << "</watchdescriptor>\n";
        cout << "    </xfolder>" << endl;
    }
    cout << "  </xfolders>\n  <poolfolders>" << endl;
    vector<poolfolder_t>::iterator ipf;
    for(ipf = settings.poolFolders.begin(); ipf != settings.poolFolders.end(); ipf++)
    {
        cout << "    <poolfolder>\n";
        cout << "      <path>" << ipf->path << "</path>\n";
        cout << "      <sizeMB>" << ipf->sizeMB << "</sizeMB>\n";
        cout << "      <freeMB>" << ipf->freeMB << "</freeMB>\n";
        cout << "      <usedPerc>" << ipf->usedPerc << "</usedPerc>\n";
        cout << "    </poolfolder>" << endl;
    }
    cout << "  </poolfolders>\n</settings>" << endl;
}

void Worker::getPoolSizes()
{
    struct statvfs buffer;
    vector<poolfolder_t>::iterator iter;
    for(iter = settings.poolFolders.begin(); iter != settings.poolFolders.end(); iter++)
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
    vector<poolfolder_t>::iterator ip;
    vector<xfolder_t>::iterator ix;
    for(ix = settings.xFolders.begin(); ix != settings.xFolders.end(); ix++)
    {
        DIR* dir = opendir((char*)ix->path.c_str());
        if (dir)
        {
            struct dirent* entry;
            while((entry = readdir(dir)) != NULL)
            {
                if ((string)entry->d_name != "." && (string)entry->d_name != "..")
                {
                    xfile_t f;
                    f.name = entry->d_name;
                    f.x_path = ix->path;
                    f.role = NONE;
                    f.copies = 0;
                    ix->files.push_back(f);
                }
            }
        }
        else
        {
            writeLog("ERROR: loadFileStructure: could not open xfile: "+ (string)ix->path);
        }
    }
    for(ip = settings.poolFolders.begin(); ip != settings.poolFolders.end(); ip++)
    {
        for(ix = settings.xFolders.begin(); ix != settings.xFolders.end(); ix++)
        {
            string pad = ip->path + "/" + ix->path;
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
    loadFileStructure();
    cout << "<fileStructure>" << endl;
    vector<xfolder_t>::iterator ixf;
    vector<xfile_t>::iterator xf;
    cout << "  <xfolders>" << endl;
    for(ixf = settings.xFolders.begin(); ixf != settings.xFolders.end(); ixf++)
    {
        cout << "    <xfolder>\n";
        cout << "      <path>" <<  ixf->path << "</path>\n";
        cout << "      <copies>" << ixf->copies << "</copies>\n"; 
        cout << "      <watchdescriptor>" << ixf->watchdescriptor << "</watchdescriptor>\n";
        cout << "      <files>" << endl;
        for(xf = ixf->files.begin(); xf != ixf->files.end(); xf++)
        {
            cout << "        <xfile>" << endl;
            cout << "          <name>" << xf->name << "</name>\n";
            cout << "          <x_path>" << xf->x_path << "</x_path>\n";
            cout << "          <role>" << xf->role << "</role>\n";
            cout << "        </xfile>" << endl;
        }
        cout << "      </files>" << endl;
        cout << "    </xfolder>" << endl;
    }
    cout << "  </xfolders>\n  <poolfolders>" << endl;
    vector<poolfolder_t>::iterator ipf;
    vector<poolfile_t>::iterator pf;
    for(ipf = settings.poolFolders.begin(); ipf != settings.poolFolders.end(); ipf++)
    {
        cout << "    <poolfolder>\n";
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
            cout << "          <role>" << pf->role << "</role>\n";
            cout << "        </poolfile>" << endl;
        }
        cout << "      </files>" << endl;
        cout << "    </poolfolder>" << endl;
    }
    cout << "  </poolfolders>\n</fileStructure>" << endl;
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

bool poolfolderSort(poolfolder_t d1, poolfolder_t d2)
{
    return d1.usedPerc < d2.usedPerc;
}

vector<poolfolder_t> Worker::getNfolders(int n)
{
    vector<poolfolder_t> ret;
    int teller = 0;
    vector<poolfolder_t>::iterator i;
    getPoolSizes();
    sort(settings.poolFolders.begin(), settings.poolFolders.end(), poolfolderSort);
    for(i=settings.poolFolders.begin(); i != settings.poolFolders.end(); i++)
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
    if (readlink(path, buf, sizeof(buf)) < 0)
    {
        writeLog("No link target found");
        buf[0] = '\0';
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
    string com = "cp -ruf " + (string)from + " " + (string)to; 
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
    string com = "cp -rsf " + (string)target + " " + (string)linkname;
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

int Worker::actionCreateFolder(char * path)
{
    string cmd = "mkdir -p " + (string)path;
    //TODO: find alternative for system call (mkdir not recursive)
    return system((char*)cmd.c_str());
}

void Worker::startWorker(pthread_mutex_t * mutex, pthread_cond_t * condition)
{
    bool stop = false;
    while(stop == false)
    {
        pthread_mutex_lock(mutex);
        if (settings.tasks.size() == 0)
        {
            writeLog("WORKER: waiting...");
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
        string logEntry;
        stringstream ss, sn;
        sn << (settings.tasks.size() + 1);
        ss << task->ID;
        logEntry = "WORKER: " + sn.str() + " tasks, ";
        switch (task->ID)
        {
            case ADD:
                logEntry += "ADD task: ";
                break;
            case SYNC:
                logEntry += "SYNC task: ";
                if (actionSyncFile((char*)task->from.c_str(), (char*)task->to.c_str()) != 0)
                {
                    logEntry += "FAILED ";
                }
                else
                {
                    logEntry += "OK ";
                }
                break;
            case DELETE:
                logEntry += "DELETE task: ";
                if (actionDeleteFile((char*)task->from.c_str()) != 0)
                {
                    logEntry += "FAILED ";
                }
                else
                {
                    logEntry += "OK ";
                }
                break;
            case REMOVE:
                logEntry += "REMOVE task: ";
                break;
            case RENAME:
                logEntry += "RENAME task: ";
                break;
            default:
                logEntry += "ERROR: unknown task-id: ";
                break;
        }
        logEntry +=  ", from: " + task->from + ", to:" + task->to;
    writeLog(logEntry);
    return 0;
}

void Worker::getStructFromPath(xfolder_t * xfolder, xfile_t * xfile, string path)
{
    xfolder = 0;
    xfile = 0;

}



