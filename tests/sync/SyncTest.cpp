#include <QTest>
#include <QSignalSpy>
#include "core/Settings.h"
#include "core/SyncWorker.h"
#include "storage/Account.h"
#include "storage/Database.h"

class SyncTest : public QObject
{
    Q_OBJECT

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

    void testDefaultSyncSettings()
    {
        int interval = Settings::instance().intValue(Settings::Key::SyncIntervalSeconds);
        QCOMPARE(interval, 300);

        int fetchLimit = Settings::instance().intValue(Settings::Key::SyncFetchLimit);
        QCOMPARE(fetchLimit, 200);

        int cacheMB = Settings::instance().intValue(Settings::Key::SyncOfflineCacheMB);
        QCOMPARE(cacheMB, 500);
    }

    void testSettingsPropagation()
    {
        Account acc;
        acc.id = 1;
        acc.email = QStringLiteral("syncuser@example.com");

        SyncWorker worker(acc);

        QSignalSpy spy(&Settings::instance(), &Settings::settingChanged);

        // Change interval
        Settings::instance().setValue(Settings::Key::SyncIntervalSeconds, 120);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(Settings::instance().intValue(Settings::Key::SyncIntervalSeconds), 120);

        // Restore default
        Settings::instance().setValue(Settings::Key::SyncIntervalSeconds, 300);
    }
};

QTEST_MAIN(SyncTest)
#include "SyncTest.moc"
