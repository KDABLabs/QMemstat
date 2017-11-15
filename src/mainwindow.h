/*
  mainwindow.h

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
