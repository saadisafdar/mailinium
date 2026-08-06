#pragma once

#include <QMainWindow>
#include <optional>
#include "storage/Draft.h"
#include "storage/Message.h"

class QAction;
class QActionGroup;
class QDockWidget;
class QLabel;
class QSplitter;

class AccountSidebar;
class MessageListPanel;
class MessageViewPanel;
class SettingsDialog;

//
// MainWindow — the application's top-level window.
//
// Owns the complete three-pane layout:
//   Left dock:  AccountSidebar (account list + folder tree)
//   Center:     QSplitter
//                ├─ MessageListPanel (messages for selected folder)
//                └─ MessageViewPanel (selected message body)
//
// Also owns all QActions. Actions are wired to menus, toolbar, and
// keyboard shortcuts from a single source — no duplication.
//
// Message-context actions (Reply, Forward, Archive, Delete) are declared
// now but disabled. MessageViewPanel will enable them when a message is
// selected (Phase 3E).
//
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // Display a transient message in the left section of the status bar.
    // If timeoutMs == 0, the message persists until replaced.
    void showStatusMessage(const QString& message, int timeoutMs = 0);

    // Update the right-side permanent sync/connection label.
    void setSyncStatus(const QString& status);

public slots:
    void onThemeChanged(bool dark);
    void onSyncStatusChanged(qint64 accountId, const QString& status);
    void onFoldersUpdated(qint64 accountId);
    
    void onFolderSelected(qint64 accountId, qint64 folderId);
    void onMessageSelected(qint64 messageId);
    void onMessagesUpdated(qint64 accountId, qint64 folderId);
    void onMessageBodyReady(qint64 accountId, qint64 messageId, const QString& body);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onNewMessage();
    void onDrafts();
    void onResumeDraft(const Draft& draft);
    void onReply();
    void onReplyAll();
    void onForward();
    void onAddAccount();
    void onSettings();
    void onAbout();

private:
    // -- Setup phases (called in order from constructor) --------------------
    void setupActions();
    void setupMenuBar();
    void setupToolBar();
    void setupLayout();
    void setupStatusBar();
    void connectSignals();

    // -- Window state persistence -------------------------------------------
    void saveWindowState();
    void restoreWindowState();

    // -----------------------------------------------------------------------
    // Layout
    // -----------------------------------------------------------------------
    QDockWidget*      m_sidebarDock   = nullptr;
    AccountSidebar*   m_sidebar       = nullptr;
    QSplitter*        m_splitter      = nullptr;
    MessageListPanel* m_messageList   = nullptr;
    MessageViewPanel* m_messageView   = nullptr;

    // -----------------------------------------------------------------------
    // Status bar
    // -----------------------------------------------------------------------
    QLabel* m_statusLabel = nullptr;   // left — transient activity text
    QLabel* m_syncLabel   = nullptr;   // right — persistent connection state

    // -----------------------------------------------------------------------
    // Dialogs (lazy-created)
    // -----------------------------------------------------------------------
    SettingsDialog* m_settingsDialog = nullptr;

    // Currently selected message (for Reply/Forward context)
    std::optional<Message> m_currentMessage;
    qint64 m_currentAccountId = -1;

    // -----------------------------------------------------------------------
    // File menu
    // -----------------------------------------------------------------------
    QAction* m_actNewMessage  = nullptr;
    QAction* m_actDrafts      = nullptr;
    QAction* m_actAddAccount  = nullptr;
    QAction* m_actSettings    = nullptr;
    QAction* m_actQuit        = nullptr;

    // -----------------------------------------------------------------------
    // Edit menu
    // -----------------------------------------------------------------------
    QAction* m_actUndo      = nullptr;
    QAction* m_actRedo      = nullptr;
    QAction* m_actFind      = nullptr;
    QAction* m_actSelectAll = nullptr;

    // -----------------------------------------------------------------------
    // View menu
    // -----------------------------------------------------------------------
    QAction* m_actDensityCompact      = nullptr;
    QAction* m_actDensityNormal       = nullptr;
    QAction* m_actDensityComfortable  = nullptr;
    QActionGroup* m_densityGroup      = nullptr;

    QAction* m_actThemeSystem  = nullptr;
    QAction* m_actThemeLight   = nullptr;
    QAction* m_actThemeDark    = nullptr;
    QActionGroup* m_themeGroup = nullptr;

    // -----------------------------------------------------------------------
    // Message menu (disabled until a message is selected — Phase 3E)
    // -----------------------------------------------------------------------
    QAction* m_actReply       = nullptr;
    QAction* m_actReplyAll    = nullptr;
    QAction* m_actForward     = nullptr;
    QAction* m_actArchive     = nullptr;
    QAction* m_actDelete      = nullptr;
    QAction* m_actMarkRead    = nullptr;
    QAction* m_actMarkUnread  = nullptr;

    // -----------------------------------------------------------------------
    // Help menu
    // -----------------------------------------------------------------------
    QAction* m_actAbout = nullptr;
};
