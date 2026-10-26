#include "NetworkAssistant.h"
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QUrl>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace bitfly {
QString NetworkAssistant::findClient() {
#ifdef Q_OS_WIN
    for (const auto& variable : {"ProgramW6432", "ProgramFiles", "ProgramFiles(x86)"}) {
        const auto base = qEnvironmentVariable(variable);
        if (base.isEmpty())
            continue;
        const QFileInfo candidate(base + "/Cloudflare/Cloudflare WARP/warp-cli.exe");
        if (candidate.isAbsolute() && candidate.isFile() && !candidate.isSymLink())
            return candidate.canonicalFilePath();
    }
#endif
    return {};
}
bool NetworkAssistant::supportedMode(const QString& mode) {
    const QStringList modes{"doh", "dot", "warp", "warp+doh", "warp+dot", "tunnel_only", "proxy"};
    return modes.contains(mode);
}
NetworkAssistant::NetworkAssistant(QObject* parent) : QObject(parent), m_cli(findClient()) {
    m_deadline.setSingleShot(true);
#ifdef Q_OS_WIN
    m_process.setCreateProcessArgumentsModifier(
        [](QProcess::CreateProcessArguments* arguments) { arguments->flags |= CREATE_NO_WINDOW; });
#endif
    auto read = [this] {
        m_output += m_process.readAllStandardOutput();
        m_output += m_process.readAllStandardError();
        if (m_output.size() > 8192) {
            m_output = "Cloudflare returned an unexpectedly large response.";
            m_process.kill();
        }
    };
    connect(&m_process, &QProcess::readyReadStandardOutput, this, read);
    connect(&m_process, &QProcess::readyReadStandardError, this, read);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            m_deadline.stop();
            m_status =
                "Could not start the installed Cloudflare client. Open Cloudflare to finish setup.";
            emit changed();
        }
    });
    connect(&m_process, &QProcess::finished, this,
            [this, read](int code, QProcess::ExitStatus exitStatus) {
                m_deadline.stop();
                read();
                const auto text = QString::fromUtf8(m_output).trimmed().left(2000);
                const bool success = code == 0 && exitStatus == QProcess::NormalExit;
                m_status = text.isEmpty() ? (success ? "Cloudflare command completed."
                                                     : "Cloudflare could not complete this action. "
                                                       "Open its app to check setup and policy.")
                                          : text;
                emit changed();
                if (success && !m_statusRequest)
                    QTimer::singleShot(0, this, &NetworkAssistant::refresh);
            });
    connect(&m_deadline, &QTimer::timeout, this, [this] {
        m_output = "Cloudflare did not respond in time. Open its app to check the connection.";
        m_process.kill();
    });
    m_status = installed()
                   ? "Cloudflare is installed. Refresh to read its status."
                   : "Cloudflare is not installed. Get the official client to use these controls.";
}
NetworkAssistant::~NetworkAssistant() {
    m_process.disconnect(this);
    if (busy()) {
        m_process.kill();
        m_process.waitForFinished(1000);
    }
}
void NetworkAssistant::run(QStringList arguments) {
    if (busy())
        return;
    m_cli = findClient();
    if (m_cli.isEmpty()) {
        m_status = "Install the official Cloudflare client and finish its setup first.";
        emit changed();
        return;
    }
    m_statusRequest = arguments == QStringList{"status"};
    m_output.clear();
    m_status = "Contacting Cloudflare…";
    arguments.prepend("--no-paginate");
    arguments.prepend("--no-ansi");
    m_process.start(m_cli, arguments, QIODevice::ReadOnly);
    m_deadline.start(10000);
    emit changed();
}
void NetworkAssistant::refresh() {
    run({"status"});
}
void NetworkAssistant::connectClient() {
    run({"connect"});
}
void NetworkAssistant::disconnectClient() {
    run({"disconnect"});
}
void NetworkAssistant::setMode(QString mode) {
    if (!supportedMode(mode)) {
        m_status = "Choose one of the supported Cloudflare modes.";
        emit changed();
        return;
    }
    run({"mode", mode});
}
bool NetworkAssistant::openDownloadPage() {
    return QDesktopServices::openUrl(
        QUrl("https://developers.cloudflare.com/cloudflare-one/team-and-resources/devices/"
             "cloudflare-one-client/download/"));
}
bool NetworkAssistant::openClient() {
    const auto cli = findClient();
    if (cli.isEmpty())
        return openDownloadPage();
    const auto app = QFileInfo(cli).dir().filePath("Cloudflare WARP.exe");
    return QFileInfo::exists(app) && QProcess::startDetached(app, {});
}
} // namespace bitfly
