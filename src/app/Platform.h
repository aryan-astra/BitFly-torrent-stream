#pragma once
#include <QString>
#include <QUrl>
#include <array>
#include <optional>
namespace bitfly {
QString findVlc(const QString& preferred = {});
bool validVlc(const QString& path);
bool validPlayer(const QString& path);
QStringList playerArguments(const QString& path, const QUrl& url, int resumeSeconds = 0);
QByteArray protectSecret(const QString& secret);
QString unprotectSecret(const QByteArray& secret);
bool prefersReducedMotion();
std::optional<std::array<int, 3>> stableVersion(QString value);
} // namespace bitfly
