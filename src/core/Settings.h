#pragma once

#include <QObject>
#include <QVariant>
#include <QString>

//
// Settings — typed wrapper over QSettings.
//
// All configuration access in the application must go through this class.
// No raw QSettings calls are permitted outside of this file.
//
// Key groups mirror the architecture document:
//   General / Sync / UI / Notifications / Compose / Privacy / Advanced
//
// Usage:
//   Settings::instance().setValue(Settings::Key::UiTheme, "dark");
//   QString theme = Settings::instance().value(Settings::Key::UiTheme).toString();
//
class Settings : public QObject
{
    Q_OBJECT

public:
    // -----------------------------------------------------------------------
    // All valid setting keys — strongly typed to prevent typos.
    // -----------------------------------------------------------------------
    enum class Key {
        // General
        GeneralLanguage,       // QString — IETF language tag, e.g. "en-US"
        GeneralTheme,          // QString — "system" | "light" | "dark"
        GeneralStartMinimized, // bool

        // Sync
        SyncIntervalSeconds,   // int — polling interval when IDLE unavailable
        SyncFetchLimit,        // int — max messages to fetch in initial sync
        SyncOfflineCacheMB,    // int — max MB for attachment cache

        // UI
        UiSidebarWidth,        // int — pixels
        UiMessageListWidth,    // int — pixels
        UiFontSizePt,          // int — points
        UiMessageDensity,      // QString — "compact" | "normal" | "comfortable"

        // Notifications
        NotificationsEnabled,  // bool
        NotificationsSound,    // bool

        // Compose
        ComposeDefaultReply,   // QString — "reply" | "reply-all"

        // Privacy
        PrivacyLoadRemoteImages, // bool — false by default

        // Advanced
        AdvancedLogLevel,      // QString — "debug" | "info" | "warning" | "error"
        AdvancedConnectionTimeoutMs, // int

        // Window state — persisted across sessions
        WindowGeometry,        // QByteArray — from QMainWindow::saveGeometry()
        WindowState,           // QByteArray — from QMainWindow::saveState() (toolbars + docks)
        MessageSplitterState,  // QByteArray — from QSplitter::saveState() (list vs. view)
    };

    static Settings& instance();

    // Initialize with the application data directory.
    // Must be called before any value() or setValue() calls.
    void initialize(const QString& dataDir);

    ~Settings();

    QVariant value(Key key) const;
    void     setValue(Key key, const QVariant& value);

    // Convenience typed accessors
    bool    boolValue  (Key key) const;
    int     intValue   (Key key) const;
    QString stringValue(Key key) const;

    // Flush to disk — called on application shutdown.
    void sync();

signals:
    void settingChanged(Key key, QVariant newValue);

private:
    explicit Settings(QObject* parent = nullptr);

    QString        keyString(Key key) const;
    QVariant       defaultValue(Key key) const;
    void           applyDefaults();

    struct Private;
    Private* d = nullptr;
};
