#include "MessageViewPanel.h"

#include <QApplication>
#include <QFrame>
#include <QLabel>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "../storage/Message.h"

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

MessageViewPanel::MessageViewPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

MessageViewPanel::~MessageViewPanel() = default;

// ---------------------------------------------------------------------------
// UI setup
// ---------------------------------------------------------------------------

void MessageViewPanel::setupUi()
{
    setMinimumWidth(300);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // -- Message header area -------------------------------------------------
    // Will hold: Subject, From, Date, To, action buttons (Reply, Forward, etc.)
    // Phase 3: empty header bar — establishes the layout slot.
    m_headerArea = new QWidget(this);
    m_headerArea->setFixedHeight(80);
    m_headerArea->setAutoFillBackground(true);
    m_headerArea->hide(); // hidden initially

    auto* headerLayout = new QVBoxLayout(m_headerArea);
    headerLayout->setContentsMargins(16, 12, 16, 12);
    
    m_subjectLabel = new QLabel(m_headerArea);
    {
        QFont f = m_subjectLabel->font();
        f.setPointSizeF(f.pointSizeF() * 1.2);
        f.setBold(true);
        m_subjectLabel->setFont(f);
    }
    headerLayout->addWidget(m_subjectLabel);
    
    auto* subHeaderLayout = new QHBoxLayout();
    m_fromLabel = new QLabel(m_headerArea);
    m_dateLabel = new QLabel(m_headerArea);
    
    {
        QPalette p = m_dateLabel->palette();
        p.setColor(QPalette::WindowText, QApplication::palette().color(QPalette::Disabled, QPalette::WindowText));
        m_dateLabel->setPalette(p);
    }
    
    subHeaderLayout->addWidget(m_fromLabel);
    subHeaderLayout->addStretch();
    subHeaderLayout->addWidget(m_dateLabel);
    
    headerLayout->addLayout(subHeaderLayout);

    layout->addWidget(m_headerArea);

    // -- Separator -----------------------------------------------------------
    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Plain);
    layout->addWidget(sep);

    // -- Empty state placeholder ---------------------------------------------
    // Replaced by QWebEngineView (sandboxed HTML renderer) in Phase 3E.
    m_placeholderMsg = new QLabel(tr("No message selected."), this);
    m_placeholderMsg->setAlignment(Qt::AlignCenter);
    m_placeholderMsg->setWordWrap(true);
    m_placeholderMsg->setContentsMargins(32, 0, 32, 0);
    {
        QPalette p = m_placeholderMsg->palette();
        p.setColor(QPalette::WindowText,
                   QApplication::palette().color(QPalette::Disabled, QPalette::WindowText));
        m_placeholderMsg->setPalette(p);
        QFont f = m_placeholderMsg->font();
        f.setPointSizeF(f.pointSizeF() * 0.92);
        m_placeholderMsg->setFont(f);
    }
    layout->addWidget(m_placeholderMsg, 1, Qt::AlignCenter);
    
    m_textBrowser = new QTextBrowser(this);
    m_textBrowser->setFrameShape(QFrame::NoFrame);
    m_textBrowser->setOpenExternalLinks(true);
    m_textBrowser->hide();
    
    layout->addWidget(m_textBrowser, 1);
}

void MessageViewPanel::displayMessage(const Message& msg, const QString& body)
{
    m_placeholderMsg->hide();
    m_headerArea->show();
    m_textBrowser->show();
    
    m_subjectLabel->setText(msg.subject);
    m_fromLabel->setText(msg.fromName.isEmpty() ? msg.fromAddress : msg.fromName + " <" + msg.fromAddress + ">");
    m_dateLabel->setText(msg.date.toString("yyyy-MM-dd hh:mm"));
    
    m_textBrowser->setPlainText(body);
}
