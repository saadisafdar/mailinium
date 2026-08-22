#include <QTest>
#include <QSignalSpy>
#include <QDateTime>
#include <QDir>
#include <QFile>

#include "core/Logger.h"
#include "core/Settings.h"
#include "core/SyncManager.h"
#include "core/SyncWorker.h"
#include "storage/Account.h"
#include "storage/AccountStore.h"
#include "storage/Database.h"
#include "storage/FolderStore.h"
#include "storage/MessageStore.h"

class AccountAuthTest : public QObject
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

    void testUpdateOAuthTokens()
    {
        AccountStore store;
        Account acc;
        acc.email = QStringLiteral("oauth-test@example.com");
        acc.authType = AuthType::OAuth2;
        acc.accessToken = QStringLiteral("old-access-token");
        acc.refreshToken = QStringLiteral("old-refresh-token");
        acc.tokenExpiry = QDateTime::currentDateTimeUtc().addSecs(-3600); // expired
        acc.imapHost = QStringLiteral("127.0.0.1");
        acc.imapPort = 993;

        qint64 id = store.insertAccount(acc);
        QVERIFY(id > 0);

        QDateTime newExpiry = QDateTime::currentDateTimeUtc().addSecs(3600);
        bool updated = store.updateOAuthTokens(id, QStringLiteral("new-access-token"), QStringLiteral("new-refresh-token"), newExpiry);
        QVERIFY(updated);

        auto opt = store.accountById(id);
        QVERIFY(opt.has_value());
        QCOMPARE(opt->accessToken, QStringLiteral("new-access-token"));
        QCOMPARE(opt->refreshToken, QStringLiteral("new-refresh-token"));
        QVERIFY(opt->tokenExpiry.isValid());
        QVERIFY(opt->tokenExpiry > QDateTime::currentDateTimeUtc());

        store.deleteAccount(id);
    }

    void testUpdatePassword()
    {
        AccountStore store;
        Account acc;
        acc.email = QStringLiteral("pass-test@example.com");
        acc.authType = AuthType::Password;
        acc.password = QStringLiteral("old-password");
        acc.imapHost = QStringLiteral("127.0.0.1");
        acc.imapPort = 143;

        qint64 id = store.insertAccount(acc);
        QVERIFY(id > 0);

        bool updated = store.updatePassword(id, QStringLiteral("super-secret-new-password"));
        QVERIFY(updated);

        auto opt = store.accountById(id);
        QVERIFY(opt.has_value());
        QCOMPARE(opt->password, QStringLiteral("super-secret-new-password"));

        store.deleteAccount(id);
    }

    void testOAuthRevocationHaltsSyncTimer()
    {
        AccountStore store;
        Account acc;
        acc.email = QStringLiteral("revoked-test@example.com");
        acc.authType = AuthType::OAuth2;
        acc.accessToken = QStringLiteral("expired-access");
        acc.refreshToken = QStringLiteral("revoked-refresh");
        acc.tokenExpiry = QDateTime::currentDateTimeUtc().addSecs(-60);
        acc.imapHost = QStringLiteral("127.0.0.1");
        acc.imapPort = 993;

        qint64 id = store.insertAccount(acc);
        QVERIFY(id > 0);
        acc.id = id;

        // Populate local folder and message to ensure local data is kept intact
        FolderStore folderStore;
        Folder folder;
        folder.accountId = id;
        folder.name = QStringLiteral("INBOX");
        folder.role = QStringLiteral("inbox");
        qint64 fId = folderStore.insertFolder(folder);
        QVERIFY(fId > 0);

        MessageStore msgStore;
        Message msg;
        msg.folderId = fId;
        msg.uid = 1001;
        msg.subject = QStringLiteral("Cached offline message");
        msg.fromAddress = QStringLiteral("sender@example.com");
        msg.date = QDateTime::currentDateTimeUtc();
        qint64 mId = msgStore.insertMessage(msg);
        QVERIFY(mId > 0);

        SyncWorker worker(acc);
        QSignalSpy statusSpy(&worker, &SyncWorker::statusChanged);

        // Simulate permanent auth failure
        QMetaObject::invokeMethod(&worker, "onTokenRefreshFailed",
                                  Q_ARG(QString, QStringLiteral("invalid_grant")),
                                  Q_ARG(bool, true));

        // Verify status changed to Needs Attention
        QVERIFY(statusSpy.count() > 0);
        QCOMPARE(statusSpy.last().at(0).toString(), QStringLiteral("Needs Attention"));

        // Verify local cached message is completely intact
        QCOMPARE(msgStore.messageCount(fId), 1);
        auto optMsg = msgStore.messageById(mId);
        QVERIFY(optMsg.has_value());
        QCOMPARE(optMsg->subject, QStringLiteral("Cached offline message"));

        store.deleteAccount(id);
    }

    void testReauthenticateFlow()
    {
        AccountStore store;
        Account acc;
        acc.email = QStringLiteral("reauth-test@example.com");
        acc.authType = AuthType::OAuth2;
        acc.accessToken = QStringLiteral("bad-token");
        acc.refreshToken = QStringLiteral("bad-refresh");
        acc.imapHost = QStringLiteral("127.0.0.1");
        acc.imapPort = 993;

        qint64 id = store.insertAccount(acc);
        QVERIFY(id > 0);
        acc.id = id;

        SyncWorker worker(acc);
        QSignalSpy statusSpy(&worker, &SyncWorker::statusChanged);

        // Trigger permanent failure
        QMetaObject::invokeMethod(&worker, "onLoginFailed",
                                  Q_ARG(QString, QStringLiteral("Authentication failed")));
        QCOMPARE(statusSpy.last().at(0).toString(), QStringLiteral("Needs Attention"));

        // Update credentials in store
        store.updateOAuthTokens(id, QStringLiteral("valid-access"), QStringLiteral("valid-refresh"),
                                QDateTime::currentDateTimeUtc().addSecs(3600));

        // Trigger reauthentication
        worker.reauthenticate();

        // Worker should reset authFailed and attempt connection
        QVERIFY(statusSpy.count() > 1);
        QCOMPARE(statusSpy.last().at(0).toString(), QStringLiteral("Connecting..."));

        worker.stop();
        store.deleteAccount(id);
    }

    void testMultiAccountWorkerIsolation()
    {
        AccountStore store;
        Account acc1;
        acc1.email = QStringLiteral("user1@example.com");
        acc1.imapHost = QStringLiteral("127.0.0.1");
        acc1.imapPort = 993;
        qint64 id1 = store.insertAccount(acc1);
        QVERIFY(id1 > 0);
        acc1.id = id1;

        Account acc2;
        acc2.email = QStringLiteral("user2@example.com");
        acc2.imapHost = QStringLiteral("127.0.0.1");
        acc2.imapPort = 993;
        qint64 id2 = store.insertAccount(acc2);
        QVERIFY(id2 > 0);
        acc2.id = id2;

        SyncManager& sm = SyncManager::instance();
        sm.addAccount(acc1);
        sm.addAccount(acc2);

        QVERIFY(sm.workerForAccount(id1) != nullptr);
        QVERIFY(sm.workerForAccount(id2) != nullptr);
        QVERIFY(sm.workerForAccount(id1) != sm.workerForAccount(id2));

        // Stop worker for account 1
        sm.stopWorker(id1);
        QVERIFY(sm.workerForAccount(id1) == nullptr);
        QVERIFY(sm.workerForAccount(id2) != nullptr); // account 2 remains unaffected

        sm.stopAll();
        store.deleteAccount(id1);
        store.deleteAccount(id2);
    }

    void testAccountRemovalCascade()
    {
        AccountStore store;
        Account acc;
        acc.email = QStringLiteral("cascade-test@example.com");
        acc.imapHost = QStringLiteral("127.0.0.1");
        acc.imapPort = 993;
        qint64 id = store.insertAccount(acc);
        QVERIFY(id > 0);
        acc.id = id;

        FolderStore folderStore;
        Folder folder;
        folder.accountId = id;
        folder.name = QStringLiteral("INBOX");
        folder.role = QStringLiteral("inbox");
        qint64 fId = folderStore.insertFolder(folder);
        QVERIFY(fId > 0);

        MessageStore msgStore;
        Message msg;
        msg.folderId = fId;
        msg.uid = 5001;
        msg.subject = QStringLiteral("To be cascaded");
        msg.date = QDateTime::currentDateTimeUtc();
        qint64 mId = msgStore.insertMessage(msg);
        QVERIFY(mId > 0);

        SyncManager& sm = SyncManager::instance();
        sm.addAccount(acc);
        QVERIFY(sm.workerForAccount(id) != nullptr);

        QSignalSpy removeSpy(&sm, &SyncManager::accountRemoved);

        // Remove account via SyncManager
        sm.removeAccount(id);

        QCOMPARE(removeSpy.count(), 1);
        QCOMPARE(removeSpy.first().at(0).toLongLong(), id);
        QVERIFY(sm.workerForAccount(id) == nullptr);

        // Verify account, folder, and messages were cascade deleted from DB
        QVERIFY(!store.accountById(id).has_value());
        QVERIFY(!folderStore.folderById(fId).has_value());
        QCOMPARE(msgStore.messageCount(fId), 0);
    }

    void testOfflineStartupGracefulDegradation()
    {
        Account acc;
        acc.id = 999;
        acc.email = QStringLiteral("offline-startup@example.com");
        acc.authType = AuthType::Password;
        acc.password = QStringLiteral("pass");
        acc.imapHost = QStringLiteral("127.0.0.1");
        acc.imapPort = 1; // unavailable closed port

        SyncWorker worker(acc);
        QSignalSpy statusSpy(&worker, &SyncWorker::statusChanged);

        worker.start();

        // Allow socket error to process
        QTRY_VERIFY_WITH_TIMEOUT(!statusSpy.isEmpty(), 3000);

        // Verify initial connect status is reported
        QCOMPARE(statusSpy.first().at(0).toString(), QStringLiteral("Connecting..."));

        worker.stop();
    }
};

QTEST_MAIN(AccountAuthTest)
#include "AccountAuthTest.moc"
