#include "AccountSidebar.h"
#include "../core/ThemeManager.h"
#include "../core/SyncManager.h"
#include "../storage/AccountStore.h"
#include "../storage/FolderStore.h"
#include "../core/Logger.h"

#include "../auth/GoogleOAuth.h"

#include <QApplication>
#include <QFrame>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QMenu>
#include <QMessageBox>

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
            
    connect(&SyncManager::instance(), &SyncManager::foldersUpdated,
            this, &AccountSidebar::onFoldersUpdated);

    connect(&SyncManager::instance(), &SyncManager::syncStatusChanged,
            this, &AccountSidebar::onSyncStatusChanged);

    connect(&SyncManager::instance(), &SyncManager::accountRemoved,
            this, &AccountSidebar::loadAccounts);
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
    
    connect(m_accountList, &QListWidget::itemSelectionChanged, 
            this, &AccountSidebar::onAccountSelected);
    m_accountList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_accountList, &QListWidget::customContextMenuRequested, 
            this, &AccountSidebar::onAccountContextMenu);
            
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
    
    connect(m_folderTree, &QTreeWidget::itemSelectionChanged,
            this, &AccountSidebar::onFolderSelectionChanged);
            
    layout->addWidget(m_folderTree, 1); // stretch — takes remaining height

    loadAccounts();
}

void AccountSidebar::loadAccounts()
{
    m_accountList->clear();

    AccountStore store;
    const auto accounts = store.allAccounts();

    if (accounts.isEmpty()) {
        m_accountList->hide();
        m_emptyHint->show();
    } else {
        m_emptyHint->hide();
        m_accountList->show();
        for (const Account& acc : accounts) {
            auto* item = new QListWidgetItem(acc.email, m_accountList);
            item->setData(Qt::UserRole, acc.id);
        }
        
        if (m_accountList->count() > 0) {
            m_accountList->setCurrentRow(0);
        }
    }
}

void AccountSidebar::onAccountSelected()
{
    m_folderTree->clear();
    
    auto items = m_accountList->selectedItems();
    if (items.isEmpty()) {
        return;
    }
    
    qint64 accountId = items.first()->data(Qt::UserRole).toLongLong();
    
    FolderStore store;
    QList<Folder> folders = store.foldersForAccount(accountId);
    Logger::instance().info(QStringLiteral("ui"), QStringLiteral("Sidebar loading ") + QString::number(folders.size()) + " folders for account " + QString::number(accountId));
    
    for (const Folder& f : folders) {
        auto* item = new QTreeWidgetItem(m_folderTree);
        item->setText(0, f.name);
        item->setData(0, Qt::UserRole, f.id);
        
        if (f.role == QStringLiteral("noselect")) {
            item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
            // Optionally make it visually distinct
            QFont font = item->font(0);
            font.setItalic(true);
            item->setFont(0, font);
            item->setForeground(0, QBrush(Qt::gray));
        }
    }
}

void AccountSidebar::onFolderSelectionChanged()
{
    auto accounts = m_accountList->selectedItems();
    if (accounts.isEmpty()) return;
    qint64 accountId = accounts.first()->data(Qt::UserRole).toLongLong();
    
    auto items = m_folderTree->selectedItems();
    if (items.isEmpty()) return;
    
    qint64 folderId = items.first()->data(0, Qt::UserRole).toLongLong();
    emit folderSelected(accountId, folderId);
}

void AccountSidebar::onSyncStatusChanged(qint64 accountId, const QString& status)
{
    for (int i = 0; i < m_accountList->count(); ++i) {
        QListWidgetItem* item = m_accountList->item(i);
        if (item && item->data(Qt::UserRole).toLongLong() == accountId) {
            AccountStore store;
            auto opt = store.accountById(accountId);
            QString email = opt.has_value() ? opt->email : item->text();

            if (status == QStringLiteral("Needs Attention") || status == QStringLiteral("Login failed")) {
                item->setText(email + QStringLiteral(" [!]"));
                item->setToolTip(tr("Authentication failed: ") + status);
            } else if (status == QStringLiteral("Offline")) {
                item->setText(email + QStringLiteral(" (offline)"));
                item->setToolTip(tr("Network offline — showing cached mail"));
            } else {
                item->setText(email);
                item->setToolTip(status);
            }
            break;
        }
    }
}

void AccountSidebar::onAccountContextMenu(const QPoint& pos)
{
    QListWidgetItem* item = m_accountList->itemAt(pos);
    if (!item) return;

    qint64 accountId = item->data(Qt::UserRole).toLongLong();
    
    QMenu menu(this);
    QAction* syncAction = menu.addAction(tr("Sync Account"));
    QAction* reauthAction = menu.addAction(tr("Re-authenticate..."));
    menu.addSeparator();
    QAction* removeAction = menu.addAction(tr("Remove Account"));
    
    QAction* selectedAction = menu.exec(m_accountList->mapToGlobal(pos));
    if (selectedAction == syncAction) {
        SyncManager::instance().refreshFolder(accountId, -1);
    } else if (selectedAction == reauthAction) {
        AccountStore store;
        auto optAcc = store.accountById(accountId);
        if (!optAcc.has_value()) return;

        Account acc = *optAcc;
        if (acc.authType == AuthType::OAuth2) {
            auto* oauth = new GoogleOAuth(this);
            connect(oauth, &GoogleOAuth::authSuccessful, this, [this, oauth, accountId](const QString& email, const QString& accessToken, const QString& refreshToken, int expiresInSecs) {
                oauth->deleteLater();
                AccountStore s;
                s.updateOAuthTokens(accountId, accessToken, refreshToken, QDateTime::currentDateTimeUtc().addSecs(expiresInSecs));
                SyncManager::instance().reauthenticateAccount(accountId);
                loadAccounts();
                QMessageBox::information(this, tr("Authentication Succeeded"), tr("Successfully re-authenticated %1.").arg(email));
            });
            connect(oauth, &GoogleOAuth::authFailed, this, [this, oauth](const QString& errorMsg, bool /*isPermanent*/) {
                oauth->deleteLater();
                QMessageBox::warning(this, tr("Authentication Failed"), errorMsg);
            });
            oauth->startReauthFlow(acc.email);
        } else {
            bool ok = false;
            QString newPass = QInputDialog::getText(this, tr("Re-authenticate Account"),
                tr("Enter password for %1:").arg(acc.email), QLineEdit::Password, QString(), &ok);
            if (ok && !newPass.isEmpty()) {
                AccountStore s;
                s.updatePassword(accountId, newPass);
                SyncManager::instance().reauthenticateAccount(accountId);
                loadAccounts();
            }
        }
    } else if (selectedAction == removeAction) {
        if (QMessageBox::question(this, tr("Remove Account"), tr("Are you sure you want to remove this account? This will delete all downloaded messages.")) == QMessageBox::Yes) {
            SyncManager::instance().removeAccount(accountId);
            loadAccounts();
        }
    }
}

void AccountSidebar::onFoldersUpdated(qint64 accountId)
{
    Logger::instance().info(QStringLiteral("ui"), QStringLiteral("Sidebar received foldersUpdated for account ") + QString::number(accountId));
    auto items = m_accountList->selectedItems();
    if (!items.isEmpty()) {
        qint64 selectedId = items.first()->data(Qt::UserRole).toLongLong();
        if (selectedId == accountId) {
            onAccountSelected(); // Reload folders for this account
        }
    }
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
