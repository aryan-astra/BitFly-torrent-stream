#include "TorrentEngine.h"

#include <QCollator>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStorageInfo>
#include <QUuid>
#include <algorithm>
#include <limits>

#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/load_torrent.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/read_resume_data.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/torrent_status.hpp>
#include <libtorrent/write_resume_data.hpp>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace bitfly {
namespace lt = libtorrent;
namespace {
constexpr qint64 MaxTorrentBytes = 32 * 1024 * 1024;
constexpr qint64 LowDiskFreeBytes = 1024LL * 1024 * 1024;
constexpr int CacheConcurrentReads = 2;
constexpr int MaxTorrentFiles = 10000;
constexpr int MaxTorrentPieces = 2000000;
constexpr int MaxPieceBytes = 16 * 1024 * 1024;
constexpr int MaxSessions = 256;

bool playable(const QString& name) {
    static const QStringList extensions = {
        ".avi",  ".mkv", ".mov", ".mp4", ".m4v",  ".mpeg", ".mpg", ".webm", ".wmv", ".ts",
        ".m2ts", ".mp3", ".m4a", ".aac", ".flac", ".wav",  ".ogg", ".opus", ".wma"};
    return extensions.contains(QFileInfo(name).suffix().prepend('.').toLower());
}

bool isUuid(const QString& id) {
    static const QRegularExpression re(
        "^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$",
        QRegularExpression::CaseInsensitiveOption);
    return re.match(id).hasMatch();
}

bool isLinkOrReparsePoint(const QFileInfo& info);

bool hasOwnershipMarker(const QString& dir, const QString& id) {
    const QString path = QDir(dir).filePath(".bitfly-session");
    const QFileInfo info(path);
    if (!info.isFile() || isLinkOrReparsePoint(info) || info.size() > 64)
        return false;
    QFile marker(path);
    return marker.open(QIODevice::ReadOnly) && QString::fromUtf8(marker.readAll()).trimmed() == id;
}

QString cacheKey(const QString& id, int piece) {
    return id + QLatin1Char(':') + QString::number(piece);
}

QString statusName(lt::torrent_status::state_t state) {
    switch (state) {
    case lt::torrent_status::checking_files:
        return "checking";
    case lt::torrent_status::checking_resume_data:
        return "checking";
    case lt::torrent_status::downloading_metadata:
        return "metadata";
    case lt::torrent_status::downloading:
        return "downloading";
    case lt::torrent_status::finished:
        return "finished";
    case lt::torrent_status::seeding:
        return "seeding";
    default:
        return "paused";
    }
}

qint64 allocatedSize(const QFileInfo& info) {
#ifdef Q_OS_WIN
    DWORD high = 0;
    SetLastError(NO_ERROR);
    const DWORD low =
        GetCompressedFileSizeW(reinterpret_cast<LPCWSTR>(info.absoluteFilePath().utf16()), &high);
    if (low != INVALID_FILE_SIZE || GetLastError() == NO_ERROR)
        return (qint64(high) << 32) | qint64(low);
#endif
    return info.size();
}

bool isLinkOrReparsePoint(const QFileInfo& info) {
#ifdef Q_OS_WIN
    const DWORD attributes =
        GetFileAttributesW(reinterpret_cast<LPCWSTR>(info.absoluteFilePath().utf16()));
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
    return info.isSymLink();
#endif
}
} // namespace

struct TorrentEngine::Torrent {
    QString id, dir, input, name, error;
    lt::torrent_handle handle;
    int selected = -1, requestedFile = -1;
    qint64 lastDemandMs = -1, metadataStartedMs = -1;
    bool metadataChecked = false, userPaused = false, streamReadyEmitted = false;
    QSet<int> demandedPieces, readPending;
};

TorrentEngine::TorrentEngine(QString cacheRoot, QObject* parent)
    : QObject(parent), m_root(QDir(cacheRoot).filePath("torrents")) {
    QDir().mkpath(m_root);
    const QFileInfo rootInfo(m_root);
    const QString canonicalRoot = rootInfo.canonicalFilePath();
    const QString absoluteRoot = QDir::cleanPath(rootInfo.absoluteFilePath());
    const bool pathChanged = QString::compare(absoluteRoot, QDir::cleanPath(canonicalRoot),
#ifdef Q_OS_WIN
                                              Qt::CaseInsensitive
#else
                                              Qt::CaseSensitive
#endif
                                              ) != 0;
    if (isLinkOrReparsePoint(rootInfo) || canonicalRoot.isEmpty() || pathChanged) {
        m_root.clear();
        QTimer::singleShot(0, this, [this] {
            emit error("The managed torrent cache directory is unavailable or is a symbolic link.");
        });
        return;
    }
    m_root = canonicalRoot;
    lt::settings_pack settings;
    settings.set_int(lt::settings_pack::alert_mask,
                     int(lt::alert_category::error | lt::alert_category::storage |
                         lt::alert_category::status | lt::alert_category::file_progress |
                         lt::alert_category::peer | lt::alert_category::piece_progress));
    settings.set_bool(lt::settings_pack::enable_dht, true);
    settings.set_bool(lt::settings_pack::enable_lsd, true);
    settings.set_bool(lt::settings_pack::enable_upnp, false);
    settings.set_bool(lt::settings_pack::enable_natpmp, false);
    settings.set_int(lt::settings_pack::connections_limit, 120);
    m_session = std::make_unique<lt::session>(settings);
    m_uptime.start();
    m_lastDiskScan.invalidate();
    m_pollTimer.setInterval(50);
    connect(&m_pollTimer, &QTimer::timeout, this, &TorrentEngine::poll);
    m_publishTimer.setInterval(500);
    connect(&m_publishTimer, &QTimer::timeout, this, &TorrentEngine::publish);
    connect(&m_shutdownTimer, &QTimer::timeout, this, &TorrentEngine::finishStop);
    m_pollTimer.start();
    m_publishTimer.start();
}

TorrentEngine::~TorrentEngine() {
    if (m_session)
        m_session->abort();
    qDeleteAll(m_torrents);
}

QString TorrentEngine::torrentDir(const QString& id) const {
    return QDir(m_root).filePath(id);
}

void TorrentEngine::add(QString input) {
    if (m_stopping || !m_session)
        return;
    if (m_root.isEmpty()) {
        emit error("The managed torrent cache directory is unavailable.");
        return;
    }
    if (m_torrents.size() >= MaxSessions) {
        emit error("The maximum number of active torrent sessions has been reached.");
        return;
    }
    input = input.trimmed();
    if (input.isEmpty() || input.size() > 16384) {
        emit error("Input must be a magnet link or a local .torrent file.");
        return;
    }
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    addTorrent(id, input);
}

void TorrentEngine::addTorrent(const QString& id, const QString& input, bool restoring) {
    if (!isUuid(id) || input.isEmpty() || m_torrents.contains(id) ||
        m_torrents.size() >= MaxSessions)
        return;
    lt::add_torrent_params params;
    const QString dir = torrentDir(id);
    if (restoring && !hasOwnershipMarker(dir, id))
        return;
    if (!restoring && QFileInfo::exists(dir)) {
        emit error("Generated torrent storage ID already exists; refusing to reuse it.");
        return;
    }
    if (!QDir().mkpath(dir)) {
        emit error("Could not create managed torrent storage.");
        return;
    }
    if (!restoring)
        writeAtomic(QDir(dir).filePath(".bitfly-session"), id.toUtf8());
    if (!hasOwnershipMarker(dir, id)) {
        emit error("Could not mark managed torrent storage as app-owned.");
        return;
    }
    const QString dataDir = QDir(dir).filePath("data");
    QDir().mkpath(dataDir);
    const QString canonicalRoot = QFileInfo(m_root).canonicalFilePath();
    const QString canonicalData = QFileInfo(dataDir).canonicalFilePath();
    const QString relativeData = QDir(canonicalRoot).relativeFilePath(canonicalData);
    if (isLinkOrReparsePoint(QFileInfo(dir)) || isLinkOrReparsePoint(QFileInfo(dataDir)) ||
        canonicalData.isEmpty() || QDir::isAbsolutePath(relativeData) || relativeData == ".." ||
        relativeData.startsWith("../") || relativeData.startsWith("..\\")) {
        emit error("The managed torrent storage path is unsafe.");
        return;
    }
    params.save_path = QDir::toNativeSeparators(dataDir).toStdString();
    params.flags |= lt::torrent_flags::default_dont_download;
    params.storage_mode = lt::storage_mode_sparse;
    bool hadResume = false;

    const bool magnetInput = input.startsWith("magnet:", Qt::CaseInsensitive);
    if (magnetInput) {
        lt::error_code ec;
        lt::parse_magnet_uri(input.toStdString(), params, ec);
        if (ec) {
            emit error(QString::fromStdString(ec.message()));
            return;
        }
    } else if (restoring) {
        params.flags |= lt::torrent_flags::paused | lt::torrent_flags::default_dont_download;
        bool resumeLoaded = false;
        QFile resume(QDir(dir).filePath(".fastresume"));
        if (resume.size() > 0 && resume.size() <= MaxTorrentBytes &&
            resume.open(QIODevice::ReadOnly)) {
            const QByteArray bytes = resume.read(MaxTorrentBytes + 1);
            if (bytes.size() <= MaxTorrentBytes) {
                lt::error_code ec;
                params = lt::read_resume_data({bytes.constData(), bytes.size()}, ec);
                if (!ec) {
                    params.save_path = QDir::toNativeSeparators(dataDir).toStdString();
                    resumeLoaded = true;
                    hadResume = true;
                }
            }
        }
        if (!resumeLoaded) {
            const QString torrentFile = QDir(dir).filePath(".torrent");
            QFile file(torrentFile);
            if (!file.open(QIODevice::ReadOnly) || file.size() > MaxTorrentBytes)
                return;
            try {
                params = lt::load_torrent_file(torrentFile.toStdString());
            } catch (const lt::system_error&) {
                return;
            }
            params.save_path = QDir::toNativeSeparators(dataDir).toStdString();
            params.flags |= lt::torrent_flags::default_dont_download | lt::torrent_flags::paused;
        }
    } else {
        params.flags |= lt::torrent_flags::paused;
        QFileInfo info(input);
        if (!info.isFile() || info.size() <= 0 || info.size() > MaxTorrentBytes) {
            emit error("Torrent file must be between 1 byte and 32 MiB.");
            return;
        }
        try {
            params = lt::load_torrent_file(info.absoluteFilePath().toStdString());
        } catch (const lt::system_error& e) {
            emit error(QString::fromStdString(e.what()));
            return;
        }
        params.save_path = QDir::toNativeSeparators(dataDir).toStdString();
        params.flags |= lt::torrent_flags::default_dont_download | lt::torrent_flags::paused;
        QFile source(info.absoluteFilePath());
        if (source.open(QIODevice::ReadOnly))
            writeAtomic(QDir(dir).filePath(".torrent"), source.read(MaxTorrentBytes + 1));
    }

    params.flags |= lt::torrent_flags::default_dont_download;
    params.flags &= ~lt::torrent_flags::auto_managed;
    if (magnetInput)
        params.flags &= ~lt::torrent_flags::paused;
    else if (!restoring || !hadResume)
        params.flags |= lt::torrent_flags::paused;
    if (restoring && hadResume)
        params.flags |= lt::torrent_flags::override_resume_data;
    auto* t = new Torrent;
    t->id = id;
    t->dir = dir;
    t->input = input;
    t->userPaused =
        restoring && hadResume && static_cast<bool>(params.flags & lt::torrent_flags::paused);
    if (restoring && params.ti) {
        for (int i = 0;
             i < int(params.file_priorities.size()) && i < params.ti->files().num_files(); ++i) {
            const auto fi = lt::file_index_t(i);
            if (params.file_priorities[size_t(i)] != lt::dont_download &&
                playable(QString::fromStdString(params.ti->files().file_path(fi)))) {
                t->selected = i;
                break;
            }
        }
    }
    params.userdata = t;
    m_torrents.insert(id, t);
    m_session->async_add_torrent(std::move(params));
    publish();
}

void TorrentEngine::restore() {
    if (!m_session || m_root.isEmpty())
        return;
    QDirIterator it(m_root, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::NoIteratorFlags);
    while (it.hasNext()) {
        const QString path = it.next();
        const QFileInfo info(path);
        const QString id = info.fileName();
        if (isLinkOrReparsePoint(info) || !isUuid(id) ||
            QFileInfo(QDir(path).filePath(".retained")).exists() || !hasOwnershipMarker(path, id))
            continue;
        addTorrent(id, "restore", true);
    }
}

bool TorrentEngine::validateMetadata(Torrent& t) {
    if (t.metadataChecked)
        return true;
    const auto ti = t.handle.torrent_file();
    if (!ti)
        return false;
    const auto& files = ti->files();
    QSet<QString> foldedPaths;
    if (files.num_files() <= 0 || files.num_files() > MaxTorrentFiles || ti->num_pieces() <= 0 ||
        ti->num_pieces() > MaxTorrentPieces || ti->piece_length() <= 0 ||
        ti->piece_length() > MaxPieceBytes) {
        t.error = "Torrent metadata exceeds supported file, piece, or memory limits.";
        t.handle.pause();
        m_session->remove_torrent(t.handle);
        emit error(t.error);
        return false;
    }
    for (auto i : files.file_range()) {
        const QString path = QString::fromStdString(files.file_path(i));
        QString folded = path;
        folded.replace('\\', '/');
        folded = folded.toCaseFolded();
        if (!isSafeTorrentPath(path) || foldedPaths.contains(folded) || !files.symlink(i).empty()) {
            t.error = "Torrent contains an unsafe path or symbolic link.";
            t.handle.pause();
            m_session->remove_torrent(t.handle);
            emit error(t.error);
            return false;
        }
        foldedPaths.insert(folded);
    }
    t.name = QString::fromStdString(ti->name());
    t.metadataChecked = true;
    configureFiles(t);
    try {
        t.handle.save_resume_data(lt::torrent_handle::save_info_dict);
        ++m_resumePending;
    } catch (const lt::system_error&) {
    }
    return true;
}

void TorrentEngine::configureFiles(Torrent& t) {
    if (!t.metadataChecked)
        return;
    const auto ti = t.handle.torrent_file();
    if (!ti)
        return;
    if (t.selected < 0 && t.requestedFile >= 0) {
        if (t.requestedFile >= ti->files().num_files() ||
            !playable(
                QString::fromStdString(ti->files().file_path(lt::file_index_t(t.requestedFile))))) {
            t.error = "The requested file is not a playable file in this torrent.";
            t.handle.pause();
            emit error(t.error);
            return;
        }
        t.selected = t.requestedFile;
    }
    if (t.selected < 0) {
        QList<int> candidates;
        for (auto i : ti->files().file_range()) {
            if (playable(QString::fromStdString(ti->files().file_path(i))) &&
                ti->files().file_size(i) <= m_diskQuotaLimit)
                candidates.append(int(i));
        }
        QCollator collator;
        collator.setNumericMode(true);
        std::stable_sort(candidates.begin(), candidates.end(), [&collator, &ti](int a, int b) {
            return collator.compare(
                       QString::fromStdString(ti->files().file_path(lt::file_index_t(a))),
                       QString::fromStdString(ti->files().file_path(lt::file_index_t(b)))) < 0;
        });
        if (!candidates.isEmpty())
            t.selected = candidates.first();
    }
    if (t.selected < 0 || t.selected >= ti->files().num_files()) {
        t.error = "No playable file fits the configured cache quota.";
        t.handle.pause();
        emit error(t.error);
        return;
    }
    if (ti->files().file_size(lt::file_index_t(t.selected)) > m_diskQuotaLimit) {
        t.error = "Selected media is larger than the configured cache quota.";
        t.handle.pause();
        return;
    }
    if (!selectedFilesFit(t, t.selected)) {
        t.error = "Selected media does not fit within the remaining cache quota or disk space.";
        t.handle.pause();
        return;
    }
    t.error.clear();
    std::vector<lt::download_priority_t> priorities(size_t(ti->files().num_files()),
                                                    lt::dont_download);
    if (t.selected >= 0 && t.selected < int(priorities.size()))
        priorities[size_t(t.selected)] = lt::default_priority;
    t.handle.prioritize_files(priorities);
    if (!t.userPaused && !m_storagePressure) {
        t.handle.resume();
        schedule(
            t, 0,
            std::min<qint64>(2 * 1024 * 1024, ti->files().file_size(lt::file_index_t(t.selected))));
    }
    maybeStreamReady(t);
}

bool TorrentEngine::selectedFilesFit(const Torrent& changed, int changedIndex) const {
    qint64 remaining = 0;
    for (const auto* t : m_torrents) {
        const auto ti = t->handle.is_valid() ? t->handle.torrent_file() : nullptr;
        if (!ti)
            continue;
        const int index = t == &changed ? changedIndex : t->selected;
        if (index < 0 || index >= ti->files().num_files())
            continue;
        const qint64 size = ti->files().file_size(lt::file_index_t(index));
        if (size > m_diskQuotaLimit)
            return false;
        const qint64 done = std::min<qint64>(size, t->handle.status().total_done);
        if (remaining > m_diskQuotaLimit - (size - done))
            return false;
        remaining += size - done;
    }
    const qint64 quotaFree = std::max<qint64>(0, m_diskQuotaLimit - m_diskUsage);
    QStorageInfo storage(m_root);
    storage.refresh();
    const qint64 diskFree = std::max<qint64>(0, storage.bytesAvailable() - LowDiskFreeBytes);
    return remaining <= std::min(quotaFree, diskFree);
}

void TorrentEngine::select(QString sessionId, int fileIndex) {
    auto* t = m_torrents.value(sessionId, nullptr);
    if (!t)
        return;
    if (!t->metadataChecked) {
        t->requestedFile = fileIndex;
        return;
    }
    const auto ti = t->handle.torrent_file();
    if (!ti || fileIndex < 0 || fileIndex >= ti->files().num_files() ||
        !playable(QString::fromStdString(ti->files().file_path(lt::file_index_t(fileIndex)))))
        return;
    if (ti->files().file_size(lt::file_index_t(fileIndex)) > m_diskQuotaLimit) {
        emit error("Selected media is larger than the configured cache quota.");
        return;
    }
    if (!selectedFilesFit(*t, fileIndex)) {
        emit error("Selected media does not fit within the remaining cache quota or disk space.");
        return;
    }
    for (int piece : t->demandedPieces)
        t->handle.reset_piece_deadline(lt::piece_index_t(piece));
    t->demandedPieces.clear();
    t->streamReadyEmitted = false;
    t->selected = fileIndex;
    t->userPaused = false;
    configureFiles(*t);
    publish();
}

void TorrentEngine::pause(QString sessionId, bool paused) {
    if (auto* t = m_torrents.value(sessionId, nullptr); t && t->handle.is_valid()) {
        t->userPaused = paused;
        if (paused || m_storagePressure || !t->error.isEmpty())
            t->handle.pause();
        else
            t->handle.resume();
    }
}

void TorrentEngine::remove(QString sessionId, bool deleteData) {
    auto* t = m_torrents.take(sessionId);
    if (!t)
        return;
    const bool hasHandle = m_session && t->handle.is_valid();
    if (hasHandle)
        m_session->remove_torrent(t->handle,
                                  deleteData ? lt::session::delete_files : lt::remove_flags_t{});
    for (auto it = m_cache.begin(); it != m_cache.end();) {
        if (it.key().startsWith(sessionId + ':')) {
            m_cacheBytes -= it->bytes.size();
            m_lru.erase(it->lru);
            it = m_cache.erase(it);
        } else
            ++it;
    }
    if (isUuid(sessionId)) {
        const QFileInfo dirInfo(t->dir);
        const QString root = QFileInfo(m_root).canonicalFilePath();
        const QString canonicalDir = dirInfo.canonicalFilePath();
        if (!m_root.isEmpty() && !isLinkOrReparsePoint(dirInfo) && !root.isEmpty() &&
            !canonicalDir.isEmpty() && dirInfo.fileName() == sessionId &&
            hasOwnershipMarker(t->dir, sessionId) &&
            QDir::cleanPath(QFileInfo(canonicalDir).absolutePath()) == QDir::cleanPath(root)) {
            QFile::remove(QDir(t->dir).filePath(".fastresume"));
            QFile::remove(QDir(t->dir).filePath(".torrent"));
            if (deleteData) {
                QFile::remove(QDir(t->dir).filePath(".bitfly-session"));
                if (!hasHandle)
                    QDir(t->dir).removeRecursively();
            } else {
                // Keep downloaded media as retained user data without restoring the removed
                // session.
                writeAtomic(QDir(t->dir).filePath(".retained"), sessionId.toUtf8());
                QFile::remove(QDir(t->dir).filePath(".bitfly-session"));
            }
        }
    }
    delete t;
    m_lastDiskScan.invalidate();
    scanManagedStorage();
    publish();
}

std::optional<StreamFile> TorrentEngine::describe(const QString& id) const {
    const int slash = id.lastIndexOf('/');
    if (slash <= 0)
        return std::nullopt;
    const auto* t = m_torrents.value(id.left(slash), nullptr);
    const int index = id.mid(slash + 1).toInt();
    if (!t || !t->metadataChecked || index != t->selected)
        return std::nullopt;
    const auto ti = t->handle.torrent_file();
    if (!ti || index < 0 || index >= ti->files().num_files())
        return std::nullopt;
    const QString name = QString::fromStdString(ti->files().file_path(lt::file_index_t(index)));
    return StreamFile{id, name, ti->files().file_size(lt::file_index_t(index)),
                      QMimeDatabase().mimeTypeForFile(name).name()};
}

void TorrentEngine::demand(const QString& id, qint64 offset, qint64 length) {
    const int slash = id.lastIndexOf('/');
    if (slash <= 0 || length <= 0)
        return;
    auto* t = m_torrents.value(id.left(slash), nullptr);
    if (!t || !t->metadataChecked || id.mid(slash + 1).toInt() != t->selected)
        return;
    schedule(*t, offset, length);
}

void TorrentEngine::schedule(Torrent& t, qint64 offset, qint64 length) {
    t.lastDemandMs = m_uptime.elapsed();
    if (m_storagePressure || t.userPaused)
        return;
    const auto ti = t.handle.torrent_file();
    if (!ti || t.selected < 0)
        return;
    const auto& files = ti->files();
    const auto fi = lt::file_index_t(t.selected);
    const auto pieces = scheduledPieces(files.file_offset(fi), files.file_size(fi), offset, length,
                                        ti->piece_length(), ti->num_pieces(), CachePieceLookAhead);
    const QSet<int> current(pieces.begin(), pieces.end());
    for (int old : t.demandedPieces)
        if (!current.contains(old))
            t.handle.reset_piece_deadline(lt::piece_index_t(old));
    t.demandedPieces = current;
    const int rate = t.handle.status().download_payload_rate;
    const int pieceEtaMs =
        rate > 0 ? qBound(60, int(qint64(ti->piece_length()) * 1000 / rate), 1200) : 350;
    int distance = 0;
    for (int piece : pieces) {
        const auto pi = lt::piece_index_t(piece);
        t.handle.set_piece_deadline(pi, std::min(15000, 80 + distance * pieceEtaMs));
        ++distance;
    }
    requestPieceReads(t);
}

void TorrentEngine::requestPieceReads(Torrent& t) {
    if (!t.handle.is_valid() || t.readPending.size() >= CacheConcurrentReads)
        return;
    QList<int> pieces = t.demandedPieces.values();
    std::sort(pieces.begin(), pieces.end());
    for (int piece : pieces) {
        if (t.readPending.size() >= CacheConcurrentReads)
            break;
        if (t.readPending.contains(piece) || m_cache.contains(cacheKey(t.id, piece)))
            continue;
        const auto pi = lt::piece_index_t(piece);
        if (!t.handle.have_piece(pi))
            continue;
        t.readPending.insert(piece);
        t.handle.read_piece(pi);
    }
}

QByteArray TorrentEngine::readVerified(const QString& id, qint64 offset, qint64 maxBytes) {
    const auto file = describe(id);
    if (!file || offset < 0 || maxBytes <= 0 || offset >= file->size)
        return {};
    QByteArray result;
    const qint64 end = std::min(
        file->size, offset + std::min(maxBytes, std::numeric_limits<qint64>::max() - offset));
    const int slash = id.lastIndexOf('/');
    auto* t = m_torrents.value(id.left(slash), nullptr);
    const auto ti = t->handle.torrent_file();
    const auto& files = ti->files();
    const qint64 torrentOffset = files.file_offset(lt::file_index_t(t->selected)) + offset;
    qint64 cursor = torrentOffset;
    while (cursor < files.file_offset(lt::file_index_t(t->selected)) + end) {
        const int piece = int(cursor / ti->piece_length());
        const qint64 pieceStart = qint64(piece) * ti->piece_length();
        const QString key = cacheKey(t->id, piece);
        auto it = m_cache.find(key);
        if (it == m_cache.end())
            break;
        m_lru.splice(m_lru.begin(), m_lru, it->lru);
        const qint64 within = cursor - pieceStart;
        const qint64 take =
            std::min(qint64(it->bytes.size()) - within,
                     files.file_offset(lt::file_index_t(t->selected)) + end - cursor);
        if (within < 0 || take <= 0)
            break;
        result.append(it->bytes.constData() + within, int(take));
        cursor += take;
    }
    return result;
}

void TorrentEngine::cachePiece(const QString& key, QByteArray bytes) {
    if (bytes.isEmpty() || bytes.size() > m_ramCacheLimit)
        return;
    if (auto old = m_cache.find(key); old != m_cache.end()) {
        m_cacheBytes -= old->bytes.size();
        m_lru.erase(old->lru);
        m_cache.erase(old);
    }
    m_lru.push_front(key);
    m_cache.insert(key, CacheEntry{std::move(bytes), m_lru.begin()});
    m_cacheBytes += m_cache.value(key).bytes.size();
    evictCache();
}

void TorrentEngine::maybeStreamReady(Torrent& t) {
    if (t.streamReadyEmitted || !t.metadataChecked || t.selected < 0)
        return;
    const auto ti = t.handle.torrent_file();
    if (!ti)
        return;
    const auto file = describe(t.id + "/" + QString::number(t.selected));
    if (!file)
        return;
    const qint64 target = std::min<qint64>(2 * 1024 * 1024, file->size);
    if (readVerified(file->id, 0, target).size() >= target) {
        t.streamReadyEmitted = true;
        emit streamReady(t.id, file->id);
    }
}

void TorrentEngine::evictCache() {
    while (m_cacheBytes > m_ramCacheLimit && !m_lru.empty()) {
        const QString key = m_lru.back();
        m_lru.pop_back();
        auto it = m_cache.find(key);
        if (it != m_cache.end()) {
            m_cacheBytes -= it->bytes.size();
            m_cache.erase(it);
        }
    }
}

qint64 TorrentEngine::cacheBytes() const {
    return m_diskUsage;
}
void TorrentEngine::setCacheLimitBytes(qint64 bytes) {
    if (!m_session || m_root.isEmpty())
        return;
    m_diskQuotaLimit = std::max<qint64>(0, bytes);
    for (auto* t : m_torrents) {
        const auto ti = t->handle.is_valid() ? t->handle.torrent_file() : nullptr;
        if (!ti || t->selected < 0)
            continue;
        if (!selectedFilesFit(*t, t->selected)) {
            t->error =
                "Selected media does not fit within the remaining cache quota or disk space.";
            t->handle.pause();
        } else
            configureFiles(*t);
    }
    m_lastDiskScan.invalidate();
    scanManagedStorage();
    publish();
}

void TorrentEngine::scanManagedStorage() {
    if (m_root.isEmpty() || !QFileInfo(m_root).isDir())
        return;
    if (m_lastDiskScan.isValid() && m_lastDiskScan.elapsed() < 5000)
        return;
    m_lastDiskScan.restart();
    qint64 total = 0;
    int scanned = 0;
    bool capped = false;
    QDirIterator dirs(m_root, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::NoIteratorFlags);
    while (dirs.hasNext()) {
        if (++scanned > 100000) {
            capped = true;
            break;
        }
        const QFileInfo dirInfo(dirs.next());
        if (isLinkOrReparsePoint(dirInfo) || !isUuid(dirInfo.fileName()))
            continue;
        QDirIterator files(dirInfo.absoluteFilePath(), QDir::Files | QDir::NoDotAndDotDot,
                           QDirIterator::Subdirectories);
        while (files.hasNext()) {
            if (++scanned > 100000) {
                capped = true;
                break;
            }
            const QFileInfo info(files.next());
            if (!isLinkOrReparsePoint(info) && info.isFile()) {
                const qint64 size = allocatedSize(info);
                total = size > std::numeric_limits<qint64>::max() - total
                            ? std::numeric_limits<qint64>::max()
                            : total + size;
            }
        }
        if (capped)
            break;
    }
    // Treat a truncated scan as full so unbounded trees cannot bypass admission.
    if (capped)
        total = std::max(total, m_diskQuotaLimit);
    if (m_diskUsage != total) {
        m_diskUsage = total;
        emit cacheBytesChanged(total);
    }
    QStorageInfo storage(m_root);
    storage.refresh();
    m_storagePressure =
        m_diskUsage >= m_diskQuotaLimit || storage.bytesAvailable() < LowDiskFreeBytes;
    const bool atQuota = m_diskUsage >= m_diskQuotaLimit;
    for (auto* t : m_torrents)
        if (t->handle.is_valid()) {
            const bool active =
                t->lastDemandMs >= 0 && m_uptime.elapsed() - t->lastDemandMs < 120000;
            if (m_storagePressure &&
                (!active || atQuota || storage.bytesAvailable() < LowDiskFreeBytes))
                t->handle.pause();
            else if (!m_storagePressure && !t->userPaused && t->error.isEmpty() && t->selected >= 0)
                t->handle.resume();
        }
}

void TorrentEngine::writeAtomic(const QString& path, const QByteArray& bytes) const {
    if (bytes.size() > MaxTorrentBytes)
        return;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) {
        file.cancelWriting();
        return;
    }
    file.commit();
}

void TorrentEngine::save() {
    if (!m_session || m_stopping)
        return;
    for (auto* t : m_torrents)
        if (t->handle.is_valid()) {
            try {
                t->handle.save_resume_data(lt::torrent_handle::save_info_dict);
                ++m_resumePending;
            } catch (const lt::system_error&) {
            }
        }
}

void TorrentEngine::stop() {
    if (m_stopping)
        return;
    m_stopping = true;
    m_publishTimer.stop();
    if (m_session)
        for (auto* t : m_torrents)
            if (t->handle.is_valid()) {
                try {
                    t->handle.save_resume_data(lt::torrent_handle::save_info_dict);
                    ++m_resumePending;
                } catch (const lt::system_error&) {
                }
            }
    if (m_resumePending == 0)
        finishStop();
    else
        m_shutdownTimer.start(1500);
}

void TorrentEngine::finishStop() {
    m_shutdownTimer.stop();
    m_pollTimer.stop();
    if (m_session) {
        m_session->abort();
        m_session.reset();
    }
    m_resumePending = 0;
    emit stopped();
}

void TorrentEngine::poll() {
    if (!m_session)
        return;
    m_session->post_torrent_updates();
    std::vector<lt::alert*> alerts;
    m_session->pop_alerts(&alerts);
    for (const lt::alert* alert : alerts) {
        if (auto* added = lt::alert_cast<lt::add_torrent_alert>(alert)) {
            const auto* cookie = added->params.userdata.get<Torrent>();
            Torrent* t = nullptr;
            for (auto* candidate : m_torrents)
                if (candidate == cookie) {
                    t = candidate;
                    break;
                }
            if (t && added->handle.is_valid()) {
                t->handle = added->handle;
                t->metadataStartedMs = m_uptime.elapsed();
                if (t->handle.torrent_file())
                    validateMetadata(*t);
            } else if (added->handle.is_valid()) {
                // The caller removed this session while async_add_torrent was in flight.
                // The userdata cookie is intentionally treated as an opaque identity here.
                m_session->remove_torrent(added->handle);
            }
            if (added->error) {
                if (t)
                    t->error = QString::fromStdString(added->error.message());
                emit error(QString::fromStdString(added->error.message()));
            }
        } else if (auto* metadata = lt::alert_cast<lt::metadata_received_alert>(alert)) {
            for (auto* t : m_torrents)
                if (t->handle == metadata->handle)
                    validateMetadata(*t);
        } else if (auto* piece = lt::alert_cast<lt::read_piece_alert>(alert)) {
            for (auto* t : m_torrents)
                if (t->handle == piece->handle) {
                    t->readPending.remove(int(piece->piece));
                    if (!piece->error && piece->buffer &&
                        t->demandedPieces.contains(int(piece->piece))) {
                        cachePiece(cacheKey(t->id, int(piece->piece)),
                                   QByteArray(piece->buffer.get(), piece->size));
                        maybeStreamReady(*t);
                    }
                    requestPieceReads(*t);
                    break;
                }
        } else if (auto* done = lt::alert_cast<lt::save_resume_data_alert>(alert)) {
            for (auto* t : m_torrents)
                if (t->handle == done->handle) {
                    auto params = done->params;
                    if (t->userPaused)
                        params.flags |= lt::torrent_flags::paused;
                    else
                        params.flags &= ~lt::torrent_flags::paused;
                    const auto bytes = lt::write_resume_data_buf(params);
                    writeAtomic(QDir(t->dir).filePath(".fastresume"),
                                QByteArray(bytes.data(), int(bytes.size())));
                    const auto torrentBytes = lt::write_torrent_file_buf(params, {});
                    if (!torrentBytes.empty())
                        writeAtomic(QDir(t->dir).filePath(".torrent"),
                                    QByteArray(torrentBytes.data(), int(torrentBytes.size())));
                    break;
                }
            if (--m_resumePending <= 0 && m_stopping)
                finishStop();
        } else if (lt::alert_cast<lt::save_resume_data_failed_alert>(alert)) {
            if (--m_resumePending <= 0 && m_stopping)
                finishStop();
        } else if (auto* finished = lt::alert_cast<lt::piece_finished_alert>(alert)) {
            for (auto* t : m_torrents)
                if (t->handle == finished->handle &&
                    t->demandedPieces.contains(int(finished->piece_index))) {
                    requestPieceReads(*t);
                    break;
                }
        } else if (auto* errorAlert = lt::alert_cast<lt::torrent_error_alert>(alert)) {
            emit error(QString::fromStdString(errorAlert->message()));
        }
        if (!m_session)
            break;
    }
}

QVariantMap TorrentEngine::snapshot(Torrent& t) {
    QVariantMap out{{"id", t.id},    {"name", t.name},   {"state", "metadata"},   {"progress", 0.0},
                    {"rate", 0},     {"peers", 0},       {"downloaded", 0},       {"total", 0},
                    {"buffer", 0.0}, {"error", t.error}, {"selected", t.selected}};
    QVariantList files;
    const auto ti = t.handle.is_valid() ? t.handle.torrent_file() : nullptr;
    if (ti) {
        const auto s = t.handle.status();
        out["name"] = QString::fromStdString(ti->name());
        out["state"] = (t.userPaused || m_storagePressure || !t.error.isEmpty())
                           ? "paused"
                           : statusName(s.state);
        out["progress"] = double(s.progress_ppm) / 1000000.0;
        out["rate"] = s.download_payload_rate;
        out["peers"] = s.num_peers;
        out["downloaded"] = s.total_done;
        out["total"] = ti->total_size();
        qint64 mediaBytes = 0;
        for (auto i : ti->files().file_range()) {
            const QString name = QString::fromStdString(ti->files().file_path(i));
            const qint64 size = ti->files().file_size(i);
            files.append(QVariantMap{
                {"index", int(i)}, {"name", name}, {"size", size}, {"playable", playable(name)}});
            if (int(i) == t.selected) {
                mediaBytes = size;
            }
        }
        const qint64 startupTarget = std::min<qint64>(2 * 1024 * 1024, mediaBytes);
        const qint64 buffered =
            startupTarget > 0
                ? readVerified(t.id + "/" + QString::number(t.selected), 0, startupTarget).size()
                : 0;
        out["buffer"] = startupTarget ? double(buffered) / startupTarget : 0.0;
    } else if (t.metadataStartedMs >= 0 && m_uptime.elapsed() - t.metadataStartedMs >= 30000 &&
               t.error.isEmpty()) {
        t.error = "Still waiting for torrent metadata. Check peer connectivity or try another "
                  "magnet link.";
        out["error"] = t.error;
    }
    out["files"] = files;
    return out;
}

void TorrentEngine::publish() {
    scanManagedStorage();
    QVariantList list;
    for (auto* t : m_torrents)
        list.append(snapshot(*t));
    emit sessionsChanged(list);
}
} // namespace bitfly
