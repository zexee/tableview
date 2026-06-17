#pragma once

#include "DataTypes.h"

#include <QString>

TextEncoding detectDbfEncoding(const QString &path);
TableData readDbfFile(const QString &path, TextEncoding encoding);
void saveDbfFile(const QString &path, const TableData &table);

