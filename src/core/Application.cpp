#include "Application.h"
#include "Logger.h"
#include "Settings.h"
#include "ThemeManager.h"
#include "../storage/Database.h"
#include "../ui/MainWindow.h"

#include <QStandardPaths>
#include <QDir>

// ---------------------------------------------------------------------------
// Constructor — strict initialization order:
//   1. QApplication metadata
//   2. Data directory resolution
//   3. Logger
//   4. Settings
//   5. ThemeManager (reads Settings, must run before any UI is created)
//   6. UI
// ---------------------------------------------------------------------------

Application::Application(int& argc, char** argv)
    : QApplication(argc, argv)
{
    // Qt application metadata — used by QSettings, window title, etc.
    setApplicationName(QStringLiteral("Mailinium"));
    setApplicationDisplayName(QStringLiteral("Mailinium"));
    setApplicationVersion(QStringLiteral("0.1.0"));
    setOrganizationName(QStringLiteral("Mailinium"));
    setOrganizationDomain(QStringLiteral("mailinium.app"));

    connect(this, &QApplication::aboutToQuit, this, &Application::onAboutToQuit);

    initializeCore();
    initializeUi();
}

Application::~Application() = default;

// ---------------------------------------------------------------------------
// Core initialization (no UI)
// ---------------------------------------------------------------------------

void Application::initializeCore()
{
    const QString dataDir = dataDirectory();

    // Logger must be first — everything after this can log.
    const QString logDir = dataDir + QStringLiteral("/logs");
#ifdef MAILINIUM_DEBUG
    Logger::instance().initialize(logDir, Logger::Level::Debug);
#else
    Logger::instance().initialize(logDir, Logger::Level::Info);
#endif

    Logger::instance().info(QStringLiteral("core"), QStringLiteral("Mailinium starting — version 0.1.0"));
    Logger::instance().info(QStringLiteral("core"), QStringLiteral("Data directory: ") + dataDir);

    Settings::instance().initialize(dataDir);
    Logger::instance().info(QStringLiteral("core"), QStringLiteral("Settings loaded"));

    const QString dbPath = dataDir + QStringLiteral("/mailinium.db");
    if (!Database::instance().open(dbPath)) {
        Logger::instance().critical(QStringLiteral("core"),
            QStringLiteral("Failed to open database — storage is unavailable"));
        // Continue without storage for now; Phase 3B will surface this to the user.
    }

    ThemeManager::instance().initialize();
    Logger::instance().info(QStringLiteral("core"), QStringLiteral("Theme applied"));
}

// ---------------------------------------------------------------------------
// UI initialization
// ---------------------------------------------------------------------------

void Application::initializeUi()
{
    m_mainWindow = new MainWindow();
    m_mainWindow->show();
    Logger::instance().info(QStringLiteral("ui"), QStringLiteral("Main window shown"));
}

// ---------------------------------------------------------------------------
// Shutdown
// ---------------------------------------------------------------------------

void Application::onAboutToQuit()
{
    Logger::instance().info(QStringLiteral("core"), QStringLiteral("Mailinium shutting down"));
    Database::instance().close();
    Settings::instance().sync();
    Logger::instance().flush();
}

// ---------------------------------------------------------------------------
// Platform data directory
// ---------------------------------------------------------------------------

QString Application::dataDirectory() const
{
    // QStandardPaths::AppDataLocation resolves to:
    //   Windows: %APPDATA%\Mailinium
    //   macOS:   ~/Library/Application Support/Mailinium
    //   Linux:   ~/.local/share/mailinium
    const QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!QDir().mkpath(path)) {
        qWarning("Mailinium: could not create data directory: %ls", qUtf16Printable(path));
    }
    return path;
}
