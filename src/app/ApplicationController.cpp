#include "ApplicationController.h"
#include "NetworkAssistant.h"
#include "Platform.h"
#include "PreviewCapture.h"
#include "SearchService.h"
#include "ThumbnailArt.h"
#include "UpdateService.h"
#include "stream/HttpServer.h"
#include "torrent/TorrentEngine.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QProcessEnvironment>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimer>
#include <QUrlQuery>
#include <QUuid>

namespace bitfly {
static QString dataRoot() {
    auto root = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(root);
    return root;
}

static QString magnetKey(QString input) {
    input = input.trimmed();
    if (!input.startsWith("magnet:", Qt::CaseInsensitive))
        return input;
    for (const QString& field : input.mid(7).split('&', Qt::SkipEmptyParts))
        if (field.startsWith("xt=", Qt::CaseInsensitive))
            return field.toLower();
    return input.toLower();
}

static QString displayTitle(const QVariantMap& row) {
    const QString title = row.value("name").toString().trimmed();
    return title.isEmpty() ? QStringLiteral("Fetching metadata…") : title;
}

static bool videoFile(const QString& name) {
    static const QStringList extensions = {"mp4", "mkv",  "avi", "webm", "mov", "m4v",
                                           "ts",  "m2ts", "wmv", "mpg",  "mpeg"};
    return extensions.contains(QFileInfo(name).suffix().toLower());
}

static bool hasCachedMedia(const QVariantMap& row) {
    const QString root = row.value("savePath").toString();
    const QString relative = row.value("fileName", row.value("selectedName")).toString();
    if (root.isEmpty() || relative.isEmpty() || QDir::isAbsolutePath(relative))
        return false;
    const QString clean = QDir::cleanPath(relative);
    if (clean == ".." || clean.startsWith("../"))
        return false;
    const QFileInfo file(QDir(root).filePath(clean));
    const QString canonicalRoot = QFileInfo(root).canonicalFilePath();
    return !canonicalRoot.isEmpty() && file.isFile() && file.size() > 0 &&
           file.canonicalFilePath().startsWith(canonicalRoot + '/', Qt::CaseInsensitive);
}

ApplicationController::ApplicationController(QObject* parent, QString directory)
    : QObject(parent),
      m_dataRoot(directory.isEmpty() ? dataRoot() : QDir(directory).absolutePath()),
      m_store(m_dataRoot + "/sessions.sqlite") {
    m_cachePath = m_store.get("cachePath", m_dataRoot + "/cache").toString();
    m_vlcPath = findVlc(m_store.get("vlcPath").toString());
    m_cacheLimitGiB = qBound(1, m_store.get("cacheLimitGiB", 20).toInt(), 1024);
    m_seedMinutes = qBound(0, m_store.get("seedMinutes", 0).toInt(), 1440);
    m_connectionLimit = qBound(20, m_store.get("connectionLimit", 200).toInt(), 1000);
    m_uploadLimitKiB = qBound(0, m_store.get("uploadLimitKiB", 64).toInt(), 102400);
    m_streamAheadMiB = qBound(8, m_store.get("streamAheadMiB", 64).toInt(), 8192);
    m_advanced = m_store.get("advanced", false).toBool();
    m_playerPath = m_store.get("playerPath", m_vlcPath).toString();
    if (!validPlayer(m_playerPath))
        m_playerPath = m_vlcPath;
    m_reducedMotion = m_store.get("reducedMotion", prefersReducedMotion()).toBool();
    m_generatePreviews = m_store.get("generatePreviews", true).toBool();
    m_downloadLimitKiB = qBound(0, m_store.get("downloadLimitKiB", 0).toInt(), 102400);
    m_dhtEnabled = m_store.get("dhtEnabled", true).toBool();
    m_localDiscoveryEnabled = m_store.get("localDiscoveryEnabled", true).toBool();
    m_portMappingEnabled = m_store.get("portMappingEnabled", false).toBool();
    m_searchFeed = m_store.get("searchFeed").toString();
    m_protectedSearchKey = m_store.get("searchKeyProtected").toString();
    m_searchKey = unprotectSecret(QByteArray::fromBase64(m_protectedSearchKey.toLatin1()));
    if (m_searchKey.isEmpty())
        m_searchKey = qEnvironmentVariable("BITFLY_SEARCH_API_KEY");
    m_search = new SearchService(this);
    if (!m_searchFeed.isEmpty())
        m_search->configure(QUrl(m_searchFeed), m_searchKey);
    connect(m_search, &SearchService::changed, this, &ApplicationController::changed);
    connect(m_search, &SearchService::inputReady, this, &ApplicationController::openInput);
    connect(m_search, &SearchService::torrentReady, this, &ApplicationController::openTorrent);
    m_updates = new UpdateService(BITFLY_VERSION, this);
    connect(m_updates, &UpdateService::changed, this, [this] {
        if (m_stopping)
            return;
        m_updateMessage = m_updates->message();
        const QString version = m_updates->availableVersion();
        if (!version.isEmpty() && !m_updates->busy() && m_updates->installerPath().isEmpty() &&
            !m_updateAttempts.contains(version)) {
            m_updateAttempts.insert(version);
            QTimer::singleShot(0, m_updates, &UpdateService::download);
        }
        emit changed();
    });
    m_releaseUrl = QUrl("https://github.com/aryan-astra/BitFly-torrent-stream/releases/latest");
    m_networkAssistant = new NetworkAssistant(this);
    connect(m_networkAssistant, &NetworkAssistant::changed, this, &ApplicationController::changed);
    m_previews = new PreviewCapture(this);
    connect(m_previews, &PreviewCapture::ready, this,
            [this](QString id) { artworkReady(id, "frame"); });
    connect(m_previews, &PreviewCapture::started, this, [this](QString id) {
        m_previewStatus[id] = "Generating video preview…";
        emit changed();
    });
    connect(m_previews, &PreviewCapture::failed, this, [this](QString id) {
        if (m_stopping)
            return;
        const auto row = session(id);
        m_previewFailedBytes[id] = row.value("downloaded").toLongLong();
        const int count = ++m_previewFailures[id];
        m_previewRetryAfter[id] =
            QDateTime::currentMSecsSinceEpoch() + qMin(300000, 30000 * (1 << qMin(count - 1, 4)));
        m_previewStatus[id] = row.value("progress").toDouble() >= 1.0
                                  ? "Preview unavailable · use the artwork menu to retry"
                                  : "Waiting for video data · preview will retry automatically";
        emit changed();
    });

    m_storageRoots = m_store.get("storageRoots", QStringList{m_cachePath}).toStringList();
    if (!m_storageRoots.contains(m_cachePath))
        m_storageRoots.prepend(m_cachePath);
    m_history = m_store.get("history", QVariantList{}).toList();
    for (auto& value : m_history) {
        auto entry = value.toMap();
        if (!entry.contains("fileIndex") || entry.value("fileIndex").toInt() < 0)
            entry["fileIndex"] = entry.value("selected", -1);
        ensureThumbnail(entry);
        value = entry;
    }
    m_queue = m_store.get("queue", QVariantList{}).toList();
    m_context = new QObject;
    m_context->moveToThread(&m_worker);
    connect(&m_worker, &QThread::finished, m_context, &QObject::deleteLater);
    const auto root = m_cachePath;
    const auto roots = m_storageRoots;
    const auto initialQuota = qint64(m_cacheLimitGiB) * 1024 * 1024 * 1024;
    const int connections = m_connectionLimit;
    const int uploadKiB = m_uploadLimitKiB;
    const int downloadKiB = m_downloadLimitKiB;
    const bool dht = m_dhtEnabled, lsd = m_localDiscoveryEnabled, mapping = m_portMappingEnabled;
    const qint64 streamAhead = qint64(m_streamAheadMiB) * 1024 * 1024;
    connect(
        &m_worker, &QThread::started, m_context,
        [this, root, roots, initialQuota, connections, uploadKiB, downloadKiB, dht, lsd, mapping,
         streamAhead] {
            m_engine = new TorrentEngine(root, m_context);
            m_engine->adoptStorageRoots(roots);
            m_engine->setCacheLimitBytes(initialQuota);
            m_engine->setNetworkLimits(connections, uploadKiB, downloadKiB);
            m_engine->setDiscovery(dht, lsd, mapping);
            m_engine->setStreamAheadBytes(streamAhead);
            m_http = new HttpServer(m_engine, m_context);
            connect(m_engine, &TorrentEngine::sessionsChanged, this, [this](QVariantList value) {
                m_sessions = std::move(value);
                syncHistoryWithSessions();
                for (auto& item : m_sessions) {
                    auto row = item.toMap();
                    const auto id = row.value("id").toString();
                    if (m_generatePreviews && validVlc(m_vlcPath) &&
                        row.value("selected", -1).toInt() < 0 && !m_requested.contains(id) &&
                        !row.value("state").toString().contains("pause", Qt::CaseInsensitive) &&
                        historyEntry(id).value("fileIndex", -1).toInt() < 0 &&
                        !m_previewSelections.contains(id)) {
                        for (const auto& value : row.value("files").toList()) {
                            const auto file = value.toMap();
                            if (!file.value("playable").toBool() ||
                                !videoFile(file.value("name").toString()))
                                continue;
                            m_previewSelections.insert(id);
                            const int index = file.value("index").toInt();
                            QMetaObject::invokeMethod(
                                m_context, [this, id, index] { m_engine->select(id, index); });
                            break;
                        }
                    }
                    row["active"] = true;
                    const auto saved = historyEntry(id);
                    if (m_generatePreviews && m_previewFailedBytes.contains(id) &&
                        QDateTime::currentMSecsSinceEpoch() >= m_previewRetryAfter.value(id) &&
                        row.value("downloaded").toLongLong() > m_previewFailedBytes.value(id) &&
                        !row.value("state").toString().contains("pause", Qt::CaseInsensitive) &&
                        saved.value("thumbnailKind", "generated") == "generated") {
                        m_previewFailedBytes.remove(id);
                        QTimer::singleShot(0, this, [this, id] {
                            if (m_generatePreviews && !m_stopping && !session(id).isEmpty() &&
                                historyEntry(id).value("thumbnailKind", "generated") == "generated")
                                refreshPreview(id);
                        });
                    }
                    row["previewStatus"] = m_previewStatus.value(id);
                    if (m_generatePreviews && row.value("previewStatus").toString().isEmpty() &&
                        saved.value("thumbnailKind", "generated") == "generated" &&
                        videoFile(row.value("selectedName").toString())) {
                        row["previewStatus"] =
                            !validVlc(m_vlcPath) ? "Install VLC to create video previews"
                            : row.value("state").toString().contains("pause", Qt::CaseInsensitive)
                                ? "Preview paused"
                                : "Waiting for video preview data…";
                    }
                    row["thumbnail"] = saved.value("thumbnail");
                    row["thumbnailKind"] = saved.value("thumbnailKind", "generated");
                    row["available"] = hasCachedMedia(row);
                    row["vlcRunning"] = m_players.contains(id);
                    row["torrentState"] = row.value("state");
                    if (m_players.contains(id))
                        row["state"] = "VLC open";
                    else if (m_requested.contains(id) && row.value("error").toString().isEmpty())
                        row["state"] = "preparing";
                    item = row;
                }
                emit changed();
            });
            connect(m_engine, &TorrentEngine::cacheBytesChanged, this, [this](qint64 value) {
                m_cacheBytes = value;
                emit changed();
            });
            connect(m_engine, &TorrentEngine::error, this,
                    [this](QString message) { fail(message); });
            connect(m_engine, &TorrentEngine::removalFinished, this,
                    [this](QString id, bool, bool success) {
                        if (!m_pendingForget.remove(id)) {
                            emit changed();
                            return;
                        }
                        if (!success) {
                            fail("Could not remove the cached files. The library entry was kept.");
                            return;
                        }
                        m_forgotten.insert(id);
                        for (qsizetype i = m_history.size(); i-- > 0;)
                            if (m_history[i].toMap().value("id").toString() == id)
                                m_history.removeAt(i);
                        for (qsizetype i = m_queue.size(); i-- > 0;)
                            if (m_queue[i].toMap().value("id").toString() == id)
                                m_queue.removeAt(i);
                        m_previews->cancel(id);
                        QFile::remove(ownedThumbnailPath(m_dataRoot, id));
                        persist();
                        setNotice("Removed the entry, its artwork, and retained cached files.");
                        emit changed();
                    });
            connect(m_engine, &TorrentEngine::sessionAdded, this,
                    [this](QString id, QString input) {
                        const QString key = magnetKey(input);
                        auto pending = m_pendingRelaunch.find(key);
                        if (pending == m_pendingRelaunch.end())
                            return;
                        const int index = pending->value("fileIndex", -1).toInt();
                        m_pendingRelaunch.erase(pending);
                        if (index < 0) {
                            fail("That library entry has no saved file selection.");
                            return;
                        }
                        m_requested.insert(id, index);
                        QMetaObject::invokeMethod(
                            m_context, [this, id, index] { m_engine->select(id, index); });
                        setNotice("Reconnected the saved magnet. Buffering the selected file…");
                        emit changed();
                    });
            connect(m_engine, &TorrentEngine::streamReady, m_context,
                    [this](QString id, QString streamId) {
                        const auto url = m_http->url(streamId);
                        QMetaObject::invokeMethod(this, [this, id, url, streamId] {
                            capturePreview(id, url);
                            if (!m_requested.contains(id))
                                return;
                            if (streamId != id + "/" + QString::number(m_requested.value(id)))
                                return;
                            m_requested.remove(id);
                            launch(id, url);
                        });
                    });
            if (!m_http->start())
                QMetaObject::invokeMethod(
                    this, [this] { fail("Cannot open the local streaming server."); });
            m_engine->restore();
            QMetaObject::invokeMethod(this, [this] { emit workerReady(); });
        });
    m_worker.setObjectName("BitFly engine and HTTP");
    m_worker.start();
    if (directory.isEmpty()) {
        QTimer::singleShot(0, this, &ApplicationController::checkUpdates);
        QTimer::singleShot(0, m_networkAssistant, &NetworkAssistant::refresh);
    }
    if (!m_store.error().isEmpty())
        fail("Cannot open session database: " + m_store.error());
    else
        persist();
}
ApplicationController::~ApplicationController() {
    m_stopping = true;
    m_updates->cancel();
    m_search->cancel();
    delete m_previews;
    m_previews = nullptr;
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
void ApplicationController::setNotice(QString message) {
    m_notice = std::move(message);
}
void ApplicationController::clearError() {
    m_error.clear();
    emit changed();
}
void ApplicationController::clearNotice() {
    m_notice.clear();
    emit changed();
}
void ApplicationController::persist() {
    const QList<QPair<QString, QVariant>> values = {
        {"cachePath", m_cachePath},
        {"storageRoots", m_storageRoots},
        {"vlcPath", m_vlcPath},
        {"playerPath", m_playerPath},
        {"reducedMotion", m_reducedMotion},
        {"generatePreviews", m_generatePreviews},
        {"downloadLimitKiB", m_downloadLimitKiB},
        {"dhtEnabled", m_dhtEnabled},
        {"localDiscoveryEnabled", m_localDiscoveryEnabled},
        {"portMappingEnabled", m_portMappingEnabled},
        {"cacheLimitGiB", m_cacheLimitGiB},
        {"seedMinutes", m_seedMinutes},
        {"connectionLimit", m_connectionLimit},
        {"uploadLimitKiB", m_uploadLimitKiB},
        {"streamAheadMiB", m_streamAheadMiB},
        {"advanced", m_advanced},
        {"history", m_history},
        {"queue", m_queue},
        {"searchFeed", m_searchFeed},
        {"searchKeyProtected", m_protectedSearchKey}};
    if (!m_store.putMany(values))
        fail("Cannot save settings: " + m_store.error());
}

QVariantList ApplicationController::library() {
    QVariantList out;
    QSet<QString> active;
    for (const auto& value : m_sessions) {
        auto row = value.toMap();
        const QString id = row.value("id").toString();
        if (id.isEmpty() || m_forgotten.contains(id))
            continue;
        active.insert(id);
        row["active"] = true;
        row["dimmed"] = false;
        auto entry = historyEntry(id);
        for (const auto& key : {"thumbnail", "thumbnailKind", "thumbnailRatio"})
            if (entry.contains(key))
                row[key] = entry.value(key);
        out.append(row);
    }
    for (const auto& value : m_history) {
        auto entry = value.toMap();
        const QString id = entry.value("id").toString();
        if (id.isEmpty() || active.contains(id) || m_forgotten.contains(id))
            continue;
        entry["active"] = false;
        entry["available"] = hasCachedMedia(entry);
        entry["dimmed"] = !entry.value("available").toBool();
        entry["state"] = entry.value("available").toBool() ? "Cached" : "Not downloaded";
        entry["progress"] = 0.0;
        entry["rate"] = 0;
        entry["peers"] = 0;
        entry["buffer"] = 0.0;
        if (entry.value("name").toString().isEmpty())
            entry["name"] = QStringLiteral("Untitled torrent");
        out.append(entry);
    }
    return out;
}

void ApplicationController::syncHistoryWithSessions() {
    bool dirty = false;
    QVariantList merged;
    QSet<QString> seen;
    for (const auto& value : m_sessions) {
        const auto row = value.toMap();
        const QString id = row.value("id").toString();
        if (id.isEmpty() || m_forgotten.contains(id))
            continue;
        QVariantMap entry;
        for (const auto& existing : m_history) {
            const auto candidate = existing.toMap();
            if (candidate.value("id").toString() == id) {
                entry = candidate;
                break;
            }
        }
        if (entry.isEmpty()) {
            entry["id"] = id;
            entry["opened"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
            dirty = true;
        }
        const auto assign = [&entry, &dirty](const QString& key, const QVariant& next) {
            if (entry.value(key) != next) {
                entry[key] = next;
                dirty = true;
            }
        };
        assign("name", displayTitle(row));
        if (!row.value("magnet").toString().isEmpty())
            assign("magnet", row.value("magnet"));
        assign("input", row.value("input"));
        assign("savePath", row.value("savePath"));
        assign("total", row.value("total"));
        const int selected = row.value("selected").toInt();
        if (selected >= 0) {
            assign("selected", selected);
            assign("fileIndex", selected);
        }
        if (!row.value("selectedName").toString().isEmpty())
            assign("fileName", row.value("selectedName"));
        const QString thumbnail = entry.value("thumbnail").toString();
        ensureThumbnail(entry);
        if (entry.value("thumbnail").toString() != thumbnail)
            dirty = true;
        merged.append(entry);
        seen.insert(id);
    }
    for (const auto& value : m_history) {
        const auto entry = value.toMap();
        const QString id = entry.value("id").toString();
        if (id.isEmpty() || seen.contains(id) || m_forgotten.contains(id))
            continue;
        merged.append(entry);
        seen.insert(id);
    }
    if (merged != m_history) {
        m_history = merged;
        dirty = true;
    }
    if (dirty)
        persist();
}

void ApplicationController::rememberPlayback(QString id) {
    QVariantMap entry = historyEntry(id);
    if (entry.isEmpty()) {
        const auto current = session(id);
        entry["id"] = id;
        entry["name"] = displayTitle(current);
        entry["magnet"] = current.value("magnet");
        entry["input"] = current.value("input");
        entry["savePath"] = current.value("savePath");
        entry["total"] = current.value("total");
        const int selected = current.value("selected").toInt();
        if (selected >= 0) {
            entry["selected"] = selected;
            entry["fileIndex"] = selected;
        }
        if (!current.value("selectedName").toString().isEmpty())
            entry["fileName"] = current.value("selectedName");
    }
    entry["opened"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    ensureThumbnail(entry);
    for (int i = m_history.size() - 1; i >= 0; --i)
        if (m_history[i].toMap().value("id").toString() == id)
            m_history.removeAt(i);
    m_history.prepend(entry);
    persist();
}

void ApplicationController::ensureThumbnail(QVariantMap& entry) {
    const QString id = entry.value("id").toString();
    if (id.isEmpty())
        return;
    const QString title = entry.value("name").toString().trimmed().isEmpty()
                              ? QStringLiteral("BitFly Library")
                              : entry.value("name").toString().trimmed();
    const QString path = ownedThumbnailPath(m_dataRoot, id);
    if (!QFileInfo::exists(path)) {
        const QString legacy = QDir(m_dataRoot).filePath("thumbnails/" + id + ".png");
        if (!QUuid(id).isNull() && QFileInfo(legacy).isFile() && !QFileInfo(legacy).isSymLink()) {
            QFile::copy(legacy, path);
            if (QFileInfo::exists(path))
                QFile::remove(legacy);
        }
    }
    if (entry.value("thumbnailKind").toString().isEmpty())
        entry["thumbnailKind"] = "generated";
    const bool needsWrite =
        !QFileInfo::exists(path) || (entry.value("thumbnailKind") == "generated" &&
                                     entry.value("thumbTitle").toString() != title);
    if (needsWrite) {
        QString error;
        if (writeCoverArt(path, title, id, &error, 384))
            entry["thumbTitle"] = title;
    }
    if (QFileInfo::exists(path)) {
        auto url = QUrl::fromLocalFile(path);
        url.setQuery("v=" + QString::number(QFileInfo(path).lastModified().toMSecsSinceEpoch()));
        entry["thumbnail"] = url.toString();
    }
}
QVariantMap ApplicationController::session(QString id) const {
    for (const auto& value : m_sessions)
        if (value.toMap().value("id").toString() == id)
            return value.toMap();
    return {};
}
QVariantMap ApplicationController::historyEntry(QString id) const {
    for (const auto& value : m_history)
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
    if (!validPlayer(m_playerPath)) {
        fail("Choose an installed media player in Settings before streaming.");
        return;
    }
    if (m_players.contains(id)) {
        fail("This torrent already has an active player session. Close it before switching files.");
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
                    fail("The media player could not start. Check its executable in Settings.");
                }
            });
    connect(
        process, &QProcess::finished, this,
        [this, id, process](int exitCode, QProcess::ExitStatus exitStatus) {
            m_players.remove(id);
            process->deleteLater();
            if (m_stopping)
                return;
            const bool failed = exitCode != 0 || exitStatus != QProcess::NormalExit;
            if (failed)
                fail("The media player stopped unexpectedly. Your cached media has been retained.");
            ++m_seedGeneration[id];
            pause(id, true);
            bool continuesSameTorrent = false;
            if (!failed && !m_queue.isEmpty()) {
                const auto next = m_queue.first().toMap();
                playQueued(0);
                continuesSameTorrent =
                    next.value("id").toString() == id && m_requested.contains(id);
            }
            if (!continuesSameTorrent)
                emit playbackEnded(id);
        });
    connect(process, &QProcess::started, this, [this, id] {
        rememberPlayback(id);
        m_notice.clear();
        emit changed();
    });
    auto environment = QProcessEnvironment::systemEnvironment();
    for (const auto& key :
         {"QT_PLUGIN_PATH", "QT_QPA_PLATFORM_PLUGIN_PATH", "QML_IMPORT_PATH", "QML2_IMPORT_PATH"})
        environment.remove(key);
    environment.insert("PATH", QFileInfo(m_playerPath).absolutePath() + ";" +
                                   qEnvironmentVariable("SystemRoot", "C:/Windows") + "/System32;" +
                                   qEnvironmentVariable("SystemRoot", "C:/Windows"));
    process->setProcessEnvironment(environment);
    process->start(m_playerPath, playerArguments(m_playerPath, url));
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
    requestSavedPlayback(id, entry.value("fileIndex", -1).toInt());
    bool accepted = m_requested.contains(id);
    for (const auto& pending : m_pendingRelaunch)
        accepted |= pending.value("id").toString() == id;
    if (accepted)
        removeQueue(index);
}
void ApplicationController::pause(QString id, bool paused) {
    if (paused && m_players.contains(id)) {
        fail("Close this session’s VLC window before pausing its torrent.");
        return;
    }
    if (paused) {
        m_requested.remove(id);
        m_previews->cancel(id);
        m_previewStatus[id] = "Preview paused";
    } else if (historyEntry(id).value("thumbnailKind", "generated") == "generated") {
        m_previewFailedBytes[id] = -1;
        m_previewRetryAfter[id] = 0;
    }
    QMetaObject::invokeMethod(m_context, [this, id, paused] { m_engine->pause(id, paused); });
}
void ApplicationController::remove(QString id, bool deleteData) {
    if (m_players.contains(id)) {
        fail("Close this session’s VLC window before removing its cached data.");
        return;
    }
    m_requested.remove(id);
    m_previews->cancel(id);
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
    requestSavedPlayback(id, -1);
}
void ApplicationController::requestSavedPlayback(QString id, int requestedIndex) {
    const auto current = session(id);
    if (!current.isEmpty()) {
        play(id, requestedIndex >= 0 ? requestedIndex : current.value("selected", -1).toInt());
        return;
    }
    auto entry = historyEntry(id);
    if (entry.isEmpty()) {
        fail("This library entry is no longer available.");
        return;
    }
    QString input = entry.value("magnet").toString().trimmed();
    if (input.isEmpty())
        input = entry.value("input").toString().trimmed();
    if (input.isEmpty()) {
        fail("This entry has no saved magnet link. Open the torrent again to rebuild it.");
        return;
    }
    if (!validPlayer(m_playerPath)) {
        fail("Choose an installed media player in Settings before streaming.");
        return;
    }
    if (requestedIndex >= 0)
        entry["fileIndex"] = requestedIndex;
    const int index = entry.value("fileIndex", entry.value("selected", -1)).toInt();
    if (index < 0) {
        fail("This entry has no saved file selection. Open the torrent and choose a file.");
        return;
    }
    m_forgotten.remove(id);
    const QString key = magnetKey(input);
    if (m_pendingRelaunch.contains(key)) {
        setNotice("This torrent is already reconnecting…");
        emit changed();
        return;
    }
    const QString requestToken = QUuid::createUuid().toString();
    entry["requestToken"] = requestToken;
    m_pendingRelaunch.insert(key, entry);
    setNotice("Reconnecting the saved magnet and buffering the selected file…");
    emit changed();
    QMetaObject::invokeMethod(m_context, [this, id, input] { m_engine->reopen(id, input); });
    QTimer::singleShot(90000, this, [this, key, requestToken] {
        if (m_pendingRelaunch.value(key).value("requestToken").toString() != requestToken)
            return;
        m_pendingRelaunch.remove(key);
        fail("BitFly could not reconnect that torrent. Check the saved magnet and try again.");
    });
}

void ApplicationController::removeLibraryEntry(QString id) {
    const auto entry = historyEntry(id);
    if (entry.isEmpty())
        return;
    if (!session(id).isEmpty()) {
        fail("Stop and remove the active session instead. Its cached files are still in use.");
        return;
    }
    if (m_players.contains(id)) {
        fail("Close this session’s VLC window before deleting it from the library.");
        return;
    }
    if (m_pendingForget.contains(id))
        return;
    ++m_seedGeneration[id];
    m_pendingForget.insert(id);
    m_previews->cancel(id);
    QMetaObject::invokeMethod(m_context, [this, id] { m_engine->purge(id); });
    setNotice("Removing the saved entry and its retained files…");
    emit changed();
}

void ApplicationController::openStorageFolder() {
    if (!QDir().mkpath(m_cachePath)) {
        fail("Could not create the download folder.");
        return;
    }
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(m_cachePath)))
        fail("Could not open the download folder.");
}

void ApplicationController::revealSession(QString id) {
    QString path;
    const auto current = session(id);
    if (!current.isEmpty())
        path = current.value("savePath").toString();
    else {
        const auto entry = historyEntry(id);
        path = entry.value("savePath").toString();
        if (path.isEmpty())
            path = entry.value("input").toString();
    }
    if (path.isEmpty())
        path = m_cachePath;
    QFileInfo info(path);
    if (!info.exists()) {
        if (!QDir().mkpath(info.isDir() || info.suffix().isEmpty() ? path : info.absolutePath())) {
            fail("Could not open the storage location for this entry.");
            return;
        }
        info = QFileInfo(path);
    }
    const QUrl folder =
        QUrl::fromLocalFile(info.isDir() ? info.absoluteFilePath() : info.absolutePath());
    if (!QDesktopServices::openUrl(folder))
        fail("Could not open this entry’s storage folder.");
}

void ApplicationController::setVlcPath(QUrl url) {
    if (!url.isLocalFile() || !validVlc(url.toLocalFile())) {
        fail("Choose the VLC installation’s vlc.exe file.");
        return;
    }
    m_vlcPath = url.toLocalFile();
    m_playerPath = m_vlcPath;
    persist();
    emit changed();
}
void ApplicationController::setCachePath(QUrl url) {
    if (!url.isLocalFile() || !QFileInfo(url.toLocalFile()).isDir()) {
        fail("Choose an existing writable download folder.");
        return;
    }
    QTemporaryFile probe(QDir(url.toLocalFile()).filePath(".bitfly-write-test-XXXXXX"));
    if (!probe.open()) {
        fail("BitFly cannot write to this download folder. Choose another location.");
        return;
    }
    const QString path = QDir::cleanPath(url.toLocalFile());
    if (path == m_cachePath) {
        setNotice("New downloads already save to this folder.");
        emit changed();
        return;
    }
    QMetaObject::invokeMethod(m_context, [this, path] {
        m_engine->setStorageRoot(path);
        const QString actual = m_engine->storageRoot();
        const QString expected = QFileInfo(QDir(path).filePath("torrents")).canonicalFilePath();
        if (expected.isEmpty() || actual != expected)
            return;
        QMetaObject::invokeMethod(this, [this, actual] {
            if (m_stopping)
                return;
            m_cachePath = QFileInfo(actual).absolutePath();
            m_storageRoots.removeAll(m_cachePath);
            m_storageRoots.prepend(m_cachePath);
            persist();
            setNotice("New downloads save here. Active sessions keep their original folder. "
                      "If streaming could not initialize at startup, restart BitFly.");
            emit changed();
        });
    });
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
void ApplicationController::applyNetworkSettings() {
    const int connections = m_connectionLimit, upload = m_uploadLimitKiB,
              download = m_downloadLimitKiB;
    QMetaObject::invokeMethod(m_context, [this, connections, upload, download] {
        m_engine->setNetworkLimits(connections, upload, download);
    });
    persist();
    emit changed();
}
void ApplicationController::setConnectionLimit(int value) {
    m_connectionLimit = qBound(20, value, 1000);
    applyNetworkSettings();
}
void ApplicationController::setUploadLimitKiB(int value) {
    m_uploadLimitKiB = qBound(0, value, 102400);
    applyNetworkSettings();
}
void ApplicationController::setDownloadLimitKiB(int value) {
    m_downloadLimitKiB = qBound(0, value, 102400);
    applyNetworkSettings();
}
void ApplicationController::applyDiscoverySettings() {
    const bool dht = m_dhtEnabled, lsd = m_localDiscoveryEnabled, mapping = m_portMappingEnabled;
    QMetaObject::invokeMethod(
        m_context, [this, dht, lsd, mapping] { m_engine->setDiscovery(dht, lsd, mapping); });
    persist();
    emit changed();
}
void ApplicationController::setDhtEnabled(bool value) {
    m_dhtEnabled = value;
    applyDiscoverySettings();
}
void ApplicationController::setLocalDiscoveryEnabled(bool value) {
    m_localDiscoveryEnabled = value;
    applyDiscoverySettings();
}
void ApplicationController::setPortMappingEnabled(bool value) {
    m_portMappingEnabled = value;
    applyDiscoverySettings();
}
void ApplicationController::setReducedMotion(bool value) {
    m_reducedMotion = value;
    persist();
    emit changed();
}
void ApplicationController::setGeneratePreviews(bool value) {
    m_generatePreviews = value;
    if (!value)
        for (const auto& row : m_sessions)
            m_previews->cancel(row.toMap().value("id").toString());
    if (value)
        for (const auto& value : m_sessions) {
            const auto row = value.toMap();
            const auto id = row.value("id").toString();
            if (row.value("selected", -1).toInt() >= 0 &&
                historyEntry(id).value("thumbnailKind", "generated") == "generated")
                refreshPreview(id);
        }
    persist();
    emit changed();
}
void ApplicationController::setStreamAheadMiB(int value) {
    m_streamAheadMiB = qBound(8, value, 8192);
    const auto bytes = qint64(m_streamAheadMiB) * 1024 * 1024;
    QMetaObject::invokeMethod(m_context, [this, bytes] { m_engine->setStreamAheadBytes(bytes); });
    persist();
    emit changed();
}
void ApplicationController::setAdvanced(bool value) {
    m_advanced = value;
    persist();
    emit changed();
}
QString ApplicationController::playerName() const {
    const auto name = QFileInfo(m_playerPath).completeBaseName();
    if (name.compare("vlc", Qt::CaseInsensitive) == 0)
        return "VLC";
    if (name.startsWith("mpc-", Qt::CaseInsensitive))
        return "Media Player Classic";
    return name.isEmpty() ? "Media player" : name;
}
void ApplicationController::setPlayerPath(QUrl url) {
    if (!url.isLocalFile() || !validPlayer(url.toLocalFile())) {
        fail("Choose vlc.exe, mpv.exe, or an MPC-HC/BE executable.");
        return;
    }
    m_playerPath = QFileInfo(url.toLocalFile()).absoluteFilePath();
    if (validVlc(m_playerPath))
        m_vlcPath = m_playerPath;
    persist();
    emit changed();
}
void ApplicationController::capturePreview(QString id, QUrl url) {
    const auto row = session(id);
    if (!m_generatePreviews || m_stopping || !validVlc(m_vlcPath) || row.isEmpty() ||
        m_forgotten.contains(id))
        return;
    const auto entry = historyEntry(id);
    const QString kind = entry.value("thumbnailKind", "generated").toString();
    if (kind == "custom" || (kind == "frame" && PreviewCapture::usefulFrame(
                                                    QImage(ownedThumbnailPath(m_dataRoot, id)))))
        return;
    if (row.value("state").toString().contains("pause", Qt::CaseInsensitive))
        return;
    if (videoFile(row.value("selectedName").toString()))
        m_previews->submit(id, url, m_vlcPath, ownedThumbnailPath(m_dataRoot, id));
}
void ApplicationController::artworkReady(QString id, QString kind) {
    if (m_stopping || m_forgotten.contains(id) || m_pendingForget.contains(id))
        return;
    m_previewStatus.remove(id);
    m_previewFailedBytes.remove(id);
    m_previewFailures.remove(id);
    for (auto& value : m_history) {
        auto row = value.toMap();
        if (row.value("id").toString() != id)
            continue;
        row["thumbnailKind"] = kind;
        row["thumbnailRatio"] =
            QImageReader(ownedThumbnailPath(m_dataRoot, id)).size().width() /
            double(qMax(1, QImageReader(ownedThumbnailPath(m_dataRoot, id)).size().height()));
        ensureThumbnail(row);
        value = row;
        persist();
        emit changed();
        return;
    }
}
void ApplicationController::refreshPreview(QString id) {
    if (!m_generatePreviews || !validVlc(m_vlcPath)) {
        fail("Enable video previews and select an installed VLC in Settings to capture a frame.");
        return;
    }
    if (session(id).value("selected", -1).toInt() < 0) {
        fail("Select a video in an active session to capture its preview.");
        return;
    }
    for (auto& value : m_history) {
        auto row = value.toMap();
        if (row.value("id").toString() == id) {
            row["thumbnailKind"] = "generated";
            value = row;
        }
    }
    const int index = session(id).value("selected", -1).toInt();
    QMetaObject::invokeMethod(m_context, [this, id, index] {
        const QUrl url = m_http->url(id + "/" + QString::number(index));
        QMetaObject::invokeMethod(this, [this, id, url] { capturePreview(id, url); });
    });
}
void ApplicationController::setLibraryArtwork(QString id, QUrl file) {
    if (historyEntry(id).isEmpty() || !file.isLocalFile() ||
        QFileInfo(file.toLocalFile()).size() > 20 * 1024 * 1024) {
        fail("Choose a local image under 20 MB for an existing library entry.");
        return;
    }
    QImageReader reader(file.toLocalFile());
    reader.setAutoTransform(true);
    const auto size = reader.size();
    if (!size.isValid() || size.width() > 4096 || size.height() > 4096) {
        fail("Choose an image no larger than 4096 × 4096 pixels.");
        return;
    }
    reader.setScaledSize(size.scaled(512, 512, Qt::KeepAspectRatio));
    const auto image = reader.read();
    if (image.isNull()) {
        fail("This image could not be opened.");
        return;
    }
    m_previews->cancel(id);
    if (!writeArtworkImage(ownedThumbnailPath(m_dataRoot, id), image)) {
        fail("Could not save the library artwork.");
        return;
    }
    artworkReady(id, "custom");
}
QVariantList ApplicationController::searchResults() const {
    return m_search->results();
}
bool ApplicationController::searchBusy() const {
    return m_search->busy();
}
QString ApplicationController::searchMessage() const {
    return m_search->message();
}
void ApplicationController::configureSearch(QString feed, QString apiKey) {
    QUrl url(feed.trimmed());
    QUrlQuery query(url);
    if (apiKey.isEmpty())
        apiKey = query.queryItemValue("apikey", QUrl::FullyDecoded);
    query.removeAllQueryItems("apikey");
    url.setQuery(query);
    if (apiKey.isEmpty())
        apiKey = m_searchKey;
    const auto encrypted = protectSecret(apiKey);
    if (!apiKey.isEmpty() && encrypted.isEmpty()) {
        fail("Could not protect the indexer key with your Windows account.");
        return;
    }
    if (!m_search->configure(url, apiKey)) {
        emit changed();
        return;
    }
    m_searchFeed = url.toString();
    m_searchKey = apiKey;
    m_protectedSearchKey = QString::fromLatin1(encrypted.toBase64());
    persist();
    emit changed();
}
void ApplicationController::search(QString query) {
    m_search->search(std::move(query));
}
void ApplicationController::cancelSearch() {
    m_search->cancel();
}
void ApplicationController::openSearchResult(int index) {
    m_search->openResult(index);
}
bool ApplicationController::updateBusy() const {
    return m_updates->busy();
}
double ApplicationController::updateProgress() const {
    return m_updates->progress();
}
bool ApplicationController::updateReady() const {
    return !m_updates->installerPath().isEmpty();
}
QString ApplicationController::availableVersion() const {
    return m_updates->availableVersion();
}
void ApplicationController::checkUpdates() {
    m_updateAttempts.clear();
    m_updates->check();
}
void ApplicationController::downloadUpdate() {
    m_updates->download();
}
void ApplicationController::installUpdate() {
    if (!m_players.isEmpty() || !m_sessions.isEmpty()) {
        fail("Stop active torrent sessions and close their players before installing an update.");
        return;
    }
    if (!m_updates->launchInstaller())
        return;
    QCoreApplication::quit();
}
bool ApplicationController::cloudflareInstalled() const {
    return m_networkAssistant->installed();
}
bool ApplicationController::cloudflareBusy() const {
    return m_networkAssistant->busy();
}
QString ApplicationController::cloudflareStatus() const {
    return m_networkAssistant->status();
}
void ApplicationController::refreshCloudflare() {
    m_networkAssistant->refresh();
}
void ApplicationController::connectCloudflare() {
    m_networkAssistant->connectClient();
}
void ApplicationController::disconnectCloudflare() {
    m_networkAssistant->disconnectClient();
}
void ApplicationController::setCloudflareMode(QString mode) {
    m_networkAssistant->setMode(std::move(mode));
}
void ApplicationController::openCloudflareDownload() {
    m_networkAssistant->openDownloadPage();
}
void ApplicationController::openCloudflareClient() {
    m_networkAssistant->openClient();
}
} // namespace bitfly
