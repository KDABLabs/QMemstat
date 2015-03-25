#include "mosaicwindow.h"

#include <cassert>
#include <QDebug>
#include <utility>

#include <linux/kernel-page-flags.h>

using namespace std;


///////////////////////////////////////////////////////////////////////////////////////////////////

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

static bool isFlagSet(uint32_t flags, uint testFlagShift)
{
    return flags & (1 << testFlagShift);
}

QString printablePageFlags(uint32_t flags)
{
    QString ret;
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

///////////////////////////////////////////////////////////////////////////////////////////////////

static const uint pixelsPerTile = 2;

bool PageInfoReader::addData(const QByteArray &data)
{
    m_buffer += data;
    bool ret = false;
    // is not guaranteed that there is one or less dataset per chunk of data received, so keep looping
    while (true) {
        if (m_length < 0 && m_buffer.length() >= sizeof(size_t)) {
            m_length = *reinterpret_cast<const uint64_t *>(m_buffer.constData());
        }
        if (m_length >= 0 && m_buffer.length() >= m_length + sizeof(size_t)) {
            ret = true;
            m_mappedRegions.clear();

            const size_t endPos = m_length + sizeof(m_length);
            const char *buf = m_buffer.constData();

            for (int pos = sizeof(size_t); pos < endPos; ) {
                MappedRegion mr;
                mr.start = *reinterpret_cast<const uint64_t *>(buf + pos);
                pos += sizeof(uint64_t);
                mr.end = *reinterpret_cast<const uint64_t *>(buf + pos);
                pos += sizeof(uint64_t);

                size_t arrayLength = (mr.end - mr.start) / PageInfo::pageSize;

                const uint32_t *array = reinterpret_cast<const uint32_t *>(buf + pos);
                mr.useCounts.assign(array, array + arrayLength);
                array += arrayLength;
                mr.combinedFlags.assign(array, array + arrayLength);
                pos += 2 * arrayLength * sizeof(uint32_t);

                m_mappedRegions.push_back(move(mr));
            }

            m_buffer.remove(0, endPos);
            m_length = -1;
        } else {
            break;
        }
    }
    return ret;
}

// bypass QImage API to save cycles; it does make a difference.
class Rgb32PixelAccess
{
public:
    Rgb32PixelAccess(int width, int height, uchar *buffer)
       : m_width(width),
         m_height(height),
         m_buffer(reinterpret_cast<quint32 *>(buffer))
    {}
    inline quint32 pixel(int x, int y) const { return m_buffer[y * m_width + x]; }
    inline void setPixel(int x, int y, quint32 value) { m_buffer[y * m_width + x] = value | (0xff << 24); }

private:
    const int m_width;
    const int m_height;
    quint32 *const m_buffer;
};

class ColorCache
{
public:
    ColorCache()
       : m_cachedColor(0)
    {
        for (int i = 0; i < pixelsPerTile * pixelsPerTile; i++) {
            m_colorCache[i] = 0;
        }
    }

    void paintTile(Rgb32PixelAccess *img, uint x, uint y, uint tileSize, const QColor &color);

private:
    // QColor::darker() is fairly slow, so memoize the result
    void maybeUpdateColors(const QColor &color);

    uint m_cachedColor;
    uint m_colorCache[pixelsPerTile * pixelsPerTile];
};

void ColorCache::maybeUpdateColors(const QColor &color)
{
    if (color.rgb() == m_cachedColor) {
        return;
    }
    m_cachedColor = color.rgb();
    QColor c = color;
    for (int i = 0; i < pixelsPerTile * pixelsPerTile; i++) {
        m_colorCache[i] = c.rgb();
        c = c.darker(115);
    }
}

void ColorCache::paintTile(Rgb32PixelAccess *img, uint x, uint y, uint tileSize, const QColor &color)
{
    maybeUpdateColors(color);
    const uint yStart = y * tileSize;
    const uint xEnd = (x + 1) * tileSize;
    const uint yEnd = (y + 1) * tileSize;
    uint colorIndex = 0;
    for (x = x * tileSize ; x < xEnd; x++) {
        for (y = yStart ; y < yEnd; y++) {
            img->setPixel(x, y, m_colorCache[colorIndex++]);
        }
    }
}

MosaicWindow::MosaicWindow(uint pid)
   : m_pid(pid)
{
    qDebug() << "local process";
    m_updateIntervalWatch.start();
    // we're not usually *reaching* 50 milliseconds update interval... but trying doesn't hurt.
    m_updateTimer.setInterval(50);
    connect(&m_updateTimer, SIGNAL(timeout()), SLOT(localUpdateTimeout()));
    m_updateTimer.start();
    localUpdateTimeout();
}

MosaicWindow::MosaicWindow(const QByteArray &host, uint port)
   : m_pid(0)
{
    qDebug() << "process on server:" << host << port;
    m_socket.connectToHost(QString::fromLatin1(host), port, QIODevice::ReadOnly);
    connect(&m_socket, SIGNAL(readyRead()), SLOT(networkDataAvailable()));
}

void MosaicWindow::localUpdateTimeout()
{
    PageInfo pageInfo(m_pid);
    updatePageInfo(pageInfo.mappedRegions());
}

void MosaicWindow::networkDataAvailable()
{
    if (m_pageInfoReader.addData(m_socket.readAll())) {
        updatePageInfo(m_pageInfoReader.m_mappedRegions);
    }
}

void MosaicWindow::updatePageInfo(const vector<MappedRegion> &regions)
{
    qint64 elapsed = m_updateIntervalWatch.restart();
    qDebug() << " >> frame interval" << elapsed << "millseconds";

    if (regions.empty()) {
        m_img = QImage();
        setPixmap(QPixmap::fromImage(m_img));
        return;
    }

    for (const MappedRegion &mappedRegion : regions) {
        assert(mappedRegion.end >= mappedRegion.start); // == unfortunately happens sometimes
    }
    for (int i = 1; i < regions.size(); i++) {
        if (regions[i].start < regions[i - 1].end) {
            qDebug() << "ranges.." << QString("%1").arg(regions[i - 1].start, 0, 16)
                                   << QString("%1").arg(regions[i - 1].end, 0, 16)
                                   << QString("%1").arg(regions[i].start, 0, 16)
                                   << QString("%1").arg(regions[i].end, 0, 16);

        }
        assert(regions[i].start >= regions[i - 1].end);
    }


    const quint64 totalRange = regions.back().end - regions.front().start;

    //qDebug() << "Address range covered (in pages) is" << totalRange / PageInfo::pageSize;
    quint64 mappedSpace = 0;
    for (const MappedRegion &r : regions) {
        mappedSpace += r.end - r.start;
    }
    //qDebug() << "Number of pages in mapped address space (VSZ) is" << mappedSpace / PageInfo::pageSize;

    // The difference between page count in mapped address space and page count in the "spanned" address
    // space can be HUGE, so we must insert some (...) in the graphical representation. Find the contiguous
    // regions and thus the points to graphically separate them.
    // TODO implement a separator later, be it a line, spacing, labeling....
    vector<pair<quint64, quint64>> largeRegions;
    {
        pair<quint64, quint64> largeRegion = make_pair(regions.front().start, regions.front().end);
        static const quint64 maxAllowedGap = 64 * PageInfo::pageSize;
        for (const MappedRegion &r : regions) {
            if (r.start > largeRegion.second + maxAllowedGap) {
                largeRegions.push_back(largeRegion);
                largeRegion.first = r.start;
            }
            largeRegion.second = r.end;
        }
        largeRegions.push_back(largeRegion);
    }

    //qDebug() << "number of large regions in the address space is" << largeRegions.size();
    for (auto &r : largeRegions) {
        // hex output...
        // qDebug() << "region" << QString("%1").arg(r.first, 0, 16) << QString("%1").arg(r.second, 0, 16);
    }

    static const uint columnCount = 512;
    static const uint tilesPerSeparator = 2;

    // determine size
    // separators between largeRegions
    uint rowCount = (largeRegions.size() - 1) * tilesPerSeparator;
    // space for tiles showing showing pages (corresponding to contents of largeRegions)
    for (pair<quint64, quint64> largeRegion : largeRegions) {
        rowCount += ((largeRegion.second - largeRegion.first) / PageInfo::pageSize + (columnCount - 1)) /
                    columnCount;
    }
    qDebug() << "row count is" << rowCount << " largeRegion count is" << largeRegions.size();

    // paint!

    m_img = QImage(columnCount * pixelsPerTile, rowCount * pixelsPerTile, QImage::Format_RGB32);
    // Theoretically we need to get the stride of the image, but in practice it is equal to width,
    // especially with the power-of-2 widths we are using.
    Rgb32PixelAccess pixels(m_img.width(), m_img.height(), m_img.bits());
    // don't always construct QColors from enums - this would eat ~ 10% or so of frame time.
    const QColor colorWhite(Qt::white);
    const QColor colorGray(Qt::darkGray);
    const QColor colorMagenta(Qt::magenta);
    const QColor colorMagentaLight = QColor(Qt::magenta).lighter(150);
    const QColor colorYellow(Qt::yellow);
    const QColor colorCyan(Qt::blue);
    const QColor colorGreen(Qt::green);
    const QColor colorGreenDark(Qt::darkGreen);
    const QColor colorRedDark(Qt::darkRed);
    const QColor colorBlack(Qt::black);
    // cache results of QColor::darken()
    ColorCache cc;

    uint row = 0;
    size_t iMappedRegion = 0;
    for (pair<quint64, quint64> largeRegion : largeRegions) {
        uint column = 0;
        assert(iMappedRegion < regions.size());
        const MappedRegion *region = &regions[iMappedRegion];
        for ( ;region->end <= largeRegion.second; region = &regions[iMappedRegion]) {
            assert(iMappedRegion < regions.size());

            quint64 pagesInRegion = (region->end - region->start) / PageInfo::pageSize;
            assert(iMappedRegion >= 0);
            assert(region->end >= region->start);
            size_t iPage = 0;
            // when to stop painting the current column:
            // - iPage >= pagesInRegion
            // - column >= columnCount
            while (iPage < region->useCounts.size()) {
                const size_t endColumn = qMin(column + region->useCounts.size() - iPage, size_t(columnCount));
                //qDebug() << "painting region" << column << endColumn;
                for ( ; column < endColumn; column++, iPage++) {
                    //qDebug() << "magenta" << column << row;
                    QColor color = colorWhite;
                    if (!(region->combinedFlags[iPage] & (1 << 31))) { // TODO no magic numbers - checking if "present" flag clear here
                        color = colorGray;
                    } else if ((region->combinedFlags[iPage] & (1 << KPF_MMAP)) &&
                               !(region->combinedFlags[iPage] & (1 << KPF_ANON))) {
                        color = region->useCounts[iPage] > 1 ? colorGreen : colorGreenDark;
                    } else if (region->combinedFlags[iPage] & (1 << KPF_THP)) {
                        // THP implies use count 1; the kernel wrongly reports use count 0 in this case
                        color = colorMagentaLight;
                    } else if (region->useCounts[iPage] == 1) {
                        color = colorMagenta;
                    } else if (region->useCounts[iPage] > 1) {
                        color = colorYellow;
                    } else if (region->combinedFlags[iPage] & (1 << KPF_NOPAGE)) {
                        color = colorRedDark;
                    } else {
                        qDebug() << "white page has use count" << region->useCounts[iPage] << "and flags"
                                 << printablePageFlags(region->combinedFlags[iPage]);
                    }
                    cc.paintTile(&pixels, column, row, pixelsPerTile, color);
                }
                if (column == columnCount) {
                    column = 0;
                    row++;
                }
            }
            assert(region->start + iPage * PageInfo::pageSize == region->end);

            // fill tiles up to either next MappedRegion or (if at end of current largeRegion) end of row

            // why increase it here? a) convenience, otherwise there'd be a lot of "iMappedRegion + 1" below
            // b) correctness, we need to fill up the current row if we're at end of region. this could be
            //    done differently, but i don't think it's worth it if you think about how to do it.
            iMappedRegion++;

            assert(column <= columnCount);
            size_t gapPages = column ? (columnCount - column) : 0;
            if (iMappedRegion < regions.size() && regions[iMappedRegion].start < largeRegion.second) {
                //qDebug() << "doing it..." << QString("%1").arg(region->end, 0, 16)
                //                          << QString("%1").arg(regions[iMappedRegion].start, 0, 16);
                assert(regions[iMappedRegion].start >= region->end);
                // region still points to previous MappedRegion...
                gapPages = (regions[iMappedRegion].start - region->end) / PageInfo::pageSize;
            }
            assert(gapPages < columnCount);
            while (gapPages) {
                const size_t endColumn = qMin(size_t(columnCount), column + gapPages);
                gapPages -= (endColumn - column);
                //qDebug() << "painting gap" << column << endColumn << gapPages;
                for ( ; column < endColumn; column++) {
                    cc.paintTile(&pixels, column, row, pixelsPerTile, colorCyan);
                }
                if (column == columnCount) {
                    column = 0;
                    row++;
                }
            }

            assert(region->end <= largeRegion.second);
            // loop control snippet "region = &regions[iMappedRegion]" would access out of bounds otherwise
            if (iMappedRegion >= regions.size()) {
                break;
            }
        }
        assert(column == 0);
        // draw separator line; we avoid a line after the last largeRegion via the "&& y < rowCount"
        // condition and decreasing rowCount by the width (here really: height) of a line.
        for (int y = row; y < row + tilesPerSeparator && y < rowCount; y++) {
            for (int x = 0 ; x < columnCount; x++) {
                cc.paintTile(&pixels, x, y, pixelsPerTile, colorBlack);
            }
        }
        row += tilesPerSeparator;
    }

    setPixmap(QPixmap::fromImage(m_img));
}
