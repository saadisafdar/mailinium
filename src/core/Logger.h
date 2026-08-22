#pragma once

#include <QObject>
#include <QString>
#include <QMutex>
#include <QFile>
#include <QTextStream>

//
// Logger — thread-safe, rotating-file application logger.
//
// Usage:
//   Logger::instance().info("core", "Application started");
//   Logger::instance().warning("sync", "Connection timeout, retrying...");
//
// Log format:
//   [2025-01-15T14:23:01.123+05:00] [INFO ] [0x1234] [core] Application started
//
// PII policy: never log full email addresses or any secret material.
// Rotate: up to MaxFiles files of MaxFileSizeBytes each.
//
class Logger
{
public:
    enum class Level {
        Debug   = 0,
        Info    = 1,
        Warning = 2,
        Error   = 3,
        Critical = 4,
    };

    static Logger& instance();

    // Initialize must be called once before any logging.
    // logDir: directory where log files are written.
    // minLevel: messages below this level are silently discarded.
    void initialize(const QString& logDir, Level minLevel = Level::Info);

    void debug   (const QString& module, const QString& message);
    void info    (const QString& module, const QString& message);
    void warning (const QString& module, const QString& message);
    void error   (const QString& module, const QString& message);
    void critical(const QString& module, const QString& message);

    // Flush all pending writes. Called on application shutdown.
    void flush();

    // Log inspection & export
    QString logDir() const;
    QStringList allLogFiles() const;
    bool exportLogs(const QString& destinationPath);

private:
    Logger();
    ~Logger();

    void log(Level level, const QString& module, const QString& message);
    void rotateIfNeeded();
    QString levelString(Level level) const;
    QString currentLogPath() const;
    void openLogFile();

    static constexpr qint64 MaxFileSizeBytes = 5 * 1024 * 1024; // 5 MB
    static constexpr int    MaxFiles         = 3;

    mutable QMutex  m_mutex;
    QFile           m_file;
    QTextStream     m_stream;
    QString         m_logDir;
    Level           m_minLevel = Level::Info;
    int             m_currentFileIndex = 0;
    bool            m_initialized = false;
};
