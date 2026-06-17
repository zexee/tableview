#include "ZipReader.h"

#include <QFile>
#include <QtEndian>
#include <algorithm>
#include <stdexcept>
#include <zlib.h>

namespace {

quint16 u16(const QByteArray &data, int offset)
{
    return qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(data.constData() + offset));
}

quint32 u32(const QByteArray &data, int offset)
{
    return qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(data.constData() + offset));
}

QByteArray inflateRaw(const QByteArray &input, int expectedSize)
{
    QByteArray output(expectedSize, 0);
    z_stream stream {};
    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(input.constData()));
    stream.avail_in = static_cast<uInt>(input.size());
    stream.next_out = reinterpret_cast<Bytef *>(output.data());
    stream.avail_out = static_cast<uInt>(output.size());

    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK)
        throw std::runtime_error("Failed to initialize deflate stream");
    const int result = inflate(&stream, Z_FINISH);
    inflateEnd(&stream);
    if (result != Z_STREAM_END)
        throw std::runtime_error("Failed to inflate ZIP entry");
    output.truncate(static_cast<int>(stream.total_out));
    return output;
}

} // namespace

ZipReader::ZipReader(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        throw std::runtime_error(file.errorString().toStdString());
    m_data = file.readAll();

    int eocd = -1;
    const int minOffset = std::max<int>(0, static_cast<int>(m_data.size()) - 66000);
    for (int i = m_data.size() - 22; i >= minOffset; --i) {
        if (u32(m_data, i) == 0x06054b50) {
            eocd = i;
            break;
        }
    }
    if (eocd < 0)
        throw std::runtime_error("Invalid ZIP: end of central directory not found");

    const int entries = u16(m_data, eocd + 10);
    int centralOffset = static_cast<int>(u32(m_data, eocd + 16));
    for (int i = 0; i < entries; ++i) {
        if (centralOffset + 46 > m_data.size() || u32(m_data, centralOffset) != 0x02014b50)
            throw std::runtime_error("Invalid ZIP central directory");

        const int nameLen = u16(m_data, centralOffset + 28);
        const int extraLen = u16(m_data, centralOffset + 30);
        const int commentLen = u16(m_data, centralOffset + 32);
        Entry entry;
        entry.method = u16(m_data, centralOffset + 10);
        entry.compressedSize = u32(m_data, centralOffset + 20);
        entry.uncompressedSize = u32(m_data, centralOffset + 24);
        entry.localHeaderOffset = u32(m_data, centralOffset + 42);
        const QString name = QString::fromUtf8(m_data.mid(centralOffset + 46, nameLen));
        m_entries.insert(name, entry);
        centralOffset += 46 + nameLen + extraLen + commentLen;
    }
}

bool ZipReader::contains(const QString &name) const
{
    return m_entries.contains(name);
}

QByteArray ZipReader::fileData(const QString &name) const
{
    const auto it = m_entries.constFind(name);
    if (it == m_entries.constEnd())
        return {};

    const Entry &entry = it.value();
    const int local = static_cast<int>(entry.localHeaderOffset);
    if (local + 30 > m_data.size() || u32(m_data, local) != 0x04034b50)
        throw std::runtime_error("Invalid ZIP local header");
    const int nameLen = u16(m_data, local + 26);
    const int extraLen = u16(m_data, local + 28);
    const int dataOffset = local + 30 + nameLen + extraLen;
    const QByteArray compressed = m_data.mid(dataOffset, entry.compressedSize);

    if (entry.method == 0)
        return compressed;
    if (entry.method == 8)
        return inflateRaw(compressed, static_cast<int>(entry.uncompressedSize));
    throw std::runtime_error("Unsupported ZIP compression method");
}
