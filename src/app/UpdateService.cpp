#include "UpdateService.h"
#include "Platform.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

namespace bitfly {
namespace {
constexpr qint64 maximumInstaller = 256LL * 1024 * 1024;
QString architecture() {
#if defined(Q_PROCESSOR_ARM_64)
    return "arm64";
#else
    return "x64";
#endif
}
} // namespace
UpdateService::UpdateService(QString currentVersion, QObject* parent)
    : QObject(parent), m_currentVersion(std::move(currentVersion)) {
    m_deadline.setSingleShot(true);
    connect(&m_deadline, &QTimer::timeout, this,
            [this] { fail("Update request timed out. Try again."); });
}
UpdateService::~UpdateService() {
    blockSignals(true);
    cancel();
}
bool UpdateService::officialUrl(const QUrl& url) {
    const QStringList hosts{"api.github.com", "github.com", "release-assets.githubusercontent.com",
                            "objects.githubusercontent.com"};
    return url.isValid() && url.scheme() == "https" && url.userInfo().isEmpty() &&
           (url.port() == -1 || url.port() == 443) && hosts.contains(url.host().toLower());
}
std::optional<UpdateRelease> UpdateService::parseRelease(const QByteArray& data,
                                                         const QString& currentVersion,
                                                         const QString& arch, QString* error) {
    auto reject = [error](QString message) -> std::optional<UpdateRelease> {
        if (error)
            *error = message;
        return {};
    };
    if (data.size() > 1024 * 1024)
        return reject("Release information is too large.");
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return reject("Could not read official release information.");
    const auto json = document.object();
    const auto tag = json.value("tag_name").toString();
    const auto version = stableVersion(tag), current = stableVersion(currentVersion);
    if (!version || !current || json.value("prerelease").toBool() || json.value("draft").toBool())
        return reject("No stable release information is available.");
    if (*version <= *current)
        return reject("You’re using the latest published version.");
    if (arch != "x64" && arch != "arm64")
        return reject("This architecture has no supported installer.");
    const auto number = QString::number((*version)[0]) + "." + QString::number((*version)[1]) +
                        "." + QString::number((*version)[2]);
    const auto fileName = "BitFly-" + number + "-windows-" + arch + "-setup.exe";
    static const QRegularExpression digestPattern("^sha256:([0-9a-fA-F]{64})$");
    for (const auto& value : json.value("assets").toArray()) {
        const auto asset = value.toObject();
        if (asset.value("name").toString() != fileName)
            continue;
        const auto digest = digestPattern.match(asset.value("digest").toString());
        const QUrl url(asset.value("browser_download_url").toString());
        const auto expectedPath =
            "/aryan-astra/BitFly-torrent-stream/releases/download/" + tag + "/" + fileName;
        const double bytes = asset.value("size").toDouble(-1);
        if (!digest.hasMatch() || !officialUrl(url) || url.host() != "github.com" ||
            url.path() != expectedPath || !url.query().isEmpty() || bytes < 1 ||
            bytes > maximumInstaller || bytes != qint64(bytes))
            return reject(
                "This release has no verifiable installer. Open the official release page.");
        return UpdateRelease{tag, fileName, url, QByteArray::fromHex(digest.captured(1).toLatin1()),
                             qint64(bytes)};
    }
    return reject("The latest release has no installer for this PC yet.");
}
double UpdateService::progress() const {
    return m_downloading && m_release ? double(m_received) / double(m_release->size) : 0;
}
void UpdateService::check() {
    if (busy())
        return;
    m_release.reset();
    m_installer.clear();
    m_body.clear();
    m_received = 0;
    m_downloading = false;
    m_message = "Checking official releases…";
    m_deadline.start(15000);
    fetch(QUrl("https://api.github.com/repos/aryan-astra/BitFly-torrent-stream/releases/latest"));
}
void UpdateService::download() {
    if (busy() || !m_release)
        return;
    const auto directory =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/updates";
    if (!QDir().mkpath(directory)) {
        fail("Could not create the update folder.");
        return;
    }
    const auto path = directory + "/BitFly-update-" + architecture() + ".exe";
    if (verifiedInstaller(path, m_release->sha256, m_release->size)) {
        m_installer = path;
        m_received = m_release->size;
        m_downloading = true;
        m_message = "Update verified. Ready to install.";
        emit changed();
        return;
    }
    m_output = std::make_unique<QSaveFile>(path);
    m_output->setDirectWriteFallback(false);
    if (!m_output->open(QIODevice::WriteOnly)) {
        fail("Could not save the update installer.");
        return;
    }
    m_installer.clear();
    m_received = 0;
    m_hash.reset();
    m_progressClock.start();
    m_downloading = true;
    m_message = "Downloading update…";
    m_deadline.start(5 * 60 * 1000);
    fetch(m_release->url);
}
void UpdateService::fetch(const QUrl& url, int redirects) {
    if (!officialUrl(url) || redirects > 5) {
        fail("The update download redirected to an untrusted location.");
        return;
    }
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "BitFly/" + m_currentVersion);
    request.setRawHeader("Accept", m_downloading ? "application/octet-stream"
                                                 : "application/vnd.github+json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(15000);
    auto* reply = m_network.get(request);
    reply->setReadBufferSize(64 * 1024);
    m_reply = reply;
    connect(reply, &QNetworkReply::readyRead, this, &UpdateService::consume);
    connect(reply, &QNetworkReply::finished, this, [this, reply, redirects] {
        if (m_reply != reply) {
            reply->deleteLater();
            return;
        }
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status >= 300 && status <= 399) {
            const auto target = reply->url().resolved(
                reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl());
            m_reply.clear();
            reply->deleteLater();
            fetch(target, redirects + 1);
            return;
        }
        consume();
        if (m_reply != reply) {
            reply->deleteLater();
            return;
        }
        m_reply.clear();
        m_deadline.stop();
        if (status == 404 && !m_downloading)
            fail("No stable release has been published yet.");
        else if (reply->error() != QNetworkReply::NoError || status != 200)
            fail("Could not fetch the update. Check your connection and try again.");
        else if (m_downloading) {
            if (!m_release || m_received != m_release->size || m_hash.result() != m_release->sha256)
                fail("The update checksum did not match. The installer was discarded.");
            else {
                const auto path = m_output->fileName();
                if (!m_output->commit())
                    fail("Could not finish saving the update.");
                else {
                    m_output.reset();
                    m_installer = path;
                    m_message = "Update verified. Ready to install.";
                }
            }
        } else {
            QString message;
            m_release = parseRelease(m_body, m_currentVersion, architecture(), &message);
            m_body.clear();
            m_message = m_release ? "BitFly " + m_release->version + " is available." : message;
        }
        reply->deleteLater();
        emit changed();
    });
    emit changed();
}
void UpdateService::consume() {
    if (!m_reply)
        return;
    const int status = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status != 200) {
        m_reply->readAll();
        return;
    }
    const auto bytes = m_reply->readAll();
    m_received += bytes.size();
    const auto limit = m_downloading && m_release ? m_release->size : 1024 * 1024;
    if (m_received > limit) {
        fail("The update response exceeded its expected size.");
        return;
    }
    if (m_downloading) {
        if (!m_output || m_output->write(bytes) != bytes.size()) {
            fail("Could not write the update installer.");
            return;
        }
        m_hash.addData(bytes);
    } else
        m_body += bytes;
    if (m_downloading && m_progressClock.elapsed() >= 100) {
        m_progressClock.restart();
        emit changed();
    }
}
void UpdateService::cancel() {
    m_deadline.stop();
    if (m_reply) {
        auto* reply = m_reply.data();
        m_reply.clear();
        reply->abort();
        reply->deleteLater();
    }
    m_output.reset();
    m_body.clear();
    emit changed();
}
void UpdateService::fail(QString message) {
    cancel();
    m_message = std::move(message);
    emit changed();
}
bool UpdateService::launchInstaller() {
    if (busy() || m_installer.isEmpty() || !m_release)
        return false;
    if (!verifiedInstaller(m_installer, m_release->sha256, m_release->size)) {
        m_installer.clear();
        fail("The saved installer changed. Download the update again.");
        return false;
    }
    // The digest comes from the official HTTPS GitHub API, not an independent signing key.
    if (!QProcess::startDetached(m_installer, {"/CURRENTUSER", "/NORESTART",
                                               "/DIR=" + QCoreApplication::applicationDirPath()})) {
        fail("Could not start the update installer.");
        return false;
    }
    return true;
}
bool UpdateService::verifiedInstaller(const QString& path, const QByteArray& sha256, qint64 size) {
    QFile file(path);
    QCryptographicHash hash(QCryptographicHash::Sha256);
    return size > 0 && size <= maximumInstaller && sha256.size() == 32 &&
           file.open(QIODevice::ReadOnly) && file.size() == size && hash.addData(&file) &&
           hash.result() == sha256;
}
} // namespace bitfly
