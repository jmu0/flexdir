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
void * wThread(void * id);
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition = PTHREAD_COND_INITIALIZER;

int main()
{
    s = w.getSettings();
    s->verbose = true;   
    startThreads();
    //symlinkTest();
    //fileTest();
    //w.printSettings();    
    //w.printFileStructure();
    //ndirtest();
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

void testTasks()
{
    task_t t;
    for (int j = 0; j<5;j++)
    {
        for (int i = 0; i<5;i++)
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
        sleep(1);
    }
    for (int j = 0; j<5;j++)
    {
        for (int i = 0; i<5;i++)
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
}
