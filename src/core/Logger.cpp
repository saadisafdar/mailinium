#include "Logger.h"

#include <QDateTime>
#include <QDir>
#include <QThread>
#include <QFileInfo>

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------

Logger& Logger::instance()
{
    static Logger s_instance;
    return s_instance;
}

Logger::Logger(QObject* parent)
    : QObject(parent)
{
}

Logger::~Logger()
{
    flush();
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

void Logger::initialize(const QString& logDir, Level minLevel)
{
    QMutexLocker lock(&m_mutex);

    if (m_initialized) {
        return;
    }

    m_logDir   = logDir;
    m_minLevel = minLevel;

    QDir dir;
    dir.mkpath(logDir);

    openLogFile();
    m_initialized = true;
}

void Logger::openLogFile()
{
    // Close any previously open file
    if (m_file.isOpen()) {
        m_stream.flush();
        m_file.close();
    }

    m_file.setFileName(currentLogPath());
    m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    m_stream.setDevice(&m_file);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void Logger::debug   (const QString& module, const QString& message) { log(Level::Debug,    module, message); }
void Logger::info    (const QString& module, const QString& message) { log(Level::Info,     module, message); }
void Logger::warning (const QString& module, const QString& message) { log(Level::Warning,  module, message); }
void Logger::error   (const QString& module, const QString& message) { log(Level::Error,    module, message); }
void Logger::critical(const QString& module, const QString& message) { log(Level::Critical, module, message); }

void Logger::flush()
{
    QMutexLocker lock(&m_mutex);
    m_stream.flush();
}

// ---------------------------------------------------------------------------
// Core logging
// ---------------------------------------------------------------------------

void Logger::log(Level level, const QString& module, const QString& message)
{
    if (level < m_minLevel) {
        return;
    }

    QMutexLocker lock(&m_mutex);

    if (!m_initialized) {
        // Fallback: write to stderr before initialization completes
        QTextStream err(stderr);
        err << levelString(level) << " [" << module << "] " << message << "\n";
        return;
    }

    rotateIfNeeded();

    const QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    // Represent thread ID as a hex value for brevity
    const quintptr threadId = reinterpret_cast<quintptr>(QThread::currentThread());

    m_stream << "[" << timestamp << "]"
             << " [" << levelString(level) << "]"
             << " [0x" << QString::number(threadId, 16) << "]"
             << " [" << module << "] "
             << message
             << "\n";

    // Debug builds also echo to stderr
#ifdef MAILINIUM_DEBUG
    QTextStream err(stderr);
    err << "[" << levelString(level) << "] [" << module << "] " << message << "\n";
#endif
}

void Logger::rotateIfNeeded()
{
    // Note: called with m_mutex already held
    if (m_file.size() < MaxFileSizeBytes) {
        return;
    }

    m_stream.flush();
    m_file.close();

    m_currentFileIndex = (m_currentFileIndex + 1) % MaxFiles;
    openLogFile();

    // Truncate the new file (it may be a recycled old log)
    m_file.resize(0);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

QString Logger::levelString(Level level) const
{
    switch (level) {
    case Level::Debug:    return QStringLiteral("DEBUG");
    case Level::Info:     return QStringLiteral("INFO ");
    case Level::Warning:  return QStringLiteral("WARN ");
    case Level::Error:    return QStringLiteral("ERROR");
    case Level::Critical: return QStringLiteral("CRIT ");
    }
    return QStringLiteral("?????");
}

QString Logger::currentLogPath() const
{
    return m_logDir + QStringLiteral("/mailinium_%1.log").arg(m_currentFileIndex);
}
