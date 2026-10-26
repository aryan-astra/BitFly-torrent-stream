#pragma once

#include "../core/ByteSource.h"
#include "StreamingRules.h"
#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <list>
#include <memory>

namespace libtorrent {
class session;
class torrent_handle;
} // namespace libtorrent

namespace bitfly {

class TorrentEngine final : public QObject, public ByteSource {
    Q_OBJECT
  public:
    explicit TorrentEngine(QString cacheRoot, QObject* parent = nullptr);
    ~TorrentEngine() override;

    std::optional<StreamFile> describe(const QString& id) const override;
    void demand(const QString& id, qint64 offset, qint64 length) override;
    QByteArray readVerified(const QString& id, qint64 offset, qint64 maxBytes) override;
    qint64 cacheBytes() const;
    QString storageRoot() const {
        return m_root;
    }

  public slots:
    void add(QString input);
    void reopen(QString sessionId, QString input);
    void select(QString sessionId, int fileIndex);
    void pause(QString sessionId, bool paused);
    void remove(QString sessionId, bool deleteData);
    void setCacheLimitBytes(qint64 bytes);
    void setNetworkLimits(int connections, int uploadKiB, int downloadKiB = 0);
    void setDiscovery(bool dht, bool lsd, bool portMapping);
    void setStreamAheadBytes(qint64 bytes);
    void setStorageRoot(QString path);
    void adoptStorageRoots(QStringList roots);
    void purge(QString sessionId);
    void save();
    void restore();
    void stop();

  signals:
    void sessionsChanged(QVariantList sessions);
    void sessionAdded(QString sessionId, QString input);
    void error(QString message);
    void diagnostic(QString message);
    void streamReady(QString sessionId, QString streamId);
    void cacheBytesChanged(qint64 bytes);
    void removalFinished(QString sessionId, bool deleteData, bool success);
    void stopped();

  private slots:
    void poll();
    void publish();
    void finishStop();

  private:
    struct Torrent;
    struct CacheEntry {
        QByteArray bytes;
        std::list<QString>::iterator lru;
    };
    void addTorrent(const QString& id, const QString& input, bool restore = false,
                    const QString& existingDir = {}, bool notifyAdded = false);
    void configureFiles(Torrent& t);
    void finishRemoval(Torrent* torrent, bool success);
    bool validateMetadata(Torrent& t);
    void maybeStreamReady(Torrent& t);
    bool selectedFilesFit(const Torrent& changed, int changedIndex) const;
    void schedule(Torrent& t, qint64 offset, qint64 length);
    void applyStreamWindow(Torrent& t, qint64 offset);
    void resetStreamWindow(Torrent& t);
    void requestPieceReads(Torrent& t);
    void cachePiece(const QString& key, QByteArray bytes);
    void evictCache();
    void scanManagedStorage();
    QString torrentDir(const QString& id) const;
    QStringList managedRoots() const;
    QString managedRootFor(const QString& dir) const;
    qint64 managedFreeBytes() const;
    bool writeAtomic(const QString& path, const QByteArray& bytes);
    QVariantMap snapshot(Torrent& t);

    QString m_root;
    QSet<QString> m_extraRoots;
    std::unique_ptr<libtorrent::session> m_session;
    QHash<QString, Torrent*> m_torrents;
    QHash<QString, Torrent*> m_removing;
    QHash<QString, CacheEntry> m_cache;
    std::list<QString> m_lru;
    QTimer m_pollTimer;
    QTimer m_publishTimer;
    QTimer m_shutdownTimer;
    qint64 m_cacheBytes = 0;
    qint64 m_ramCacheLimit = 64 * 1024 * 1024;
    qint64 m_diskQuotaLimit = 20LL * 1024 * 1024 * 1024;
    qint64 m_diskUsage = 0;
    qint64 m_streamAheadBytes = 64LL * 1024 * 1024;
    int m_connectionsLimit = 200;
    int m_uploadLimitKiB = 64;
    int m_downloadLimitKiB = 0;
    bool m_dhtEnabled = true;
    bool m_lsdEnabled = true;
    bool m_portMappingEnabled = false;
    int m_resumePending = 0;
    bool m_stopping = false;
    bool m_storagePressure = false;
    QElapsedTimer m_uptime, m_lastDiskScan;
};
} // namespace bitfly
