#include "Settings.h"

#include <QSettings>
#include <QDir>

// ---------------------------------------------------------------------------
// Private implementation
// QSettings inherits QObject and is non-copyable, so we hold it by pointer.
// ---------------------------------------------------------------------------

struct Settings::Private {
    QSettings* qs = nullptr;

    Private() = default;
    ~Private() { delete qs; }

    // Non-copyable — QSettings is non-copyable.
    Private(const Private&)            = delete;
    Private& operator=(const Private&) = delete;
};

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------

Settings& Settings::instance()
{
    static Settings s_instance;
    return s_instance;
}

Settings::Settings(QObject* parent)
    : QObject(parent)
    , d(new Private)
{
}

Settings::~Settings()
{
    delete d;
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

void Settings::initialize(const QString& dataDir)
{
    if (!QDir().mkpath(dataDir)) {
        // Cannot create data directory. QSettings will fail gracefully on
        // subsequent reads/writes, returning defaults.
    }

    // Delete any previous QSettings instance (initialize() should only be
    // called once, but guard against accidental re-calls defensively).
    delete d->qs;
    d->qs = new QSettings(dataDir + QStringLiteral("/settings.ini"), QSettings::IniFormat);

    applyDefaults();
}

void Settings::applyDefaults()
{
    // Write default values for any key that has not been set yet so that
    // call sites never need to check for missing keys.
    const Key keys[] = {
        Key::GeneralLanguage,
        Key::GeneralTheme,
        Key::GeneralStartMinimized,
        Key::SyncIntervalSeconds,
        Key::SyncFetchLimit,
        Key::SyncOfflineCacheMB,
        Key::UiSidebarWidth,
        Key::UiMessageListWidth,
        Key::UiFontSizePt,
        Key::UiMessageDensity,
        Key::NotificationsEnabled,
        Key::NotificationsSound,
        Key::ComposeDefaultReply,
        Key::PrivacyLoadRemoteImages,
        Key::AdvancedLogLevel,
        Key::AdvancedConnectionTimeoutMs,
        // Window state keys intentionally excluded — empty QByteArray defaults
        // should not be written to the INI file; they serve as sentinels.
    };

    for (Key key : keys) {
        const QString k = keyString(key);
        if (!d->qs->contains(k)) {
            d->qs->setValue(k, defaultValue(key));
        }
    }

    d->qs->sync();
}

// ---------------------------------------------------------------------------
// Access
// ---------------------------------------------------------------------------

QVariant Settings::value(Key key) const
{
    return d->qs->value(keyString(key), defaultValue(key));
}

void Settings::setValue(Key key, const QVariant& val)
{
    d->qs->setValue(keyString(key), val);
    emit settingChanged(key, val);
}

bool    Settings::boolValue  (Key key) const { return value(key).toBool();   }
int     Settings::intValue   (Key key) const { return value(key).toInt();    }
QString Settings::stringValue(Key key) const { return value(key).toString(); }

void Settings::sync()
{
    d->qs->sync();
}

// ---------------------------------------------------------------------------
// Key → string mapping (INI file keys)
// ---------------------------------------------------------------------------

QString Settings::keyString(Key key) const
{
    switch (key) {
    case Key::GeneralLanguage:             return QStringLiteral("General/Language");
    case Key::GeneralTheme:                return QStringLiteral("General/Theme");
    case Key::GeneralStartMinimized:       return QStringLiteral("General/StartMinimized");
    case Key::SyncIntervalSeconds:         return QStringLiteral("Sync/IntervalSeconds");
    case Key::SyncFetchLimit:              return QStringLiteral("Sync/FetchLimit");
    case Key::SyncOfflineCacheMB:          return QStringLiteral("Sync/OfflineCacheMB");
    case Key::UiSidebarWidth:              return QStringLiteral("UI/SidebarWidth");
    case Key::UiMessageListWidth:          return QStringLiteral("UI/MessageListWidth");
    case Key::UiFontSizePt:               return QStringLiteral("UI/FontSizePt");
    case Key::UiMessageDensity:            return QStringLiteral("UI/MessageDensity");
    case Key::NotificationsEnabled:        return QStringLiteral("Notifications/Enabled");
    case Key::NotificationsSound:          return QStringLiteral("Notifications/Sound");
    case Key::ComposeDefaultReply:         return QStringLiteral("Compose/DefaultReply");
    case Key::PrivacyLoadRemoteImages:     return QStringLiteral("Privacy/LoadRemoteImages");
    case Key::AdvancedLogLevel:            return QStringLiteral("Advanced/LogLevel");
    case Key::AdvancedConnectionTimeoutMs: return QStringLiteral("Advanced/ConnectionTimeoutMs");
    case Key::WindowGeometry:              return QStringLiteral("Window/Geometry");
    case Key::WindowState:                 return QStringLiteral("Window/State");
    case Key::MessageSplitterState:        return QStringLiteral("Window/MessageSplitterState");
    }
    return {};
}

// ---------------------------------------------------------------------------
// Default values
// ---------------------------------------------------------------------------

QVariant Settings::defaultValue(Key key) const
{
    switch (key) {
    case Key::GeneralLanguage:             return QStringLiteral("en-US");
    case Key::GeneralTheme:                return QStringLiteral("system");
    case Key::GeneralStartMinimized:       return false;
    case Key::SyncIntervalSeconds:         return 300;
    case Key::SyncFetchLimit:              return 200;
    case Key::SyncOfflineCacheMB:          return 500;
    case Key::UiSidebarWidth:              return 220;
    case Key::UiMessageListWidth:          return 320;
    case Key::UiFontSizePt:               return 0;
    case Key::UiMessageDensity:            return QStringLiteral("normal");
    case Key::NotificationsEnabled:        return true;
    case Key::NotificationsSound:          return false;
    case Key::ComposeDefaultReply:         return QStringLiteral("reply");
    case Key::PrivacyLoadRemoteImages:     return false;
    case Key::AdvancedLogLevel:            return QStringLiteral("info");
    case Key::AdvancedConnectionTimeoutMs: return 30000;
    case Key::WindowGeometry:              return QByteArray{};
    case Key::WindowState:                 return QByteArray{};
    case Key::MessageSplitterState:        return QByteArray{};
    }
    return {};
}
