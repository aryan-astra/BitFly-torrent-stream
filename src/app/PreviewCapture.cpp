#include "PreviewCapture.h"
#include "ThumbnailArt.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QProcessEnvironment>
#include <QRandomGenerator>

namespace bitfly {
PreviewCapture::PreviewCapture(QObject* parent) : QObject(parent) {
    m_process.setStandardOutputFile(QProcess::nullDevice());
    m_process.setStandardErrorFile(QProcess::nullDevice());
    m_poll.setInterval(300);
    m_timeout.setSingleShot(true);
    connect(&m_poll, &QTimer::timeout, this, [this] {
        if (saveFrame())
            finish();
    });
    connect(&m_timeout, &QTimer::timeout, this, &PreviewCapture::attemptEnded);
    connect(&m_process, &QProcess::finished, this, [this] { attemptEnded(); });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            attemptEnded();
    });
}
PreviewCapture::~PreviewCapture() {
    disconnect(&m_process, nullptr, this, nullptr);
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(500);
    }
}
void PreviewCapture::submit(QString id, QUrl url, QString vlc, QString destination,
                            int startSeconds) {
    if (id.isEmpty() || id == m_current.id || m_jobs.size() >= 16 || url.scheme() != "http" ||
        url.host() != "127.0.0.1" || !QFileInfo(vlc).isFile())
        return;
    for (const auto& job : m_jobs)
        if (job.id == id)
            return;
    m_jobs.enqueue({std::move(id), std::move(url), std::move(vlc), std::move(destination),
                    startSeconds < 0 ? QRandomGenerator::global()->bounded(30, 91) : startSeconds});
    next();
}
void PreviewCapture::cancel(const QString& id) {
    for (qsizetype i = m_jobs.size(); i-- > 0;)
        if (m_jobs[i].id == id)
            m_jobs.removeAt(i);
    if (m_current.id == id) {
        m_current.id.clear();
        finish();
    }
}
void PreviewCapture::next() {
    if (!m_current.id.isEmpty() || m_process.state() != QProcess::NotRunning || m_jobs.isEmpty())
        return;
    m_current = m_jobs.dequeue();
    m_directory = std::make_unique<QTemporaryDir>();
    if (!m_directory->isValid()) {
        finish();
        return;
    }
    startCurrent();
}
void PreviewCapture::startCurrent() {
    QFile::remove(m_directory->filePath("frame.png"));
    auto environment = QProcessEnvironment::systemEnvironment();
    for (const auto& key :
         {"QT_PLUGIN_PATH", "QT_QPA_PLATFORM_PLUGIN_PATH", "QML_IMPORT_PATH", "QML2_IMPORT_PATH"})
        environment.remove(key);
#ifdef Q_OS_WIN
    const QString windows = qEnvironmentVariable("SystemRoot", "C:/Windows");
    environment.insert("PATH", QFileInfo(m_current.vlc).absolutePath() + ";" + windows +
                                   "/System32;" + windows);
#endif
    m_process.setProcessEnvironment(environment);
    m_process.start(m_current.vlc,
                    {"--ignore-config", "--no-one-instance", "--intf=dummy", "--no-audio",
                     "--vout=dummy", "--play-and-exit", "--no-loop", "--no-repeat",
                     "--avcodec-hw=none", "--start-time=" + QString::number(m_current.startSeconds),
                     "--video-filter=scene", "--scene-format=png", "--scene-ratio=12",
                     "--scene-replace", "--scene-width=512", "--scene-path=" + m_directory->path(),
                     "--scene-prefix=frame", m_current.url.toString(QUrl::FullyEncoded)});
    m_poll.start();
    m_timeout.start(45000);
    emit started(m_current.id);
}
bool PreviewCapture::saveFrame() {
    if (m_current.id.isEmpty() || !m_directory)
        return false;
    const QString file = m_directory->filePath("frame.png");
    if (QFileInfo(file).size() > 4 * 1024 * 1024)
        return false;
    QImageReader reader(file);
    if (!reader.size().isValid() || reader.size().width() > 2048 || reader.size().height() > 2048)
        return false;
    const auto frame = reader.read();
    if (!usefulFrame(frame) || !writeArtworkImage(m_current.destination, frame))
        return false;
    const QString id = m_current.id;
    m_current.id.clear();
    emit ready(id);
    return true;
}
bool PreviewCapture::usefulFrame(const QImage& image) {
    // shortcut: luma rejects blank frames, not logos; add classification if later sampling is
    // insufficient.
    if (image.isNull())
        return false;
    const auto sample = image
                            .copy(image.width() / 10, image.height() / 5, image.width() * 8 / 10,
                                  image.height() * 3 / 5)
                            .scaled(64, 36, Qt::IgnoreAspectRatio)
                            .convertToFormat(QImage::Format_RGB32);
    double sum = 0, squares = 0;
    int visible = 0;
    for (int y = 0; y < sample.height(); ++y) {
        const auto* row = reinterpret_cast<const QRgb*>(sample.constScanLine(y));
        for (int x = 0; x < sample.width(); ++x) {
            const int light = qGray(row[x]);
            sum += light;
            squares += light * light;
            if (light > 22 && light < 240)
                ++visible;
        }
    }
    const double pixels = sample.width() * sample.height(), mean = sum / pixels;
    return mean > 12 && mean < 245 && visible > pixels * 0.08 &&
           squares / pixels - mean * mean > 100;
}
void PreviewCapture::attemptEnded() {
    if (saveFrame()) {
        finish();
        return;
    }
    m_poll.stop();
    m_timeout.stop();
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        return;
    }
    if (!m_current.id.isEmpty() && m_current.startSeconds > 0) {
        m_current.startSeconds = 0;
        startCurrent();
        return;
    }
    const QString id = m_current.id;
    m_current.id.clear();
    if (!id.isEmpty())
        emit failed(id);
    finish();
}
void PreviewCapture::finish() {
    m_poll.stop();
    m_timeout.stop();
    m_current = {};
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        // Wait for the owned decoder to exit before deleting its temporary output.
        return;
    }
    m_directory.reset();
    QTimer::singleShot(0, this, &PreviewCapture::next);
}
} // namespace bitfly
