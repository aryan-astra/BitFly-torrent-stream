#pragma once

#include <QObject>
#include <QUrl>

namespace bitfly {
class ByteSource;

class HttpServer final : public QObject {
    Q_OBJECT
public:
    explicit HttpServer(ByteSource* source, QObject* parent = nullptr);
    ~HttpServer() override;

    bool start();
    void stop();
    QUrl url(const QString& streamId) const;

private:
    struct Client;
    void acceptConnections();
    void readRequest(Client* client);
    void pump(Client* client);
    void removeClient(Client* client);
    void respond(Client* client, int status, const QByteArray& reason,
                 const QByteArray& headers = {});

    class Impl;
    Impl* d;
};
}
