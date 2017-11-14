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

#ifndef PAGEINFO_H
#define PAGEINFO_H

#include <cstdint>
#include <string>
#include <vector>

// TODO
// - tell the backing file for each MappedRegion in case there is one (mmap!)

struct MappedRegion
{
    uint64_t start;
    uint64_t end;
    std::string backingFile;
    std::vector<uint32_t> useCounts;
    std::vector<uint32_t> combinedFlags;
    bool operator<(const MappedRegion &other) const { return start < other.start; }
};

class PageInfo
{
public:
    static const unsigned int pageShift = 12;
    static const unsigned int pageSize = 1 << pageShift; // the well-known 4096 bytes

    PageInfo(unsigned int pid);
    const std::vector<MappedRegion> &mappedRegions() const { return m_mappedRegions; }
private:
    std::vector<MappedRegion> m_mappedRegions;
};

#endif // PAGEINFO_H
