#include "DbfReader.h"

#include "Codec.h"

#include <QDate>
#include <QFile>
#include <QtEndian>
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace {

constexpr uchar DeletedFlag = 0x2A;
constexpr uchar ValidFlag = 0x20;

QByteArray slice(const QByteArray &data, int offset, int length)
{
    if (offset >= data.size())
        return {};
    return data.mid(offset, std::min<int>(length, static_cast<int>(data.size() - offset)));
}

} // namespace

TextEncoding detectDbfEncoding(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        throw std::runtime_error(file.errorString().toStdString());

    const QByteArray header = file.read(32);
    if (header.size() < 32)
        return TextEncoding::Gbk;

    const int headerLength = qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(header.constData() + 8));
    if (headerLength <= 32)
        return TextEncoding::Gbk;
    return detectEncoding(file.read(headerLength - 32));
}

TableData readDbfFile(const QString &path, TextEncoding encoding)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        throw std::runtime_error(file.errorString().toStdString());

    const QByteArray bytes = file.readAll();
    if (bytes.size() < 32)
        throw std::runtime_error("Invalid DBF header");

    const int recordCount = qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(bytes.constData() + 4));
    const int headerLength = qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(bytes.constData() + 8));
    const int recordLength = qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(bytes.constData() + 10));
    const int fieldCount = (headerLength - 33) / 32;

    TableData table;
    table.format = FileFormat::Dbf;
    table.path = path;
    table.encoding = encoding;

    for (int i = 0; i < fieldCount; ++i) {
        const int offset = 32 + i * 32;
        const QByteArray desc = slice(bytes, offset, 32);
        if (desc.size() < 32)
            break;
        int nameLength = desc.left(11).indexOf('\0');
        if (nameLength < 0)
            nameLength = 11;
        Column col;
        col.name = decodeText(desc.left(nameLength), encoding).trimmed();
        if (col.name.isEmpty())
            col.name = QStringLiteral("F%1").arg(i);
        col.dbfType = QChar::fromLatin1(desc[11]);
        col.dbfLength = static_cast<uchar>(desc[16]);
        col.dbfDecimals = static_cast<uchar>(desc[17]);
        col.kind = (col.dbfType == QLatin1Char('N') || col.dbfType == QLatin1Char('F')) ? ColumnKind::Number : ColumnKind::Text;
        table.columns.append(col);
    }

    int recordOffset = headerLength;
    for (int r = 0; r < recordCount && recordOffset + recordLength <= bytes.size(); ++r, recordOffset += recordLength) {
        const QByteArray record = bytes.mid(recordOffset, recordLength);
        Row row;
        row.deleted = static_cast<uchar>(record[0]) == DeletedFlag;
        int fieldOffset = 1;
        for (const Column &col : table.columns) {
            QString value;
            if (!row.deleted) {
                value = decodeText(record.mid(fieldOffset, col.dbfLength), encoding)
                            .remove(QChar::Null)
                            .trimmed();
                if (col.kind == ColumnKind::Number) {
                    bool ok = false;
                    value.toDouble(&ok);
                    if (!ok)
                        value.clear();
                }
            }
            row.values.append(value);
            fieldOffset += col.dbfLength;
        }
        table.rows.append(row);
    }

    return table;
}

void saveDbfFile(const QString &path, const TableData &table)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        throw std::runtime_error(file.errorString().toStdString());

    const int headerLength = 32 + table.columns.size() * 32 + 1;
    int recordLength = 1;
    for (const Column &col : table.columns)
        recordLength += std::max(1, col.dbfLength);

    QByteArray header(32, 0);
    const QDate now = QDate::currentDate();
    header[0] = char(0x30);
    header[1] = char(now.year() - 1900);
    header[2] = char(now.month());
    header[3] = char(now.day());
    qToLittleEndian<quint32>(table.rows.size(), reinterpret_cast<uchar *>(header.data() + 4));
    qToLittleEndian<quint16>(headerLength, reinterpret_cast<uchar *>(header.data() + 8));
    qToLittleEndian<quint16>(recordLength, reinterpret_cast<uchar *>(header.data() + 10));
    header[29] = char(0x86);
    file.write(header);

    for (const Column &col : table.columns) {
        QByteArray desc(32, 0);
        QByteArray name = encodeText(col.name, table.encoding).left(10);
        std::memcpy(desc.data(), name.constData(), name.size());
        desc[11] = col.dbfType.toLatin1();
        desc[16] = char(std::max(1, std::min(255, col.dbfLength)));
        desc[17] = char(std::max(0, std::min(15, col.dbfDecimals)));
        file.write(desc);
    }
    file.write(QByteArray(1, char(0x0D)));

    for (const Row &row : table.rows) {
        file.write(QByteArray(1, row.deleted ? char(DeletedFlag) : char(ValidFlag)));
        for (int i = 0; i < table.columns.size(); ++i) {
            const Column &col = table.columns[i];
            QString value = i < row.values.size() ? row.values[i] : QString();
            if (col.kind == ColumnKind::Number && !value.trimmed().isEmpty()) {
                bool ok = false;
                const double num = value.toDouble(&ok);
                value = ok ? QString::number(num, 'f', col.dbfDecimals) : QString();
            }
            QByteArray encoded = encodeText(value, table.encoding).left(col.dbfLength);
            if (encoded.size() < col.dbfLength)
                encoded.append(QByteArray(col.dbfLength - encoded.size(), ' '));
            file.write(encoded);
        }
    }
    file.write(QByteArray(1, char(0x1A)));
}
