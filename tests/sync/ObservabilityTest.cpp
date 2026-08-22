#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>

#include "core/Logger.h"
#include "core/DiagnosticsManager.h"
#include "ui/NotificationBanner.h"
#include "storage/Database.h"

class ObservabilityTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testEmailMasking();
    void testDiagnosticsReport();
    void testNotificationBanner();
    void testLoggerExport();

private:
    QTemporaryDir m_tempDir;
};

void ObservabilityTest::initTestCase()
{
    QVERIFY(m_tempDir.isValid());
    Database::instance().open(m_tempDir.path() + "/test_obs.db");
    Logger::instance().initialize(m_tempDir.path() + "/logs", Logger::Level::Debug);
}

void ObservabilityTest::cleanupTestCase()
{
    Database::instance().close();
}

void ObservabilityTest::testEmailMasking()
{
    QCOMPARE(DiagnosticsManager::maskEmail(QStringLiteral("saadisafdar@gmail.com")), QStringLiteral("sa***@gmail.com"));
    QCOMPARE(DiagnosticsManager::maskEmail(QStringLiteral("me@company.org")), QStringLiteral("m***@company.org"));
    QCOMPARE(DiagnosticsManager::maskEmail(QStringLiteral("a@b.com")), QStringLiteral("a***@b.com"));
    QCOMPARE(DiagnosticsManager::maskEmail(QStringLiteral("invalid")), QStringLiteral("***@***"));
}

void ObservabilityTest::testDiagnosticsReport()
{
    QString report = DiagnosticsManager::generateReport();
    QVERIFY(!report.isEmpty());
    QVERIFY(report.contains(QStringLiteral("Mailinium System & Diagnostic Report")));
    QVERIFY(report.contains(QStringLiteral("Application & Environment")));
    QVERIFY(report.contains(QStringLiteral("Storage & Database Metrics")));
    QVERIFY(report.contains(QStringLiteral("Accounts & Synchronization")));
}

void ObservabilityTest::testNotificationBanner()
{
    NotificationBanner banner;
    QVERIFY(!banner.isVisible());

    bool actionClicked = false;
    banner.showNotification(
        NotificationBanner::Type::Error,
        QStringLiteral("Failed to connect to IMAP server"),
        QStringLiteral("Retry"),
        [&actionClicked]() { actionClicked = true; },
        0
    );

    QVERIFY(banner.isVisible());

    QSignalSpy dismissSpy(&banner, &NotificationBanner::dismissed);
    banner.dismiss();
    QVERIFY(!banner.isVisible());
    QCOMPARE(dismissSpy.count(), 1);
}

void ObservabilityTest::testLoggerExport()
{
    Logger::instance().info(QStringLiteral("test"), QStringLiteral("Observability test log message"));
    Logger::instance().flush();

    QString exportPath = m_tempDir.path() + "/exported_logs.txt";
    bool ok = Logger::instance().exportLogs(exportPath);
    QVERIFY(ok);
    QVERIFY(QFile::exists(exportPath));

    QFile file(exportPath);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QString content = QString::fromUtf8(file.readAll());
    QVERIFY(content.contains(QStringLiteral("Mailinium Application Diagnostics & Logs Export")));
    QVERIFY(content.contains(QStringLiteral("Observability test log message")));
}

QTEST_MAIN(ObservabilityTest)
#include "ObservabilityTest.moc"
