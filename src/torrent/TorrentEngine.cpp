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
#include <cstdint>
#include <exception>
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
constexpr int CachePieceLookAhead = 24;
constexpr int CacheConcurrentReads = 2;
constexpr int MaxTorrentFiles = 10000;
constexpr int MaxTorrentPieces = 2000000;
constexpr int MaxPieceBytes = 16 * 1024 * 1024;
constexpr int MaxSessions = 256;
constexpr int MaxStreamWindowPieces = 4096;

lt::settings_pack tunedSettings(int connections, int uploadKiB, int downloadKiB, bool dht, bool lsd,
                                bool portMapping) {
    lt::settings_pack settings;
    // Only the categories poll() actually consumes. read_piece and resume-data alerts are
    // "storage"; metadata and torrent updates are "status"; piece_finished is
    // "piece_progress"; peer_blocked is "ip_block". file_progress is not consumed.
    const lt::alert_category_t alerts =
        lt::alert_category::error | lt::alert_category::storage | lt::alert_category::status |
        lt::alert_category::peer | lt::alert_category::piece_progress |
        lt::alert_category::tracker | lt::alert_category::connect | lt::alert_category::ip_block;
    settings.set_int(lt::settings_pack::alert_mask,
                     static_cast<int>(static_cast<std::uint32_t>(alerts)));
    settings.set_int(lt::settings_pack::connections_limit, std::clamp(connections, 20, 1000));
    settings.set_int(lt::settings_pack::upload_rate_limit, std::clamp(uploadKiB, 0, 102400) * 1024);
    settings.set_int(lt::settings_pack::download_rate_limit,
                     std::clamp(downloadKiB, 0, 102400) * 1024);
    settings.set_bool(lt::settings_pack::enable_dht, dht);
    settings.set_bool(lt::settings_pack::enable_lsd, lsd);
    settings.set_bool(lt::settings_pack::enable_upnp, portMapping);
    settings.set_bool(lt::settings_pack::enable_natpmp, portMapping);
    return settings;
}

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

bool retainedMarkerMatches(const QString& dir, const QString& id) {
    const QString path = QDir(dir).filePath(".retained");
    const QFileInfo info(path);
    if (!info.isFile() || isLinkOrReparsePoint(info) || info.size() > 64)
        return false;
    QFile marker(path);
    return marker.open(QIODevice::ReadOnly) && QString::fromUtf8(marker.readAll()).trimmed() == id;
}

// Resolves a user-selected base folder to a canonical app-owned `torrents` folder.
// Rejecting links and reparse points keeps a redirected folder from silently moving
// managed data outside the quota and cleanup boundary.
QString canonicalManagedRoot(const QString& cacheRoot, QString* error) {
    if (cacheRoot.trimmed().isEmpty()) {
        if (error)
            *error = "Choose a download folder.";
        return {};
    }
    const QString rootPath = QDir(cacheRoot).filePath("torrents");
    if (!QDir().mkpath(rootPath)) {
        if (error)
            *error = "BitFly could not create the download folder.";
        return {};
    }
    const QFileInfo rootInfo(rootPath);
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
        if (error)
            *error = "The download folder is unavailable or is a symbolic link.";
        return {};
    }
    return canonicalRoot;
}

bool nestedManagedRoots(QString first, QString second) {
    first = QDir::cleanPath(first);
    second = QDir::cleanPath(second);
#ifdef Q_OS_WIN
    first = first.toCaseFolded();
    second = second.toCaseFolded();
#endif
    return first != second && (first.startsWith(second + '/') || second.startsWith(first + '/'));
}
} // namespace

struct TorrentEngine::Torrent {
    QString id, dir, input, name, error;
    // The magnet link handed to the UI is stable once metadata is known. Caching
    // it keeps publish() from rebuilding a multi-kilobyte string on every tick.
    QString magnet;
    lt::torrent_handle handle;
    int selected = -1, requestedFile = -1;
    qint64 lastDemandMs = -1, metadataStartedMs = -1, lastWindowOffset = 0;
    bool metadataChecked = false, userPaused = false, streamReadyEmitted = false;
    bool reusedRetained = false;
    bool addPending = true, deleteDataOnRemove = false;
    QSet<int> demandedPieces, readWanted, readPending, bootstrapPieces;
};

TorrentEngine::TorrentEngine(QString cacheRoot, QObject* parent) : QObject(parent) {
    QString rootError;
    m_root = canonicalManagedRoot(cacheRoot, &rootError);
    if (m_root.isEmpty()) {
        QTimer::singleShot(0, this, [this, rootError] { emit error(rootError); });
        return;
    }
    m_session = std::make_unique<lt::session>(tunedSettings(m_connectionsLimit, m_uploadLimitKiB,
                                                            m_downloadLimitKiB, m_dhtEnabled,
                                                            m_lsdEnabled, m_portMappingEnabled));
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
    qDeleteAll(m_removing);
}

QString TorrentEngine::torrentDir(const QString& id) const {
    return QDir(m_root).filePath(id);
}

QStringList TorrentEngine::managedRoots() const {
    QStringList roots;
    if (!m_root.isEmpty())
        roots.append(m_root);
    for (const QString& extra : m_extraRoots)
        if (!extra.isEmpty() && !roots.contains(extra))
            roots.append(extra);
    return roots;
}

QString TorrentEngine::managedRootFor(const QString& dir) const {
    const QFileInfo info(dir);
    if (!info.isDir() || isLinkOrReparsePoint(info))
        return {};
    const QString canonical = info.canonicalFilePath();
    if (canonical.isEmpty())
        return {};
    for (const QString& root : managedRoots()) {
        const QFileInfo rootInfo(root);
        if (!rootInfo.isDir() || isLinkOrReparsePoint(rootInfo))
            continue;
        const QString canonicalRoot = rootInfo.canonicalFilePath();
#ifdef Q_OS_WIN
        const bool matches =
            QString::compare(QDir::cleanPath(QFileInfo(canonical).absolutePath()),
                             QDir::cleanPath(canonicalRoot), Qt::CaseInsensitive) == 0;
#else
        const bool matches =
            QDir::cleanPath(QFileInfo(canonical).absolutePath()) == QDir::cleanPath(canonicalRoot);
#endif
        if (matches)
            return canonicalRoot;
    }
    return {};
}

qint64 TorrentEngine::managedFreeBytes() const {
    qint64 free = std::numeric_limits<qint64>::max();
    bool found = false;
    QSet<QString> activeRoots{m_root};
    for (const auto* t : m_torrents)
        activeRoots.insert(managedRootFor(t->dir));
    // Retained files on an old drive count toward the quota, but cannot consume
    // free space reserved for a new download on another drive.
    for (const QString& root : activeRoots) {
        if (root.isEmpty())
            continue;
        QStorageInfo storage(root);
        storage.refresh();
        if (!storage.isValid() || !storage.isReady())
            continue;
        found = true;
        free = std::min(free, storage.bytesAvailable());
    }
    if (!found) {
        QStorageInfo storage(m_root);
        storage.refresh();
        return storage.isValid() ? storage.bytesAvailable() : 0;
    }
    return std::max<qint64>(0, free);
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

void TorrentEngine::reopen(QString sessionId, QString input) {
    input = input.trimmed();
    if (m_stopping || !m_session || !isUuid(sessionId) || input.isEmpty() || input.size() > 16384) {
        emit error("This library entry cannot be reopened right now.");
        return;
    }
    if (m_torrents.contains(sessionId)) {
        emit error("This torrent is already active.");
        return;
    }
    if (m_removing.contains(sessionId)) {
        emit error("This torrent is still being removed. Try Stream again after cleanup finishes.");
        return;
    }
    if (m_torrents.size() >= MaxSessions) {
        emit error("The maximum number of active torrent sessions has been reached.");
        return;
    }
    QString retainedDir;
    for (const QString& root : managedRoots()) {
        const QString candidate = QDir(root).filePath(sessionId);
        const QFileInfo info(candidate);
        if (info.isDir() && !isLinkOrReparsePoint(info) && info.fileName() == sessionId &&
            retainedMarkerMatches(candidate, sessionId) && !managedRootFor(candidate).isEmpty()) {
            retainedDir = candidate;
            break;
        }
    }
    addTorrent(sessionId, input, !retainedDir.isEmpty(), retainedDir, true);
}

void TorrentEngine::addTorrent(const QString& id, const QString& input, bool restoring,
                               const QString& existingDir, bool notifyAdded) {
    if (!isUuid(id) || input.isEmpty() || m_torrents.contains(id) ||
        m_torrents.size() >= MaxSessions)
        return;
    lt::add_torrent_params params;
    const QString dir = existingDir.isEmpty() ? torrentDir(id) : QDir::cleanPath(existingDir);
    const bool retained = !existingDir.isEmpty() && retainedMarkerMatches(dir, id);
    if (!existingDir.isEmpty() &&
        (managedRootFor(dir).isEmpty() || (!retained && !hasOwnershipMarker(dir, id)))) {
        emit error("The retained torrent folder is no longer safe to reopen.");
        return;
    }
    if (restoring && !retained && !hasOwnershipMarker(dir, id))
        return;
    if (!restoring && !retained && QFileInfo::exists(dir)) {
        emit error("Generated torrent storage ID already exists; refusing to reuse it.");
        return;
    }
    if (!retained && !QDir().mkpath(dir)) {
        emit error("Could not create managed torrent storage.");
        return;
    }
    if (!restoring && !retained)
        writeAtomic(QDir(dir).filePath(".bitfly-session"), id.toUtf8());
    if (!hasOwnershipMarker(dir, id) && !retainedMarkerMatches(dir, id)) {
        emit error("Could not mark managed torrent storage as app-owned.");
        return;
    }
    const QString dataDir = QDir(dir).filePath("data");
    QDir().mkpath(dataDir);
    const QString canonicalRoot = managedRootFor(dir);
    const QString canonicalData = QFileInfo(dataDir).canonicalFilePath();
    const QString relativeData = QDir(canonicalRoot).relativeFilePath(canonicalData);
    if (isLinkOrReparsePoint(QFileInfo(dir)) || isLinkOrReparsePoint(QFileInfo(dataDir)) ||
        canonicalData.isEmpty() || QDir::isAbsolutePath(relativeData) || relativeData == ".." ||
        relativeData.startsWith("../") || relativeData.startsWith("..\\")) {
        emit error("The managed torrent storage path is unsafe.");
        return;
    }
    params.save_path = QDir::toNativeSeparators(dataDir).toStdString();
    const bool magnetInput = input.startsWith("magnet:", Qt::CaseInsensitive);
    // A magnet has no file metadata yet. Marking every future file as dont_download makes
    // libtorrent consider the torrent an upload-only seed as soon as metadata arrives, which
    // drops peers and ignores piece deadlines before configureFiles can select a file. Only
    // paused inputs (.torrent, resume) keep the flag until they are configured.
    if (!magnetInput)
        params.flags |= lt::torrent_flags::default_dont_download;
    params.storage_mode = lt::storage_mode_sparse;
    bool hadResume = false;
    if (magnetInput && !restoring) {
        lt::error_code ec;
        lt::parse_magnet_uri(input.toStdString(), params, ec);
        if (ec) {
            emit error(QString::fromStdString(ec.message()));
            return;
        }
        writeAtomic(QDir(dir).filePath(".magnet"), input.toUtf8());
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
                } else {
                    emit diagnostic(QStringLiteral("Restore: resume rejected: %1")
                                        .arg(QString::fromStdString(ec.message())));
                }
            }
        }
        if (!resumeLoaded) {
            emit diagnostic(QStringLiteral("Restore: resume data unavailable, falling back to "
                                           "stored torrent metadata."));
            const QString torrentFile = QDir(dir).filePath(".torrent");
            QFile file(torrentFile);
            if (file.open(QIODevice::ReadOnly) && file.size() > 0 &&
                file.size() <= MaxTorrentBytes) {
                try {
                    params = lt::load_torrent_file(torrentFile.toStdString());
                } catch (const lt::system_error&) {
                    return;
                }
            } else {
                QFile magnetFile(QDir(dir).filePath(".magnet"));
                QString magnet;
                if (magnetFile.open(QIODevice::ReadOnly) && magnetFile.size() > 0 &&
                    magnetFile.size() <= 16384)
                    magnet = QString::fromUtf8(magnetFile.readAll()).trimmed();
                else if (input.startsWith("magnet:", Qt::CaseInsensitive))
                    magnet = input;
                if (!magnet.startsWith("magnet:", Qt::CaseInsensitive))
                    return;
                lt::error_code ec;
                lt::parse_magnet_uri(magnet.toStdString(), params, ec);
                if (ec)
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

    const bool resumeWasPaused =
        restoring && hadResume && static_cast<bool>(params.flags & lt::torrent_flags::paused);
    const bool needsMetadata = !params.ti;
    params.flags &= ~lt::torrent_flags::auto_managed;
    params.flags |= lt::torrent_flags::duplicate_is_error;
    if (needsMetadata) {
        // A restored magnet must be able to reacquire metadata before selecting a file.
        params.flags &= ~lt::torrent_flags::default_dont_download;
        if (!resumeWasPaused || notifyAdded)
            params.flags &= ~lt::torrent_flags::paused;
    } else {
        params.flags |= lt::torrent_flags::default_dont_download;
        if (!restoring || !hadResume)
            params.flags |= lt::torrent_flags::paused;
    }
    auto* t = new Torrent;
    t->id = id;
    t->dir = dir;
    t->input = input;
    t->reusedRetained = retained;
    t->userPaused = resumeWasPaused && !notifyAdded;
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
    if (!restoring || notifyAdded)
        emit sessionAdded(id, input);
    publish();
}

void TorrentEngine::restore() {
    if (!m_session)
        return;
    for (const QString& root : managedRoots()) {
        if (!QFileInfo(root).isDir())
            continue;
        QDirIterator it(root, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::NoIteratorFlags);
        while (it.hasNext()) {
            const QString path = it.next();
            const QFileInfo info(path);
            const QString id = info.fileName();
            if (isLinkOrReparsePoint(info) || !isUuid(id) || m_torrents.contains(id) ||
                QFileInfo(QDir(path).filePath(".retained")).exists() ||
                !hasOwnershipMarker(path, id))
                continue;
            addTorrent(id, "restore", true, path);
        }
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
    if (t.input.startsWith("magnet:", Qt::CaseInsensitive)) {
        t.magnet = t.input;
    } else {
        try {
            t.magnet = QString::fromStdString(lt::make_magnet_uri(t.handle));
        } catch (const std::exception&) {
            t.magnet.clear();
        }
    }
    if (!t.magnet.isEmpty())
        writeAtomic(QDir(t.dir).filePath(".magnet"), t.magnet.toUtf8());
    if (t.reusedRetained && writeAtomic(QDir(t.dir).filePath(".bitfly-session"), t.id.toUtf8())) {
        if (QFile::remove(QDir(t.dir).filePath(".retained")))
            t.reusedRetained = false;
        else
            QFile::remove(QDir(t.dir).filePath(".bitfly-session"));
    }
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
    if (t.requestedFile >= 0) {
        if (t.requestedFile >= ti->files().num_files() ||
            !playable(
                QString::fromStdString(ti->files().file_path(lt::file_index_t(t.requestedFile))))) {
            t.error = "The requested file is not a playable file in this torrent.";
            t.handle.pause();
            emit error(t.error);
            return;
        }
        t.selected = t.requestedFile;
        t.requestedFile = -1;
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
    t.bootstrapPieces.clear();
    const auto selected = lt::file_index_t(t.selected);
    const auto suffix =
        QFileInfo(QString::fromStdString(ti->files().file_path(selected))).suffix().toLower();
    if (suffix == "mp4" || suffix == "m4v" || suffix == "mov") {
        const auto size = ti->files().file_size(selected);
        const auto footer = scheduledPieces(
            ti->files().file_offset(selected), size, std::max<qint64>(0, size - 1024 * 1024),
            1024 * 1024, ti->piece_length(), ti->num_pieces(), MaxStreamWindowPieces);
        t.bootstrapPieces = QSet<int>(footer.begin(), footer.end());
    }
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
    const qint64 diskFree = std::max<qint64>(0, managedFreeBytes() - LowDiskFreeBytes);
    return remaining <= std::min(quotaFree, diskFree);
}

void TorrentEngine::select(QString sessionId, int fileIndex) {
    auto* t = m_torrents.value(sessionId, nullptr);
    if (!t)
        return;
    if (!t->metadataChecked) {
        t->requestedFile = fileIndex;
        t->userPaused = false;
        if (t->handle.is_valid())
            t->handle.resume();
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
    resetStreamWindow(*t);
    t->streamReadyEmitted = false;
    t->selected = fileIndex;
    t->userPaused = false;
    configureFiles(*t);
    publish();
}

void TorrentEngine::pause(QString sessionId, bool paused) {
    if (auto* t = m_torrents.value(sessionId, nullptr); t && t->handle.is_valid()) {
        t->userPaused = paused;
        if (paused) {
            resetStreamWindow(*t);
            t->handle.pause();
        } else if (m_storagePressure || !t->error.isEmpty())
            t->handle.pause();
        else
            t->handle.resume();
    }
}

void TorrentEngine::remove(QString sessionId, bool deleteData) {
    auto* t = m_torrents.take(sessionId);
    if (!t)
        return;
    resetStreamWindow(*t);
    const bool hasHandle = m_session && t->handle.is_valid();
    t->deleteDataOnRemove = deleteData;
    m_removing.insert(sessionId, t);
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
        const QString root = managedRootFor(t->dir);
        if (!root.isEmpty() && !isLinkOrReparsePoint(dirInfo) && dirInfo.fileName() == sessionId &&
            (hasOwnershipMarker(t->dir, sessionId) || retainedMarkerMatches(t->dir, sessionId))) {
            // Preserve ownership until libtorrent has closed or deleted its backing files.
            if (writeAtomic(QDir(t->dir).filePath(".retained"), sessionId.toUtf8()))
                QFile::remove(QDir(t->dir).filePath(".bitfly-session"));
        }
    }
    if (hasHandle)
        m_session->remove_torrent(t->handle,
                                  deleteData ? lt::session::delete_files : lt::remove_flags_t{});
    else if (!t->addPending || !m_session)
        finishRemoval(t, true);
    m_lastDiskScan.invalidate();
    scanManagedStorage();
    publish();
}

void TorrentEngine::finishRemoval(Torrent* t, bool success) {
    if (!t || m_removing.value(t->id) != t)
        return;
    if (success && t->deleteDataOnRemove && QFileInfo::exists(t->dir)) {
        const QFileInfo info(t->dir);
        success = !managedRootFor(t->dir).isEmpty() && !isLinkOrReparsePoint(info) &&
                  info.fileName() == t->id &&
                  (hasOwnershipMarker(t->dir, t->id) || retainedMarkerMatches(t->dir, t->id)) &&
                  QDir(t->dir).removeRecursively();
    }
    const QString id = t->id;
    const bool deletedData = t->deleteDataOnRemove;
    m_removing.remove(id);
    delete t;
    if (!success)
        emit error(
            "Could not remove all torrent data. Close apps using those files and retry cleanup.");
    m_lastDiskScan.invalidate();
    scanManagedStorage();
    emit removalFinished(id, deletedData, success);
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
    t.lastWindowOffset = std::max<qint64>(0, offset);
    if (m_storagePressure || t.userPaused)
        return;
    const auto ti = t.handle.torrent_file();
    if (!ti || t.selected < 0 || t.selected >= ti->files().num_files())
        return;
    const auto fi = lt::file_index_t(t.selected);
    const auto reads = scheduledPieces(ti->files().file_offset(fi), ti->files().file_size(fi),
                                       offset, std::min<qint64>(length, 2 * 1024 * 1024),
                                       ti->piece_length(), ti->num_pieces(), CachePieceLookAhead);
    t.readWanted.unite(QSet<int>(reads.begin(), reads.end()));
    if (t.readWanted.size() > 512)
        t.readWanted = QSet<int>(reads.begin(), reads.end());
    applyStreamWindow(t, t.lastWindowOffset);
    requestPieceReads(t);
}

void TorrentEngine::applyStreamWindow(Torrent& t, qint64 offset) {
    const auto ti = t.handle.torrent_file();
    if (!ti || !t.handle.is_valid() || t.selected < 0 || t.selected >= ti->files().num_files())
        return;
    const auto& files = ti->files();
    const auto fi = lt::file_index_t(t.selected);
    const qint64 pieceBytes = std::max<qint64>(1, ti->piece_length());
    // Priorities intentionally cover a bounded window ahead of the playhead. Pieces far
    // beyond the viewer's buffer are not given deadlines; re-issuing the window on each
    // demand keeps the download sequential without touching pieces already behind it.
    const int lookAhead =
        int(std::clamp<qint64>(m_streamAheadBytes / pieceBytes, 1, MaxStreamWindowPieces));
    const qint64 windowBytes = qint64(lookAhead) * pieceBytes;
    const auto pieces =
        scheduledPieces(files.file_offset(fi), files.file_size(fi), offset, windowBytes,
                        ti->piece_length(), ti->num_pieces(), lookAhead);
    QSet<int> current(pieces.begin(), pieces.end());
    current.unite(t.bootstrapPieces);
    for (int old : t.demandedPieces)
        if (!current.contains(old))
            t.handle.reset_piece_deadline(lt::piece_index_t(old));
    t.demandedPieces = current;
    const int rate = t.handle.status().download_payload_rate;
    const int pieceEtaMs =
        rate > 0 ? qBound(60, int(qint64(ti->piece_length()) * 1000 / rate), 1200) : 350;
    // MP4 can store its movie index at EOF; keep it urgent across concurrent HTTP seeks.
    for (int piece : t.bootstrapPieces)
        if (!t.handle.have_piece(lt::piece_index_t(piece)))
            t.handle.set_piece_deadline(lt::piece_index_t(piece), 1, {});
    int distance = 0;
    for (int piece : pieces) {
        if (t.bootstrapPieces.contains(piece))
            continue;
        const auto pi = lt::piece_index_t(piece);
        t.handle.set_piece_deadline(pi, std::min(15000, 80 + distance * pieceEtaMs), {});
        ++distance;
    }
}

void TorrentEngine::resetStreamWindow(Torrent& t) {
    if (t.handle.is_valid())
        for (int piece : t.demandedPieces)
            t.handle.reset_piece_deadline(lt::piece_index_t(piece));
    t.demandedPieces.clear();
    t.readWanted.clear();
}

void TorrentEngine::requestPieceReads(Torrent& t) {
    if (!t.handle.is_valid() || t.readPending.size() >= CacheConcurrentReads)
        return;
    QList<int> pieces = t.readWanted.values();
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
        CacheEntry& entry = old.value();
        m_cacheBytes -= entry.bytes.size();
        m_lru.erase(entry.lru);
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
    const qint64 buffered = readVerified(file->id, 0, target).size();
    if (buffered >= target) {
        for (int piece : t.bootstrapPieces)
            if (!t.handle.have_piece(lt::piece_index_t(piece)))
                return;
        t.streamReadyEmitted = true;
        // Consumers resolve the playable file from the last published snapshot when
        // streamReady arrives. Publish the snapshot that contains the selection first so
        // the signal contract does not depend on the periodic publish timer's phase.
        publish();
        emit streamReady(t.id, file->id);
        return;
    }
    if (buffered < target && !t.userPaused && !m_storagePressure)
        schedule(t, buffered, target - buffered);
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

void TorrentEngine::setNetworkLimits(int connections, int uploadKiB, int downloadKiB) {
    m_connectionsLimit = std::clamp(connections, 20, 1000);
    m_uploadLimitKiB = std::clamp(uploadKiB, 0, 102400);
    m_downloadLimitKiB = std::clamp(downloadKiB, 0, 102400);
    if (!m_session)
        return;
    lt::settings_pack settings;
    settings.set_int(lt::settings_pack::connections_limit, m_connectionsLimit);
    settings.set_int(lt::settings_pack::upload_rate_limit, m_uploadLimitKiB * 1024);
    settings.set_int(lt::settings_pack::download_rate_limit, m_downloadLimitKiB * 1024);
    m_session->apply_settings(settings);
}

void TorrentEngine::setDiscovery(bool dht, bool lsd, bool portMapping) {
    m_dhtEnabled = dht;
    m_lsdEnabled = lsd;
    m_portMappingEnabled = portMapping;
    if (!m_session)
        return;
    lt::settings_pack settings;
    settings.set_bool(lt::settings_pack::enable_dht, dht);
    settings.set_bool(lt::settings_pack::enable_lsd, lsd);
    settings.set_bool(lt::settings_pack::enable_upnp, portMapping);
    settings.set_bool(lt::settings_pack::enable_natpmp, portMapping);
    m_session->apply_settings(settings);
}

void TorrentEngine::setStreamAheadBytes(qint64 bytes) {
    m_streamAheadBytes = std::clamp<qint64>(bytes, 8LL * 1024 * 1024, 8LL * 1024 * 1024 * 1024);
    for (auto* t : m_torrents)
        if (t->metadataChecked && t->selected >= 0 && !t->userPaused && !m_storagePressure)
            applyStreamWindow(*t, t->lastWindowOffset);
}

void TorrentEngine::setStorageRoot(QString path) {
    QString errorMessage;
    const QString root = canonicalManagedRoot(path, &errorMessage);
    if (root.isEmpty()) {
        emit error(errorMessage);
        return;
    }
    if (root == m_root)
        return;
    for (const QString& known : managedRoots())
        if (nestedManagedRoots(root, known)) {
            emit error("Choose a download folder outside existing BitFly torrent folders.");
            return;
        }
    if (!m_root.isEmpty())
        m_extraRoots.insert(m_root);
    m_root = root;
    m_lastDiskScan.invalidate();
    scanManagedStorage();
    publish();
}

void TorrentEngine::adoptStorageRoots(QStringList roots) {
    for (const QString& path : roots) {
        QString errorMessage;
        const QString root = canonicalManagedRoot(path, &errorMessage);
        if (root.isEmpty() || root == m_root)
            continue;
        bool nested = false;
        for (const QString& known : managedRoots())
            nested |= nestedManagedRoots(root, known);
        if (nested)
            continue;
        m_extraRoots.insert(root);
    }
    m_lastDiskScan.invalidate();
    scanManagedStorage();
}

void TorrentEngine::purge(QString sessionId) {
    if (!isUuid(sessionId) || m_torrents.contains(sessionId) || m_removing.contains(sessionId)) {
        emit removalFinished(sessionId, true, false);
        return;
    }
    for (const QString& root : managedRoots()) {
        const QString dir = QDir(root).filePath(sessionId);
        const QFileInfo info(dir);
        if (!info.exists())
            continue;
        if (!info.isDir() || isLinkOrReparsePoint(info) || info.fileName() != sessionId) {
            emit removalFinished(sessionId, true, false);
            return;
        }
        if (!hasOwnershipMarker(dir, sessionId) && !retainedMarkerMatches(dir, sessionId)) {
            emit removalFinished(sessionId, true, false);
            return;
        }
        const QString canonical = info.canonicalFilePath();
        if (canonical.isEmpty() || !sameManagedPath(QFileInfo(canonical).absolutePath(),
                                                    QFileInfo(root).canonicalFilePath())) {
            emit removalFinished(sessionId, true, false);
            return;
        }
        const bool removed = QDir(dir).removeRecursively();
        if (removed) {
            m_lastDiskScan.invalidate();
            scanManagedStorage();
            publish();
        } else
            emit error("Could not delete retained torrent data. Close any app using those files.");
        emit removalFinished(sessionId, true, removed);
        return;
    }
    emit removalFinished(sessionId, true, true);
}

void TorrentEngine::scanManagedStorage() {
    if (managedRoots().isEmpty())
        return;
    if (m_lastDiskScan.isValid() && m_lastDiskScan.elapsed() < 5000)
        return;
    m_lastDiskScan.restart();
    qint64 total = 0;
    int scanned = 0;
    bool capped = false;
    for (const QString& root : managedRoots()) {
        if (capped || !QFileInfo(root).isDir())
            continue;
        QDirIterator dirs(root, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::NoIteratorFlags);
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
    }
    // Treat a truncated scan as full so unbounded trees cannot bypass admission.
    if (capped)
        total = std::max(total, m_diskQuotaLimit);
    if (m_diskUsage != total) {
        m_diskUsage = total;
        emit cacheBytesChanged(total);
    }
    const qint64 available = managedFreeBytes();
    m_storagePressure = m_diskUsage >= m_diskQuotaLimit || available < LowDiskFreeBytes;
    const bool atQuota = m_diskUsage >= m_diskQuotaLimit;
    for (auto* t : m_torrents)
        if (t->handle.is_valid()) {
            const bool active =
                t->lastDemandMs >= 0 && m_uptime.elapsed() - t->lastDemandMs < 120000;
            if (m_storagePressure && (!active || atQuota || available < LowDiskFreeBytes))
                t->handle.pause();
            else if (!m_storagePressure && !t->userPaused && t->error.isEmpty() && t->selected >= 0)
                t->handle.resume();
        }
}

bool TorrentEngine::writeAtomic(const QString& path, const QByteArray& bytes) {
    if (bytes.size() > MaxTorrentBytes) {
        emit error("Session state exceeds the supported size and could not be saved.");
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) {
        file.cancelWriting();
        emit error(
            "Could not save torrent state. Check the cache folder and available disk space.");
        return false;
    }
    if (!file.commit()) {
        emit error(
            "Could not commit torrent state. Check the cache folder and available disk space.");
        return false;
    }
    return true;
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
    bool finishStopAfterAlerts = false;
    m_session->post_torrent_updates();
    std::vector<lt::alert*> alerts;
    m_session->pop_alerts(&alerts);
    for (const lt::alert* alert : alerts) {
        if (auto* added = lt::alert_cast<lt::add_torrent_alert>(alert)) {
            const auto* cookie = added->params.userdata.get<Torrent>();
            Torrent* t = nullptr;
            for (auto* candidate : m_torrents)
                if (candidate == cookie &&
                    sameManagedPath(QDir(candidate->dir).filePath("data"),
                                    QString::fromStdString(added->params.save_path))) {
                    t = candidate;
                    break;
                }
            Torrent* removing = nullptr;
            for (auto* candidate : m_removing)
                if (candidate == cookie &&
                    sameManagedPath(QDir(candidate->dir).filePath("data"),
                                    QString::fromStdString(added->params.save_path))) {
                    removing = candidate;
                    break;
                }
            if (t)
                t->addPending = false;
            if (removing) {
                removing->addPending = false;
                if (!added->error && added->handle.is_valid()) {
                    removing->handle = added->handle;
                    m_session->remove_torrent(added->handle, removing->deleteDataOnRemove
                                                                 ? lt::session::delete_files
                                                                 : lt::remove_flags_t{});
                } else
                    finishRemoval(removing, true);
            } else if (t && !added->error && added->handle.is_valid()) {
                t->handle = added->handle;
                t->metadataStartedMs = m_uptime.elapsed();
                if (t->handle.torrent_file())
                    validateMetadata(*t);
            } else if (added->handle.is_valid()) {
                // The caller removed this session while async_add_torrent was in flight.
                // The userdata cookie is intentionally treated as an opaque identity here.
                bool alreadyOwned = false;
                for (const auto* candidate : m_torrents)
                    if (candidate->handle == added->handle) {
                        alreadyOwned = true;
                        break;
                    }
                if (!alreadyOwned)
                    m_session->remove_torrent(added->handle);
            }
            if (added->error) {
                if (t)
                    t->error = QString::fromStdString(added->error.message());
                emit error(QString::fromStdString(added->error.message()));
            }
        } else if (auto* deleted = lt::alert_cast<lt::torrent_deleted_alert>(alert)) {
            for (auto* t : m_removing)
                if (t->deleteDataOnRemove && t->handle == deleted->handle) {
                    finishRemoval(t, true);
                    break;
                }
        } else if (auto* failed = lt::alert_cast<lt::torrent_delete_failed_alert>(alert)) {
            for (auto* t : m_removing)
                if (t->deleteDataOnRemove && t->handle == failed->handle) {
                    finishRemoval(t, false);
                    break;
                }
        } else if (auto* removed = lt::alert_cast<lt::torrent_removed_alert>(alert)) {
            for (auto* t : m_removing)
                if (!t->deleteDataOnRemove && t->handle == removed->handle) {
                    finishRemoval(t, true);
                    break;
                }
        } else if (auto* metadata = lt::alert_cast<lt::metadata_received_alert>(alert)) {
            for (auto* t : m_torrents)
                if (t->handle == metadata->handle)
                    validateMetadata(*t);
        } else if (auto* piece = lt::alert_cast<lt::read_piece_alert>(alert)) {
            for (auto* t : m_torrents)
                if (t->handle == piece->handle) {
                    t->readPending.remove(int(piece->piece));
                    t->readWanted.remove(int(piece->piece));
                    if (!piece->error && piece->buffer) {
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
                    try {
                        const auto bytes = lt::write_resume_data_buf(params);
                        writeAtomic(QDir(t->dir).filePath(".fastresume"),
                                    QByteArray(bytes.data(), int(bytes.size())));
                    } catch (const std::exception& e) {
                        emit error("Could not preserve resume data: " +
                                   QString::fromUtf8(e.what()));
                    }
                    break;
                }
            if (--m_resumePending <= 0 && m_stopping)
                finishStopAfterAlerts = true;
        } else if (lt::alert_cast<lt::save_resume_data_failed_alert>(alert)) {
            if (--m_resumePending <= 0 && m_stopping)
                finishStopAfterAlerts = true;
        } else if (auto* finished = lt::alert_cast<lt::piece_finished_alert>(alert)) {
            for (auto* t : m_torrents)
                if (t->handle == finished->handle &&
                    (t->demandedPieces.contains(int(finished->piece_index)) ||
                     t->readWanted.contains(int(finished->piece_index)))) {
                    requestPieceReads(*t);
                    if (t->bootstrapPieces.contains(int(finished->piece_index)))
                        maybeStreamReady(*t);
                    break;
                }
        } else if (auto* errorAlert = lt::alert_cast<lt::torrent_error_alert>(alert)) {
            emit error(QString::fromStdString(errorAlert->message()));
        } else if (auto* trackerError = lt::alert_cast<lt::tracker_error_alert>(alert)) {
            emit diagnostic(QString::fromStdString(trackerError->message()));
        } else if (auto* trackerReply = lt::alert_cast<lt::tracker_reply_alert>(alert)) {
            emit diagnostic(QString::fromStdString(trackerReply->message()));
        } else if (auto* connectAlert = lt::alert_cast<lt::peer_connect_alert>(alert)) {
            emit diagnostic(QString::fromStdString(connectAlert->message()));
        } else if (auto* disconnectAlert = lt::alert_cast<lt::peer_disconnected_alert>(alert)) {
            emit diagnostic(QString::fromStdString(disconnectAlert->message()));
        } else if (auto* peerError = lt::alert_cast<lt::peer_error_alert>(alert)) {
            emit diagnostic(QString::fromStdString(peerError->message()));
        } else if (auto* blocked = lt::alert_cast<lt::peer_blocked_alert>(alert)) {
            emit diagnostic(QString::fromStdString(blocked->message()));
        }
        if (!m_session)
            break;
    }
    if (finishStopAfterAlerts)
        finishStop();
}

QVariantMap TorrentEngine::snapshot(Torrent& t) {
    QVariantMap out{{"id", t.id},
                    {"name", t.name},
                    {"state", "metadata"},
                    {"progress", 0.0},
                    {"rate", 0},
                    {"peers", 0},
                    {"downloaded", 0},
                    {"total", 0},
                    {"buffer", 0.0},
                    {"error", t.error},
                    {"selected", t.selected},
                    {"input", t.input},
                    {"savePath", QDir::toNativeSeparators(QDir(t.dir).filePath("data"))}};
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
        out["uploaded"] = s.total_upload;
        out["uploadRate"] = s.upload_payload_rate;
        out["uploadLimitKiB"] = m_uploadLimitKiB;
        out["downloadLimitKiB"] = m_downloadLimitKiB;
        out["connectionsLimit"] = m_connectionsLimit;
        out["peers"] = s.num_peers;
        out["downloaded"] = s.total_done;
        out["total"] = ti->total_size();
        if (!t.magnet.isEmpty())
            out["magnet"] = t.magnet;
        qint64 mediaBytes = 0;
        for (auto i : ti->files().file_range()) {
            const QString name = QString::fromStdString(ti->files().file_path(i));
            const qint64 size = ti->files().file_size(i);
            files.append(QVariantMap{
                {"index", int(i)}, {"name", name}, {"size", size}, {"playable", playable(name)}});
            if (int(i) == t.selected) {
                mediaBytes = size;
                out["selectedName"] = name;
                out["selectedSize"] = size;
            }
        }
        const qint64 startupTarget = std::min<qint64>(2 * 1024 * 1024, mediaBytes);
        const qint64 buffered =
            startupTarget > 0
                ? readVerified(t.id + "/" + QString::number(t.selected), 0, startupTarget).size()
                : 0;
        double bootstrap = 1.0;
        if (!t.bootstrapPieces.isEmpty()) {
            int present = 0;
            for (int piece : t.bootstrapPieces)
                if (t.handle.have_piece(lt::piece_index_t(piece)))
                    ++present;
            bootstrap = double(present) / t.bootstrapPieces.size();
        }
        out["buffer"] = std::min(startupTarget ? double(buffered) / startupTarget : 0.0, bootstrap);
    } else if (t.metadataStartedMs >= 0 && m_uptime.elapsed() - t.metadataStartedMs >= 30000 &&
               t.error.isEmpty()) {
        t.error = "Still waiting for torrent metadata. Check peer connectivity or try another "
                  "magnet link.";
        out["error"] = t.error;
    }
    if (!out.contains("magnet") && t.input.startsWith("magnet:", Qt::CaseInsensitive))
        out["magnet"] = t.input;
    QCollator episodeOrder;
    episodeOrder.setNumericMode(true);
    std::stable_sort(files.begin(), files.end(),
                     [&episodeOrder](const QVariant& left, const QVariant& right) {
                         return episodeOrder.compare(left.toMap().value("name").toString(),
                                                     right.toMap().value("name").toString()) < 0;
                     });
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
