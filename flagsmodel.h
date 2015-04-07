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
