#include "../src/app/Platform.h"
#include "../src/app/SessionStore.h"
#include "../src/app/ThumbnailArt.h"

#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
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

    void refusesWritesAfterCorruptSetting_data() {
        QTest::addColumn<QByteArray>("corrupt");
        QTest::newRow("truncated-json") << QByteArray("{");
        QTest::newRow("non-object") << QByteArray("[]");
        QTest::newRow("missing-value") << QByteArray("{}");
    }

    void refusesWritesAfterCorruptSetting() {
        QFETCH(QByteArray, corrupt);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath("corrupt-setting.sqlite");
        {
            SessionStore store(path);
            QVERIFY(store.putMany(
                {{"queue", QVariantList{QStringLiteral("saved media")}}, {"advanced", true}}));
            QVERIFY(store.put("nullable", QVariant()));
            QVERIFY(store.get("nullable").isNull());
            QVERIFY(store.error().isEmpty());
        }
        const auto connection = QUuid::createUuid().toString();
        {
            auto db = QSqlDatabase::addDatabase("QSQLITE", connection);
            db.setDatabaseName(path);
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.prepare("UPDATE settings SET value=? WHERE key='queue'"));
            q.addBindValue(corrupt);
            QVERIFY(q.exec());
            QCOMPARE(q.numRowsAffected(), 1);
        }
        QSqlDatabase::removeDatabase(connection);
        {
            SessionStore store(path);
            QVERIFY(store.error().isEmpty());
            QVERIFY(store.get("queue", QVariantList{}).toList().isEmpty());
            QVERIFY2(!store.error().isEmpty(),
                     "Corrupt stored JSON must not silently use defaults.");
            QVERIFY(!store.putMany({{"queue", QVariantList{}}, {"advanced", false}}));
        }
        {
            auto db = QSqlDatabase::addDatabase("QSQLITE", connection);
            db.setDatabaseName(path);
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec("SELECT value FROM settings WHERE key='queue'"));
            QVERIFY(q.next());
            QCOMPARE(q.value(0).toByteArray(), corrupt);
            QVERIFY(q.exec("SELECT value FROM settings WHERE key='advanced'"));
            QVERIFY(q.next());
            QCOMPARE(q.value(0).toByteArray(), QByteArray("{\"value\":true}"));
        }
        QSqlDatabase::removeDatabase(connection);
    }

    void failedInitializationPreservesSchemaVersion() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath("conflicting-schema.sqlite");
        const auto connection = QUuid::createUuid().toString();
        {
            auto db = QSqlDatabase::addDatabase("QSQLITE", connection);
            db.setDatabaseName(path);
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec("CREATE TABLE sentinel(id INTEGER)"));
            QVERIFY(q.exec("CREATE INDEX settings ON sentinel(id)"));
        }
        QSqlDatabase::removeDatabase(connection);
        {
            SessionStore store(path);
            QVERIFY(!store.error().isEmpty());
            QVERIFY(!store.put("queue", QVariantList{}));
        }
        {
            auto db = QSqlDatabase::addDatabase("QSQLITE", connection);
            db.setDatabaseName(path);
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec("PRAGMA user_version"));
            QVERIFY(q.next());
            QCOMPARE(q.value(0).toInt(), 0);
        }
        QSqlDatabase::removeDatabase(connection);
    }

    void coverArtIsDeterministicAndPersists() {
        const QImage first = bitfly::renderCoverArt(QStringLiteral("The Long Way Home (2026)"),
                                                    QStringLiteral("id-1"));
        const QImage second = bitfly::renderCoverArt(QStringLiteral("The Long Way Home (2026)"),
                                                     QStringLiteral("id-1"));
        const QImage different =
            bitfly::renderCoverArt(QStringLiteral("Another Library Entry"), QStringLiteral("id-2"));
        QCOMPARE(first.size(), QSize(512, 512));
        QCOMPARE(first, second);
        QVERIFY(first != different);
        QVERIFY(first.pixelColor(8, 8) != first.pixelColor(500, 500));

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath("nested/cover.png");
        QString error;
        QVERIFY2(bitfly::writeCoverArt(path, QStringLiteral("The Long Way Home"),
                                       QStringLiteral("id-1"), &error),
                 qPrintable(error));
        QVERIFY(QFileInfo::exists(path));
        QVERIFY(QFileInfo(path).size() > 1024);
        QImage loaded(path);
        QVERIFY(!loaded.isNull());
        QCOMPARE(loaded.size(), QSize(512, 512));
    }
};

QTEST_MAIN(AppTests)
#include "app_tests.moc"
