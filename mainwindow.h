#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class MosaicWidget;
class QTextEdit;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    // parameters are forwarded to MosaicWidget... this is probably going to change when
    // MainWindow becomes more like a proper main window.
    MainWindow(uint pid);
    MainWindow(const QByteArray &host, uint port);

private slots:
    void showPageInfo(quint64 addr, quint32 useCount, const QString &backingFile);

private:
    void init();

    MosaicWidget *m_mosaicWidget;
    QTextEdit *m_pageInfoText;
};

#endif // MAINWINDOW_H
