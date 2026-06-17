#include "Codec.h"

#include <QStringConverter>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <iconv.h>
#endif

namespace {

QString decodeGbk(const QByteArray &bytes)
{
#ifdef Q_OS_WIN
    const int wideLen = MultiByteToWideChar(936, 0, bytes.constData(), bytes.size(), nullptr, 0);
    if (wideLen <= 0)
        return QString::fromUtf8(bytes);
    QString out(wideLen, Qt::Uninitialized);
    MultiByteToWideChar(936, 0, bytes.constData(), bytes.size(), reinterpret_cast<wchar_t *>(out.data()), wideLen);
    return out;
#else
    iconv_t cd = iconv_open("UTF-8", "GBK");
    if (cd == reinterpret_cast<iconv_t>(-1))
        return QString::fromUtf8(bytes);

    QByteArray input = bytes;
    QByteArray output(bytes.size() * 4 + 8, 0);
    char *in = input.data();
    char *out = output.data();
    size_t inLeft = static_cast<size_t>(input.size());
    size_t outLeft = static_cast<size_t>(output.size());
    const size_t result = iconv(cd, &in, &inLeft, &out, &outLeft);
    iconv_close(cd);
    if (result == static_cast<size_t>(-1))
        return QString::fromUtf8(bytes);
    output.truncate(output.size() - static_cast<int>(outLeft));
    return QString::fromUtf8(output);
#endif
}

QByteArray encodeGbk(const QString &text)
{
#ifdef Q_OS_WIN
    const std::wstring wide = text.toStdWString();
    const int len = WideCharToMultiByte(936, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    QByteArray out(len, 0);
    if (len > 0)
        WideCharToMultiByte(936, 0, wide.data(), static_cast<int>(wide.size()), out.data(), len, nullptr, nullptr);
    return out;
#else
    const QByteArray inputUtf8 = text.toUtf8();
    iconv_t cd = iconv_open("GBK", "UTF-8");
    if (cd == reinterpret_cast<iconv_t>(-1))
        return inputUtf8;

    QByteArray input = inputUtf8;
    QByteArray output(input.size() * 2 + 16, 0);
    char *in = input.data();
    char *out = output.data();
    size_t inLeft = static_cast<size_t>(input.size());
    size_t outLeft = static_cast<size_t>(output.size());
    const size_t result = iconv(cd, &in, &inLeft, &out, &outLeft);
    iconv_close(cd);
    if (result == static_cast<size_t>(-1))
        return inputUtf8;
    output.truncate(output.size() - static_cast<int>(outLeft));
    return output;
#endif
}

} // namespace

QString decodeText(const QByteArray &bytes, TextEncoding encoding)
{
    if (encoding == TextEncoding::Utf8)
        return QString::fromUtf8(bytes);
    return decodeGbk(bytes);
}

QByteArray encodeText(const QString &text, TextEncoding encoding)
{
    if (encoding == TextEncoding::Utf8)
        return text.toUtf8();
    return encodeGbk(text);
}

TextEncoding detectEncoding(const QByteArray &sample)
{
    if (sample.startsWith("\xEF\xBB\xBF"))
        return TextEncoding::Utf8;

    QStringDecoder decoder(QStringDecoder::Utf8);
    decoder.decode(sample);
    return decoder.hasError() ? TextEncoding::Gbk : TextEncoding::Utf8;
}
