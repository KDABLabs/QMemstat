#include "mainwindow.h"

#include "mosaicwidget.h"

MainWindow::MainWindow(uint pid)
   : m_mosaicWidget(new MosaicWidget(pid))
{
    setCentralWidget(m_mosaicWidget);
}

MainWindow::MainWindow(const QByteArray &host, uint port)
   : m_mosaicWidget(new MosaicWidget(host, port))
{
    setCentralWidget(m_mosaicWidget);
}
