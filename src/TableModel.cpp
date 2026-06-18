#include "TableModel.h"

#include <QBrush>

#include <algorithm>

TableModel::TableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void TableModel::setTable(TableData table)
{
    beginResetModel();
    m_table = std::move(table);
    m_visibleRows.clear();
    for (int i = 0; i < m_table.rows.size(); ++i)
        m_visibleRows.append(i);
    m_hasTable = true;
    m_modified = false;
    endResetModel();
}

TableData &TableModel::table()
{
    return m_table;
}

const TableData &TableModel::table() const
{
    return m_table;
}

bool TableModel::hasTable() const
{
    return m_hasTable;
}

bool TableModel::isModified() const
{
    return m_modified;
}

void TableModel::setModified(bool modified)
{
    m_modified = modified;
}

void TableModel::toggleDeleted(int sourceRow)
{
    if (!m_hasTable || m_table.format != FileFormat::Dbf || sourceRow < 0 || sourceRow >= m_table.rows.size())
        return;
    m_table.rows[sourceRow].deleted = !m_table.rows[sourceRow].deleted;
    m_modified = true;
    const int proxyRow = m_visibleRows.indexOf(sourceRow);
    if (proxyRow >= 0)
        emit dataChanged(index(proxyRow, 0), index(proxyRow, columnCount() - 1));
}

void TableModel::setVisibleRows(QVector<int> rows)
{
    beginResetModel();
    m_visibleRows = std::move(rows);
    endResetModel();
}

void TableModel::clearVisibleRows()
{
    QVector<int> rows;
    for (int i = 0; i < m_table.rows.size(); ++i)
        rows.append(i);
    setVisibleRows(rows);
}

int TableModel::sourceRowForProxyRow(int proxyRow) const
{
    if (proxyRow < 0 || proxyRow >= m_visibleRows.size())
        return -1;
    return m_visibleRows[proxyRow];
}

int TableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_visibleRows.size();
}

int TableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() || !m_hasTable ? 0 : m_table.columns.size();
}

QVariant TableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || !m_hasTable)
        return {};

    const int sourceRow = sourceRowForProxyRow(index.row());
    if (sourceRow < 0 || sourceRow >= m_table.rows.size())
        return {};

    const Row &row = m_table.rows[sourceRow];
    if (index.column() < 0 || index.column() >= row.values.size())
        return {};

    if (role == Qt::DisplayRole || role == Qt::EditRole || role == Qt::ToolTipRole)
        return row.values[index.column()];

    if (role == Qt::ForegroundRole && row.deleted)
        return QBrush(Qt::gray);

    return {};
}

QVariant TableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || !m_hasTable)
        return {};

    if (orientation == Qt::Horizontal) {
        if (section >= 0 && section < m_table.columns.size())
            return m_table.columns[section].name;
        return {};
    }

    const int sourceRow = sourceRowForProxyRow(section);
    if (sourceRow < 0)
        return {};
    return m_table.rows[sourceRow].deleted ? QStringLiteral("*%1").arg(sourceRow + 1) : QString::number(sourceRow + 1);
}

Qt::ItemFlags TableModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags flags = QAbstractTableModel::flags(index);
    if (index.isValid())
        flags |= Qt::ItemIsEditable;
    return flags;
}

bool TableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role != Qt::EditRole || !index.isValid() || !m_hasTable)
        return false;

    const int sourceRow = sourceRowForProxyRow(index.row());
    if (sourceRow < 0 || sourceRow >= m_table.rows.size())
        return false;

    const QString text = value.toString();
    const Column &column = m_table.columns[index.column()];
    if (column.kind == ColumnKind::Number && !text.trimmed().isEmpty()) {
        bool ok = false;
        text.trimmed().toDouble(&ok);
        if (!ok)
            return false;
    }

    m_table.rows[sourceRow].values[index.column()] = text;
    m_modified = true;
    emit dataChanged(index, index);
    return true;
}

void TableModel::sort(int column, Qt::SortOrder order)
{
    if (!m_hasTable)
        return;

    beginResetModel();
    if (column < 0 || column >= m_table.columns.size()) {
        std::sort(m_visibleRows.begin(), m_visibleRows.end());
    } else {
        const bool numeric = m_table.columns[column].kind == ColumnKind::Number;
        std::stable_sort(m_visibleRows.begin(), m_visibleRows.end(),
            [this, column, order, numeric](int a, int b) {
                const QString &va = a < m_table.rows.size() ? m_table.rows[a].values[column] : QString();
                const QString &vb = b < m_table.rows.size() ? m_table.rows[b].values[column] : QString();

                if (va.isEmpty() && vb.isEmpty())
                    return false;
                if (va.isEmpty())
                    return order == Qt::DescendingOrder;
                if (vb.isEmpty())
                    return order == Qt::AscendingOrder;

                bool less;
                if (numeric)
                    less = va.toDouble() < vb.toDouble();
                else
                    less = QString::compare(va, vb, Qt::CaseInsensitive) < 0;

                return order == Qt::AscendingOrder ? less : !less;
            });
    }
    endResetModel();
}

