/*
  main.cpp
  This file is part of QMemstat, a Qt GUI analyzer for program memory.
  Copyright (C) 2016-2017 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Initial Author: Andreas Hartmetz <andreas.hartmetz@kdab.com>
  Maintainer: Christoph Sterz <christoph.sterz@kdab.com>
  Licensees holding valid commercial KDAB QMemstat licenses may use this file in
  accordance with QMemstat Commercial License Agreement provided with the Software.
  Contact info@kdab.com if any conditions of this licensing are not clear to you.
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "processinfo.h"

#include <cstdlib>
#include <fstream>

// POSIX specific, but this whole program only works on Linux anyway!
#include <sys/types.h>
#include <dirent.h>

using namespace std;

vector<ProcessPid> readProcessList()
{
    vector<ProcessPid> ret;

    DIR *dp = opendir ("/proc");
    if (!dp) {
        perror ("Couldn't open the /proc directory");
        exit(-1);
    }

    const string procPrefix = "/proc/";
    const string statSuffix = "/stat";
    string nameFromStat;
    ProcessPid pp;
    // For scripts and in certain other situations, /proc/<pid>/cmdline should be considered which we don't
    // do - see pidof.c from the procps-ng for how to do it 100% correctly. It can probably be said that
    // pidof is correct by definition.
    while (dirent *ep = readdir(dp)) {
        // zero is not a valid pid and also the error return value of strtoul...
        pp.pid = strtoul(ep->d_name, nullptr, 10);
        if (!pp.pid) {
            continue;
        }
        ifstream statFile(procPrefix + ep->d_name + statSuffix);
        if (!statFile.is_open()) {
            continue; // probably a harmless race - the process went away
        }
        statFile >> /* throw away the first value */ nameFromStat >> nameFromStat;
        if (nameFromStat.length() < 2) {
            continue;
        }
        // "(my_process)" -> "my_process"
        pp.name = nameFromStat.substr(1, nameFromStat.length() - 2);
        ret.push_back(pp);
    }

    closedir (dp);

    return ret;
}
