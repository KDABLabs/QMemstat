# QMemstat

**QMemstat** is a tool to **inspect the address space of a process.**

The Linux kernel exposes quite detailed information about the memory pages
that processes use, and information about the pages themselves, which can
be put together to get detailed information about a given process.
qmemstat uses this information from the kernel and shows it in a more
convenient format.
Since the kernel interfaces used are root-only for security reasons,
qmemstat needs to run as root, or at least a "server" needs to run as root
for qmemstat.

## Tools

QMemstat contains two related tools:

### memstat

Command-line tool. It must be run as root.

memstat has two modes:

- display memory use information: `memstat <pid>|<process-name>`
  outputs the following three numbers:
    - VSZ (virtual set size): the size of the address space of the process
    - RSS (resident set size): the size of physical memory in the address
      space of the process
    - PSS (proportional set size): like RSS, but for shared memory pages
      the size is divided by the number of users. This is the most accurate
      "actual memory used" value.
- server mode: `memstat <pid>|<process> --server <port-number>`
  continuously grabs address space information and provides
  it to qmemstat (see below).

### qmemstat

GUI tool which shows information about a process's address space, and
which updates the information continuously.

It has two modes:

- standalone: `qmemstat <pid>|<process-name>` (must be run as root)
  shows a graphical view of the address space of the process. 
    - Hold down
      the left mouse button to see the flags of the page under the cursor
      in the panel on the left.
- as a client to memstat running in server mode (does not need root):
  `qmemstat --client <server-address> <port-number>`
  Otherwise it works like standalone mode.
