#include <QSqlQuery>
#include <QTest>

#include "storage/Database.h"
#include "storage/Folder.h"
#include "storage/FolderStore.h"

// Helper: insert a minimal account row so folders can satisfy the FK constraint.
static qint64 insertTestAccount(const QString& email = QStringLiteral("test@example.com"))
{
    QSqlQuery q(Database::connection());
    q.prepare(QStringLiteral(
        "INSERT INTO accounts(email, display_name, created_at)"
        "    VALUES(:email, :name, '2025-01-01T00:00:00Z')"));
    q.bindValue(QStringLiteral(":email"), email);
    q.bindValue(QStringLiteral(":name"),  QStringLiteral("Test Account"));
    if (!q.exec()) return -1;
    return q.lastInsertId().toLongLong();
}

class FolderStoreTest : public QObject
{
    Q_OBJECT

    FolderStore m_store;
    qint64      m_accountId = -1;

private slots:
    void initTestCase()
    {
        QVERIFY(Database::instance().open(QStringLiteral(":memory:")));
        m_accountId = insertTestAccount();
        QVERIFY(m_accountId > 0);
    }

    void cleanupTestCase()
    {
        Database::instance().close();
    }

    // --- insertFolder ---------------------------------------------------

    void insertFolder_returnsPositiveId()
    {
        Folder f;
        f.accountId = m_accountId;
        f.name      = QStringLiteral("INBOX");
        f.role      = QStringLiteral("inbox");

        const qint64 id = m_store.insertFolder(f);
        QVERIFY2(id > 0, "Expected positive id from insertFolder");
    }

    void insertFolder_duplicateNameFails()
    {
        // UNIQUE(account_id, name) — same name under same account must fail.
        Folder f;
        f.accountId = m_accountId;
        f.name      = QStringLiteral("Sent");
        f.role      = QStringLiteral("sent");

        const qint64 id1 = m_store.insertFolder(f);
        QVERIFY(id1 > 0);

        // Second insert with identical (account_id, name) must return -1.
        const qint64 id2 = m_store.insertFolder(f);
        QCOMPARE(id2, qint64(-1));
    }

    // --- folderById -----------------------------------------------------

    void folderById_returnsInsertedValues()
    {
        Folder f;
        f.accountId     = m_accountId;
        f.name          = QStringLiteral("Archive");
        f.role          = QStringLiteral("archive");
        f.uidvalidity   = 12345;
        f.highestModseq = 67890;

        const qint64 id = m_store.insertFolder(f);
        QVERIFY(id > 0);

        const auto result = m_store.folderById(id);
        QVERIFY2(result.has_value(), "folderById returned nullopt");

        QCOMPARE(result->id,            id);
        QCOMPARE(result->accountId,     m_accountId);
        QCOMPARE(result->name,          QStringLiteral("Archive"));
        QCOMPARE(result->role,          QStringLiteral("archive"));
        QCOMPARE(result->uidvalidity,   qint64(12345));
        QCOMPARE(result->highestModseq, qint64(67890));
    }

    void folderById_missingIdReturnsNullopt()
    {
        const auto result = m_store.folderById(999999);
        QVERIFY(!result.has_value());
    }

    // --- folderByName ---------------------------------------------------

    void folderByName_findsExistingFolder()
    {
        Folder f;
        f.accountId = m_accountId;
        f.name      = QStringLiteral("Drafts");
        f.role      = QStringLiteral("drafts");

        const qint64 id = m_store.insertFolder(f);
        QVERIFY(id > 0);

        const auto result = m_store.folderByName(m_accountId, QStringLiteral("Drafts"));
        QVERIFY(result.has_value());
        QCOMPARE(result->id, id);
    }

    void folderByName_missingNameReturnsNullopt()
    {
        const auto result = m_store.folderByName(m_accountId, QStringLiteral("DoesNotExist"));
        QVERIFY(!result.has_value());
    }

    // --- foldersForAccount ----------------------------------------------

    void foldersForAccount_returnsOnlyThisAccountsFolders()
    {
        // Insert a second account with its own folder.
        const qint64 otherId = insertTestAccount(QStringLiteral("other@example.com"));
        QVERIFY(otherId > 0);

        Folder of;
        of.accountId = otherId;
        of.name      = QStringLiteral("INBOX");
        of.role      = QStringLiteral("inbox");
        m_store.insertFolder(of);

        const QList<Folder> folders = m_store.foldersForAccount(m_accountId);

        for (const Folder& folder : folders) {
            QCOMPARE(folder.accountId, m_accountId);
        }
    }

    void foldersForAccount_emptyAccountReturnsEmptyList()
    {
        // Insert a third account with no folders.
        const qint64 emptyAccountId = insertTestAccount(QStringLiteral("empty@example.com"));
        QVERIFY(emptyAccountId > 0);

        const QList<Folder> folders = m_store.foldersForAccount(emptyAccountId);
        QVERIFY(folders.isEmpty());
    }

    // --- updateSyncState ------------------------------------------------

    void updateSyncState_changesValues()
    {
        Folder f;
        f.accountId = m_accountId;
        f.name      = QStringLiteral("Trash");
        f.role      = QStringLiteral("trash");

        const qint64 id = m_store.insertFolder(f);
        QVERIFY(id > 0);

        QVERIFY(m_store.updateSyncState(id, 9999, 8888));

        const auto result = m_store.folderById(id);
        QVERIFY(result.has_value());
        QCOMPARE(result->uidvalidity,   qint64(9999));
        QCOMPARE(result->highestModseq, qint64(8888));
    }

    void updateSyncState_missingIdReturnsFalse()
    {
        QVERIFY(!m_store.updateSyncState(999999, 1, 1));
    }

    // --- deleteFolder ---------------------------------------------------

    void deleteFolder_removesRow()
    {
        Folder f;
        f.accountId = m_accountId;
        f.name      = QStringLiteral("ToDelete");
        f.role      = QStringLiteral("custom");

        const qint64 id = m_store.insertFolder(f);
        QVERIFY(id > 0);

        QVERIFY(m_store.deleteFolder(id));
        QVERIFY(!m_store.folderById(id).has_value());
    }

    void deleteFolder_missingIdReturnsFalse()
    {
        QVERIFY(!m_store.deleteFolder(999999));
    }

    void testFolderRoleDetection()
    {
        // Folders assigned by ImapClient::handleListResponse based on name.
        // We test the expected role assignments at the store level.
        qint64 accId = insertTestAccount(QStringLiteral("role-test@example.com"));

        auto insertNamed = [&](const QString& name, const QString& role) -> Folder {
            Folder f;
            f.accountId = accId;
            f.name = name;
            f.role = role;
            f.id = m_store.insertFolder(f);
            return f;
        };

        Folder allMail = insertNamed(QStringLiteral("[Gmail]/All Mail"), QStringLiteral("archive"));
        Folder inbox   = insertNamed(QStringLiteral("INBOX"),            QStringLiteral("inbox"));
        Folder custom1 = insertNamed(QStringLiteral("Work"),             QStringLiteral(""));
        Folder custom2 = insertNamed(QStringLiteral("Personal"),         QStringLiteral(""));

        QVERIFY(allMail.id > 0);
        QVERIFY(inbox.id > 0);
        QVERIFY(custom1.id > 0);
        QVERIFY(custom2.id > 0);

        // Archive role lookup should find [Gmail]/All Mail, not Work or Personal
        auto optArchive = m_store.folderByRole(accId, QStringLiteral("archive"));
        QVERIFY(optArchive.has_value());
        QCOMPARE(optArchive->id, allMail.id);
        QCOMPARE(optArchive->name, QStringLiteral("[Gmail]/All Mail"));

        // Custom folders must have empty role — not "archive"
        auto optWork = m_store.folderByName(accId, QStringLiteral("Work"));
        QVERIFY(optWork.has_value());
        QCOMPARE(optWork->role, QStringLiteral(""));

        auto optPersonal = m_store.folderByName(accId, QStringLiteral("Personal"));
        QVERIFY(optPersonal.has_value());
        QCOMPARE(optPersonal->role, QStringLiteral(""));
    }
};

QTEST_MAIN(FolderStoreTest)
#include "FolderStoreTest.moc"
