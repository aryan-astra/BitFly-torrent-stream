#include "../src/app/NetworkAssistant.h"
#include "../src/app/SearchService.h"
#include "../src/app/UpdateService.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QUrlQuery>

using namespace bitfly;
namespace {
const QString magnet = "magnet:?xt=urn:btih:0123456789012345678901234567890123456789&dn=Example";
QByteArray release(QJsonObject asset = {}) {
    if (asset.isEmpty())
        asset = {{"name", "BitFly-1.2.3-windows-x64-setup.exe"},
                 {"size", 1234},
                 {"digest", "sha256:" + QString(64, 'a')},
                 {"browser_download_url",
                  "https://github.com/aryan-astra/BitFly-torrent-stream/releases/download/v1.2.3/"
                  "BitFly-1.2.3-windows-x64-setup.exe"}};
    return QJsonDocument(QJsonObject{{"tag_name", "v1.2.3"},
                                     {"prerelease", false},
                                     {"draft", false},
                                     {"assets", QJsonArray{asset}}})
        .toJson();
}
QByteArray feed(QString links = {}) {
    if (links.isEmpty())
        links = "<link>" + magnet.toHtmlEscaped() + "</link>";
    return ("<?xml version=\"1.0\"?><rss version=\"2.0\" "
            "xmlns:torznab=\"http://torznab.com/schemas/2015/feed\"><channel><item><title>Example "
            "video</title>" +
            links +
            "<size>12345</size><torznab:attr name=\"seeders\" "
            "value=\"17\"/><jackettindexer>Example source</jackettindexer></item></channel></rss>")
        .toUtf8();
}
} // namespace
class ServicesTests final : public QObject {
    Q_OBJECT
  private slots:
    void updateRequiresStableExactInstallerAndDigest() {
        QString error;
        auto parsed = UpdateService::parseRelease(release(), "1.0.0", "x64", &error);
        QVERIFY(parsed);
        QCOMPARE(parsed->version, "v1.2.3");
        QCOMPARE(parsed->size, 1234);
        QCOMPARE(parsed->sha256, QByteArray(32, char(0xaa)));
        QVERIFY(!UpdateService::parseRelease(release(), "1.2.3", "x64", &error));
        QVERIFY(!UpdateService::parseRelease(release(), "1.0.0", "arm64", &error));
        auto object = QJsonDocument::fromJson(release()).object();
        object["prerelease"] = true;
        QVERIFY(
            !UpdateService::parseRelease(QJsonDocument(object).toJson(), "1.0.0", "x64", &error));
        object["prerelease"] = false;
        object["tag_name"] = "v1.2.3-rc.1";
        QVERIFY(
            !UpdateService::parseRelease(QJsonDocument(object).toJson(), "1.0.0", "x64", &error));
        const auto valid =
            QJsonDocument::fromJson(release()).object()["assets"].toArray()[0].toObject();
        for (const auto& change : QList<QPair<QString, QJsonValue>>{
                 {"digest", ""},
                 {"digest", "sha256:bad"},
                 {"size", -1},
                 {"size", 1234.5},
                 {"size", 300 * 1024 * 1024},
                 {"browser_download_url", "https://evil.example/update.exe"},
                 {"browser_download_url", "https://github.com/other/project/releases/download/"
                                          "v1.2.3/BitFly-1.2.3-windows-x64-setup.exe"}}) {
            auto changed = valid;
            changed[change.first] = change.second;
            QVERIFY(!UpdateService::parseRelease(release(changed), "1.0.0", "x64", &error));
        }
        QVERIFY(!UpdateService::parseRelease("not JSON", "1.0.0", "x64", &error));
    }
    void updateRedirectAllowlistRejectsCredentialsAndNonHttps() {
        QVERIFY(UpdateService::officialUrl(
            QUrl("https://release-assets.githubusercontent.com/asset?token=example")));
        for (const auto& url : {"http://github.com/asset", "https://github.com.evil.example/asset",
                                "https://user:password@github.com/asset",
                                "https://github.com:8443/asset", "file:///C:/update.exe"})
            QVERIFY(!UpdateService::officialUrl(QUrl(url)));
    }
    void detectsInstallerTamperingWithoutExecutingIt() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath("installer.exe");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("verified bytes"), 14);
        file.close();
        const auto digest = QCryptographicHash::hash("verified bytes", QCryptographicHash::Sha256);
        QVERIFY(UpdateService::verifiedInstaller(path, digest, 14));
        QVERIFY(!UpdateService::verifiedInstaller(path, digest, 13));
        QVERIFY(!UpdateService::verifiedInstaller(path, QByteArray(32, 'x'), 14));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("tampered bytes"), 14);
        file.close();
        QVERIFY(!UpdateService::verifiedInstaller(path, digest, 14));
    }
    void parsesTorznabAndRejectsExternalDownloadLinks() {
        QString error;
        const QUrl endpoint("http://127.0.0.1:9117/torznab");
        const auto results = SearchService::parseFeed(feed(), endpoint, &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(results.size(), 1);
        const auto item = results[0].toMap();
        QCOMPARE(item["title"].toString(), "Example video");
        QCOMPARE(item["magnet"].toString(), magnet);
        QCOMPARE(item["size"].toLongLong(), 12345);
        QCOMPARE(item["seeders"].toInt(), 17);
        QVERIFY(SearchService::parseFeed(feed("<link>https://evil.example/download</link>"),
                                         endpoint, &error)
                    .isEmpty());
        QVERIFY(error.isEmpty());
        const auto local = SearchService::parseFeed(
            feed("<enclosure url=\"/download/1?apikey=fake\" length=\"44\"/>"), endpoint, &error);
        QCOMPARE(local.size(), 1);
        QCOMPARE(local[0].toMap()["downloadUrl"].toUrl().host(), endpoint.host());
        QVERIFY(
            SearchService::parseFeed("<!DOCTYPE rss [<!ENTITY foo 'bar'>]><rss/>", endpoint, &error)
                .isEmpty());
        QVERIFY(!error.isEmpty());
        QVERIFY(SearchService::parseFeed("<rss><channel>", endpoint, &error).isEmpty());
        QVERIFY(!error.isEmpty());
        QVERIFY(SearchService::parseFeed(QByteArray(2 * 1024 * 1024 + 1, 'x'), endpoint, &error)
                    .isEmpty());
        QVERIFY(!error.isEmpty());
    }
    void endpointAndMagnetValidation() {
        QVERIFY(SearchService::safeEndpoint(QUrl("http://127.0.0.1:9117/torznab")));
        QVERIFY(SearchService::safeEndpoint(QUrl("http://[::1]:9117/torznab")));
        QVERIFY(SearchService::safeEndpoint(QUrl("https://indexer.example/feed")));
        QVERIFY(!SearchService::safeEndpoint(QUrl("http://indexer.example/feed")));
        QVERIFY(!SearchService::safeEndpoint(QUrl("https://user:pass@indexer.example/feed")));
        QVERIFY(!SearchService::sameOrigin(QUrl("http://127.0.0.1:9117"),
                                           QUrl("http://127.0.0.1:9118")));
        QVERIFY(SearchService::validMagnet(magnet));
        QVERIFY(!SearchService::validMagnet("magnet:?xt=urn:btih:garbage"));
        QVERIFY(!SearchService::validMagnet(
            "https://example.com/?xt=urn:btih:0123456789012345678901234567890123456789"));
    }
    void searchesLocalFeedAndOpensMagnetWithoutDownloading() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        QByteArray request;
        connect(&server, &QTcpServer::newConnection, this, [&] {
            auto* socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
                request += socket->readAll();
                if (!request.contains("\r\n\r\n"))
                    return;
                const auto body = feed();
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/rss+xml\r\nConnection: "
                              "close\r\nContent-Length: " +
                              QByteArray::number(body.size()) + "\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        });
        SearchService service;
        QVERIFY(service.configure(
            QUrl("http://127.0.0.1:" + QString::number(server.serverPort()) + "/torznab"),
            "fake-key"));
        QSignalSpy opened(&service, &SearchService::inputReady);
        service.search("a & b");
        QTRY_VERIFY_WITH_TIMEOUT(!service.busy(), 5000);
        QCOMPARE(service.results().size(), 1);
        const auto requestUrl = QUrl::fromEncoded(request.split(' ').value(1));
        const QUrlQuery parameters(requestUrl);
        QCOMPARE(parameters.queryItemValue("q"), "a & b");
        QCOMPARE(parameters.queryItemValue("apikey"), "fake-key");
        service.openResult(0);
        QCOMPARE(opened.size(), 1);
        QCOMPARE(opened.at(0).at(0).toString(), magnet);
    }
    void followsNoExternalProviderRedirectsAndCancels() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server, &QTcpServer::newConnection, this, [&] {
            auto* socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [socket] {
                socket->readAll();
                socket->write(
                    "HTTP/1.1 302 Found\r\nLocation: https://evil.example/feed\r\nContent-Length: "
                    "0\r\nConnection: close\r\n\r\n");
                socket->disconnectFromHost();
            });
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        });
        SearchService service;
        QVERIFY(service.configure(
            QUrl("http://127.0.0.1:" + QString::number(server.serverPort()) + "/feed"),
            "fake-key"));
        service.search("example");
        QTRY_VERIFY_WITH_TIMEOUT(!service.busy(), 5000);
        QVERIFY(service.message().contains("redirected"));
        QVERIFY(service.results().isEmpty());
        service.search("cancelled");
        QVERIFY(service.busy());
        service.cancel();
        QVERIFY(!service.busy());
    }
    void downloadsMetadataOnlyFromConfiguredOrigin() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        const QByteArray torrent =
            QByteArray("d4:infod6:lengthi1e4:name4:test12:piece lengthi16384e6:pieces20:") +
            QByteArray(20, 'x') + "ee";
        bool authenticatedDownload = false;
        connect(&server, &QTcpServer::newConnection, this, [&] {
            auto* socket = server.nextPendingConnection();
            auto request = std::make_shared<QByteArray>();
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket, request] {
                *request += socket->readAll();
                if (!request->contains("\r\n\r\n"))
                    return;
                const auto url = QUrl::fromEncoded(request->split(' ').value(1));
                const bool downloading = url.path() == "/download/1";
                if (downloading)
                    authenticatedDownload = QUrlQuery(url).queryItemValue("apikey") == "fake-key";
                const auto body = downloading ? torrent : feed("<link>/download/1</link>");
                socket->write("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: " +
                              QByteArray::number(body.size()) + "\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        });
        QString path;
        {
            SearchService service;
            QVERIFY(
                service.configure(QUrl("http://127.0.0.1:" + QString::number(server.serverPort()) +
                                       "/feed?apikey=fake-key"),
                                  {}));
            QSignalSpy opened(&service, &SearchService::torrentReady);
            service.search("example");
            QTRY_VERIFY_WITH_TIMEOUT(!service.busy(), 5000);
            QCOMPARE(service.results().size(), 1);
            service.openResult(0);
            QTRY_COMPARE_WITH_TIMEOUT(opened.size(), 1, 5000);
            QVERIFY(authenticatedDownload);
            path = opened.at(0).at(0).toUrl().toLocalFile();
            QFile file(path);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(file.readAll(), torrent);
        }
        QVERIFY(!QFile::exists(path));
    }
    void networkModesAreAnAllowlist() {
        QVERIFY(NetworkAssistant::supportedMode("doh"));
        QVERIFY(NetworkAssistant::supportedMode("warp+doh"));
        QVERIFY(!NetworkAssistant::supportedMode("--accept-tos"));
        QVERIFY(!NetworkAssistant::supportedMode("doh; disconnect"));
    }
};
QTEST_GUILESS_MAIN(ServicesTests)
#include "services_tests.moc"
