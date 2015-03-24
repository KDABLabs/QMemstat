#include "processinfo.h"
#include "pageinfo.h"

#include <cassert>
#include <cstdio>
#include <iostream>

#include <linux/kernel-page-flags.h>

#include "linux-pm-bits.h"

#include <QApplication>
#include "mosaicwindow.h"

static const uint maxProcessNameLength = 15; // with the way we use to read it

// from linux/Documentation/vm/pagemap.txt
static const uint pageFlagCount = 32;
static const char *pageFlagNames[pageFlagCount] = {
    // KPF_* flags from kernel-page-flags.h, documented in linux/Documentation/vm/pagemap.txt -
    // those flags are specifically meant to be stable user-space API
    "LOCKED",
    "ERROR",
    "REFERENCED",
    "UPTODATE",
    "DIRTY",
    "LRU",
    "ACTIVE",
    "SLAB",
    "WRITEBACK",
    "RECLAIM",  // 9 (10 for 1-based indexing)
    "BUDDY",
    "MMAP",
    "ANON",
    "SWAPCACHE",
    "SWAPBACKED",
    "COMPOUND_HEAD",
    "COMPOUND_TAIL",
    "HUGE",
    "UNEVICTABLE",
    "HWPOISON", // 19
    "NOPAGE",
    "KSM",
    "THP",
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    // flags from /proc/<pid>/pagemap, also documented in linux/Documentation/vm/pagemap.txt -
    // we shift them around a bit) to clearly group them together and away from the other group
    "SOFT_DIRTY",
    "FILE_PAGE / SHARE_ANON", // 29
    "SWAPPED",
    "PRESENT"
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
            assert(pageFlagNames[i]);
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
        const string procName = string(argv[1]).substr(0, maxProcessNameLength);
        for (const ProcessPid &pp : readProcessList()) {
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

    PageInfo pageInfo(pid);
    const vector<MappedRegion> &mappedRegions = pageInfo.mappedRegions();

    uint64_t pagesWithZeroUseCount = 0;
    uint64_t vsz = 0;
    uint64_t priv = 0;
    uint64_t sharedFull = 0;
    uint64_t sharedProp = 0;

    for (const MappedRegion &mr : mappedRegions) {
        vsz += mr.end - mr.start;
        uint64_t addr = mr.start;
        for (size_t i = 0; i < mr.useCounts.size(); i++, addr += PageInfo::pageSize) {
            uint64_t useCount = mr.useCounts[i];
            uint64_t pageFlags = mr.combinedFlags[i];
            //cout << pfn << ": use count " << useCount << ", flags: " << printablePageFlags(pageInfos.flags(pfn)) << '\n';
            // currently, use count is misreported as 0 for transparent hugepage tail (all after the first)
            // pages - it should be 1
            // ### should we also copy flags from the head page to tail pages?
            if (useCount == 1 || isFlagSet(pageFlags, KPF_THP)) {
                // divisions are very slow even on modern CPUs
                priv += PageInfo::pageSize;
            } else if (useCount == 0) {
                pagesWithZeroUseCount++;
            } else {
                sharedFull += PageInfo::pageSize;
                sharedProp += PageInfo::pageSize / useCount;
            }
        }
        assert(addr == mr.end);
    }

    cout << "VSZ is " << vsz / 1024 / 1024 << "MiB\n";
    cout << "RSS is " << (priv + sharedFull) / 1024 / 1024 << "MiB\n";
    cout << "PSS is " << (priv + sharedProp) / 1024 / 1024 << "MiB\n";
    cout << "number of pages with zero use count is " << pagesWithZeroUseCount << '\n';

    QApplication app(argc, argv);
    MosaicWindow *mosaic = new MosaicWindow(pid);
    mosaic->show();
    return app.exec();
}
