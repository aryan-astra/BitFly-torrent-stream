#pragma once
#include <QImage>
#include <QObject>
#include <QProcess>
#include <QQueue>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <memory>

namespace bitfly {
class PreviewCapture final : public QObject {
    Q_OBJECT
  public:
    explicit PreviewCapture(QObject* parent = nullptr);
    ~PreviewCapture() override;
    void submit(QString id, QUrl url, QString vlc, QString destination, int startSeconds = -1);
    void cancel(const QString& id);
    static bool usefulFrame(const QImage& image);
  signals:
    void ready(QString id);
    void failed(QString id);

  private:
    struct Job {
        QString id;
        QUrl url;
        QString vlc, destination;
        int startSeconds = 0;
    };
    void next();
    void finish();
    void startCurrent();
    void attemptEnded();
    bool saveFrame();
    QQueue<Job> m_jobs;
    Job m_current;
    QProcess m_process;
    QTimer m_poll, m_timeout;
    std::unique_ptr<QTemporaryDir> m_directory;
};
} // namespace bitfly
