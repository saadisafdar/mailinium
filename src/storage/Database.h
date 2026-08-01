#pragma once

#include <QSqlDatabase>
#include <QString>

//
// Database — manages the application's single SQLite connection and schema.
//
// Responsibilities:
//   - Open / close the SQLite file
//   - Set connection pragmas (foreign_keys, journal_mode, busy_timeout)
//   - Run schema migrations in version order on startup
//
// Usage:
//   // In Application::initializeCore():
//   if (!Database::instance().open(dataDir + "/mailinium.db")) {
//       // handle failure
//   }
//
//   // In a store method:
//   QSqlDatabase db = Database::connection();
//   QSqlQuery q(db);
//   q.prepare("SELECT ...");
//
// Thread safety:
//   The underlying SQLite connection is single-threaded (WAL mode, single
//   writer). All database operations must run on the thread that called open().
//   For Phase 3 this is always the UI thread. A dedicated DB thread is added
//   in Phase 3D when background sync starts.
//
class Database
{
    Q_DISABLE_COPY_MOVE(Database)

public:
    static Database& instance();

    // Returns the active QSqlDatabase for use by store classes.
    // Do not cache the returned object across calls.
    static QSqlDatabase connection();

    // Opens (or creates) the database file at filePath.
    // Runs any pending migrations before returning.
    // Returns false if the database cannot be opened or any migration fails.
    bool open(const QString& filePath);

    void close();
    bool isOpen() const;

private:
    Database();
    ~Database();

    bool createMigrationsTable();
    bool runMigrations();
    bool runMigration(int version, const QString& description, const QStringList& statements);
    int  appliedMigrationVersion();

    QString m_connectionName;
};
