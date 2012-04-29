#include <iostream>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <pthread.h>
#include "worker.h"
#include "checker.h"

using namespace std;

Worker w;
Checker ch(&w);
void printHelp();
void printStatus();
void printFileStructure();
void repair();
void resync();

int main(int argc, char * argv[])
{
    //print log entries
    w.getSettings()->verbose = true;
    //print help message when no arguments
    if (argc == 1)
    {
        printHelp();
    }
    else
    {
        for (int i = 1; i < argc; i++)
        {
            string arg = (string)argv[i];
            if (arg == "-d")
            {
                ch.debug();
            }
            else if (arg == "-t")
            {
                w.printSettings();
            }
            else if (arg == "-f")
            {
                printFileStructure();
            }
            else if (arg == "-h")
            {
                printHelp();
            }
            else if (arg == "-s")
            {
                printStatus();
            }
            else if (arg == "-r")
            {
                repair();
            }
            else if (arg == "-y")
            {
                resync();
            }
        }
    }
    return 0;
}

void printHelp()
{
    cout << "Flexdir options:" << endl;
    cout << "-f : print filesystem as xml" << endl;
    cout << "-h : print this page" << endl;
    cout << "-r : repair errors" << endl;
    cout << "-s : print status" << endl;
    cout << "-t : print settings as xml" << endl;
    cout << "-y : resync extra copies" << endl;
}

void printStatus()
{
    w.loadFileStructure();
    ch.analyze();
    int errors = ch.getErrorCount();
    cout << "Status: ";
    if (errors > 0)
    {
        cout << errors << " errors found, run flexdir -r to repair" << endl;
    }
    else
    {
        cout << "OK" << endl;
    }
}

void printFileStructure()
{
    w.loadFileStructure();
    w.getPoolSizes();
    ch.analyze();
    w.printFileStructure();
}

void repair()
{
    w.loadFileStructure();
    ch.analyze();
    if(ch.getErrorCount() > 0)
    {
        if(ch.repair(true)==0)
        {
            repair();
        }
    }
}

void resync()
{
    w.loadFileStructure();
    ch.resync();
}
