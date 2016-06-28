#include "pageinfo.h"

#include <algorithm>
#include <cassert>
#include <cinttypes>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

// POSIX specific, but this whole program only works on Linux anyway!
#include <sys/types.h>
#include <dirent.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

// Linux specific, obviously
#include "kernel-page-flags.h"
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
        char filename[PATH_MAX];
        filename[0] = '\0';

        sscanf(mapLine.c_str(), "%" SCNx64 "-%" SCNx64 " %*4s %*x %*5s %*d %s",
               &region.start, &region.end, filename);
        region.backingFile = string(filename);

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

    ostringstream pagemapNameStream;
    pagemapNameStream << "/proc/" << pid << "/pagemap";
    string pagemapName = pagemapNameStream.str();

    // using Linux API for reading isn't a huge win here, but it's somewhat faster and easier on
    // the eyes than fstream API, too, so...
    int pagemapFd = open(pagemapName.c_str(), O_RDONLY);
    if (pagemapFd < 0) {
        return ret; // TODO error reporting
    }

    for (MappedRegionInternal &region : *mappedRegions) {
        const size_t pageCount = (region.end - region.start) / PageInfo::pageSize;
        region.pagemapEntries.resize(pageCount);
        region.useCounts.resize(pageCount);
        region.combinedFlags.resize(pageCount);

        pread64(pagemapFd, &region.pagemapEntries[0],
                (region.end - region.start) / PageInfo::pageSize * pageFlagsSize,
                region.start / PageInfo::pageSize * pageFlagsSize);

        for (size_t i = 0; i < pageCount; i++) {
            const uint64_t pageBits = region.pagemapEntries[i];
            const uint64_t pfn = pfnForPagemapEntry(pageBits);
            if (pfn) {
                ret.push_back(pfn);
            }
            // copy pagemap flag bits into combined flags as follows:
            // 55-> 28 ; 61 -> 29 ; 62 -> 30 ; 63 -> 31
            region.combinedFlags[i] = ((pageBits >> 27) & 0x10000000) | // shift and mask bit 55 to bit 28
                                      ((pageBits >> 32) & 0xe0000000); // shift and mask upper 3 bits
            // flags from /proc/kpageflags are added later
        }
    }
    close(pagemapFd);
    return ret;
}

// PFN: page frame number, a kind of unique identifier inside the kernel paging subsystem
struct PfnRange
{
    uint64_t useCount(uint64_t *buffer, uint64_t pfn) const
    {
        assert(pfn >= start && pfn <= last);
        return buffer[m_useCountsBufferOffset + pfn - start];
    }

    uint64_t flags(uint64_t *buffer, uint64_t pfn) const
    {
        assert(pfn >= start && pfn <= last);
        return buffer[m_flagsBufferOffset + pfn - start];
    }

    bool operator<(const PfnRange &other) const { return last < other.last; }
    // comparing pfn to last so lower_bound immediately finds the right range; same above for consistency
    bool operator<(uint64_t pfn) const { return last < pfn; }

    void allocBufferSpace(size_t *bufferPos)
    {
        const size_t count = last - start + 1;
        m_useCountsBufferOffset = *bufferPos;
        *bufferPos += count;
        m_flagsBufferOffset = *bufferPos;
        *bufferPos += count;
    }

    // maxGapSize has been determined empirically (basically watching "time" output when mapping some
    // largish process) - one would think that much larger values help because every read() is a syscall
    // and therefore expensive... but no, so let's just waste a little less memory from uselessly reading
    // gaps between PFN entries that we want.
    // Note: one possible speed advantage of not reading too much is that the kernel must generate output
    //       even for inexistent PFNs, which looks kind of but not very expensive to do. See Linux kernel
    //       functions: kpagecount_read(), kpageflags_read() in linux/fs/proc/page.c
    //       - note that copy_to_user also has a (not very large, some flag tests and memcpy) cost
    static const uint64_t maxGapSize = 16;

    uint64_t start;
    uint64_t last;
    size_t m_useCountsBufferOffset;
    size_t m_flagsBufferOffset;
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

    // create reasonably sized ranges to read
    // ### Optimization: allocate memory for all ranges en bloc and store offsets into the
    //     allocated memory in the ranges. This is a surprisingly large performance win -
    //     it reduces the time for the whole PageInfo generation by roughly 40%.
    //     Benefits are cache locality, one less layer of indirection, avoidance of malloc() and
    //     free() calls, and avoidance of vector<uint64_t>::resize() uselessly initializing data.
    size_t rangesStoragePos = 0;
    PfnRange range;
    range.start = pfns.front();
    range.last = pfns.front();
    for (uint64_t pfn : pfns) {
        if (pfn > range.last + PfnRange::maxGapSize) {
            // found a big gap, store previous range and start a new one
            range.allocBufferSpace(&rangesStoragePos);
            ret.push_back(range);
            range.start = pfn;
         }
         range.last = pfn;
    }
    range.allocBufferSpace(&rangesStoragePos);
    ret.push_back(range);

    return ret;
}

class PfnInfos
{
public:
    PfnInfos(vector<PfnRange> data)
       : m_ranges(data),
         m_buffer(nullptr),
         m_cachedRange(m_ranges.begin())
    {
        readUseCountsAndFlags();
    }

    ~PfnInfos() { if (m_buffer) free(m_buffer); }

    uint64_t useCount(uint64_t pfn) const;
    uint64_t flags(uint64_t pfn) const;

private:
    void readUseCountsAndFlags();
    void findRange(uint64_t pfn) const;
    vector<PfnRange> m_ranges;
    uint64_t *m_buffer;
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
    m_cachedRange = lower_bound(m_ranges.begin(), m_ranges.end(), pfn);
    assert(m_cachedRange != m_ranges.end());
}

uint64_t PfnInfos::useCount(uint64_t pfn) const
{
    findRange(pfn);
    return m_cachedRange->useCount(m_buffer, pfn);
}

uint64_t PfnInfos::flags(uint64_t pfn) const
{
    findRange(pfn);
    return m_cachedRange->flags(m_buffer, pfn);
}

// read kpagemap and kpagecount
void PfnInfos::readUseCountsAndFlags()
{
    assert(!m_buffer);
    if (m_ranges.empty()) {
        return;
    }

    // Extract buffer size from m_ranges using a little shortcut
    const PfnRange &lastRange = m_ranges.back();
    const size_t allocSize = (lastRange.m_flagsBufferOffset +
                               (lastRange.m_flagsBufferOffset - lastRange.m_useCountsBufferOffset)) *
                             pageFlagsSize;
    m_buffer = static_cast<uint64_t *>(malloc(allocSize));

    // ### this function takes about half the CPU time of a whole data gathering pass when using
    //     std::ifstream, and since we're tied to Linux anyway, just use Linux API (note: it only
    //     shaves off about 30% of this function's execution time - syscalls take the longest time!!)
    int kpagecountFd = open("/proc/kpagecount", O_RDONLY);
    int kpageflagsFd = open("/proc/kpageflags", O_RDONLY);
    if (kpagecountFd < 0 || kpageflagsFd < 0) {
        return; // TODO error reporting
    }

    uint64_t readTotal = 0;

    for (PfnRange &range : m_ranges) {
        size_t count = range.last - range.start + 1;
        readTotal += count * 2 * pageFlagsSize;

        pread64(kpagecountFd, m_buffer + range.m_useCountsBufferOffset,
                count * pageFlagsSize, range.start * pageFlagsSize);
        pread64(kpageflagsFd, m_buffer + range.m_flagsBufferOffset,
                count * pageFlagsSize, range.start * pageFlagsSize);
    }

    assert(readTotal == allocSize);
    // cout << "PFN ranges total read bytes: " << readTotal << '\n';

    close(kpagecountFd);
    close(kpageflagsFd);
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

    {
        vector<MappedRegionInternal> mappedRegions = readMappedRegions(pid);
        vector<uint64_t> pagemap = readPagemap(pid, &mappedRegions);
        if (pagemap.empty()) {
            // usual cause: couldn't read pagemap due to lack of permissions (user is not root)
            return;
        }
        vector<PfnRange> pfnRanges = rangifyPfns(move(pagemap));
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

            MappedRegion publicMappedRegion = { mappedRegion.start, mappedRegion.end,
                                                move(mappedRegion.backingFile),
                                                move(mappedRegion.useCounts),
                                                move(mappedRegion.combinedFlags) };
            m_mappedRegions.push_back(move(publicMappedRegion));
        }
    }

    // this should be a no-op, but why not make sure... it make little performance difference.
    sort(m_mappedRegions.begin(), m_mappedRegions.end());

    for (const MappedRegion &mappedRegion : m_mappedRegions) {
        assert(mappedRegion.start < mappedRegion.end);
    }

    // ### regions can sometimes overlap(!), presumably due to data races in the kernel when watching
    // a running process. Just assign any overlapping area to the first region to "claim" it, i.e. the
    // one with the smallest start address.
    for (size_t i = 1; i < m_mappedRegions.size(); i++) {
        if (m_mappedRegions[i].start < m_mappedRegions[i - 1].end) {
            cout << "correcting " << hex << m_mappedRegions[i - 1].start << " " << hex << m_mappedRegions[i - 1].end << " "
                                  << m_mappedRegions[i].start << " " << hex << m_mappedRegions[i].end << endl;
            uint32_t prevStart = m_mappedRegions[i].start;
            m_mappedRegions[i].start = m_mappedRegions[i - 1].end;
            if (m_mappedRegions[i].start >= m_mappedRegions[i].end) {
                // This renders the range inert... might be better to remove it altogether.
                // Note that we move the end instead of the start, to maintain the invariant that the
                // start address of region n+1 is >= end address of region n.
                m_mappedRegions[i].end = m_mappedRegions[i].start;
                m_mappedRegions[i].useCounts.clear();
                m_mappedRegions[i].combinedFlags.clear();
            } else if (!m_mappedRegions[i].useCounts.empty()) {
                const size_t delCount = (m_mappedRegions[i].start - prevStart) / pageSize;
                m_mappedRegions[i].useCounts.erase(m_mappedRegions[i].useCounts.begin(),
                                                   m_mappedRegions[i].useCounts.begin() + delCount);
                m_mappedRegions[i].combinedFlags.erase(m_mappedRegions[i].combinedFlags.begin(),
                                                       m_mappedRegions[i].combinedFlags.begin() + delCount);
            }
            cout << "corrected  " << hex << m_mappedRegions[i - 1].start << hex << " " << m_mappedRegions[i - 1].end << " "
                 << m_mappedRegions[i].start << " " << hex << m_mappedRegions[i].end << endl;
        }
    }
}
