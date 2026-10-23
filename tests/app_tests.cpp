#include "../src/app/Platform.h"
#include "../src/app/SessionStore.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

using namespace bitfly;

class AppTests final : public QObject {
    Q_OBJECT
  private slots:
    void parsesStableVersions() {
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
        QVERIFY(!stableVersion("1.2.3+.."));
        QVERIFY(!stableVersion("1.2.3+build..7"));
    }

    void sessionStorePersistsValues() {
        QVERIFY2(QSqlDatabase::isDriverAvailable("QSQLITE"),
                 "Required Qt SQLite plugin is missing");
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
            QCOMPARE(store.get("missing", QStringLiteral("fallback")).toString(),
                     QStringLiteral("fallback"));
        }
        {
            SessionStore store(path);
            QVERIFY2(store.error().isEmpty(), qPrintable(store.error()));
            QCOMPARE(store.get("cacheLimit").toInt(), 12);
            QVERIFY(store.get("advanced").toBool());
            QCOMPARE(store.get("vlcPath").toString(), QStringLiteral("C:/Apps/VLC/vlc.exe"));
            QVERIFY(store.put("cacheLimit", 16));
            QCOMPARE(store.get("cacheLimit").toInt(), 16);
            QVERIFY(store.putMany({{"cacheLimit", 20}, {"advanced", false}}));
            QCOMPARE(store.get("cacheLimit").toInt(), 20);
            QVERIFY(!store.get("advanced").toBool());
        }
    }
    void refusesFutureSchemaWithoutChangingIt() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath("future.sqlite");
        const auto connection = QUuid::createUuid().toString();
        {
            auto db = QSqlDatabase::addDatabase("QSQLITE", connection);
            db.setDatabaseName(path);
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec("PRAGMA user_version=99"));
        }
        QSqlDatabase::removeDatabase(connection);
        {
            SessionStore store(path);
            QVERIFY(!store.error().isEmpty());
            QVERIFY(!store.put("test", true));
        }
        {
            auto db = QSqlDatabase::addDatabase("QSQLITE", connection);
            db.setDatabaseName(path);
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec("PRAGMA user_version"));
            QVERIFY(q.next());
            QCOMPARE(q.value(0).toInt(), 99);
            QVERIFY(q.exec("SELECT count(*) FROM sqlite_master WHERE name='settings'"));
            QVERIFY(q.next());
            QCOMPARE(q.value(0).toInt(), 0);
        }
        QSqlDatabase::removeDatabase(connection);
    }
};

QTEST_GUILESS_MAIN(AppTests)
#include "app_tests.moc"
