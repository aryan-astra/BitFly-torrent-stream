#include "../src/core/ByteSource.h"
#include "../src/stream/HttpServer.h"

#include <QEventLoop>
#include <QHash>
#include <QTcpSocket>
#include <QTest>
#include <QTimer>
#include <algorithm>

using namespace bitfly;

class MockByteSource final : public ByteSource {
  public:
    QByteArray data = "0123456789abcdefghijklmnopqrstuvwxyz";
    int emptyReads = 0;
    int demandCalls = 0;
    bool alwaysPending = false;
    bool pendingAfterFirstChunk = false;
    qint64 advertisedSize = -1;
    int maxChunk = 0;
    QHash<QString, int> reads;

    std::optional<StreamFile> describe(const QString& id) const override {
        if (id != "session/0")
            return std::nullopt;
        return StreamFile{id, "file.mp4", advertisedSize >= 0 ? advertisedSize : data.size(),
                          "video/mp4"};
    }

    void demand(const QString&, qint64, qint64) override {
        ++demandCalls;
    }

    QByteArray readVerified(const QString& id, qint64 offset, qint64 maxBytes) override {
        ++reads[id];
        if (alwaysPending || reads[id] <= emptyReads)
            return {};
        if (pendingAfterFirstChunk && reads[id] > emptyReads + 1)
            return {};
        const qint64 limit = maxChunk > 0 ? std::min<qint64>(maxBytes, maxChunk) : maxBytes;
        return data.mid(qsizetype(offset), qsizetype(limit));
    }
};

class HttpTests final : public QObject {
    Q_OBJECT
  private:
    static QByteArray exchange(const QUrl& url, const QByteArray& request) {
        QTcpSocket socket;
        socket.connectToHost(url.host(), quint16(url.port()));
        if (!socket.waitForConnected(2000))
            return {};
        socket.write(request);
        QByteArray response;
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        QObject::connect(&socket, &QTcpSocket::readyRead, &loop,
                         [&] { response += socket.readAll(); });
        QObject::connect(&socket, &QTcpSocket::disconnected, &loop, &QEventLoop::quit);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        timeout.start(5000);
        loop.exec();
        response += socket.readAll();
        return response;
    }

    static QByteArray get(const QUrl& url, const QByteArray& extra = {}) {
        return exchange(url, "GET " + url.path(QUrl::FullyEncoded).toUtf8() +
                                 " HTTP/1.1\r\nHost: localhost\r\n" + extra +
                                 "Connection: close\r\n\r\n");
    }

    static QByteArray body(const QByteArray& response) {
        const qsizetype split = response.indexOf("\r\n\r\n");
        return split < 0 ? QByteArray{} : response.mid(split + 4);
    }

  private slots:
    void servesFullAndRangeResponses() {
        MockByteSource source;
        HttpServer server(&source);
        QVERIFY(server.start());
        const QUrl url = server.url("session/0");
        QVERIFY(url.isValid());

        const QByteArray full = get(url);
        QVERIFY(full.startsWith("HTTP/1.1 200 OK\r\n"));
        QVERIFY(full.contains("Content-Length: 36\r\n"));
        QCOMPARE(body(full), source.data);

        const QByteArray partial = get(url, "Range: bytes=4-8\r\n");
        QVERIFY(partial.startsWith("HTTP/1.1 206 Partial Content\r\n"));
        QVERIFY(partial.contains("Content-Range: bytes 4-8/36\r\n"));
        QCOMPARE(body(partial), QByteArray("45678"));

        const QByteArray suffix = get(url, "Range: bytes=-3\r\n");
        QCOMPARE(body(suffix), QByteArray("xyz"));
        const QByteArray open = get(url, "Range: bytes=33-\r\n");
        QCOMPARE(body(open), QByteArray("xyz"));
    }

    void handlesHeadAndUnsatisfiableRange() {
        MockByteSource source;
        HttpServer server(&source);
        QVERIFY(server.start());
        const QUrl url = server.url("session/0");
        const QByteArray head = exchange(url, "HEAD " + url.path(QUrl::FullyEncoded).toUtf8() +
                                                  " HTTP/1.1\r\nHost: localhost\r\n\r\n");
        QVERIFY(head.startsWith("HTTP/1.1 200 OK\r\n"));
        QVERIFY(head.contains("Content-Length: 36\r\n"));
        QVERIFY(body(head).isEmpty());

        const QByteArray invalid = get(url, "Range: bytes=36-\r\n");
        QVERIFY(invalid.startsWith("HTTP/1.1 416 Range Not Satisfiable\r\n"));
        QVERIFY(invalid.contains("Content-Range: bytes */36\r\n"));
        QVERIFY(body(invalid).isEmpty());
        QCOMPARE(source.demandCalls, 0);
    }

    void servesEmptyFilesWithoutDemand() {
        MockByteSource source;
        source.advertisedSize = 0;
        HttpServer server(&source);
        QVERIFY(server.start());
        const QUrl url = server.url("session/0");

        const QByteArray response = get(url);
        QVERIFY(response.startsWith("HTTP/1.1 200 OK\r\n"));
        QVERIFY(response.contains("Content-Length: 0\r\n"));
        QVERIFY(body(response).isEmpty());
        QCOMPARE(source.demandCalls, 0);
    }

    void rejectsMalformedAndUnauthenticatedRequests() {
        MockByteSource source;
        HttpServer server(&source);
        QVERIFY(server.start());
        const QUrl url = server.url("session/0");
        const QByteArray malformed =
            exchange(url, "GET " + url.path(QUrl::FullyEncoded).toUtf8() +
                              " HTTP/1.1\r\nRange: bytes=1-2\r\nRange: bytes=3-4\r\n\r\n");
        QVERIFY(malformed.startsWith("HTTP/1.1 400 Bad Request\r\n"));
        const QByteArray unauthorized =
            exchange(url, "GET /v1/wrong/c2Vzc2lvbi8w HTTP/1.1\r\nHost: localhost\r\n\r\n");
        QVERIFY(unauthorized.startsWith("HTTP/1.1 404 Not Found\r\n"));
        const QByteArray duplicateLength = exchange(
            url,
            "GET " + url.path(QUrl::FullyEncoded).toUtf8() +
                " HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\ncontent-length: 0\r\n\r\n");
        QVERIFY(duplicateLength.startsWith("HTTP/1.1 400 Bad Request\r\n"));
        const QByteArray missingHost =
            exchange(url, "GET " + url.path(QUrl::FullyEncoded).toUtf8() + " HTTP/1.1\r\n\r\n");
        QVERIFY(missingHost.startsWith("HTTP/1.1 400 Bad Request\r\n"));
        const QByteArray overflowRange = get(url, "Range: bytes=9223372036854775808-\r\n");
        QVERIFY(overflowRange.startsWith("HTTP/1.1 416 Range Not Satisfiable\r\n"));
        QCOMPARE(source.demandCalls, 0);
    }

    void waitsForVerifiedDataWithoutInventingBytes() {
        MockByteSource source;
        source.emptyReads = 3;
        HttpServer server(&source);
        QVERIFY(server.start());
        const QByteArray response = get(server.url("session/0"));
        QVERIFY(response.startsWith("HTTP/1.1 200 OK\r\n"));
        QCOMPARE(body(response), source.data);
        QVERIFY(source.demandCalls >= 4);
    }

    void servesConcurrentClients() {
        MockByteSource source;
        HttpServer server(&source);
        QVERIFY(server.start());
        const QUrl url = server.url("session/0");
        QTcpSocket first, second;
        first.connectToHost(url.host(), quint16(url.port()));
        second.connectToHost(url.host(), quint16(url.port()));
        QVERIFY(first.waitForConnected(2000));
        QVERIFY(second.waitForConnected(2000));
        QByteArray firstResponse, secondResponse;
        int disconnected = 0;
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        QObject::connect(&first, &QTcpSocket::readyRead, &loop,
                         [&] { firstResponse += first.readAll(); });
        QObject::connect(&second, &QTcpSocket::readyRead, &loop,
                         [&] { secondResponse += second.readAll(); });
        QObject::connect(&first, &QTcpSocket::disconnected, &loop, [&] {
            ++disconnected;
            if (disconnected == 2)
                loop.quit();
        });
        QObject::connect(&second, &QTcpSocket::disconnected, &loop, [&] {
            ++disconnected;
            if (disconnected == 2)
                loop.quit();
        });
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        const QByteArray request =
            "GET " + url.path(QUrl::FullyEncoded).toUtf8() + " HTTP/1.1\r\nHost: localhost\r\n\r\n";
        first.write(request);
        second.write(request);
        timeout.start(5000);
        loop.exec();
        firstResponse += first.readAll();
        secondResponse += second.readAll();
        QCOMPARE(disconnected, 2);
        QCOMPARE(body(firstResponse), source.data);
        QCOMPARE(body(secondResponse), source.data);
    }

    void disconnectStopsPendingPolling() {
        MockByteSource source;
        source.alwaysPending = true;
        HttpServer server(&source);
        QVERIFY(server.start());
        const QUrl url = server.url("session/0");
        QTcpSocket socket;
        socket.connectToHost(url.host(), quint16(url.port()));
        QVERIFY(socket.waitForConnected(2000));
        socket.write("GET " + url.path(QUrl::FullyEncoded).toUtf8() +
                     " HTTP/1.1\r\nHost: localhost\r\n\r\n");
        QTRY_VERIFY_WITH_TIMEOUT(source.demandCalls > 0, 1500);
        socket.abort();
        const int demandsAtDisconnect = source.demandCalls;
        QTest::qWait(100);
        QCOMPARE(source.demandCalls, demandsAtDisconnect);
    }

    void doesNotInventBytesForAnIncompleteVerifiedRead() {
        MockByteSource source;
        source.maxChunk = 5;
        source.pendingAfterFirstChunk = true;
        HttpServer server(&source);
        QVERIFY(server.start());
        const QUrl url = server.url("session/0");
        QTcpSocket socket;
        QByteArray response;
        socket.connectToHost(url.host(), quint16(url.port()));
        QVERIFY(socket.waitForConnected(2000));
        connect(&socket, &QTcpSocket::readyRead, this, [&] { response += socket.readAll(); });
        socket.write("GET " + url.path(QUrl::FullyEncoded).toUtf8() +
                     " HTTP/1.1\r\nHost: localhost\r\n\r\n");
        QTRY_VERIFY_WITH_TIMEOUT(response.contains("\r\n\r\n"), 1500);
        QTest::qWait(150);
        response += socket.readAll();
        QVERIFY(response.startsWith("HTTP/1.1 200 OK\r\n"));
        QVERIFY(response.contains("Content-Length: 36\r\n"));
        QCOMPARE(body(response), source.data.left(5));
        QVERIFY(source.reads.value("session/0") >= 2);
        socket.abort();
    }

    void stopClosesClientsWaitingForData() {
        MockByteSource source;
        source.alwaysPending = true;
        HttpServer server(&source);
        QVERIFY(server.start());
        const QUrl url = server.url("session/0");
        QTcpSocket socket;
        QByteArray response;
        socket.connectToHost(url.host(), quint16(url.port()));
        QVERIFY(socket.waitForConnected(2000));
        connect(&socket, &QTcpSocket::readyRead, this, [&] { response += socket.readAll(); });
        socket.write("GET " + url.path(QUrl::FullyEncoded).toUtf8() +
                     " HTTP/1.1\r\nHost: localhost\r\n\r\n");
        QTRY_VERIFY_WITH_TIMEOUT(response.contains("\r\n\r\n"), 1500);
        QTRY_VERIFY_WITH_TIMEOUT(source.demandCalls > 0, 1500);
        server.stop();
        QTRY_COMPARE_WITH_TIMEOUT(socket.state(), QAbstractSocket::UnconnectedState, 1500);
        const int demandsAtStop = source.demandCalls;
        QTest::qWait(100);
        QCOMPARE(source.demandCalls, demandsAtStop);
    }
};

QTEST_GUILESS_MAIN(HttpTests)
#include "http_tests.moc"
