/*
  mainwindow.cpp

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
    m_textOptionsSet = false;
    m_serverConnectionBroken = false;

    QWidget *mainContainer = new QWidget();

    QHBoxLayout *mainLayout = new QHBoxLayout();
    QVBoxLayout *infoLayout = new QVBoxLayout();
    mainLayout->addItem(infoLayout);

    m_pageInfoText = new QTextEdit();
    m_pageInfoText->setFixedHeight(300);
    m_pageInfoText->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_pageInfoText->setReadOnly(true);
    m_pageInfoText->setText("Page information (click on page)\n\n"
                            "For information about page flags, read "
                            "linux/Documentation/vm/pagemap.txt.");
    {
        QFont font = m_pageInfoText->document()->defaultFont();
        font.setPointSize(font.pointSize() + 1);
        m_pageInfoText->document()->setDefaultFont(font);
    }
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
    connect(m_mosaicWidget, SIGNAL(serverConnectionBroke(bool)), this, SLOT(serverConnectionBroke(bool)));

    setCentralWidget(mainContainer);
}

void MainWindow::showPageInfo(quint64 addr, quint32 useCount, const QString &backingFile)
{
    if (!m_textOptionsSet) {
        m_textOptionsSet = true;
        QTextOption to = m_pageInfoText->document()->defaultTextOption();
        to.setTabArray(QList<qreal>() << 66);
        to.setWrapMode(QTextOption::WrapAnywhere);
        m_pageInfoText->document()->setDefaultTextOption(to);
    }
    if (addr) {
        QString backingFileText = backingFile.isEmpty() ? QString::fromLatin1("[none]") : backingFile;
        QString infoText = QString::fromLatin1("Address:\t0x%1\nUse count:\t%2\nBacking file:\n%3")
            .arg(addr, 0, 16).arg(useCount).arg(backingFileText);
        if (m_serverConnectionBroken) {
            infoText.prepend(QString::fromLatin1("Disconnected from server.\n"));
        }
        m_pageInfoText->setText(infoText);
    } else {
        QTextOption to = m_pageInfoText->document()->defaultTextOption();
        to.setWrapMode(QTextOption::WordWrap);
        m_pageInfoText->document()->setDefaultTextOption(to);
        m_pageInfoText->setText(QString::fromLatin1("Could not read page information.<br>You should either "
                                                    "run qmemstat as root or make use of <i>memstat</i> as "
                                                    "root in server mode."));
    }
}

void MainWindow::serverConnectionBroke(bool wasConnected)
{
    m_serverConnectionBroken = true;
    m_textOptionsSet = false;
    QTextOption to = m_pageInfoText->document()->defaultTextOption();
    to.setWrapMode(QTextOption::WordWrap);
    m_pageInfoText->document()->setDefaultTextOption(to);
    m_pageInfoText->setText(QString::fromLatin1(
        wasConnected ? "Disconnected from server."
                     : "Could not connect to server."
    ));
}
