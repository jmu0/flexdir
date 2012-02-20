#ifndef WORKER_H
#define WORKER_H
#include <string>
#include <vector>
#include <queue>
#include <pthread.h>

using namespace std;

enum role_t { NONE, XFILE, NEW, PRIMARY, SECONDARY, ORPHAN, COPIES };
enum taskID_t { ADD, SYNC, DELETE, REMOVE, RENAME };

struct task_t
{
    taskID_t ID;
    string from;
    string to;
};

struct xfile_t
{
    string name;
    string x_path;
    role_t role;
    int copies;
};

struct xfolder_t
{
    string path;
    int copies;
    int watchdescriptor;
    vector<xfile_t> files;
};

struct poolfile_t
{
    string name;
    string p_path;
    string x_path;
    role_t role;
};

struct poolfolder_t
{
    string path;
    long sizeMB;
    long freeMB;
    double usedPerc;
    vector<poolfile_t> files;
};

bool poolfolderSort(poolfolder_t d1, poolfolder_t d2);

struct settings_t 
{
    vector<xfolder_t> xFolders;
    vector<poolfolder_t> poolFolders;
    queue<task_t> tasks;
    int maxWorkerThreads;
    double maxLoadAverage;
    int maxLoadSleep;
    bool verbose;
};


class Worker
{
    private:
        string logFile;
        void loadSettings();
        settings_t settings;
        bool working;
        bool settingsValidLine(const string &line) const;
        void getPoolSizes();
        void loadFileStructure();
        void getStructFromPath(xfolder_t * xfolder, xfile_t * xfile, string path);
    public:
        Worker();
        ~Worker();
        void writeLog(string txt);
        double getLoadAverage();
        string getTime(const char * format);
        settings_t * getSettings();
        void printSettings();
        vector<poolfolder_t> getNfolders(int n);
        bool getFileExists(const char * path);
        void printFileStructure();
        bool getIsLink(char * path);
        string getLinkTarget(char * path);
        int actionMoveFile(char * from, char * to);
        int actionDeleteFile(char * path);
        int actionCopyFile(char * from, char * to);
        int actionSyncFile(char * from, char * to);
        int actionCreateLink(char * target, char * linkname);
        int actionChangeLink(char * link, char * newTarget);
        int actionCreateFolder(char * path);
        void startWorker(pthread_mutex_t * mutex, pthread_cond_t * condition);
        int addTask(taskID_t ID, string from, string to);
        int doTask(task_t * task);
};
#endif
