#include "CsvReader.h"

#include "Codec.h"

#include <QFile>
#include <QSet>
#include <algorithm>
#include <stdexcept>

namespace {

QList<QStringList> parseCsv(const QString &text, QChar delimiter)
{
    QList<QStringList> rows;
    QStringList row;
    QString field;
    bool quoted = false;

    for (int i = 0; i < text.size(); ++i) {
        const QChar ch = text[i];
        if (quoted) {
            if (ch == QLatin1Char('"')) {
                if (i + 1 < text.size() && text[i + 1] == QLatin1Char('"')) {
                    field.append(QLatin1Char('"'));
                    ++i;
                } else {
                    quoted = false;
                }
            } else {
                field.append(ch);
            }
            continue;
        }

        if (ch == QLatin1Char('"') && field.isEmpty()) {
            quoted = true;
        } else if (ch == delimiter) {
            row.append(field.trimmed());
            field.clear();
        } else if (ch == QLatin1Char('\n')) {
            row.append(field.trimmed());
            field.clear();
            rows.append(row);
            row.clear();
        } else if (ch != QLatin1Char('\r')) {
            field.append(ch);
        }
    }

    if (!field.isEmpty() || !row.isEmpty()) {
        row.append(field.trimmed());
        rows.append(row);
    }
    return rows;
}

QString excelColumnName(int index)
{
    QString name;
    int n = index;
    while (n >= 0) {
        name.prepend(QChar(QLatin1Char('A').unicode() + (n % 26)));
        n = n / 26 - 1;
    }
    return name;
}

void dedupe(QStringList &headers)
{
    QHash<QString, int> used;
    for (int i = 0; i < headers.size(); ++i) {
        if (headers[i].trimmed().isEmpty())
            headers[i] = QStringLiteral("F%1").arg(i);
        const QString base = headers[i];
        const int count = used.value(base, 0) + 1;
        used[base] = count;
        if (count > 1)
            headers[i] = QStringLiteral("%1_%2").arg(base).arg(count);
    }
}

QList<ColumnKind> detectKinds(const QList<QStringList> &rows, int columns)
{
    QList<ColumnKind> kinds;
    for (int c = 0; c < columns; ++c) {
        bool any = false;
        bool number = true;
        const int sample = std::min<int>(static_cast<int>(rows.size()), 100);
        for (int r = 0; r < sample; ++r) {
            const QString value = c < rows[r].size() ? rows[r][c].trimmed() : QString();
            if (value.isEmpty())
                continue;
            any = true;
            bool ok = false;
            value.toDouble(&ok);
            if (!ok) {
                number = false;
                break;
            }
        }
        kinds.append(any && number ? ColumnKind::Number : ColumnKind::Text);
    }
    return kinds;
}

QString escapeCsv(QString value, QChar delimiter)
{
    if (value.contains(delimiter) || value.contains(QLatin1Char('"')) || value.contains(QLatin1Char('\n')) || value.contains(QLatin1Char('\r')))
        return QStringLiteral("\"%1\"").arg(value.replace(QStringLiteral("\""), QStringLiteral("\"\"")));
    return value;
}

} // namespace

TableData readCsvFile(const QString &path, TextEncoding encoding, QChar delimiter, bool firstRowAsHeader)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        throw std::runtime_error(file.errorString().toStdString());

    QString text = decodeText(file.readAll(), encoding);
    if (text.startsWith(QChar(0xFEFF)))
        text.remove(0, 1);

    const QList<QStringList> parsed = parseCsv(text, delimiter);
    TableData table;
    table.format = FileFormat::Csv;
    table.path = path;
    table.encoding = encoding;
    if (parsed.isEmpty())
        return table;

    QStringList headers;
    int startRow = 0;
    if (firstRowAsHeader) {
        headers = parsed[0];
        startRow = 1;
    } else {
        for (int i = 0; i < parsed[0].size(); ++i)
            headers.append(excelColumnName(i));
    }
    dedupe(headers);

    QList<QStringList> dataRows;
    for (int i = startRow; i < parsed.size(); ++i)
        dataRows.append(parsed[i]);

    const QList<ColumnKind> kinds = detectKinds(dataRows, headers.size());
    for (int i = 0; i < headers.size(); ++i) {
        Column col;
        col.name = headers[i];
        col.kind = kinds[i];
        col.dbfType = kinds[i] == ColumnKind::Number ? QLatin1Char('N') : QLatin1Char('C');
        table.columns.append(col);
    }

    for (QStringList values : dataRows) {
        while (values.size() < table.columns.size())
            values.append(QString());
        values = values.mid(0, table.columns.size());
        table.rows.append(Row{false, values});
    }

    return table;
}

void exportCsvFile(const QString &path, const TableData &table, const QVector<int> &visibleRows, QChar delimiter)
{
    QString out;
    QStringList headers;
    for (const Column &col : table.columns)
        headers.append(escapeCsv(col.name, delimiter));
    out += headers.join(delimiter) + QLatin1Char('\n');

    for (int rowIndex : visibleRows) {
        if (rowIndex < 0 || rowIndex >= table.rows.size())
            continue;
        QStringList values;
        for (const QString &value : table.rows[rowIndex].values)
            values.append(escapeCsv(value, delimiter));
        out += values.join(delimiter) + QLatin1Char('\n');
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        throw std::runtime_error(file.errorString().toStdString());
    file.write(encodeText(out, table.encoding));
}
