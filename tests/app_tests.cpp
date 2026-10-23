#include "../src/app/Platform.h"
#include "../src/app/SessionStore.h"

#include <QSqlDatabase>
#include <QTemporaryDir>
#include <QTest>

using namespace bitfly;

class AppTests final : public QObject {
    Q_OBJECT
private slots:
    void parsesStableVersions()
    {
        const auto release = stableVersion("1.2.3");
        QVERIFY(release);
        QCOMPARE((*release)[0], 1);
        QCOMPARE((*release)[1], 2);
        QCOMPARE((*release)[2], 3);
        const auto built = stableVersion("v0.12.4+build.7");
        QVERIFY(built);
        QCOMPARE((*built)[0], 0);
        QCOMPARE((*built)[1], 12);
        QCOMPARE((*built)[2], 4);
        QVERIFY(!stableVersion("1.2"));
        QVERIFY(!stableVersion("1.2.3-rc.1"));
        QVERIFY(!stableVersion("01.2.3"));
        QVERIFY(!stableVersion("1.2.2147483648"));
    }

    void sessionStorePersistsValues()
    {
        if (!QSqlDatabase::isDriverAvailable("QSQLITE"))
            QSKIP("Qt SQLite driver is unavailable in this environment");
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath("settings.sqlite");
        {
            SessionStore store(path);
            QVERIFY2(store.error().isEmpty(), qPrintable(store.error()));
            QVERIFY(store.put("cacheLimit", 12));
            QVERIFY(store.put("advanced", true));
            QVERIFY(store.put("vlcPath", QStringLiteral("C:/Apps/VLC/vlc.exe")));
            QCOMPARE(store.get("cacheLimit").toInt(), 12);
            QCOMPARE(store.get("missing", QStringLiteral("fallback")).toString(), QStringLiteral("fallback"));
        }
        {
            SessionStore store(path);
            QVERIFY2(store.error().isEmpty(), qPrintable(store.error()));
            QCOMPARE(store.get("cacheLimit").toInt(), 12);
            QVERIFY(store.get("advanced").toBool());
            QCOMPARE(store.get("vlcPath").toString(), QStringLiteral("C:/Apps/VLC/vlc.exe"));
            QVERIFY(store.put("cacheLimit", 16));
            QCOMPARE(store.get("cacheLimit").toInt(), 16);
        }
    }
};

QTEST_GUILESS_MAIN(AppTests)
#include "app_tests.moc"
