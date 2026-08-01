#include <QSqlQuery>
#include <QStringList>
#include <QTest>
#include <QSqlError>

#include "storage/Database.h"

//
// DatabaseTest — verifies that:
//   1. An in-memory database opens without error
//   2. All expected tables exist after open()
//   3. Migration 1 is recorded in schema_migrations
//   4. A second open() call on the same instance reopens cleanly
//
class DatabaseTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY2(Database::instance().open(QStringLiteral(":memory:")),
                 "Database::open() returned false for :memory:");
    }

    void cleanupTestCase()
    {
        Database::instance().close();
        QVERIFY(!Database::instance().isOpen());
    }

    void isOpen_returnsTrue()
    {
        QVERIFY(Database::instance().isOpen());
    }

    void allExpectedTablesExist()
    {
        const QStringList expected = {
            QStringLiteral("schema_migrations"),
            QStringLiteral("accounts"),
            QStringLiteral("folders"),
            QStringLiteral("messages"),
        };

        QSqlQuery q(Database::connection());
        QVERIFY2(q.exec(QStringLiteral(
            "SELECT name FROM sqlite_master WHERE type = 'table'")),
            qUtf8Printable(q.lastError().text()));

        QStringList found;
        while (q.next()) {
            found << q.value(0).toString();
        }

        for (const QString& table : expected) {
            QVERIFY2(found.contains(table),
                     qUtf8Printable("Table missing: " + table));
        }
    }

    void ftsTableExists()
    {
        QSqlQuery q(Database::connection());
        QVERIFY(q.exec(QStringLiteral(
            "SELECT name FROM sqlite_master"
            "    WHERE type = 'table' AND name = 'messages_fts'")));
        QVERIFY2(q.next(), "messages_fts virtual table not found");
    }

    void indexesExist()
    {
        QSqlQuery q(Database::connection());
        QVERIFY(q.exec(QStringLiteral(
            "SELECT name FROM sqlite_master WHERE type = 'index'")));

        QStringList indexes;
        while (q.next()) {
            indexes << q.value(0).toString();
        }

        QVERIFY2(indexes.contains(QStringLiteral("idx_messages_folder_date")),
                 "idx_messages_folder_date missing");
        QVERIFY2(indexes.contains(QStringLiteral("idx_messages_thread")),
                 "idx_messages_thread missing");
    }

    void migrationVersion_isTwo()
    {
        QSqlQuery q(Database::connection());
        QVERIFY(q.exec(QStringLiteral(
            "SELECT MAX(version) FROM schema_migrations")));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 2);
    }

    void foreignKeysEnabled()
    {
        // Verify that the PRAGMA foreign_keys = ON took effect.
        QSqlQuery q(Database::connection());
        QVERIFY(q.exec(QStringLiteral("PRAGMA foreign_keys")));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 1);
    }

    void secondOpenReopensCleanly()
    {
        // close() + open() on the same singleton should work without errors.
        Database::instance().close();
        QVERIFY(!Database::instance().isOpen());

        QVERIFY(Database::instance().open(QStringLiteral(":memory:")));
        QVERIFY(Database::instance().isOpen());
    }
};

QTEST_MAIN(DatabaseTest)
#include "DatabaseTest.moc"
