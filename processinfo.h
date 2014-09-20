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

std::vector<ProcessPid> fetchProcessList();

#endif // PROCESSINFO_H
