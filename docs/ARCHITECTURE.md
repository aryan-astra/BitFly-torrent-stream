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

The QML interface uses the existing software scene graph with bounded raster
artwork and short event-driven transitions. Earlier renderer measurements are
historical; the redesigned build requires a fresh benchmark. Saved cover and
search views are virtualized; background/cover/icon decode sizes are bounded.

ByteSource is the module boundary: opaque file IDs, length/MIME description,
range demand, and nonblocking verified reads. HTTP has no filesystem interface.
Unpredictable tokens and a loopback-only random port limit local exposure.
HTTP buffering and verified piece retention are bounded independently of file
length. Content has native libtorrent disk storage: no unsafe partial-piece
eviction or fictitious zero-storage promise. Disk limits use admission/pausing,
not deletion of active media. Stream scheduling assigns deadlines only inside a
bounded window ahead of the playhead, so one demand can never touch the whole
torrent. Each session resolves its `save_path` against the storage root that was
current when it started; changing the root affects new sessions only, and the
engine scans every known root for quota and cache accounting.

Library artwork starts as deterministic local fallback art. Optional installed
VLC scene capture reads the existing tokenized HTTP stream asynchronously, one
owned decoder at a time with a 20-second deadline. The token is never persisted.
Local custom artwork is bounded before decoding. PNGs use QSaveFile and hashed
entry filenames; real/custom artwork survives metadata title changes. Only
explicit successful library removal deletes the owned artwork.

VLC, mpv and MPC-HC/BE are launched by absolute executable path and argument
list, never a shell. Dedicated-instance options make process exit attributable
to a session, not proof that its content was watched completely. Downloads pause
on player exit; additional seeding requires an explicit time-bounded choice.
Queue entries can reopen retained media through stable library identities.

SQLite uses WAL and transactional JSON values under a versioned schema. Torrent
resume files are atomically replaced by the engine. Optional Torznab search and
Cloudflare installed-client controls use bounded Qt network/process services on
the GUI event loop; the torrent/HTTP worker owns all libtorrent access. Search
keys are protected with Windows account DPAPI. Startup checks stable releases on
every process launch and may download a size/SHA-256-verified official installer;
installation requires an explicit action. Cloudflare is not bundled and is never
connected or reconfigured on startup. See INTEGRATIONS.md for source and trust
details. No telemetry is added.

References: [Qt supported Windows targets](https://doc.qt.io/qt-6.8/windows.html),
[libtorrent streaming](https://www.libtorrent.org/streaming.html),
[QProcess](https://doc.qt.io/qt-6.8/qprocess.html),
[GitHub runners](https://docs.github.com/en/actions/reference/runners/github-hosted-runners).
