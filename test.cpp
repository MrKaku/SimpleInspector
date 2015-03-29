#include <stdio.h>
#include <string>
#include <boost/bind.hpp>
#include "simpleinspector.h"


using namespace std;


string GetProcInfo(const string& str)
{
    FILE* fp = fopen(str.c_str(), "rt");

    char buf[2048] = {0};
    fread(buf, sizeof buf, 1, fp);
    if(fp)
    {
        fclose(fp);
    }
    return buf;
}


int main()
{  
    SimpleInspector si(8888);

    si.AddInspectee("/ProcStatus", boost::bind(GetProcInfo, "/proc/self/status"));
    si.AddInspectee("/ProcStat", boost::bind(GetProcInfo, "/proc/self/stat"));

    si.RunInThread();


    sleep(600);
    return 0;
}

