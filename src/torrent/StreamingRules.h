#pragma once

#include <QList>
#include <QString>
#include <QtGlobal>

namespace bitfly {

// Pure, deterministic validation and scheduling helpers shared by the engine and tests.
bool isSafeTorrentPath(const QString& path);
bool sameManagedPath(const QString& expected, const QString& reported);
QList<int> scheduledPieces(qint64 fileOffset, qint64 fileSize, qint64 demandOffset,
                           qint64 demandLength, int pieceSize, int pieceCount, int lookAhead = 24);

} // namespace bitfly
