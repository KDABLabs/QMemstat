/*
  main.cpp
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

#ifndef FLAGSMODEL_H
#define FLAGSMODEL_H

#include <QAbstractItemModel>
#include <QStringList>
#include <QVector>

class FlagsModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    FlagsModel();

    int columnCount(const QModelIndex &parent) const;
    int rowCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;

public slots:
    void setFlags(quint32);

protected:
    QModelIndex index(int row, int column, const QModelIndex &parent) const;
    QModelIndex parent(const QModelIndex &index) const;

private:
    int flagsCount() const;

    quint32 m_bitFlags;
    QStringList m_flagNames;
    QVector<quint32> m_flagRemapping;
};

#endif // FLAGSMODEL_H
