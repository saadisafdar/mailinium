#include "Database.h"
#include "../core/Logger.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>

// ---------------------------------------------------------------------------
// Connection name — known to both Database and store classes via connection()
// ---------------------------------------------------------------------------

static const QString s_connectionName = QStringLiteral("mailinium");

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------

Database& Database::instance()
{
    static Database s_instance;
    return s_instance;
}

Database::Database()
    : m_connectionName(s_connectionName)
{
}

Database::~Database()
{
    close();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

QSqlDatabase Database::connection()
{
    return QSqlDatabase::database(s_connectionName);
}

bool Database::open(const QString& filePath)
{
    // Remove any stale connection (e.g. from a previous open() or test run)
    if (QSqlDatabase::contains(m_connectionName)) {
        QSqlDatabase::removeDatabase(m_connectionName);
    }

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(filePath);

    if (!db.open()) {
        Logger::instance().error(QStringLiteral("db"),
            QStringLiteral("Failed to open database at '") + filePath
            + QStringLiteral("': ") + db.lastError().text());
        return false;
    }

    // Apply connection pragmas.
    // Each pragma is executed individually — QSqlQuery::exec() handles one
    // statement at a time.
    const QStringList pragmas = {
        QStringLiteral("PRAGMA foreign_keys  = ON"),      // enforce FK constraints
        QStringLiteral("PRAGMA journal_mode  = WAL"),     // write-ahead logging
        QStringLiteral("PRAGMA synchronous   = NORMAL"),  // safe with WAL
        QStringLiteral("PRAGMA busy_timeout  = 5000"),    // ms to wait on lock
        QStringLiteral("PRAGMA cache_size    = -8000"),   // 8 MB page cache
    };

    for (const QString& pragma : pragmas) {
        QSqlQuery q(db);
        if (!q.exec(pragma)) {
            // Pragmas are best-effort — log the failure but continue.
            Logger::instance().warning(QStringLiteral("db"),
                QStringLiteral("Pragma failed: ") + pragma
                + QStringLiteral(" — ") + q.lastError().text());
        }
    }

    Logger::instance().info(QStringLiteral("db"),
        QStringLiteral("Database opened: ") + filePath);

    return runMigrations();
}

void Database::close()
{
    if (!QSqlDatabase::contains(m_connectionName)) {
        return;
    }

    // QSqlDatabase objects must all go out of scope before removeDatabase().
    // The extra block forces destruction of `db` before the remove call.
    {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName, /*open=*/false);
        db.close();
    }

    QSqlDatabase::removeDatabase(m_connectionName);
    Logger::instance().info(QStringLiteral("db"), QStringLiteral("Database closed"));
}

bool Database::isOpen() const
{
    if (!QSqlDatabase::contains(m_connectionName)) {
        return false;
    }
    return QSqlDatabase::database(m_connectionName, /*open=*/false).isOpen();
}

// ---------------------------------------------------------------------------
// Migration runner
// ---------------------------------------------------------------------------

bool Database::createMigrationsTable()
{
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    const bool ok = q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS schema_migrations ("
        "    version    INTEGER PRIMARY KEY,"
        "    applied_at TEXT NOT NULL"
        ")"
    ));

    if (!ok) {
        Logger::instance().error(QStringLiteral("db"),
            QStringLiteral("Failed to create schema_migrations table: ")
            + q.lastError().text());
    }

    return ok;
}

int Database::appliedMigrationVersion()
{
    QSqlQuery q(QSqlDatabase::database(m_connectionName));

    if (!q.exec(QStringLiteral(
            "SELECT COALESCE(MAX(version), 0) FROM schema_migrations"))) {
        return 0;
    }

    return q.next() ? q.value(0).toInt() : 0;
}

bool Database::runMigration(int version,
                            const QString& description,
                            const QStringList& statements)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);

    if (!db.transaction()) {
        Logger::instance().error(QStringLiteral("db"),
            QStringLiteral("Cannot begin transaction for migration ")
            + QString::number(version));
        return false;
    }

    for (const QString& sql : statements) {
        QSqlQuery q(db);
        if (!q.exec(sql)) {
            Logger::instance().error(QStringLiteral("db"),
                QStringLiteral("Migration ") + QString::number(version)
                + QStringLiteral(" failed on statement:\n") + sql
                + QStringLiteral("\nError: ") + q.lastError().text());
            db.rollback();
            return false;
        }
    }

    // Record this migration as applied
    QSqlQuery record(db);
    record.prepare(QStringLiteral(
        "INSERT INTO schema_migrations(version, applied_at) VALUES(?, ?)"));
    record.addBindValue(version);
    record.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    if (!record.exec()) {
        Logger::instance().error(QStringLiteral("db"),
            QStringLiteral("Failed to record migration ")
            + QString::number(version) + QStringLiteral(": ")
            + record.lastError().text());
        db.rollback();
        return false;
    }

    if (!db.commit()) {
        Logger::instance().error(QStringLiteral("db"),
            QStringLiteral("Failed to commit migration ")
            + QString::number(version));
        db.rollback();
        return false;
    }

    Logger::instance().info(QStringLiteral("db"),
        QStringLiteral("Applied migration ") + QString::number(version)
        + QStringLiteral(": ") + description);

    return true;
}

// ---------------------------------------------------------------------------
// Schema migrations
//
// Rules for adding a migration:
//   1. Never modify an existing migration — add a new one.
//   2. Each statement must be complete and executable in isolation.
//   3. All DDL runs inside a transaction; if any statement fails the whole
//      migration rolls back and the application will refuse to start.
//   4. Increment the version number by 1 each time.
// ---------------------------------------------------------------------------

bool Database::runMigrations()
{
    if (!createMigrationsTable()) {
        return false;
    }

    const int current = appliedMigrationVersion();

    // -----------------------------------------------------------------------
    // Migration 1 — Initial schema
    // -----------------------------------------------------------------------
    if (current < 1) {
        const QStringList statements = {

            // Accounts — one row per configured email account.
            // AccountStore (Phase 3B) owns all CRUD for this table.
            QStringLiteral(
                "CREATE TABLE accounts ("
                "    id           INTEGER PRIMARY KEY,"
                "    email        TEXT    NOT NULL,"
                "    display_name TEXT    NOT NULL DEFAULT '',"
                "    provider     TEXT    NOT NULL DEFAULT '',"
                "    imap_host    TEXT    NOT NULL DEFAULT '',"
                "    imap_port    INTEGER NOT NULL DEFAULT 993,"
                "    smtp_host    TEXT    NOT NULL DEFAULT '',"
                "    smtp_port    INTEGER NOT NULL DEFAULT 587,"
                "    created_at   TEXT    NOT NULL"
                ")"
            ),

            // Folders — IMAP mailboxes belonging to an account.
            // UNIQUE(account_id, name) prevents duplicate folder names per account.
            QStringLiteral(
                "CREATE TABLE folders ("
                "    id            INTEGER PRIMARY KEY,"
                "    account_id    INTEGER NOT NULL REFERENCES accounts(id) ON DELETE CASCADE,"
                "    name          TEXT    NOT NULL,"
                "    role          TEXT    NOT NULL DEFAULT '',"
                "    uidvalidity   INTEGER NOT NULL DEFAULT 0,"
                "    highestmodseq INTEGER NOT NULL DEFAULT 0,"
                "    UNIQUE(account_id, name)"
                ")"
            ),

            // Messages — one row per envelope (header data).
            // Body content is stored in body_parts (added in Phase 3E).
            // UNIQUE(folder_id, uid) ensures one row per IMAP UID per folder.
            QStringLiteral(
                "CREATE TABLE messages ("
                "    id           INTEGER PRIMARY KEY,"
                "    folder_id    INTEGER NOT NULL REFERENCES folders(id) ON DELETE CASCADE,"
                "    uid          INTEGER NOT NULL,"
                "    message_id   TEXT    NOT NULL DEFAULT '',"
                "    thread_id    INTEGER NOT NULL DEFAULT 0,"
                "    subject      TEXT    NOT NULL DEFAULT '',"
                "    from_address TEXT    NOT NULL DEFAULT '',"
                "    from_name    TEXT    NOT NULL DEFAULT '',"
                "    date_utc     TEXT    NOT NULL DEFAULT '',"
                "    flags        INTEGER NOT NULL DEFAULT 0,"
                "    size         INTEGER NOT NULL DEFAULT 0,"
                "    body_fetched INTEGER NOT NULL DEFAULT 0,"
                "    UNIQUE(folder_id, uid)"
                ")"
            ),

            // Index: fetch messages for a folder sorted newest-first (most common query)
            QStringLiteral(
                "CREATE INDEX idx_messages_folder_date"
                "    ON messages(folder_id, date_utc DESC)"
            ),

            // Index: thread grouping queries
            QStringLiteral(
                "CREATE INDEX idx_messages_thread"
                "    ON messages(thread_id)"
            ),

            // FTS5 full-text search index.
            // Standalone table — triggers and population added in Phase 3H.
            // Creating the table now so no schema migration is needed when search arrives.
            QStringLiteral(
                "CREATE VIRTUAL TABLE messages_fts USING fts5("
                "    subject,"
                "    from_address,"
                "    from_name,"
                "    tokenize = 'unicode61'"
                ")"
            ),
        };

        if (!runMigration(1, QStringLiteral("Initial schema"), statements)) {
            return false;
        }
    }

    // -----------------------------------------------------------------------
    // Migration 2 — Add password to accounts
    // -----------------------------------------------------------------------
    if (current < 2) {
        const QStringList statements = {
            QStringLiteral(
                "ALTER TABLE accounts ADD COLUMN password TEXT NOT NULL DEFAULT ''"
            )
        };

        if (!runMigration(2, QStringLiteral("Add password to accounts"), statements)) {
            return false;
        }
    }

    // Future migrations: add new `if (current < N)` blocks here.
    // Never modify a migration that has already been applied.

    return true;
}
