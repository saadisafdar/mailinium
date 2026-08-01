#include "MainWindow.h"
#include "AccountSidebar.h"
#include "MessageListPanel.h"
#include "MessageViewPanel.h"
#include "SettingsDialog.h"
#include "../core/Logger.h"
#include "../core/Settings.h"
#include "../core/ThemeManager.h"
#include "AccountSetupWizard.h"
#include "../storage/AccountStore.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QDockWidget>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QScreen>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>

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

    m_actFind = new QAction(tr("Find..."), this);
    m_actFind->setShortcut(QKeySequence::Find);
    m_actFind->setStatusTip(tr("Search your messages"));
    m_actFind->setEnabled(false); // Phase 3H

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

    m_actReplyAll = new QAction(tr("Reply All"), this);
    m_actReplyAll->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_R);
    m_actReplyAll->setEnabled(false);

    m_actForward = new QAction(tr("Forward"), this);
    m_actForward->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_F);
    m_actForward->setEnabled(false);

    m_actArchive = new QAction(tr("Archive"), this);
    m_actArchive->setShortcut(Qt::Key_E);
    m_actArchive->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_actArchive->setEnabled(false);

    m_actDelete = new QAction(tr("Delete"), this);
    m_actDelete->setShortcut(QKeySequence::Delete);
    m_actDelete->setEnabled(false);

    m_actMarkRead = new QAction(tr("Mark as Read"), this);
    m_actMarkRead->setShortcut(Qt::SHIFT | Qt::Key_I);
    m_actMarkRead->setEnabled(false);

    m_actMarkUnread = new QAction(tr("Mark as Unread"), this);
    m_actMarkUnread->setShortcut(Qt::SHIFT | Qt::Key_U);
    m_actMarkUnread->setEnabled(false);

    // --- Help ---------------------------------------------------------------

    m_actAbout = new QAction(tr("About Mailinium"), this);
    m_actAbout->setMenuRole(QAction::AboutRole);
}

// ---------------------------------------------------------------------------
// Menu bar
// ---------------------------------------------------------------------------

void MainWindow::setupMenuBar()
{
    // File
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(m_actNewMessage);
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

    // Help
    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
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
    toolbar->setFloatable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    toolbar->setIconSize(QSize(16, 16));

    toolbar->addAction(m_actNewMessage);
    toolbar->addSeparator();
    toolbar->addAction(m_actReply);
    toolbar->addAction(m_actForward);
    toolbar->addSeparator();
    toolbar->addAction(m_actArchive);
    toolbar->addAction(m_actDelete);
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

void MainWindow::setupLayout()
{
    // Sidebar — in a dock widget so QMainWindow::saveState() tracks it
    m_sidebar = new AccountSidebar(this);

    m_sidebarDock = new QDockWidget(tr("Navigation"), this);
    m_sidebarDock->setObjectName(QStringLiteral("sidebarDock")); // required for saveState()
    m_sidebarDock->setWidget(m_sidebar);
    m_sidebarDock->setFeatures(
        QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
    m_sidebarDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_sidebarDock->setMinimumWidth(180);
    addDockWidget(Qt::LeftDockWidgetArea, m_sidebarDock);

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

    setCentralWidget(m_splitter);
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
    connect(m_actSettings, &QAction::triggered, this, &MainWindow::onSettings);
    connect(m_actQuit,     &QAction::triggered, qApp,  &QApplication::quit);

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

void MainWindow::onNewMessage()
{
    Logger::instance().info(QStringLiteral("ui"), QStringLiteral("New message requested"));
    // Future: open composer window
    showStatusMessage(QStringLiteral("New message triggered"), 2000);
}

void MainWindow::onAddAccount()
{
    AccountSetupWizard wizard(this);
    if (wizard.exec() == QDialog::Accepted) {
        if (m_sidebar) {
            m_sidebar->loadAccounts();
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

void MainWindow::onAbout()
{
    QMessageBox::about(this,
        tr("About Mailinium"),
        tr("<b>Mailinium</b> 0.1.0<br><br>"
           "A lightweight, native desktop email client.<br><br>"
           "Built with Qt %1 and C++20.")
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

// ---------------------------------------------------------------------------
// Close event
// ---------------------------------------------------------------------------

void MainWindow::closeEvent(QCloseEvent* event)
{
    saveWindowState();
    Logger::instance().info(QStringLiteral("ui"), QStringLiteral("Main window closing"));
    event->accept();
}
