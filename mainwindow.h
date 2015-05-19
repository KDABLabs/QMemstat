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
    void serverConnectionBroke(bool);

private:
    void init();

    MosaicWidget *m_mosaicWidget;
    QTextEdit *m_pageInfoText;
    bool m_textOptionsSet;
    bool m_serverConnectionBroken;
};

#endif // MAINWINDOW_H
