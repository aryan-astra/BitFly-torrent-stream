#pragma once
#include <QObject>
#include <QThread>
#include <QVariantList>
#include <QUrl>
#include <QHash>
#include <QSet>
#include <QProcess>
#include <QNetworkAccessManager>
#include "SessionStore.h"

namespace bitfly {
class TorrentEngine;
class HttpServer;
class ApplicationController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList sessions READ sessions NOTIFY changed)
    Q_PROPERTY(QVariantList history READ history NOTIFY changed)
    Q_PROPERTY(QVariantList queue READ queue NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString vlcPath READ vlcPath NOTIFY changed)
    Q_PROPERTY(QString cachePath READ cachePath NOTIFY changed)
    Q_PROPERTY(qint64 cacheBytes READ cacheBytes NOTIFY changed)
    Q_PROPERTY(int cacheLimitGiB READ cacheLimitGiB WRITE setCacheLimitGiB NOTIFY changed)
    Q_PROPERTY(int seedMinutes READ seedMinutes WRITE setSeedMinutes NOTIFY changed)
    Q_PROPERTY(bool advanced READ advanced WRITE setAdvanced NOTIFY changed)
    Q_PROPERTY(QString updateMessage READ updateMessage NOTIFY changed)
    Q_PROPERTY(QUrl releaseUrl READ releaseUrl NOTIFY changed)
public:
    explicit ApplicationController(QObject* parent=nullptr);
    ~ApplicationController() override;
    QVariantList sessions() const { return m_sessions; }
    QVariantList history() const { return m_history; }
    QVariantList queue() const { return m_queue; }
    QString error() const { return m_error; }
    QString vlcPath() const { return m_vlcPath; }
    QString cachePath() const { return m_cachePath; }
    qint64 cacheBytes() const { return m_cacheBytes; }
    int cacheLimitGiB() const { return m_cacheLimitGiB; }
    int seedMinutes() const { return m_seedMinutes; }
    bool advanced() const { return m_advanced; }
    QString updateMessage() const { return m_updateMessage; }
    QUrl releaseUrl() const { return m_releaseUrl; }
    void setCacheLimitGiB(int value);
    void setSeedMinutes(int value);
    void setAdvanced(bool value);
    Q_INVOKABLE void openInput(QString input);
    Q_INVOKABLE void openTorrent(QUrl url);
    Q_INVOKABLE void play(QString id,int index);
    Q_INVOKABLE void enqueue(QString id,int index);
    Q_INVOKABLE void moveQueue(int from,int to);
    Q_INVOKABLE void removeQueue(int index);
    Q_INVOKABLE void playQueued(int index);
    Q_INVOKABLE void pause(QString id,bool paused);
    Q_INVOKABLE void remove(QString id,bool deleteData);
    Q_INVOKABLE void finishSession(QString id,QString action);
    Q_INVOKABLE void setVlcPath(QUrl url);
    Q_INVOKABLE void setCachePath(QUrl url);
    Q_INVOKABLE void checkUpdates();
    Q_INVOKABLE void clearError();
    Q_INVOKABLE void relaunch(QString id);
signals:
    void changed();
    void playbackEnded(QString id);
    void workerReady();
private:
    void launch(QString id,QUrl url);
    void persist();
    void fail(QString message);
    QVariantMap session(QString id) const;
    SessionStore m_store;
    QThread m_worker;
    QObject* m_context = nullptr;
    TorrentEngine* m_engine = nullptr;
    HttpServer* m_http = nullptr;
    QVariantList m_sessions,m_history,m_queue;
    QString m_error,m_vlcPath,m_cachePath,m_updateMessage;
    QUrl m_releaseUrl;
    qint64 m_cacheBytes=0;
    int m_cacheLimitGiB=20,m_seedMinutes=15;
    bool m_advanced=false,m_stopping=false,m_updatePending=false;
    QHash<QString,int> m_requested;
    QHash<QString,QProcess*> m_players;
    QHash<QString,int> m_seedGeneration;
    QNetworkAccessManager m_network;
};
}
