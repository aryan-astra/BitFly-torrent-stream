#include "Platform.h"
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <wincrypt.h>
#endif
namespace bitfly {
bool prefersReducedMotion() {
#ifdef Q_OS_WIN
    BOOL enabled = TRUE;
    if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0))
        return !enabled;
#endif
    return false;
}
bool validPlayer(const QString& path) {
    const QFileInfo file(path);
    const QStringList supported = {"vlc.exe",      "mpv.exe",    "mpc-hc.exe",
                                   "mpc-hc64.exe", "mpc-be.exe", "mpc-be64.exe"};
    return file.isAbsolute() && file.isFile() &&
           supported.contains(file.fileName(), Qt::CaseInsensitive);
}
QStringList playerArguments(const QString& path, const QUrl& url) {
    const auto name = QFileInfo(path).fileName().toLower();
    if (name == "vlc.exe")
        return {"--no-one-instance", "--play-and-exit", url.toString(QUrl::FullyEncoded)};
    if (name == "mpv.exe")
        return {"--force-window=yes", "--keep-open=no", "--", url.toString(QUrl::FullyEncoded)};
    return {"/new", "/close", url.toString(QUrl::FullyEncoded)};
}
QByteArray protectSecret(const QString& secret) {
    if (secret.isEmpty())
        return {};
#ifdef Q_OS_WIN
    auto bytes = secret.toUtf8();
    DATA_BLOB input{DWORD(bytes.size()), reinterpret_cast<BYTE*>(bytes.data())}, output{};
    if (!CryptProtectData(&input, L"BitFly indexer key", nullptr, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN, &output))
        return {};
    const QByteArray protectedBytes(reinterpret_cast<char*>(output.pbData), output.cbData);
    LocalFree(output.pbData);
    return protectedBytes;
#else
    return {};
#endif
}
QString unprotectSecret(const QByteArray& secret) {
#ifdef Q_OS_WIN
    DATA_BLOB input{DWORD(secret.size()),
                    reinterpret_cast<BYTE*>(const_cast<char*>(secret.constData()))},
        output{};
    if (secret.isEmpty() || !CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
                                                CRYPTPROTECT_UI_FORBIDDEN, &output))
        return {};
    const QString value = QString::fromUtf8(reinterpret_cast<char*>(output.pbData), output.cbData);
    SecureZeroMemory(output.pbData, output.cbData);
    LocalFree(output.pbData);
    return value;
#else
    return {};
#endif
}
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
