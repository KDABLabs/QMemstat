#include "processinfo.h"
#include "pageinfo.h"

#include <cstdio>
#include <iostream>

// from the kernel - instead of decrypting all that crap, let's just use it

#define PM_ENTRY_BYTES      sizeof(uint64_t)
#define PM_STATUS_BITS      3
#define PM_STATUS_OFFSET    (64 - PM_STATUS_BITS)
#define PM_STATUS_MASK      (((1LL << PM_STATUS_BITS) - 1) << PM_STATUS_OFFSET)
#define PM_STATUS(nr)       (((nr) << PM_STATUS_OFFSET) & PM_STATUS_MASK)
#define PM_PSHIFT_BITS      6
#define PM_PSHIFT_OFFSET    (PM_STATUS_OFFSET - PM_PSHIFT_BITS)
#define PM_PSHIFT_MASK      (((1LL << PM_PSHIFT_BITS) - 1) << PM_PSHIFT_OFFSET)
#define __PM_PSHIFT(x)      (((uint64_t) (x) << PM_PSHIFT_OFFSET) & PM_PSHIFT_MASK)
#define PM_PFRAME_MASK      ((1LL << PM_PSHIFT_OFFSET) - 1)
#define PM_PFRAME(x)        ((x) & PM_PFRAME_MASK)

#define __PM_SOFT_DIRTY      (1LL)
#define PM_PRESENT          PM_STATUS(4LL)
#define PM_SWAP             PM_STATUS(2LL)
#define PM_SOFT_DIRTY       __PM_PSHIFT(__PM_SOFT_DIRTY)

static const uint maxProcessNameLength = 15; // with the way we use to read it
static const uint pageFlagsSize = sizeof(uint64_t); // aka 64 bits aka 8 bytes
static const uint pageShift = 12;
static const uint pageSize = 4096;

// from linux/Documentation/vm/pagemap.txt
static const uint pageFlagCount = 23;
static const char *pageFlagNames[pageFlagCount] = {
    "LOCKED",
    "ERROR",
    "REFERENCED",
    "UPTODATE",
    "DIRTY",
    "LRU",
    "ACTIVE",
    "SLAB",
    "WRITEBACK",
    "RECLAIM",
    "BUDDY",
    "MMAP",
    "ANON",
    "SWAPCACHE",
    "SWAPBACKED",
    "COMPOUND_HEAD",
    "COMPOUND_TAIL",
    "HUGE",
    "UNEVICTABLE",
    "HWPOISON",
    "NOPAGE",
    "KSM",
    "THP"
};

using namespace std; // fuck the police :P


static bool isFlagSet(uint64_t flags, uint testFlagShift)
{
    return flags & (1 << testFlagShift);
}

string printablePageFlags(uint64_t flags)
{
    string ret;
    for (uint i = 0; i < pageFlagCount; i++) {
        if (isFlagSet(flags, i)) {
            ret += pageFlagNames[i];
            ret += ", ";
        }
    }
    if (ret.length() >= 2) {
        ret.resize(ret.length() - 2);
    }
    return ret;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        cerr << "Usage: memstat <pid>/<process-name>\n";
        return -1;
    }

    uint pid = strtoul(argv[1], nullptr, 10);
    if (!pid) {
        string procName = string(argv[1]).substr(0, maxProcessNameLength);
        for (const ProcessPid &pp : fetchProcessList()) {
            if (pp.name == procName) {
                pid = pp.pid;
                break;
            }
        }
    }
    if (!pid) {
        cerr << "Found no such PID or process " << argv[1] << "!\n";
        return -1;
    }

    uint64_t vsz = 0;
    for (const MappedRegion &mr : mappedRegions) {
        vsz += mr.end - mr.start;
    }

    uint64_t pss = 0;
    uint64_t pagesWithZeroUseCount = 0;
    for (uint64_t pfn : seenPfns) {
        uint64_t useCount = pageInfos.useCount(pfn);
        uint64_t pageFlags = pageInfos.flags(pfn);
        //cout << pfn << ": use count " << useCount << ", flags: " << printablePageFlags(pageInfos.flags(pfn)) << '\n';
        // currently, use count is misreported as 0 for transparent hugepage tail (all after the first)
        // pages - it should be 1
        // ### not sure if fixable for kernel so we might have to do this forever: TODO also copy the flags
        //     from the head page to all tail pages.
        if (useCount == 1 || isFlagSet(pageFlags, KPF_THP)) {
            // divisions are very slow even on modern CPUs
            pss += 4096;
        } else if (useCount == 0) {
            //cout << "WHAT'S GOING ON HERE?\n";
            pagesWithZeroUseCount++;
        } else {
            pss += 4096 / useCount;
        }
    }

    cout << "VSZ or something is " << vsz / 1024 / 1024 << "MiB\n";
    cout << "RSS or something is " << seenPfns.size() * 4096 / 1024 / 1024 << "MiB\n";
    cout << "PSS or something is " << pss / 1024 / 1024 << "MiB\n";
    cout << "number of pages with zero use count is " << pagesWithZeroUseCount << '\n';
}
