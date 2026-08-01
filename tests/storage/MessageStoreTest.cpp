#include <QSqlQuery>
#include <QTest>
#include <QTimeZone>

#include "storage/Database.h"
#include "storage/FolderStore.h"
#include "storage/Message.h"
#include "storage/MessageStore.h"

// Helpers: set up an account and folder so messages can satisfy FK constraints.

static qint64 insertTestAccount()
{
    QSqlQuery q(Database::connection());
    q.prepare(QStringLiteral(
        "INSERT INTO accounts(email, display_name, created_at)"
        "    VALUES('msg-test@example.com', 'Msg Test', '2025-01-01T00:00:00Z')"));
    if (!q.exec()) return -1;
    return q.lastInsertId().toLongLong();
}

static qint64 insertTestFolder(qint64 accountId)
{
    FolderStore fs;
    Folder f;
    f.accountId = accountId;
    f.name      = QStringLiteral("INBOX");
    f.role      = QStringLiteral("inbox");
    return fs.insertFolder(f);
}

static Message makeTestMessage(qint64 folderId, qint64 uid = 1)
{
    Message m;
    m.folderId    = folderId;
    m.uid         = uid;
    m.messageId   = QStringLiteral("<") + QString::number(uid) + QStringLiteral("@test>");
    m.subject     = QStringLiteral("Test Subject ") + QString::number(uid);
    m.fromAddress = QStringLiteral("sender@example.com");
    m.fromName    = QStringLiteral("Test Sender");
    m.date        = QDateTime(QDate(2025, 1, 15), QTime(14, 23, 1), QTimeZone::UTC);
    m.flags       = 0;
    m.size        = 1024;
    return m;
}

class MessageStoreTest : public QObject
{
    Q_OBJECT

    MessageStore m_store;
    qint64       m_folderId = -1;

private slots:
    void initTestCase()
    {
        QVERIFY(Database::instance().open(QStringLiteral(":memory:")));

        const qint64 accountId = insertTestAccount();
        QVERIFY(accountId > 0);

        m_folderId = insertTestFolder(accountId);
        QVERIFY(m_folderId > 0);
    }

    void cleanupTestCase()
    {
        Database::instance().close();
    }

    // --- insertMessage --------------------------------------------------

    void insertMessage_returnsPositiveId()
    {
        const Message msg = makeTestMessage(m_folderId, 1);
        const qint64 id = m_store.insertMessage(msg);
        QVERIFY2(id > 0, "Expected positive id from insertMessage");
    }

    void insertMessage_duplicateUidReturnsExistingId()
    {
        // INSERT OR IGNORE semantics: same (folder_id, uid) → returns existing id
        const Message msg = makeTestMessage(m_folderId, 2);
        const qint64 id1 = m_store.insertMessage(msg);
        QVERIFY(id1 > 0);

        const qint64 id2 = m_store.insertMessage(msg);
        QCOMPARE(id1, id2);
    }

    // --- messageById ----------------------------------------------------

    void messageById_returnsAllFields()
    {
        Message msg = makeTestMessage(m_folderId, 3);
        msg.flags  = MessageFlags::Seen | MessageFlags::Flagged;
        msg.size   = 2048;

        const qint64 id = m_store.insertMessage(msg);
        QVERIFY(id > 0);

        const auto result = m_store.messageById(id);
        QVERIFY2(result.has_value(), "messageById returned nullopt");

        QCOMPARE(result->id,          id);
        QCOMPARE(result->folderId,    m_folderId);
        QCOMPARE(result->uid,         qint64(3));
        QCOMPARE(result->subject,     QStringLiteral("Test Subject 3"));
        QCOMPARE(result->fromAddress, QStringLiteral("sender@example.com"));
        QCOMPARE(result->fromName,    QStringLiteral("Test Sender"));
        QCOMPARE(result->flags,       MessageFlags::Seen | MessageFlags::Flagged);
        QCOMPARE(result->size,        qint64(2048));
        QVERIFY(!result->bodyFetched);

        // Date round-trip check (to-second precision, UTC)
        QCOMPARE(result->date.toTimeZone(QTimeZone::UTC).toString(Qt::ISODate),
                 msg.date.toTimeZone(QTimeZone::UTC).toString(Qt::ISODate));
    }

    void messageById_missingIdReturnsNullopt()
    {
        QVERIFY(!m_store.messageById(999999).has_value());
    }

    // --- messageByUid ---------------------------------------------------

    void messageByUid_findsMessage()
    {
        const Message msg = makeTestMessage(m_folderId, 4);
        const qint64 id = m_store.insertMessage(msg);
        QVERIFY(id > 0);

        const auto result = m_store.messageByUid(m_folderId, 4);
        QVERIFY(result.has_value());
        QCOMPARE(result->id, id);
        QCOMPARE(result->uid, qint64(4));
    }

    void messageByUid_wrongFolderReturnsNullopt()
    {
        QVERIFY(!m_store.messageByUid(m_folderId + 999, 4).has_value());
    }

    // --- updateFlags ----------------------------------------------------

    void updateFlags_changesFlags()
    {
        const Message msg = makeTestMessage(m_folderId, 5);
        const qint64 id = m_store.insertMessage(msg);
        QVERIFY(id > 0);

        QVERIFY(m_store.updateFlags(id, MessageFlags::Seen));

        const auto result = m_store.messageById(id);
        QVERIFY(result.has_value());
        QVERIFY(result->flags & MessageFlags::Seen);
    }

    void updateFlags_missingIdReturnsFalse()
    {
        QVERIFY(!m_store.updateFlags(999999, MessageFlags::Seen));
    }

    // --- markBodyFetched ------------------------------------------------

    void markBodyFetched_setsFlag()
    {
        const Message msg = makeTestMessage(m_folderId, 6);
        const qint64 id = m_store.insertMessage(msg);
        QVERIFY(id > 0);

        QVERIFY(m_store.markBodyFetched(id));

        const auto result = m_store.messageById(id);
        QVERIFY(result.has_value());
        QVERIFY(result->bodyFetched);
    }

    // --- messagesForFolder ----------------------------------------------

    void messagesForFolder_returnsInsertedMessages()
    {
        // Insert messages with uids 10–14 for a clean count
        for (qint64 uid = 10; uid <= 14; ++uid) {
            const qint64 id = m_store.insertMessage(makeTestMessage(m_folderId, uid));
            QVERIFY(id > 0);
        }

        const QList<Message> msgs = m_store.messagesForFolder(m_folderId, /*limit=*/5, /*offset=*/0);
        QVERIFY(msgs.size() >= 5);
    }

    void messagesForFolder_limitIsRespected()
    {
        const QList<Message> msgs = m_store.messagesForFolder(m_folderId, 2, 0);
        QVERIFY(msgs.size() <= 2);
    }

    void messagesForFolder_emptyFolderReturnsEmptyList()
    {
        // Use a second folder that has no messages.
        FolderStore fs;
        Folder f;
        f.accountId = 1; // first account (already inserted)
        f.name      = QStringLiteral("EmptyFolder");
        f.role      = QStringLiteral("custom");
        const qint64 emptyFolderId = fs.insertFolder(f);
        QVERIFY(emptyFolderId > 0);

        QVERIFY(m_store.messagesForFolder(emptyFolderId).isEmpty());
    }

    // --- messageCount / unreadCount -------------------------------------

    void messageCount_isAccurate()
    {
        const int before = m_store.messageCount(m_folderId);

        const qint64 id = m_store.insertMessage(makeTestMessage(m_folderId, 20));
        QVERIFY(id > 0);

        QCOMPARE(m_store.messageCount(m_folderId), before + 1);
    }

    void unreadCount_countsUnseenMessages()
    {
        // Insert a seen and an unseen message
        Message seen = makeTestMessage(m_folderId, 21);
        seen.flags = MessageFlags::Seen;
        m_store.insertMessage(seen);

        Message unseen = makeTestMessage(m_folderId, 22);
        unseen.flags = 0;
        m_store.insertMessage(unseen);

        const int total  = m_store.messageCount(m_folderId);
        const int unread = m_store.unreadCount(m_folderId);

        QVERIFY(unread >= 1);
        QVERIFY(unread <= total);
    }

    // --- deleteMessage --------------------------------------------------

    void deleteMessage_removesRow()
    {
        const qint64 id = m_store.insertMessage(makeTestMessage(m_folderId, 30));
        QVERIFY(id > 0);

        QVERIFY(m_store.deleteMessage(id));
        QVERIFY(!m_store.messageById(id).has_value());
    }

    void deleteMessage_missingIdReturnsFalse()
    {
        QVERIFY(!m_store.deleteMessage(999999));
    }

    // --- deleteMessagesForFolder ----------------------------------------

    void deleteMessagesForFolder_clearsAllMessages()
    {
        FolderStore fs;
        Folder f;
        f.accountId = 1;
        f.name      = QStringLiteral("ClearMe");
        f.role      = QStringLiteral("custom");
        const qint64 fid = fs.insertFolder(f);
        QVERIFY(fid > 0);

        m_store.insertMessage(makeTestMessage(fid, 1));
        m_store.insertMessage(makeTestMessage(fid, 2));
        QVERIFY(m_store.messageCount(fid) >= 2);

        QVERIFY(m_store.deleteMessagesForFolder(fid));
        QCOMPARE(m_store.messageCount(fid), 0);
    }
};

QTEST_MAIN(MessageStoreTest)
#include "MessageStoreTest.moc"
