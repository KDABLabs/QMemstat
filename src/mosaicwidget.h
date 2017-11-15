/*
  mosaicwidget.h

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

#ifndef MOSAICWIDGET_H
#define MOSAICWIDGET_H

#include <QByteArray>
#include <QElapsedTimer>
#include <QImage>
#include <QLabel>
#include <QScrollArea>
#include <QTimer>
#include <QTcpSocket>

#include <utility>
#include <vector>
#include "pageinfo.h"

class PageInfoReader
{
public:
    // returns true when a new dataset was just completed
    bool addData(const QByteArray &data);
    std::vector<MappedRegion> m_mappedRegions;

private:
    int64_t m_length = -1;
    QByteArray m_buffer;
};

class MosaicWidget : public QScrollArea
{
    Q_OBJECT
public:
    MosaicWidget(uint pid);
    MosaicWidget(const QByteArray &host, uint port);

signals:
    void showPageInfo(quint64 addr, quint32 useCount, const QString &backingFile);
    // value ~0 / (all bits set) on combinedFlags parameter means invalid page
    void showFlags(quint32 combinedFlags);
    void serverConnectionBroke(bool);

private slots:
    void socketError();

protected:
    bool eventFilter(QObject *, QEvent *) override;

private slots:
    void localUpdateTimeout();
    void networkDataAvailable();

private:
    void updatePageInfo(const std::vector<MappedRegion> &regions);

    void printPageFlagsAtPos(const QPoint &widgetPos);
    quint64 addressAtPos(const QPoint &widgetPos);
    void printPageFlagsAtAddr(quint64 addr);

    uint m_pid;
    QTimer m_updateTimer;
    QElapsedTimer m_updateIntervalWatch;
    QTcpSocket m_socket;
    PageInfoReader m_pageInfoReader;

    std::vector<MappedRegion> m_regions; // for tooltips and other mouseover info
    // v meaning:  line, address (of the start of each largeRegion)
    std::vector<std::pair<quint32, quint64>> m_largeRegions; // needed for picking the right info

    QLabel m_mosaicWidget;
    QImage m_img;
};

#endif // MOSAICWIDGET_H
