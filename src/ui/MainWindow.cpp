#include "MainWindow.h"
#include "FolderPickerDialog.h"
#include <QMessageBox>
#include "AccountSidebar.h"
#include "MessageListPanel.h"
#include "MessageViewPanel.h"
#include "SettingsDialog.h"
#include "ComposeWindow.h"
#include "DraftsDialog.h"
#include "KeyboardShortcutsDialog.h"
#include "NotificationBanner.h"
#include "../core/DiagnosticsManager.h"
#include "../core/Logger.h"
#include "../core/Settings.h"
#include "../core/ThemeManager.h"
#include "../core/SyncManager.h"
#include "AccountSetupWizard.h"
#include "../storage/AccountStore.h"
#include "../storage/FolderStore.h"
#include "../storage/MessageStore.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QScreen>
#include <QSplitter>
#include <QStatusBar>
#include <QTextEdit>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setObjectName(QStringLiteral("MainWindow"));
    setWindowTitle(QStringLiteral("Mailinium"));
    setMinimumSize(800, 500);
    setUnifiedTitleAndToolBarOnMac(true);

    setupActions();
    setupMenuBar();
    setupToolBar();
    setupLayout();
    setupStatusBar();
    connectSignals();

    // Restore saved geometry. Must come after setupLayout() so the dock widget
    // and splitter exist before we attempt to restore their state.
    restoreWindowState();

    // Check if we need to show the wizard on first run
    QTimer::singleShot(0, this, [this]() {
        AccountStore store;
        if (store.accountCount() == 0) {
            onAddAccount();
        } else {
            SyncManager::instance().startAll();
        }
    });
}

MainWindow::~MainWindow() = default;

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

void MainWindow::setupActions()
{
    // --- File ---------------------------------------------------------------

    m_actNewMessage = new QAction(tr("New Message"), this);
    m_actNewMessage->setShortcut(QKeySequence::New);
    m_actNewMessage->setStatusTip(tr("Compose a new email message"));

    m_actDrafts = new QAction(tr("Drafts"), this);
    m_actDrafts->setStatusTip(tr("View saved drafts"));

    m_actAddAccount = new QAction(tr("Add Account..."), this);
    connect(m_actAddAccount, &QAction::triggered, this, &MainWindow::onAddAccount);
    m_actAddAccount->setStatusTip(tr("Connect a new email account"));

    m_actSettings = new QAction(tr("Settings..."), this);
    m_actSettings->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));
    m_actSettings->setStatusTip(tr("Open application settings"));
    m_actSettings->setMenuRole(QAction::PreferencesRole); // macOS Preferences menu

    m_actQuit = new QAction(tr("Quit Mailinium"), this);
    m_actQuit->setShortcut(QKeySequence::Quit);
    m_actQuit->setMenuRole(QAction::QuitRole);

    // --- Edit ---------------------------------------------------------------

    m_actUndo = new QAction(tr("Undo"), this);
    m_actUndo->setShortcut(QKeySequence::Undo);
    m_actUndo->setEnabled(false);

    m_actRedo = new QAction(tr("Redo"), this);
    m_actRedo->setShortcut(QKeySequence::Redo);
    m_actRedo->setEnabled(false);

    m_actFind = new QAction(tr("Find in Folder..."), this);
    m_actFind->setShortcuts({ QKeySequence::Find, QKeySequence(Qt::CTRL | Qt::Key_K) });
    m_actFind->setStatusTip(tr("Search your messages"));
    m_actFind->setEnabled(true);
    connect(m_actFind, &QAction::triggered, this, &MainWindow::onFind);

    m_actSelectAll = new QAction(tr("Select All"), this);
    m_actSelectAll->setShortcut(QKeySequence::SelectAll);
    m_actSelectAll->setEnabled(false);

    // --- View: density (radio group) ----------------------------------------

    m_densityGroup = new QActionGroup(this);

    m_actDensityCompact = new QAction(tr("Compact"), this);
    m_actDensityCompact->setCheckable(true);
    m_densityGroup->addAction(m_actDensityCompact);

    m_actDensityNormal = new QAction(tr("Normal"), this);
    m_actDensityNormal->setCheckable(true);
    m_actDensityNormal->setChecked(true); // default
    m_densityGroup->addAction(m_actDensityNormal);

    m_actDensityComfortable = new QAction(tr("Comfortable"), this);
    m_actDensityComfortable->setCheckable(true);
    m_densityGroup->addAction(m_actDensityComfortable);

    // Restore saved density
    const QString savedDensity = Settings::instance().stringValue(Settings::Key::UiMessageDensity);
    if (savedDensity == QLatin1String("compact"))     m_actDensityCompact->setChecked(true);
    else if (savedDensity == QLatin1String("comfortable")) m_actDensityComfortable->setChecked(true);
    else                                              m_actDensityNormal->setChecked(true);

    // --- View: theme (radio group) ------------------------------------------

    m_themeGroup = new QActionGroup(this);

    m_actThemeSystem = new QAction(tr("Follow System"), this);
    m_actThemeSystem->setCheckable(true);
    m_themeGroup->addAction(m_actThemeSystem);

    m_actThemeLight = new QAction(tr("Light"), this);
    m_actThemeLight->setCheckable(true);
    m_themeGroup->addAction(m_actThemeLight);

    m_actThemeDark = new QAction(tr("Dark"), this);
    m_actThemeDark->setCheckable(true);
    m_themeGroup->addAction(m_actThemeDark);

    // Reflect current theme
    switch (ThemeManager::instance().currentTheme()) {
    case ThemeManager::Theme::Light:  m_actThemeLight->setChecked(true);  break;
    case ThemeManager::Theme::Dark:   m_actThemeDark->setChecked(true);   break;
    default:                          m_actThemeSystem->setChecked(true); break;
    }

    // --- Message (all disabled — enabled when a message is selected) --------

    m_actReply = new QAction(tr("Reply"), this);
    m_actReply->setShortcut(Qt::CTRL | Qt::Key_R);
    m_actReply->setEnabled(false);
    connect(m_actReply, &QAction::triggered, this, &MainWindow::onReply);

    m_actReplyAll = new QAction(tr("Reply All"), this);
    m_actReplyAll->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_R);
    m_actReplyAll->setEnabled(false);
    connect(m_actReplyAll, &QAction::triggered, this, &MainWindow::onReplyAll);

    m_actForward = new QAction(tr("Forward"), this);
    m_actForward->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_F);
    m_actForward->setEnabled(false);
    connect(m_actForward, &QAction::triggered, this, &MainWindow::onForward);

    m_actArchive = new QAction(tr("Archive"), this);
    m_actArchive->setShortcut(Qt::Key_E);
    m_actArchive->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_actArchive->setEnabled(false);
    connect(m_actArchive, &QAction::triggered, this, &MainWindow::onArchiveMessage);

    m_actDelete = new QAction(tr("Delete"), this);
    m_actDelete->setShortcut(QKeySequence::Delete);
    m_actDelete->setEnabled(false);
    connect(m_actDelete, &QAction::triggered, this, &MainWindow::onDeleteMessage);

    m_actMarkRead = new QAction(tr("Mark as Read"), this);
    m_actMarkRead->setShortcut(Qt::SHIFT | Qt::Key_I);
    m_actMarkRead->setEnabled(false);
    connect(m_actMarkRead, &QAction::triggered, this, &MainWindow::onMarkRead);

    m_actMarkUnread = new QAction(tr("Mark as Unread"), this);
    m_actMarkUnread->setShortcut(Qt::SHIFT | Qt::Key_U);
    m_actMarkUnread->setEnabled(false);
    connect(m_actMarkUnread, &QAction::triggered, this, &MainWindow::onMarkUnread);

    m_actToggleStar = new QAction(tr("Toggle Star"), this);
    m_actToggleStar->setShortcut(Qt::Key_S);
    m_actToggleStar->setEnabled(false);
    connect(m_actToggleStar, &QAction::triggered, this, &MainWindow::onToggleStar);

    m_actMove = new QAction(tr("Move Message..."), this);
    m_actMove->setShortcut(Qt::Key_M);
    m_actMove->setEnabled(false);
    connect(m_actMove, &QAction::triggered, this, &MainWindow::onMoveMessage);

    m_actCopy = new QAction(tr("Copy Message..."), this);
    m_actCopy->setShortcut(Qt::Key_C);
    m_actCopy->setEnabled(false);
    connect(m_actCopy, &QAction::triggered, this, &MainWindow::onCopyMessage);

    m_actRefresh = new QAction(tr("Refresh Folder"), this);
    m_actRefresh->setShortcut(QKeySequence::Refresh);
    m_actRefresh->setEnabled(true);
    connect(m_actRefresh, &QAction::triggered, this, &MainWindow::onRefreshFolder);

    // --- Batch operations (Phase 4C) ----------------------------------------

    m_actBatchDelete = new QAction(tr("Delete Selected"), this);
    m_actBatchDelete->setEnabled(false);
    connect(m_actBatchDelete, &QAction::triggered, this, &MainWindow::onBatchDelete);

    m_actBatchArchive = new QAction(tr("Archive Selected"), this);
    m_actBatchArchive->setEnabled(false);
    connect(m_actBatchArchive, &QAction::triggered, this, &MainWindow::onBatchArchive);

    m_actBatchMarkRead = new QAction(tr("Mark Selected as Read"), this);
    m_actBatchMarkRead->setEnabled(false);
    connect(m_actBatchMarkRead, &QAction::triggered, this, &MainWindow::onBatchMarkRead);

    // --- Help ---------------------------------------------------------------

    m_actShortcuts = new QAction(tr("Keyboard Shortcuts"), this);
    m_actShortcuts->setShortcut(QKeySequence::HelpContents);
    connect(m_actShortcuts, &QAction::triggered, this, &MainWindow::onShortcutsSheet);

    m_actDiagnostics = new QAction(tr("System Diagnostics Report..."), this);
    connect(m_actDiagnostics, &QAction::triggered, this, &MainWindow::onDiagnosticsReport);

    m_actExportLogs = new QAction(tr("Export Logs..."), this);
    connect(m_actExportLogs, &QAction::triggered, this, &MainWindow::onExportLogs);

    m_actOpenLogDir = new QAction(tr("Open Log Folder"), this);
    connect(m_actOpenLogDir, &QAction::triggered, this, &MainWindow::onOpenLogDir);

    m_actAbout = new QAction(tr("About Mailinium"), this);
    m_actAbout->setMenuRole(QAction::AboutRole);
    connect(m_actAbout, &QAction::triggered, this, &MainWindow::onAbout);
}

// ---------------------------------------------------------------------------
// Menu bar
// ---------------------------------------------------------------------------

void MainWindow::setupMenuBar()
{
    // File
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(m_actNewMessage);
    fileMenu->addAction(m_actDrafts);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actAddAccount);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actSettings);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actQuit);

    // Edit
    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(m_actUndo);
    editMenu->addAction(m_actRedo);
    editMenu->addSeparator();
    editMenu->addAction(m_actSelectAll);
    editMenu->addSeparator();
    editMenu->addAction(m_actFind);

    // View
    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));

    // The dock widget provides its own checkable toggle action for free
    // (added after setupLayout() where the dock is created; see connectSignals)

    viewMenu->addSeparator();

    QMenu* densityMenu = viewMenu->addMenu(tr("Message Density"));
    densityMenu->addAction(m_actDensityCompact);
    densityMenu->addAction(m_actDensityNormal);
    densityMenu->addAction(m_actDensityComfortable);

    viewMenu->addSeparator();

    QMenu* themeMenu = viewMenu->addMenu(tr("Theme"));
    themeMenu->addAction(m_actThemeSystem);
    themeMenu->addAction(m_actThemeLight);
    themeMenu->addAction(m_actThemeDark);

    // Message
    QMenu* messageMenu = menuBar()->addMenu(tr("&Message"));
    messageMenu->addAction(m_actReply);
    messageMenu->addAction(m_actReplyAll);
    messageMenu->addAction(m_actForward);
    messageMenu->addSeparator();
    messageMenu->addAction(m_actArchive);
    messageMenu->addAction(m_actDelete);
    messageMenu->addSeparator();
    messageMenu->addAction(m_actMarkRead);
    messageMenu->addAction(m_actMarkUnread);
    messageMenu->addAction(m_actToggleStar);
    messageMenu->addSeparator();
    messageMenu->addAction(m_actMove);
    messageMenu->addAction(m_actCopy);
    messageMenu->addSeparator();
    messageMenu->addAction(m_actRefresh);
    messageMenu->addSeparator();
    messageMenu->addAction(m_actBatchDelete);
    messageMenu->addAction(m_actBatchArchive);
    messageMenu->addAction(m_actBatchMarkRead);

    // Help
    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(m_actShortcuts);
    helpMenu->addSeparator();
    helpMenu->addAction(m_actDiagnostics);
    helpMenu->addAction(m_actExportLogs);
    helpMenu->addAction(m_actOpenLogDir);
    helpMenu->addSeparator();
    helpMenu->addAction(m_actAbout);
}

// ---------------------------------------------------------------------------
// Toolbar
// ---------------------------------------------------------------------------

void MainWindow::setupToolBar()
{
    QToolBar* toolbar = addToolBar(tr("Main Toolbar"));
    toolbar->setObjectName(QStringLiteral("mainToolbar")); // required for saveState()
    toolbar->setMovable(false);

    toolbar->addAction(m_actNewMessage);
    toolbar->addSeparator();
    toolbar->addAction(m_actReply);
    toolbar->addAction(m_actReplyAll);
    toolbar->addAction(m_actForward);
    toolbar->addSeparator();
    toolbar->addAction(m_actArchive);
    toolbar->addAction(m_actDelete);
    toolbar->addSeparator();
    toolbar->addAction(m_actRefresh);
}

// ---------------------------------------------------------------------------
// Layout & Docking
// ---------------------------------------------------------------------------

void MainWindow::setupLayout()
{
    // Sidebar dock: accounts and folder tree
    m_sidebarDock = new QDockWidget(tr("Mailboxes"), this);
    m_sidebarDock->setObjectName(QStringLiteral("sidebarDock")); // required for saveState()
    m_sidebarDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable);
    m_sidebarDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    m_sidebar = new AccountSidebar(m_sidebarDock);
    m_sidebarDock->setWidget(m_sidebar);
    m_sidebarDock->setMinimumWidth(180);

    addDockWidget(Qt::LeftDockWidgetArea, m_sidebarDock);

    // Notification banner
    m_notificationBanner = new NotificationBanner(this);

    // Central content: message list + message view split horizontally
    m_messageList = new MessageListPanel(this);
    m_messageView = new MessageViewPanel(this);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setObjectName(QStringLiteral("messageSplitter"));
    m_splitter->setChildrenCollapsible(false);
    m_splitter->addWidget(m_messageList);
    m_splitter->addWidget(m_messageView);
    m_splitter->setStretchFactor(0, 0); // message list: fixed proportion
    m_splitter->setStretchFactor(1, 1); // message view: takes remaining space
    m_splitter->setSizes({340, 860});   // defaults; overridden by restoreWindowState()

    auto* centralWidget = new QWidget(this);
    auto* centralLayout = new QVBoxLayout(centralWidget);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    centralLayout->addWidget(m_notificationBanner);
    centralLayout->addWidget(m_splitter, 1);

    setCentralWidget(centralWidget);
}

// ---------------------------------------------------------------------------
// Status bar
// ---------------------------------------------------------------------------

void MainWindow::setupStatusBar()
{
    m_statusLabel = new QLabel(tr("Ready"), this);
    m_statusLabel->setContentsMargins(4, 0, 4, 0);

    m_syncLabel = new QLabel(tr("Not configured"), this);
    m_syncLabel->setContentsMargins(4, 0, 8, 0);

    statusBar()->addWidget(m_statusLabel, 1); // stretch = 1 (takes available space)
    statusBar()->addPermanentWidget(m_syncLabel);
    statusBar()->setSizeGripEnabled(true);
}

// ---------------------------------------------------------------------------
// Signal connections
// ---------------------------------------------------------------------------

void MainWindow::connectSignals()
{
    // Add dock toggle to View menu (the dock creates a correct QAction for us)
    // We insert it at position 0 in the View menu — find it by searching menus
    for (QAction* action : menuBar()->actions()) {
        if (action->text() == tr("&View")) {
            QMenu* viewMenu = action->menu();
            QAction* toggleAction = m_sidebarDock->toggleViewAction();
            toggleAction->setText(tr("Show Sidebar"));
            toggleAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Backslash));
            viewMenu->insertAction(viewMenu->actions().first(), toggleAction);
            break;
        }
    }

    // File actions
    connect(m_actNewMessage, &QAction::triggered, this, &MainWindow::onNewMessage);
    connect(m_actDrafts,     &QAction::triggered, this, &MainWindow::onDrafts);
    connect(m_actSettings,   &QAction::triggered, this, &MainWindow::onSettings);
    connect(m_actQuit,       &QAction::triggered, qApp, &QApplication::quit);

    // View: density
    connect(m_actDensityCompact, &QAction::triggered, this, [this] {
        Settings::instance().setValue(Settings::Key::UiMessageDensity, QStringLiteral("compact"));
    });
    connect(m_actDensityNormal, &QAction::triggered, this, [this] {
        Settings::instance().setValue(Settings::Key::UiMessageDensity, QStringLiteral("normal"));
    });
    connect(m_actDensityComfortable, &QAction::triggered, this, [this] {
        Settings::instance().setValue(Settings::Key::UiMessageDensity, QStringLiteral("comfortable"));
    });

    // View: theme
    connect(m_actThemeSystem, &QAction::triggered, this, [this] {
        ThemeManager::instance().apply(ThemeManager::Theme::System);
    });
    connect(m_actThemeLight, &QAction::triggered, this, [this] {
        ThemeManager::instance().apply(ThemeManager::Theme::Light);
    });
    connect(m_actThemeDark, &QAction::triggered, this, [this] {
        ThemeManager::instance().apply(ThemeManager::Theme::Dark);
    });

    // React to theme changes (update theme radio check state on system change)
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &MainWindow::onThemeChanged);

    // Sync Manager
    connect(&SyncManager::instance(), &SyncManager::syncStatusChanged,
            this, &MainWindow::onSyncStatusChanged);
    connect(&SyncManager::instance(), &SyncManager::foldersUpdated,
            this, &MainWindow::onFoldersUpdated);
    connect(&SyncManager::instance(), &SyncManager::messagesUpdated,
            this, &MainWindow::onMessagesUpdated);
    connect(&SyncManager::instance(), &SyncManager::messageBodyReady,
            this, &MainWindow::onMessageBodyReady);
            
    // Sidebar
    connect(m_sidebar, &AccountSidebar::folderSelected,
            this, &MainWindow::onFolderSelected);
            
    // Message List
    connect(m_messageList, &MessageListPanel::messageSelected, this, &MainWindow::onMessageSelected);
    connect(m_messageList, &MessageListPanel::selectionChanged, this, &MainWindow::onSelectionChanged);

    // Find — focus the search bar in the message list
    connect(m_actFind, &QAction::triggered, this, [this] {
        m_actFind->setEnabled(true); // keep enabled
        // We hook into the message list panel
        m_messageList->setFocus(Qt::ShortcutFocusReason);
    });
    m_actFind->setEnabled(true);

    // Help
    connect(m_actAbout, &QAction::triggered, this, &MainWindow::onAbout);
}

// ---------------------------------------------------------------------------
// Window state persistence
// ---------------------------------------------------------------------------

void MainWindow::saveWindowState()
{
    Settings& s = Settings::instance();
    s.setValue(Settings::Key::WindowGeometry,      saveGeometry());
    s.setValue(Settings::Key::WindowState,         QMainWindow::saveState());
    s.setValue(Settings::Key::MessageSplitterState, m_splitter->saveState());
}

void MainWindow::restoreWindowState()
{
    Settings& s = Settings::instance();

    const QByteArray geo   = s.value(Settings::Key::WindowGeometry).toByteArray();
    const QByteArray state = s.value(Settings::Key::WindowState).toByteArray();
    const QByteArray split = s.value(Settings::Key::MessageSplitterState).toByteArray();

    if (!geo.isEmpty())   restoreGeometry(geo);
    if (!state.isEmpty()) QMainWindow::restoreState(state);
    if (!split.isEmpty()) m_splitter->restoreState(split);

    // If no saved geometry, center the window at a good default size
    if (geo.isEmpty()) {
        resize(1200, 750);
        // Center on the primary screen
        if (const QScreen* screen = QApplication::primaryScreen()) {
            const QRect available = screen->availableGeometry();
            move(available.center() - rect().center());
        }
    }
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void MainWindow::onSyncStatusChanged(qint64 accountId, const QString& status)
{
    showStatusMessage(QStringLiteral("Sync: ") + status);

    if (status.contains(QStringLiteral("failed"), Qt::CaseInsensitive) || 
        status.contains(QStringLiteral("Error"), Qt::CaseInsensitive) ||
        status.contains(QStringLiteral("Needs Attention"), Qt::CaseInsensitive)) {
        if (m_notificationBanner) {
            m_notificationBanner->showNotification(
                NotificationBanner::Type::Error,
                tr("Account %1 sync error: %2").arg(accountId).arg(status),
                tr("Re-authenticate"),
                [accountId]() {
                    SyncManager::instance().reauthenticateAccount(accountId);
                }
            );
        }
    } else if (status.contains(QStringLiteral("Offline"), Qt::CaseInsensitive)) {
        if (m_notificationBanner) {
            m_notificationBanner->showNotification(
                NotificationBanner::Type::Warning,
                tr("Network offline — displaying cached emails."),
                tr("Retry"),
                [accountId]() {
                    SyncManager::instance().reauthenticateAccount(accountId);
                },
                8000
            );
        }
    } else if (status.contains(QStringLiteral("Synced"), Qt::CaseInsensitive) ||
               status.contains(QStringLiteral("Idle"), Qt::CaseInsensitive)) {
        if (m_notificationBanner) {
            m_notificationBanner->dismiss();
        }
    }
}

void MainWindow::onFoldersUpdated(qint64 accountId)
{
    Q_UNUSED(accountId);
    // Sidebar updates automatically, but we can log here
    Logger::instance().info(QStringLiteral("ui"), QStringLiteral("MainWindow received folders update for account ") + QString::number(accountId));
}

void MainWindow::onFolderSelected(qint64 accountId, qint64 folderId)
{
    m_currentAccountId = accountId; // Track for drafts
    m_currentMessage = std::nullopt;
    m_selectedMessageIds.clear();

    disableMessageActions();

    Logger::instance().info(QStringLiteral("ui"), QStringLiteral("MainWindow folder selected: ") + QString::number(folderId));
    m_messageList->loadMessages(folderId);
    SyncManager::instance().syncFolder(accountId, folderId);
}

void MainWindow::onMessageSelected(qint64 messageId)
{
    MessageStore store;
    auto optMsg = store.messageById(messageId);
    if (!optMsg.has_value()) return;

    // Track for reply/forward context
    m_currentMessage = optMsg;

    // Enable message context actions
    m_actReply->setEnabled(true);
    m_actReplyAll->setEnabled(true);
    m_actForward->setEnabled(true);
    m_actArchive->setEnabled(true);
    m_actDelete->setEnabled(true);
    m_actMarkRead->setEnabled(true);
    m_actMarkUnread->setEnabled(true);
    m_actToggleStar->setEnabled(true);
    m_actMove->setEnabled(true);
    m_actCopy->setEnabled(true);

    // [OPTIMIZATION] Check memory view cache first (refreshing with latest metadata)
    if (m_messageView->loadFromCache(messageId, optMsg.value())) {
        Logger::instance().debug(QStringLiteral("ui"), QStringLiteral("Message loaded from memory view cache: ") + QString::number(messageId));
        return;
    }

    if (optMsg->bodyFetched) {
        auto bodies = store.getMessageBodies(messageId);
        auto attachments = store.getAttachments(messageId, true); // true = load inline image blobs only!
        m_messageView->displayMimeMessage(optMsg.value(), bodies.first, bodies.second, attachments);
        return;
    }

    FolderStore fstore;
    auto optFolder = fstore.folderById(optMsg->folderId);
    if (!optFolder.has_value()) return;

    SyncManager::instance().fetchMessageBody(optFolder->accountId, optMsg->folderId, messageId);
}

void MainWindow::onMessagesUpdated(qint64 accountId, qint64 folderId)
{
    Q_UNUSED(accountId);
    m_messageList->loadMessages(folderId);
}

void MainWindow::onMessageBodyReady(qint64 accountId, qint64 messageId, const QString& body)
{
    Q_UNUSED(accountId);
    MessageStore store;
    auto optMsg = store.messageById(messageId);
    if (optMsg.has_value()) {
        auto bodies = store.getMessageBodies(messageId);
        auto attachments = store.getAttachments(messageId, true); // true = load inline image blobs only!

        if (!bodies.first.isEmpty() || !bodies.second.isEmpty()) {
            m_messageView->displayMimeMessage(optMsg.value(), bodies.first, bodies.second, attachments);
        } else {
            m_messageView->displayMessage(optMsg.value(), body);
        }
    }
}

void MainWindow::onNewMessage()
{
    Logger::instance().info(QStringLiteral("ui"), QStringLiteral("New message requested"));

    // Find the first available account
    AccountStore astore;
    const auto accounts = astore.allAccounts();
    if (accounts.isEmpty()) {
        QMessageBox::information(this, tr("No Account"),
            tr("Please add an email account first."));
        return;
    }

    const Account& acc = accounts.first();
    auto* w = new ComposeWindow(acc, this);
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->show();
}

void MainWindow::onAddAccount()
{
    AccountSetupWizard wizard(this);
    if (wizard.exec() == QDialog::Accepted) {
        if (m_sidebar) {
            m_sidebar->loadAccounts();
        }
        
        AccountStore store;
        auto acc = store.accountById(wizard.createdAccountId());
        if (acc.has_value()) {
            Logger::instance().info(QStringLiteral("ui"), QStringLiteral("Adding newly created account to SyncManager: ") + acc.value().email);
            SyncManager::instance().addAccount(acc.value());
        } else {
            Logger::instance().error(QStringLiteral("ui"), QStringLiteral("Failed to fetch newly created account ID: ") + QString::number(wizard.createdAccountId()));
        }
        
        showStatusMessage(QStringLiteral("Account added successfully"), 3000);
    }
}

void MainWindow::onSettings()
{
    Logger::instance().info(QStringLiteral("ui"), QStringLiteral("Settings requested"));

    if (!m_settingsDialog) {
        m_settingsDialog = new SettingsDialog(this);
    }
    m_settingsDialog->exec();
}

void MainWindow::onShortcutsSheet()
{
    KeyboardShortcutsDialog dlg(this);
    dlg.exec();
}

void MainWindow::onDiagnosticsReport()
{
    QString report = DiagnosticsManager::generateReport();
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(tr("System Diagnostics & Health Report"));
    dlg->resize(680, 520);

    auto* layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto* titleLabel = new QLabel(tr("<h3>System Diagnostics Report</h3>"), dlg);
    layout->addWidget(titleLabel);

    auto* textEdit = new QTextEdit(dlg);
    textEdit->setReadOnly(true);
    textEdit->setFontFamily(QStringLiteral("Consolas, monospace"));
    textEdit->setPlainText(report);
    layout->addWidget(textEdit, 1);

    auto* btnBox = new QDialogButtonBox(dlg);
    auto* btnCopy = btnBox->addButton(tr("Copy to Clipboard"), QDialogButtonBox::ActionRole);
    auto* btnClose = btnBox->addButton(QDialogButtonBox::Close);

    connect(btnCopy, &QPushButton::clicked, this, [report, this]() {
        QApplication::clipboard()->setText(report);
        QMessageBox::information(this, tr("Copied"), tr("Diagnostics report copied to clipboard."));
    });
    connect(btnClose, &QPushButton::clicked, dlg, &QDialog::accept);

    layout->addWidget(btnBox);
    dlg->exec();
    dlg->deleteLater();
}

void MainWindow::onExportLogs()
{
    QString defaultName = QStringLiteral("mailinium_logs_%1.txt")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss")));
    QString target = QFileDialog::getSaveFileName(
        this,
        tr("Export Application Logs"),
        defaultName,
        tr("Text Files (*.txt);;All Files (*)")
    );

    if (!target.isEmpty()) {
        if (Logger::instance().exportLogs(target)) {
            QMessageBox::information(this, tr("Logs Exported"),
                tr("Application logs successfully exported to:\n%1").arg(target));
        } else {
            QMessageBox::warning(this, tr("Export Failed"),
                tr("Failed to export logs to the specified file."));
        }
    }
}

void MainWindow::onOpenLogDir()
{
    QString dir = Logger::instance().logDir();
    if (!dir.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    }
}

void MainWindow::onFind()
{
    if (m_messageList) {
        m_messageList->focusSearch();
    }
}

void MainWindow::onAbout()
{
    QMessageBox::about(this,
        tr("About Mailinium"),
        tr("<h2>Mailinium 0.1.0</h2>"
           "<p>A lightweight, high-performance native desktop email client.</p>"
           "<p><b>Architecture Highlights:</b>"
           "<ul>"
           "<li>Native C++20 &amp; Qt %1</li>"
           "<li>Gmail OAuth 2.0 &amp; Standard IMAP/SMTP protocols</li>"
           "<li>Fast local SQLite FTS5 full-text search</li>"
           "<li>Low-RAM architecture with on-demand body streaming</li>"
           "<li>Resilient offline caching &amp; automatic recovery</li>"
           "</ul></p>"
           "<p>&copy; 2026 Mailinium Contributors.</p>")
            .arg(QLatin1String(QT_VERSION_STR)));
}

void MainWindow::onThemeChanged(bool /*dark*/)
{
    // Update the theme radio group to reflect any OS-driven change
    switch (ThemeManager::instance().currentTheme()) {
    case ThemeManager::Theme::Light:  m_actThemeLight->setChecked(true);  break;
    case ThemeManager::Theme::Dark:   m_actThemeDark->setChecked(true);   break;
    default:                          m_actThemeSystem->setChecked(true); break;
    }
}

// ---------------------------------------------------------------------------
// Public helpers
// ---------------------------------------------------------------------------

void MainWindow::showStatusMessage(const QString& message, int timeoutMs)
{
    if (timeoutMs > 0) {
        statusBar()->showMessage(message, timeoutMs);
    } else {
        m_statusLabel->setText(message);
    }
}

void MainWindow::setSyncStatus(const QString& status)
{
    m_syncLabel->setText(status);
}

void MainWindow::disableMessageActions()
{
    m_actReply->setEnabled(false);
    m_actReplyAll->setEnabled(false);
    m_actForward->setEnabled(false);
    m_actArchive->setEnabled(false);
    m_actDelete->setEnabled(false);
    m_actMarkRead->setEnabled(false);
    m_actMarkUnread->setEnabled(false);
    m_actToggleStar->setEnabled(false);
    m_actMove->setEnabled(false);
    m_actCopy->setEnabled(false);
    m_actBatchDelete->setEnabled(false);
    m_actBatchArchive->setEnabled(false);
    m_actBatchMarkRead->setEnabled(false);
}

// ---------------------------------------------------------------------------
// Reply / ReplyAll / Forward
// ---------------------------------------------------------------------------

static Account findAccountForMessage(const Message& msg)
{
    // Find the account owning the folder that contains this message
    FolderStore fs;
    const auto optFolder = fs.folderById(msg.folderId);
    if (!optFolder.has_value()) return {};

    AccountStore as;
    const auto optAcc = as.accountById(optFolder->accountId);
    return optAcc.value_or(Account{});
}

static QString getPlainBodyForMessage(qint64 messageId)
{
    MessageStore ms;
    const auto bodies = ms.getMessageBodies(messageId);
    return bodies.second.isEmpty() ? bodies.first : bodies.second;
}

void MainWindow::onReply()
{
    if (!m_currentMessage.has_value()) return;
    const Message& msg = m_currentMessage.value();
    const Account acc  = findAccountForMessage(msg);
    if (acc.id == -1) {
        Logger::instance().error(QStringLiteral("ui"),
            QStringLiteral("onReply: could not find account for message"));
        return;
    }

    const QString body = getPlainBodyForMessage(msg.id);
    auto* w = ComposeWindow::createReply(acc, msg, body, this);
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->show();
}

void MainWindow::onReplyAll()
{
    if (!m_currentMessage.has_value()) return;
    const Message& msg = m_currentMessage.value();
    const Account acc  = findAccountForMessage(msg);
    if (acc.id == -1) return;

    const QString body = getPlainBodyForMessage(msg.id);
    auto* w = ComposeWindow::createReplyAll(acc, msg, body, this);
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->show();
}

void MainWindow::onForward()
{
    if (!m_currentMessage.has_value()) return;
    const Message& msg = m_currentMessage.value();
    const Account acc  = findAccountForMessage(msg);
    if (acc.id == -1) return;

    const QString body = getPlainBodyForMessage(msg.id);
    auto* w = ComposeWindow::createForward(acc, msg, body, this);
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->show();
}

void MainWindow::onDrafts()
{
    if (m_currentAccountId == -1) {
        QMessageBox::information(this, tr("No Account"), tr("Please select an account first."));
        return;
    }

    AccountStore store;
    auto optAcc = store.accountById(m_currentAccountId);
    if (!optAcc) return;

    auto* dialog = new DraftsDialog(*optAcc, this);
    connect(dialog, &DraftsDialog::draftResumed, this, &MainWindow::onResumeDraft);
    dialog->exec();
    dialog->deleteLater();
}

void MainWindow::onResumeDraft(const Draft& draft)
{
    AccountStore store;
    auto optAcc = store.accountById(draft.accountId);
    if (!optAcc) return;

    auto* w = new ComposeWindow(*optAcc, this);
    w->loadDraft(draft);
    w->show();
}

void MainWindow::onDeleteMessage()
{
    if (!m_currentMessage.has_value()) return;
    qint64 folderId = m_currentMessage->folderId;
    qint64 messageId = m_currentMessage->id;
    
    m_currentMessage = std::nullopt;
    disableMessageActions();
    m_messageView->clear();

    SyncManager::instance().deleteMessage(m_currentAccountId, folderId, messageId);
}

void MainWindow::onArchiveMessage()
{
    if (!m_currentMessage.has_value()) return;
    qint64 folderId = m_currentMessage->folderId;
    qint64 messageId = m_currentMessage->id;
    
    m_currentMessage = std::nullopt;
    disableMessageActions();
    m_messageView->clear();

    SyncManager::instance().archiveMessage(m_currentAccountId, folderId, messageId);
}

void MainWindow::onMarkRead()
{
    if (!m_currentMessage.has_value()) return;
    SyncManager::instance().markMessageRead(m_currentAccountId, m_currentMessage->folderId, m_currentMessage->id, true);
}

void MainWindow::onMarkUnread()
{
    if (!m_currentMessage.has_value()) return;
    SyncManager::instance().markMessageRead(m_currentAccountId, m_currentMessage->folderId, m_currentMessage->id, false);
}

void MainWindow::onToggleStar()
{
    if (!m_currentMessage.has_value()) return;
    bool currentlyStarred = (m_currentMessage->flags & MessageFlags::Flagged) != 0;
    SyncManager::instance().starMessage(m_currentAccountId, m_currentMessage->folderId, m_currentMessage->id, !currentlyStarred);
    
    if (currentlyStarred) {
        m_currentMessage->flags &= ~MessageFlags::Flagged;
    } else {
        m_currentMessage->flags |= MessageFlags::Flagged;
    }
}

void MainWindow::onMoveMessage()
{
    if (!m_currentMessage.has_value()) return;
    qint64 sourceFolderId = m_currentMessage->folderId;
    qint64 messageId = m_currentMessage->id;
    
    FolderPickerDialog dlg(m_currentAccountId, sourceFolderId, this);
    if (dlg.exec() == QDialog::Accepted) {
        qint64 destFolderId = dlg.selectedFolderId();
        if (destFolderId != -1) {
            m_currentMessage = std::nullopt;
            disableMessageActions();
            m_messageView->clear();

            SyncManager::instance().moveMessage(m_currentAccountId, sourceFolderId, destFolderId, messageId);
        }
    }
}

void MainWindow::onCopyMessage()
{
    if (!m_currentMessage.has_value()) return;
    qint64 sourceFolderId = m_currentMessage->folderId;
    
    FolderPickerDialog dlg(m_currentAccountId, sourceFolderId, this);
    if (dlg.exec() == QDialog::Accepted) {
        qint64 destFolderId = dlg.selectedFolderId();
        if (destFolderId != -1) {
            SyncManager::instance().copyMessage(m_currentAccountId, sourceFolderId, destFolderId, m_currentMessage->id);
        }
    }
}

void MainWindow::onRefreshFolder()
{
    if (m_currentAccountId != -1 && m_messageList) {
        qint64 folderId = m_messageList->currentFolderId();
        if (folderId != -1) {
            SyncManager::instance().refreshFolder(m_currentAccountId, folderId);
        }
    }
}

// ---------------------------------------------------------------------------
// Batch operations (Phase 4C)
// ---------------------------------------------------------------------------

void MainWindow::onSelectionChanged(const QList<qint64>& ids)
{
    m_selectedMessageIds = ids;
    const bool multiSelected = ids.size() > 1;
    m_actBatchDelete->setEnabled(multiSelected);
    m_actBatchArchive->setEnabled(multiSelected);
    m_actBatchMarkRead->setEnabled(multiSelected);
}

void MainWindow::onBatchDelete()
{
    if (m_selectedMessageIds.isEmpty() || m_currentAccountId == -1) return;

    MessageStore store;
    // Collect folder IDs — all selected messages may be in different folders,
    // but in practice they will all be in the same folder since the list shows one folder.
    QSet<qint64> affectedFolders;
    for (qint64 msgId : m_selectedMessageIds) {
        auto optMsg = store.messageById(msgId);
        if (optMsg.has_value()) {
            affectedFolders.insert(optMsg->folderId);
            SyncManager::instance().deleteMessage(m_currentAccountId, optMsg->folderId, msgId);
        }
    }

    m_selectedMessageIds.clear();
    m_currentMessage = std::nullopt;
    m_messageView->clear();
    disableMessageActions();
}

void MainWindow::onBatchArchive()
{
    if (m_selectedMessageIds.isEmpty() || m_currentAccountId == -1) return;

    MessageStore store;
    for (qint64 msgId : m_selectedMessageIds) {
        auto optMsg = store.messageById(msgId);
        if (optMsg.has_value()) {
            SyncManager::instance().archiveMessage(m_currentAccountId, optMsg->folderId, msgId);
        }
    }

    m_selectedMessageIds.clear();
    m_currentMessage = std::nullopt;
    m_messageView->clear();
    disableMessageActions();
}

void MainWindow::onBatchMarkRead()
{
    if (m_selectedMessageIds.isEmpty() || m_currentAccountId == -1) return;

    MessageStore store;
    for (qint64 msgId : m_selectedMessageIds) {
        auto optMsg = store.messageById(msgId);
        if (optMsg.has_value()) {
            SyncManager::instance().markMessageRead(m_currentAccountId, optMsg->folderId, msgId, true);
        }
    }
    m_selectedMessageIds.clear();
}

// ---------------------------------------------------------------------------
// Close event
// ---------------------------------------------------------------------------

void MainWindow::closeEvent(QCloseEvent* event)
{
    saveWindowState();
    Logger::instance().info(QStringLiteral("ui"), QStringLiteral("Main window closing"));
    event->accept();
}

#include "moc_MainWindow.cpp"
