#pragma once

#include <QString>
#include <QList>
#include "protocols/MimeMessage.h"

//
// HtmlSanitizer — Cleans untrusted HTML email content before rendering.
// Removes script tags, unsafe event handlers, handles cid: inline image substitution,
// and enforces remote image privacy policies based on Settings.
//
class HtmlSanitizer
{
public:
    HtmlSanitizer() = default;

    // Sanitizes input HTML string and injects data URIs for inline cid: images.
    static QString sanitize(const QString& rawHtml, const QList<AttachmentInfo>& inlineImages);
};
