#include <QTest>
#include <QSignalSpy>
#include <QSqlQuery>
#include <QSqlError>
#include <QTcpServer>
#include <QTcpSocket>
#include <QDir>
#include <QFile>
#include <QElapsedTimer>
#include "core/Settings.h"
#include "core/SyncWorker.h"
#include "network/ImapClient.h"
#include "storage/Account.h"
#include "storage/AccountStore.h"
#include "storage/Database.h"
#include "storage/FolderStore.h"
#include "storage/MessageStore.h"

//
// MockImapServer — in-process IMAP server script player
//
class MockImapServer : public QObject
{
    Q_OBJECT
public:
    explicit MockImapServer(QObject* parent = nullptr) : QObject(parent) {}

    bool listen()
    {
        m_server = new QTcpServer(this);
        if (!m_server->listen(QHostAddress::LocalHost, 0)) return false;
        m_port = m_server->serverPort();
        connect(m_server, &QTcpServer::newConnection, this, &MockImapServer::onNewConnection);
        return true;
    }

    quint16 port() const { return m_port; }

    void setSelectResponse(const QByteArray& resp) { m_selectResponse = resp; }
    void setSearchResponse(const QByteArray& resp) { m_searchResponse = resp; }
    void setFetchResponse(const QByteArray& resp) { m_fetchResponse = resp; }
    void setLoginSuccess(bool success) { m_loginSuccess = success; }
    void setCloseOnConnect(bool close) { m_closeOnConnect = close; }

    int selectCommandCount() const { return m_selectCount; }

signals:
    void commandReceived(const QByteArray& cmd);

private slots:
    void onNewConnection()
    {
        m_client = m_server->nextPendingConnection();
        if (m_closeOnConnect) {
            m_client->disconnectFromHost();
            return;
        }
        connect(m_client, &QTcpSocket::readyRead, this, &MockImapServer::onReadyRead);
        m_client->write("* OK IMAP4rev1 Ready\r\n");
    }

    void onReadyRead()
    {
        while (m_client && m_client->canReadLine()) {
            QByteArray line = m_client->readLine().trimmed();
            emit commandReceived(line);

            int spaceIdx = line.indexOf(' ');
            if (spaceIdx == -1) continue;
            QByteArray tag = line.left(spaceIdx);
            QByteArray cmd = line.mid(spaceIdx + 1);

            if (cmd.startsWith("CAPABILITY")) {
                m_client->write("* CAPABILITY IMAP4rev1\r\n" + tag + " OK CAPABILITY completed\r\n");
            } else if (cmd.startsWith("LOGIN") || cmd.startsWith("AUTHENTICATE")) {
                if (m_loginSuccess) {
                    m_client->write(tag + " OK LOGIN completed\r\n");
                } else {
                    m_client->write(tag + " NO [AUTHENTICATIONFAILED] Invalid credentials\r\n");
                }
            } else if (cmd.startsWith("LIST")) {
                m_client->write("* LIST () \"/\" \"INBOX\"\r\n" + tag + " OK LIST completed\r\n");
            } else if (cmd.startsWith("SELECT")) {
                m_selectCount++;
                m_client->write(m_selectResponse);
                m_client->write(tag + " OK [READ-WRITE] SELECT completed\r\n");
            } else if (cmd.startsWith("UID SEARCH")) {
                m_client->write(m_searchResponse);
                m_client->write(tag + " OK SEARCH completed\r\n");
            } else if (cmd.startsWith("UID FETCH")) {
                m_client->write(m_fetchResponse);
                m_client->write(tag + " OK FETCH completed\r\n");
            } else if (cmd.startsWith("LOGOUT")) {
                m_client->write("* BYE\r\n" + tag + " OK LOGOUT completed\r\n");
                m_client->disconnectFromHost();
            }
        }
    }

private:
    QTcpServer* m_server = nullptr;
    QTcpSocket* m_client = nullptr;
    quint16 m_port = 0;
    QByteArray m_selectResponse;
    QByteArray m_searchResponse;
    QByteArray m_fetchResponse;
    bool m_loginSuccess = true;
    bool m_closeOnConnect = false;
    int m_selectCount = 0;
};

//
// RecoveryTest — Integration and Robustness Tests
//
class RecoveryTest : public QObject
{
    Q_OBJECT

private:
    qint64 insertTestAccount()
    {
        QSqlQuery q(Database::connection());
        q.prepare(QStringLiteral(
            "INSERT INTO accounts(email, display_name, created_at)"
            "    VALUES('recovery-test@example.com', 'Recovery Test', '2026-01-01T00:00:00Z')"));
        if (!q.exec()) return -1;
        return q.lastInsertId().toLongLong();
    }

    Folder insertTestFolder(qint64 accountId, const QString& name, const QString& role, qint64 uidval = 0)
    {
        FolderStore fs;
        Folder f;
        f.accountId = accountId;
        f.name      = name;
        f.role      = role;
        f.uidvalidity = uidval;
        f.id = fs.insertFolder(f);
        return f;
    }

    Message makeTestMessage(qint64 folderId, qint64 uid)
    {
        Message m;
        m.folderId    = folderId;
        m.uid         = uid;
        m.messageId   = QStringLiteral("<") + QString::number(uid) + QStringLiteral("@recovery-test>");
        m.subject     = QStringLiteral("Test Subject ") + QString::number(uid);
        m.fromAddress = QStringLiteral("sender@example.com");
        m.fromName    = QStringLiteral("Test Sender");
        m.date        = QDateTime::currentDateTimeUtc();
        m.flags       = 0;
        m.size        = 512;
        return m;
    }

    bool waitForStatus(QSignalSpy& spy, const QString& expectedStatus, int timeoutMs = 4000)
    {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < timeoutMs) {
            for (const auto& call : spy) {
                if (call.at(0).toString() == expectedStatus) {
                    return true;
                }
            }
            QTest::qWait(50);
        }
        return false;
    }

    bool waitForStatusCount(QSignalSpy& spy, const QString& expectedStatus, int expectedCount, int timeoutMs = 4000)
    {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < timeoutMs) {
            int count = 0;
            for (const auto& call : spy) {
                if (call.at(0).toString() == expectedStatus) {
                    count++;
                }
            }
            if (count >= expectedCount) {
                return true;
            }
            QTest::qWait(50);
        }
        
        int count = 0;
        for (const auto& call : spy) {
            if (call.at(0).toString() == expectedStatus) {
                count++;
            }
        }
        QTextStream(stderr) << "waitForStatusCount failed! Expected: " << expectedStatus 
                            << " Count: " << expectedCount << " Got: " << count << "\n";
        for (const auto& call : spy) {
            QTextStream(stderr) << "  - Status: " << call.at(0).toString() << "\n";
        }
        return false;
    }

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

    void testDuplicateSyncPrevention()
    {
        MockImapServer server;
        QVERIFY(server.listen());

        qint64 accId = insertTestAccount();
        QVERIFY(accId > 0);

        Folder inbox = insertTestFolder(accId, QStringLiteral("INBOX_DUP"), QStringLiteral("inbox"));
        QVERIFY(inbox.id > 0);

        Account acc;
        acc.id = accId;
        acc.email = QStringLiteral("recovery-test@example.com");
        acc.imapHost = QStringLiteral("127.0.0.1");
        acc.imapPort = server.port();

        server.setSelectResponse(
            "* 2 EXISTS\r\n"
            "* OK [UIDVALIDITY 100]\r\n"
            "* OK [UIDNEXT 1003]\r\n"
        );
        server.setSearchResponse("* SEARCH 1001 1002\r\n");
        server.setFetchResponse(
            "* 1 FETCH (UID 1001 FLAGS ())\r\n"
            "* 2 FETCH (UID 1002 FLAGS ())\r\n"
        );

        SyncWorker worker(acc);
        QSignalSpy statusSpy(&worker, &SyncWorker::statusChanged);
        worker.start();

        QVERIFY(waitForStatus(statusSpy, QStringLiteral("Folders synced")));

        // Call syncFolder twice
        worker.syncFolder(inbox.id);
        worker.syncFolder(inbox.id);

        QTest::qWait(500);

        QCOMPARE(server.selectCommandCount(), 1);
    }

    void testUidValidityChanges()
    {
        MockImapServer server;
        QVERIFY(server.listen());

        qint64 accId = insertTestAccount();
        QVERIFY(accId > 0);

        Folder inbox = insertTestFolder(accId, QStringLiteral("INBOX_VAL"), QStringLiteral("inbox"), 100);
        QVERIFY(inbox.id > 0);

        MessageStore msgStore;
        msgStore.insertMessage(makeTestMessage(inbox.id, 1001));
        msgStore.insertMessage(makeTestMessage(inbox.id, 1002));
        QCOMPARE(msgStore.messageCount(inbox.id), 2);

        Account acc;
        acc.id = accId;
        acc.email = QStringLiteral("recovery-test@example.com");
        acc.imapHost = QStringLiteral("127.0.0.1");
        acc.imapPort = server.port();

        // Server reports mismatching UIDVALIDITY = 200
        server.setSelectResponse(
            "* 2 EXISTS\r\n"
            "* OK [UIDVALIDITY 200]\r\n"
            "* OK [UIDNEXT 1003]\r\n"
        );
        server.setSearchResponse("* SEARCH 1001 1002\r\n");
        server.setFetchResponse(
            "* 1 FETCH (UID 1001 FLAGS ())\r\n"
            "* 2 FETCH (UID 1002 FLAGS ())\r\n"
        );

        SyncWorker worker(acc);
        QSignalSpy statusSpy(&worker, &SyncWorker::statusChanged);
        worker.start();

        QVERIFY(waitForStatus(statusSpy, QStringLiteral("Folders synced")));

        worker.syncFolder(inbox.id);
        QVERIFY(waitForStatusCount(statusSpy, QStringLiteral("Synced"), 1));

        // Verify local messages were kept at count 2 (deleted and refetched)
        QCOMPARE(msgStore.messageCount(inbox.id), 2);

        // Verify UIDVALIDITY was updated to 200 in the DB
        FolderStore fstore;
        auto optF = fstore.folderById(inbox.id);
        QVERIFY(optF.has_value());
        QCOMPARE(optF->uidvalidity, (qint64)200);
    }

    void testMessageDisappearance()
    {
        MockImapServer server;
        QVERIFY(server.listen());

        qint64 accId = insertTestAccount();
        QVERIFY(accId > 0);

        Folder inbox = insertTestFolder(accId, QStringLiteral("INBOX_DIS"), QStringLiteral("inbox"), 100);
        QVERIFY(inbox.id > 0);

        MessageStore msgStore;
        qint64 m1 = msgStore.insertMessage(makeTestMessage(inbox.id, 1001));
        qint64 m2 = msgStore.insertMessage(makeTestMessage(inbox.id, 1002));
        qint64 m3 = msgStore.insertMessage(makeTestMessage(inbox.id, 1003));
        QVERIFY(m1 > 0 && m2 > 0 && m3 > 0);
        QCOMPARE(msgStore.messageCount(inbox.id), 3);

        Account acc;
        acc.id = accId;
        acc.email = QStringLiteral("recovery-test@example.com");
        acc.imapHost = QStringLiteral("127.0.0.1");
        acc.imapPort = server.port();

        // Server only returns UIDs 1001 and 1003
        server.setSelectResponse(
            "* 2 EXISTS\r\n"
            "* OK [UIDVALIDITY 100]\r\n"
            "* OK [UIDNEXT 1004]\r\n"
        );
        server.setSearchResponse("* SEARCH 1001 1003\r\n");
        server.setFetchResponse(
            "* 1 FETCH (UID 1001 FLAGS ())\r\n"
            "* 2 FETCH (UID 1003 FLAGS ())\r\n"
        );

        SyncWorker worker(acc);
        QSignalSpy statusSpy(&worker, &SyncWorker::statusChanged);
        worker.start();

        QVERIFY(waitForStatus(statusSpy, QStringLiteral("Folders synced")));

        worker.syncFolder(inbox.id);
        QVERIFY(waitForStatusCount(statusSpy, QStringLiteral("Synced"), 1));

        // Verify local message 1002 (m2) was deleted, but 1001 and 1003 remain
        QCOMPARE(msgStore.messageCount(inbox.id), 2);
        QVERIFY(msgStore.messageById(m1).has_value());
        QVERIFY(!msgStore.messageById(m2).has_value());
        QVERIFY(msgStore.messageById(m3).has_value());
    }

    void testFlagSynchronization()
    {
        MockImapServer server;
        QVERIFY(server.listen());

        qint64 accId = insertTestAccount();
        QVERIFY(accId > 0);

        Folder inbox = insertTestFolder(accId, QStringLiteral("INBOX_FLAGS"), QStringLiteral("inbox"), 100);
        QVERIFY(inbox.id > 0);

        MessageStore msgStore;
        Message msg1 = makeTestMessage(inbox.id, 1001);
        msg1.flags = 0; // Unread
        qint64 m1 = msgStore.insertMessage(msg1);

        Message msg2 = makeTestMessage(inbox.id, 1002);
        msg2.flags = MessageFlags::Seen; // Read
        qint64 m2 = msgStore.insertMessage(msg2);

        QVERIFY(m1 > 0 && m2 > 0);

        Account acc;
        acc.id = accId;
        acc.email = QStringLiteral("recovery-test@example.com");
        acc.imapHost = QStringLiteral("127.0.0.1");
        acc.imapPort = server.port();

        // Server reports 1001 is Seen, 1002 is Flagged
        server.setSelectResponse(
            "* 2 EXISTS\r\n"
            "* OK [UIDVALIDITY 100]\r\n"
            "* OK [UIDNEXT 1003]\r\n"
        );
        server.setSearchResponse("* SEARCH 1001 1002\r\n");
        server.setFetchResponse(
            "* 1 FETCH (UID 1001 FLAGS (\\Seen))\r\n"
            "* 2 FETCH (UID 1002 FLAGS (\\Flagged))\r\n"
        );

        SyncWorker worker(acc);
        QSignalSpy statusSpy(&worker, &SyncWorker::statusChanged);
        worker.start();

        QVERIFY(waitForStatus(statusSpy, QStringLiteral("Folders synced")));

        worker.syncFolder(inbox.id);
        QVERIFY(waitForStatusCount(statusSpy, QStringLiteral("Synced"), 1));

        auto optMsg1 = msgStore.messageById(m1);
        QVERIFY(optMsg1.has_value());
        QCOMPARE(optMsg1->flags, (int)MessageFlags::Seen);

        auto optMsg2 = msgStore.messageById(m2);
        QVERIFY(optMsg2.has_value());
        QCOMPARE(optMsg2->flags, (int)MessageFlags::Flagged);
    }

    void testOfflineReconnectionRecovery()
    {
        MockImapServer server;
        server.setCloseOnConnect(true);
        QVERIFY(server.listen());

        qint64 accId = insertTestAccount();
        QVERIFY(accId > 0);

        Folder inbox = insertTestFolder(accId, QStringLiteral("INBOX_OFFLINE"), QStringLiteral("inbox"), 100);
        QVERIFY(inbox.id > 0);

        Account acc;
        acc.id = accId;
        acc.email = QStringLiteral("recovery-test@example.com");
        acc.imapHost = QStringLiteral("127.0.0.1");
        acc.imapPort = server.port();

        SyncWorker worker(acc);
        QSignalSpy statusSpy(&worker, &SyncWorker::statusChanged);
        worker.start();

        // Trigger sync while connection is immediately closed on connect.
        // This will result in a connection failure which sets status to Offline: ...
        worker.syncFolder(inbox.id);
        
        // Wait until we reach Offline status.
        QVERIFY(waitForStatus(statusSpy, QStringLiteral("Offline: The remote host closed the connection")));

        // Now configure mock server to succeed
        server.setCloseOnConnect(false);
        server.setSelectResponse(
            "* 0 EXISTS\r\n"
            "* OK [UIDVALIDITY 100]\r\n"
            "* OK [UIDNEXT 1001]\r\n"
        );
        server.setSearchResponse("* SEARCH\r\n");

        // Manually start reconnection retry
        worker.start();

        // Wait for it to become Synced (will be the 1st Synced event since login failure didn't emit Synced)
        QVERIFY(waitForStatusCount(statusSpy, QStringLiteral("Synced"), 1));
    }

    void testAuthenticationFailureHandling()
    {
        MockImapServer server;
        server.setLoginSuccess(false);
        QVERIFY(server.listen());

        qint64 accId = insertTestAccount();
        QVERIFY(accId > 0);

        Account acc;
        acc.id = accId;
        acc.email = QStringLiteral("recovery-test@example.com");
        acc.imapHost = QStringLiteral("127.0.0.1");
        acc.imapPort = server.port();

        SyncWorker worker(acc);
        QSignalSpy statusSpy(&worker, &SyncWorker::statusChanged);
        worker.start();

        // Wait for login to fail
        QVERIFY(waitForStatus(statusSpy, QStringLiteral("Needs Attention")) || waitForStatus(statusSpy, QStringLiteral("Login failed")));

        // Configure server to allow login
        server.setLoginSuccess(true);
        server.setSelectResponse(
            "* 0 EXISTS\r\n"
            "* OK [UIDVALIDITY 100]\r\n"
            "* OK [UIDNEXT 1001]\r\n"
        );
        server.setSearchResponse("* SEARCH\r\n");

        // Manually trigger sync - should clear auth failed status and succeed
        Folder inbox = insertTestFolder(accId, QStringLiteral("INBOX_AUTH"), QStringLiteral("inbox"), 100);
        worker.syncFolder(inbox.id);

        // Wait for it to sync successfully (1st Synced event because login failure didn't emit Synced)
        QVERIFY(waitForStatusCount(statusSpy, QStringLiteral("Synced"), 1));
    }

    void testStartupCleanup()
    {
        QString dbPath = QDir::tempPath() + QStringLiteral("/mailinium_recovery_test.db");
        QFile::remove(dbPath);

        QVERIFY(Database::instance().open(dbPath));

        qint64 accId = insertTestAccount();
        QVERIFY(accId > 0);

        Folder inbox = insertTestFolder(accId, QStringLiteral("INBOX_CLEAN"), QStringLiteral("inbox"));
        QVERIFY(inbox.id > 0);

        MessageStore msgStore;
        qint64 m1 = msgStore.insertMessage(makeTestMessage(inbox.id, 5001));

        Message optMsg = makeTestMessage(inbox.id, -5002);
        optMsg.uid = -5002;
        qint64 m2 = msgStore.insertMessage(optMsg);

        QVERIFY(m1 > 0 && m2 > 0);

        QSqlQuery q(Database::connection());
        QVERIFY(q.exec(QStringLiteral("SELECT COUNT(*) FROM messages")));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 2);

        // Reopen database - triggers startup recovery
        Database::instance().close();
        QVERIFY(Database::instance().open(dbPath));

        // Check cleanup completed: negative UID was deleted, positive remains!
        QSqlQuery q2(Database::connection());
        QVERIFY(q2.exec(QStringLiteral("SELECT uid FROM messages")));
        QVERIFY(q2.next());
        QCOMPARE(q2.value(0).toLongLong(), (qint64)5001);
        QVERIFY(!q2.next());

        Database::instance().close();
        QFile::remove(dbPath);
    }
};

QTEST_MAIN(RecoveryTest)
#include "RecoveryTest.moc"
