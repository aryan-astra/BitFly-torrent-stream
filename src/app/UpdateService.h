#pragma once
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QSaveFile>
#include <QTimer>
#include <QUrl>
#include <memory>
#include <optional>

namespace bitfly {
struct UpdateRelease {
    QString version, fileName;
    QUrl url;
    QByteArray sha256;
    qint64 size = 0;
};
class UpdateService final : public QObject {
    Q_OBJECT
  public:
    explicit UpdateService(QString currentVersion, QObject* parent = nullptr);
    ~UpdateService() override;
    void check();
    void download();
    bool launchInstaller();
    void cancel();
    bool busy() const {
        return !m_reply.isNull();
    }
    QString message() const {
        return m_message;
    }
    QString availableVersion() const {
        return m_release ? m_release->version : QString();
    }
    QString installerPath() const {
        return m_installer;
    }
    double progress() const;
    static bool officialUrl(const QUrl& url);
    static bool verifiedInstaller(const QString& path, const QByteArray& sha256, qint64 size);
    static std::optional<UpdateRelease> parseRelease(const QByteArray& json,
                                                     const QString& currentVersion,
                                                     const QString& architecture, QString* error);
  signals:
    void changed();

  private:
    void fetch(const QUrl& url, int redirects = 0);
    void consume();
    void fail(QString message);
    QString m_currentVersion, m_message, m_installer;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    QTimer m_deadline;
    QElapsedTimer m_progressClock;
    std::optional<UpdateRelease> m_release;
    std::unique_ptr<QSaveFile> m_output;
    QCryptographicHash m_hash{QCryptographicHash::Sha256};
    QByteArray m_body;
    qint64 m_received = 0;
    bool m_downloading = false;
};
} // namespace bitfly
