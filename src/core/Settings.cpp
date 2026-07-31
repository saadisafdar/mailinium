#include "Settings.h"

#include <QSettings>
#include <QDir>
#include <QHash>

// ---------------------------------------------------------------------------
// Private implementation (pimpl idiom — insulates consumers from QSettings)
// ---------------------------------------------------------------------------

struct Settings::Private {
    QSettings qs;
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

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

void Settings::initialize(const QString& dataDir)
{
    QDir dir;
    dir.mkpath(dataDir);

    // Re-point QSettings at our chosen file location
    d->qs = QSettings(dataDir + QStringLiteral("/settings.ini"), QSettings::IniFormat);

    applyDefaults();
}

void Settings::applyDefaults()
{
    // Write default values for any key that has not been set yet.
    // This means first-run defaults are always present without explicit checks
    // at every call site.
    const auto keys = {
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
    };

    for (Key key : keys) {
        const QString k = keyString(key);
        if (!d->qs.contains(k)) {
            d->qs.setValue(k, defaultValue(key));
        }
    }

    d->qs.sync();
}

// ---------------------------------------------------------------------------
// Access
// ---------------------------------------------------------------------------

QVariant Settings::value(Key key) const
{
    return d->qs.value(keyString(key), defaultValue(key));
}

void Settings::setValue(Key key, const QVariant& val)
{
    d->qs.setValue(keyString(key), val);
    emit settingChanged(key, val);
}

bool    Settings::boolValue  (Key key) const { return value(key).toBool();   }
int     Settings::intValue   (Key key) const { return value(key).toInt();    }
QString Settings::stringValue(Key key) const { return value(key).toString(); }

void Settings::sync()
{
    d->qs.sync();
}

// ---------------------------------------------------------------------------
// Key → string mapping (INI file keys)
// ---------------------------------------------------------------------------

QString Settings::keyString(Key key) const
{
    switch (key) {
    case Key::GeneralLanguage:           return QStringLiteral("General/Language");
    case Key::GeneralTheme:              return QStringLiteral("General/Theme");
    case Key::GeneralStartMinimized:     return QStringLiteral("General/StartMinimized");
    case Key::SyncIntervalSeconds:       return QStringLiteral("Sync/IntervalSeconds");
    case Key::SyncFetchLimit:            return QStringLiteral("Sync/FetchLimit");
    case Key::SyncOfflineCacheMB:        return QStringLiteral("Sync/OfflineCacheMB");
    case Key::UiSidebarWidth:            return QStringLiteral("UI/SidebarWidth");
    case Key::UiMessageListWidth:        return QStringLiteral("UI/MessageListWidth");
    case Key::UiFontSizePt:              return QStringLiteral("UI/FontSizePt");
    case Key::UiMessageDensity:          return QStringLiteral("UI/MessageDensity");
    case Key::NotificationsEnabled:      return QStringLiteral("Notifications/Enabled");
    case Key::NotificationsSound:        return QStringLiteral("Notifications/Sound");
    case Key::ComposeDefaultReply:       return QStringLiteral("Compose/DefaultReply");
    case Key::PrivacyLoadRemoteImages:   return QStringLiteral("Privacy/LoadRemoteImages");
    case Key::AdvancedLogLevel:          return QStringLiteral("Advanced/LogLevel");
    case Key::AdvancedConnectionTimeoutMs: return QStringLiteral("Advanced/ConnectionTimeoutMs");
    case Key::WindowGeometry:             return QStringLiteral("Window/Geometry");
    case Key::WindowState:                return QStringLiteral("Window/State");
    case Key::MessageSplitterState:       return QStringLiteral("Window/MessageSplitterState");
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
    case Key::SyncIntervalSeconds:         return 300;   // 5 minutes
    case Key::SyncFetchLimit:              return 200;
    case Key::SyncOfflineCacheMB:          return 500;
    case Key::UiSidebarWidth:              return 220;
    case Key::UiMessageListWidth:          return 320;
    case Key::UiFontSizePt:               return 0;     // 0 = follow system font
    case Key::UiMessageDensity:            return QStringLiteral("normal");
    case Key::NotificationsEnabled:        return true;
    case Key::NotificationsSound:          return false;
    case Key::ComposeDefaultReply:         return QStringLiteral("reply");
    case Key::PrivacyLoadRemoteImages:     return false; // secure default
    case Key::AdvancedLogLevel:            return QStringLiteral("info");
    case Key::AdvancedConnectionTimeoutMs: return 30000; // 30 seconds
    case Key::WindowGeometry:              return QByteArray{};
    case Key::WindowState:                 return QByteArray{};
    case Key::MessageSplitterState:        return QByteArray{};
    }
    return {};
}
