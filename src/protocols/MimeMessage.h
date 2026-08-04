#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

//
// AttachmentInfo — Represents a file attachment extracted from a MIME message.
//
struct AttachmentInfo {
    qint64     id          = -1;
    qint64     messageId   = -1;
    QString    filename;
    QString    contentType;
    QString    contentId;   // e.g. "image001@domain" for inline cid:
    qint64     size        = 0;
    bool       isInline    = false;
    QByteArray data;
};

//
// MimePart — Individual section of a multipart MIME message.
//
struct MimePart {
    QString            contentType;      // e.g. "text/html", "image/png"
    QString            charset           = QStringLiteral("utf-8");
    QString            transferEncoding; // e.g. "quoted-printable", "base64"
    QString            contentDisposition; // e.g. "inline", "attachment"
    QString            filename;
    QString            contentId;
    QByteArray         rawData;
    QString            decodedText;
};

//
// MimeMessage — Parsed result of a full email MIME structure.
//
struct MimeMessage {
    QString               htmlBody;
    QString               plainBody;
    QList<AttachmentInfo> attachments;
    QList<AttachmentInfo> inlineImages;
};
