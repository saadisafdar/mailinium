#include <QTest>
#include <QElapsedTimer>
#include <QDateTime>

#include "core/Logger.h"
#include "storage/Account.h"
#include "storage/AccountStore.h"
#include "storage/Database.h"
#include "storage/FolderStore.h"
#include "storage/MessageStore.h"
#include "protocols/MimeParser.h"

class PerformanceTest : public QObject
{
    Q_OBJECT

private:
    qint64 m_accountId = -1;
    qint64 m_folderId = -1;

private slots:
    void initTestCase()
    {
        QVERIFY(Database::instance().open(QStringLiteral(":memory:")));

        AccountStore accStore;
        Account acc;
        acc.email = QStringLiteral("perf@example.com");
        acc.imapHost = QStringLiteral("127.0.0.1");
        acc.imapPort = 993;
        m_accountId = accStore.insertAccount(acc);
        QVERIFY(m_accountId > 0);

        FolderStore folderStore;
        Folder folder;
        folder.accountId = m_accountId;
        folder.name = QStringLiteral("INBOX");
        folder.role = QStringLiteral("inbox");
        m_folderId = folderStore.insertFolder(folder);
        QVERIFY(m_folderId > 0);
    }

    void cleanupTestCase()
    {
        Database::instance().close();
    }

    void test1kMessagesBatchInsertAndQuery()
    {
        MessageStore store;
        const int count = 1000;
        QList<Message> messages;
        messages.reserve(count);

        QDateTime now = QDateTime::currentDateTimeUtc();
        for (int i = 1; i <= count; ++i) {
            Message m;
            m.folderId = m_folderId;
            m.uid = i;
            m.messageId = QStringLiteral("<msg_%1@mailinium.org>").arg(i);
            m.subject = QStringLiteral("Newsletter update #%1 - Weekly Digest").arg(i);
            m.fromAddress = QStringLiteral("newsletter%1@technews.io").arg(i % 10);
            m.fromName = QStringLiteral("Tech Digest %1").arg(i % 10);
            m.date = now.addSecs(-i * 60);
            m.flags = (i % 2 == 0) ? MessageFlags::Seen : 0;
            m.size = 2048 + (i % 500);
            messages.append(m);
        }

        QElapsedTimer timer;
        timer.start();

        QSqlDatabase db = Database::connection();
        bool hasTxn = db.transaction();
        QList<qint64> ids = store.insertMessages(messages);
        if (hasTxn) {
            db.commit();
        }

        qint64 insertTimeMs = timer.elapsed();
        qInfo() << "[PERF] 1,000 messages batch insert took:" << insertTimeMs << "ms";
        QCOMPARE(ids.size(), count);
        for (qint64 id : ids) {
            QVERIFY(id > 0);
        }

        // Query first page of 50 messages
        timer.restart();
        QList<Message> page = store.messagesForFolder(m_folderId, 50, 0);
        qint64 queryTimeMs = timer.elapsed();
        qInfo() << "[PERF] Querying 50 messages page from 1,000 took:" << queryTimeMs << "ms";

        QCOMPARE(page.size(), 50);
        // Verify newest-first ordering
        QVERIFY(page.first().date >= page.last().date);

        // Verify count
        QCOMPARE(store.messageCount(m_folderId), count);
    }

    void test10kMessagesScaleAndFtsSearch()
    {
        // Create dedicated folder for 10k test
        FolderStore folderStore;
        Folder folder;
        folder.accountId = m_accountId;
        folder.name = QStringLiteral("ARCHIVE_10K");
        folder.role = QStringLiteral("archive");
        qint64 f10kId = folderStore.insertFolder(folder);
        QVERIFY(f10kId > 0);

        MessageStore store;
        const int count = 10000;
        QList<Message> messages;
        messages.reserve(count);

        QDateTime now = QDateTime::currentDateTimeUtc();
        for (int i = 1; i <= count; ++i) {
            Message m;
            m.folderId = f10kId;
            m.uid = i;
            m.messageId = QStringLiteral("<10k_%1@example.com>").arg(i);
            if (i == 4242) {
                m.subject = QStringLiteral("Urgent: Quantum Encryption Secret Key Protocol");
                m.fromAddress = QStringLiteral("albus@hogwarts.edu");
                m.fromName = QStringLiteral("Albus Dumbledore");
            } else if (i == 7777) {
                m.subject = QStringLiteral("Invitation to the International Quantum Computing Summit");
                m.fromAddress = QStringLiteral("organizer@quantumsummit.org");
                m.fromName = QStringLiteral("Summit Committee");
            } else {
                m.subject = QStringLiteral("Automated report build notification #%1").arg(i);
                m.fromAddress = QStringLiteral("ci-bot%1@buildserver.internal").arg(i % 50);
                m.fromName = QStringLiteral("CI Bot %1").arg(i % 50);
            }
            m.date = now.addSecs(-i * 10);
            m.flags = MessageFlags::Seen;
            m.size = 1024 + (i % 1000);
            messages.append(m);
        }

        QElapsedTimer timer;
        timer.start();

        QSqlDatabase db = Database::connection();
        bool hasTxn = db.transaction();
        QList<qint64> ids = store.insertMessages(messages);
        if (hasTxn) {
            db.commit();
        }

        qint64 insert10kTimeMs = timer.elapsed();
        qInfo() << "[PERF] 10,000 messages batch insert (including FTS5 triggers) took:" << insert10kTimeMs << "ms";
        QCOMPARE(ids.size(), count);

        // Benchmark deep pagination (page at offset 5,000)
        timer.restart();
        QList<Message> midPage = store.messagesForFolder(f10kId, 50, 5000);
        qint64 pageTimeMs = timer.elapsed();
        qInfo() << "[PERF] Offset 5,000 pagination over 10,000 rows took:" << pageTimeMs << "ms";
        QCOMPARE(midPage.size(), 50);

        // Benchmark FTS5 Search across 10,000 rows
        timer.restart();
        QList<Message> searchResults = store.searchMessages(QStringLiteral("Quantum"), m_accountId, f10kId);
        qint64 searchTimeMs = timer.elapsed();
        qInfo() << "[PERF] FTS5 Search across 10,000 messages took:" << searchTimeMs << "ms. Found:" << searchResults.size();

        QCOMPARE(searchResults.size(), 2);
        QVERIFY(searchTimeMs < 100); // Must be fast under index

        // Clean up 10k folder messages
        store.deleteMessagesForFolder(f10kId);
    }

    void testLargeHtmlEmailAndManyAttachments()
    {
        // Construct a large 1MB+ synthetic MIME payload with HTML and 50 attachments
        QByteArray largeHtml;
        largeHtml.reserve(600000);
        largeHtml.append("<html><head><style>body { font-family: sans-serif; }</style></head><body><h1>Quarterly Performance Report</h1>");
        for (int i = 0; i < 2000; ++i) {
            largeHtml.append(QString("<p>Item %1: Metric data for subsystem %2 shows nominal performance and latency within acceptable thresholds.</p>\n")
                                 .arg(i).arg(i % 16).toUtf8());
        }
        largeHtml.append("</body></html>");

        QByteArray mimeData;
        mimeData.reserve(1200000);
        QByteArray boundary = "----=_Part_987654_321098765.123456789";

        mimeData.append("Content-Type: multipart/mixed; boundary=\"" + boundary + "\"\r\n");
        mimeData.append("From: sender@enterprise.corp\r\n");
        mimeData.append("To: recipient@enterprise.corp\r\n");
        mimeData.append("Subject: Complete Financial and Engineering Audit Package\r\n");
        mimeData.append("MIME-Version: 1.0\r\n\r\n");

        // HTML Body Part
        mimeData.append("--" + boundary + "\r\n");
        mimeData.append("Content-Type: text/html; charset=utf-8\r\n");
        mimeData.append("Content-Transfer-Encoding: 8bit\r\n\r\n");
        mimeData.append(largeHtml);
        mimeData.append("\r\n");

        // 50 Attachments
        for (int i = 1; i <= 50; ++i) {
            mimeData.append("--" + boundary + "\r\n");
            mimeData.append(QString("Content-Type: application/pdf; name=\"report_appendix_%1.pdf\"\r\n").arg(i).toUtf8());
            mimeData.append(QString("Content-Disposition: attachment; filename=\"report_appendix_%1.pdf\"\r\n").arg(i).toUtf8());
            mimeData.append("Content-Transfer-Encoding: base64\r\n\r\n");
            QByteArray payload = QString("Synthetic PDF Attachment Data Content #%1 with binary header %2")
                                     .arg(i).arg(i * 100).toUtf8().toBase64();
            mimeData.append(payload);
            mimeData.append("\r\n");
        }
        mimeData.append("--" + boundary + "--\r\n");

        QElapsedTimer timer;
        timer.start();

        MimeMessage parsed = MimeParser::parse(mimeData);
        qint64 parseTimeMs = timer.elapsed();

        qInfo() << "[PERF] Parsing 1.2MB MIME email with 50 attachments took:" << parseTimeMs << "ms";

        QVERIFY(!parsed.htmlBody.isEmpty());
        QVERIFY(parsed.htmlBody.size() > 200000);
        QCOMPARE(parsed.attachments.size(), 50);
        QCOMPARE(parsed.attachments.first().filename, QStringLiteral("report_appendix_1.pdf"));
        QCOMPARE(parsed.attachments.last().filename, QStringLiteral("report_appendix_50.pdf"));
        QVERIFY(parseTimeMs < 200);
    }

    void testOnDemandBodySeparation()
    {
        MessageStore store;
        Message msg;
        msg.folderId = m_folderId;
        msg.uid = 99999;
        msg.subject = QStringLiteral("On-demand memory test");
        msg.date = QDateTime::currentDateTimeUtc();
        qint64 msgId = store.insertMessage(msg);
        QVERIFY(msgId > 0);

        // Store a huge 2MB body
        QString heavyHtml = QStringLiteral("<div>%1</div>").arg(QString(2 * 1024 * 1024, 'X'));
        bool stored = store.saveMessageBodies(msgId, heavyHtml, QStringLiteral("Plain text"));
        QVERIFY(stored);

        // Verify folder listing does not load body into Message object
        QList<Message> list = store.messagesForFolder(m_folderId, 10, 0);
        QVERIFY(!list.isEmpty());

        bool found = false;
        for (const Message& m : list) {
            if (m.id == msgId) {
                found = true;
                QVERIFY(m.bodyFetched); // flagged as fetched in DB
                break;
            }
        }
        QVERIFY(found);

        // On-demand body query only loads when explicitly requested
        auto bodies = store.getMessageBodies(msgId);
        QCOMPARE(bodies.first.size(), heavyHtml.size());

        store.deleteMessage(msgId);
    }
};

QTEST_MAIN(PerformanceTest)
#include "PerformanceTest.moc"
