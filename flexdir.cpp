#include <iostream>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include "worker.h"
#include "checker.h"

using namespace std;

Worker w;
Checker ch(&w);
void printHelp();

int main(int argc, char * argv[])
{

    Worker wo = w;//TODO: delete this line
    if (argc == 1)
    {
        printHelp();
    }
    else
    {
        for (int i = 1; i < argc; i++)
        {
            string arg = (string)argv[i];
            if (arg == "-s")
            {
                w.printSettings();
            }
            else if (arg == "-f")
            {
                w.loadFileStructure();
                w.getPoolSizes();
                ch.analyze();
                w.printFileStructure();
            }
            else if (arg == "-h")
            {
                printHelp();
            }
            else if (arg == "-t")
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
            else if (arg == "-r")
            {
                w.loadFileStructure();
                ch.analyze();
                ch.repair(true);
            }
            else if (arg == "-y")
            {
                w.loadFileStructure();
                ch.resync(true);
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
    cout << "-s : print settings as xml" << endl;
    cout << "-t : print status" << endl;
}
