#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>

namespace bitfly {
class NetworkAssistant final : public QObject {
    Q_OBJECT
  public:
    explicit NetworkAssistant(QObject* parent = nullptr);
    ~NetworkAssistant() override;
    bool installed() const {
        return !m_cli.isEmpty();
    }
    bool busy() const {
        return m_process.state() != QProcess::NotRunning;
    }
    QString status() const {
        return m_status;
    }
    void refresh();
    void connectClient();
    void disconnectClient();
    void setMode(QString mode);
    bool openDownloadPage();
    bool openClient();
    static bool supportedMode(const QString& mode);
    static QString findClient();
  signals:
    void changed();

  private:
    void run(QStringList arguments);
    QProcess m_process;
    QTimer m_deadline;
    QString m_cli, m_status;
    QByteArray m_output;
    bool m_statusRequest = false;
};
} // namespace bitfly
