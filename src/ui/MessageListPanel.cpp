#include "MessageListPanel.h"

#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>
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
    // Will hold: folder name, message count, sort button, filter button.
    // Phase 3: shows a static placeholder label.
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
    m_messageList->hide(); // hidden initially
    layout->addWidget(m_messageList, 1); // stretch
    
    connect(m_messageList, &QListWidget::itemSelectionChanged, this, &MessageListPanel::onSelectionChanged);
}

void MessageListPanel::loadMessages(qint64 folderId)
{
    m_currentFolderId = folderId;
    
    FolderStore fstore;
    auto optFolder = fstore.folderById(folderId);
    if (optFolder.has_value()) {
        m_folderLabel->setText(optFolder->name);
    }
    
    m_messageList->clear();
    
    MessageStore store;
    QList<Message> messages = store.messagesForFolder(folderId, 100); // load up to 100
    
    Logger::instance().debug(QStringLiteral("imap_pipeline"), QStringLiteral("[PIPELINE] 8. Number loaded from MessageStore: ") + QString::number(messages.size()));
    Logger::instance().debug(QStringLiteral("imap_pipeline"), QStringLiteral("[PIPELINE] 9. Number passed to MessageListPanel: ") + QString::number(messages.size()));
    if (messages.isEmpty()) {
        Logger::instance().error(QStringLiteral("imap_pipeline"), QStringLiteral("[PIPELINE] STOP: 0 messages reached the UI!"));
    }
    
    if (messages.isEmpty()) {
        m_messageList->hide();
        m_placeholderMsg->setText(tr("No messages in this folder."));
        m_placeholderMsg->show();
    } else {
        m_placeholderMsg->hide();
        m_messageList->show();
        
        for (const Message& msg : messages) {
            // For Phase 3C, we just use a simple string.
            // In Phase 3E this would use a QStyledItemDelegate
            QString displayText = msg.fromName.isEmpty() ? msg.fromAddress : msg.fromName;
            displayText += "\n" + msg.subject;
            displayText += "\n" + msg.date.toString("yyyy-MM-dd hh:mm");
            
            auto* item = new QListWidgetItem(displayText, m_messageList);
            item->setData(Qt::UserRole, msg.id);
            // Add some padding
            item->setSizeHint(QSize(m_messageList->width(), 60));
        }
        Logger::instance().debug(QStringLiteral("ui"), QStringLiteral("MessageListPanel populated ") + QString::number(messages.size()) + " items in the list widget.");
    }
}

void MessageListPanel::onSelectionChanged()
{
    auto items = m_messageList->selectedItems();
    if (!items.isEmpty()) {
        qint64 messageId = items.first()->data(Qt::UserRole).toLongLong();
        emit messageSelected(messageId);
    }
}
