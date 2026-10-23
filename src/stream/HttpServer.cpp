#include "HttpServer.h"

#include "../core/ByteSource.h"

#include <QHostAddress>
#include <QElapsedTimer>
#include <QHash>
#include <QRandomGenerator>
#include <QStringList>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>
#include <algorithm>
#include <limits>

namespace bitfly {
namespace {
constexpr qsizetype MaxHeaderBytes = 16 * 1024;
constexpr qsizetype MaxClients = 32;
constexpr qint64 ReadChunk = 64 * 1024;
constexpr qint64 DemandWindow = 1024 * 1024;
constexpr qint64 MaxSocketQueue = 128 * 1024;
constexpr int PollMs = 25;
constexpr int HeaderTimeoutMs = 15000;
constexpr int BodyStallTimeoutMs = 120000;

QByteArray randomToken()
{
    QByteArray bytes(32, Qt::Uninitialized);
    auto* rng = QRandomGenerator::system();
    for (qsizetype i = 0; i < bytes.size(); i += 4) {
        const quint32 value = rng->generate();
        const qsizetype count = std::min<qsizetype>(4, bytes.size() - i);
        for (qsizetype j = 0; j < count; ++j)
            bytes[i + j] = char(value >> (j * 8));
    }
    return bytes.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

bool parseDecimal(const QByteArray& text, qint64* value)
{
    if (text.isEmpty()) return false;
    for (char c : text) if (c < '0' || c > '9') return false;
    bool ok = false;
    const qulonglong parsed = text.toULongLong(&ok, 10);
    if (!ok || parsed > quint64(std::numeric_limits<qint64>::max())) return false;
    *value = qint64(parsed);
    return true;
}

bool parseRange(const QByteArray& header, qint64 size, qint64* first, qint64* last)
{
    if (!header.startsWith("bytes=") || header.contains(',')) return false;
    const QByteArray value = header.mid(6);
    const qsizetype dash = value.indexOf('-');
    if (dash < 0 || value.indexOf('-', dash + 1) >= 0 || size <= 0) return false;
    const QByteArray left = value.left(dash);
    const QByteArray right = value.mid(dash + 1);
    if (left.isEmpty()) {
        qint64 suffix = 0;
        if (!parseDecimal(right, &suffix) || suffix == 0) return false;
        *first = suffix >= size ? 0 : size - suffix;
        *last = size - 1;
        return true;
    }
    if (!parseDecimal(left, first) || *first >= size) return false;
    if (right.isEmpty()) {
        *last = size - 1;
        return true;
    }
    if (!parseDecimal(right, last) || *last < *first) return false;
    *last = std::min(*last, size - 1);
    return true;
}

bool isLoopback(const QHostAddress& address)
{
    return address == QHostAddress::LocalHost || address == QHostAddress::LocalHostIPv6
        || (address.protocol() == QAbstractSocket::IPv6Protocol
            && address.toIPv4Address() != 0
            && QHostAddress(address.toIPv4Address()).isLoopback());
}
}

struct HttpServer::Client {
    QTcpSocket* socket = nullptr;
    QTimer* timer = nullptr;
    QByteArray input;
    QByteArray output;
    qsizetype outputOffset = 0;
    QString streamId;
    qint64 position = 0;
    qint64 end = -1;
    qint64 fileSize = 0;
    qint64 lastProgressMs = 0;
    qint64 connectedMs = 0;
    bool headersDone = false;
    bool headOnly = false;
    bool bodyStarted = false;
};

class HttpServer::Impl {
public:
    explicit Impl(ByteSource* byteSource) : source(byteSource) {}
    ByteSource* source;
    QTcpServer server;
    QByteArray token;
    QHash<QTcpSocket*, Client*> clients;
    QElapsedTimer clock;
};

HttpServer::HttpServer(ByteSource* source, QObject* parent)
    : QObject(parent), d(new Impl(source))
{
    connect(&d->server, &QTcpServer::newConnection, this, &HttpServer::acceptConnections);
}

HttpServer::~HttpServer()
{
    stop();
    delete d;
}

bool HttpServer::start()
{
    if (!d->source) return false;
    if (d->server.isListening()) return true;
    d->token = randomToken();
    d->clock.start();
    if (!d->server.listen(QHostAddress::LocalHost, 0)) {
        d->token.clear();
        return false;
    }
    return true;
}

void HttpServer::stop()
{
    d->server.close();
    d->token.clear();
    const auto clients = d->clients.values();
    for (Client* client : clients) removeClient(client);
}

QUrl HttpServer::url(const QString& streamId) const
{
    if (!d->server.isListening() || streamId.isEmpty()) return {};
    const QByteArray encodedId = streamId.toUtf8().toBase64(
        QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    QUrl result;
    result.setScheme(QStringLiteral("http"));
    result.setHost(QStringLiteral("127.0.0.1"));
    result.setPort(d->server.serverPort());
    result.setPath(QStringLiteral("/v1/%1/%2").arg(QString::fromLatin1(d->token), QString::fromLatin1(encodedId)));
    return result;
}

void HttpServer::acceptConnections()
{
    while (d->server.hasPendingConnections()) {
        QTcpSocket* socket = d->server.nextPendingConnection();
        if (!isLoopback(socket->peerAddress()) || d->clients.size() >= MaxClients) {
            socket->abort();
            socket->deleteLater();
            continue;
        }
        auto* client = new Client;
        client->socket = socket;
        socket->setReadBufferSize(MaxHeaderBytes + 1);
        client->timer = new QTimer(socket);
        client->timer->setInterval(PollMs);
        client->lastProgressMs = d->clock.elapsed();
        client->connectedMs = client->lastProgressMs;
        d->clients.insert(socket, client);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
            if (Client* current = d->clients.value(socket, nullptr)) readRequest(current);
        });
        connect(socket, &QTcpSocket::bytesWritten, this, [this, socket](qint64) {
            if (Client* current = d->clients.value(socket, nullptr)) pump(current);
        });
        connect(socket, &QTcpSocket::disconnected, this, [this, socket] {
            if (Client* current = d->clients.value(socket, nullptr)) removeClient(current);
        });
        connect(client->timer, &QTimer::timeout, this, [this, socket] {
            if (Client* current = d->clients.value(socket, nullptr)) pump(current);
        });
        client->timer->start();
    }
}

void HttpServer::readRequest(Client* client)
{
    if (!d->clients.contains(client->socket) || client->headersDone) return;
    const QByteArray received = client->socket->readAll();
    if (!received.isEmpty()) client->lastProgressMs = d->clock.elapsed();
    client->input += received;
    if (client->input.size() > MaxHeaderBytes) {
        respond(client, 431, "Request Header Fields Too Large");
        return;
    }
    const qsizetype end = client->input.indexOf("\r\n\r\n");
    if (end < 0) {
        if (client->input.contains("\n\n") || client->input.contains('\0'))
            respond(client, 400, "Bad Request");
        return;
    }
    if (end + 4 != client->input.size()) {
        respond(client, 400, "Bad Request");
        return;
    }
    const QList<QByteArray> lines = client->input.left(end).split('\n');
    if (lines.isEmpty()) { respond(client, 400, "Bad Request"); return; }
    QByteArray request = lines.first();
    if (request.endsWith('\r')) request.chop(1);
    const QList<QByteArray> requestParts = request.split(' ');
    if (requestParts.size() != 3 || (requestParts[0] != "GET" && requestParts[0] != "HEAD")
        || (requestParts[2] != "HTTP/1.0" && requestParts[2] != "HTTP/1.1")) {
        respond(client, 400, "Bad Request"); return;
    }
    QHash<QByteArray, QByteArray> headers;
    for (qsizetype i = 1; i < lines.size(); ++i) {
        QByteArray line = lines[i];
        if (line.endsWith('\r')) line.chop(1);
        const qsizetype colon = line.indexOf(':');
        if (colon <= 0 || line.startsWith(' ') || line.startsWith('\t')) {
            respond(client, 400, "Bad Request"); return;
        }
        QByteArray name = line.left(colon).toLower();
        for (char c : name) {
            if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-') ) {
                respond(client, 400, "Bad Request"); return;
            }
        }
        QByteArray value = line.mid(colon + 1).trimmed();
        for (char c : value) if ((c < 32 && c != '\t') || c == 127) {
            respond(client, 400, "Bad Request"); return;
        }
        if (headers.contains(name)) { respond(client, 400, "Bad Request"); return; }
        headers.insert(name, value);
    }
    if ((headers.contains("content-length") && headers.value("content-length") != "0")
        || headers.contains("transfer-encoding")) {
        respond(client, 400, "Bad Request"); return;
    }
    if (requestParts[2] == "HTTP/1.1" && (!headers.contains("host") || headers.value("host").isEmpty())) {
        respond(client, 400, "Bad Request"); return;
    }

    const QUrl target = QUrl::fromEncoded(requestParts[1], QUrl::StrictMode);
    if (!target.isValid() || target.hasQuery() || target.hasFragment() || !target.path().startsWith("/")) {
        respond(client, 400, "Bad Request"); return;
    }
    const QStringList parts = target.path().split('/', Qt::KeepEmptyParts);
    if (parts.size() != 4 || parts[0] != QString() || parts[1] != "v1"
        || parts[2].toLatin1() != d->token || parts[3].isEmpty()) {
        respond(client, 404, "Not Found"); return;
    }
    const QByteArray idBytes = QByteArray::fromBase64(parts[3].toLatin1(), QByteArray::Base64UrlEncoding);
    if (idBytes.isEmpty() || idBytes.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals) != parts[3].toLatin1()) {
        respond(client, 404, "Not Found"); return;
    }
    const auto file = d->source->describe(QString::fromUtf8(idBytes));
    if (!file || file->size < 0) { respond(client, 404, "Not Found"); return; }
    client->streamId = file->id;
    client->fileSize = file->size;
    client->position = 0;
    client->end = file->size == 0 ? -1 : file->size - 1;
    client->headOnly = requestParts[0] == "HEAD";

    QByteArray mime = file->mime.toLatin1();
    for (char& c : mime) if (c < 32 || c > 126) c = ' ';
    QByteArray extra = "Accept-Ranges: bytes\r\n";
    extra += "Content-Type: " + mime + "\r\n";
    int status = 200;
    QByteArray reason = "OK";
    if (headers.contains("range")) {
        qint64 first = 0, last = 0;
        if (!parseRange(headers.value("range"), file->size, &first, &last)) {
            const QByteArray unsatisfied = "Content-Range: bytes */" + QByteArray::number(file->size) + "\r\n";
            respond(client, 416, "Range Not Satisfiable", extra + unsatisfied);
            return;
        }
        status = 206;
        reason = "Partial Content";
        client->position = first;
        client->end = last;
        extra += "Content-Range: bytes " + QByteArray::number(first) + "-" + QByteArray::number(last)
            + "/" + QByteArray::number(file->size) + "\r\n";
    }
    const qint64 contentLength = client->end < client->position ? 0 : client->end - client->position + 1;
    extra += "Content-Length: " + QByteArray::number(contentLength) + "\r\n";
    client->headersDone = true;
    client->socket->write("HTTP/1.1 " + QByteArray::number(status) + " " + reason + "\r\n"
                          + extra + "Connection: close\r\n\r\n");
    if (client->headOnly || contentLength == 0) {
        client->socket->disconnectFromHost();
        return;
    }
    client->bodyStarted = true;
    pump(client);
}

void HttpServer::pump(Client* client)
{
    if (!d->clients.contains(client->socket)) return;
    if (!client->headersDone) {
        if (d->clock.elapsed() - client->connectedMs > HeaderTimeoutMs)
            respond(client, 408, "Request Timeout");
        return;
    }
    if (client->headOnly) return;
    if (client->position > client->end && client->outputOffset >= client->output.size()) {
        client->timer->stop();
        client->socket->disconnectFromHost();
        return;
    }
    if (d->clock.elapsed() - client->lastProgressMs > BodyStallTimeoutMs) {
        client->socket->abort();
        return;
    }
    if (client->socket->bytesToWrite() >= MaxSocketQueue) return;
    if (client->outputOffset >= client->output.size()) {
        client->output.clear();
        client->outputOffset = 0;
        const qint64 remaining = client->end - client->position + 1;
        d->source->demand(client->streamId, client->position,
                          std::min(remaining, DemandWindow));
        const qint64 allowance = std::min({ReadChunk, remaining, MaxSocketQueue - client->socket->bytesToWrite()});
        if (allowance <= 0) return;
        client->output = d->source->readVerified(client->streamId, client->position, allowance);
        if (client->output.isEmpty()) return;
        if (client->output.size() > allowance) client->output.truncate(qsizetype(allowance));
        client->position += client->output.size();
    }
    const qint64 capacity = MaxSocketQueue - client->socket->bytesToWrite();
    const qint64 count = std::min<qint64>(client->output.size() - client->outputOffset, capacity);
    const qint64 written = client->socket->write(client->output.constData() + client->outputOffset, count);
    if (written < 0) { client->socket->abort(); return; }
    if (written > 0) {
        client->outputOffset += qsizetype(written);
        client->lastProgressMs = d->clock.elapsed();
    }
}

void HttpServer::respond(Client* client, int status, const QByteArray& reason, const QByteArray& headers)
{
    if (!d->clients.contains(client->socket)) return;
    client->headersDone = true;
    client->timer->stop();
    client->socket->write("HTTP/1.1 " + QByteArray::number(status) + " " + reason + "\r\n"
                          + headers + "Content-Length: 0\r\nConnection: close\r\n\r\n");
    client->socket->disconnectFromHost();
}

void HttpServer::removeClient(Client* client)
{
    if (!client || !d->clients.remove(client->socket)) return;
    client->timer->stop();
    client->socket->deleteLater();
    delete client;
}
}
