#pragma once

#include <QImage>
#include <QString>

namespace bitfly {
QString ownedThumbnailPath(const QString& root, const QString& id);
bool writeArtworkImage(const QString& path, const QImage& image);

// A local fallback until a video frame or user-selected image is available.
QImage renderCoverArt(const QString& title, const QString& seed, int size = 512);

// Writes the generated cover atomically. Returns false and fills `error` when the folder is
// missing or unwritable.
bool writeCoverArt(const QString& path, const QString& title, const QString& seed,
                   QString* error = nullptr, int size = 512);

} // namespace bitfly
