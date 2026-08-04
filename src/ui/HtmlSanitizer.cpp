#include "ui/HtmlSanitizer.h"
#include "core/Settings.h"

#include <QRegularExpression>

QString HtmlSanitizer::sanitize(const QString& rawHtml, const QList<AttachmentInfo>& inlineImages)
{
    if (rawHtml.isEmpty()) {
        return QStringLiteral("<html><body></body></html>");
    }

    QString clean = rawHtml;

    // 1. Remove <script>...</script> tags
    static const QRegularExpression scriptRegex(QStringLiteral("<script[^>]*>.*?</script>"), QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
    clean.remove(scriptRegex);

    // 2. Remove <iframe>, <object>, <embed>
    static const QRegularExpression unsafeTags(QStringLiteral("<(iframe|object|embed)[^>]*>.*?</\\1>"), QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
    clean.remove(unsafeTags);

    // 3. Remove inline event handlers (on*="...")
    static const QRegularExpression onEvents(QStringLiteral("\\s+on[a-z]+\\s*=\\s*(\"[^\"]*\"|'[^']*'|[^\\s>]+)"), QRegularExpression::CaseInsensitiveOption);
    clean.remove(onEvents);

    // 4. Handle cid: inline image replacements
    for (const AttachmentInfo& img : inlineImages) {
        if (img.contentId.isEmpty() || img.data.isEmpty()) continue;

        QString mime = img.contentType.isEmpty() ? QStringLiteral("image/png") : img.contentType;
        QString base64Data = QString::fromLatin1(img.data.toBase64());
        QString dataUri = QStringLiteral("data:%1;base64,%2").arg(mime, base64Data);

        // Replace cid:contentId or cid:<contentId>
        clean.replace(QStringLiteral("cid:") + img.contentId, dataUri, Qt::CaseInsensitive);
        clean.replace(QStringLiteral("cid:<") + img.contentId + QStringLiteral(">"), dataUri, Qt::CaseInsensitive);
    }

    // 5. Remote image privacy policy check
    bool loadRemoteImages = Settings::instance().boolValue(Settings::Key::PrivacyLoadRemoteImages);
    if (!loadRemoteImages) {
        // Block external http/https image src attributes unless cid: or data:
        static const QRegularExpression remoteImgRegex(QStringLiteral("src\\s*=\\s*([\"'])(https?://[^\"']+)\\1"), QRegularExpression::CaseInsensitiveOption);
        clean.replace(remoteImgRegex, QStringLiteral("src=\"about:blank\" alt=\"[Remote image blocked]\""));
    }

    return clean;
}
