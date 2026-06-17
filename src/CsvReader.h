#pragma once

#include "DataTypes.h"

#include <QString>

TableData readCsvFile(const QString &path, TextEncoding encoding, QChar delimiter, bool firstRowAsHeader);
void exportCsvFile(const QString &path, const TableData &table, const QVector<int> &visibleRows, QChar delimiter);

