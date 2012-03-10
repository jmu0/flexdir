#include <iostream>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include "worker.h"
#include "checker.h"

using namespace std;

Worker w;
Checker ch(&w);

int main(int argc, char * argv[])
{

    Worker wo = w;//TODO: delete this line
    for (int i = 1; i < argc; i++)
    {
        if ((string)argv[i] == "-s")
        {
            w.printSettings();
        }
        else if ((string)argv[i] == "-f")
        {
            w.loadFileStructure();
            w.getPoolSizes();
            ch.analyze();
            w.printFileStructure();
        }
    }
    return 0;
}

