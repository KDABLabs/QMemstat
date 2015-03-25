#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class MosaicWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    // parameters are forwarded to MosaicWidget... this is probably going to change when
    // MainWindow becomes more like a proper main window.
    MainWindow(uint pid);
    MainWindow(const QByteArray &host, uint port);

private:
    MosaicWidget *m_mosaicWidget;
};

#endif // MAINWINDOW_H
