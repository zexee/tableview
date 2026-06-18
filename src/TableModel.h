#pragma once

#include "DataTypes.h"

#include <QAbstractTableModel>

class TableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit TableModel(QObject *parent = nullptr);

    void setTable(TableData table);
    TableData &table();
    const TableData &table() const;
    bool hasTable() const;
    bool isModified() const;
    void setModified(bool modified);
    void toggleDeleted(int sourceRow);
    void setVisibleRows(QVector<int> rows);
    void clearVisibleRows();
    int sourceRowForProxyRow(int proxyRow) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

private:
    TableData m_table;
    QVector<int> m_visibleRows;
    bool m_hasTable = false;
    bool m_modified = false;
};

