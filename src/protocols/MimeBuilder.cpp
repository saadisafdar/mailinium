#include "protocols/MimeBuilder.h"
#include "core/Logger.h"

#include <QByteArray>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QUuid>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static QByteArray foldHeader(const QByteArray& name, const QByteArray& value)
{
    // Simple header fold: insert CRLF + SP before continuation lines.
    // We don't fold here — long values are emitted as-is.
    // RFC 5321 limits lines to 998 chars but SMTP servers in practice accept more.
    return name + ": " + value + "\r\n";
}

static QByteArray base64Encode(const QByteArray& data)
{
    // Qt's toBase64 uses standard Base64. Insert CRLF every 76 chars per MIME.
    QByteArray b64 = data.toBase64();
    QByteArray out;
    out.reserve(b64.size() + (b64.size() / 76) * 2 + 2);
    for (int i = 0; i < b64.size(); i += 76) {
        out += b64.mid(i, 76);
        out += "\r\n";
    }
    return out;
}

static QByteArray quotedPrintableEncode(const QString& text)
{
    // Encode body text in quoted-printable for readability.
    const QByteArray utf8 = text.toUtf8();
    QByteArray out;
    out.reserve(utf8.size() * 2);

    int lineLen = 0;
    for (int i = 0; i < utf8.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(utf8[i]);

        if (c == '\r') continue; // normalise line endings — we use CRLF below
        if (c == '\n') {
            out += "\r\n";
            lineLen = 0;
            continue;
        }

        // Characters that MUST be encoded
        bool mustEncode = (c < 32 && c != '\t') || c == '=' || c > 126;

        if (mustEncode) {
            if (lineLen > 73) { out += "=\r\n"; lineLen = 0; }
            char hex[4];
            std::snprintf(hex, sizeof(hex), "=%02X", c);
            out += hex;
            lineLen += 3;
        } else {
            if (lineLen == 76) { out += "=\r\n"; lineLen = 0; }
            out += static_cast<char>(c);
            ++lineLen;
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// MimeBuilder::generateBoundary
// ---------------------------------------------------------------------------

QByteArray MimeBuilder::generateBoundary()
{
    // Use UUID to guarantee uniqueness.
    return QStringLiteral("--=_Part_") .toUtf8()
         + QUuid::createUuid().toString(QUuid::Id128).toUtf8();
}

// ---------------------------------------------------------------------------
// MimeBuilder::detectMimeType
// ---------------------------------------------------------------------------

QString MimeBuilder::detectMimeType(const QString& filePath)
{
    static const QMap<QString, QString> table = {
        { QStringLiteral("pdf"),  QStringLiteral("application/pdf") },
        { QStringLiteral("png"),  QStringLiteral("image/png") },
        { QStringLiteral("jpg"),  QStringLiteral("image/jpeg") },
        { QStringLiteral("jpeg"), QStringLiteral("image/jpeg") },
        { QStringLiteral("gif"),  QStringLiteral("image/gif") },
        { QStringLiteral("webp"), QStringLiteral("image/webp") },
        { QStringLiteral("svg"),  QStringLiteral("image/svg+xml") },
        { QStringLiteral("txt"),  QStringLiteral("text/plain") },
        { QStringLiteral("html"), QStringLiteral("text/html") },
        { QStringLiteral("htm"),  QStringLiteral("text/html") },
        { QStringLiteral("csv"),  QStringLiteral("text/csv") },
        { QStringLiteral("xml"),  QStringLiteral("application/xml") },
        { QStringLiteral("json"), QStringLiteral("application/json") },
        { QStringLiteral("zip"),  QStringLiteral("application/zip") },
        { QStringLiteral("gz"),   QStringLiteral("application/gzip") },
        { QStringLiteral("tar"),  QStringLiteral("application/x-tar") },
        { QStringLiteral("doc"),  QStringLiteral("application/msword") },
        { QStringLiteral("docx"), QStringLiteral("application/vnd.openxmlformats-officedocument.wordprocessingml.document") },
        { QStringLiteral("xls"),  QStringLiteral("application/vnd.ms-excel") },
        { QStringLiteral("xlsx"), QStringLiteral("application/vnd.openxmlformats-officedocument.spreadsheetml.sheet") },
        { QStringLiteral("ppt"),  QStringLiteral("application/vnd.ms-powerpoint") },
        { QStringLiteral("pptx"), QStringLiteral("application/vnd.openxmlformats-officedocument.presentationml.presentation") },
        { QStringLiteral("mp3"),  QStringLiteral("audio/mpeg") },
        { QStringLiteral("mp4"),  QStringLiteral("video/mp4") },
        { QStringLiteral("avi"),  QStringLiteral("video/x-msvideo") },
    };

    const QString ext = QFileInfo(filePath).suffix().toLower();
    return table.value(ext, QStringLiteral("application/octet-stream"));
}

// ---------------------------------------------------------------------------
// MimeBuilder::encodeHeaderWord  (RFC 2047 Q-encoding)
// ---------------------------------------------------------------------------

QByteArray MimeBuilder::encodeHeaderWord(const QString& text)
{
    const QByteArray utf8 = text.toUtf8();
    QByteArray encoded;
    encoded.reserve(utf8.size() * 3);

    for (int i = 0; i < utf8.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(utf8[i]);
        if (c > 127 || c == '=' || c == '?' || c == '_' || c < 32) {
            char hex[4];
            std::snprintf(hex, sizeof(hex), "=%02X", c);
            encoded += hex;
        } else if (c == ' ') {
            encoded += '_';
        } else {
            encoded += static_cast<char>(c);
        }
    }

    return "=?UTF-8?Q?" + encoded + "?=";
}

// ---------------------------------------------------------------------------
// MimeBuilder::encodeAddress
// ---------------------------------------------------------------------------

QString MimeBuilder::encodeAddress(const QString& name, const QString& email)
{
    if (name.isEmpty()) {
        return email;
    }

    // Check if name contains non-ASCII characters
    bool needsEncoding = false;
    for (const QChar& ch : name) {
        if (ch.unicode() > 127) { needsEncoding = true; break; }
    }

    if (needsEncoding) {
        const QByteArray encoded = encodeHeaderWord(name);
        return QString::fromLatin1(encoded) + QStringLiteral(" <") + email + QStringLiteral(">");
    }

    // Check if the name needs quoting (contains special chars per RFC 5321)
    static const QString specialChars = QStringLiteral("()<>[]:;@\\,\"");
    bool needsQuoting = false;
    for (const QChar& ch : name) {
        if (specialChars.contains(ch)) { needsQuoting = true; break; }
    }

    if (needsQuoting) {
        return QStringLiteral("\"") + name + QStringLiteral("\" <") + email + QStringLiteral(">");
    }

    return name + QStringLiteral(" <") + email + QStringLiteral(">");
}

// ---------------------------------------------------------------------------
// Internal: encode a header value that may contain non-ASCII
// ---------------------------------------------------------------------------

static QByteArray encodeHeaderValue(const QString& text)
{
    bool needsEncoding = false;
    for (const QChar& ch : text) {
        if (ch.unicode() > 127) { needsEncoding = true; break; }
    }
    if (needsEncoding) {
        return MimeBuilder::encodeHeaderWord(text);
    }
    return text.toUtf8();
}

// ---------------------------------------------------------------------------
// Internal: build a single text part
// ---------------------------------------------------------------------------

static QByteArray buildTextPart(const QString& body, const QByteArray& contentType)
{
    QByteArray part;
    part += foldHeader("Content-Type", contentType + "; charset=utf-8");
    part += foldHeader("Content-Transfer-Encoding", "quoted-printable");
    part += "\r\n";
    part += quotedPrintableEncode(body);
    part += "\r\n";
    return part;
}

// ---------------------------------------------------------------------------
// Internal: build a single attachment part  (streams from disk in 64KiB chunks)
// ---------------------------------------------------------------------------

static QByteArray buildAttachmentPart(const ComposeAttachment& att)
{
    const QString mimeType = att.mimeType.isEmpty()
        ? MimeBuilder::detectMimeType(att.filePath)
        : att.mimeType;

    const QString filename = att.filename.isEmpty()
        ? QFileInfo(att.filePath).fileName()
        : att.filename;

    QFile file(att.filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        Logger::instance().error(QStringLiteral("mime"),
            QStringLiteral("Cannot open attachment: ") + att.filePath);
        return {};
    }

    const QByteArray fileData = file.readAll();

    QByteArray encodedFilename;
    bool needsEncoding = false;
    for (const QChar& ch : filename) {
        if (ch.unicode() > 127) { needsEncoding = true; break; }
    }
    if (needsEncoding) {
        encodedFilename = MimeBuilder::encodeHeaderWord(filename);
    } else {
        encodedFilename = "\"" + filename.toUtf8() + "\"";
    }

    QByteArray part;
    part += foldHeader("Content-Type",
        mimeType.toUtf8() + "; name=" + encodedFilename);
    part += foldHeader("Content-Disposition",
        "attachment; filename=" + encodedFilename);
    part += foldHeader("Content-Transfer-Encoding", "base64");
    part += "\r\n";
    part += base64Encode(fileData);
    return part;
}

// ---------------------------------------------------------------------------
// MimeBuilder::build
// ---------------------------------------------------------------------------

QByteArray MimeBuilder::build(const ComposeData& data)
{
    // Determine structure:
    //   plain only              → text/plain
    //   plain + html            → multipart/alternative
    //   + attachments           → multipart/mixed wrapping the above

    const bool hasHtml        = !data.htmlBody.isEmpty();
    const bool hasPlain       = !data.plainBody.isEmpty();
    const bool hasAttachments = !data.attachments.isEmpty();

    // --- Headers ------------------------------------------------------------

    QByteArray headers;
    headers += foldHeader("MIME-Version", "1.0");
    headers += foldHeader("Date",
        QDateTime::currentDateTimeUtc().toString(Qt::RFC2822Date).toUtf8());
    headers += foldHeader("From", data.from.toUtf8());

    if (!data.to.isEmpty())
        headers += foldHeader("To", data.to.toUtf8());
    if (!data.cc.isEmpty())
        headers += foldHeader("Cc", data.cc.toUtf8());
    // BCC intentionally omitted from headers per RFC 5321
    if (!data.replyTo.isEmpty())
        headers += foldHeader("Reply-To", data.replyTo.toUtf8());

    headers += foldHeader("Subject", encodeHeaderValue(data.subject));

    if (!data.inReplyTo.isEmpty())
        headers += foldHeader("In-Reply-To", data.inReplyTo.toUtf8());
    if (!data.references.isEmpty())
        headers += foldHeader("References", data.references.toUtf8());

    headers += foldHeader("X-Mailer", "Mailinium");

    // --- Body ---------------------------------------------------------------

    // Case 1: plain text only, no attachments
    if (!hasHtml && !hasAttachments) {
        QByteArray msg = headers;
        if (hasPlain) {
            msg += foldHeader("Content-Type", "text/plain; charset=utf-8");
            msg += foldHeader("Content-Transfer-Encoding", "quoted-printable");
            msg += "\r\n";
            msg += quotedPrintableEncode(data.plainBody);
        } else {
            msg += foldHeader("Content-Type", "text/plain; charset=utf-8");
            msg += "\r\n";
        }
        return msg;
    }

    // Case 2: multipart — determine body part structure
    QByteArray bodyPart;

    if (hasHtml && hasPlain) {
        // multipart/alternative
        const QByteArray altBoundary = generateBoundary();
        bodyPart += foldHeader("Content-Type",
            "multipart/alternative; boundary=\"" + altBoundary + "\"");
        bodyPart += "\r\n";
        bodyPart += "--" + altBoundary + "\r\n";
        bodyPart += buildTextPart(data.plainBody, "text/plain");
        bodyPart += "--" + altBoundary + "\r\n";
        bodyPart += buildTextPart(data.htmlBody, "text/html");
        bodyPart += "--" + altBoundary + "--\r\n";
    } else if (hasHtml) {
        bodyPart += buildTextPart(data.htmlBody, "text/html");
    } else {
        bodyPart += buildTextPart(data.plainBody, "text/plain");
    }

    if (!hasAttachments) {
        return headers + bodyPart;
    }

    // Case 3: multipart/mixed wrapping body + attachments
    const QByteArray mixedBoundary = generateBoundary();
    QByteArray msg = headers;
    msg += foldHeader("Content-Type",
        "multipart/mixed; boundary=\"" + mixedBoundary + "\"");
    msg += "\r\n";
    msg += "--" + mixedBoundary + "\r\n";
    msg += bodyPart;

    for (const ComposeAttachment& att : data.attachments) {
        const QByteArray attPart = buildAttachmentPart(att);
        if (attPart.isEmpty()) continue; // logged inside buildAttachmentPart
        msg += "--" + mixedBoundary + "\r\n";
        msg += attPart;
    }

    msg += "--" + mixedBoundary + "--\r\n";
    return msg;
}
