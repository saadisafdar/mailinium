#include "ui/ComposeWindow.h"
#include "core/SmtpSendJob.h"
#include "storage/DraftStore.h"
#include "core/Logger.h"

#include <QAction>
#include <QCloseEvent>
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QShortcut>
#include <QTextEdit>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

ComposeWindow::ComposeWindow(const Account& account, QWidget* parent, Mode mode)
    : QDialog(parent)
    , m_account(account)
    , m_mode(mode)
{
    setWindowTitle(tr("Compose"));
    setMinimumSize(680, 520);
    setSizeGripEnabled(true);
    setWindowModality(Qt::NonModal);   // non-blocking — user can still read mail

    setupUi();

    // Auto-save timer — 30 seconds after last edit
    m_autoSaveTimer = new QTimer(this);
    m_autoSaveTimer->setInterval(30000);
    m_autoSaveTimer->setSingleShot(true);
    connect(m_autoSaveTimer, &QTimer::timeout, this, &ComposeWindow::onAutoSave);

    // Keyboard shortcuts
    auto* shortcutSend = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return), this);
    connect(shortcutSend, &QShortcut::activated, this, &ComposeWindow::onSend);

    auto* shortcutSave = new QShortcut(QKeySequence::Save, this);
    connect(shortcutSave, &QShortcut::activated, this, &ComposeWindow::onSaveDraft);

    auto* shortcutClose = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_W), this);
    connect(shortcutClose, &QShortcut::activated, this, &QDialog::reject);
}

ComposeWindow::~ComposeWindow()
{
    // Auto-save if there's unsaved content
    if (m_draftDirty && !m_sending) {
        saveDraftNow();
    }
}

// ---------------------------------------------------------------------------
// UI Setup
// ---------------------------------------------------------------------------

void ComposeWindow::setupUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    setupToolBar();

    auto* formContainer = new QWidget(this);
    auto* formLayout    = new QFormLayout(formContainer);
    formLayout->setContentsMargins(12, 8, 12, 8);
    formLayout->setVerticalSpacing(6);

    // From (read-only)
    m_fromLabel = new QLabel(m_account.email.isEmpty()
                              ? m_account.displayName
                              : m_account.email, this);
    m_fromLabel->setObjectName(QStringLiteral("composeFromLabel"));
    formLayout->addRow(tr("From:"), m_fromLabel);

    // To
    m_toEdit = new QLineEdit(this);
    m_toEdit->setObjectName(QStringLiteral("composeToEdit"));
    m_toEdit->setPlaceholderText(tr("Recipient(s), comma-separated"));
    formLayout->addRow(tr("To:"), m_toEdit);
    connect(m_toEdit, &QLineEdit::textChanged, this, &ComposeWindow::onBodyChanged);

    // Cc + BCC toggle
    m_ccEdit = new QLineEdit(this);
    m_ccEdit->setObjectName(QStringLiteral("composeCcEdit"));
    m_ccEdit->setPlaceholderText(tr("Cc recipients, comma-separated"));
    connect(m_ccEdit, &QLineEdit::textChanged, this, &ComposeWindow::onBodyChanged);

    m_bccToggle = new QPushButton(tr("Bcc"), this);
    m_bccToggle->setObjectName(QStringLiteral("composeBccToggle"));
    m_bccToggle->setFlat(true);
    m_bccToggle->setCheckable(true);
    connect(m_bccToggle, &QPushButton::clicked, this, &ComposeWindow::onToggleBcc);

    auto* ccRow = new QHBoxLayout;
    ccRow->addWidget(m_ccEdit);
    ccRow->addWidget(m_bccToggle);
    formLayout->addRow(tr("Cc:"), ccRow);

    m_bccEdit = new QLineEdit(this);
    m_bccEdit->setObjectName(QStringLiteral("composeBccEdit"));
    m_bccEdit->setPlaceholderText(tr("Bcc recipients, comma-separated"));
    m_bccEdit->hide();
    connect(m_bccEdit, &QLineEdit::textChanged, this, &ComposeWindow::onBodyChanged);
    formLayout->addRow(tr("Bcc:"), m_bccEdit);

    // Subject
    m_subjectEdit = new QLineEdit(this);
    m_subjectEdit->setObjectName(QStringLiteral("composeSubjectEdit"));
    formLayout->addRow(tr("Subject:"), m_subjectEdit);
    connect(m_subjectEdit, &QLineEdit::textChanged, this, &ComposeWindow::onBodyChanged);

    root->addWidget(formContainer);

    // Separator
    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    root->addWidget(sep);

    // Body editor
    m_bodyEdit = new QTextEdit(this);
    m_bodyEdit->setObjectName(QStringLiteral("composeBodyEdit"));
    m_bodyEdit->setAcceptRichText(true);
    m_bodyEdit->setMinimumHeight(200);
    connect(m_bodyEdit, &QTextEdit::textChanged, this, &ComposeWindow::onBodyChanged);
    root->addWidget(m_bodyEdit, 1);

    // Attachment list (hidden until attachments are added)
    m_attachmentList = new QListWidget(this);
    m_attachmentList->setObjectName(QStringLiteral("composeAttachmentList"));
    m_attachmentList->setMaximumHeight(100);
    m_attachmentList->hide();
    root->addWidget(m_attachmentList);

    // Progress bar (hidden until sending)
    m_progressBar = new QProgressBar(this);
    m_progressBar->setObjectName(QStringLiteral("composeSendProgress"));
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->hide();
    root->addWidget(m_progressBar);

    // Status label
    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("composeStatusLabel"));
    m_statusLabel->setContentsMargins(8, 4, 8, 4);
    m_statusLabel->hide();
    root->addWidget(m_statusLabel);
}

void ComposeWindow::setupToolBar()
{
    auto* toolbar = new QToolBar(tr("Compose Toolbar"), this);
    toolbar->setObjectName(QStringLiteral("composeToolbar"));
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);

    auto* actSend   = toolbar->addAction(tr("Send"));
    auto* actDraft  = toolbar->addAction(tr("Save Draft"));
    toolbar->addSeparator();
    auto* actAttach = toolbar->addAction(tr("Attach File"));
    auto* actRemove = toolbar->addAction(tr("Remove Attachment"));
    toolbar->addSeparator();
    auto* actDiscard = toolbar->addAction(tr("Discard"));

    connect(actSend,    &QAction::triggered, this, &ComposeWindow::onSend);
    connect(actDraft,   &QAction::triggered, this, &ComposeWindow::onSaveDraft);
    connect(actAttach,  &QAction::triggered, this, &ComposeWindow::onAttachFile);
    connect(actRemove,  &QAction::triggered, this, &ComposeWindow::onRemoveAttachment);
    connect(actDiscard, &QAction::triggered, this, &QDialog::reject);

    // Add toolbar to the top of root layout via insertWidget after layout exists
    static_cast<QVBoxLayout*>(layout())->insertWidget(0, toolbar);
}

// ---------------------------------------------------------------------------
// Static factory methods
// ---------------------------------------------------------------------------

// static
QString ComposeWindow::buildQuoteBlock(const QString& body, const Message& original)
{
    const QString dateStr = original.date.toString(QStringLiteral("ddd, MMM d, yyyy 'at' h:mm AP"));
    const QString who = original.fromName.isEmpty()
        ? original.fromAddress
        : original.fromName + QStringLiteral(" <") + original.fromAddress + QStringLiteral(">");

    const QString attribution = QStringLiteral("On ") + dateStr
                              + QStringLiteral(", ") + who
                              + QStringLiteral(" wrote:");

    // Quote each line with "> "
    QStringList lines = body.split('\n');
    for (QString& line : lines) {
        line.prepend(QStringLiteral("> "));
    }

    return QStringLiteral("\n\n") + attribution + QStringLiteral("\n") + lines.join('\n');
}

// static
ComposeWindow* ComposeWindow::createReply(const Account& account,
                                           const Message& original,
                                           const QString& quotedBody,
                                           QWidget* parent)
{
    auto* w = new ComposeWindow(account, parent, Mode::Reply);

    // Determine reply-to address
    const QString replyAddr = original.replyTo.isEmpty()
        ? original.fromAddress
        : original.replyTo;

    w->m_toEdit->setText(replyAddr);

    const QString subj = original.subject.trimmed();
    w->m_subjectEdit->setText(subj.startsWith(QStringLiteral("Re:"), Qt::CaseInsensitive)
                              ? subj
                              : QStringLiteral("Re: ") + subj);

    w->m_inReplyTo = original.messageId;
    w->m_references = original.messageId;

    const QString quote = buildQuoteBlock(quotedBody, original);
    w->m_bodyEdit->setPlainText(quote);
    // Move cursor to top so user types above the quote
    auto cursor = w->m_bodyEdit->textCursor();
    cursor.movePosition(QTextCursor::Start);
    w->m_bodyEdit->setTextCursor(cursor);

    return w;
}

// static
ComposeWindow* ComposeWindow::createReplyAll(const Account& account,
                                              const Message& original,
                                              const QString& quotedBody,
                                              QWidget* parent)
{
    auto* w = createReply(account, original, quotedBody, parent);
    w->m_mode = Mode::ReplyAll;

    // Add Cc (original To + Cc, minus self)
    QStringList ccList;
    auto addFiltered = [&](const QString& csv) {
        const QStringList parts = csv.split(',', Qt::SkipEmptyParts);
        for (const QString& p : parts) {
            const QString addr = p.trimmed();
            if (!addr.contains(account.email, Qt::CaseInsensitive)) {
                ccList.append(addr);
            }
        }
    };
    addFiltered(original.toList);
    addFiltered(original.ccList);

    w->m_ccEdit->setText(ccList.join(QStringLiteral(", ")));
    return w;
}

// static
ComposeWindow* ComposeWindow::createForward(const Account& account,
                                             const Message& original,
                                             const QString& quotedBody,
                                             QWidget* parent)
{
    auto* w = new ComposeWindow(account, parent, Mode::Forward);

    const QString subj = original.subject.trimmed();
    w->m_subjectEdit->setText(subj.startsWith(QStringLiteral("Fwd:"), Qt::CaseInsensitive)
                              ? subj
                              : QStringLiteral("Fwd: ") + subj);

    const QString quote = buildQuoteBlock(quotedBody, original);
    w->m_bodyEdit->setPlainText(quote);

    auto cursor = w->m_bodyEdit->textCursor();
    cursor.movePosition(QTextCursor::Start);
    w->m_bodyEdit->setTextCursor(cursor);

    return w;
}

// ---------------------------------------------------------------------------
// Draft support
// ---------------------------------------------------------------------------

void ComposeWindow::loadDraft(const Draft& draft)
{
    m_draft = draft;
    m_toEdit->setText(draft.toList);
    m_ccEdit->setText(draft.ccList);
    m_subjectEdit->setText(draft.subject);
    m_inReplyTo  = draft.inReplyTo;
    m_references = draft.references;

    if (!draft.bccList.isEmpty()) {
        m_bccEdit->setText(draft.bccList);
        m_bccEdit->show();
        m_bccToggle->setChecked(true);
    }

    if (!draft.bodyHtml.isEmpty()) {
        m_bodyEdit->setHtml(draft.bodyHtml);
    } else {
        m_bodyEdit->setPlainText(draft.bodyPlain);
    }

    m_attachments.clear();
    m_attachmentList->clear();
    for (const QString& path : draft.attachmentPaths) {
        if (QFile::exists(path)) {
            ComposeAttachment att;
            att.filePath = path;
            att.filename = QFileInfo(path).fileName();
            att.mimeType = MimeBuilder::detectMimeType(path);
            m_attachments.append(att);
            m_attachmentList->addItem(att.filename + QStringLiteral(" (") + att.mimeType + QStringLiteral(")"));
        }
    }
    if (!m_attachments.isEmpty()) {
        m_attachmentList->show();
    } else {
        m_attachmentList->hide();
    }

    m_draftDirty = false;
}

bool ComposeWindow::saveDraftNow()
{
    m_draft.accountId  = m_account.id;
    m_draft.toList     = m_toEdit->text().trimmed();
    m_draft.ccList     = m_ccEdit->text().trimmed();
    m_draft.bccList    = m_bccEdit->text().trimmed();
    m_draft.subject    = m_subjectEdit->text().trimmed();
    m_draft.bodyHtml   = m_bodyEdit->toHtml();
    m_draft.bodyPlain  = m_bodyEdit->toPlainText();
    m_draft.inReplyTo  = m_inReplyTo;
    m_draft.references = m_references;
    
    QStringList paths;
    for (const auto& att : m_attachments) {
        paths.append(att.filePath);
    }
    m_draft.attachmentPaths = paths;

    m_draft.updatedAt  = QDateTime::currentDateTimeUtc();

    DraftStore store;
    bool ok = false;
    if (m_draft.id == -1) {
        const qint64 id = store.insertDraft(m_draft);
        ok = (id > 0);
        if (ok) {
            Logger::instance().info(QStringLiteral("compose"),
                QStringLiteral("Draft created with id: ") + QString::number(m_draft.id));
        } else {
            Logger::instance().error(QStringLiteral("compose"),
                QStringLiteral("Draft insert failed. draft.id remains: ") + QString::number(m_draft.id));
        }
    } else {
        ok = store.updateDraft(m_draft);
        if (ok) {
            Logger::instance().debug(QStringLiteral("compose"),
                QStringLiteral("Draft updated: ") + QString::number(m_draft.id));
        } else {
            Logger::instance().error(QStringLiteral("compose"),
                QStringLiteral("Draft update failed for id: ") + QString::number(m_draft.id));
        }
    }

    if (ok) {
        m_draftDirty = false;
    }
    // If !ok, m_draftDirty stays true so auto-save or destructor can retry.
    return ok;
}

// ---------------------------------------------------------------------------
// Build ComposeData for sending
// ---------------------------------------------------------------------------

ComposeData ComposeWindow::buildComposeData() const
{
    ComposeData cd;
    cd.from      = m_account.email;
    cd.to        = m_toEdit->text().trimmed();
    cd.cc        = m_ccEdit->text().trimmed();
    cd.bcc       = m_bccEdit->text().trimmed();
    cd.subject   = m_subjectEdit->text().trimmed();
    cd.htmlBody  = m_bodyEdit->toHtml();
    cd.plainBody = m_bodyEdit->toPlainText();
    cd.inReplyTo = m_inReplyTo;
    cd.references = m_references;
    cd.attachments = m_attachments;
    return cd;
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void ComposeWindow::onBodyChanged()
{
    m_draftDirty = true;
    m_autoSaveTimer->start(); // restarts 30-second timer on each edit
}

void ComposeWindow::closeEvent(QCloseEvent* event)
{
    if (m_sending) {
        event->ignore();
        return;
    }

    const bool hasContent = !m_toEdit->text().trimmed().isEmpty() ||
                            !m_subjectEdit->text().trimmed().isEmpty() ||
                            !m_bodyEdit->toPlainText().trimmed().isEmpty() ||
                            !m_attachments.isEmpty();

    if (m_draftDirty && hasContent) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            tr("Save Draft?"),
            tr("Do you want to save this message as a draft before closing?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save
        );

        if (reply == QMessageBox::Save) {
            saveDraftNow();
            event->accept();
        } else if (reply == QMessageBox::Discard) {
            if (m_draft.id != -1) {
                DraftStore store;
                store.deleteDraft(m_draft.id);
            }
            event->accept();
        } else {
            event->ignore();
        }
    } else {
        event->accept();
    }
}

void ComposeWindow::onAutoSave()
{
    if (m_draftDirty && !m_sending) {
        if (saveDraftNow()) {
            m_statusLabel->setText(tr("Draft auto-saved at %1").arg(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss"))));
            m_statusLabel->show();
            QTimer::singleShot(4000, m_statusLabel, &QLabel::hide);
        }
    }
}

void ComposeWindow::onSaveDraft()
{
    if (saveDraftNow()) {
        m_statusLabel->setText(tr("Draft saved."));
        m_statusLabel->show();
        QTimer::singleShot(3000, m_statusLabel, &QLabel::hide);
    } else {
        m_statusLabel->setText(tr("Failed to save draft."));
        m_statusLabel->show();
        QMessageBox::warning(this, tr("Draft Error"),
            tr("Could not save draft to database. Check the application log for details."));
    }
}

void ComposeWindow::onSend()
{
    if (m_sending) return;

    const QString to      = m_toEdit->text().trimmed();
    const QString subject = m_subjectEdit->text().trimmed();

    if (to.isEmpty()) {
        QMessageBox::warning(this, tr("Cannot Send"), tr("Please enter at least one recipient."));
        return;
    }

    m_sending = true;
    m_progressBar->setValue(0);
    m_progressBar->show();
    m_statusLabel->setText(tr("Sending…"));
    m_statusLabel->show();

    ComposeData cd = buildComposeData();

    auto* job = new SmtpSendJob(m_account, cd, this);
    connect(job, &SmtpSendJob::progress,   this, &ComposeWindow::onSendProgress);
    connect(job, &SmtpSendJob::succeeded,  this, &ComposeWindow::onSendSucceeded);
    connect(job, &SmtpSendJob::failed,     this, &ComposeWindow::onSendFailed);
    job->start();
}

void ComposeWindow::onSendProgress(int percent)
{
    m_progressBar->setValue(percent);
}

void ComposeWindow::onSendSucceeded()
{
    m_sending = false;
    m_progressBar->hide();

    // Delete saved draft if it exists
    if (m_draft.id != -1) {
        DraftStore store;
        store.deleteDraft(m_draft.id);
    }

    Logger::instance().info(QStringLiteral("compose"), QStringLiteral("Message sent successfully"));
    QMessageBox::information(this, tr("Sent"), tr("Message sent successfully."));
    accept();
}

void ComposeWindow::onSendFailed(const QString& error)
{
    m_sending = false;
    m_progressBar->hide();
    m_statusLabel->setText(tr("Send failed: ") + error);
    m_statusLabel->show();

    QMessageBox::critical(this, tr("Send Failed"), error);
}

void ComposeWindow::onAttachFile()
{
    const QStringList files = QFileDialog::getOpenFileNames(
        this, tr("Attach Files"), QString(), tr("All Files (*)"));

    for (const QString& file : files) {
        ComposeAttachment att;
        att.filePath = file;
        QFileInfo fi(file);
        att.filename = fi.fileName();
        att.mimeType = MimeBuilder::detectMimeType(file);
        m_attachments.append(att);

        qint64 size = fi.size();
        QString sizeStr;
        if (size < 1024) sizeStr = QStringLiteral("%1 B").arg(size);
        else if (size < 1024 * 1024) sizeStr = QStringLiteral("%1 KB").arg(size / 1024);
        else sizeStr = QStringLiteral("%1 MB").arg(QString::number(size / (1024.0 * 1024.0), 'f', 1));

        m_attachmentList->addItem(QStringLiteral("%1 (%2)").arg(att.filename, sizeStr));
        onBodyChanged();
    }

    if (!m_attachments.isEmpty()) {
        m_attachmentList->show();
    }
}

void ComposeWindow::onRemoveAttachment()
{
    const int row = m_attachmentList->currentRow();
    if (row < 0 || row >= m_attachments.size()) return;

    m_attachments.removeAt(row);
    delete m_attachmentList->takeItem(row);
    onBodyChanged();

    if (m_attachments.isEmpty()) {
        m_attachmentList->hide();
    }
}

void ComposeWindow::onToggleBcc()
{
    const bool show = m_bccToggle->isChecked();
    m_bccEdit->setVisible(show);
}
