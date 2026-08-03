#include <QSignalSpy>
#include <QSqlQuery>
#include <QTest>
#include <QTimeZone>

#include "storage/Account.h"
#include "storage/AccountStore.h"
#include "storage/Database.h"

//
// Tests for AccountStore CRUD operations.
//
class AccountStoreTest : public QObject
{
    Q_OBJECT

    AccountStore m_store;

private slots:
    void initTestCase()
    {
        // Open a fresh in-memory database
        QVERIFY(Database::instance().open(QStringLiteral(":memory:")));
    }

    void cleanupTestCase()
    {
        Database::instance().close();
    }

    void cleanup()
    {
        // Clear all accounts after each test to ensure isolation
        QSqlQuery q(Database::connection());
        q.exec(QStringLiteral("DELETE FROM accounts"));
    }

    // --- insertAccount ---------------------------------------------------

    void insertAccount_returnsPositiveId()
    {
        Account acc;
        acc.email       = QStringLiteral("test1@example.com");
        acc.displayName = QStringLiteral("Test User 1");
        acc.password    = QStringLiteral("secret123");
        acc.provider    = QStringLiteral("Custom");
        acc.imapHost    = QStringLiteral("imap.example.com");
        acc.imapPort    = 993;
        acc.smtpHost    = QStringLiteral("smtp.example.com");
        acc.smtpPort    = 587;

        const qint64 id = m_store.insertAccount(acc);
        QVERIFY(id > 0);
        QVERIFY(acc.id == id);
        
        QCOMPARE(acc.authType, AuthType::Password);
        QCOMPARE(acc.accessToken, QStringLiteral(""));
        QCOMPARE(acc.refreshToken, QStringLiteral(""));
        QVERIFY(!acc.tokenExpiry.isValid());
    }

    void insertAccount_setsCreatedAt()
    {
        Account acc;
        acc.email = QStringLiteral("test@example.com");
        // Not setting createdAt explicitly

        const qint64 id = m_store.insertAccount(acc);
        QVERIFY(id > 0);
        QVERIFY(acc.createdAt.isValid());
    }

    // --- updateAccount ---------------------------------------------------

    void updateAccount_modifiesExistingRow()
    {
        Account acc;
        acc.email = QStringLiteral("update_before@example.com");
        const qint64 id = m_store.insertAccount(acc);
        QVERIFY(id > 0);

        acc.email       = QStringLiteral("updated@example.com");
        acc.displayName = QStringLiteral("Updated Name");
        acc.password    = QStringLiteral("newpassword");
        acc.authType    = AuthType::OAuth2;
        acc.accessToken = QStringLiteral("new_access");

        QVERIFY(m_store.updateAccount(acc));

        const auto fetched = m_store.accountById(id);
        QVERIFY(fetched.has_value());
        QCOMPARE(fetched->email, QStringLiteral("updated@example.com"));
        QCOMPARE(fetched->displayName, QStringLiteral("Updated Name"));
        QCOMPARE(fetched->password, QStringLiteral("newpassword"));
        QCOMPARE(fetched->authType, AuthType::OAuth2);
        QCOMPARE(fetched->accessToken, QStringLiteral("new_access"));
    }

    void updateAccount_missingIdReturnsFalse()
    {
        Account acc;
        acc.id    = 99999;
        acc.email = QStringLiteral("ghost@example.com");

        QVERIFY(!m_store.updateAccount(acc));
    }

    // --- deleteAccount ---------------------------------------------------

    void deleteAccount_removesRow()
    {
        Account acc;
        acc.email = QStringLiteral("delete_me@example.com");
        const qint64 id = m_store.insertAccount(acc);
        QVERIFY(id > 0);

        QVERIFY(m_store.deleteAccount(id));
        QVERIFY(!m_store.accountById(id).has_value());
        QCOMPARE(m_store.accountCount(), 0);
    }

    void deleteAccount_missingIdReturnsFalse()
    {
        QVERIFY(!m_store.deleteAccount(99999));
    }

    // --- allAccounts / accountCount --------------------------------------

    void allAccounts_returnsAllInOrder()
    {
        Account acc1;
        acc1.email = QStringLiteral("test1@example.com");
        acc1.authType = AuthType::Password;
        qint64 id1 = m_store.insertAccount(acc1);

        Account acc2;
        acc2.email = QStringLiteral("oauth@example.com");
        acc2.authType = AuthType::OAuth2;
        acc2.accessToken = QStringLiteral("access_123");
        acc2.refreshToken = QStringLiteral("refresh_456");
        acc2.tokenExpiry = QDateTime::currentDateTimeUtc().addSecs(3600);
        qint64 id2 = m_store.insertAccount(acc2);

        QCOMPARE(m_store.accountCount(), 2);

        const QList<Account> accounts = m_store.allAccounts();
        QCOMPARE(accounts.size(), 2);

        QCOMPARE(accounts[0].id, id1);
        QCOMPARE(accounts[0].email, QStringLiteral("test1@example.com"));
        QCOMPARE(accounts[0].authType, AuthType::Password);

        QCOMPARE(accounts[1].id, id2);
        QCOMPARE(accounts[1].email, QStringLiteral("oauth@example.com"));
        QCOMPARE(accounts[1].authType, AuthType::OAuth2);
        QCOMPARE(accounts[1].accessToken, QStringLiteral("access_123"));
        QCOMPARE(accounts[1].refreshToken, QStringLiteral("refresh_456"));
        QVERIFY(accounts[1].tokenExpiry.isValid());
    }
};

QTEST_MAIN(AccountStoreTest)
#include "AccountStoreTest.moc"
