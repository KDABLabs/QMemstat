#ifndef PROCESSINFO_H
#define PROCESSINFO_H

#include <cstdint>
#include <string>
#include <vector>

struct ProcessPid
{
    uint pid;
    std::string name;
};

// Not a map because there are several ways to match with certain special cases like for shellscripts,
// so the "natural" interface is a list on which one can do arbitrary matching.
std::vector<ProcessPid> readProcessList();

#endif // PROCESSINFO_H
