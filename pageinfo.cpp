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

using namespace std;

struct MappedRegion
{
    uintptr_t start;
    uintptr_t end;
    vector<uint64_t> pagemapEntries;
    vector<uint32_t> useCounts;
    vector<uint32_t> combinedFlags;
};

static vector<MappedRegion> readMappedRegions(uint pid)
{
    vector<MappedRegion> ret;
    ostringstream mapsName;
    mapsName << "/proc/" << pid << "/maps";

    ifstream mapsFile(mapsName.str());
    if (!mapsFile.is_open()) {
        return ret; // TODO error msg
    }
    string mapLine;
    while (getline(mapsFile, mapLine)) {
        // cout << mapLine << '\n';
        MappedRegion region;
        region.start = 0;
        region.end = 0;
        sscanf(mapLine.c_str(), "%p-%p", reinterpret_cast<void **>(&region.start),
                                         reinterpret_cast<void **>(&region.end));
        // cout << region.start << " - " << region.end << "\n";
        ret.push_back(region);
    }
    return ret;
}

static uint64_t pfnForPagemapEntry(uint64_t pmEntry)
{
    return (pmEntry & PM_PRESENT) ? PM_PFRAME(pmEntry) : 0;
}

// return value: unsorted list of all seen and present PFNs
static vector<uint64_t> readPagemap(uint pid, vector<MappedRegion> *mappedRegions)
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

    for (MappedRegion &region : *mappedRegions) {
        const size_t pageCount = (region.end - region.start) / pageSize;
        region.pagemapEntries.resize(pageCount);
        region.useCounts.resize(pageCount);
        region.combinedFlags.resize(pageCount);

        pagemap.seekg(region.start / pageSize * pageFlagsSize, pagemap.beg);
        pagemap.read(reinterpret_cast<char *>(&region.pagemapEntries[0]),
                     ((region.end - region.start) / pageSize * pageFlagsSize));

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

// PFN: page frame number, a kind of unique identifier inside the kerne paging subsystem
struct PfnRange
{
    uint64_t useCountForPfn(uint64_t pfn) const
    {
        //cout << "start " << start << " pfn " << pfn << " last " << last << '\n';
        assert(pfn >= start && pfn <= last);
        return useCounts[pfn - start];
    }

    uint64_t flagsForPfn(uint64_t pfn) const
    {
        //cout << "start " << start << " pfn " << pfn << " last " << last << '\n';
        assert(pfn >= start && pfn <= last);
        return flags[pfn - start];
    }

    bool operator<(const PfnRange &other) const { return start < other.start; }
    bool operator<(uint64_t pfn) const { return start < pfn; }

    static const uint64_t maxGapSize = 512;

    uint64_t start;
    uint64_t last;
    vector<uint64_t> useCounts;
    vector<uint64_t> flags;
};

static vector<PfnRange> rangifyPfns(const vector<uint64_t> &pfns)
{
    vector<PfnRange> ret;
    // create reasonably sized chunks to read
    if (!pfns.empty()) {
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
    }
    cout << "PFN range count: " << ret.size() << '\n';
    return ret;
}

// read kpagemap and kpagecount
static void readUseCountsAndFlags(vector<PfnRange> *pfnRanges)
{
    uint64_t readTotal;

    ifstream kpagecount;
    ifstream kpageflags;
    kpagecount.rdbuf()->pubsetbuf(nullptr, 0);
    kpageflags.rdbuf()->pubsetbuf(nullptr, 0);
    kpagecount.open("/proc/kpagecount", ifstream::binary);
    kpageflags.open("/proc/kpageflags", ifstream::binary);
    if (!kpagecount.is_open() || !kpageflags.is_open()) {
        return -1; // TODO error msg
    }

    for (PfnRange &range : *pfnRanges) {
        size_t count = range.last - range.start + 1;
        readTotal += count * 2 * pageFlagsSize;
        range.useCounts.resize(count);
        range.flags.resize(count);

        kpagecount.seekg(range.start * pageFlagsSize, kpagecount.beg);
        kpageflags.seekg(range.start * pageFlagsSize, kpageflags.beg);

        kpagecount.read(reinterpret_cast<char *>(&range.useCounts[0]),
                        count * pageFlagsSize);
        kpageflags.read(reinterpret_cast<char *>(&range.flags[0]),
                        count * pageFlagsSize);
    }

    cout << "PFN ranges total read bytes: " << readTotal << '\n';
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
    void findRangeForPfn(uint64_t pfn) const;
    vector<PfnRange> m_data;
    mutable vector<PfnRange>::const_iterator m_cachedRange;
};

void PfnInfos::findRangeForPfn(uint64_t pfn) const
{
    // we're making the assumption that the pfn *is* contained in one of the ranges!
    if (pfn >= m_cachedRange->start && pfn <= m_cachedRange->last) {
        // fast path: it's in the same range as last PFN we were asked for
        return;
    }
    // binary search
    m_cachedRange = lower_bound(m_data.begin(), m_data.end(), pfn);
}

uint64_t PfnInfos::useCount(uint64_t pfn) const
{
    findRangeForPfn(pfn);
    return m_cachedRange->useCountForPfn(pfn);
}

uint64_t PfnInfos::flags(uint64_t pfn) const
{
    findRangeForPfn(pfn);
    return m_cachedRange->flagsForPfn(pfn);
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

    vector<MappedRegion> mappedRegions = readMappedRegions(pid);
    vector<uint64_t> seenPfns = readPagemap(pid, &mappedRegions);

    sort(seenPfns.begin(), seenPfns.end());
    // remove duplicates
    seenPfns.erase(unique(seenPfns.begin(), seenPfns.end()), seenPfns.end());
    cout << "PFN count: " << seenPfns.size() << '\n';

    vector<PfnRange> pfnRanges = rangifyPfns(seenPfns);
    readUseCountsAndFlags(&pfnRanges);
    PfnInfos pfnInfos(move(pfnRanges));

    for (MappedRegion &mappedRegion : mappedRegions) {
        for (size_t i = 0; i < mappedRegion.pagemapEntries.size(); i++) {
            const uint64_t pfn = pfnForPagemapEntry(region.pagemapEntries[i]);
            if (pfn) {
                mappedRegion.useCounts[i] = uint32_t(pfnInfos.useCount(i));
                mappedRegion.combinedFlags[i] = mappedRegion.combinedFlags[i] |
                                                uint32_t(pfnInfos.flags(i));
            }
        }
        // don't need them anymore
        mappedRegion.pagemapEntries.clear();
    }

}
