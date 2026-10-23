# ADR 001: native streaming architecture

Accepted 2026-10-23. C++20, dynamically linked Qt 6.8.3 and Qt Quick, libtorrent
2.0.11, Qt Network, SQLite through Qt SQL, CMake/Ninja. Qt 6.8 supports Windows
10 1809+ x64 and Windows 11 ARM64; x64 uses its matching MinGW 13.1 runtime
locally, CI uses MSVC 2022. Neither Electron nor embedded VLC is needed.

The GUI thread owns QML, the controller, SQLite settings/queue/history and
asynchronous owned VLC processes. A worker QThread owns the torrent engine,
alert timer, verified-piece cache and loopback HTTP server. Queued invocations
carry commands; copied QVariant snapshots carry UI state. libtorrent owns its
internal network/disk threads. HTTP never blocks waiting for pieces. Small
settings transactions run on the UI thread; large media I/O belongs to libtorrent.

The static QML interface uses Qt's software scene graph. Three warm x64 runs
measured about 80 MiB peak private committed memory and 130 MiB peak working set,
down from about 141 MiB and 185–191 MiB with the hardware renderer, with a 718 ms
median first frame. Startup remains sensitive to machine pressure; this choice
reduces renderer allocation without changing the streaming engine.

ByteSource is the module boundary: opaque file IDs, length/MIME description,
range demand, and nonblocking verified reads. HTTP has no filesystem interface.
Unpredictable tokens and a loopback-only random port limit local exposure.
HTTP buffering and verified piece retention are bounded independently of file
length. Content has native libtorrent disk storage: no unsafe partial-piece
eviction or fictitious zero-storage promise. Disk limits use admission/pausing,
not deletion of active media.

VLC is launched via absolute executable path and a QStringList, never a shell.
Each session has a dedicated process (`--no-one-instance --play-and-exit`). Its
exit is a session-end signal, not proof that the viewer watched to completion.
No exact playback position is inferred. Byte demands drive seek scheduling.
Cached content is retained until the user chooses deletion. Temporary seeding
is time-bounded; the default is fifteen minutes after a VLC process exits.
Queue entries are explicit next choices; they are advanced on VLC exit.

SQLite uses WAL and transactional JSON values under a versioned schema. Torrent
resume files are atomically replaced by the engine. Updates only query the fixed
official GitHub Releases API; a fixed official release URL is opened by the user.
No downloaded program is automatically run. No telemetry or cloud service.

References: [Qt supported Windows targets](https://doc.qt.io/qt-6.8/windows.html),
[libtorrent streaming](https://www.libtorrent.org/streaming.html),
[QProcess](https://doc.qt.io/qt-6.8/qprocess.html),
[GitHub runners](https://docs.github.com/en/actions/reference/runners/github-hosted-runners).
