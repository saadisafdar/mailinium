#pragma once

#include <QWidget>
#include <QList>
#include "protocols/MimeMessage.h"

class QLabel;
class QTextBrowser;
class AttachmentBar;
#include "storage/Message.h"

#ifdef MAILINIUM_HAS_WEBENGINE
class QWebEngineView;
#endif

//
// MessageViewPanel — Pane displaying the selected email message.
// Uses QWebEngineView (with fallback to QTextBrowser) for sanitized HTML email rendering,
// displays headers, and shows the AttachmentBar for files.
//
class MessageViewPanel : public QWidget
{
    Q_OBJECT

public:
    explicit MessageViewPanel(QWidget* parent = nullptr);
    ~MessageViewPanel() override;

    void displayMessage(const Message& msg, const QString& rawBody);
    void displayMimeMessage(const Message& msg, const QString& htmlBody, const QString& plainBody, const QList<AttachmentInfo>& attachments);
    void clear();

    // [OPTIMIZATION] Check cache before hitting SQLite
    bool loadFromCache(qint64 messageId, const Message& latestMsg);

private:
    struct CachedMessageData {
        qint64 messageId = -1;
        Message msg;
        QString sanitizedHtml;
        QList<AttachmentInfo> regularAttachments;
    };

    void setupUi();
    void updateMetadataLabels(const Message& msg);
    void cacheMessage(qint64 messageId, const Message& msg, const QString& sanitizedHtml, const QList<AttachmentInfo>& regularAttachments);

    QWidget*       m_headerArea     = nullptr;
    QLabel*        m_placeholderMsg = nullptr;
    QLabel*        m_subjectLabel   = nullptr;
    QLabel*        m_fromLabel      = nullptr;
    QLabel*        m_toLabel        = nullptr;
    QLabel*        m_ccLabel        = nullptr;
    QLabel*        m_bccLabel       = nullptr;
    QLabel*        m_replyToLabel   = nullptr;
    QLabel*        m_dateLabel      = nullptr;
    QLabel*        m_attachmentIndicatorLabel = nullptr;

    AttachmentBar* m_attachmentBar  = nullptr;

    QTextBrowser*  m_textBrowser    = nullptr;
#ifdef MAILINIUM_HAS_WEBENGINE
    QWebEngineView* m_webView       = nullptr;
#endif

    // Cache containers
    QHash<qint64, CachedMessageData> m_viewCache;
    QList<qint64> m_cacheLruList;
    static constexpr int MAX_CACHE_SIZE = 30;
};
