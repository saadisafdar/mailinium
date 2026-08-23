#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QSqlQuery>
#include <QSqlError>

#include "core/Logger.h"
#include "core/Settings.h"
#include "core/ThemeManager.h"
#include "core/DiagnosticsManager.h"
#include "storage/Database.h"
#include "storage/AccountStore.h"
#include "storage/FolderStore.h"
#include "storage/MessageStore.h"
#include "storage/DraftStore.h"
#include "ui/NotificationBanner.h"

//
// ReleaseValidationTest — Phase 5F Release Validation Suite
//
// Validates:
//   1. Fresh profile startup & 8-step migration application
//   2. Database integrity & foreign key consistency
//   3. End-to-end subsystem lifecycle (Account, Folder, Message, Draft, Search, Observability)
//   4. Clean shutdown and persistent storage integrity
//
class ReleaseValidationTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testFreshProfileAndMigrations();
    void testDatabaseSchemaIntegrity();
    void testEndToEndSubsystemIntegration();
    void testFtsSearchAndDrafts();
    void testDiagnosticsAndObservability();
    void testCleanShutdownIntegrity();

private:
    QTemporaryDir m_tempDir;
    QString       m_dbPath;
};

void ReleaseValidationTest::initTestCase()
{
    QVERIFY(m_tempDir.isValid());
    m_dbPath = m_tempDir.path() + "/release_validation.db";
    Logger::instance().initialize(m_tempDir.path() + "/logs", Logger::Level::Debug);
}

void ReleaseValidationTest::cleanupTestCase()
{
    Database::instance().close();
}

void ReleaseValidationTest::testFreshProfileAndMigrations()
{
    // 1. Fresh database opening triggers all migrations (1 to 8)
    bool opened = Database::instance().open(m_dbPath);
    QVERIFY(opened);
    QVERIFY(Database::instance().isOpen());

    // 2. Verify migration table records
    QSqlQuery query(Database::connection());
    QVERIFY(query.exec(QStringLiteral("SELECT MAX(version) FROM schema_migrations;")));
    QVERIFY(query.next());
    int maxVersion = query.value(0).toInt();
    QCOMPARE(maxVersion, 8);
}

void ReleaseValidationTest::testDatabaseSchemaIntegrity()
{
    QSqlQuery query(Database::connection());

    // Check PRAGMA integrity_check
    QVERIFY(query.exec(QStringLiteral("PRAGMA integrity_check;")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString().toLower(), QStringLiteral("ok"));

    // Check PRAGMA foreign_key_check
    QVERIFY(query.exec(QStringLiteral("PRAGMA foreign_key_check;")));
    QVERIFY(!query.next()); // No FK violations

    // Verify all required tables exist
    QStringList requiredTables = {
        QStringLiteral("accounts"),
        QStringLiteral("folders"),
        QStringLiteral("messages"),
        QStringLiteral("message_bodies"),
        QStringLiteral("attachments"),
        QStringLiteral("drafts"),
        QStringLiteral("schema_migrations"),
        QStringLiteral("messages_fts")
    };

    for (const QString& table : requiredTables) {
        QVERIFY(query.exec(QStringLiteral("SELECT count(*) FROM sqlite_master WHERE type IN ('table', 'shadow') AND name='%1';").arg(table)));
        QVERIFY(query.next());
        QVERIFY2(query.value(0).toInt() > 0, qPrintable(QStringLiteral("Missing table: ") + table));
    }
}

void ReleaseValidationTest::testEndToEndSubsystemIntegration()
{
    AccountStore accountStore;
    FolderStore folderStore;
    MessageStore messageStore;

    // 1. Create account
    Account account;
    account.email = QStringLiteral("release.user@example.com");
    account.displayName = QStringLiteral("Release User");
    account.authType = AuthType::OAuth2;
    account.accessToken = QStringLiteral("ya29.release_test_token");
    account.refreshToken = QStringLiteral("1//release_refresh_token");
    account.imapHost = QStringLiteral("imap.gmail.com");
    account.imapPort = 993;
    account.smtpHost = QStringLiteral("smtp.gmail.com");
    account.smtpPort = 465;

    qint64 accountId = accountStore.insertAccount(account);
    QVERIFY(accountId > 0);

    // 2. Create folders
    Folder inbox;
    inbox.accountId = accountId;
    inbox.name = QStringLiteral("INBOX");
    inbox.role = QStringLiteral("inbox");
    inbox.uidvalidity = 12345;
    qint64 inboxId = folderStore.insertFolder(inbox);
    QVERIFY(inboxId > 0);

    Folder sent;
    sent.accountId = accountId;
    sent.name = QStringLiteral("Sent");
    sent.role = QStringLiteral("sent");
    sent.uidvalidity = 12345;
    qint64 sentId = folderStore.insertFolder(sent);
    QVERIFY(sentId > 0);

    // 3. Ingest message
    Message msg;
    msg.folderId = inboxId;
    msg.uid = 101;
    msg.subject = QStringLiteral("Release Validation Final Check");
    msg.fromAddress = QStringLiteral("team@mailinium.app");
    msg.fromName = QStringLiteral("Mailinium Core Team");
    msg.date = QDateTime::currentDateTimeUtc();
    msg.flags = MessageFlags::Flagged;

    qint64 messageId = messageStore.insertMessage(msg);
    QVERIFY(messageId > 0);

    // Store message body
    QVERIFY(messageStore.saveMessageBodies(
        messageId,
        QStringLiteral("<html><body><p>Mailinium <b>Release</b> build verified!</p></body></html>"),
        QStringLiteral("Mailinium release validation check: complete end-to-end integration confirmed.")
    ));

    // Verify retrieval
    auto retrieved = messageStore.messageById(messageId);
    QVERIFY(retrieved.has_value());
    QCOMPARE(retrieved->subject, QStringLiteral("Release Validation Final Check"));
    QVERIFY((retrieved->flags & MessageFlags::Flagged) != 0);

    auto bodies = messageStore.getMessageBodies(messageId);
    QVERIFY(!bodies.first.isEmpty());
    QVERIFY(!bodies.second.isEmpty());
}

void ReleaseValidationTest::testFtsSearchAndDrafts()
{
    MessageStore messageStore;
    DraftStore draftStore;
    AccountStore accountStore;

    auto accounts = accountStore.allAccounts();
    QVERIFY(!accounts.isEmpty());
    qint64 accountId = accounts.first().id;

    // 1. Test FTS5 Search
    auto searchResults = messageStore.searchMessages(QStringLiteral("Release Validation"));
    QVERIFY(!searchResults.isEmpty());
    QCOMPARE(searchResults.first().subject, QStringLiteral("Release Validation Final Check"));

    // 2. Test Draft Lifecycle
    Draft draft;
    draft.accountId = accountId;
    draft.toList = QStringLiteral("reviewer@example.com");
    draft.subject = QStringLiteral("Draft for Release");
    draft.bodyPlain = QStringLiteral("Preparing the release notes...");
    
    qint64 draftId = draftStore.insertDraft(draft);
    QVERIFY(draftId > 0);

    auto retrievedDraft = draftStore.draftById(draftId);
    QVERIFY(retrievedDraft.has_value());
    QCOMPARE(retrievedDraft->subject, QStringLiteral("Draft for Release"));

    // Update draft
    draft.id = draftId;
    draft.subject = QStringLiteral("Draft for Release - Updated");
    draft.bodyPlain = QStringLiteral("Final release notes prepared.");
    QVERIFY(draftStore.updateDraft(draft));

    auto updatedDraft = draftStore.draftById(draftId);
    QVERIFY(updatedDraft.has_value());
    QCOMPARE(updatedDraft->subject, QStringLiteral("Draft for Release - Updated"));

    // Cleanup draft
    QVERIFY(draftStore.deleteDraft(draftId));
    QVERIFY(!draftStore.draftById(draftId).has_value());
}

void ReleaseValidationTest::testDiagnosticsAndObservability()
{
    // 1. Diagnostics report generation
    QString report = DiagnosticsManager::generateReport();
    QVERIFY(!report.isEmpty());
    QVERIFY(report.contains(QStringLiteral("Mailinium System & Diagnostic Report")));
    QVERIFY(report.contains(QStringLiteral("Accounts & Synchronization")));

    // Verify email masking in report
    QVERIFY(report.contains(QStringLiteral("re***@example.com")));
    QVERIFY(!report.contains(QStringLiteral("release.user@example.com")));

    // 2. Notification banner testing
    NotificationBanner banner;
    bool actionClicked = false;
    banner.showNotification(
        NotificationBanner::Type::Info,
        QStringLiteral("System operational"),
        QStringLiteral("OK"),
        [&actionClicked]() { actionClicked = true; }
    );
    banner.show();
    QCOMPARE(banner.isVisible(), true);
    banner.dismiss();
}

void ReleaseValidationTest::testCleanShutdownIntegrity()
{
    // Flush logs and verify file exists
    Logger::instance().info(QStringLiteral("release"), QStringLiteral("Validation test concluding"));
    Logger::instance().flush();

    QStringList logs = Logger::instance().allLogFiles();
    QVERIFY(!logs.isEmpty());

    // Verify SQLite clean close and reopen integrity
    Database::instance().close();
    QVERIFY(!Database::instance().isOpen());

    QVERIFY(Database::instance().open(m_dbPath));
    QSqlQuery query(Database::connection());
    QVERIFY(query.exec(QStringLiteral("PRAGMA integrity_check;")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString().toLower(), QStringLiteral("ok"));
}

QTEST_MAIN(ReleaseValidationTest)
#include "ReleaseValidationTest.moc"
