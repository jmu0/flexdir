#include "worker.h"
#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <sys/statvfs.h>
#include <pthread.h>

using namespace std;

Worker w;
settings_t * s;
void fileTest();
void symlinkTest();
void linktst(string testlink);
void ndirtest();
void testTasks();
void startThreads();
void testFunctions();

void * wThread(void * id);
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition = PTHREAD_COND_INITIALIZER;

int main()
{
    s = w.getSettings();
    s->verbose = true;   
    //symlinkTest();
    //fileTest();
    //w.printSettings();    
    //w.printFileStructure();
    //ndirtest();
    //testFunctions();
    startThreads();
}

void startThreads()
{
    pthread_t wt;
    int retw;
    retw = pthread_create(&wt, NULL, wThread, (void *) 1);
    testTasks();
    pthread_join(wt, NULL);
}

void * wThread(void * id)
{
    w.startWorker(&mutex, &condition);
}

void symlinkTest(){
    string testlink = "/home/jos/tmp/testlink";
    string islink = "/home/jos/tmp/islink";
    string nothing= "/home/jos/tmp/doesntexist";
    linktst(testlink);
    linktst(islink);
    linktst(nothing);
}
void linktst(string testlink)
{
    cout <<testlink << ": ";
    string buffer;
    if (w.getIsLink((char*)testlink.c_str()))
    {
        cout << "yes: "; 
        buffer = w.getLinkTarget((char*)testlink.c_str());
        cout << buffer<< endl;
    }
    else 
    {
        cout << "no" << endl;
    }
}
void fileTest()
{
    cout << "Test copy/rename/delete files" << endl;
    system("touch /home/jos/tmp/testcopy.txt");
    Worker wo = w;
    if (w.getFileExists("/home/jos/tmp/testcopy.txt"))
    {
        cout << "testfile created" << endl;
    }
    else
    {
        cout << "error creating testfile" << endl;
    }
    w.actionMoveFile((char*)"/home/jos/tmp/testcopy.txt", (char*)"/home/jos/tmp/movedTest.txt");
    if (w.getFileExists("/home/jos/tmp/movedTest.txt"))
    {
        cout << "testfile moved" << endl;

    }
    else
    {
        cout << "error moving testfile" << endl;
    }
    w.actionCopyFile((char*)"/home/jos/tmp/movedTest.txt", (char*)"/home/jos/tmp/copiedTest.txt");
    if (w.getFileExists((char*)"/home/jos/tmp/copiedTest.txt"))
    {
        cout << "testfile copied" << endl;
    }
    else
    {
        cout << "error copying testfile" << endl;
    }
    w.actionCreateLink((char*)"/home/jos/tmp/movedTest.txt", (char*)"/home/jos/tmp/testlink");
    if (w.getFileExists("/home/jos/tmp/testlink"))
    {
        cout << "testfile linked" << w.getLinkTarget((char*)"/home/jos/tmp/testlink") << endl;
    }
    else
    {
        cout << "error linking testfile" << endl;
    }
    w.actionChangeLink((char*)"/home/jos/tmp/testlink", (char*)"/home/jos/tmp/copiedTest.txt");
    if (w.getFileExists((char*)"/home/jos/tmp/testlink"))
    {
        cout << "linktarget changed" << w.getLinkTarget((char*)"/home/jos/tmp/testlink") << endl;
    }
    else
    {
        cout << "error changing linktarget" << endl;
    }
    w.actionDeleteFile((char*)"/home/jos/tmp/movedTest.txt");
    if (w.getFileExists("/home/jos/tmp/movedTest.txt"))
    {
        cout << "error deleting testfile" << endl;
    }
    else
    {
        cout << "testfile deleted" << endl;
    }
    w.actionCreatedir((char*)"/home/jos/pool2/home/jos/tmp");
    if (w.getFileExists((char*)"/home/jos/pool2/home/jos/tmp"))
    {
        cout << "testdir created" << endl;
    }
    else
    {
        cout << "error creating dir" << endl;
    }
}

void ndirtest()
{
    vector<pooldir_t> dirs = w.getNdirs(2); 
    vector<pooldir_t>::iterator iter;
    cout << "the two most empty dirs are: " << endl;
    for(iter=dirs.begin(); iter != dirs.end(); iter++)
    {
        cout << iter->usedPerc << "  :  " << iter->path << endl;
    }
}

void testFunctions()
{
    cout << "TEST FUNCTIONS" << endl;
    flexdir_t fd;
    flexfile_t ff;
    Worker wo = w;
    system("touch /home/jos/tmp/test1.txt");
    w.getFlexStructFromPath(&fd, &ff, "/home/jos/tmp/test1.txt");
    cout << "Flex-structure: /home/jos/tmp/test1.txt" << endl;
    cout << "flexdir path: " <<  fd.path << endl;
    cout << "flexfile name: " << ff.name << endl;
    pooldir_t pd;
    poolfile_t pf;
    system("touch /home/jos/pool1/home/jos/tmp/test1.txt");
    w.getPoolStructFromPath(&pd, &pf, "/home/jos/pool1/home/jos/tmp/test1.txt");
    cout << "Pool-structure: /home/jos/pool1/home/jos/tmp/test1.txt" << endl;
    cout << "pooldir path: " << pd.path << endl;
    cout << "poolfile name: " << pf.name << endl;
    cout << "poolfile p_path: " << pf.p_path << endl;
    cout << "poolfile x_path: " << pf.x_path << endl;
}

void testTasks()
{
    task_t t;
    for (int j = 0; j<2;j++)
    {
        for (int i = 0; i<1;i++)
        {
            stringstream ss;
            ss << j << i;
            t.ID = SYNC;
            t.from = "/home/jos/tmp/from"+ss.str()+".txt";
            string file = "touch " + t.from;
            system((char*) file.c_str());
            t.to = "/home/jos/pool1/to"+ss.str()+".txt";
            pthread_mutex_lock(&mutex);
            w.addTask(t.ID, t.from, t.to);
            cout << "TEST: task added: " << j << i << endl;
            pthread_mutex_unlock(&mutex);
        }
        pthread_cond_signal(&condition);
    }
    for (int j = 0; j<2;j++)
    {
        for (int i = 0; i<1;i++)
        {
            stringstream ss;
            ss << j << i;
            t.ID = DELETE;
            t.from = "/home/jos/tmp/from"+ss.str()+".txt";
            t.to = "/home/jos/pool1/to"+ss.str()+".txt";
            if(!w.getFileExists((char*)t.from.c_str()))
            {
                cout << "TEST: ERROR file: " << t.from << " does not exists" << endl;
            }
            if(!w.getFileExists((char*)t.to.c_str()))
            {
                cout << "TEST: ERROR file: " << t.to << " does not exists" << endl;
            }
            pthread_mutex_lock(&mutex);
            w.addTask(DELETE, t.from, " ");
            w.addTask(DELETE, t.to, " ");
            cout << "TEST: delete task added: " << t.from << endl;
            cout << "TEST: delete task added: " << t.to << endl;
            pthread_mutex_unlock(&mutex);
        }
    }
    pthread_cond_signal(&condition);
    system("touch /home/jos/tmp/addTest.txt");
    cout << "TEST add task" << endl;
    pthread_mutex_lock(&mutex);
    w.addTask(ADD, "/home/jos/tmp/addTest.txt", " ");
    pthread_mutex_unlock(&mutex);
    pthread_cond_signal(&condition);
    system("touch /home/jos/pool1/home/jos/tmp/removeTest.txt");
    pthread_mutex_lock(&mutex);
    w.addTask(REMOVE, "/home/jos/pool1/home/jos/tmp/removeTest.txt", " ");
    pthread_mutex_unlock(&mutex);
    pthread_cond_signal(&condition);
}
