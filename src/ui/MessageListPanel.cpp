#include "MessageListPanel.h"

#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

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
    // Replaced by QListView + MessageListModel in Phase 3E.
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
    layout->addWidget(m_placeholderMsg, 1, Qt::AlignCenter);
}
