#include "ThumbnailArt.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QPainter>
#include <QRadialGradient>
#include <QSaveFile>
#include <algorithm>

namespace bitfly {
QString ownedThumbnailPath(const QString& root, const QString& id) {
    const auto name = QCryptographicHash::hash(id.toUtf8(), QCryptographicHash::Sha256).toHex();
    return QDir(root).filePath("thumbnails/" + QString::fromLatin1(name) + ".png");
}
bool writeArtworkImage(const QString& path, const QImage& image) {
    if (image.isNull() || !QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && image.save(&file, "PNG") && file.commit();
}
namespace {
QString monogram(const QString& title) {
    for (const QChar ch : title) {
        if (ch.isLetterOrNumber())
            return QString(ch).toUpper();
    }
    return QStringLiteral("B");
}

QColor tint(quint32 hash, int hueOffset, int saturation, int lightness) {
    const int hue = int((hash + quint32(hueOffset)) % 360u);
    return QColor::fromHsl(hue, saturation, lightness);
}

QString clipped(const QString& text, const QFontMetrics& metrics, int width, int maxLines) {
    const QString clean = text.simplified();
    if (clean.isEmpty())
        return QStringLiteral("Untitled");
    QStringList lines;
    QString current;
    for (const QString& word : clean.split(' ')) {
        const QString candidate = current.isEmpty() ? word : current + ' ' + word;
        if (metrics.horizontalAdvance(candidate) <= width || current.isEmpty()) {
            current = candidate;
        } else {
            lines.append(current);
            current = word;
            if (lines.size() == maxLines - 1)
                break;
        }
    }
    if (lines.size() < maxLines && !current.isEmpty())
        lines.append(current);
    if (lines.size() > maxLines)
        lines = lines.mid(0, maxLines);
    if (lines.size() == maxLines)
        lines[maxLines - 1] = metrics.elidedText(lines.last(), Qt::ElideRight, width);
    return lines.join('\n');
}
} // namespace

QImage renderCoverArt(const QString& title, const QString& seed, int size) {
    const int edge = std::clamp(size, 128, 2048);
    QImage image(edge, edge, QImage::Format_RGB32);
    image.fill(Qt::black);

    const QString identity = seed.isEmpty() ? title : seed + '\n' + title;
    const quint32 hash = qChecksum(identity.toUtf8());
    const QColor top = tint(hash, 0, 65, 239);
    const QColor bottom = tint(hash, 25, 75, 218);
    const QColor glow = tint(hash, 193, 85, 230);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    QLinearGradient background(0, 0, edge, edge);
    background.setColorAt(0.0, top);
    background.setColorAt(0.58, bottom);
    background.setColorAt(1.0, top);
    painter.fillRect(image.rect(), background);

    QRadialGradient halo(QPointF(edge * 0.78, edge * 0.18), edge * 0.72);
    halo.setColorAt(0.0, QColor(glow.red(), glow.green(), glow.blue(), 118));
    halo.setColorAt(0.55, QColor(glow.red(), glow.green(), glow.blue(), 20));
    halo.setColorAt(1.0, Qt::transparent);
    painter.fillRect(image.rect(), halo);

    painter.save();
    painter.setPen(Qt::NoPen);
    for (int band = -2; band < 5; ++band) {
        QColor stripe = band % 2 ? QColor(255, 255, 255, 9) : QColor(0, 0, 0, 16);
        QPolygonF shape;
        const qreal x = edge * (0.08 + band * 0.22);
        shape << QPointF(x, edge) << QPointF(x + edge * 0.16, edge) << QPointF(x + edge * 0.72, 0)
              << QPointF(x + edge * 0.56, 0);
        painter.setBrush(stripe);
        painter.drawPolygon(shape);
    }
    painter.restore();

    QFont markFont(QStringLiteral("Segoe UI"));
    markFont.setWeight(QFont::Black);
    markFont.setPixelSize(int(edge * 0.38));
    painter.setFont(markFont);
    painter.setPen(QColor(57, 63, 65, 220));
    const QRectF markRect(edge * 0.08, edge * 0.10, edge * 0.76, edge * 0.48);
    painter.drawText(markRect, Qt::AlignLeft | Qt::AlignVCenter, monogram(title));

    // A thin inner border keeps the cover distinct from the panel behind it.
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(255, 255, 255, 28), std::max(1.0, edge * 0.003)));
    painter.drawRoundedRect(QRectF(1, 1, edge - 2, edge - 2), edge * 0.02, edge * 0.02);
    return image;
}

bool writeCoverArt(const QString& path, const QString& title, const QString& seed, QString* error,
                   int size) {
    if (writeArtworkImage(path, renderCoverArt(title, seed, size)))
        return true;
    if (error)
        *error = QStringLiteral("Could not save the library thumbnail.");
    return false;
}

} // namespace bitfly
