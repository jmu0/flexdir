#ifndef WORKER_H
#define WORKER_H
#include <string>
#include <vector>
#include <queue>
#include <pthread.h>

using namespace std;

enum role_t { NONE, FLEXFILE, NEW, PRIMARY, SECONDARY, ORPHAN, COPIES };
enum taskID_t { ADD, SYNC, DELETE, REMOVE, RENAME, LINK };

struct task_t
{
    taskID_t ID;
    string from;
    string to;
};

struct flexfile_t
{
    string name;
    string x_path;
    role_t role;
    int actualCopies;
};

struct flexdir_t
{
    string path;
    int copies;
    int watchdescriptor;
    vector<flexfile_t> files;
};

struct poolfile_t
{
    string name;
    string p_path;
    string x_path;
    role_t role;
};

struct pooldir_t
{
    string path;
    long sizeMB;
    long freeMB;
    double usedPerc;
    vector<poolfile_t> files;
};

bool pooldirSort(pooldir_t d1, pooldir_t d2);

struct settings_t 
{
    vector<flexdir_t> flexdirs;
    vector<pooldir_t> pooldirs;
    queue<task_t> tasks;
    int maxWorkerThreads;
    double maxLoadAverage;
    int maxLoadSleep;
    bool verbose;
    bool deleteWaitFlag;
    bool dailySync;
    string dailySyncTime;
    bool dailyRepair;
    string dailyRepairTime;
    int alarmThreshold;
};


class Worker
{
    private:
        string logFile;
        void loadSettings();
        settings_t settings;
        bool working;
        bool settingsValidLine(const string &line) const;
        void loadFlexFiles();
        void loadPoolFiles();
    public:
        Worker();
        ~Worker();
        void writeLog(string txt);
        double getLoadAverage();
        string getTime(const char * format);
        settings_t * getSettings();
        void printSettings();
        vector<pooldir_t> getNdirs(int n);
        bool getFileExists(const char * path);
        vector<poolfile_t> getPoolFiles(flexfile_t * flexfile);
        void loadFileStructure();
        void getPoolSizes();
        void printFileStructure();
        string role2string(role_t role);
        bool getIsLink(char * path);
        string getLinkTarget(char * path);
        int actionMoveFile(char * from, char * to);
        int actionDeleteFile(char * path);
        int actionCopyFile(char * from, char * to);
        int actionSyncFile(char * from, char * to);
        int actionCreateLink(char * target, char * linkname);
        int actionChangeLink(char * link, char * newTarget);
        int actionCreatedir(char * path);
        int actionRemovePoolFile(char * path);
        void startWorker(pthread_mutex_t * mutex, pthread_cond_t * condition);
        int addTask(taskID_t ID, string from, string to);
        int doTask(task_t * task);
        int getFlexStructFromPath(flexdir_t * flexdir, flexfile_t * flexfile, string path);
        int getPoolStructFromPath(pooldir_t * pooldir, poolfile_t * poolfile, string path);
};
#endif
