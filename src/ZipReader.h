#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>

class ZipReader {
public:
    explicit ZipReader(const QString &path);
    QByteArray fileData(const QString &name) const;
    bool contains(const QString &name) const;

private:
    struct Entry {
        quint16 method = 0;
        quint32 compressedSize = 0;
        quint32 uncompressedSize = 0;
        quint32 localHeaderOffset = 0;
    };

    QByteArray m_data;
    QHash<QString, Entry> m_entries;
};

