/*
  qmemstat.cpp

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

#include "mainwindow.h"

#include <iostream>
#include <linux/kernel-page-flags.h>
#include "linux-pm-bits.h"
#include <QApplication>
#include <QByteArray>

static const uint maxProcessNameLength = 15; // with the way we use to read it

static const uint defaultPort = 5550;

using namespace std;

static void printUsage()
{
    cerr << "Usage: qmemstat <pid>/<process-name>\n"
         << "       qmemstat --client <host> [<port>]\n";
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printUsage();
        return -1;
    }

    int pid = -1;
    QByteArray host;
    uint port = defaultPort;

    if (QByteArray(argv[1]) != QByteArray("--client")) {
        if (argc != 2) {
            printUsage();
            return -1;
        }

        pid = strtoul(argv[1], nullptr, 10);

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
    } else {
        if (argc < 3 || argc > 4) {
            printUsage();
            return -1;
        }
        host = QByteArray(argv[2]);

        if (argc == 4) {
            port = strtoul(argv[3], nullptr, 10);
            if (!port) {
                cerr << "Invalid port number" << argv[3] << '\n';
                printUsage();
                return -1;
            }
        }
    }

    QApplication app(argc, argv);
    MainWindow *mainWindow = nullptr;
    if (pid > 0) {
        cerr << "local mode.\n";
        mainWindow = new MainWindow(pid);
    } else {
        cerr << "client mode.\n";
        mainWindow = new MainWindow(host, port);
    }
    mainWindow->show();
    return app.exec();
}
