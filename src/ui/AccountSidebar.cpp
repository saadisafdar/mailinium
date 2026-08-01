#include "AccountSidebar.h"
#include "../core/ThemeManager.h"

#include <QApplication>
#include <QFrame>
#include <QLabel>
#include <QListWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

AccountSidebar::AccountSidebar(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    applyBackground(ThemeManager::instance().isDark());

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &AccountSidebar::onThemeChanged);
}

AccountSidebar::~AccountSidebar() = default;

// ---------------------------------------------------------------------------
// UI setup
// ---------------------------------------------------------------------------

void AccountSidebar::setupUi()
{
    // Prevent the sidebar from collapsing in the dock
    setMinimumWidth(180);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 8, 0, 0);
    layout->setSpacing(0);

    // -- Accounts section header -------------------------------------------
    m_accountsHeader = new QLabel(tr("ACCOUNTS"), this);
    m_accountsHeader->setContentsMargins(12, 0, 8, 4);
    {
        QFont f = m_accountsHeader->font();
        f.setPointSizeF(f.pointSizeF() * 0.78);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
        f.setBold(true);
        m_accountsHeader->setFont(f);
    }
    // Use the disabled text color as a subtle header color
    QPalette headerPal = m_accountsHeader->palette();
    headerPal.setColor(QPalette::WindowText,
                       QApplication::palette().color(QPalette::Disabled, QPalette::WindowText));
    m_accountsHeader->setPalette(headerPal);
    layout->addWidget(m_accountsHeader);

    // -- Account list ---------------------------------------------------------
    m_accountList = new QListWidget(this);
    m_accountList->setFrameShape(QFrame::NoFrame);
    m_accountList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_accountList->setMaximumHeight(150); // grows up to ~4 accounts
    m_accountList->setSpacing(1);
    layout->addWidget(m_accountList);

    // -- Empty state hint (shown while account list has no items) -------------
    m_emptyHint = new QLabel(tr("No accounts.\nFile \u2192 Add Account"), this);
    m_emptyHint->setAlignment(Qt::AlignCenter);
    m_emptyHint->setContentsMargins(12, 8, 12, 12);
    m_emptyHint->setWordWrap(true);
    {
        QFont f = m_emptyHint->font();
        f.setPointSizeF(f.pointSizeF() * 0.88);
        m_emptyHint->setFont(f);
    }
    QPalette hintPal = m_emptyHint->palette();
    hintPal.setColor(QPalette::WindowText,
                     QApplication::palette().color(QPalette::Disabled, QPalette::WindowText));
    m_emptyHint->setPalette(hintPal);
    layout->addWidget(m_emptyHint);

    // -- Separator ------------------------------------------------------------
    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Plain);
    sep->setContentsMargins(0, 4, 0, 4);
    layout->addWidget(sep);

    // -- Folders section header -----------------------------------------------
    m_foldersHeader = new QLabel(tr("FOLDERS"), this);
    m_foldersHeader->setContentsMargins(12, 4, 8, 4);
    {
        QFont f = m_foldersHeader->font();
        f.setPointSizeF(f.pointSizeF() * 0.78);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
        f.setBold(true);
        m_foldersHeader->setFont(f);
    }
    m_foldersHeader->setPalette(headerPal); // reuse the same subtle color
    layout->addWidget(m_foldersHeader);

    // -- Folder tree ----------------------------------------------------------
    m_folderTree = new QTreeWidget(this);
    m_folderTree->setFrameShape(QFrame::NoFrame);
    m_folderTree->setHeaderHidden(true);
    m_folderTree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_folderTree->setIndentation(16);
    m_folderTree->setRootIsDecorated(false);
    layout->addWidget(m_folderTree, 1); // stretch — takes remaining height
}

// ---------------------------------------------------------------------------
// Theme
// ---------------------------------------------------------------------------

void AccountSidebar::onThemeChanged(bool dark)
{
    applyBackground(dark);

    // Re-apply the subtle header color using the freshly updated application palette.
    // Each widget's palette is derived from itself so all other roles stay correct.
    const QColor subtleColor =
        QApplication::palette().color(QPalette::Disabled, QPalette::WindowText);

    auto applySubtle = [&subtleColor](QLabel* label) {
        QPalette p = label->palette();
        p.setColor(QPalette::WindowText, subtleColor);
        label->setPalette(p);
    };

    applySubtle(m_accountsHeader);
    applySubtle(m_foldersHeader);
    applySubtle(m_emptyHint);
}

void AccountSidebar::applyBackground(bool dark)
{
    // The sidebar uses a slightly different background to visually separate
    // it from the message content area — standard in all email clients.
    const QColor bg = dark ? QColor(37, 37, 38)    // #252526 — VSCode sidebar
                           : QColor(240, 240, 240); // slightly darker than white

    QPalette pal = QApplication::palette();
    pal.setColor(QPalette::Window, bg);
    pal.setColor(QPalette::Base,   bg);

    setAutoFillBackground(true);
    setPalette(pal);

    // The list and tree widgets also need their base color updated
    if (m_accountList) {
        QPalette lp = m_accountList->palette();
        lp.setColor(QPalette::Base,          bg);
        lp.setColor(QPalette::AlternateBase, bg);
        m_accountList->setPalette(lp);
    }
    if (m_folderTree) {
        QPalette tp = m_folderTree->palette();
        tp.setColor(QPalette::Base,          bg);
        tp.setColor(QPalette::AlternateBase, bg);
        m_folderTree->setPalette(tp);
    }
}
