#pragma once
#include "SessionStore.h"
#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QProcess>
#include <QSet>
#include <QStringList>
#include <QThread>
#include <QUrl>
#include <QVariantList>

namespace bitfly {
class TorrentEngine;
class HttpServer;
class PreviewCapture;
class SearchService;
class UpdateService;
class NetworkAssistant;
class ApplicationController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList sessions READ sessions NOTIFY changed)
    Q_PROPERTY(QVariantList library READ library NOTIFY changed)
    Q_PROPERTY(QVariantList history READ history NOTIFY changed)
    Q_PROPERTY(QVariantList queue READ queue NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString notice READ notice NOTIFY changed)
    Q_PROPERTY(QString vlcPath READ vlcPath NOTIFY changed)
    Q_PROPERTY(QString playerPath READ playerPath NOTIFY changed)
    Q_PROPERTY(QString playerName READ playerName NOTIFY changed)
    Q_PROPERTY(bool reducedMotion MEMBER m_reducedMotion WRITE setReducedMotion NOTIFY changed)
    Q_PROPERTY(
        bool generatePreviews MEMBER m_generatePreviews WRITE setGeneratePreviews NOTIFY changed)
    Q_PROPERTY(
        int downloadLimitKiB MEMBER m_downloadLimitKiB WRITE setDownloadLimitKiB NOTIFY changed)
    Q_PROPERTY(bool dhtEnabled MEMBER m_dhtEnabled WRITE setDhtEnabled NOTIFY changed)
    Q_PROPERTY(bool localDiscoveryEnabled MEMBER m_localDiscoveryEnabled WRITE
                   setLocalDiscoveryEnabled NOTIFY changed)
    Q_PROPERTY(bool portMappingEnabled MEMBER m_portMappingEnabled WRITE setPortMappingEnabled
                   NOTIFY changed)
    Q_PROPERTY(QString cachePath READ cachePath NOTIFY changed)
    Q_PROPERTY(qint64 cacheBytes READ cacheBytes NOTIFY changed)
    Q_PROPERTY(int cacheLimitGiB READ cacheLimitGiB WRITE setCacheLimitGiB NOTIFY changed)
    Q_PROPERTY(int seedMinutes READ seedMinutes WRITE setSeedMinutes NOTIFY changed)
    Q_PROPERTY(int connectionLimit READ connectionLimit WRITE setConnectionLimit NOTIFY changed)
    Q_PROPERTY(int uploadLimitKiB READ uploadLimitKiB WRITE setUploadLimitKiB NOTIFY changed)
    Q_PROPERTY(int streamAheadMiB READ streamAheadMiB WRITE setStreamAheadMiB NOTIFY changed)
    Q_PROPERTY(bool advanced READ advanced WRITE setAdvanced NOTIFY changed)
    Q_PROPERTY(QString updateMessage READ updateMessage NOTIFY changed)
    Q_PROPERTY(QUrl releaseUrl READ releaseUrl NOTIFY changed)
    Q_PROPERTY(QString searchFeed MEMBER m_searchFeed NOTIFY changed)
    Q_PROPERTY(bool searchKeySaved READ searchKeySaved NOTIFY changed)
    Q_PROPERTY(QVariantList searchResults READ searchResults NOTIFY changed)
    Q_PROPERTY(bool searchBusy READ searchBusy NOTIFY changed)
    Q_PROPERTY(QString searchMessage READ searchMessage NOTIFY changed)
    Q_PROPERTY(bool updateBusy READ updateBusy NOTIFY changed)
    Q_PROPERTY(double updateProgress READ updateProgress NOTIFY changed)
    Q_PROPERTY(bool updateReady READ updateReady NOTIFY changed)
    Q_PROPERTY(QString availableVersion READ availableVersion NOTIFY changed)
    Q_PROPERTY(bool cloudflareInstalled READ cloudflareInstalled NOTIFY changed)
    Q_PROPERTY(bool cloudflareBusy READ cloudflareBusy NOTIFY changed)
    Q_PROPERTY(QString cloudflareStatus READ cloudflareStatus NOTIFY changed)
  public:
    explicit ApplicationController(QObject* parent = nullptr, QString directory = {});
    ~ApplicationController() override;
    QVariantList sessions() const {
        return m_sessions;
    }
    QVariantList library();
    QVariantList history() const {
        return m_history;
    }
    QVariantList queue() const {
        return m_queue;
    }
    QString error() const {
        return m_error;
    }
    QString notice() const {
        return m_notice;
    }
    QString vlcPath() const {
        return m_vlcPath;
    }
    QString playerPath() const {
        return m_playerPath;
    }
    QString playerName() const;
    QString cachePath() const {
        return m_cachePath;
    }
    qint64 cacheBytes() const {
        return m_cacheBytes;
    }
    int cacheLimitGiB() const {
        return m_cacheLimitGiB;
    }
    int seedMinutes() const {
        return m_seedMinutes;
    }
    int connectionLimit() const {
        return m_connectionLimit;
    }
    int uploadLimitKiB() const {
        return m_uploadLimitKiB;
    }
    int streamAheadMiB() const {
        return m_streamAheadMiB;
    }
    bool advanced() const {
        return m_advanced;
    }
    QString updateMessage() const {
        return m_updateMessage;
    }
    QUrl releaseUrl() const {
        return m_releaseUrl;
    }
    void setCacheLimitGiB(int value);
    void setSeedMinutes(int value);
    void setConnectionLimit(int value);
    void setUploadLimitKiB(int value);
    void setStreamAheadMiB(int value);
    void setAdvanced(bool value);
    void setReducedMotion(bool value);
    void setGeneratePreviews(bool value);
    void setDownloadLimitKiB(int value);
    void setDhtEnabled(bool value);
    void setLocalDiscoveryEnabled(bool value);
    void setPortMappingEnabled(bool value);
    bool searchKeySaved() const {
        return !m_searchKey.isEmpty();
    }
    QVariantList searchResults() const;
    bool searchBusy() const;
    QString searchMessage() const;
    bool updateBusy() const;
    double updateProgress() const;
    bool updateReady() const;
    QString availableVersion() const;
    bool cloudflareInstalled() const;
    bool cloudflareBusy() const;
    QString cloudflareStatus() const;
    Q_INVOKABLE void configureSearch(QString feed, QString apiKey);
    Q_INVOKABLE void search(QString query);
    Q_INVOKABLE void cancelSearch();
    Q_INVOKABLE void openSearchResult(int index);
    Q_INVOKABLE void downloadUpdate();
    Q_INVOKABLE void installUpdate();
    Q_INVOKABLE void refreshCloudflare();
    Q_INVOKABLE void connectCloudflare();
    Q_INVOKABLE void disconnectCloudflare();
    Q_INVOKABLE void setCloudflareMode(QString mode);
    Q_INVOKABLE void openCloudflareDownload();
    Q_INVOKABLE void openCloudflareClient();
    Q_INVOKABLE void setPlayerPath(QUrl url);
    Q_INVOKABLE void refreshPreview(QString id);
    Q_INVOKABLE void setLibraryArtwork(QString id, QUrl file);
    Q_INVOKABLE void openInput(QString input);
    Q_INVOKABLE void openTorrent(QUrl url);
    Q_INVOKABLE void play(QString id, int index);
    Q_INVOKABLE void enqueue(QString id, int index);
    Q_INVOKABLE void moveQueue(int from, int to);
    Q_INVOKABLE void removeQueue(int index);
    Q_INVOKABLE void playQueued(int index);
    Q_INVOKABLE void pause(QString id, bool paused);
    Q_INVOKABLE void remove(QString id, bool deleteData);
    Q_INVOKABLE void finishSession(QString id, QString action);
    Q_INVOKABLE void setVlcPath(QUrl url);
    Q_INVOKABLE void setCachePath(QUrl url);
    Q_INVOKABLE void checkUpdates();
    Q_INVOKABLE void clearError();
    Q_INVOKABLE void clearNotice();
    Q_INVOKABLE void relaunch(QString id);
    Q_INVOKABLE void removeLibraryEntry(QString id);
    Q_INVOKABLE void openStorageFolder();
    Q_INVOKABLE void revealSession(QString id);
  signals:
    void changed();
    void playbackEnded(QString id);
    void workerReady();

  private:
    void launch(QString id, QUrl url);
    void persist();
    void fail(QString message);
    void setNotice(QString message);
    void syncHistoryWithSessions();
    void rememberPlayback(QString id);
    void ensureThumbnail(QVariantMap& entry);
    void capturePreview(QString id, QUrl url);
    void artworkReady(QString id, QString kind);
    void applyNetworkSettings();
    void applyDiscoverySettings();
    void requestSavedPlayback(QString id, int index);
    QVariantMap session(QString id) const;
    QVariantMap historyEntry(QString id) const;
    QString m_dataRoot;
    SessionStore m_store;
    PreviewCapture* m_previews = nullptr;
    SearchService* m_search = nullptr;
    UpdateService* m_updates = nullptr;
    NetworkAssistant* m_networkAssistant = nullptr;
    QThread m_worker;
    QObject* m_context = nullptr;
    TorrentEngine* m_engine = nullptr;
    HttpServer* m_http = nullptr;
    QVariantList m_sessions, m_history, m_queue;
    QString m_error, m_notice, m_vlcPath, m_playerPath, m_cachePath, m_updateMessage;
    QStringList m_storageRoots;
    QUrl m_releaseUrl;
    qint64 m_cacheBytes = 0;
    int m_cacheLimitGiB = 20, m_seedMinutes = 0;
    int m_connectionLimit = 200, m_uploadLimitKiB = 64, m_downloadLimitKiB = 0,
        m_streamAheadMiB = 64;
    bool m_reducedMotion = false, m_generatePreviews = true;
    bool m_dhtEnabled = true, m_localDiscoveryEnabled = true, m_portMappingEnabled = false;
    bool m_advanced = false, m_stopping = false, m_updatePending = false;
    QHash<QString, int> m_requested;
    QHash<QString, QVariantMap> m_pendingRelaunch;
    QSet<QString> m_forgotten;
    QSet<QString> m_pendingForget;
    QString m_searchFeed, m_searchKey, m_protectedSearchKey;
    QSet<QString> m_updateAttempts;
    QHash<QString, QProcess*> m_players;
    QHash<QString, int> m_seedGeneration;
    QNetworkAccessManager m_network;
};
} // namespace bitfly
