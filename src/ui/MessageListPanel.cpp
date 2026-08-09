#include "MessageListPanel.h"

#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include "../storage/Message.h"
#include "../storage/MessageStore.h"
#include "../storage/FolderStore.h"
#include "../core/Logger.h"

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

MessageListPanel::MessageListPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

MessageListPanel::~MessageListPanel() = default;

// ---------------------------------------------------------------------------
// UI setup
// ---------------------------------------------------------------------------

void MessageListPanel::setupUi()
{
    setMinimumWidth(240);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // -- Header bar ----------------------------------------------------------
    m_headerBar = new QWidget(this);
    m_headerBar->setFixedHeight(40);
    m_headerBar->setAutoFillBackground(true);

    auto* headerLayout = new QHBoxLayout(m_headerBar);
    headerLayout->setContentsMargins(12, 0, 8, 0);

    m_folderLabel = new QLabel(tr("Mailinium"), m_headerBar);
    {
        QFont f = m_folderLabel->font();
        f.setBold(true);
        m_folderLabel->setFont(f);
    }
    headerLayout->addWidget(m_folderLabel);
    headerLayout->addStretch();

    layout->addWidget(m_headerBar);

    // -- Search bar ----------------------------------------------------------
    m_searchBox = new QLineEdit(this);
    m_searchBox->setPlaceholderText(tr("Search messages\u2026"));
    m_searchBox->setClearButtonEnabled(true);
    m_searchBox->setContentsMargins(6, 4, 6, 4);
    {
        QFont f = m_searchBox->font();
        f.setPointSizeF(f.pointSizeF() * 0.95);
        m_searchBox->setFont(f);
    }
    layout->addWidget(m_searchBox);

    connect(m_searchBox, &QLineEdit::textChanged,
            this, &MessageListPanel::onSearchTextChanged);

    // -- Separator -----------------------------------------------------------
    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Plain);
    layout->addWidget(sep);

    // -- Empty state placeholder ---------------------------------------------
    m_placeholderMsg = new QLabel(tr("Select a folder to view messages."), this);
    m_placeholderMsg->setAlignment(Qt::AlignCenter);
    m_placeholderMsg->setWordWrap(true);
    m_placeholderMsg->setContentsMargins(24, 0, 24, 0);
    {
        QPalette p = m_placeholderMsg->palette();
        p.setColor(QPalette::WindowText,
                   QApplication::palette().color(QPalette::Disabled, QPalette::WindowText));
        m_placeholderMsg->setPalette(p);
        QFont f = m_placeholderMsg->font();
        f.setPointSizeF(f.pointSizeF() * 0.92);
        m_placeholderMsg->setFont(f);
    }
    layout->addWidget(m_placeholderMsg, 0, Qt::AlignCenter);
    
    // -- Message list --------------------------------------------------------
    m_messageList = new QListWidget(this);
    m_messageList->setFrameShape(QFrame::NoFrame);
    m_messageList->setSelectionMode(QAbstractItemView::ExtendedSelection); // Phase 4C: batch
    m_messageList->hide(); // hidden initially
    layout->addWidget(m_messageList, 1); // stretch
    
    connect(m_messageList, &QListWidget::itemSelectionChanged,
            this, &MessageListPanel::onSelectionChanged);
}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

void MessageListPanel::loadMessages(qint64 folderId)
{
    m_currentFolderId = folderId;
    
    // Clear search box when changing folder
    m_searchBox->blockSignals(true);
    m_searchBox->clear();
    m_searchBox->blockSignals(false);

    FolderStore fstore;
    auto optFolder = fstore.folderById(folderId);
    if (optFolder.has_value()) {
        m_folderLabel->setText(optFolder->name);
    }
    
    MessageStore store;
    QList<Message> messages = store.messagesForFolder(folderId, 100);
    
    Logger::instance().debug(QStringLiteral("imap_pipeline"),
        QStringLiteral("[PIPELINE] 8. Number loaded from MessageStore: ") + QString::number(messages.size()));
    Logger::instance().debug(QStringLiteral("imap_pipeline"),
        QStringLiteral("[PIPELINE] 9. Number passed to MessageListPanel: ") + QString::number(messages.size()));
    if (messages.isEmpty()) {
        Logger::instance().error(QStringLiteral("imap_pipeline"),
            QStringLiteral("[PIPELINE] STOP: 0 messages reached the UI!"));
    }
    
    populateList(messages);
}

QList<qint64> MessageListPanel::selectedMessageIds() const
{
    QList<qint64> ids;
    for (const QListWidgetItem* item : m_messageList->selectedItems()) {
        ids.append(item->data(Qt::UserRole).toLongLong());
    }
    return ids;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void MessageListPanel::populateList(const QList<Message>& messages)
{
    m_messageList->clear();

    if (messages.isEmpty()) {
        m_messageList->hide();
        m_placeholderMsg->setText(
            m_searchBox->text().isEmpty()
            ? tr("No messages in this folder.")
            : tr("No messages match your search."));
        m_placeholderMsg->show();
        return;
    }

    m_placeholderMsg->hide();
    m_messageList->show();

    for (const Message& msg : messages) {
        QString displayText = msg.fromName.isEmpty() ? msg.fromAddress : msg.fromName;
        displayText += "\n" + msg.subject;
        displayText += "\n" + msg.date.toString("yyyy-MM-dd hh:mm");

        auto* item = new QListWidgetItem(displayText, m_messageList);
        item->setData(Qt::UserRole, msg.id);
        item->setSizeHint(QSize(m_messageList->width(), 60));

        // Bold for unread
        if ((msg.flags & MessageFlags::Seen) == 0) {
            QFont f = item->font();
            f.setBold(true);
            item->setFont(f);
        }
    }
    Logger::instance().debug(QStringLiteral("ui"),
        QStringLiteral("MessageListPanel populated ") + QString::number(messages.size()) + " items.");
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void MessageListPanel::onSelectionChanged()
{
    QList<qint64> ids = selectedMessageIds();
    
    // Emit batch signal first
    emit selectionChanged(ids);
    
    // Then emit single-select signal for backwards compatibility
    if (ids.size() == 1) {
        emit messageSelected(ids.first());
    }
}

void MessageListPanel::onSearchTextChanged(const QString& text)
{
    if (m_currentFolderId == -1) return;

    if (text.trimmed().isEmpty()) {
        // Reload full folder
        MessageStore store;
        populateList(store.messagesForFolder(m_currentFolderId, 100));
        return;
    }

    // Search within current folder using FTS5
    MessageStore store;
    QList<Message> results = store.searchMessages(text.trimmed(), -1, m_currentFolderId);
    populateList(results);
}
