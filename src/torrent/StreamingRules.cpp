#include "StreamingRules.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <algorithm>
#include <limits>

namespace bitfly {
namespace {
// Hard ceiling for deadline scheduling. The engine clamps its configurable
// stream-ahead window to MaxStreamWindowPieces; this cap keeps a single
// demand from touching the whole torrent when pieces are tiny, while still
// allowing a 256 MiB buffer at a typical 256 KiB piece size.
constexpr int MaxScheduledLookAhead = 4096;
} // namespace

bool isSafeTorrentPath(const QString& path) {
    if (path.isEmpty() || path.startsWith('/') || path.startsWith('\\') || path.contains(':'))
        return false;
    const auto parts = QString(path).replace('\\', '/').split('/', Qt::KeepEmptyParts);
    static const QRegularExpression invalid("[<>:\"|?*\\x00-\\x1f]");
    static const QRegularExpression reserved(
        "^(con|prn|aux|nul|conin\\$|conout\\$|com[1-9¹²³]|lpt[1-9¹²³])(\\..*)?$",
        QRegularExpression::CaseInsensitiveOption);
    for (const QString& part : parts) {
        if (part.isEmpty() || part.size() > 255 || part == "." || part == ".." ||
            part.endsWith('.') || part.endsWith(' ') || invalid.match(part).hasMatch() ||
            reserved.match(part).hasMatch())
            return false;
    }
    return true;
}

bool sameManagedPath(const QString& expected, const QString& reported) {
    if (expected.isEmpty() || reported.isEmpty())
        return false;
    const QString lhs = QDir::cleanPath(QFileInfo(expected).absoluteFilePath());
    const QString rhs = QDir::cleanPath(QFileInfo(reported).absoluteFilePath());
#ifdef Q_OS_WIN
    return QString::compare(lhs, rhs, Qt::CaseInsensitive) == 0;
#else
    return lhs == rhs;
#endif
}

QList<int> scheduledPieces(qint64 fileOffset, qint64 fileSize, qint64 demandOffset,
                           qint64 demandLength, int pieceSize, int pieceCount, int lookAhead) {
    QList<int> out;
    if (fileOffset < 0 || fileSize <= 0 || demandOffset < 0 || demandLength <= 0 ||
        pieceSize <= 0 || pieceCount <= 0 || lookAhead <= 0 || demandOffset >= fileSize)
        return out;
    const int boundedLookAhead = std::min(lookAhead, MaxScheduledLookAhead);
    const qint64 max = std::numeric_limits<qint64>::max();
    if (demandOffset > max - fileOffset)
        return out;
    const qint64 start = fileOffset + demandOffset;
    const qint64 length = std::min(demandLength, fileSize - demandOffset);
    if (length <= 0 || length > max - start)
        return out;
    const qint64 end = start + length;
    const qint64 first = start / pieceSize;
    const qint64 last = (end - 1) / pieceSize;
    if (first > std::numeric_limits<int>::max() || last > std::numeric_limits<int>::max())
        return out;
    for (qint64 p = first; p <= last && p < pieceCount && out.size() < boundedLookAhead; ++p)
        out.append(int(p));
    return out;
}

} // namespace bitfly
