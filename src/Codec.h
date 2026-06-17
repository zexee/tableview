#pragma once

#include "DataTypes.h"

#include <QByteArray>
#include <QString>

QString decodeText(const QByteArray &bytes, TextEncoding encoding);
QByteArray encodeText(const QString &text, TextEncoding encoding);
TextEncoding detectEncoding(const QByteArray &sample);

