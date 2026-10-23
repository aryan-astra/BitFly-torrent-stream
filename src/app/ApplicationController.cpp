#include "ApplicationController.h"
#include "Platform.h"
#include "stream/HttpServer.h"
#include "torrent/TorrentEngine.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimer>

namespace bitfly {
static QString dataRoot() {
    auto root = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(root);
    return root;
}
ApplicationController::ApplicationController(QObject* parent)
    : QObject(parent), m_store(dataRoot() + "/sessions.sqlite") {
    m_cachePath = m_store.get("cachePath", dataRoot() + "/cache").toString();
    m_vlcPath = findVlc(m_store.get("vlcPath").toString());
    m_cacheLimitGiB = qBound(1, m_store.get("cacheLimitGiB", 20).toInt(), 1024);
    m_seedMinutes = qBound(0, m_store.get("seedMinutes", 15).toInt(), 1440);
    m_advanced = m_store.get("advanced", false).toBool();
    m_history = m_store.get("history", QVariantList{}).toList();
    m_queue = m_store.get("queue", QVariantList{}).toList();
    m_context = new QObject;
    m_context->moveToThread(&m_worker);
    connect(&m_worker, &QThread::finished, m_context, &QObject::deleteLater);
    const auto root = m_cachePath;
    const auto initialQuota = qint64(m_cacheLimitGiB) * 1024 * 1024 * 1024;
    connect(&m_worker, &QThread::started, m_context, [this, root, initialQuota] {
        m_engine = new TorrentEngine(root, m_context);
        m_engine->setCacheLimitBytes(initialQuota);
        m_http = new HttpServer(m_engine, m_context);
        connect(m_engine, &TorrentEngine::sessionsChanged, this, [this](QVariantList value) {
            for (auto& item : value) {
                auto row = item.toMap();
                const auto id = row.value("id").toString();
                row["vlcRunning"] = m_players.contains(id);
                row["torrentState"] = row.value("state");
                if (m_players.contains(id))
                    row["state"] = "VLC open";
                else if (m_requested.contains(id) && row.value("error").toString().isEmpty())
                    row["state"] = "preparing";
                item = row;
            }
            m_sessions = std::move(value);
            emit changed();
        });
        connect(m_engine, &TorrentEngine::cacheBytesChanged, this, [this](qint64 value) {
            m_cacheBytes = value;
            emit changed();
        });
        connect(m_engine, &TorrentEngine::error, this, &ApplicationController::fail);
        connect(m_engine, &TorrentEngine::streamReady, m_context,
                [this](QString id, QString streamId) {
                    const auto url = m_http->url(streamId);
                    QMetaObject::invokeMethod(this, [this, id, url, streamId] {
                        if (!m_requested.contains(id))
                            return;
                        if (streamId != id + "/" + QString::number(m_requested.value(id)))
                            return;
                        m_requested.remove(id);
                        launch(id, url);
                    });
                });
        if (!m_http->start())
            QMetaObject::invokeMethod(this,
                                      [this] { fail("Cannot open the local streaming server."); });
        m_engine->restore();
        QMetaObject::invokeMethod(this, [this] { emit workerReady(); });
    });
    m_worker.setObjectName("BitFly engine and HTTP");
    m_worker.start();
    if (!m_store.error().isEmpty())
        fail("Cannot open session database: " + m_store.error());
}
ApplicationController::~ApplicationController() {
    m_stopping = true;
    persist();
    for (auto* player : m_players) {
        disconnect(player, nullptr, this, nullptr);
        player->terminate();
        if (!player->waitForFinished(1500)) {
            player->kill();
            player->waitForFinished(500);
        }
    }
    QEventLoop shutdown;
    QMetaObject::invokeMethod(m_context, [this, &shutdown] {
        connect(m_engine, &TorrentEngine::stopped, &shutdown, &QEventLoop::quit,
                Qt::QueuedConnection);
        m_http->stop();
        m_engine->stop();
    });
    QTimer::singleShot(4000, &shutdown, &QEventLoop::quit);
    shutdown.exec();
    m_worker.quit();
    m_worker.wait();
}
void ApplicationController::fail(QString message) {
    m_error = std::move(message);
    emit changed();
}
void ApplicationController::clearError() {
    m_error.clear();
    emit changed();
}
void ApplicationController::persist() {
    const QList<QPair<QString, QVariant>> values = {{"cachePath", m_cachePath},
                                                    {"vlcPath", m_vlcPath},
                                                    {"cacheLimitGiB", m_cacheLimitGiB},
                                                    {"seedMinutes", m_seedMinutes},
                                                    {"advanced", m_advanced},
                                                    {"history", m_history},
                                                    {"queue", m_queue}};
    if (!m_store.putMany(values))
        fail("Cannot save settings: " + m_store.error());
}
QVariantMap ApplicationController::session(QString id) const {
    for (const auto& value : m_sessions)
        if (value.toMap().value("id").toString() == id)
            return value.toMap();
    return {};
}
void ApplicationController::openInput(QString input) {
    input = input.trimmed();
    if (input.isEmpty() || input.size() > 16384) {
        fail("Enter a magnet link or select a .torrent file.");
        return;
    }
    QMetaObject::invokeMethod(m_context, [this, input] { m_engine->add(input); });
}
void ApplicationController::openTorrent(QUrl url) {
    if (!url.isLocalFile()) {
        fail("Select a local .torrent file.");
        return;
    }
    openInput(url.toLocalFile());
}
void ApplicationController::play(QString id, int index) {
    if (!validVlc(m_vlcPath)) {
        fail("VLC was not found. Install VLC or choose vlc.exe in Settings.");
        return;
    }
    if (m_players.contains(id)) {
        fail("This torrent already has an active VLC session. Close it before switching files.");
        return;
    }
    const auto current = session(id);
    bool found = false;
    for (const auto& file : current.value("files").toList()) {
        const auto f = file.toMap();
        if (f.value("index").toInt() == index && f.value("playable").toBool()) {
            found = true;
            break;
        }
    }
    if (!found) {
        fail("This media is unavailable. Open the torrent and select a playable file.");
        return;
    }
    ++m_seedGeneration[id];
    m_requested.insert(id, index);
    QMetaObject::invokeMethod(m_context, [this, id, index] { m_engine->select(id, index); });
}
void ApplicationController::launch(QString id, QUrl url) {
    if (m_stopping || m_players.contains(id))
        return;
    if (!url.isValid() || url.scheme() != "http" || url.host() != "127.0.0.1") {
        fail("The local streaming endpoint is unavailable. Restart BitFly and try again.");
        return;
    }
    auto* process = new QProcess(this);
    m_players.insert(id, process);
    // A dedicated VLC instance makes its lifetime attributable to this session.
    process->setStandardOutputFile(QProcess::nullDevice());
    process->setStandardErrorFile(QProcess::nullDevice());
    connect(process, &QProcess::errorOccurred, this,
            [this, id, process](QProcess::ProcessError value) {
                if (value == QProcess::FailedToStart) {
                    m_players.remove(id);
                    process->deleteLater();
                    fail("VLC could not start. Check the VLC executable in Settings.");
                }
            });
    connect(process, &QProcess::finished, this,
            [this, id, process](int exitCode, QProcess::ExitStatus exitStatus) {
                m_players.remove(id);
                process->deleteLater();
                if (m_stopping)
                    return;
                const bool failed = exitCode != 0 || exitStatus != QProcess::NormalExit;
                if (failed)
                    fail("VLC stopped unexpectedly. Your cached media has been retained.");
                const int generation = ++m_seedGeneration[id];
                QTimer::singleShot(m_seedMinutes * 60000, this, [this, id, generation] {
                    if (m_seedGeneration.value(id) == generation && !m_players.contains(id))
                        pause(id, true);
                });
                bool continuesSameTorrent = false;
                if (!failed && !m_queue.isEmpty()) {
                    auto next = m_queue.takeFirst().toMap();
                    persist();
                    emit changed();
                    continuesSameTorrent = next.value("id").toString() == id;
                    play(next.value("id").toString(), next.value("fileIndex").toInt());
                }
                if (!continuesSameTorrent)
                    emit playbackEnded(id);
            });
    connect(process, &QProcess::started, this, [this, id] {
        const auto current = session(id);
        QVariantMap record{{"id", id},
                           {"name", current.value("name")},
                           {"selected", current.value("selected")},
                           {"opened", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}};
        for (int i = m_history.size() - 1; i >= 0; --i)
            if (m_history[i].toMap().value("id").toString() == id)
                m_history.removeAt(i);
        m_history.prepend(record);
        while (m_history.size() > 100)
            m_history.removeLast();
        persist();
        emit changed();
    });
    process->start(m_vlcPath,
                   {"--no-one-instance", "--play-and-exit", url.toString(QUrl::FullyEncoded)});
}
void ApplicationController::enqueue(QString id, int index) {
    if (m_queue.size() >= 200) {
        fail("The playback queue is full.");
        return;
    }
    auto s = session(id);
    QString title = s.value("name").toString();
    for (const auto& f : s.value("files").toList())
        if (f.toMap().value("index").toInt() == index)
            title = f.toMap().value("name").toString();
    m_queue.append(QVariantMap{{"id", id}, {"fileIndex", index}, {"name", title}});
    persist();
    emit changed();
}
void ApplicationController::moveQueue(int from, int to) {
    if (from < 0 || to < 0 || from >= m_queue.size() || to >= m_queue.size())
        return;
    m_queue.move(from, to);
    persist();
    emit changed();
}
void ApplicationController::removeQueue(int index) {
    if (index < 0 || index >= m_queue.size())
        return;
    m_queue.removeAt(index);
    persist();
    emit changed();
}
void ApplicationController::playQueued(int index) {
    if (index < 0 || index >= m_queue.size())
        return;
    const auto entry = m_queue[index].toMap();
    const auto id = entry.value("id").toString();
    if (!validVlc(m_vlcPath) || m_players.contains(id) || session(id).isEmpty()) {
        fail("Check VLC and make sure this torrent is available before playing the queue.");
        return;
    }
    removeQueue(index);
    play(id, entry.value("fileIndex").toInt());
}
void ApplicationController::pause(QString id, bool paused) {
    if (paused && m_players.contains(id)) {
        fail("Close this session’s VLC window before pausing its torrent.");
        return;
    }
    if (paused)
        m_requested.remove(id);
    QMetaObject::invokeMethod(m_context, [this, id, paused] { m_engine->pause(id, paused); });
}
void ApplicationController::remove(QString id, bool deleteData) {
    if (m_players.contains(id)) {
        fail("Close this session’s VLC window before removing its cached data.");
        return;
    }
    m_requested.remove(id);
    ++m_seedGeneration[id];
    QMetaObject::invokeMethod(m_context,
                              [this, id, deleteData] { m_engine->remove(id, deleteData); });
    for (int i = m_queue.size() - 1; i >= 0; --i)
        if (m_queue[i].toMap().value("id").toString() == id)
            m_queue.removeAt(i);
    persist();
    emit changed();
}
void ApplicationController::finishSession(QString id, QString action) {
    if (action == "delete")
        remove(id, true);
    else if (action == "keep") {
        ++m_seedGeneration[id];
        pause(id, true);
    } else if (action == "seed") {
        pause(id, false);
        const auto generation = ++m_seedGeneration[id];
        QTimer::singleShot(m_seedMinutes * 60000, this, [this, id, generation] {
            if (m_seedGeneration.value(id) == generation && !m_players.contains(id))
                pause(id, true);
        });
    }
}
void ApplicationController::relaunch(QString id) {
    const auto s = session(id);
    play(id, s.value("selected", -1).toInt());
}
void ApplicationController::setVlcPath(QUrl url) {
    if (!url.isLocalFile() || !validVlc(url.toLocalFile())) {
        fail("Choose the VLC installation’s vlc.exe file.");
        return;
    }
    m_vlcPath = url.toLocalFile();
    persist();
    emit changed();
}
void ApplicationController::setCachePath(QUrl url) {
    if (!url.isLocalFile() || !QFileInfo(url.toLocalFile()).isDir()) {
        fail("Choose an existing writable cache folder.");
        return;
    }
    QTemporaryFile probe(QDir(url.toLocalFile()).filePath(".bitfly-write-test-XXXXXX"));
    if (!probe.open()) {
        fail("BitFly cannot write to this cache folder. Choose another location.");
        return;
    }
    m_cachePath = QDir::cleanPath(url.toLocalFile());
    persist();
    fail("Cache folder saved. Restart BitFly to use it. Existing media stays in its current "
         "folder.");
}
void ApplicationController::setCacheLimitGiB(int value) {
    m_cacheLimitGiB = qBound(1, value, 1024);
    const auto bytes = qint64(m_cacheLimitGiB) * 1024 * 1024 * 1024;
    QMetaObject::invokeMethod(m_context, [this, bytes] { m_engine->setCacheLimitBytes(bytes); });
    persist();
    emit changed();
}
void ApplicationController::setSeedMinutes(int value) {
    m_seedMinutes = qBound(0, value, 1440);
    persist();
    emit changed();
}
void ApplicationController::setAdvanced(bool value) {
    m_advanced = value;
    persist();
    emit changed();
}
void ApplicationController::checkUpdates() {
    if (m_updatePending)
        return;
    const auto last = m_store.get("lastUpdateCheck", qint64(0)).toLongLong();
    if (QDateTime::currentSecsSinceEpoch() - last < 3600) {
        m_updateMessage = "Update checks are limited to once per hour.";
        emit changed();
        return;
    }
    m_updatePending = true;
    m_updateMessage = "Checking official releases…";
    emit changed();
    QNetworkRequest request(
        QUrl("https://api.github.com/repos/aryan-astra/BitFly-torrent-stream/releases/latest"));
    request.setHeader(QNetworkRequest::UserAgentHeader, "BitFly/" BITFLY_VERSION);
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setTransferTimeout(10000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::SameOriginRedirectPolicy);
    auto* reply = m_network.get(request);
    connect(reply, &QNetworkReply::readyRead, this, [reply] {
        if (reply->bytesAvailable() > 256 * 1024)
            reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        m_updatePending = false;
        m_store.put("lastUpdateCheck", QDateTime::currentSecsSinceEpoch());
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 404)
            m_updateMessage = "No stable release has been published yet.";
        else if (reply->error() != QNetworkReply::NoError)
            m_updateMessage = "Could not check updates. Try again later.";
        else {
            const auto json = QJsonDocument::fromJson(reply->readAll()).object();
            const auto version = stableVersion(json.value("tag_name").toString());
            const auto current = stableVersion(BITFLY_VERSION);
            if (!version || json.value("prerelease").toBool() || json.value("draft").toBool())
                m_updateMessage = "No stable release information is available.";
            else if (current && *version > *current) {
                m_updateMessage = "BitFly " + json.value("tag_name").toString() + " is available.";
                m_releaseUrl =
                    QUrl("https://github.com/aryan-astra/BitFly-torrent-stream/releases/latest");
            } else
                m_updateMessage = "You’re using the latest published version.";
        }
        reply->deleteLater();
        emit changed();
    });
}
} // namespace bitfly
