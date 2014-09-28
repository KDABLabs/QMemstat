#include "pageinfo.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

// POSIX specific, but this whole program only works on Linux anyway!
#include <sys/types.h>
#include <dirent.h>

// Linux specific, obviously
#include <linux/kernel-page-flags.h>

#include "linux-pm-bits.h"

using namespace std;

static const uint pageFlagsSize = sizeof(uint64_t); // aka 64 bits aka 8 bytes

struct MappedRegionInternal : MappedRegion
{
    // we only need this while we're connecting the different data sources, not afterwards
    vector<uint64_t> pagemapEntries;
};

static vector<MappedRegionInternal> readMappedRegions(uint pid)
{
    vector<MappedRegionInternal> ret;
    ostringstream mapsName;
    mapsName << "/proc/" << pid << "/maps";

    ifstream mapsFile(mapsName.str());
    if (!mapsFile.is_open()) {
        return ret; // TODO error msg
    }
    string mapLine;
    while (getline(mapsFile, mapLine)) {
        // cout << mapLine << '\n';
        MappedRegionInternal region;
        region.start = 0;
        region.end = 0;
        sscanf(mapLine.c_str(), "%p-%p", reinterpret_cast<void **>(&region.start),
                                         reinterpret_cast<void **>(&region.end));
        ret.push_back(region);
    }
    return ret;
}

static uint64_t pfnForPagemapEntry(uint64_t pmEntry)
{
    return (pmEntry & PM_PRESENT) ? PM_PFRAME(pmEntry) : 0;
}

// return value: unsorted list of all seen and present PFNs
static vector<uint64_t> readPagemap(uint pid, vector<MappedRegionInternal> *mappedRegions)
{
    vector<uint64_t> ret;
    ostringstream pagemapName;
    pagemapName << "/proc/" << pid << "/pagemap";

    ifstream pagemap;
    // disable buffering - the file is not regular at all!
    pagemap.rdbuf()->pubsetbuf(nullptr, 0);
    pagemap.open(pagemapName.str(), ifstream::binary);
    if (!pagemap.is_open()) {
        return ret; // TODO error msg
    }

    for (MappedRegionInternal &region : *mappedRegions) {
        const size_t pageCount = (region.end - region.start) / PageInfo::pageSize;
        region.pagemapEntries.resize(pageCount);
        region.useCounts.resize(pageCount);
        region.combinedFlags.resize(pageCount);

        pagemap.seekg(region.start / PageInfo::pageSize * pageFlagsSize, pagemap.beg);
        pagemap.read(reinterpret_cast<char *>(&region.pagemapEntries[0]),
                     ((region.end - region.start) / PageInfo::pageSize * pageFlagsSize));

        for (int i = 0; i < pageCount; i++) {
            const uint64_t pageBits = region.pagemapEntries[i];
            const uint64_t pfn = pfnForPagemapEntry(pageBits);
            if (pfn) {
                ret.push_back(pfn);
            }
            // copy pagemap flag bits into combined flags as follows:
            // 63 -> 31; 62 -> 30; 61 -> 29; 55-> 28
            region.combinedFlags[i] = ((pageBits >> 32) & 0xe0000000) | // shift and mask upper 3 bits
                                      ((pageBits >> 27) & 0x10000000); // shift and mask bit 55 to bit 28
            // flags from /proc/kpageflags are added later
        }
    }
    return ret;
}

// PFN: page frame number, a kind of unique identifier inside the kernel paging subsystem
struct PfnRange
{
    uint64_t useCount(uint64_t pfn) const
    {
        assert(pfn >= start && pfn <= last);
        return m_useCounts[pfn - start];
    }

    uint64_t flags(uint64_t pfn) const
    {
        assert(pfn >= start && pfn <= last);
        return m_flags[pfn - start];
    }

    bool operator<(const PfnRange &other) const { return last < other.last; }
    bool operator<(uint64_t pfn) const { return last < pfn; }

    // maxGapSize has been determined empirically (basically watching "time" output when mapping some
    // largish process) - one would think that much larger values help because every read() is a syscall
    // and therefore expensive... but no, so let's just waste a little less memory from uselessly reading
    // gaps between PFN entries that we want.
    static const uint64_t maxGapSize = 16;

    uint64_t start;
    uint64_t last;
    vector<uint64_t> m_useCounts; // for consistency...
    vector<uint64_t> m_flags; // disambiguate from method name, yuck
};

static vector<PfnRange> rangifyPfns(vector<uint64_t> pfns)
{
    vector<PfnRange> ret;
    if (pfns.empty()) {
        return ret; // pfns.front() would blow up
    }

    sort(pfns.begin(), pfns.end());
    // remove duplicates
    pfns.erase(unique(pfns.begin(), pfns.end()), pfns.end());

    // create reasonably sized chunks to read
    PfnRange range;
    range.start = pfns.front();
    range.last = pfns.front();
    for (uint64_t pfn : pfns) {
        if (pfn > range.last + PfnRange::maxGapSize) {
            // found a big gap, store previous range and start a new one
            ret.push_back(range);
            range.start = pfn;
            range.last = pfn;
        } else {
            // expand range
            range.last = pfn;
        }
    }
    ret.push_back(range);

    return ret;
}

// read kpagemap and kpagecount
static void readUseCountsAndFlags(vector<PfnRange> *pfnRanges)
{
    uint64_t readTotal = 0;

    ifstream kpagecount;
    ifstream kpageflags;
    kpagecount.rdbuf()->pubsetbuf(nullptr, 0);
    kpageflags.rdbuf()->pubsetbuf(nullptr, 0);
    kpagecount.open("/proc/kpagecount", ifstream::binary);
    kpageflags.open("/proc/kpageflags", ifstream::binary);
    if (!kpagecount.is_open() || !kpageflags.is_open()) {
        return; // TODO error reporting
    }

    for (PfnRange &range : *pfnRanges) {
        size_t count = range.last - range.start + 1;
        readTotal += count * 2 * pageFlagsSize;
        range.m_useCounts.resize(count);
        range.m_flags.resize(count);

        kpagecount.seekg(range.start * pageFlagsSize, kpagecount.beg);
        kpageflags.seekg(range.start * pageFlagsSize, kpageflags.beg);

        kpagecount.read(reinterpret_cast<char *>(&range.m_useCounts[0]),
                        count * pageFlagsSize);
        kpageflags.read(reinterpret_cast<char *>(&range.m_flags[0]),
                        count * pageFlagsSize);
    }

    // cout << "PFN ranges total read bytes: " << readTotal << '\n';
}

class PfnInfos
{
public:
    PfnInfos(vector<PfnRange> data)
       : m_data(data),
         m_cachedRange(m_data.begin())
    {}
    uint64_t useCount(uint64_t pfn) const;
    uint64_t flags(uint64_t pfn) const;
private:
    void findRange(uint64_t pfn) const;
    vector<PfnRange> m_data;
    mutable vector<PfnRange>::const_iterator m_cachedRange;
};

void PfnInfos::findRange(uint64_t pfn) const
{
    // we're making the assumption that the pfn *is* contained in one of the ranges!
    if (pfn >= m_cachedRange->start && pfn <= m_cachedRange->last) {
        // fast path: it's in the same range as last PFN we were asked for
        return;
    }
    // binary search
    m_cachedRange = lower_bound(m_data.begin(), m_data.end(), pfn);
    assert(m_cachedRange != m_data.end());
}

uint64_t PfnInfos::useCount(uint64_t pfn) const
{
    findRange(pfn);
    return m_cachedRange->useCount(pfn);
}

uint64_t PfnInfos::flags(uint64_t pfn) const
{
    findRange(pfn);
    return m_cachedRange->flags(pfn);
}

PageInfo::PageInfo(uint pid)
{
    // - read information about mapped ranges, from /proc/<pid>/maps
    // - read mapping of addresses to (PFNs and certain flags), from /proc/<pid>/pagemap
    // - read flags (from /proc/kpageflags) and use counts (from /proc/kpagecount) for PFNs
    // - using the mapping of addresses to PFNs, store use counts and flags under addresses
    //   because PFNs are rather kernel-internal and of little interest outside
    // - we can now retrieve flags and use count for a page at a given (virtual) address
    // - profit!

    vector<MappedRegionInternal> mappedRegions = readMappedRegions(pid);
    vector<PfnRange> pfnRanges = rangifyPfns(readPagemap(pid, &mappedRegions));
    readUseCountsAndFlags(&pfnRanges);
    PfnInfos pfnInfos(move(pfnRanges));

    for (MappedRegionInternal &mappedRegion : mappedRegions) {
        for (size_t i = 0; i < mappedRegion.pagemapEntries.size(); i++) {
            const uint64_t pfn = pfnForPagemapEntry(mappedRegion.pagemapEntries[i]);
            if (pfn) {
                mappedRegion.useCounts[i] = uint32_t(pfnInfos.useCount(pfn));
                mappedRegion.combinedFlags[i] = mappedRegion.combinedFlags[i] |
                                                uint32_t(pfnInfos.flags(pfn));
            }
        }

        // don't need them anymore - this reduces peak memory allocation a bit
        mappedRegion.pagemapEntries.clear();

        MappedRegion publicMappedRegion;
        publicMappedRegion.start = mappedRegion.start;
        publicMappedRegion.end = mappedRegion.end;
        publicMappedRegion.useCounts = move(mappedRegion.useCounts);
        publicMappedRegion.combinedFlags = move(mappedRegion.combinedFlags);
        m_mappedRegions.emplace_back(move(publicMappedRegion));
    }

    // this should be a no-op, but why not make sure... it make little performance difference.
    sort(m_mappedRegions.begin(), m_mappedRegions.end());
}
