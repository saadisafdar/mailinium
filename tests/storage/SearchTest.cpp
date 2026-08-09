#include <QSqlQuery>
#include <QTest>
#include <QTimeZone>

#include "storage/Database.h"
#include "storage/FolderStore.h"
#include "storage/Message.h"
#include "storage/MessageStore.h"

static qint64 insertTestAccount(const QString& email)
{
    QSqlQuery q(Database::connection());
    q.prepare(QStringLiteral(
        "INSERT INTO accounts(email, display_name, created_at)"
        "    VALUES(:email, 'Search Test', '2026-01-01T00:00:00Z')"));
    q.bindValue(QStringLiteral(":email"), email);
    if (!q.exec()) return -1;
    return q.lastInsertId().toLongLong();
}

static qint64 insertTestFolder(qint64 accountId, const QString& name)
{
    FolderStore fs;
    Folder f;
    f.accountId = accountId;
    f.name      = name;
    f.role      = QStringLiteral("inbox");
    return fs.insertFolder(f);
}

class SearchTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(Database::instance().open(QStringLiteral(":memory:")));
    }

    void cleanupTestCase()
    {
        Database::instance().close();
    }

    void testSearchAndTriggers()
    {
        qint64 accId1 = insertTestAccount(QStringLiteral("user1@example.com"));
        qint64 accId2 = insertTestAccount(QStringLiteral("user2@example.com"));
        QVERIFY(accId1 > 0);
        QVERIFY(accId2 > 0);

        qint64 folderId1 = insertTestFolder(accId1, QStringLiteral("INBOX"));
        qint64 folderId2 = insertTestFolder(accId2, QStringLiteral("Work"));
        QVERIFY(folderId1 > 0);
        QVERIFY(folderId2 > 0);

        MessageStore store;

        // 1. Test insertion triggering FTS indexing
        Message m1;
        m1.folderId = folderId1;
        m1.uid = 101;
        m1.subject = QStringLiteral("Important security update");
        m1.fromAddress = QStringLiteral("admin@secure.net");
        m1.fromName = QStringLiteral("Admin Team");
        m1.date = QDateTime::currentDateTimeUtc();
        qint64 id1 = store.insertMessage(m1);
        QVERIFY(id1 > 0);

        Message m2;
        m2.folderId = folderId2;
        m2.uid = 202;
        m2.subject = QStringLiteral("Weekly newsletter");
        m2.fromAddress = QStringLiteral("news@daily.com");
        m2.fromName = QStringLiteral("Daily News");
        m2.date = QDateTime::currentDateTimeUtc();
        qint64 id2 = store.insertMessage(m2);
        QVERIFY(id2 > 0);

        // Search globally
        QList<Message> results = store.searchMessages(QStringLiteral("security"));
        QCOMPARE(results.size(), 1);
        QCOMPARE(results.first().id, id1);

        results = store.searchMessages(QStringLiteral("news"));
        QCOMPARE(results.size(), 1);
        QCOMPARE(results.first().id, id2);

        // Search by accountId filter
        results = store.searchMessages(QStringLiteral("newsletter"), accId2, -1);
        QCOMPARE(results.size(), 1);
        results = store.searchMessages(QStringLiteral("newsletter"), accId1, -1);
        QCOMPARE(results.size(), 0);

        // Search by folderId filter
        results = store.searchMessages(QStringLiteral("security"), -1, folderId1);
        QCOMPARE(results.size(), 1);
        results = store.searchMessages(QStringLiteral("security"), -1, folderId2);
        QCOMPARE(results.size(), 0);

        // 2. Test update triggering FTS indexing update
        m1.id = id1;
        m1.subject = QStringLiteral("Emergency protocol updated");
        QVERIFY(store.updateFlags(id1, 1)); // update some flags, triggers shouldn't break

        // We update the subject manually to test update trigger
        QSqlQuery q(Database::connection());
        q.prepare(QStringLiteral("UPDATE messages SET subject = :subj WHERE id = :id"));
        q.bindValue(QStringLiteral(":subj"), m1.subject);
        q.bindValue(QStringLiteral(":id"), id1);
        QVERIFY(q.exec());

        // Verify old search term fails and new one succeeds
        results = store.searchMessages(QStringLiteral("security"));
        QCOMPARE(results.size(), 0);
        results = store.searchMessages(QStringLiteral("emergency"));
        QCOMPARE(results.size(), 1);
        QCOMPARE(results.first().id, id1);

        // 3. Test delete triggering FTS deletion
        QVERIFY(store.deleteMessage(id1));
        results = store.searchMessages(QStringLiteral("emergency"));
        QCOMPARE(results.size(), 0);
    }

    void testSearchEdgeCases()
    {
        qint64 accId = insertTestAccount(QStringLiteral("edge@example.com"));
        qint64 folderId = insertTestFolder(accId, QStringLiteral("INBOX"));

        MessageStore store;
        Message m;
        m.folderId = folderId;
        m.uid = 999;
        m.subject = QStringLiteral("Hello (World): A-Test message");
        m.fromAddress = QStringLiteral("sender@example.com");
        m.date = QDateTime::currentDateTimeUtc();
        qint64 mid = store.insertMessage(m);
        QVERIFY(mid > 0);

        // Empty query returns nothing
        QList<Message> results = store.searchMessages(QStringLiteral(""));
        QCOMPARE(results.size(), 0);

        // Whitespace-only query returns nothing
        results = store.searchMessages(QStringLiteral("   "));
        QCOMPARE(results.size(), 0);

        // Special FTS5 chars must not crash (sanitized as phrase)
        results = store.searchMessages(QStringLiteral("Hello (World)"));
        QVERIFY(results.size() >= 0); // must not crash, may or may not match

        results = store.searchMessages(QStringLiteral("Hello:"));
        QVERIFY(results.size() >= 0);

        results = store.searchMessages(QStringLiteral("A-Test"));
        QVERIFY(results.size() >= 0);

        // Normal prefix search should work
        results = store.searchMessages(QStringLiteral("Hell"));
        QCOMPARE(results.size(), 1);
        QCOMPARE(results.first().id, mid);
    }
};

QTEST_MAIN(SearchTest)
#include "SearchTest.moc"
