#include "app/ApplicationController.h"
#include "app/Platform.h"
#include "app/ThumbnailArt.h"
#include <QDir>
#include <QFile>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

using namespace bitfly;
class ControllerTests final : public QObject {
    Q_OBJECT
    void seed(const QString& root, QVariantList history = {}) {
        SessionStore store(root + "/sessions.sqlite");
        QVERIFY(store.putMany(
            {{"dhtEnabled", false}, {"localDiscoveryEnabled", false}, {"history", history}}));
    }
  private slots:
    void retainsLibraryAndOwnsArtworkPaths() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVariantList history;
        for (int i = 0; i < 105; ++i)
            history.append(QVariantMap{
                {"id", QString::number(i)},
                {"name", "Saved media"},
                {"magnet", "magnet:?xt=urn:btih:0123456789012345678901234567890123456789"}});
        history.append(QVariantMap{{"id", "../outside"}, {"name", "Malformed old identity"}});
        seed(directory.path(), history);
        const QString outside = directory.filePath("outside.png");
        QFile sentinel(outside);
        QVERIFY(sentinel.open(QIODevice::WriteOnly));
        sentinel.write("keep");
        sentinel.close();
        {
            ApplicationController controller(nullptr, directory.path());
            QSignalSpy ready(&controller, &ApplicationController::workerReady);
            QTRY_VERIFY_WITH_TIMEOUT(!ready.isEmpty(), 10000);
            QCOMPARE(controller.history().size(), history.size());
            for (const auto& value : controller.library()) {
                const auto row = value.toMap();
                QVERIFY(QUrl(row.value("thumbnail").toString())
                            .toLocalFile()
                            .startsWith(directory.path() + "/thumbnails/"));
                QVERIFY(row.value("dimmed").toBool());
            }
            QVERIFY(QFileInfo::exists(outside));
        }
        SessionStore store(directory.filePath("sessions.sqlite"));
        QCOMPARE(store.get("history").toList().size(), history.size());
        QCOMPARE(store.get("history").toList().first().toMap().value("magnet"),
                 history.first().toMap().value("magnet"));
    }
    void persistsSettingsAndCustomArtworkWithoutExposingSearchKey() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        seed(directory.path(), {QVariantMap{{"id", id}, {"name", "My film"}}});
        QImage image(80, 40, QImage::Format_RGB32);
        image.fill(Qt::red);
        const QString imagePath = directory.filePath("chosen.png");
        QVERIFY(image.save(imagePath));
        {
            ApplicationController controller(nullptr, directory.path());
            QSignalSpy ready(&controller, &ApplicationController::workerReady);
            QTRY_VERIFY_WITH_TIMEOUT(!ready.isEmpty(), 10000);
            controller.setReducedMotion(true);
            controller.setGeneratePreviews(false);
            controller.setConnectionLimit(2000);
            controller.setStreamAheadMiB(-1);
            controller.setDownloadLimitKiB(-1);
            controller.setLibraryArtwork(id, QUrl::fromLocalFile(imagePath));
            QCOMPARE(controller.history().first().toMap().value("thumbnailKind").toString(),
                     "custom");
            QCOMPARE(QImage(ownedThumbnailPath(directory.path(), id)).size(), QSize(512, 256));
            controller.configureSearch("http://127.0.0.1:9117/feed?apikey=private-test-key", "");
            QVERIFY(!controller.property("searchFeed").toString().contains("private-test-key"));
            QVERIFY(controller.searchKeySaved());
            QCOMPARE(controller.connectionLimit(), 1000);
            QCOMPARE(controller.streamAheadMiB(), 8);
        }
        {
            SessionStore store(directory.filePath("sessions.sqlite"));
            QVERIFY(!store.get("searchKeyProtected").toString().contains("private-test-key"));
            QCOMPARE(unprotectSecret(QByteArray::fromBase64(
                         store.get("searchKeyProtected").toString().toLatin1())),
                     "private-test-key");
        }
        {
            ApplicationController controller(nullptr, directory.path());
            QSignalSpy ready(&controller, &ApplicationController::workerReady);
            QTRY_VERIFY_WITH_TIMEOUT(!ready.isEmpty(), 10000);
            QVERIFY(controller.property("reducedMotion").toBool());
            QVERIFY(!controller.property("generatePreviews").toBool());
            QCOMPARE(controller.history().first().toMap().value("thumbnailKind").toString(),
                     "custom");
            QCOMPARE(QImage(ownedThumbnailPath(directory.path(), id)).pixelColor(250, 120),
                     QColor(Qt::red));
        }
    }
    void preparesPreviewWithoutLaunchingPlayer() {
        QTemporaryDir directory;
        const auto vlc = directory.filePath("vlc.exe");
        QFile player(vlc);
        QVERIFY(player.open(QIODevice::WriteOnly));
        player.close();
        seed(directory.path());
        {
            SessionStore store(directory.filePath("sessions.sqlite"));
            QVERIFY(store.put("vlcPath", vlc));
        }
        const auto torrent = directory.filePath("preview.torrent");
        QFile metadata(torrent);
        QVERIFY(metadata.open(QIODevice::WriteOnly));
        metadata.write(
            "d4:infod6:lengthi255946e4:name11:preview.avi12:piece lengthi262144e6:pieces20:");
        metadata.write(QByteArray(20, '\0'));
        metadata.write("ee");
        metadata.close();
        ApplicationController controller(nullptr, directory.path());
        QSignalSpy ready(&controller, &ApplicationController::workerReady);
        QTRY_VERIFY_WITH_TIMEOUT(!ready.isEmpty(), 10000);
        controller.openTorrent(QUrl::fromLocalFile(torrent));
        QTRY_VERIFY_WITH_TIMEOUT(!controller.sessions().isEmpty(), 10000);
        QTRY_COMPARE_WITH_TIMEOUT(controller.sessions().first().toMap().value("selected").toInt(),
                                  0, 10000);
        QVERIFY(!controller.sessions().first().toMap().value("vlcRunning").toBool());
    }
    void playerArgumentsUseDedicatedInstances() {
        QTemporaryDir directory;
        const auto vlc = directory.filePath("vlc.exe");
        QFile file(vlc);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        QVERIFY(validPlayer(vlc));
        QVERIFY(!validPlayer(directory.filePath("unknown.exe")));
        const QUrl url("http://127.0.0.1:1234/stream?token=test");
        QVERIFY(playerArguments(vlc, url).contains("--no-one-instance"));
        QVERIFY(playerArguments("mpv.exe", url).contains("--keep-open=no"));
        QVERIFY(playerArguments("mpc-hc64.exe", url).contains("/new"));
        QCOMPARE(playerArguments(vlc, url).last(), url.toString(QUrl::FullyEncoded));
    }
    void storageChangesWaitForAcceptanceAndFailedPurgeKeepsHistory() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        seed(directory.path(), {QVariantMap{{"id", "../outside"}, {"name", "Old entry"}}});
        ApplicationController controller(nullptr, directory.path());
        QSignalSpy ready(&controller, &ApplicationController::workerReady);
        QTRY_VERIFY_WITH_TIMEOUT(!ready.isEmpty(), 10000);
        const QString original = controller.cachePath();
        const auto nested = QDir(original).filePath("torrents/nested");
        QVERIFY(QDir().mkpath(nested));
        controller.setCachePath(QUrl::fromLocalFile(nested));
        QTRY_VERIFY_WITH_TIMEOUT(!controller.error().isEmpty(), 3000);
        QCOMPARE(controller.cachePath(), original);
        controller.clearError();
        const auto next = directory.filePath("other-drive");
        QVERIFY(QDir().mkpath(next));
        controller.setCachePath(QUrl::fromLocalFile(next));
        QTRY_COMPARE_WITH_TIMEOUT(controller.cachePath(), QFileInfo(next).canonicalFilePath(),
                                  3000);
        controller.removeLibraryEntry("../outside");
        QTRY_VERIFY_WITH_TIMEOUT(!controller.error().isEmpty(), 3000);
        QCOMPARE(controller.history().size(), 1);
    }
};
QTEST_MAIN(ControllerTests)
#include "controller_tests.moc"
