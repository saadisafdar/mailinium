#include "MessageViewPanel.h"

#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>

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

    // Header area will be populated in Phase 3E.
    // For now it is intentionally empty — it represents the reserved space.
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
                   qApp->palette().color(QPalette::Disabled, QPalette::WindowText));
        m_placeholderMsg->setPalette(p);
        QFont f = m_placeholderMsg->font();
        f.setPointSizeF(f.pointSizeF() * 0.92);
        m_placeholderMsg->setFont(f);
    }
    layout->addWidget(m_placeholderMsg, 1, Qt::AlignCenter);
}
