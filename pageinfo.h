#ifndef PAGEINFO_H
#define PAGEINFO_H

#include <cstdint>
#include <vector>

// TODO clean up the interface:
// - iterator?
// - explain and / or export page size (==4096)
// - vector of struct, instead of struct of vectors?
// - tell the backing file in case there is one (mmap!)

struct MappedRegion
{
    uintptr_t start;
    uintptr_t end;
    std::vector<uint32_t> useCounts;
    std::vector<uint32_t> combinedFlags;
    bool operator<(const MappedRegion &other) const { return start < other.start; }
};

class PageInfo
{
public:
    static const unsigned int pageShift = 12;
    static const unsigned int pageSize = 1 << pageShift; // the well-known 4096 bytes

    PageInfo(unsigned int pid);
    const std::vector<MappedRegion> &mappedRegions() const { return m_mappedRegions; }
private:
    std::vector<MappedRegion> m_mappedRegions;
};

#endif // PAGEINFO_H
