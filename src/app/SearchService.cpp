#include "SearchService.h"
#include <QFile>
#include <QHostAddress>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSet>
#include <QUrlQuery>
#include <QUuid>
#include <QXmlStreamReader>

namespace bitfly {
SearchService::SearchService(QObject* parent) : QObject(parent) {
    m_deadline.setSingleShot(true);
    connect(&m_deadline, &QTimer::timeout, this,
            [this] { fail("Search timed out. Try another query or check your indexer."); });
}
SearchService::~SearchService() {
    blockSignals(true);
    cancel();
}
bool SearchService::safeEndpoint(const QUrl& url) {
    const auto host = url.host().toLower();
    return url.isValid() && !host.isEmpty() && url.userInfo().isEmpty() &&
           url.fragment().isEmpty() &&
           (url.scheme() == "https" ||
            (url.scheme() == "http" && (host == "localhost" || QHostAddress(host).isLoopback())));
}
bool SearchService::sameOrigin(const QUrl& a, const QUrl& b) {
    auto port = [](const QUrl& url) { return url.port(url.scheme() == "https" ? 443 : 80); };
    return a.scheme() == b.scheme() && a.host().compare(b.host(), Qt::CaseInsensitive) == 0 &&
           port(a) == port(b);
}
bool SearchService::validMagnet(const QString& value) {
    if (value.size() > 16384)
        return false;
    const QUrl url(value);
    if (!url.isValid() || url.scheme() != "magnet")
        return false;
    static const QRegularExpression hash(
        "^urn:(btih:([0-9a-fA-F]{40}|[A-Z2-7a-z]{32})|btmh:1220[0-9a-fA-F]{64})$");
    for (const auto& xt : QUrlQuery(url).allQueryItemValues("xt"))
        if (hash.match(xt).hasMatch())
            return true;
    return false;
}
bool SearchService::configure(QUrl feed, QString apiKey) {
    if (!safeEndpoint(feed)) {
        fail("Use an HTTPS Torznab feed, or an HTTP feed on this PC.");
        return false;
    }
    QUrlQuery parameters(feed);
    if (apiKey.isEmpty())
        apiKey = parameters.queryItemValue("apikey");
    if (apiKey.size() > 1024 || apiKey.contains('\n') || apiKey.contains('\r')) {
        fail("The indexer API key is invalid.");
        return false;
    }
    for (const auto& key : {"apikey", "t", "q", "limit", "offset"})
        parameters.removeAllQueryItems(key);
    feed.setQuery(parameters);
    cancel();
    m_feed = std::move(feed);
    m_apiKey = std::move(apiKey);
    m_results.clear();
    m_message = "Search provider saved.";
    emit changed();
    return true;
}
QVariantList SearchService::parseFeed(const QByteArray& data, const QUrl& feed, QString* error) {
    auto reject = [error](QString message) {
        if (error)
            *error = message;
        return QVariantList();
    };
    if (error)
        error->clear();
    if (data.size() > 2 * 1024 * 1024 || !safeEndpoint(feed))
        return reject("The search response is invalid or too large.");
    QXmlStreamReader xml(data);
    QVariantList results;
    QSet<QString> seen;
    bool rss = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.tokenType() == QXmlStreamReader::DTD)
            return reject("The search response contains an unsupported document type.");
        if (!xml.isStartElement())
            continue;
        if (xml.name() == u"rss")
            rss = true;
        if (xml.name() == u"error")
            return reject("The indexer rejected this search. Check its configuration and API key.");
        if (xml.name() != u"item")
            continue;
        QString title, magnet, link, provider;
        qint64 size = 0;
        int seeders = 0;
        while (xml.readNextStartElement()) {
            const auto name = xml.name();
            if (name == u"title")
                title = xml.readElementText().left(512);
            else if (name == u"link")
                link = xml.readElementText().left(16384);
            else if (name == u"size")
                size = qMax(qint64(0), xml.readElementText().toLongLong());
            else if (name == u"jackettindexer")
                provider = xml.readElementText().left(100);
            else if (name == u"enclosure") {
                const auto attributes = xml.attributes();
                if (link.isEmpty())
                    link = attributes.value("url").toString().left(16384);
                if (!size)
                    size = qMax(qint64(0), attributes.value("length").toLongLong());
                xml.skipCurrentElement();
            } else if (name == u"attr") {
                const auto attributes = xml.attributes();
                const auto field = attributes.value("name"), value = attributes.value("value");
                if (field == u"magneturl")
                    magnet = value.toString().left(16384);
                else if (field == u"seeders")
                    seeders = qMax(0, value.toInt());
                else if (field == u"size")
                    size = qMax(qint64(0), value.toLongLong());
                xml.skipCurrentElement();
            } else
                xml.skipCurrentElement();
        }
        if (!validMagnet(magnet))
            magnet = validMagnet(link) ? link : QString();
        const auto download = feed.resolved(QUrl(link));
        if (title.trimmed().isEmpty() ||
            (magnet.isEmpty() && (!safeEndpoint(download) || !sameOrigin(download, feed))))
            continue;
        const auto identity = magnet.isEmpty() ? download.toString() : magnet;
        if (seen.contains(identity))
            continue;
        seen.insert(identity);
        results.push_back(QVariantMap{{"title", title},
                                      {"size", size},
                                      {"seeders", seeders},
                                      {"provider", provider.isEmpty() ? feed.host() : provider},
                                      {"magnet", magnet},
                                      {"downloadUrl", magnet.isEmpty() ? download : QUrl()}});
        if (results.size() == 100)
            break;
    }
    if (xml.hasError() || !rss)
        return reject("The provider did not return a valid Torznab feed.");
    return results;
}
void SearchService::search(QString query) {
    query = query.trimmed();
    if (query.isEmpty() || query.size() > 300) {
        fail("Enter a search between 1 and 300 characters.");
        return;
    }
    if (m_feed.isEmpty()) {
        fail("Add a Jackett or Prowlarr Torznab feed in Settings first.");
        return;
    }
    cancel();
    m_results.clear();
    QUrl url = m_feed;
    QUrlQuery parameters(url);
    parameters.addQueryItem("t", "search");
    parameters.addQueryItem("q", query);
    parameters.addQueryItem("limit", "100");
    if (!m_apiKey.isEmpty())
        parameters.addQueryItem("apikey", m_apiKey);
    url.setQuery(parameters);
    m_message = "Searching your configured indexers…";
    m_deadline.start(30000);
    fetch(url, false);
}
void SearchService::openResult(int index) {
    if (busy() || index < 0 || index >= m_results.size())
        return;
    const auto item = m_results.at(index).toMap();
    const auto magnet = item.value("magnet").toString();
    if (validMagnet(magnet)) {
        emit inputReady(magnet);
        return;
    }
    const auto url = item.value("downloadUrl").toUrl();
    if (!safeEndpoint(url) || !sameOrigin(url, m_feed)) {
        fail("This result has no supported download link.");
        return;
    }
    m_body.clear();
    m_message = "Fetching torrent metadata…";
    m_deadline.start(30000);
    fetch(url, true);
}
void SearchService::fetch(QUrl url, bool torrent, int redirects) {
    if (!safeEndpoint(url) || !sameOrigin(url, m_feed) || redirects > 3) {
        fail("The provider redirected outside its configured server.");
        return;
    }
    QUrlQuery parameters(url);
    parameters.removeAllQueryItems("apikey");
    if (!m_apiKey.isEmpty())
        parameters.addQueryItem("apikey", m_apiKey);
    url.setQuery(parameters);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "BitFly");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(15000);
    auto* reply = m_network.get(request);
    reply->setReadBufferSize(64 * 1024);
    m_reply = reply;
    auto read = [this, reply, torrent] {
        if (m_reply != reply)
            return;
        m_body += reply->readAll();
        if (m_body.size() > (torrent ? 8 : 2) * 1024 * 1024)
            fail("The provider response is too large.");
    };
    connect(reply, &QNetworkReply::readyRead, this, read);
    connect(reply, &QNetworkReply::finished, this, [this, reply, torrent, redirects, read] {
        if (m_reply != reply) {
            reply->deleteLater();
            return;
        }
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status >= 300 && status <= 399) {
            const auto target = reply->url().resolved(
                reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl());
            m_reply.clear();
            m_body.clear();
            reply->deleteLater();
            if (torrent && validMagnet(target.toString())) {
                m_deadline.stop();
                m_message = "Torrent ready.";
                emit changed();
                emit inputReady(target.toString());
            } else
                fetch(target, torrent, redirects + 1);
            return;
        }
        read();
        if (m_reply != reply) {
            reply->deleteLater();
            return;
        }
        m_reply.clear();
        m_deadline.stop();
        if (reply->error() != QNetworkReply::NoError || status != 200)
            fail("Could not reach the provider. Check its address, API key, and indexers.");
        else if (torrent) {
            if (m_body.isEmpty() || !m_body.startsWith('d') || !m_downloads.isValid())
                fail("The provider did not return torrent metadata.");
            else {
                QFile file(m_downloads.filePath(QUuid::createUuid().toString(QUuid::WithoutBraces) +
                                                ".torrent"));
                if (!file.open(QIODevice::WriteOnly) || file.write(m_body) != m_body.size())
                    fail("Could not save torrent metadata.");
                else {
                    file.close();
                    m_message = "Torrent ready.";
                    emit torrentReady(QUrl::fromLocalFile(file.fileName()));
                }
            }
        } else {
            QString error;
            m_results = parseFeed(m_body, m_feed, &error);
            m_message = error.isEmpty() ? QString::number(m_results.size()) +
                                              " results from your configured indexers."
                                        : error;
        }
        m_body.clear();
        reply->deleteLater();
        emit changed();
    });
    emit changed();
}
void SearchService::cancel() {
    m_deadline.stop();
    if (m_reply) {
        auto* reply = m_reply.data();
        m_reply.clear();
        reply->abort();
        reply->deleteLater();
    }
    m_body.clear();
    emit changed();
}
void SearchService::fail(QString message) {
    cancel();
    m_message = std::move(message);
    emit changed();
}
} // namespace bitfly
