#include "processinfo.h"

// POSIX specific, but this whole program only works on Linux anyway!
#include <sys/types.h>
#include <dirent.h>

// Linux specific, obviously
#include <linux/kernel-page-flags.h>

using namespace std;

vector<ProcessPid> fetchProcessList()
{
    vector<ProcessPid> ret;

    DIR *dp = opendir ("/proc");
    if (!dp) {
        perror ("Couldn't open the /proc directory");
        exit(-1);
    }

    const string procPrefix = "/proc/";
    const string statSuffix = "/stat";
    string nameFromStat;
    ProcessPid pp;
    // For scripts and in certain other situations, /proc/<pid>/cmdline should be considered which we don't
    // do - see pidof.c from the procps-ng for how to do it 100% correctly. It can probably be said that
    // pidof is correct by definition.
    while (dirent *ep = readdir(dp)) {
        // zero is not a valid pid and also the error return value of strtoul...
        pp.pid = strtoul(ep->d_name, nullptr, 10);
        if (!pp.pid) {
            continue;
        }
        ifstream statFile(procPrefix + ep->d_name + statSuffix);
        if (!statFile.is_open()) {
            continue; // probably a harmless race - the process went away
        }
        statFile >> /* throw away the first value */ nameFromStat >> nameFromStat;
        if (nameFromStat.length() < 2) {
            continue;
        }
        // "(my_process)" -> "my_process"
        pp.name = nameFromStat.substr(1, nameFromStat.length() - 2);
        ret.push_back(pp);
    }

    closedir (dp);

    return ret;
}
