#include "ThumbnailArt.h"
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFontInfo>
#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>
#include <QUrl>
#include <QVariantMap>
#include <cstdio>
#include <functional>

class MockApp final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList sessions MEMBER m_sessions NOTIFY changed)
    Q_PROPERTY(QVariantList library MEMBER m_library NOTIFY changed)
    Q_PROPERTY(QVariantList history MEMBER m_history NOTIFY changed)
    Q_PROPERTY(QVariantList queue MEMBER m_queue NOTIFY changed)
    Q_PROPERTY(QString error MEMBER m_error NOTIFY changed)
    Q_PROPERTY(QString notice MEMBER m_notice NOTIFY changed)
    Q_PROPERTY(QString vlcPath MEMBER m_vlcPath NOTIFY changed)
    Q_PROPERTY(QString playerPath MEMBER m_vlcPath NOTIFY changed)
    Q_PROPERTY(QString playerName READ playerName NOTIFY changed)
    Q_PROPERTY(bool reducedMotion MEMBER m_reducedMotion NOTIFY changed)
    Q_PROPERTY(bool generatePreviews MEMBER m_generatePreviews NOTIFY changed)
    Q_PROPERTY(int downloadLimitKiB MEMBER m_downloadLimitKiB NOTIFY changed)
    Q_PROPERTY(bool dhtEnabled MEMBER m_dhtEnabled NOTIFY changed)
    Q_PROPERTY(bool localDiscoveryEnabled MEMBER m_localDiscoveryEnabled NOTIFY changed)
    Q_PROPERTY(bool portMappingEnabled MEMBER m_portMappingEnabled NOTIFY changed)
    Q_PROPERTY(QString searchFeed MEMBER m_searchFeed NOTIFY changed)
    Q_PROPERTY(bool searchKeySaved MEMBER m_searchKeySaved NOTIFY changed)
    Q_PROPERTY(QVariantList searchResults MEMBER m_searchResults NOTIFY changed)
    Q_PROPERTY(bool searchBusy MEMBER m_searchBusy NOTIFY changed)
    Q_PROPERTY(QString searchMessage MEMBER m_searchMessage NOTIFY changed)
    Q_PROPERTY(bool updateBusy MEMBER m_updateBusy NOTIFY changed)
    Q_PROPERTY(double updateProgress MEMBER m_updateProgress NOTIFY changed)
    Q_PROPERTY(bool updateReady MEMBER m_updateReady NOTIFY changed)
    Q_PROPERTY(QString availableVersion MEMBER m_availableVersion NOTIFY changed)
    Q_PROPERTY(bool cloudflareInstalled MEMBER m_cloudflareInstalled NOTIFY changed)
    Q_PROPERTY(bool cloudflareBusy MEMBER m_cloudflareBusy NOTIFY changed)
    Q_PROPERTY(QString cloudflareStatus MEMBER m_cloudflareStatus NOTIFY changed)
    Q_PROPERTY(QString cachePath MEMBER m_cachePath NOTIFY changed)
    Q_PROPERTY(qint64 cacheBytes MEMBER m_cacheBytes NOTIFY changed)
    Q_PROPERTY(int cacheLimitGiB MEMBER m_cacheLimitGiB WRITE setCacheLimitGiB NOTIFY changed)
    Q_PROPERTY(int seedMinutes MEMBER m_seedMinutes WRITE setSeedMinutes NOTIFY changed)
    Q_PROPERTY(int connectionLimit MEMBER m_connectionLimit WRITE setConnectionLimit NOTIFY changed)
    Q_PROPERTY(int uploadLimitKiB MEMBER m_uploadLimitKiB WRITE setUploadLimitKiB NOTIFY changed)
    Q_PROPERTY(int streamAheadMiB MEMBER m_streamAheadMiB WRITE setStreamAheadMiB NOTIFY changed)
    Q_PROPERTY(bool advanced MEMBER m_advanced WRITE setAdvanced NOTIFY changed)
    Q_PROPERTY(QString updateMessage MEMBER m_updateMessage NOTIFY changed)
    Q_PROPERTY(QUrl releaseUrl MEMBER m_releaseUrl NOTIFY changed)
  public:
    explicit MockApp(QString scenario) {
        m_error.clear();
        m_cacheBytes = 6LL * 1024 * 1024 * 1024;
        m_vlcPath = "C:/Program Files/VideoLAN/VLC/vlc.exe";
        m_cachePath = "C:/Users/Tester/AppData/Local/BitFly/cache";
        m_updateMessage = "You're up to date.";
        m_releaseUrl = QUrl("https://github.com/aryan-astra/BitFly-torrent-stream/releases");
        m_advanced = true;
        if (scenario == "search") {
            m_searchFeed = "http://127.0.0.1:9117/api/v2.0/indexers/all/results/torznab";
            m_searchKeySaved = true;
            m_searchMessage = "2 results from your configured indexers.";
            m_searchResults = {QVariantMap{{"title", "Big Buck Bunny — Blender Open Movie"},
                                           {"size", 734003200LL},
                                           {"seeders", 42},
                                           {"provider", "Local test indexer"}},
                               QVariantMap{{"title", "Sintel — Blender Open Movie"},
                                           {"size", 1073741824LL},
                                           {"seeders", 17},
                                           {"provider", "Local test indexer"}}};
            return;
        }
        if (scenario == "empty")
            return;
        if (scenario == "shelf" || scenario == "large-library") {
            const QString titles[] = {"The Quiet Orbit", "Deep Field",          "Aperture",
                                      "Night Signals",   "Long Way Around",     "Paper Planets",
                                      "Static Bloom",    "The Last Projection", "Meridian",
                                      "Slow Light",      "Field Notes",         "After the Rain"};
            const QString dir = QDir(QDir::tempPath()).filePath("bitfly-qml-smoke-covers");
            const int count = scenario == "large-library" ? 1000 : 12;
            for (int i = 0; i < count; ++i) {
                const QString title = titles[i % 12];
                const QString id = QString("shelf-%1").arg(i + 1);
                const QString path = QDir(dir).filePath(QString("shelf-art-%1.png").arg(i % 12));
                if (i < 12)
                    bitfly::writeCoverArt(path, title, QString::number(i), nullptr, 384);
                m_library.append(QVariantMap{{"id", id},
                                             {"name", title},
                                             {"fileName", i % 2 ? "Episode 01.mkv" : "Feature.mp4"},
                                             {"total", qint64(420 + i * 137) * 1024 * 1024},
                                             {"active", false},
                                             {"available", i % 2 == 0},
                                             {"thumbnail", QUrl::fromLocalFile(path).toString()}});
            }
            return;
        }

        m_error = "Tracker connection timed out. BitFly will keep trying.";
        const QVariantList media = {
            QVariantMap{{"index", 0},
                        {"name", "Episode 04 - A Long Documentary Title (1080p WEB-DL).mkv"},
                        {"size", 2LL * 1024 * 1024 * 1024},
                        {"playable", true}},
            QVariantMap{{"index", 1},
                        {"name", "Episode 04.en.srt"},
                        {"size", 82LL * 1024},
                        {"playable", false}},
            QVariantMap{{"index", 2},
                        {"name", "Bonus Feature.mp4"},
                        {"size", 640LL * 1024 * 1024},
                        {"playable", true}}};
        m_sessions = {QVariantMap{{"id", "session-1"},
                                  {"previewStatus", "Generating video preview…"},
                                  {"name", "A Very Long Torrent Release Name That Should Be "
                                           "Ellipsized Without Pushing Controls.mkv"},
                                  {"state", "Downloading"},
                                  {"progress", 0.47},
                                  {"rate", 7.8 * 1024 * 1024},
                                  {"peers", 31},
                                  {"downloaded", 1006632960LL},
                                  {"total", 2147483648LL},
                                  {"buffer", 0.32},
                                  {"error", ""},
                                  {"files", media},
                                  {"selected", 0},
                                  {"thumbnail", ""}},
                      QVariantMap{{"id", "session-2"},
                                  {"name", "Another sample torrent"},
                                  {"state", "Paused"},
                                  {"progress", 0.82},
                                  {"rate", 0.0},
                                  {"peers", 14},
                                  {"downloaded", 1761607680LL},
                                  {"total", 2147483648LL},
                                  {"buffer", 0.71},
                                  {"error", ""},
                                  {"files", media},
                                  {"selected", 2},
                                  {"thumbnail", ""}}};
        for (int i = 0; i < 12; ++i) {
            m_queue.append(QVariantMap{
                {"id", "session-1"},
                {"fileIndex", i % 2 ? 2 : 0},
                {"name", QString("Queued title %1 with a long descriptive name").arg(i + 1)},
                {"fileName", i % 2 ? "Bonus Feature.mp4" : "Episode 04.mkv"}});
        }
        m_library = {QVariantMap{{"id", "library-1"},
                                 {"name", "Saved science documentary"},
                                 {"fileName", "Documentary.mkv"},
                                 {"total", 3221225472LL},
                                 {"active", false},
                                 {"thumbnail", ""}},
                     QVariantMap{{"id", "library-2"},
                                 {"name", "Saved animation collection"},
                                 {"fileName", "Animation.mp4"},
                                 {"total", 734003200LL},
                                 {"active", false},
                                 {"thumbnail", ""}},
                     QVariantMap{{"id", "session-1"},
                                 {"name", "Active entry hidden from the dimmed shelf"},
                                 {"active", true},
                                 {"thumbnail", ""}}};
        m_history = m_library;
    }

    Q_INVOKABLE void openInput(const QString&) {}
    Q_INVOKABLE void openTorrent(const QUrl&) {}
    Q_INVOKABLE void play(const QString&, int) {}
    Q_INVOKABLE void playQueued(int) {}
    Q_INVOKABLE void enqueue(const QString&, int) {}
    Q_INVOKABLE void moveQueue(int, int) {}
    Q_INVOKABLE void removeQueue(int) {}
    Q_INVOKABLE void pause(const QString&, bool) {}
    Q_INVOKABLE void remove(const QString&, bool) {}
    Q_INVOKABLE void finishSession(const QString&, const QString&) {}
    Q_INVOKABLE void refreshSessions() {
        if (!m_sessions.isEmpty()) {
            QVariantMap first = m_sessions.first().toMap();
            first.insert("progress", 0.56);
            m_sessions[0] = first;
            emit changed();
        }
    }
    QString playerName() const {
        return "VLC";
    }
    QString lastSearch;
    Q_INVOKABLE void setPlayerPath(const QUrl&) {}
    Q_INVOKABLE void refreshPreview(const QString&) {}
    Q_INVOKABLE void setLibraryArtwork(const QString&, const QUrl&) {}
    Q_INVOKABLE void configureSearch(const QString& feed, const QString& key) {
        m_searchFeed = feed;
        if (!key.isEmpty())
            m_searchKeySaved = true;
        emit changed();
    }
    Q_INVOKABLE void search(const QString& query) {
        lastSearch = query;
    }
    Q_INVOKABLE void cancelSearch() {}
    Q_INVOKABLE void openSearchResult(int) {}
    Q_INVOKABLE void downloadUpdate() {}
    Q_INVOKABLE void installUpdate() {}
    Q_INVOKABLE void refreshCloudflare() {}
    Q_INVOKABLE void connectCloudflare() {}
    Q_INVOKABLE void disconnectCloudflare() {}
    Q_INVOKABLE void setCloudflareMode(const QString&) {}
    Q_INVOKABLE void openCloudflareDownload() {}
    Q_INVOKABLE void openCloudflareClient() {}
    Q_INVOKABLE void loadFirstThumbnail() {
        if (m_sessions.isEmpty())
            return;
        const QString path = QDir(QDir::tempPath()).filePath("bitfly-qml-smoke-covers/morph.png");
        bitfly::writeCoverArt(path, "Cover transition fixture", "morph", nullptr, 384);
        auto first = m_sessions.first().toMap();
        auto url = QUrl::fromLocalFile(path);
        url.setQuery("v=123");
        first.insert("thumbnail", url.toString());
        first.insert("previewStatus", "");
        m_sessions[0] = first;
        emit changed();
    }
    Q_INVOKABLE void setVlcPath(const QUrl&) {}
    Q_INVOKABLE void setCachePath(const QUrl&) {}
    Q_INVOKABLE void checkUpdates() {}
    Q_INVOKABLE void clearError() {
        m_error.clear();
        emit changed();
    }
    Q_INVOKABLE void clearNotice() {
        m_notice.clear();
        emit changed();
    }
    Q_INVOKABLE void relaunch(const QString&) {}
    Q_INVOKABLE void removeLibraryEntry(const QString&) {}
    Q_INVOKABLE void openStorageFolder() {}
    Q_INVOKABLE void revealSession(const QString&) {}
    void setCacheLimitGiB(int value) {
        m_cacheLimitGiB = value;
        emit changed();
    }
    void setSeedMinutes(int value) {
        m_seedMinutes = value;
        emit changed();
    }
    void setConnectionLimit(int value) {
        m_connectionLimit = value;
        emit changed();
    }
    void setUploadLimitKiB(int value) {
        m_uploadLimitKiB = value;
        emit changed();
    }
    void setStreamAheadMiB(int value) {
        m_streamAheadMiB = value;
        emit changed();
    }
    void setAdvanced(bool value) {
        m_advanced = value;
        emit changed();
    }
  signals:
    void changed();
    void playbackEnded(const QString& id);

  private:
    QVariantList m_sessions, m_library, m_history, m_queue, m_searchResults;
    QString m_searchFeed, m_searchMessage, m_availableVersion;
    QString m_cloudflareStatus = "Cloudflare One Client is not installed.";
    bool m_reducedMotion = false, m_generatePreviews = true, m_dhtEnabled = true,
         m_localDiscoveryEnabled = true, m_portMappingEnabled = false;
    bool m_searchKeySaved = false, m_searchBusy = false, m_updateBusy = false,
         m_updateReady = false, m_cloudflareInstalled = false, m_cloudflareBusy = false;
    int m_downloadLimitKiB = 0;
    double m_updateProgress = 0;
    QString m_error = "Tracker connection timed out. BitFly will keep trying.";
    QString m_notice;
    QString m_vlcPath, m_cachePath, m_updateMessage;
    qint64 m_cacheBytes = 0;
    int m_cacheLimitGiB = 20, m_seedMinutes = 0;
    int m_connectionLimit = 200, m_uploadLimitKiB = 64, m_streamAheadMiB = 64;
    bool m_advanced = false;
    QUrl m_releaseUrl;
};

int main(int argc, char** argv) {
    QString scale = "1";
    QString scenario = "dense";
    QString page = "Library";
    QString output;
    int width = 900;
    int height = 600;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg.startsWith("--scale="))
            scale = arg.mid(8);
        else if (arg.startsWith("--scenario="))
            scenario = arg.mid(11);
        else if (arg.startsWith("--page="))
            page = arg.mid(7);
        else if (arg.startsWith("--output="))
            output = arg.mid(9);
        else if (arg.startsWith("--width="))
            width = arg.mid(8).toInt();
        else if (arg.startsWith("--height="))
            height = arg.mid(9).toInt();
    }
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_SCALE_FACTOR", scale.toLocal8Bit());
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    qputenv("QT_QUICK_BACKEND", "software");
    if (QFileInfo::exists("C:/Windows/Fonts/segoeui.ttf"))
        qputenv("QT_QPA_FONTDIR", "C:/Windows/Fonts");
    QGuiApplication app(argc, argv);
    qInfo() << "Segoe UI available:" << QFontDatabase::families().contains("Segoe UI")
            << "default font:" << QFontInfo(app.font()).family();
    MockApp controller(scenario);
    QQmlApplicationEngine engine;
    QStringList qmlWarnings;
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &engine,
                     [&qmlWarnings](const QList<QQmlError>& warnings) {
                         for (const auto& warning : warnings)
                             qmlWarnings.append(warning.toString());
                     });
    engine.rootContext()->setContextProperty("app", &controller);
    const auto mainUrl = QUrl::fromLocalFile(QStringLiteral(BITFLY_QML_MAIN));
    engine.load(mainUrl);
    if (engine.rootObjects().isEmpty()) {
        for (const auto& warning : qmlWarnings)
            std::fprintf(stderr, "QML warning: %s\n", qPrintable(warning));
        QQmlComponent diagnostic(&engine, mainUrl);
        for (const auto& error : diagnostic.errors())
            std::fprintf(stderr, "QML error: %s\n", qPrintable(error.toString()));
        std::fprintf(stderr, "No root objects after load; component status=%d\n",
                     int(diagnostic.status()));
        return 2;
    }
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (!window)
        return 3;
    window->setWidth(width);
    window->setHeight(height);
    window->setProperty("page", page);
    window->show();
    QTest::qWait(350);
    if (scenario == "dense") {
        window->setProperty("page", "Library");
        QTest::qWait(75);
        QObject* repeater = window->findChild<QObject*>("sessionRepeater");
        const auto firstCard = [repeater]() -> QQuickItem* {
            QQuickItem* item = nullptr;
            if (repeater)
                QMetaObject::invokeMethod(repeater, "itemAt", Qt::DirectConnection,
                                          Q_RETURN_ARG(QQuickItem*, item), Q_ARG(int, 0));
            return item;
        };
        QObject* card = firstCard();
        QObject* combo = card ? card->findChild<QObject*>("fileChoice_session-1") : nullptr;
        if (!card || !combo) {
            QObject* model = window->findChild<QObject*>("activeSessions");
            QObject* shelf = window->findChild<QObject*>("libraryRepeater");
            std::fprintf(stderr,
                         "Regression fixture: app.sessions=%d model.count=%d repeater.count=%d "
                         "library.count=%d card=%p combo=%p\n",
                         controller.property("sessions").toList().size(),
                         model ? model->property("count").toInt() : -1,
                         repeater ? repeater->property("count").toInt() : -1,
                         shelf ? shelf->property("count").toInt() : -1, static_cast<void*>(card),
                         static_cast<void*>(combo));
            for (const auto& warning : qmlWarnings)
                std::fprintf(stderr, "QML warning: %s\n", qPrintable(warning));
            return 9;
        }
        auto* viewport = window->findChild<QQuickItem*>("libraryRepeater");
        if (!viewport ||
            firstCard()->mapToScene(QPointF()).y() < viewport->mapToScene(QPointF()).y())
            return 26;
        if (!QMetaObject::invokeMethod(combo, "activated", Qt::DirectConnection, Q_ARG(int, 1)))
            return 10;
        if (card->property("selectedFileIndex").toInt() != 2)
            return 11;
        controller.refreshSessions();
        QCoreApplication::processEvents();
        if (firstCard() != card)
            return 12;
        if (card->property("selectedFileIndex").toInt() != 2)
            return 13;
        QObject* cover = card->findChild<QObject*>("cover_session-1");
        if (!cover || cover->property("ready").toBool() ||
            cover->property("currentCoverWidth").toDouble() > 73)
            return 15;
        controller.setProperty("reducedMotion", true);
        controller.loadFirstThumbnail();
        if (!QTest::qWaitFor([cover] { return cover->property("ready").toBool(); }, 3000))
            return 16;
        QTest::qWait(30);
        if (cover->property("currentCornerRadius").toDouble() > 13 ||
            cover->property("currentCoverWidth").toDouble() <= 73)
            return 17;
        controller.setProperty("reducedMotion", false);
        QObject* shelf = window->findChild<QObject*>("libraryRepeater");
        if (!shelf || shelf->property("count").toInt() != 2) {
            std::fprintf(stderr, "Library shelf expected 2 dimmed posters, got %d\n",
                         shelf ? shelf->property("count").toInt() : -1);
            return 14;
        }
    }
    for (const auto& [name, target] :
         {qMakePair("navLibrary", "Library"), qMakePair("navQueue", "Queue"),
          qMakePair("navSearch", "Search"), qMakePair("navSettings", "Settings")}) {
        QObject* button = window->findChild<QObject*>(QString::fromLatin1(name));
        if (!button || !QMetaObject::invokeMethod(button, "click"))
            return 6;
        QCoreApplication::processEvents();
        if (window->property("page").toString() != QLatin1String(target))
            return 7;
    }
    auto* queueNavigation = window->findChild<QQuickItem*>("navQueue");
    if (!queueNavigation)
        return 21;
    queueNavigation->forceActiveFocus(Qt::TabFocusReason);
    QTest::keyClick(window, Qt::Key_Space);
    QCoreApplication::processEvents();
    if (window->property("page").toString() != "Queue")
        return 22;
    window->setProperty("page", "Settings");
    auto* help = window->findChild<QQuickItem*>("help_Cache limit");
    auto* tooltip = help ? help->findChild<QObject*>("helpTooltip") : nullptr;
    if (!help || !tooltip)
        return 23;
    help->forceActiveFocus(Qt::TabFocusReason);
    if (!QTest::qWaitFor([tooltip] { return tooltip->property("visible").toBool(); }, 1500))
        return 24;
    queueNavigation->forceActiveFocus(Qt::TabFocusReason);
    if (scenario == "large-library") {
        window->setProperty("page", "Library");
        QTest::qWait(100);
        auto* shelf = window->findChild<QObject*>("libraryRepeater");
        const std::function<int(QQuickItem*)> countCards = [&countCards](QQuickItem* item) {
            int count = item->objectName().startsWith("libraryCard_") ? 1 : 0;
            for (auto* child : item->childItems())
                count += countCards(child);
            return count;
        };
        const int instantiated = countCards(window->contentItem());
        if (!shelf || shelf->property("count").toInt() != 1000 || instantiated == 0 ||
            instantiated > 50) {
            std::fprintf(
                stderr,
                "Library virtualization: expected1000 rows and1..50 instances, got%d instances\n",
                instantiated);
            return 20;
        }
    }
    if (scenario == "search") {
        window->setProperty("page", "Search");
        QTest::qWait(30);
        auto* query = window->findChild<QObject*>("torrentSearchInput");
        auto* button = window->findChild<QObject*>("torrentSearchButton");
        auto* secret = window->findChild<QObject*>("searchKeyInput");
        if (!query || !button || !secret || !secret->property("text").toString().isEmpty())
            return 18;
        query->setProperty("text", "  Blender  ");
        if (!QMetaObject::invokeMethod(button, "click") || controller.lastSearch != "Blender")
            return 19;
    }
    window->setProperty("page", page);
    QTest::qWait(500);
    if (!qmlWarnings.isEmpty()) {
        for (const auto& warning : qmlWarnings)
            qWarning().noquote() << warning;
        return 8;
    }
    const QImage image = window->grabWindow();
    if (image.isNull() || image.width() < width || image.height() < height)
        return 4;
    if (!output.isEmpty()) {
        QFileInfo info(output);
        QDir().mkpath(info.absolutePath());
        if (!image.save(output))
            return 5;
    }
    return 0;
}

#include "qml_smoke.moc"
