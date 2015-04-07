#include "mainwindow.h"

#include "flagsmodel.h"
#include "mosaicwidget.h"

#include <QBoxLayout>
#include <QLabel>
#include <QListView>
#include <QTextEdit>

MainWindow::MainWindow(uint pid)
   : m_mosaicWidget(new MosaicWidget(pid))
{
    init();
}

MainWindow::MainWindow(const QByteArray &host, uint port)
   : m_mosaicWidget(new MosaicWidget(host, port))
{
    init();
}

void MainWindow::init()
{
    QWidget *mainContainer = new QWidget();

    QHBoxLayout *mainLayout = new QHBoxLayout();
    QVBoxLayout *infoLayout = new QVBoxLayout();
    mainLayout->addItem(infoLayout);

    m_pageInfoText = new QTextEdit();
    m_pageInfoText->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_pageInfoText->setReadOnly(true);
    m_pageInfoText->setText("foo:\tfalse\nbar:\ttrue");
    infoLayout->addWidget(m_pageInfoText);

    infoLayout->addSpacing(10);
    QLabel *flagsLabel = new QLabel(QString::fromLatin1("Page flags"));
    flagsLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    infoLayout->addWidget(flagsLabel);

    QListView *flagsView = new QListView();
    flagsView->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    FlagsModel *flagsModel = new FlagsModel();
    flagsView->setModel(flagsModel);
    infoLayout->addWidget(flagsView);

    mainLayout->addWidget(m_mosaicWidget);

    mainContainer->setLayout(mainLayout);

    connect(m_mosaicWidget, SIGNAL(showFlags(quint32)), flagsModel, SLOT(setFlags(quint32)));
    connect(m_mosaicWidget, SIGNAL(showPageInfo(quint64, quint32, QString)),
            this, SLOT(showPageInfo(quint64, quint32, QString)));

    setCentralWidget(mainContainer);
}


void MainWindow::showPageInfo(quint64 addr, quint32 useCount, const QString &backingFile)
{
    QString infoText = QString::fromLatin1("Address:\t0x%1\nUse count:\t%2\nBacking file:\t\"%3\"")
        .arg(addr, 0, 16).arg(useCount).arg(backingFile);
    m_pageInfoText->setText(infoText);
}
