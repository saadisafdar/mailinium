#include "Logger.h"

#include <QDateTime>
#include <QDir>
#include <QThread>

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
    if (!dir.mkpath(logDir)) {
        // Cannot create log directory. Log to stderr and carry on —
        // subsequent file opens will fail gracefully.
        QTextStream(stderr) << "Mailinium: could not create log directory: " << logDir << "\n";
    }

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

    // open() returns bool; we check it and fall back to stderr on failure.
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream(stderr) << "Mailinium: could not open log file: "
                            << currentLogPath() << "\n";
        m_stream.setDevice(nullptr);
        return;
    }

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
        QTextStream(stderr) << levelString(level) << " [" << module << "] " << message << "\n";
        return;
    }

    rotateIfNeeded();

    const QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    const quintptr threadId = reinterpret_cast<quintptr>(QThread::currentThread());

    m_stream << "[" << timestamp << "]"
             << " [" << levelString(level) << "]"
             << " [0x" << QString::number(threadId, 16) << "]"
             << " [" << module << "] "
             << message
             << "\n";

    if (message.contains(QStringLiteral("[PIPELINE]"))) {
        QFile pipelineFile(QStringLiteral("pipeline.log"));
        if (pipelineFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&pipelineFile);
            out << "[" << timestamp << "] " << message << "\n";
        }
    }

#ifdef MAILINIUM_DEBUG
    QTextStream(stderr) << "[" << levelString(level) << "] [" << module << "] " << message << "\n";
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

    // Truncate the recycled log file. resize() returns bool — checked here.
    if (m_file.isOpen() && !m_file.resize(0)) {
        // Non-fatal: the file will just keep old content until overwritten.
        QTextStream(stderr) << "Mailinium: could not truncate log file\n";
    }
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
