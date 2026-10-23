#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTest>
#include <QImage>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFontInfo>
#include <QUrl>
#include <QVariantMap>

class MockApp final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList sessions MEMBER m_sessions NOTIFY changed)
    Q_PROPERTY(QVariantList history MEMBER m_history NOTIFY changed)
    Q_PROPERTY(QVariantList queue MEMBER m_queue NOTIFY changed)
    Q_PROPERTY(QString error MEMBER m_error NOTIFY changed)
    Q_PROPERTY(QString vlcPath MEMBER m_vlcPath NOTIFY changed)
    Q_PROPERTY(QString cachePath MEMBER m_cachePath NOTIFY changed)
    Q_PROPERTY(qint64 cacheBytes MEMBER m_cacheBytes NOTIFY changed)
    Q_PROPERTY(int cacheLimitGiB MEMBER m_cacheLimitGiB WRITE setCacheLimitGiB NOTIFY changed)
    Q_PROPERTY(int seedMinutes MEMBER m_seedMinutes WRITE setSeedMinutes NOTIFY changed)
    Q_PROPERTY(bool advanced MEMBER m_advanced WRITE setAdvanced NOTIFY changed)
    Q_PROPERTY(QString updateMessage MEMBER m_updateMessage NOTIFY changed)
    Q_PROPERTY(QUrl releaseUrl MEMBER m_releaseUrl NOTIFY changed)
public:
    explicit MockApp(QString scenario) {
        m_cacheBytes = 6LL * 1024 * 1024 * 1024;
        m_vlcPath = "C:/Program Files/VideoLAN/VLC/vlc.exe";
        m_cachePath = "C:/Users/Tester/AppData/Local/BitFly/cache";
        m_updateMessage = "You're up to date.";
        m_releaseUrl = QUrl("https://github.com/aryan-astra/BitFly-torrent-stream/releases");
        m_advanced = true;
        if (scenario == "empty") return;

        const QVariantList media = {
            QVariantMap{{"index", 0}, {"name", "Episode 04 - A Long Documentary Title (1080p WEB-DL).mkv"}, {"size", 2LL * 1024 * 1024 * 1024}, {"playable", true}},
            QVariantMap{{"index", 1}, {"name", "Episode 04.en.srt"}, {"size", 82LL * 1024}, {"playable", false}},
            QVariantMap{{"index", 2}, {"name", "Bonus Feature.mp4"}, {"size", 640LL * 1024 * 1024}, {"playable", true}}
        };
        m_sessions = {
            QVariantMap{{"id", "session-1"}, {"name", "A Very Long Torrent Release Name That Should Be Ellipsized Without Pushing Controls.mkv"},
                {"state", "Downloading"}, {"progress", 0.47}, {"rate", 7.8 * 1024 * 1024}, {"peers", 31},
                {"downloaded", 1006632960LL}, {"total", 2147483648LL}, {"buffer", 0.32}, {"error", ""}, {"files", media}, {"selected", 0}},
            QVariantMap{{"id", "session-2"}, {"name", "Another sample torrent"}, {"state", "Paused"}, {"progress", 0.82},
                {"rate", 0.0}, {"peers", 14}, {"downloaded", 1761607680LL}, {"total", 2147483648LL}, {"buffer", 0.71},
                {"error", ""}, {"files", media}, {"selected", 2}}
        };
        for (int i = 0; i < 12; ++i) {
            m_queue.append(QVariantMap{{"id", "session-1"}, {"fileIndex", i % 2 ? 2 : 0},
                                       {"name", QString("Queued title %1 with a long descriptive name").arg(i + 1)},
                                       {"fileName", i % 2 ? "Bonus Feature.mp4" : "Episode 04.mkv"}});
        }
        m_history = {
            QVariantMap{{"id", "session-1"}, {"name", "Previously played film"}, {"fileName", "Film.mkv"}},
            QVariantMap{{"id", "session-2"}, {"name", "Previously played episode"}, {"fileName", "Episode.mp4"}}
        };
    }

    Q_INVOKABLE void openInput(const QString &) {}
    Q_INVOKABLE void openTorrent(const QUrl &) {}
    Q_INVOKABLE void play(const QString &, int) {}
    Q_INVOKABLE void playQueued(int) {}
    Q_INVOKABLE void enqueue(const QString &, int) {}
    Q_INVOKABLE void moveQueue(int, int) {}
    Q_INVOKABLE void removeQueue(int) {}
    Q_INVOKABLE void pause(const QString &, bool) {}
    Q_INVOKABLE void remove(const QString &, bool) {}
    Q_INVOKABLE void finishSession(const QString &, const QString &) {}
    Q_INVOKABLE void setVlcPath(const QUrl &) {}
    Q_INVOKABLE void setCachePath(const QUrl &) {}
    Q_INVOKABLE void checkUpdates() {}
    Q_INVOKABLE void clearError() { m_error.clear(); emit changed(); }
    Q_INVOKABLE void relaunch(const QString &) {}
    void setCacheLimitGiB(int value) { m_cacheLimitGiB = value; emit changed(); }
    void setSeedMinutes(int value) { m_seedMinutes = value; emit changed(); }
    void setAdvanced(bool value) { m_advanced = value; emit changed(); }
signals:
    void changed();
    void playbackEnded(const QString &id);
private:
    QVariantList m_sessions, m_history, m_queue;
    QString m_error = "Tracker connection timed out. BitFly will keep trying.";
    QString m_vlcPath, m_cachePath, m_updateMessage;
    qint64 m_cacheBytes = 0;
    int m_cacheLimitGiB = 20, m_seedMinutes = 15;
    bool m_advanced = false;
    QUrl m_releaseUrl;
};

int main(int argc, char **argv) {
    QString scale = "1";
    QString scenario = "dense";
    QString page = "Stream";
    QString output;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg.startsWith("--scale=")) scale = arg.mid(8);
        else if (arg.startsWith("--scenario=")) scenario = arg.mid(11);
        else if (arg.startsWith("--page=")) page = arg.mid(7);
        else if (arg.startsWith("--output=")) output = arg.mid(9);
    }
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_SCALE_FACTOR", scale.toLocal8Bit());
    if (QFileInfo::exists("C:/Windows/Fonts/segoeui.ttf"))
        qputenv("QT_QPA_FONTDIR", "C:/Windows/Fonts");
    QGuiApplication app(argc, argv);
    qInfo() << "Segoe UI available:" << QFontDatabase::families().contains("Segoe UI")
            << "default font:" << QFontInfo(app.font()).family();
    MockApp controller(scenario);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("app", &controller);
    engine.load(QUrl::fromLocalFile(QStringLiteral(BITFLY_QML_MAIN)));
    if (engine.rootObjects().isEmpty()) return 2;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!window) return 3;
    window->setWidth(900);
    window->setHeight(600);
    window->setProperty("page", page);
    window->show();
    QTest::qWait(350);
    const QImage image = window->grabWindow();
    if (image.isNull() || image.width() < 900 || image.height() < 600) return 4;
    if (!output.isEmpty()) {
        QFileInfo info(output);
        QDir().mkpath(info.absolutePath());
        if (!image.save(output)) return 5;
    }
    return 0;
}

#include "qml_smoke.moc"
