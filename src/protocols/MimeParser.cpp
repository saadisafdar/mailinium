#include "protocols/MimeParser.h"
#include "core/Logger.h"

#include <QRegularExpression>
#include <QStringDecoder>
#include <QTextBoundaryFinder>

static inline int hexToVal(char c, bool& ok)
{
    if (c >= '0' && c <= '9') { ok = true; return c - '0'; }
    if (c >= 'a' && c <= 'f') { ok = true; return c - 'a' + 10; }
    if (c >= 'A' && c <= 'F') { ok = true; return c - 'A' + 10; }
    ok = false;
    return 0;
}

static inline bool isSpaceChar(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

QByteArray MimeParser::decodeQuotedPrintable(const QByteArray& input)
{
    QByteArray output;
    output.reserve(input.size());

    int i = 0;
    const int len = input.size();

    while (i < len) {
        char ch = input.at(i);
        if (ch == '=') {
            if (i + 2 < len) {
                char next1 = input.at(i + 1);
                char next2 = input.at(i + 2);

                if (next1 == '\r' && next2 == '\n') {
                    // Soft line break: =CRLF
                    i += 3;
                    continue;
                } else if (next1 == '\n') {
                    // Soft line break: =LF
                    i += 2;
                    continue;
                } else {
                    // Hex byte: =XX
                    bool ok1 = false, ok2 = false;
                    int h1 = hexToVal(next1, ok1);
                    int h2 = hexToVal(next2, ok2);

                    if (ok1 && ok2) {
                        output.append(static_cast<char>((h1 << 4) | h2));
                        i += 3;
                        continue;
                    }
                }
            }
        }
        output.append(ch);
        ++i;
    }

    return output;
}

QString MimeParser::decodeTextPart(const QByteArray& data, const QString& transferEncoding, const QString& charset)
{
    QByteArray decodedBytes;
    QString encLower = transferEncoding.toLower().trimmed();

    if (encLower == QStringLiteral("base64")) {
        decodedBytes = QByteArray::fromBase64(data);
    } else if (encLower == QStringLiteral("quoted-printable")) {
        decodedBytes = decodeQuotedPrintable(data);
    } else {
        decodedBytes = data;
    }

    // Convert charset to QString
    QString csLower = charset.toLower().trimmed();
    if (csLower.isEmpty() || csLower == QStringLiteral("us-ascii")) {
        csLower = QStringLiteral("utf-8");
    }

    auto decoder = QStringDecoder(csLower.toUtf8().constData());
    if (decoder.isValid()) {
        return decoder(decodedBytes);
    }

    // Fallback UTF-8
    return QString::fromUtf8(decodedBytes);
}

QString MimeParser::decodeHeaderValue(const QString& input)
{
    if (!input.contains(QStringLiteral("=?"))) {
        return input;
    }

    // Matches RFC 2047 encoded words: =?charset?encoding?text?=
    static const QRegularExpression regex(QStringLiteral("=\\?([^?]+)\\?([qQbB])\\?([^?]+)\\?="));
    static const QRegularExpression betweenEncodedWords(QStringLiteral("\\?=\\s+=\\?"));

    QString result = input;
    result.replace(betweenEncodedWords, QStringLiteral("?==?"));

    QRegularExpressionMatchIterator it = regex.globalMatch(result);

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString fullMatch = match.captured(0);
        QString charset = match.captured(1);
        QString encType = match.captured(2).toUpper();
        QString encodedText = match.captured(3);

        QByteArray raw;
        if (encType == QStringLiteral("B")) {
            raw = QByteArray::fromBase64(encodedText.toLatin1());
        } else if (encType == QStringLiteral("Q")) {
            // In Q encoding, '_' represents space
            QByteArray qData = encodedText.toLatin1();
            qData.replace('_', ' ');
            raw = decodeQuotedPrintable(qData);
        }

        auto decoder = QStringDecoder(charset.toUtf8().constData());
        QString decodedStr = decoder.isValid() ? decoder(raw) : QString::fromUtf8(raw);
        result.replace(fullMatch, decodedStr);
    }

    return result;
}

static void parsePart(const QByteArray& fullData, int start, int end, MimeMessage& msg)
{
    if (start >= end) return;

    // Separate headers from payload
    int headerEnd = -1;
    int headerLen = 4;

    int searchIdx = fullData.indexOf("\r\n\r\n", start);
    if (searchIdx != -1 && searchIdx + 4 <= end) {
        headerEnd = searchIdx;
        headerLen = 4;
    } else {
        searchIdx = fullData.indexOf("\n\n", start);
        if (searchIdx != -1 && searchIdx + 2 <= end) {
            headerEnd = searchIdx;
            headerLen = 2;
        }
    }

    int bodyStart = start;
    int bodyEnd = end;
    int headerStart = start;
    int headerEndPos = start;

    if (headerEnd != -1) {
        headerEndPos = headerEnd;
        bodyStart = headerEnd + headerLen;
    } else {
        headerEndPos = start;
        bodyStart = start;
    }

    // Parse headers into variables
    QString contentType = QStringLiteral("text/plain");
    QString charset = QStringLiteral("utf-8");
    QString transferEncoding;
    QString contentDisposition;
    QString filename;
    QString contentId;

    if (headerEndPos > headerStart) {
        QString headerStr = QString::fromUtf8(fullData.constData() + headerStart, headerEndPos - headerStart);
        QStringList rawLines = headerStr.split(QLatin1Char('\n'));
        QStringList lines;
        lines.reserve(rawLines.size());

        for (const QString& rawLine : rawLines) {
            QString trimmedLine = rawLine;
            if (trimmedLine.endsWith(QLatin1Char('\r'))) {
                trimmedLine.chop(1);
            }
            if (trimmedLine.isEmpty()) continue;

            if ((trimmedLine.startsWith(QLatin1Char(' ')) || trimmedLine.startsWith(QLatin1Char('\t'))) && !lines.isEmpty()) {
                lines.last() += QLatin1Char(' ') + trimmedLine.trimmed();
            } else {
                lines.append(trimmedLine);
            }
        }

        for (const QString& line : lines) {
            int colonIdx = line.indexOf(':');
            if (colonIdx == -1) continue;

            QString name = line.left(colonIdx).trimmed().toLower();
            QString val = line.mid(colonIdx + 1).trimmed();

            if (name == QStringLiteral("content-type")) {
                QStringList parts = val.split(';');
                contentType = parts.first().trimmed().toLower();
                for (int i = 1; i < parts.size(); ++i) {
                    QString p = parts[i].trimmed();
                    if (p.toLower().startsWith(QStringLiteral("charset="))) {
                        charset = p.mid(8).trimmed().remove('"');
                    } else if (p.toLower().startsWith(QStringLiteral("name="))) {
                        filename = p.mid(5).trimmed().remove('"');
                    }
                }
            } else if (name == QStringLiteral("content-transfer-encoding")) {
                transferEncoding = val.toLower().trimmed();
            } else if (name == QStringLiteral("content-disposition")) {
                QStringList parts = val.split(';');
                contentDisposition = parts.first().trimmed().toLower();
                for (int i = 1; i < parts.size(); ++i) {
                    QString p = parts[i].trimmed();
                    if (p.toLower().startsWith(QStringLiteral("filename="))) {
                        filename = p.mid(9).trimmed().remove('"');
                    }
                }
            } else if (name == QStringLiteral("content-id")) {
                contentId = val.trimmed().remove('<').remove('>');
            }
        }
    }

    // Check for boundary if multipart
    if (contentType.startsWith(QStringLiteral("multipart/"))) {
        QString headerStr = QString::fromUtf8(fullData.constData() + headerStart, headerEndPos - headerStart);
        int boundaryIdx = headerStr.indexOf(QStringLiteral("boundary="), 0, Qt::CaseInsensitive);
        if (boundaryIdx != -1) {
            int endIdx = headerStr.length();
            for (int i = boundaryIdx + 9; i < headerStr.length(); ++i) {
                QChar c = headerStr.at(i);
                if (c == ';' || c == '\r' || c == '\n') {
                    endIdx = i;
                    break;
                }
            }
            QString boundary = headerStr.mid(boundaryIdx + 9, endIdx - (boundaryIdx + 9)).trimmed().remove('"');

            QByteArray boundaryDelimiter = "--" + boundary.toUtf8();
            int bLen = boundaryDelimiter.size();
            int pos = fullData.indexOf(boundaryDelimiter, bodyStart);
            while (pos != -1 && pos < bodyEnd) {
                int nextPos = fullData.indexOf(boundaryDelimiter, pos + bLen);
                if (nextPos == -1 || nextPos > bodyEnd) {
                    nextPos = bodyEnd;
                }

                int subStart = pos + bLen;
                int subEnd = nextPos;

                while (subStart < subEnd && isSpaceChar(fullData.at(subStart))) {
                    subStart++;
                }
                while (subEnd > subStart && isSpaceChar(fullData.at(subEnd - 1))) {
                    subEnd--;
                }

                bool isClosing = false;
                if (subEnd - subStart >= 2 && fullData.at(subStart) == '-' && fullData.at(subStart + 1) == '-') {
                    isClosing = true;
                }

                if (subStart < subEnd && !isClosing) {
                    parsePart(fullData, subStart, subEnd, msg);
                }

                if (nextPos == bodyEnd) {
                    break;
                }
                pos = nextPos;
            }
            return;
        }
    }

    // Zero-copy view using fromRawData for leaf payload
    QByteArray rawLeafData = QByteArray::fromRawData(fullData.constData() + bodyStart, bodyEnd - bodyStart);

    bool isAttachment = (contentDisposition == QStringLiteral("attachment")) || !filename.isEmpty();
    bool isInlineImage = (contentDisposition == QStringLiteral("inline") || !contentId.isEmpty()) && contentType.startsWith(QStringLiteral("image/"));

    if (isAttachment && !isInlineImage) {
        AttachmentInfo att;
        att.filename = MimeParser::decodeHeaderValue(filename.isEmpty() ? QStringLiteral("attachment.dat") : filename);
        att.contentType = contentType;
        att.contentId = contentId;
        att.isInline = false;

        QString encLower = transferEncoding.toLower().trimmed();
        if (encLower == QStringLiteral("base64")) {
            att.data = QByteArray::fromBase64(rawLeafData);
        } else if (encLower == QStringLiteral("quoted-printable")) {
            att.data = MimeParser::decodeQuotedPrintable(rawLeafData);
        } else {
            att.data = rawLeafData;
        }
        att.size = att.data.size();
        msg.attachments.append(std::move(att));
    } else if (isInlineImage) {
        AttachmentInfo att;
        att.filename = MimeParser::decodeHeaderValue(filename.isEmpty() ? QStringLiteral("inline_image") : filename);
        att.contentType = contentType;
        att.contentId = contentId;
        att.isInline = true;

        QString encLower = transferEncoding.toLower().trimmed();
        if (encLower == QStringLiteral("base64")) {
            att.data = QByteArray::fromBase64(rawLeafData);
        } else if (encLower == QStringLiteral("quoted-printable")) {
            att.data = MimeParser::decodeQuotedPrintable(rawLeafData);
        } else {
            att.data = rawLeafData;
        }
        att.size = att.data.size();
        msg.inlineImages.append(std::move(att));
    } else {
        QString decodedText = MimeParser::decodeTextPart(rawLeafData, transferEncoding, charset);
        if (contentType == QStringLiteral("text/html")) {
            msg.htmlBody = std::move(decodedText);
        } else if (contentType == QStringLiteral("text/plain")) {
            msg.plainBody = std::move(decodedText);
        }
    }
}

MimeMessage MimeParser::parse(const QByteArray& rawMessage)
{
    MimeMessage msg;
    if (rawMessage.isEmpty()) return msg;

    parsePart(rawMessage, 0, rawMessage.size(), msg);

    if (msg.htmlBody.isEmpty() && !msg.plainBody.isEmpty()) {
        QString escaped = msg.plainBody.toHtmlEscaped();
        escaped.replace('\n', QStringLiteral("<br>"));
        msg.htmlBody = QStringLiteral("<html><body><div>%1</div></body></html>").arg(escaped);
    }

    return msg;
}
