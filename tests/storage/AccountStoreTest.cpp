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

        acc.email       = QStringLiteral("update_after@example.com");
        acc.displayName = QStringLiteral("Updated User");
        acc.password    = QStringLiteral("new_secret");
        acc.imapPort    = 143;

        QVERIFY(m_store.updateAccount(acc));

        const auto fetched = m_store.accountById(id);
        QVERIFY(fetched.has_value());
        QCOMPARE(fetched->email, QStringLiteral("update_after@example.com"));
        QCOMPARE(fetched->displayName, QStringLiteral("Updated User"));
        QCOMPARE(fetched->password, QStringLiteral("new_secret"));
        QCOMPARE(fetched->imapPort, 143);
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
        Account acc1, acc2, acc3;
        acc1.email = QStringLiteral("a1@example.com");
        acc2.email = QStringLiteral("a2@example.com");
        acc3.email = QStringLiteral("a3@example.com");

        m_store.insertAccount(acc1);
        m_store.insertAccount(acc2);
        m_store.insertAccount(acc3);

        QCOMPARE(m_store.accountCount(), 3);

        const QList<Account> all = m_store.allAccounts();
        QCOMPARE(all.size(), 3);
        QCOMPARE(all[0].email, QStringLiteral("a1@example.com"));
        QCOMPARE(all[1].email, QStringLiteral("a2@example.com"));
        QCOMPARE(all[2].email, QStringLiteral("a3@example.com"));
    }
};

QTEST_MAIN(AccountStoreTest)
#include "AccountStoreTest.moc"
