#pragma once

#include "../core/ByteSource.h"
#include "StreamingRules.h"
#include <QElapsedTimer>
#include <QHash>
#include <QObject>
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

  public slots:
    void add(QString input);
    void select(QString sessionId, int fileIndex);
    void pause(QString sessionId, bool paused);
    void remove(QString sessionId, bool deleteData);
    void setCacheLimitBytes(qint64 bytes);
    void save();
    void restore();
    void stop();

  signals:
    void sessionsChanged(QVariantList sessions);
    void error(QString message);
    void streamReady(QString sessionId, QString streamId);
    void cacheBytesChanged(qint64 bytes);
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
    void addTorrent(const QString& id, const QString& input, bool restore = false);
    void configureFiles(Torrent& t);
    bool validateMetadata(Torrent& t);
    void maybeStreamReady(Torrent& t);
    bool selectedFilesFit(const Torrent& changed, int changedIndex) const;
    void schedule(Torrent& t, qint64 offset, qint64 length);
    void requestPieceReads(Torrent& t);
    void cachePiece(const QString& key, QByteArray bytes);
    void evictCache();
    void scanManagedStorage();
    QString torrentDir(const QString& id) const;
    void writeAtomic(const QString& path, const QByteArray& bytes) const;
    QVariantMap snapshot(Torrent& t);

    QString m_root;
    std::unique_ptr<libtorrent::session> m_session;
    QHash<QString, Torrent*> m_torrents;
    QHash<QString, CacheEntry> m_cache;
    std::list<QString> m_lru;
    QTimer m_pollTimer;
    QTimer m_publishTimer;
    QTimer m_shutdownTimer;
    qint64 m_cacheBytes = 0;
    qint64 m_ramCacheLimit = 64 * 1024 * 1024;
    qint64 m_diskQuotaLimit = 20LL * 1024 * 1024 * 1024;
    qint64 m_diskUsage = 0;
    int m_resumePending = 0;
    bool m_stopping = false;
    bool m_storagePressure = false;
    QElapsedTimer m_uptime, m_lastDiskScan;
};
} // namespace bitfly
