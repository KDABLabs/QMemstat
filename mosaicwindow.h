#ifndef MOSAICWINDOW_H
#define MOSAICWINDOW_H

#include <QElapsedTimer>
#include <QImage>
#include <QLabel>
#include <QTimer>

class MosaicWindow : public QLabel
{
    Q_OBJECT
public:
    MosaicWindow(uint pid);

private slots:
    void updatePageInfo();

private:
    uint m_pid;
    QTimer m_updateTimer;
    QElapsedTimer m_updateIntervalWatch;
    QImage m_img;
};

#endif // MOSAICWINDOW_H
