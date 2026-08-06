#include <QtTest>
#include "storage/Database.h"
#include "storage/DraftStore.h"
#include "storage/AccountStore.h"

//
// DraftStoreTest — verifies DraftStore CRUD against an in-memory SQLite database.
//
class DraftStoreTest : public QObject
{
    Q_OBJECT

private:
    qint64 m_accountId = -1;

    static Draft makeTestDraft(qint64 accountId)
    {
        Draft d;
        d.accountId  = accountId;
        d.subject    = QStringLiteral("Test Subject");
        d.toList     = QStringLiteral("recipient@example.com");
        d.ccList     = QStringLiteral("cc@example.com");
        d.bccList    = QStringLiteral("bcc@example.com");
        d.bodyHtml   = QStringLiteral("<p>Hello World</p>");
        d.bodyPlain  = QStringLiteral("Hello World");
        d.inReplyTo  = QStringLiteral("<original@example.com>");
        d.references = QStringLiteral("<original@example.com>");
        return d;
    }

private slots:

    void initTestCase()
    {
        QVERIFY(Database::instance().open(QStringLiteral(":memory:")));

        // Insert a dummy account so FK constraint is satisfied
        AccountStore as;
        Account acc;
        acc.email       = QStringLiteral("test@example.com");
        acc.displayName = QStringLiteral("Test User");
        acc.provider    = QStringLiteral("test");
        acc.imapHost    = QStringLiteral("imap.example.com");
        acc.smtpHost    = QStringLiteral("smtp.example.com");
        m_accountId = as.insertAccount(acc);
        QVERIFY(m_accountId > 0);
    }

    void cleanupTestCase()
    {
        Database::instance().close();
    }

    // --- insertDraft -----------------------------------------------------

    void insertDraft_returnsPositiveId()
    {
        DraftStore store;
        Draft d = makeTestDraft(m_accountId);
        const qint64 id = store.insertDraft(d);
        QVERIFY(id > 0);
        QCOMPARE(d.id, id);
    }

    void insertDraft_canBeRetrievedById()
    {
        DraftStore store;
        Draft d = makeTestDraft(m_accountId);
        store.insertDraft(d);

        const auto result = store.draftById(d.id);
        QVERIFY(result.has_value());
        QCOMPARE(result->subject,   d.subject);
        QCOMPARE(result->toList,    d.toList);
        QCOMPARE(result->ccList,    d.ccList);
        QCOMPARE(result->bccList,   d.bccList);
        QCOMPARE(result->bodyHtml,  d.bodyHtml);
        QCOMPARE(result->bodyPlain, d.bodyPlain);
        QCOMPARE(result->inReplyTo, d.inReplyTo);
        QCOMPARE(result->references, d.references);
        QCOMPARE(result->accountId, m_accountId);
    }

    // --- updateDraft -----------------------------------------------------

    void updateDraft_changesFields()
    {
        DraftStore store;
        Draft d = makeTestDraft(m_accountId);
        store.insertDraft(d);

        d.subject  = QStringLiteral("Updated Subject");
        d.bodyPlain = QStringLiteral("Updated body text");
        QVERIFY(store.updateDraft(d));

        const auto result = store.draftById(d.id);
        QVERIFY(result.has_value());
        QCOMPARE(result->subject,   QStringLiteral("Updated Subject"));
        QCOMPARE(result->bodyPlain, QStringLiteral("Updated body text"));
    }

    void updateDraft_missingIdReturnsFalse()
    {
        DraftStore store;
        Draft d = makeTestDraft(m_accountId);
        d.id = 999999; // non-existent
        QVERIFY(!store.updateDraft(d));
    }

    // --- deleteDraft -----------------------------------------------------

    void deleteDraft_removesRow()
    {
        DraftStore store;
        Draft d = makeTestDraft(m_accountId);
        store.insertDraft(d);

        QVERIFY(store.deleteDraft(d.id));
        QVERIFY(!store.draftById(d.id).has_value());
    }

    void deleteDraft_missingIdReturnsFalse()
    {
        DraftStore store;
        QVERIFY(!store.deleteDraft(999999));
    }

    // --- draftsForAccount ------------------------------------------------

    void draftsForAccount_returnsAllDrafts()
    {
        DraftStore store;

        const int before = store.draftsForAccount(m_accountId).size();

        Draft d1 = makeTestDraft(m_accountId);
        Draft d2 = makeTestDraft(m_accountId);
        d1.subject = QStringLiteral("Draft A");
        d2.subject = QStringLiteral("Draft B");
        store.insertDraft(d1);
        store.insertDraft(d2);

        const QList<Draft> all = store.draftsForAccount(m_accountId);
        QVERIFY(all.size() >= before + 2);
    }

    void draftsForAccount_emptyForUnknownAccount()
    {
        DraftStore store;
        QVERIFY(store.draftsForAccount(999999).isEmpty());
    }

    // --- draftById -------------------------------------------------------

    void draftById_missingReturnsNullopt()
    {
        DraftStore store;
        QVERIFY(!store.draftById(999999).has_value());
    }
};

QTEST_MAIN(DraftStoreTest)
#include "DraftStoreTest.moc"
