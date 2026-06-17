#include "XlsxReader.h"

#include "ZipReader.h"

#include <QXmlStreamReader>
#include <algorithm>
#include <stdexcept>

namespace {

QString readElementText(QXmlStreamReader &xml)
{
    return xml.readElementText(QXmlStreamReader::IncludeChildElements);
}

QStringList readSharedStrings(ZipReader &zip)
{
    QStringList strings;
    const QByteArray data = zip.fileData(QStringLiteral("xl/sharedStrings.xml"));
    if (data.isEmpty())
        return strings;

    QXmlStreamReader xml(data);
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == QLatin1String("si")) {
            QString value;
            while (!(xml.isEndElement() && xml.name() == QLatin1String("si")) && !xml.atEnd()) {
                xml.readNext();
                if (xml.isStartElement() && xml.name() == QLatin1String("t"))
                    value += readElementText(xml);
            }
            strings.append(value);
        }
    }
    return strings;
}

QString firstSheetPath(ZipReader &zip)
{
    const QByteArray workbook = zip.fileData(QStringLiteral("xl/workbook.xml"));
    if (workbook.isEmpty())
        return QStringLiteral("xl/worksheets/sheet1.xml");

    QString relationId;
    QXmlStreamReader workbookXml(workbook);
    while (!workbookXml.atEnd()) {
        workbookXml.readNext();
        if (workbookXml.isStartElement() && workbookXml.name() == QLatin1String("sheet")) {
            relationId = workbookXml.attributes().value(QStringLiteral("r:id")).toString();
            break;
        }
    }
    if (relationId.isEmpty())
        return QStringLiteral("xl/worksheets/sheet1.xml");

    const QByteArray rels = zip.fileData(QStringLiteral("xl/_rels/workbook.xml.rels"));
    QXmlStreamReader relsXml(rels);
    while (!relsXml.atEnd()) {
        relsXml.readNext();
        if (!relsXml.isStartElement() || relsXml.name() != QLatin1String("Relationship"))
            continue;
        if (relsXml.attributes().value(QStringLiteral("Id")).toString() != relationId)
            continue;
        QString target = relsXml.attributes().value(QStringLiteral("Target")).toString();
        if (target.startsWith(QLatin1String("/xl/")))
            return target.mid(1);
        if (target.startsWith(QLatin1String("xl/")))
            return target;
        return QStringLiteral("xl/%1").arg(target);
    }

    return QStringLiteral("xl/worksheets/sheet1.xml");
}

int columnIndexFromRef(const QString &ref)
{
    int value = 0;
    bool any = false;
    for (const QChar ch : ref) {
        if (!ch.isLetter())
            break;
        any = true;
        value = value * 26 + (ch.toUpper().unicode() - QLatin1Char('A').unicode() + 1);
    }
    return any ? value - 1 : -1;
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

QString cellValue(QXmlStreamReader &xml, const QStringList &sharedStrings, const QString &type)
{
    QString value;
    while (!(xml.isEndElement() && xml.name() == QLatin1String("c")) && !xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == QLatin1String("v")) {
            value = readElementText(xml);
        } else if (xml.isStartElement() && xml.name() == QLatin1String("t")) {
            value = readElementText(xml);
        }
    }

    if (type == QLatin1String("s")) {
        bool ok = false;
        const int index = value.toInt(&ok);
        return ok && index >= 0 && index < sharedStrings.size() ? sharedStrings[index] : QString();
    }
    if (type == QLatin1String("b"))
        return value == QLatin1String("1") ? QStringLiteral("TRUE") : QStringLiteral("FALSE");
    return value;
}

} // namespace

TableData readXlsxFile(const QString &path)
{
    ZipReader zip(path);
    const QStringList sharedStrings = readSharedStrings(zip);
    const QString sheetPath = firstSheetPath(zip);
    const QByteArray sheetData = zip.fileData(sheetPath);
    if (sheetData.isEmpty())
        throw std::runtime_error("No worksheet found in XLSX");

    QList<QStringList> rows;
    int maxColumn = 0;
    QXmlStreamReader xml(sheetData);
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement() || xml.name() != QLatin1String("row"))
            continue;

        QHash<int, QString> cells;
        while (!(xml.isEndElement() && xml.name() == QLatin1String("row")) && !xml.atEnd()) {
            xml.readNext();
            if (!xml.isStartElement() || xml.name() != QLatin1String("c"))
                continue;
            const QString ref = xml.attributes().value(QStringLiteral("r")).toString();
            const QString type = xml.attributes().value(QStringLiteral("t")).toString();
            const int col = columnIndexFromRef(ref);
            if (col < 0)
                continue;
            cells[col] = cellValue(xml, sharedStrings, type);
            maxColumn = std::max(maxColumn, col + 1);
        }

        QStringList row;
        row.resize(maxColumn);
        for (auto it = cells.constBegin(); it != cells.constEnd(); ++it) {
            if (it.key() >= row.size())
                row.resize(it.key() + 1);
            row[it.key()] = it.value();
        }
        rows.append(row);
    }

    TableData table;
    table.format = FileFormat::Xlsx;
    table.path = path;
    table.encoding = TextEncoding::Utf8;
    if (rows.isEmpty())
        return table;

    QStringList headers = rows.first();
    while (headers.size() < maxColumn)
        headers.append(excelColumnName(headers.size()));
    for (int i = 0; i < headers.size(); ++i) {
        if (headers[i].trimmed().isEmpty())
            headers[i] = excelColumnName(i);
    }
    dedupe(headers);

    QList<QStringList> dataRows;
    for (int i = 1; i < rows.size(); ++i) {
        QStringList row = rows[i];
        while (row.size() < headers.size())
            row.append(QString());
        dataRows.append(row.mid(0, headers.size()));
    }

    const QList<ColumnKind> kinds = detectKinds(dataRows, headers.size());
    for (int i = 0; i < headers.size(); ++i) {
        Column col;
        col.name = headers[i];
        col.kind = kinds[i];
        col.dbfType = kinds[i] == ColumnKind::Number ? QLatin1Char('N') : QLatin1Char('C');
        table.columns.append(col);
    }
    for (const QStringList &values : dataRows)
        table.rows.append(Row{false, values});

    return table;
}
