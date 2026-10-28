#include "app/ApplicationController.h"
#include "app/Platform.h"
#include "app/PreviewCapture.h"
#include "app/ThumbnailArt.h"
#include "core/ByteSource.h"
#include "stream/HttpServer.h"
#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QtEndian>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/create_torrent.hpp>
#include <libtorrent/file_storage.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/torrent_status.hpp>

using namespace bitfly;
class PreviewSource final : public ByteSource {
  public:
    QByteArray data;
    std::optional<StreamFile> describe(const QString& id) const override {
        if (id != "preview/0")
            return {};
        return StreamFile{id, "test-pattern.avi", data.size(), "video/x-msvideo"};
    }
    void demand(const QString&, qint64, qint64) override {}
    QByteArray readVerified(const QString&, qint64 offset, qint64 length) override {
        return data.mid(offset, length);
    }
};
class PreviewTests final : public QObject {
    Q_OBJECT
  private slots:
    void capturesThroughHttpAndCancelsOwnedDecoder() {
        const auto vlc = findVlc(qEnvironmentVariable("BITFLY_TEST_VLC"));
        QVERIFY2(validVlc(vlc), "This explicit media test requires installed VLC.");
        QFile fixture(QStringLiteral(BITFLY_PREVIEW_FIXTURE));
        QVERIFY(fixture.open(QIODevice::ReadOnly));
        PreviewSource source;
        source.data = fixture.readAll();
        HttpServer server(&source);
        QVERIFY(server.start());
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        PreviewCapture capture;
        QSignalSpy ready(&capture, &PreviewCapture::ready);
        const auto cancelled = directory.filePath("cancelled.png");
        const auto saved = directory.filePath("saved.png");
        capture.submit("cancelled", server.url("preview/0"), vlc, cancelled, 0);
        capture.cancel("cancelled");
        capture.submit("saved", server.url("preview/0"), vlc, saved, 0);
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 25000);
        QCOMPARE(ready.first().first().toString(), "saved");
        QVERIFY(!QFile::exists(cancelled));
        const QImage frame(saved);
        QVERIFY(!frame.isNull());
        QCOMPARE(frame.width(), 512);
        QVERIFY(frame.pixelColor(20, 20) != frame.pixelColor(300, 150));
    }
    void skipsBlackIntroAndSeeksToLaterFrame_data() {
        QTest::addColumn<int>("seconds");
        QTest::newRow("reject-opening-black") << 0;
        QTest::newRow("sample-later-frame") << 5;
        QTest::newRow("past-end-falls-back") << 90;
    }
    void skipsBlackIntroAndSeeksToLaterFrame() {
        QFETCH(int, seconds);
        QFile fixture(
            QFileInfo(QStringLiteral(BITFLY_PREVIEW_FIXTURE)).dir().filePath("preview-intro.avi"));
        QVERIFY(fixture.open(QIODevice::ReadOnly));
        PreviewSource source;
        source.data = fixture.readAll();
        HttpServer server(&source);
        QVERIFY(server.start());
        QTemporaryDir directory;
        PreviewCapture capture;
        QSignalSpy ready(&capture, &PreviewCapture::ready);
        const auto path = directory.filePath("frame.png");
        capture.submit("intro", server.url("preview/0"), findVlc(), path, seconds);
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 30000);
        QVERIFY(PreviewCapture::usefulFrame(QImage(path)));
    }
    void addedTorrentGetsMovieThumbnailWithoutPressingPlay_data() {
        QTest::addColumn<bool>("tailIndex");
        QTest::addColumn<bool>("forceRetry");
        QTest::newRow("avi") << false << false;
        QTest::newRow("partial-h264-mp4-tail-index") << true << false;
        QTest::newRow("retry-after-more-data") << true << true;
    }
    void addedTorrentGetsMovieThumbnailWithoutPressingPlay() {
        QFETCH(bool, tailIndex);
        QFETCH(bool, forceRetry);
        namespace lt = libtorrent;
        QTemporaryDir source, data;
        const auto video =
            QFileInfo(QStringLiteral(BITFLY_PREVIEW_FIXTURE)).dir().filePath("preview-intro.avi");
        const QString name = tailIndex ? "movie.mp4" : "movie.avi";
        if (tailIndex) {
            QFile input(QFileInfo(video).dir().filePath("preview-tail.mp4"));
            QVERIFY(input.open(QIODevice::ReadOnly));
            const auto bytes = input.readAll();
            qsizetype moov = 0;
            while (moov + 8 <= bytes.size() && bytes.mid(moov + 4, 4) != "moov") {
                const auto size = qFromBigEndian<quint32>(bytes.constData() + moov);
                QVERIFY(size >= 8);
                moov += size;
            }
            QVERIFY(moov + 8 <= bytes.size());
            QFile output(source.filePath(name));
            QVERIFY(output.open(QIODevice::WriteOnly));
            output.write(bytes.first(moov));
            const quint32 padding = 32 * 1024 * 1024;
            QByteArray header(8, 0);
            qToBigEndian(padding, header.data());
            header.replace(4, 4, "free");
            output.write(header);
            QVERIFY(output.seek(moov + padding));
            output.write(bytes.mid(moov));
            output.close();
        } else
            QVERIFY(QFile::copy(video, source.filePath(name)));
        lt::file_storage files;
        files.add_file(name.toStdString(), QFileInfo(source.filePath(name)).size());
        lt::create_torrent maker(files, 16 * 1024, lt::create_torrent::v1_only);
        lt::set_piece_hashes(maker, source.path().toStdString());
        std::vector<char> bytes;
        lt::bencode(std::back_inserter(bytes), maker.generate());
        const auto torrent = source.filePath("movie.torrent");
        QFile metadata(torrent);
        QVERIFY(metadata.open(QIODevice::WriteOnly));
        metadata.write(bytes.data(), bytes.size());
        metadata.close();
        lt::settings_pack settings;
        settings.set_str(lt::settings_pack::listen_interfaces, "127.0.0.1:0");
        settings.set_str(lt::settings_pack::peer_fingerprint, "BFTHUMB-TEST-000001");
        settings.set_bool(lt::settings_pack::enable_dht, false);
        settings.set_bool(lt::settings_pack::enable_lsd, false);
        settings.set_bool(lt::settings_pack::enable_upnp, false);
        settings.set_bool(lt::settings_pack::enable_natpmp, false);
        if (tailIndex)
            settings.set_int(lt::settings_pack::upload_rate_limit, 256 * 1024);
        lt::session seeder(settings);
        QTRY_VERIFY_WITH_TIMEOUT(seeder.listen_port() != 0, 10000);
        lt::add_torrent_params params;
        params.ti = std::make_shared<lt::torrent_info>(torrent.toStdString());
        params.save_path = source.path().toStdString();
        params.flags |= lt::torrent_flags::seed_mode;
        params.flags &= ~(lt::torrent_flags::paused | lt::torrent_flags::auto_managed);
        const auto handle = seeder.add_torrent(params);
        if (tailIndex)
            handle.set_upload_limit(256 * 1024);
        QTRY_VERIFY_WITH_TIMEOUT(handle.status().is_seeding, 10000);
        {
            SessionStore store(data.filePath("sessions.sqlite"));
            QVERIFY(store.putMany({{"dhtEnabled", false},
                                   {"localDiscoveryEnabled", false},
                                   {"vlcPath", findVlc()},
                                   {"streamAheadMiB", 8}}));
        }
        ApplicationController controller(nullptr, data.path());
        QSignalSpy worker(&controller, &ApplicationController::workerReady);
        QTRY_VERIFY_WITH_TIMEOUT(!worker.isEmpty(), 10000);
        bool interrupted = false;
        auto* capture = controller.findChild<PreviewCapture*>();
        QVERIFY(capture);
        if (forceRetry)
            connect(capture, &PreviewCapture::started, this, [capture, &interrupted](QString id) {
                if (interrupted)
                    return;
                interrupted = true;
                capture->cancel(id);
                QMetaObject::invokeMethod(capture, "failed", Qt::QueuedConnection,
                                          Q_ARG(QString, id));
            });
        controller.openInput(QString::fromStdString(lt::make_magnet_uri(handle)) +
                             "&x.pe=127.0.0.1:" + QString::number(seeder.listen_port()));
        QTRY_VERIFY_WITH_TIMEOUT(!controller.history().isEmpty(), 10000);
        if (forceRetry) {
            QTRY_VERIFY_WITH_TIMEOUT(interrupted, 30000);
            QTRY_VERIFY_WITH_TIMEOUT(controller.sessions()
                                         .first()
                                         .toMap()
                                         .value("previewStatus")
                                         .toString()
                                         .contains("automatically"),
                                     3000);
        }
        QTRY_COMPARE_WITH_TIMEOUT(
            controller.history().first().toMap().value("thumbnailKind").toString(),
            QString("frame"), 60000);
        const auto id = controller.history().first().toMap().value("id").toString();
        QVERIFY(PreviewCapture::usefulFrame(QImage(ownedThumbnailPath(data.path(), id))));
        QVERIFY(!controller.sessions().first().toMap().value("vlcRunning").toBool());
        if (!tailIndex) {
            // The real player must report progress and seek on the next launch.
            auto player = [&controller]() -> QProcess* {
                for (auto* process : controller.findChildren<QProcess*>())
                    if (process->state() == QProcess::Running &&
                        process->arguments().contains("--extraintf=http"))
                        return process;
                return nullptr;
            };
            auto status = [](QProcess* process, const QString& command = QString()) {
                QString port, password;
                for (const auto& arg : process->arguments()) {
                    if (arg.startsWith("--http-port="))
                        port = arg.mid(12);
                    if (arg.startsWith("--http-password="))
                        password = arg.mid(16);
                }
                QNetworkAccessManager network;
                QNetworkRequest request(
                    QUrl("http://127.0.0.1:" + port + "/requests/status.json" + command));
                request.setRawHeader("Authorization",
                                     "Basic " + (":" + password).toUtf8().toBase64());
                request.setTransferTimeout(1000);
                auto* reply = network.get(request);
                QEventLoop loop;
                QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
                loop.exec();
                if (reply->error() != QNetworkReply::NoError)
                    return QJsonObject{};
                return QJsonDocument::fromJson(reply->readAll()).object();
            };
            controller.play(id, 0);
            QTRY_VERIFY_WITH_TIMEOUT(player(), 10000);
            auto* first = player();
            QTRY_COMPARE_WITH_TIMEOUT(status(first).value("state").toString(), QString("playing"),
                                      10000);
            QCOMPARE(status(first, "?command=pl_forcepause").value("state").toString(),
                     QString("paused"));
            status(first, "?command=seek&val=3");
            status(first, "?command=pl_forceresume");
            QTRY_VERIFY_WITH_TIMEOUT(status(first).value("time").toInt() >= 3, 3000);
            status(first, "?command=pl_forcepause");
            const int savedTime = status(first).value("time").toInt();
            QVERIFY(savedTime >= 3 && savedTime < 6);
            const QString magnet = QString::fromStdString(lt::make_magnet_uri(handle));
            const QString identity =
                "xt=" + magnet.section("xt=", 1, 1).section('&', 0, 0).toLower() + ":0";
            const QString key =
                "playback/" + QString::fromLatin1(QCryptographicHash::hash(
                                                      identity.toUtf8(), QCryptographicHash::Sha256)
                                                      .toHex());
            SessionStore progress(data.filePath("sessions.sqlite"));
            QTRY_COMPARE_WITH_TIMEOUT(progress.get(key, -1).toInt(), savedTime, 5000);
            first->kill();
            QTRY_VERIFY_WITH_TIMEOUT(!player(), 5000);
            controller.clearError();
            controller.play(id, 0);
            QTRY_VERIFY_WITH_TIMEOUT(player(), 10000);
            QVERIFY(player()->arguments().contains("--start-time=" + QString::number(savedTime)));
            QTRY_VERIFY_WITH_TIMEOUT(status(player()).value("time").toInt() >= savedTime, 10000);
            player()->kill();
            QTRY_VERIFY_WITH_TIMEOUT(!player(), 5000);
        }
        if (tailIndex)
            QVERIFY(controller.sessions().first().toMap().value("downloaded").toLongLong() <
                    QFileInfo(source.filePath(name)).size() / 2);
    }
    void rejectsBlankFrames() {
        QImage frame(512, 288, QImage::Format_RGB32);
        for (const auto color : {Qt::black, Qt::white, Qt::darkGray}) {
            frame.fill(color);
            QVERIFY(!PreviewCapture::usefulFrame(frame));
        }
        frame.fill(Qt::black);
        for (int y = 120; y < 125; ++y)
            for (int x = 240; x < 245; ++x)
                frame.setPixelColor(x, y, Qt::white);
        QVERIFY(!PreviewCapture::usefulFrame(frame));
    }
    void rejectsUntrustedEndpointsAndPreservesArtworkOnFailure() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath("cover.png");
        QImage original(32, 32, QImage::Format_RGB32);
        original.fill(Qt::green);
        QVERIFY(writeArtworkImage(path, original));
        QVERIFY(!writeArtworkImage(path, QImage()));
        QCOMPARE(QImage(path), original);
        PreviewCapture capture;
        QSignalSpy ready(&capture, &PreviewCapture::ready);
        capture.submit("remote", QUrl("http://example.com/video"), findVlc(), path);
        QTest::qWait(350);
        QVERIFY(ready.isEmpty());
        QCOMPARE(QImage(path), original);
    }
};
QTEST_MAIN(PreviewTests)
#include "preview_tests.moc"
