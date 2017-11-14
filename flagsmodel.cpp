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

#include "flagsmodel.h"

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
    // we shift them around a bit to clearly group them together and away from the other group,
    // as documented in readPagemap() in pageinfo.cpp: 55-> 28 ; 61 -> 29 ; 62 -> 30; 63 -> 31
    "SOFT_DIRTY",
    "FILE_PAGE / SHARE_ANON", // 29
    "SWAPPED",
    "PRESENT"
};

FlagsModel::FlagsModel()
   : m_bitFlags(0)
{
    for (uint i = 0; i < pageFlagCount; i++) {
        if (pageFlagNames[i]) {
            m_flagRemapping.append(i);
            m_flagNames.append(QString::fromLatin1(pageFlagNames[i]));
        }
    }
}

int FlagsModel::flagsCount() const
{
    return m_flagRemapping.count();
}

int FlagsModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 1;
}

int FlagsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : flagsCount();
}

QVariant FlagsModel::data(const QModelIndex &index, int role) const
{
    Q_ASSERT(!index.parent().isValid());
    Q_ASSERT(index.row() >= 0 && index.row() < flagsCount());
    Q_ASSERT(index.column() >= 0 && index.column() < 1);

    if (role == Qt::DisplayRole) {
        return m_flagNames.at(index.row());
    } else if (role == Qt::CheckStateRole) {
        const int remappedPosition = m_flagRemapping.at(index.row());
        return (m_bitFlags & (1 << remappedPosition)) ? Qt::Checked : Qt::Unchecked;
    }
    return QVariant();
}

Qt::ItemFlags FlagsModel::flags(const QModelIndex &/* index */) const
{
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QModelIndex FlagsModel::index(int row, int column, const QModelIndex &parent) const
{
    //if (!isIndexValid(row, column, parent)) {
    if (parent.isValid() || column < 0 || column >= 1 || row < 0 || row >= flagsCount()) {
        return QModelIndex();
    }
    return createIndex(row, column);
}

QModelIndex FlagsModel::parent(const QModelIndex &index) const
{
    Q_UNUSED(index);
    return QModelIndex();
}

// slot
void FlagsModel::setFlags(quint32 flags)
{
    if (m_bitFlags == flags) {
        return;
    }
    beginResetModel();
    m_bitFlags = flags;
    endResetModel();
}

