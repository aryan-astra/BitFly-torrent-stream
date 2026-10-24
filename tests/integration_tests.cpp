#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <QtEndian>
#include <QtTest>
#include <cstdint>
#include <cstring>
#include <memory>

#include "../src/stream/HttpServer.h"
#include "../src/torrent/TorrentEngine.h"

#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/create_torrent.hpp>
#include <libtorrent/file_storage.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/torrent_flags.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/torrent_status.hpp>

namespace lt = libtorrent;

namespace {
constexpr qsizetype MediaSize = 8 * 1024 * 1024;

class LocalTracker final : public QTcpServer {
  public:
    bool open() {
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (hasPendingConnections()) {
                QTcpSocket* socket = nextPendingConnection();
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
                    if (socket->property("answered").toBool())
                        return;
                    QByteArray request = socket->property("requestBuffer").toByteArray();
                    request.append(socket->readAll());
                    socket->setProperty("requestBuffer", request);
                    if (!request.contains("\r\n\r\n"))
                        return;
                    ++m_requestCount;
                    socket->setProperty("answered", true);
                    // Never return the seeder to itself. The announce port is plain ASCII in
                    // the request target, even though info_hash and peer_id are percent-encoded.
                    const int portAt = request.indexOf("port=");
                    const int portEnd = portAt < 0 ? -1 : request.indexOf('&', portAt);
                    bool fromSeeder = false;
                    if (portAt >= 0) {
                        const QByteArray announcedPort = request.mid(
                            portAt + 5,
                            (portEnd < 0 ? request.indexOf(" ", portAt) : portEnd) - (portAt + 5));
                        fromSeeder = announcedPort.toUShort() == m_peerPort;
                    }
                    QByteArray compact;
                    if (!fromSeeder) {
                        compact.append("\x7f\0\0\1", 4);
                        compact.append(char((m_peerPort >> 8) & 0xff));
                        compact.append(char(m_peerPort & 0xff));
                    }
                    const QByteArray body = "d8:intervali1e8:completei1e10:incompletei0e5:peers" +
                                            QByteArray::number(compact.size()) + ':' + compact +
                                            'e';
                    socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nConnection: "
                                  "close\r\nContent-Length: " +
                                  QByteArray::number(body.size()) + "\r\n\r\n" + body);
                    socket->disconnectFromHost();
                });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
        return listen(QHostAddress::LocalHost, 0);
    }
    void setPeerPort(quint16 port) {
        m_peerPort = port;
    }
    int requestCount() const {
        return m_requestCount;
    }
    QUrl announceUrl() const {
        QUrl url;
        url.setScheme("http");
        url.setHost("127.0.0.1");
        url.setPort(serverPort());
        url.setPath("/announce");
        return url;
    }

  private:
    quint16 m_peerPort = 0;
    int m_requestCount = 0;
};

QStringList peerDiagnostics(lt::session& session) {
    std::vector<lt::alert*> alerts;
    session.pop_alerts(&alerts);
    QStringList lines;
    for (const auto* alert : alerts) {
        if (lt::alert_cast<lt::peer_connect_alert>(alert) ||
            lt::alert_cast<lt::peer_disconnected_alert>(alert) ||
            lt::alert_cast<lt::peer_error_alert>(alert) ||
            lt::alert_cast<lt::peer_blocked_alert>(alert) ||
            lt::alert_cast<lt::incoming_connection_alert>(alert) ||
            lt::alert_cast<lt::listen_failed_alert>(alert))
            lines.append(QString::fromStdString(alert->message()));
    }
    return lines;
}

struct HttpResult {
    int status = 0;
    QByteArray body;
    QString error;
};

HttpResult getRange(const QUrl& url, qint64 first, qint64 last) {
    HttpResult result;
    QTcpSocket socket;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&socket, &QTcpSocket::connected, &loop, [&] {
        const QByteArray target = url.path(QUrl::FullyEncoded).toUtf8();
        socket.write("GET " + target +
                     " HTTP/1.1\r\nHost: 127.0.0.1:" + QByteArray::number(url.port()) +
                     "\r\nRange: bytes=" + QByteArray::number(first) + '-' +
                     QByteArray::number(last) + "\r\nConnection: close\r\n\r\n");
    });
    QObject::connect(&socket, &QTcpSocket::readyRead, &loop,
                     [&] { result.body.append(socket.readAll()); });
    QObject::connect(&socket, &QTcpSocket::disconnected, &loop, &QEventLoop::quit);
    QObject::connect(&socket, &QTcpSocket::errorOccurred, &loop, [&](QAbstractSocket::SocketError) {
        if (socket.state() == QAbstractSocket::UnconnectedState)
            loop.quit();
    });
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&] {
        result.error = "Timed out waiting for the range response.";
        socket.abort();
        loop.quit();
    });
    timeout.start(45000);
    socket.connectToHost(url.host(), quint16(url.port()));
    loop.exec();
    if (!result.error.isEmpty())
        return result;
    const qsizetype separator = result.body.indexOf("\r\n\r\n");
    if (separator < 0) {
        result.error = "HTTP response ended before its headers.";
        return result;
    }
    const QByteArray statusLine = result.body.left(result.body.indexOf("\r\n"));
    const QList<QByteArray> parts = statusLine.split(' ');
    if (parts.size() < 2) {
        result.error = "Malformed HTTP status line.";
        return result;
    }
    bool ok = false;
    result.status = parts[1].toInt(&ok);
    if (!ok) {
        result.error = "Invalid HTTP status code.";
        return result;
    }
    result.body = result.body.mid(separator + 4);
    return result;
}

void putLe32(QByteArray& bytes, qsizetype offset, quint32 value) {
    qToLittleEndian(value, reinterpret_cast<uchar*>(bytes.data() + offset));
}

QByteArray makeMedia() {
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

QVariantMap findSession(const QVariantList& sessions, const QString& id) {
    for (const QVariant& item : sessions) {
        const QVariantMap map = item.toMap();
        if (map.value("id").toString() == id)
            return map;
    }
    return {};
}

bool awaitStop(bitfly::TorrentEngine& engine, int timeoutMs = 5000) {
    QEventLoop loop;
    QTimer timeout;
    bool stopped = false;
    timeout.setSingleShot(true);
    QObject::connect(&engine, &bitfly::TorrentEngine::stopped, &loop, [&] {
        stopped = true;
        loop.quit();
    });
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(timeoutMs);
    engine.stop();
    if (!stopped)
        loop.exec();
    return stopped;
}
} // namespace

class IntegrationTests final : public QObject {
    Q_OBJECT
  private slots:
    void realLibtorrentStreamsRangesAndRestores_data() {
        QTest::addColumn<QString>("mode");
        QTest::addColumn<QString>("fault");
        QTest::newRow("v1") << QStringLiteral("v1") << QString();
        QTest::newRow("v1magnet") << QStringLiteral("v1magnet") << QString();
        QTest::newRow("v2") << QStringLiteral("v2") << QString();
        QTest::newRow("hybrid") << QStringLiteral("hybrid") << QString();
        QTest::newRow("v1-corrupt-resume") << QStringLiteral("v1") << QStringLiteral("resume");
        QTest::newRow("v2-corrupt-resume") << QStringLiteral("v2") << QStringLiteral("resume");
        QTest::newRow("v1-quota") << QStringLiteral("v1") << QStringLiteral("quota");
    }

    void realLibtorrentStreamsRangesAndRestores() {
        QFETCH(QString, mode);
        QFETCH(QString, fault);
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
        // Multiple libtorrent sessions in this process otherwise share the default peer ID and
        // reject each other as self-connections, preventing a realistic local swarm test.
        seedSettings.set_str(lt::settings_pack::peer_fingerprint, "BFSEEDER-TEST-000001");
        seedSettings.set_int(lt::settings_pack::upload_rate_limit, 256 * 1024);
        const lt::alert_category_t seedAlerts =
            lt::alert_category::error | lt::alert_category::peer | lt::alert_category::connect |
            lt::alert_category::status;
        seedSettings.set_int(lt::settings_pack::alert_mask,
                             static_cast<int>(static_cast<std::uint32_t>(seedAlerts)));
        lt::session seeder(seedSettings);
        QTRY_VERIFY_WITH_TIMEOUT(seeder.listen_port() != 0, 10000);
        tracker.setPeerPort(seeder.listen_port());

        lt::file_storage files;
        files.add_file("fixture.wav", expected.size());
        lt::create_flags_t createFlags;
        if (mode == "v1" || mode == "v1magnet")
            createFlags = lt::create_torrent::v1_only;
        else if (mode == "v2")
            createFlags = lt::create_torrent::v2_only;
        const int pieceSize = (mode == "v1" || mode == "v1magnet") ? 16 * 1024 : 256 * 1024;
        lt::create_torrent creator(files, pieceSize, createFlags);
        // libtorrent deliberately omits the ut_metadata extension for private torrents, so a
        // private fixture can never resolve from a magnet link. Keep the .torrent rows private
        // to exercise that path and make the magnet rows public, as real magnet swarms are.
        const bool magnetMode = mode == "v1magnet" || mode == "v2";
        creator.set_priv(!magnetMode);
        creator.add_tracker(tracker.announceUrl().toString(QUrl::FullyEncoded).toStdString());
        lt::set_piece_hashes(creator, QDir::toNativeSeparators(seedDir).toStdString());
        const lt::entry torrentEntry = creator.generate();
        std::vector<char> torrentBytes;
        lt::bencode(std::back_inserter(torrentBytes), torrentEntry);
        const QString torrentPath = temp.filePath("fixture.torrent");
        QFile torrentFile(torrentPath);
        QVERIFY(torrentFile.open(QIODevice::WriteOnly));
        QCOMPARE(torrentFile.write(torrentBytes.data(), qint64(torrentBytes.size())),
                 qint64(torrentBytes.size()));
        torrentFile.close();
        lt::error_code seedError;
        lt::add_torrent_params seedParams;
        seedParams.ti = std::make_shared<lt::torrent_info>(torrentPath.toStdString());
        seedParams.save_path = QDir::toNativeSeparators(seedDir).toStdString();
        seedParams.flags |= lt::torrent_flags::seed_mode;
        seedParams.flags &= ~(lt::torrent_flags::paused | lt::torrent_flags::auto_managed);
        const lt::torrent_handle seedHandle = seeder.add_torrent(std::move(seedParams), seedError);
        QVERIFY2(!seedError, seedError.message().c_str());
        seedHandle.set_upload_limit(256 * 1024);
        QTRY_VERIFY_WITH_TIMEOUT(seedHandle.status().is_seeding, 10000);
        QStringList seedDiagnostics;
        QTimer seedAlertTimer;
        connect(&seedAlertTimer, &QTimer::timeout, this,
                [&] { seedDiagnostics.append(peerDiagnostics(seeder)); });
        seedAlertTimer.start(25);

        auto engine = std::make_unique<bitfly::TorrentEngine>(cacheDir);
        if (fault == "quota")
            engine->setCacheLimitBytes(0);
        QVariantList latestSessions;
        connect(engine.get(), &bitfly::TorrentEngine::sessionsChanged, this,
                [&](const QVariantList& value) { latestSessions = value; });
        QSignalSpy ready(engine.get(), &bitfly::TorrentEngine::streamReady);
        QVERIFY(ready.isValid());
        QSignalSpy engineErrors(engine.get(), &bitfly::TorrentEngine::error);
        QVERIFY(engineErrors.isValid());
        QSignalSpy engineDiagnostics(engine.get(), &bitfly::TorrentEngine::diagnostic);
        QVERIFY(engineDiagnostics.isValid());
        QString engineInput = torrentPath;
        QString inputKind = QStringLiteral("torrent-file");
        if (mode == "v2" || mode == "v1magnet") {
            engineInput = QString::fromStdString(lt::make_magnet_uri(seedHandle)) +
                          QStringLiteral("&x.pe=127.0.0.1:") +
                          QString::number(seeder.listen_port());
            inputKind = QStringLiteral("magnet");
        }
        qInfo().noquote() << QStringLiteral("INTEGRATION_INPUT mode=%1 kind=%2 input=%3")
                                 .arg(mode, inputKind, engineInput);
        engine->add(engineInput);
        QTRY_VERIFY2_WITH_TIMEOUT(
            !latestSessions.isEmpty() &&
                !latestSessions.first().toMap().value("name").toString().isEmpty(),
            qPrintable(
                QStringLiteral(
                    "Torrent metadata did not resolve. mode=%1 sessions=%2 trackerAnnounces=%3 "
                    "seederPeers=%4 engineErrors=%5 engineDiagnostics=%6 seedDiagnostics=%7")
                    .arg(mode)
                    .arg(QString::fromUtf8(
                        QJsonDocument::fromVariant(latestSessions).toJson(QJsonDocument::Compact)))
                    .arg(tracker.requestCount())
                    .arg(seedHandle.status().num_peers)
                    .arg([&] {
                        QStringList values;
                        for (const auto& value : engineErrors)
                            values.append(value.value(0).toString());
                        return values.join(QStringLiteral(" | "));
                    }())
                    .arg([&] {
                        QStringList values;
                        for (const auto& value : engineDiagnostics)
                            values.append(value.value(0).toString());
                        return values.join(QStringLiteral(" | "));
                    }())
                    .arg(seedDiagnostics.join(QStringLiteral(" | ")))),
            20000);
        const QString id = latestSessions.first().toMap().value("id").toString();
        QVERIFY(!id.isEmpty());
        if (fault == "quota") {
            QTRY_VERIFY_WITH_TIMEOUT(
                !findSession(latestSessions, id).value("error").toString().isEmpty(), 10000);
            QCOMPARE(findSession(latestSessions, id).value("downloaded").toLongLong(), 0);
            QCOMPARE(ready.count(), 0);
            QVERIFY(QFileInfo::exists(QDir(cacheDir).filePath("torrents/" + id + "/.torrent")));
            engine->setCacheLimitBytes(64 * 1024 * 1024);
        }
        engine->select(id, 0);
        if (ready.isEmpty() && !ready.wait(60000)) {
            const QString snapshot =
                QString::fromUtf8(QJsonDocument::fromVariant(findSession(latestSessions, id))
                                      .toJson(QJsonDocument::Compact));
            QStringList errors;
            for (const auto& error : engineErrors)
                errors.append(error.value(0).toString());
            for (const auto& diagnostic : engineDiagnostics)
                errors.append(diagnostic.value(0).toString());
            const QString diagnostic =
                QStringLiteral("Timed out waiting for verified startup buffer. snapshot=%1 "
                               "engineErrors=%2 trackerAnnounces=%3 seederPeers=%4")
                    .arg(snapshot, errors.join(QStringLiteral(" | ")))
                    .arg(tracker.requestCount())
                    .arg(seedHandle.status().num_peers);
            const auto seedStatus = seedHandle.status();
            const QString seederState =
                QStringLiteral(
                    "seeder_state=%1 paused=%2 seeding=%3 progress=%4 seedDiagnostics=%5")
                    .arg(int(seedStatus.state))
                    .arg(bool(seedStatus.flags & lt::torrent_flags::paused))
                    .arg(seedStatus.is_seeding)
                    .arg(seedStatus.progress_ppm)
                    .arg(seedDiagnostics.join(QStringLiteral(" | ")));
            QFAIL(qPrintable(diagnostic + QLatin1Char(' ') + seederState));
        }
        QCOMPARE(ready.first().at(0).toString(), id);
        const QString streamId = ready.first().at(1).toString();
        QCOMPARE(streamId, id + "/0");
        const QVariantMap readyState = findSession(latestSessions, id);
        QVERIFY(!readyState.isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(findSession(latestSessions, id).value("downloaded").toLongLong() >=
                                     2 * 1024 * 1024,
                                 10000);
        const qint64 downloadedAtReady =
            findSession(latestSessions, id).value("downloaded").toLongLong();
        QVERIFY2(downloadedAtReady < expected.size(),
                 qPrintable(
                     QStringLiteral("Fixture completed before streamReady in %1 mode").arg(mode)));

        auto http = std::make_unique<bitfly::HttpServer>(engine.get());
        QVERIFY(http->start());
        const QUrl streamUrl = http->url(streamId);
        QVERIFY(streamUrl.isValid());
        QElapsedTimer seekTimer;
        seekTimer.start();
        const QList<QPair<qint64, qint64>> ranges{
            {0, 64 * 1024 - 1},
            {4 * 1024 * 1024 + 37, 4 * 1024 * 1024 + 37 + 16383},
            {MediaSize - 32768, MediaSize - 1}};
        for (const auto& range : ranges) {
            const HttpResult response = getRange(streamUrl, range.first, range.second);
            QVERIFY2(response.error.isEmpty(), qPrintable(response.error));
            QCOMPARE(response.status, 206);
            QCOMPARE(response.body, expected.mid(range.first, range.second - range.first + 1));
        }
        qInfo().noquote()
            << QStringLiteral(
                   "INTEGRATION_METRICS mode=%1 downloaded_at_stream_ready=%2 seek_ranges_ms=%3")
                   .arg(mode)
                   .arg(downloadedAtReady)
                   .arg(seekTimer.elapsed());

        const QString vlcPath = qEnvironmentVariable("BITFLY_TEST_VLC");
        if (!vlcPath.isEmpty()) {
            QVERIFY2(
                QFileInfo(vlcPath).isFile(),
                qPrintable(
                    QStringLiteral("BITFLY_TEST_VLC is not an executable file: %1").arg(vlcPath)));
            const QString decodedPath =
                temp.filePath(QStringLiteral("vlc-decoded-%1.wav").arg(mode));
            struct VlcResult {
                bool finished = false;
                bool timedOut = false;
                QProcess::ExitStatus exitStatus = QProcess::CrashExit;
                int exitCode = -1;
                QString output;
                QString log;
            };
            const auto runVlc = [&](const QString& input, const QString& outputPath,
                                    const QString& logPath) {
                VlcResult result;
                QFile::remove(outputPath);
                QFile::remove(logPath);
                const QString sout =
                    QStringLiteral("#transcode{acodec=s16l,channels=1,samplerate=48000}:"
                                   "std{access=file,mux=wav,dst=%1}")
                        .arg(QDir::toNativeSeparators(outputPath));
                QProcess vlc;
                vlc.setProcessChannelMode(QProcess::MergedChannels);
                QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
                env.remove("QT_PLUGIN_PATH");
                env.remove("QT_QPA_PLATFORM_PLUGIN_PATH");
                env.insert("PATH", QFileInfo(vlcPath).absolutePath() + QLatin1Char(';') +
                                       qEnvironmentVariable("SystemRoot") +
                                       QStringLiteral("\\System32;") +
                                       qEnvironmentVariable("SystemRoot"));
                vlc.setProcessEnvironment(env);
                QEventLoop vlcLoop;
                QTimer vlcTimeout;
                vlcTimeout.setSingleShot(true);
                QObject::connect(&vlc, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                                 &vlcLoop, [&](int, QProcess::ExitStatus) {
                                     result.finished = true;
                                     vlcLoop.quit();
                                 });
                QObject::connect(&vlc, &QProcess::errorOccurred, &vlcLoop,
                                 [&](QProcess::ProcessError) { vlcLoop.quit(); });
                QObject::connect(&vlcTimeout, &QTimer::timeout, &vlc, [&] {
                    result.timedOut = true;
                    vlc.kill();
                    vlcLoop.quit();
                });
                const QStringList vlcArgs{"--no-one-instance",    "--intf=dummy",   "--no-video",
                                          "--play-and-exit",      "--stop-time=1",  "--no-loop",
                                          "--no-repeat",          "--file-logging", "--verbose=2",
                                          "--logfile=" + logPath, "--sout=" + sout, input};
                vlc.start(vlcPath, vlcArgs);
                vlcTimeout.start(30000);
                if (!result.finished && vlc.state() != QProcess::NotRunning)
                    vlcLoop.exec();
                if (vlcTimeout.isActive())
                    vlcTimeout.stop();
                if (vlc.state() != QProcess::NotRunning) {
                    vlc.kill();
                    vlc.waitForFinished(3000);
                }
                result.exitStatus = vlc.exitStatus();
                result.exitCode = vlc.exitCode();
                result.output = QString::fromLocal8Bit(vlc.readAllStandardOutput());
                QFile log(logPath);
                if (log.open(QIODevice::ReadOnly))
                    result.log = QString::fromLocal8Bit(log.readAll());
                return result;
            };
            const QString localOutput = temp.filePath(QStringLiteral("vlc-local-%1.wav").arg(mode));
            const QString localLog = temp.filePath(QStringLiteral("vlc-local-%1.log").arg(mode));
            const VlcResult localResult =
                runVlc(QUrl::fromLocalFile(sourceFile.fileName()).toString(QUrl::FullyEncoded),
                       localOutput, localLog);
            if (localResult.timedOut || !localResult.finished ||
                localResult.exitStatus != QProcess::NormalExit || localResult.exitCode != 0) {
                temp.setAutoRemove(false);
                QFAIL(qPrintable(QStringLiteral("VLC local-file baseline failed: timed_out=%1 "
                                                "finished=%2 status=%3 code=%4 "
                                                "stdout=%5 log=%6 artifacts=%7")
                                     .arg(localResult.timedOut)
                                     .arg(localResult.finished)
                                     .arg(int(localResult.exitStatus))
                                     .arg(localResult.exitCode)
                                     .arg(localResult.output, localResult.log, temp.path())));
            }
            qInfo().noquote() << QStringLiteral("VLC_LOCAL_DECODE_OK mode=%1 bytes=%2")
                                     .arg(mode)
                                     .arg(QFileInfo(localOutput).size());

            const QString vlcLog = temp.filePath(QStringLiteral("vlc-stream-%1.log").arg(mode));
            const VlcResult vlcResult =
                runVlc(streamUrl.toString(QUrl::FullyEncoded), decodedPath, vlcLog);
            if (vlcResult.timedOut || !vlcResult.finished ||
                vlcResult.exitStatus != QProcess::NormalExit || vlcResult.exitCode != 0) {
                temp.setAutoRemove(false);
                QFAIL(qPrintable(
                    QStringLiteral(
                        "VLC HTTP decode failed: timed_out=%1 finished=%2 status=%3 code=%4 "
                        "stdout=%5 log=%6 artifacts=%7")
                        .arg(vlcResult.timedOut)
                        .arg(vlcResult.finished)
                        .arg(int(vlcResult.exitStatus))
                        .arg(vlcResult.exitCode)
                        .arg(vlcResult.output, vlcResult.log, temp.path())));
            }
            QFile decoded(decodedPath);
            QVERIFY2(decoded.open(QIODevice::ReadOnly),
                     qPrintable(QStringLiteral("VLC did not produce output: %1").arg(decodedPath)));
            const QByteArray wavHeader = decoded.read(44);
            QVERIFY(wavHeader.size() == 44);
            QCOMPARE(wavHeader.left(4), QByteArray("RIFF"));
            QCOMPARE(wavHeader.mid(8, 4), QByteArray("WAVE"));
            QVERIFY2(qFromLittleEndian<quint32>(
                         reinterpret_cast<const uchar*>(wavHeader.constData() + 40)) > 0,
                     "VLC produced a WAV container without decoded audio samples.");
            qInfo().noquote()
                << QStringLiteral("VLC_DECODE_OK mode=%1 bytes=%2").arg(mode).arg(decoded.size());
        }

        QVERIFY(awaitStop(*engine));
        http.reset();
        engine.reset();

        if (fault == "resume") {
            const QString sessionDir = QDir(cacheDir).filePath("torrents/" + id);
            QFile resume(QDir(sessionDir).filePath(".fastresume"));
            QVERIFY(resume.size() > 0);
            QVERIFY(QFileInfo::exists(QDir(sessionDir).filePath(".torrent")));
            QVERIFY(QFileInfo::exists(QDir(sessionDir).filePath("data/fixture.wav")));
            QVERIFY(resume.open(QIODevice::WriteOnly | QIODevice::Truncate));
            const QByteArray corrupt("not bencoded resume data");
            QCOMPARE(resume.write(corrupt), qint64(corrupt.size()));
            resume.close();
        }

        auto restored = std::make_unique<bitfly::TorrentEngine>(cacheDir);
        QVariantList restoredSessions;
        connect(restored.get(), &bitfly::TorrentEngine::sessionsChanged, this,
                [&](const QVariantList& value) { restoredSessions = value; });
        QSignalSpy restoredReady(restored.get(), &bitfly::TorrentEngine::streamReady);
        QVERIFY(restoredReady.isValid());
        QSignalSpy restoreDiagnostics(restored.get(), &bitfly::TorrentEngine::diagnostic);
        restored->restore();
        QTRY_VERIFY_WITH_TIMEOUT(!findSession(restoredSessions, id).isEmpty(), 20000);
        QTRY_VERIFY_WITH_TIMEOUT(restoredReady.count() > 0, 60000);
        const auto restoredMap = findSession(restoredSessions, id);
        QCOMPARE(restoredMap.value("selected").toInt(), 0);
        QTRY_VERIFY_WITH_TIMEOUT(
            findSession(restoredSessions, id).value("downloaded").toLongLong() > 0, 10000);
        auto restoredHttp = std::make_unique<bitfly::HttpServer>(restored.get());
        QVERIFY(restoredHttp->start());
        const HttpResult resumed = getRange(restoredHttp->url(id + "/0"), 0, 8191);
        QVERIFY2(resumed.error.isEmpty(), qPrintable(resumed.error));
        QCOMPARE(resumed.status, 206);
        QCOMPARE(resumed.body, expected.left(8192));
        if (fault == "resume") {
            bool fallbackObserved = false;
            for (const auto& diagnostic : restoreDiagnostics)
                fallbackObserved |= diagnostic.first().toString().contains("falling back");
            QVERIFY2(fallbackObserved, "Corrupt resume must use stored metadata, not stale state.");
        }
        QVERIFY(awaitStop(*restored));
    }
};

QTEST_GUILESS_MAIN(IntegrationTests)
#include "integration_tests.moc"
