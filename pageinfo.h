#ifndef PAGEINFO_H
#define PAGEINFO_H

#include <cstdint>
#include <utility>
#include <vector>

class PageInfo
{
public:
    PageInfo(unsigned int pid);

    std::vector<std::pair<uint64_t, uint64_t>> mappedRanges() const;
    uint64_t useCountForPageAt(uint64_t address);
    uint64_t flagsForPageAt(uint64_t address);

private:


};

#endif // PAGEINFO_H
