#include "processinfo.h"
#include "pageinfo.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

// ### those two "should" be included from /usr/include/linux, but since the kernel gives an ABI
//     guarantee for user space, it's fairly safe to keep copies and stop requiring that Linux
//     kernel headers are installed.
#include "kernel-page-flags.h"
#include "linux-pm-bits.h"

using namespace std;

static const uint maxProcessNameLength = 15; // with the way we use to read it

static const uint defaultPort = 5550;

#include "pageinfoserializer.cpp"

static bool isFlagSet(uint64_t flags, uint testFlagShift)
{
    return flags & (1 << testFlagShift);
}

void printSummary(const PageInfo &pageInfo)
{
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
}

static void printUsage()
{
    cerr << "Usage: memstat <pid>/<process-name>\n"
         << "       memstat <pid>/<process-name> [--server [<portnumber>]]\n";
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printUsage();
        return -1;
    }

    bool network = false;
    uint port = defaultPort;

    if (argc > 2) {
        network = true;
        if (string(argv[2]) != ("--server") || argc > 4) {
            printUsage();
            return -1;
        }

        if (argc == 4) {
            port = strtoul(argv[3], nullptr, 10);
            if (!port) {
                cerr << "Invalid port number " << argv[3] << '\n';
                printUsage();
                return -1;
            }
        }
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


    if (!network) {
        cerr << "local mode.\n";
        PageInfo pageInfo(pid);
        if (pageInfo.mappedRegions().empty()) {
            cerr << "Could not read page information. Are you root?\n";
            return 1;
        }
        printSummary(pageInfo);
        return 0;
    }

    cerr << "server mode.\n";
    // listen on TCP/IP port, accept one connection, and periodically send data

    const int listenFd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenFd < 0) {
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    bool ok = true;
    ok = ok && (bind(listenFd, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    ok = ok && (listen(listenFd, /* max queued incoming connections */ 1) == 0);
    if (!ok) {
        close(listenFd);
        return -1;
    }

    const int connFd = accept(listenFd, nullptr, nullptr);
    if (connFd < 0) {
        return -1;
    }
    close(listenFd);

    while (true) {
        // destroy PageInfo and PageInfoSerializer when done sending to free their memory...
        {
            PageInfo pageInfo(pid);
            // serialize PageInfo output (vector<MappedRegion>) while sending, to avoid using even
            // more memory on the target system.
            PageInfoSerializer serializer(pageInfo);
            while (true) {
                pair<const char*, size_t> ser = serializer.serializeMore();
                if (ser.second == 0) {
                    break;
                }
                if (write(connFd, ser.first, ser.second) < ser.second) {
                    break;
                }
            }
        }
        //sleep(5);
    }

    return 0;
}
