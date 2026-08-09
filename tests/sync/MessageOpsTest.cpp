#include <QTest>
#include <QSignalSpy>
#include <QSqlQuery>
#include "core/Settings.h"
#include "core/SyncWorker.h"
#include "storage/Account.h"
#include "storage/AccountStore.h"
#include "storage/Database.h"
#include "storage/FolderStore.h"
#include "storage/MessageStore.h"

static qint64 insertTestAccount()
{
    QSqlQuery q(Database::connection());
    q.prepare(QStringLiteral(
        "INSERT INTO accounts(email, display_name, created_at)"
        "    VALUES('ops-test@example.com', 'Ops Test', '2026-01-01T00:00:00Z')"));
    if (!q.exec()) return -1;
    return q.lastInsertId().toLongLong();
}

static Folder insertTestFolder(qint64 accountId, const QString& name, const QString& role)
{
    FolderStore fs;
    Folder f;
    f.accountId = accountId;
    f.name      = name;
    f.role      = role;
    f.id = fs.insertFolder(f);
    return f;
}

static Message makeTestMessage(qint64 folderId, qint64 uid)
{
    Message m;
    m.folderId    = folderId;
    m.uid         = uid;
    m.messageId   = QStringLiteral("<") + QString::number(uid) + QStringLiteral("@ops-test>");
    m.subject     = QStringLiteral("Test Subject ") + QString::number(uid);
    m.fromAddress = QStringLiteral("sender@example.com");
    m.fromName    = QStringLiteral("Test Sender");
    m.date        = QDateTime::currentDateTimeUtc();
    m.flags       = 0;
    m.size        = 512;
    return m;
}

class MessageOpsTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(Database::instance().open(QStringLiteral(":memory:")));
        Settings::instance().initialize(QStringLiteral("."));
    }

    void cleanupTestCase()
    {
        Database::instance().close();
    }

    void testDeleteAndRollback()
    {
        qint64 accId = insertTestAccount();
        QVERIFY(accId > 0);

        Folder inbox = insertTestFolder(accId, QStringLiteral("INBOX"), QStringLiteral("inbox"));
        Folder trash = insertTestFolder(accId, QStringLiteral("Trash"), QStringLiteral("trash"));
        QVERIFY(inbox.id > 0);
        QVERIFY(trash.id > 0);

        Account acc;
        acc.id = accId;
        acc.email = QStringLiteral("ops-test@example.com");
        
        SyncWorker worker(acc);

        MessageStore store;
        Message msg = makeTestMessage(inbox.id, 1001);
        qint64 msgId = store.insertMessage(msg);
        QVERIFY(msgId > 0);

        // 1. Delete message (optimistic UI update hides it by setting Deleted flag)
        worker.deleteMessage(inbox.id, msgId);

        auto optMsg = store.messageById(msgId);
        QVERIFY(optMsg.has_value());
        // Verify Deleted flag is set
        QVERIFY((optMsg->flags & MessageFlags::Deleted) != 0);

        // 2. Trigger IMAP failure (should trigger rollback)
        QMetaObject::invokeMethod(&worker, "onClientError", Q_ARG(QString, QStringLiteral("Connection timed out")));

        optMsg = store.messageById(msgId);
        QVERIFY(optMsg.has_value());
        // Verify Deleted flag is cleared
        QVERIFY((optMsg->flags & MessageFlags::Deleted) == 0);
    }

    void testMarkReadAndRollback()
    {
        Account acc;
        acc.email = QStringLiteral("ops-test2@example.com");
        acc.id = insertTestAccount();
        QVERIFY(acc.id > 0);

        Folder inbox = insertTestFolder(acc.id, QStringLiteral("INBOX2"), QStringLiteral("inbox"));
        QVERIFY(inbox.id > 0);

        SyncWorker worker(acc);

        MessageStore store;
        Message msg = makeTestMessage(inbox.id, 2002);
        msg.flags = 0; // Unread
        qint64 msgId = store.insertMessage(msg);
        QVERIFY(msgId > 0);

        // 1. Mark Read (optimistic flags change)
        worker.markMessageRead(inbox.id, msgId, true);

        auto optMsg = store.messageById(msgId);
        QVERIFY(optMsg.has_value());
        QVERIFY((optMsg->flags & MessageFlags::Seen) != 0);

        // 2. Trigger IMAP failure (should rollback to unread)
        QMetaObject::invokeMethod(&worker, "onClientError", Q_ARG(QString, QStringLiteral("Server rejected command")));

        optMsg = store.messageById(msgId);
        QVERIFY(optMsg.has_value());
        QVERIFY((optMsg->flags & MessageFlags::Seen) == 0);
    }

    void testMoveAndRollback()
    {
        Account acc;
        acc.email = QStringLiteral("ops-test3@example.com");
        acc.id = insertTestAccount();
        QVERIFY(acc.id > 0);

        Folder src = insertTestFolder(acc.id, QStringLiteral("Source"), QStringLiteral(""));
        Folder dest = insertTestFolder(acc.id, QStringLiteral("Destination"), QStringLiteral(""));
        QVERIFY(src.id > 0);
        QVERIFY(dest.id > 0);

        SyncWorker worker(acc);

        MessageStore store;
        Message msg = makeTestMessage(src.id, 3003);
        qint64 msgId = store.insertMessage(msg);
        QVERIFY(msgId > 0);

        // 1. Move message (optimistic relocate to dest folder with temporary UID)
        worker.moveMessage(src.id, dest.id, msgId);

        auto optMsg = store.messageById(msgId);
        QVERIFY(optMsg.has_value());
        QCOMPARE(optMsg->folderId, dest.id);
        QCOMPARE(optMsg->uid, -1 * msgId); // Temporary UID

        // 2. Trigger IMAP failure (should rollback to src folder with original UID)
        QMetaObject::invokeMethod(&worker, "onClientError", Q_ARG(QString, QStringLiteral("Move failed")));

        optMsg = store.messageById(msgId);
        QVERIFY(optMsg.has_value());
        QCOMPARE(optMsg->folderId, src.id);
        QCOMPARE(optMsg->uid, (qint64)3003); // Original UID
    }

    void testMoveSuccess()
    {
        Account acc;
        acc.email = QStringLiteral("ops-test-move-success@example.com");
        acc.id = insertTestAccount();
        QVERIFY(acc.id > 0);

        Folder src = insertTestFolder(acc.id, QStringLiteral("Source"), QStringLiteral(""));
        Folder dest = insertTestFolder(acc.id, QStringLiteral("Destination"), QStringLiteral(""));
        QVERIFY(src.id > 0);
        QVERIFY(dest.id > 0);

        SyncWorker worker(acc);

        MessageStore store;
        Message msg = makeTestMessage(src.id, 4004);
        qint64 msgId = store.insertMessage(msg);
        QVERIFY(msgId > 0);

        // 1. Move message
        worker.moveMessage(src.id, dest.id, msgId);

        // Verify optimistic UI update
        auto optMsg = store.messageById(msgId);
        QVERIFY(optMsg.has_value());
        QCOMPARE(optMsg->folderId, dest.id);
        QCOMPARE(optMsg->uid, -1 * msgId);

        // 2. Trigger IMAP success
        QMetaObject::invokeMethod(&worker, "onMessagesMoved", Q_ARG(QList<qint64>, QList<qint64>{4004}));

        // Verify temporary row is deleted from local DB
        optMsg = store.messageById(msgId);
        QVERIFY(!optMsg.has_value());
    }

    void testArchiveAndRollback()
    {
        Account acc;
        acc.email = QStringLiteral("ops-test-archive@example.com");
        acc.id = insertTestAccount();
        QVERIFY(acc.id > 0);

        Folder inbox = insertTestFolder(acc.id, QStringLiteral("INBOX"), QStringLiteral("inbox"));
        Folder archive = insertTestFolder(acc.id, QStringLiteral("Archive"), QStringLiteral("archive"));
        QVERIFY(inbox.id > 0);
        QVERIFY(archive.id > 0);

        SyncWorker worker(acc);

        MessageStore store;
        Message msg = makeTestMessage(inbox.id, 5005);
        qint64 msgId = store.insertMessage(msg);
        QVERIFY(msgId > 0);

        // 1. Archive message (should trigger move to Archive folder)
        worker.archiveMessage(inbox.id, msgId);

        auto optMsg = store.messageById(msgId);
        QVERIFY(optMsg.has_value());
        QCOMPARE(optMsg->folderId, archive.id);
        QCOMPARE(optMsg->uid, -1 * msgId);

        // 2. Trigger IMAP failure (should rollback to INBOX with original UID)
        QMetaObject::invokeMethod(&worker, "onClientError", Q_ARG(QString, QStringLiteral("Archive failed")));

        optMsg = store.messageById(msgId);
        QVERIFY(optMsg.has_value());
        QCOMPARE(optMsg->folderId, inbox.id);
        QCOMPARE(optMsg->uid, (qint64)5005);
    }
};

QTEST_MAIN(MessageOpsTest)
#include "MessageOpsTest.moc"
