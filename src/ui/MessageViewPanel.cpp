#include "ui/MessageViewPanel.h"
#include "ui/HtmlSanitizer.h"
#include "ui/AttachmentBar.h"
#include "storage/Message.h"
#include "protocols/MimeMessage.h"
#include "protocols/MimeParser.h"
#include "core/Logger.h"
#include "core/Settings.h"

#include <QApplication>
#include <QFrame>
#include <QLabel>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QHBoxLayout>

#ifdef MAILINIUM_HAS_WEBENGINE
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QDesktopServices>

class ExternalLinkPage : public QWebEnginePage {
    Q_OBJECT
public:
    explicit ExternalLinkPage(QObject* parent = nullptr) : QWebEnginePage(parent) {}
protected:
    bool acceptNavigationRequest(const QUrl& url, NavigationType type, bool isMainFrame) override {
        Q_UNUSED(isMainFrame);
        if (type == NavigationTypeLinkClicked) {
            QDesktopServices::openUrl(url);
            return false;
        }
        return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
    }
};
#endif

MessageViewPanel::MessageViewPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();

    connect(&Settings::instance(), &Settings::settingChanged, this, [this](Settings::Key key, const QVariant& value) {
        Q_UNUSED(value);
        if (key == Settings::Key::PrivacyLoadRemoteImages) {
            m_viewCache.clear();
            m_cacheLruList.clear();
        }
    });
}

MessageViewPanel::~MessageViewPanel() = default;

void MessageViewPanel::setupUi()
{
    setMinimumWidth(300);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // -- Message header area -------------------------------------------------
    m_headerArea = new QWidget(this);
    m_headerArea->setAutoFillBackground(true);
    m_headerArea->hide();

    auto* headerLayout = new QVBoxLayout(m_headerArea);
    headerLayout->setContentsMargins(16, 12, 16, 12);
    headerLayout->setSpacing(4);

    m_subjectLabel = new QLabel(m_headerArea);
    {
        QFont f = m_subjectLabel->font();
        f.setPointSizeF(f.pointSizeF() * 1.2);
        f.setBold(true);
        m_subjectLabel->setFont(f);
    }
    m_subjectLabel->setWordWrap(true);
    headerLayout->addWidget(m_subjectLabel);

    auto* subHeaderLayout = new QHBoxLayout();
    m_fromLabel = new QLabel(m_headerArea);
    m_dateLabel = new QLabel(m_headerArea);

    QPalette grayPalette = m_dateLabel->palette();
    grayPalette.setColor(QPalette::WindowText, QApplication::palette().color(QPalette::Disabled, QPalette::WindowText));
    m_dateLabel->setPalette(grayPalette);

    subHeaderLayout->addWidget(m_fromLabel);
    subHeaderLayout->addStretch();
    subHeaderLayout->addWidget(m_dateLabel);

    headerLayout->addLayout(subHeaderLayout);

    // Metadata labels (To, Cc, Bcc, Reply-To)
    m_toLabel = new QLabel(m_headerArea);
    m_toLabel->setPalette(grayPalette);
    m_toLabel->setWordWrap(true);
    headerLayout->addWidget(m_toLabel);

    m_ccLabel = new QLabel(m_headerArea);
    m_ccLabel->setPalette(grayPalette);
    m_ccLabel->setWordWrap(true);
    headerLayout->addWidget(m_ccLabel);

    m_bccLabel = new QLabel(m_headerArea);
    m_bccLabel->setPalette(grayPalette);
    m_bccLabel->setWordWrap(true);
    headerLayout->addWidget(m_bccLabel);

    m_replyToLabel = new QLabel(m_headerArea);
    m_replyToLabel->setPalette(grayPalette);
    m_replyToLabel->setWordWrap(true);
    headerLayout->addWidget(m_replyToLabel);

    // Attachment Indicator Label
    m_attachmentIndicatorLabel = new QLabel(m_headerArea);
    m_attachmentIndicatorLabel->setPalette(grayPalette);
    headerLayout->addWidget(m_attachmentIndicatorLabel);

    // Attachment Bar (for file saving)
    m_attachmentBar = new AttachmentBar(m_headerArea);
    headerLayout->addWidget(m_attachmentBar);

    layout->addWidget(m_headerArea);

    // -- Separator -----------------------------------------------------------
    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Plain);
    layout->addWidget(sep);

    // -- Empty state placeholder ---------------------------------------------
    m_placeholderMsg = new QLabel(tr("No message selected."), this);
    m_placeholderMsg->setAlignment(Qt::AlignCenter);
    m_placeholderMsg->setWordWrap(true);
    m_placeholderMsg->setContentsMargins(32, 0, 32, 0);
    m_placeholderMsg->setPalette(grayPalette);
    {
        QFont f = m_placeholderMsg->font();
        f.setPointSizeF(f.pointSizeF() * 0.92);
        m_placeholderMsg->setFont(f);
    }
    layout->addWidget(m_placeholderMsg, 1, Qt::AlignCenter);

    // Web view or Text Browser
#ifdef MAILINIUM_HAS_WEBENGINE
    m_webView = new QWebEngineView(this);
    m_webView->setPage(new ExternalLinkPage(m_webView));
    m_webView->hide();
    layout->addWidget(m_webView, 1);
#else
    m_textBrowser = new QTextBrowser(this);
    m_textBrowser->setFrameShape(QFrame::NoFrame);
    m_textBrowser->setOpenExternalLinks(true);
    m_textBrowser->hide();
    layout->addWidget(m_textBrowser, 1);
#endif
}

void MessageViewPanel::displayMessage(const Message& msg, const QString& rawBody)
{
    MimeMessage mimeMsg = MimeParser::parse(rawBody.toUtf8());
    displayMimeMessage(msg, mimeMsg.htmlBody, mimeMsg.plainBody, mimeMsg.attachments);
}

void MessageViewPanel::displayMimeMessage(const Message& msg, const QString& htmlBody, const QString& plainBody, const QList<AttachmentInfo>& attachments)
{
    // 1. Separate inline images from file attachments
    QList<AttachmentInfo> regularAttachments;
    QList<AttachmentInfo> inlineImages;
    regularAttachments.reserve(attachments.size());
    inlineImages.reserve(attachments.size());

    for (const AttachmentInfo& att : attachments) {
        if (att.isInline || (!att.contentId.isEmpty() && att.contentType.startsWith(QStringLiteral("image/")))) {
            inlineImages.append(att);
        } else {
            regularAttachments.append(att);
        }
    }

    // 2. Sanitize HTML
    QString content = htmlBody;
    if (content.isEmpty()) {
        QString escaped = plainBody.toHtmlEscaped();
        escaped.replace('\n', QStringLiteral("<br>"));
        content = QStringLiteral("<html><body><div>%1</div></body></html>").arg(escaped);
    }

    QString sanitizedHtml = HtmlSanitizer::sanitize(content, inlineImages);

    // 3. Cache the processed view data
    cacheMessage(msg.id, msg, sanitizedHtml, regularAttachments);

    // 4. Update UI
    m_placeholderMsg->hide();
    m_headerArea->show();

    m_subjectLabel->setText(msg.subject.isEmpty() ? tr("(No Subject)") : msg.subject);
    m_fromLabel->setText(msg.fromName.isEmpty() ? msg.fromAddress : msg.fromName + " <" + msg.fromAddress + ">");
    m_dateLabel->setText(msg.date.toString("yyyy-MM-dd hh:mm"));

    updateMetadataLabels(msg);

    // Populate Attachment Bar with only regular file attachments
    m_attachmentBar->setAttachments(regularAttachments);

    // Update attachment indicator label in the header
    if (!regularAttachments.isEmpty()) {
        m_attachmentIndicatorLabel->setText(tr("📎 <b>%1 Attachment(s)</b>").arg(regularAttachments.size()));
        m_attachmentIndicatorLabel->show();
    } else {
        m_attachmentIndicatorLabel->hide();
    }

#ifdef MAILINIUM_HAS_WEBENGINE
    m_webView->show();
    m_webView->setHtml(sanitizedHtml);
#else
    m_textBrowser->show();
    m_textBrowser->setHtml(sanitizedHtml);
#endif
}

void MessageViewPanel::updateMetadataLabels(const Message& msg)
{
    if (!msg.toList.isEmpty()) {
        m_toLabel->setText(tr("<b>To:</b> ") + msg.toList.toHtmlEscaped());
        m_toLabel->show();
    } else {
        m_toLabel->hide();
    }

    if (!msg.ccList.isEmpty()) {
        m_ccLabel->setText(tr("<b>Cc:</b> ") + msg.ccList.toHtmlEscaped());
        m_ccLabel->show();
    } else {
        m_ccLabel->hide();
    }

    if (!msg.bccList.isEmpty()) {
        m_bccLabel->setText(tr("<b>Bcc:</b> ") + msg.bccList.toHtmlEscaped());
        m_bccLabel->show();
    } else {
        m_bccLabel->hide();
    }

    if (!msg.replyTo.isEmpty()) {
        m_replyToLabel->setText(tr("<b>Reply-To:</b> ") + msg.replyTo.toHtmlEscaped());
        m_replyToLabel->show();
    } else {
        m_replyToLabel->hide();
    }
}

void MessageViewPanel::cacheMessage(qint64 messageId, const Message& msg, const QString& sanitizedHtml, const QList<AttachmentInfo>& regularAttachments)
{
    if (messageId == -1) return;

    // Remove if already exists to refresh position in LRU list
    if (m_viewCache.contains(messageId)) {
        m_cacheLruList.removeOne(messageId);
    }

    CachedMessageData cacheData;
    cacheData.messageId = messageId;
    cacheData.msg = msg;
    cacheData.sanitizedHtml = sanitizedHtml;
    cacheData.regularAttachments = regularAttachments;

    m_viewCache.insert(messageId, std::move(cacheData));
    m_cacheLruList.append(messageId);

    // Enforce maximum cache size (LRU ejection)
    if (m_cacheLruList.size() > MAX_CACHE_SIZE) {
        qint64 oldestId = m_cacheLruList.takeFirst();
        m_viewCache.remove(oldestId);
    }
}

bool MessageViewPanel::loadFromCache(qint64 messageId, const Message& latestMsg)
{
    if (messageId == -1 || !m_viewCache.contains(messageId)) {
        return false;
    }

    // Refresh LRU order
    m_cacheLruList.removeOne(messageId);
    m_cacheLruList.append(messageId);

    // Ensure metadata is fresh
    CachedMessageData& cacheData = m_viewCache[messageId];
    cacheData.msg = latestMsg;

    // Update UI directly from cached structures
    m_placeholderMsg->hide();
    m_headerArea->show();

    m_subjectLabel->setText(cacheData.msg.subject.isEmpty() ? tr("(No Subject)") : cacheData.msg.subject);
    m_fromLabel->setText(cacheData.msg.fromName.isEmpty() ? cacheData.msg.fromAddress : cacheData.msg.fromName + " <" + cacheData.msg.fromAddress + ">");
    m_dateLabel->setText(cacheData.msg.date.toString("yyyy-MM-dd hh:mm"));

    updateMetadataLabels(cacheData.msg);

    m_attachmentBar->setAttachments(cacheData.regularAttachments);

    if (!cacheData.regularAttachments.isEmpty()) {
        m_attachmentIndicatorLabel->setText(tr("📎 <b>%1 Attachment(s)</b>").arg(cacheData.regularAttachments.size()));
        m_attachmentIndicatorLabel->show();
    } else {
        m_attachmentIndicatorLabel->hide();
    }

#ifdef MAILINIUM_HAS_WEBENGINE
    m_webView->show();
    m_webView->setHtml(cacheData.sanitizedHtml);
#else
    m_textBrowser->show();
    m_textBrowser->setHtml(cacheData.sanitizedHtml);
#endif

    return true;
}

#include "MessageViewPanel.moc"
