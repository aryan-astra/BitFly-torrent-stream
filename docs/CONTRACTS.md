# Integration contracts

C++20, Qt 6.8.3 Core/Gui/Quick/QuickControls2/Network/Sql/Test, libtorrent 2.0.11.
All network, torrent and stream work lives on one QThread event loop. UI communicates
using queued invocations and copied QVariant snapshots. No blocking network waits.

`bitfly::TorrentEngine : QObject, ByteSource`, constructor `(QString cacheRoot, QObject* parent=nullptr)`.
Public slots: `add(QString input)`, `select(QString sessionId, int fileIndex)`,
`pause(QString sessionId, bool paused)`, `remove(QString sessionId, bool deleteData)`,
`save()`, `restore()`, `stop()`. Engine signal `sessionsChanged(QVariantList)` emits
maps: id, name, state, progress (0..1), rate (bytes/s), peers, downloaded, total,
buffer (0..1), error, files (maps: index, name, size, playable), selected (int).
Signal `error(QString message)`, `streamReady(QString sessionId, QString streamId)`.
Opaque stream id is sessionId + '/' + file index; HTTP never interprets it as a path.
Implement deterministic scheduler/path tests under tests/torrent_tests.cpp with QtTest.

`bitfly::HttpServer : QObject` constructor `(ByteSource*, QObject* parent=nullptr)`.
`bool start()`, `void stop()`, `QUrl url(QString streamId) const`. Loopback, random port,
random token, no other endpoints. HTTP executes on same worker as ByteSource.
Unit/integration QtTest executable source tests/http_tests.cpp.

UI context property `app` is ApplicationController on the UI thread:
Properties: sessions QVariantList, history QVariantList, queue QVariantList,
error QString, vlcPath QString, cachePath QString, cacheBytes qint64,
cacheLimitGiB int (writable), seedMinutes int (writable), advanced bool (writable),
updateMessage QString, releaseUrl QUrl.
Invokables: openInput(QString), openTorrent(QUrl), play(QString id,int fileIndex),
enqueue(QString id,int fileIndex), moveQueue(int from,int to), removeQueue(int index),
pause(QString id,bool paused), remove(QString id,bool deleteData),
setVlcPath(QUrl), setCachePath(QUrl), checkUpdates(), clearError(), relaunch(QString id).
Additional invokables: playQueued(int index), finishSession(QString id,QString action)
where action is keep, delete or seed. Engine stopped() completes asynchronous shutdown.
Signal playbackEnded(QString id) prompts Keep/Delete/Seed in QML; signal changed()
notifies aggregate properties. Changes saved transactionally through SQLite.
QML root Main.qml, module BitFly, loaded qrc:/qt/qml/BitFly/qml/Main.qml (main may adjust).
Do not imply exact VLC playback position; HTTP demand drives scheduling.

Ownership: torrent agent owns src/torrent and torrent tests; HTTP agent owns
src/stream and HTTP tests; UI agent owns qml and docs/DESIGN.md. Root owns all
other paths, CMake, controller, persistence, platform integration, packaging and CI.
No agents commit. Root coordinates signed commits after tests.
