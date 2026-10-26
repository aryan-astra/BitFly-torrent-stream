#pragma once
#include <QNetworkAccessManager>
#include <QPointer>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

namespace bitfly {
class SearchService final : public QObject {
    Q_OBJECT
  public:
    explicit SearchService(QObject* parent = nullptr);
    ~SearchService() override;
    bool configure(QUrl feed, QString apiKey);
    void search(QString query);
    void cancel();
    void openResult(int index);
    QVariantList results() const {
        return m_results;
    }
    bool busy() const {
        return !m_reply.isNull();
    }
    QString message() const {
        return m_message;
    }
    static bool safeEndpoint(const QUrl& url);
    static bool sameOrigin(const QUrl& a, const QUrl& b);
    static bool validMagnet(const QString& value);
    static QVariantList parseFeed(const QByteArray& data, const QUrl& feed, QString* error);
  signals:
    void changed();
    void inputReady(QString magnet);
    void torrentReady(QUrl localFile);

  private:
    void fetch(QUrl url, bool torrent, int redirects = 0);
    void fail(QString message);
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    QTimer m_deadline;
    QUrl m_feed;
    QString m_apiKey, m_message;
    QVariantList m_results;
    QByteArray m_body;
    QTemporaryDir m_downloads;
};
} // namespace bitfly
