#include "Platform.h"
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>
namespace bitfly {
bool validVlc(const QString& path) {
    QFileInfo f(path);
    return f.isAbsolute() && f.isFile() &&
           f.fileName().compare("vlc.exe", Qt::CaseInsensitive) == 0;
}
QString findVlc(const QString& preferred) {
    if (validVlc(preferred))
        return QFileInfo(preferred).absoluteFilePath();
    QStringList candidates;
#ifdef Q_OS_WIN
    for (const auto& hive : {QString("HKEY_CURRENT_USER"), QString("HKEY_LOCAL_MACHINE")}) {
        for (const auto& view :
             {QSettings::NativeFormat, QSettings::Registry32Format, QSettings::Registry64Format}) {
            QSettings reg(hive + "\\Software\\VideoLAN\\VLC", view);
            candidates << reg.value("InstallDir").toString() + "/vlc.exe"
                       << reg.value(".").toString();
            QSettings app(
                hive + "\\Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\vlc.exe", view);
            candidates << app.value(".").toString();
        }
    }
#endif
    for (const auto& env : {"ProgramFiles", "ProgramFiles(x86)", "ProgramW6432"})
        if (!qEnvironmentVariable(env).isEmpty())
            candidates << qEnvironmentVariable(env) + "/VideoLAN/VLC/vlc.exe";
    for (const auto& path : candidates)
        if (validVlc(path))
            return QDir::cleanPath(path);
    return {};
}
std::optional<std::array<int, 3>> stableVersion(QString value) {
    static const QRegularExpression re("^v?(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)(?:"
                                       "\\+[0-9A-Za-z-]+(?:\\.[0-9A-Za-z-]+)*)?$");
    auto m = re.match(value);
    if (!m.hasMatch())
        return {};
    std::array<int, 3> result{};
    for (int i = 0; i < 3; ++i) {
        bool ok = false;
        result[i] = m.captured(i + 1).toInt(&ok);
        if (!ok)
            return {};
    }
    return result;
}
} // namespace bitfly
