#pragma once

#include <QByteArray>
#include <QString>
#include "protocols/MimeMessage.h"

//
// MimeParser — Native parser for MIME email messages.
// Supports text/plain, text/html, multipart/alternative, multipart/mixed, multipart/related,
// Quoted-Printable decoding, and Base64 decoding.
//
class MimeParser
{
public:
    MimeParser() = default;

    // Parses a raw RFC 822 email payload and returns a MimeMessage structure.
    static MimeMessage parse(const QByteArray& rawMessage);

    // Decodes Quoted-Printable encoded data into raw bytes.
    static QByteArray decodeQuotedPrintable(const QByteArray& input);

    // Decodes Base64 or Quoted-Printable data using the part's transfer encoding and charset.
    static QString decodeTextPart(const QByteArray& data, const QString& transferEncoding, const QString& charset);

    // Decodes MIME encoded-words in header values (RFC 2047, e.g. "=?UTF-8?B?...?=" or "=?UTF-8?Q?...?=")
    static QString decodeHeaderValue(const QString& input);
};
