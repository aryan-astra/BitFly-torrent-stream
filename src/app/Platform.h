#pragma once
#include <QString>
#include <optional>
#include <array>
namespace bitfly {
QString findVlc(const QString& preferred = {});
bool validVlc(const QString& path);
std::optional<std::array<int,3>> stableVersion(QString value);
}
