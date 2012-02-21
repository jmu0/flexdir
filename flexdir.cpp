#include <iostream>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include "worker.h"
#include "checker.h"

using namespace std;

Worker w;
Checker ch(&w);

int main()
{
    cout << "flexdir CLI" << endl;
    return 0;
}

