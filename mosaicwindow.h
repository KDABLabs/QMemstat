#ifndef MOSAICWINDOW_H
#define MOSAICWINDOW_H

#include <QByteArray>
#include <QElapsedTimer>
#include <QImage>
#include <QLabel>
#include <QTimer>
#include <QTcpSocket>

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

class MosaicWindow : public QLabel
{
    Q_OBJECT
public:
    MosaicWindow(uint pid);
    MosaicWindow(const QByteArray &host, uint port);

private slots:
    void localUpdateTimeout();
    void networkDataAvailable();

private:
    void updatePageInfo(const std::vector<MappedRegion> &regions);

    uint m_pid;
    QTimer m_updateTimer;
    QElapsedTimer m_updateIntervalWatch;
    QTcpSocket m_socket;
    PageInfoReader m_pageInfoReader;
    QImage m_img;
};

#endif // MOSAICWINDOW_H
