#pragma once
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
    void submit(QString id, QUrl url, QString vlc, QString destination);
    void cancel(const QString& id);
  signals:
    void ready(QString id);

  private:
    struct Job {
        QString id;
        QUrl url;
        QString vlc, destination;
    };
    void next();
    void finish();
    bool saveFrame();
    QQueue<Job> m_jobs;
    Job m_current;
    QProcess m_process;
    QTimer m_poll, m_timeout;
    std::unique_ptr<QTemporaryDir> m_directory;
};
} // namespace bitfly
