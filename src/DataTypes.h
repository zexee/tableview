#pragma once

#include <QChar>
#include <QList>
#include <QString>
#include <QStringList>

enum class TextEncoding {
    Gbk,
    Utf8,
};

enum class FileFormat {
    Dbf,
    Csv,
    Xlsx,
};

enum class ColumnKind {
    Text,
    Number,
};

struct Column {
    QString name;
    ColumnKind kind = ColumnKind::Text;
    QChar dbfType = QLatin1Char('C');
    int dbfLength = 80;
    int dbfDecimals = 0;
};

struct Row {
    bool deleted = false;
    QStringList values;
};

struct TableData {
    QList<Column> columns;
    QList<Row> rows;
    FileFormat format = FileFormat::Dbf;
    QString path;
    TextEncoding encoding = TextEncoding::Gbk;

    bool supportsSave() const { return format == FileFormat::Dbf; }
};

inline QString encodingLabel(TextEncoding encoding)
{
    return encoding == TextEncoding::Utf8 ? QStringLiteral("UTF-8") : QStringLiteral("GBK");
}

inline QString formatLabel(FileFormat format)
{
    switch (format) {
    case FileFormat::Dbf:
        return QStringLiteral("DBF");
    case FileFormat::Csv:
        return QStringLiteral("CSV");
    case FileFormat::Xlsx:
        return QStringLiteral("Excel");
    }
    return QStringLiteral("Unknown");
}
