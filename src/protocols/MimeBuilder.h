#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

//
// ComposeAttachment — a file attachment to include in an outgoing message.
//
struct ComposeAttachment {
    QString    filePath;       // absolute path to file on disk
    QString    filename;       // display filename (defaults to basename of filePath)
    QString    mimeType;       // e.g. "application/pdf" — detected if empty
};

//
// ComposeData — all data needed to build an outgoing MIME message.
//
struct ComposeData {
    QString                   from;       // e.g. "Jane Doe <jane@example.com>"
    QString                   to;         // comma-separated recipients
    QString                   cc;
    QString                   bcc;
    QString                   replyTo;
    QString                   subject;
    QString                   plainBody;
    QString                   htmlBody;
    QString                   inReplyTo;  // Message-ID of the parent (for replies)
    QString                   references; // space-separated chain of Message-IDs
    QList<ComposeAttachment>  attachments;
};

//
// MimeBuilder — builds RFC-compliant MIME messages for sending via SMTP.
//
// All methods are static and free of side-effects.
//
// Usage:
//   ComposeData cd;
//   cd.from    = "me@example.com";
//   cd.to      = "you@example.com";
//   cd.subject = "Hello";
//   cd.plainBody = "Hi there.";
//   QByteArray raw = MimeBuilder::build(cd);
//
class MimeBuilder
{
public:
    // Build a complete raw MIME message as bytes ready to be sent over SMTP DATA.
    // Returns an empty QByteArray on failure (logged internally).
    static QByteArray build(const ComposeData& data);

    // Encode a display-name + address into RFC 5322 mailbox format.
    // If name contains non-ASCII characters, RFC 2047 Q-encoding is applied.
    // Examples:
    //   "Jane Doe", "jane@example.com"  → "Jane Doe <jane@example.com>"
    //   "日本語",   "test@example.com"  → "=?UTF-8?Q?...?= <test@example.com>"
    static QString encodeAddress(const QString& name, const QString& email);

    // RFC 2047 Q-encoding for a single header value (non-ASCII text).
    static QByteArray encodeHeaderWord(const QString& text);

    // Detect MIME type from file extension. Returns "application/octet-stream" as fallback.
    static QString detectMimeType(const QString& filePath);

    // Generate a unique MIME boundary string.
    static QByteArray generateBoundary();
};
