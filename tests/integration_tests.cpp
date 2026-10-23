#include <QtEndian>
#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QEventLoop>
#include <QTimer>
#include <QProcess>
#include <QElapsedTimer>
#include <QFileInfo>
#include <cstring>
#include <memory>

#include "../src/stream/HttpServer.h"
#include "../src/torrent/TorrentEngine.h"

#include <libtorrent/bencode.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/create_torrent.hpp>
#include <libtorrent/file_storage.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/torrent_flags.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_status.hpp>
#include <libtorrent/torrent_info.hpp>

namespace lt = libtorrent;

namespace {
constexpr qsizetype MediaSize = 8 * 1024 * 1024;
constexpr int PieceSize = 256 * 1024;

class LocalTracker final : public QTcpServer {
public:
    bool open()
    {
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (hasPendingConnections()) {
                QTcpSocket* socket = nextPendingConnection();
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
                    if (socket->property("answered").toBool()) return;
                    QByteArray request = socket->property("requestBuffer").toByteArray();
                    request.append(socket->readAll());
                    socket->setProperty("requestBuffer", request);
                    if (!request.contains("\r\n\r\n")) return;
                    socket->setProperty("answered", true);
                    QByteArray compact("\x7f\0\0\1", 4);
                    compact.append(char((m_peerPort >> 8) & 0xff));
                    compact.append(char(m_peerPort & 0xff));
                    const QByteArray body = "d8:intervali1e5:peers" + QByteArray::number(compact.size())
                        + ':' + compact + 'e';
                    socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nConnection: close\r\nContent-Length: "
                        + QByteArray::number(body.size()) + "\r\n\r\n" + body);
                    socket->disconnectFromHost();
                });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
        return listen(QHostAddress::LocalHost, 0);
    }
    void setPeerPort(quint16 port) { m_peerPort = port; }
    QUrl announceUrl() const
    {
        QUrl url;
        url.setScheme("http");
        url.setHost("127.0.0.1");
        url.setPort(serverPort());
        url.setPath("/announce");
        return url;
    }
private:
    quint16 m_peerPort = 0;
};

struct HttpResult {
    int status = 0;
    QByteArray body;
    QString error;
};

HttpResult getRange(const QUrl& url, qint64 first, qint64 last)
{
    HttpResult result;
    QTcpSocket socket;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&socket, &QTcpSocket::connected, &loop, [&] {
        const QByteArray target = url.path(QUrl::FullyEncoded).toUtf8();
        socket.write("GET " + target + " HTTP/1.1\r\nHost: 127.0.0.1:" + QByteArray::number(url.port())
            + "\r\nRange: bytes=" + QByteArray::number(first) + '-' + QByteArray::number(last)
            + "\r\nConnection: close\r\n\r\n");
    });
    QObject::connect(&socket, &QTcpSocket::readyRead, &loop, [&] { result.body.append(socket.readAll()); });
    QObject::connect(&socket, &QTcpSocket::disconnected, &loop, &QEventLoop::quit);
    QObject::connect(&socket, &QTcpSocket::errorOccurred, &loop, [&](QAbstractSocket::SocketError) {
        if (socket.state() == QAbstractSocket::UnconnectedState) loop.quit();
    });
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&] { result.error = "Timed out waiting for the range response."; socket.abort(); loop.quit(); });
    timeout.start(45000);
    socket.connectToHost(url.host(), quint16(url.port()));
    loop.exec();
    if (!result.error.isEmpty()) return result;
    const qsizetype separator = result.body.indexOf("\r\n\r\n");
    if (separator < 0) { result.error = "HTTP response ended before its headers."; return result; }
    const QByteArray statusLine = result.body.left(result.body.indexOf("\r\n"));
    const QList<QByteArray> parts = statusLine.split(' ');
    if (parts.size() < 2) { result.error = "Malformed HTTP status line."; return result; }
    bool ok = false;
    result.status = parts[1].toInt(&ok);
    if (!ok) { result.error = "Invalid HTTP status code."; return result; }
    result.body = result.body.mid(separator + 4);
    return result;
}

void putLe32(QByteArray& bytes, qsizetype offset, quint32 value)
{
    qToLittleEndian(value, reinterpret_cast<uchar*>(bytes.data() + offset));
}

QByteArray makeMedia()
{
    QByteArray media(MediaSize, '\0');
    std::memcpy(media.data(), "RIFF", 4);
    putLe32(media, 4, quint32(media.size() - 8));
    std::memcpy(media.data() + 8, "WAVEfmt ", 8);
    putLe32(media, 16, 16);
    qToLittleEndian<quint16>(1, reinterpret_cast<uchar*>(media.data() + 20));
    qToLittleEndian<quint16>(1, reinterpret_cast<uchar*>(media.data() + 22));
    putLe32(media, 24, 48000);
    putLe32(media, 28, 96000);
    qToLittleEndian<quint16>(2, reinterpret_cast<uchar*>(media.data() + 32));
    qToLittleEndian<quint16>(16, reinterpret_cast<uchar*>(media.data() + 34));
    std::memcpy(media.data() + 36, "data", 4);
    putLe32(media, 40, quint32(media.size() - 44));
    for (qsizetype i = 44; i < media.size(); ++i)
        media[i] = char((quint64(i) * 31 + (quint64(i) >> 8) * 17) & 0xff);
    return media;
}

QVariantMap findSession(const QVariantList& sessions, const QString& id)
{
    for (const QVariant& item : sessions) {
        const QVariantMap map = item.toMap();
        if (map.value("id").toString() == id) return map;
    }
    return {};
}

bool awaitStop(bitfly::TorrentEngine& engine, int timeoutMs = 5000)
{
    QEventLoop loop;
    QTimer timeout;
    bool stopped = false;
    timeout.setSingleShot(true);
    QObject::connect(&engine, &bitfly::TorrentEngine::stopped, &loop, [&] { stopped = true; loop.quit(); });
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(timeoutMs);
    engine.stop();
    if (!stopped) loop.exec();
    return stopped;
}
}

class IntegrationTests final : public QObject {
    Q_OBJECT
private slots:
    void realLibtorrentStreamsRangesAndRestores_data()
    {
        QTest::addColumn<QString>("mode");
        QTest::newRow("v1") << QStringLiteral("v1");
        QTest::newRow("v2") << QStringLiteral("v2");
        QTest::newRow("hybrid") << QStringLiteral("hybrid");
    }

    void realLibtorrentStreamsRangesAndRestores()
    {
        QFETCH(QString, mode);
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const QString seedDir = temp.filePath("seed");
        const QString cacheDir = temp.filePath("cache");
        QVERIFY(QDir().mkpath(seedDir));
        QVERIFY(QDir().mkpath(cacheDir));
        const QByteArray expected = makeMedia();
        QFile sourceFile(QDir(seedDir).filePath("fixture.wav"));
        QVERIFY(sourceFile.open(QIODevice::WriteOnly));
        QCOMPARE(sourceFile.write(expected), qint64(expected.size()));
        sourceFile.close();

        LocalTracker tracker;
        QVERIFY(tracker.open());
        lt::settings_pack seedSettings;
        seedSettings.set_str(lt::settings_pack::listen_interfaces, "127.0.0.1:0");
        seedSettings.set_bool(lt::settings_pack::enable_dht, false);
        seedSettings.set_bool(lt::settings_pack::enable_lsd, false);
        seedSettings.set_bool(lt::settings_pack::enable_upnp, false);
        seedSettings.set_bool(lt::settings_pack::enable_natpmp, false);
        seedSettings.set_int(lt::settings_pack::upload_rate_limit, 256 * 1024);
        lt::session seeder(seedSettings);
        QTRY_VERIFY_WITH_TIMEOUT(seeder.listen_port() != 0, 10000);
        tracker.setPeerPort(seeder.listen_port());

        lt::file_storage files;
        files.add_file("fixture.wav", expected.size());
        lt::create_flags_t createFlags;
        if (mode == "v1") createFlags = lt::create_torrent::v1_only;
        else if (mode == "v2") createFlags = lt::create_torrent::v2_only;
        lt::create_torrent creator(files, PieceSize, createFlags);
        creator.set_priv(true);
        creator.add_tracker(tracker.announceUrl().toString(QUrl::FullyEncoded).toStdString());
        lt::set_piece_hashes(creator, QDir::toNativeSeparators(seedDir).toStdString());
        const lt::entry torrentEntry = creator.generate();
        std::vector<char> torrentBytes;
        lt::bencode(std::back_inserter(torrentBytes), torrentEntry);
        const QString torrentPath = temp.filePath("fixture.torrent");
        QFile torrentFile(torrentPath);
        QVERIFY(torrentFile.open(QIODevice::WriteOnly));
        QCOMPARE(torrentFile.write(torrentBytes.data(), qint64(torrentBytes.size())), qint64(torrentBytes.size()));
        torrentFile.close();
        lt::error_code seedError;
        lt::add_torrent_params seedParams;
        seedParams.ti = std::make_shared<lt::torrent_info>(torrentPath.toStdString());
        seedParams.save_path = QDir::toNativeSeparators(seedDir).toStdString();
        seedParams.flags |= lt::torrent_flags::seed_mode;
        seedParams.flags &= ~lt::torrent_flags::paused;
        const lt::torrent_handle seedHandle = seeder.add_torrent(std::move(seedParams), seedError);
        QVERIFY2(!seedError, seedError.message().c_str());
        seedHandle.set_upload_limit(256 * 1024);
        QTRY_VERIFY_WITH_TIMEOUT(seedHandle.status().is_seeding, 10000);

        auto engine = std::make_unique<bitfly::TorrentEngine>(cacheDir);
        QVariantList latestSessions;
        connect(engine.get(), &bitfly::TorrentEngine::sessionsChanged, this,
            [&](const QVariantList& value) { latestSessions = value; });
        QSignalSpy ready(engine.get(), &bitfly::TorrentEngine::streamReady);
        QVERIFY(ready.isValid());
        engine->add(torrentPath);
        QTRY_VERIFY_WITH_TIMEOUT(!latestSessions.isEmpty()
            && !latestSessions.first().toMap().value("name").toString().isEmpty(), 20000);
        const QString id = latestSessions.first().toMap().value("id").toString();
        QVERIFY(!id.isEmpty());
        engine->select(id, 0);
        if (ready.isEmpty()) QVERIFY(ready.wait(60000));
        QCOMPARE(ready.first().at(0).toString(), id);
        const QString streamId = ready.first().at(1).toString();
        QCOMPARE(streamId, id + "/0");
        const QVariantMap readyState = findSession(latestSessions, id);
        QVERIFY(!readyState.isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(findSession(latestSessions, id).value("downloaded").toLongLong() >= 2 * 1024 * 1024,
            10000);
        const qint64 downloadedAtReady = findSession(latestSessions, id).value("downloaded").toLongLong();
        QVERIFY2(downloadedAtReady < expected.size(),
            qPrintable(QStringLiteral("Fixture completed before streamReady in %1 mode").arg(mode)));

        auto http = std::make_unique<bitfly::HttpServer>(engine.get());
        QVERIFY(http->start());
        const QUrl streamUrl = http->url(streamId);
        QVERIFY(streamUrl.isValid());
        QElapsedTimer seekTimer;
        seekTimer.start();
        const QList<QPair<qint64, qint64>> ranges{
            {0, 64 * 1024 - 1},
            {4 * 1024 * 1024 + 37, 4 * 1024 * 1024 + 37 + 16383},
            {MediaSize - 32768, MediaSize - 1}
        };
        for (const auto& range : ranges) {
            const HttpResult response = getRange(streamUrl, range.first, range.second);
            QVERIFY2(response.error.isEmpty(), qPrintable(response.error));
            QCOMPARE(response.status, 206);
            QCOMPARE(response.body, expected.mid(range.first, range.second - range.first + 1));
        }
        qInfo().noquote() << QStringLiteral("INTEGRATION_METRICS mode=%1 downloaded_at_stream_ready=%2 seek_ranges_ms=%3")
            .arg(mode).arg(downloadedAtReady).arg(seekTimer.elapsed());

        const QString vlcPath = qEnvironmentVariable("BITFLY_TEST_VLC");
        if (!vlcPath.isEmpty()) {
            QVERIFY2(QFileInfo(vlcPath).isFile(), qPrintable(QStringLiteral("BITFLY_TEST_VLC is not an executable file: %1").arg(vlcPath)));
            const QString decodedPath = temp.filePath(QStringLiteral("vlc-decoded-%1.wav").arg(mode));
            QFile::remove(decodedPath);
            const QString sout = QStringLiteral("#transcode{acodec=s16l,channels=1,samplerate=48000}:std{access=file,mux=wav,dst=%1}")
                .arg(QDir::toNativeSeparators(decodedPath));
            QProcess vlc;
            vlc.setProcessChannelMode(QProcess::MergedChannels);
            QEventLoop vlcLoop;
            QTimer vlcTimeout;
            vlcTimeout.setSingleShot(true);
            bool vlcFinished = false;
            QObject::connect(&vlc, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), &vlcLoop,
                [&](int, QProcess::ExitStatus) { vlcFinished = true; vlcLoop.quit(); });
            QObject::connect(&vlc, &QProcess::errorOccurred, &vlcLoop, [&](QProcess::ProcessError) { vlcLoop.quit(); });
            QObject::connect(&vlcTimeout, &QTimer::timeout, &vlcLoop, [&] { vlc.kill(); vlcLoop.quit(); });
            const QStringList vlcArgs{
                "--no-one-instance", "--intf=dummy", "--no-video", "--play-and-exit", "--stop-time=1",
                "--sout=" + sout, "--sout-keep", streamUrl.toString(QUrl::FullyEncoded)
            };
            vlc.start(vlcPath, vlcArgs);
            vlcTimeout.start(90000);
            if (!vlcFinished && vlc.state() != QProcess::NotRunning) vlcLoop.exec();
            if (vlcTimeout.isActive()) vlcTimeout.stop();
            if (vlc.state() != QProcess::NotRunning) {
                vlc.kill();
                vlc.waitForFinished(3000);
            }
            QVERIFY2(vlcFinished, qPrintable(QStringLiteral("VLC did not finish decoding: %1").arg(QString::fromLocal8Bit(vlc.readAllStandardOutput()))));
            QVERIFY2(vlc.exitStatus() == QProcess::NormalExit && vlc.exitCode() == 0,
                qPrintable(QStringLiteral("VLC failed: %1").arg(QString::fromLocal8Bit(vlc.readAllStandardOutput()))));
            QFile decoded(decodedPath);
            QVERIFY2(decoded.open(QIODevice::ReadOnly), qPrintable(QStringLiteral("VLC did not produce output: %1").arg(decodedPath)));
            const QByteArray wavHeader = decoded.read(44);
            QVERIFY(wavHeader.size() == 44);
            QCOMPARE(wavHeader.left(4), QByteArray("RIFF"));
            QCOMPARE(wavHeader.mid(8, 4), QByteArray("WAVE"));
            QVERIFY2(qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(wavHeader.constData() + 40)) > 0,
                "VLC produced a WAV container without decoded audio samples.");
            qInfo().noquote() << QStringLiteral("VLC_DECODE_OK mode=%1 bytes=%2")
                .arg(mode).arg(decoded.size());
        }

        QVERIFY(awaitStop(*engine));
        http.reset();
        engine.reset();

        auto restored = std::make_unique<bitfly::TorrentEngine>(cacheDir);
        QVariantList restoredSessions;
        connect(restored.get(), &bitfly::TorrentEngine::sessionsChanged, this,
            [&](const QVariantList& value) { restoredSessions = value; });
        QSignalSpy restoredReady(restored.get(), &bitfly::TorrentEngine::streamReady);
        QVERIFY(restoredReady.isValid());
        restored->restore();
        QTRY_VERIFY_WITH_TIMEOUT(!findSession(restoredSessions, id).isEmpty(), 20000);
        QTRY_VERIFY_WITH_TIMEOUT(restoredReady.count() > 0, 60000);
        const auto restoredMap = findSession(restoredSessions, id);
        QCOMPARE(restoredMap.value("selected").toInt(), 0);
        QVERIFY(restoredMap.value("downloaded").toLongLong() > 0);
        auto restoredHttp = std::make_unique<bitfly::HttpServer>(restored.get());
        QVERIFY(restoredHttp->start());
        const HttpResult resumed = getRange(restoredHttp->url(id + "/0"), 0, 8191);
        QVERIFY2(resumed.error.isEmpty(), qPrintable(resumed.error));
        QCOMPARE(resumed.status, 206);
        QCOMPARE(resumed.body, expected.left(8192));
        QVERIFY(awaitStop(*restored));
    }
};

QTEST_GUILESS_MAIN(IntegrationTests)
#include "integration_tests.moc"
