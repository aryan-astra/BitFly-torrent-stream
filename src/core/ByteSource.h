#pragma once
#include <QByteArray>
#include <QString>
#include <QtGlobal>
#include <optional>

namespace bitfly {
struct StreamFile {
    QString id; // opaque session/file key, never a filesystem path
    QString name;
    qint64 size = 0;
    QString mime = "application/octet-stream";
};
// All methods execute on the engine/HTTP worker thread. No blocking waits.
class ByteSource {
  public:
    virtual ~ByteSource() = default;
    virtual std::optional<StreamFile> describe(const QString& id) const = 0;
    virtual void demand(const QString& id, qint64 offset, qint64 length) = 0;
    // Empty means pending. Never return bytes from unverified torrent pieces.
    virtual QByteArray readVerified(const QString& id, qint64 offset, qint64 maxBytes) = 0;
};
} // namespace bitfly
