# Feature research for the BitFly redesign

Research scope: `docs/REDESIGN_PLAN.md`, pinned libtorrent 2.0.11 source under
`.tools/libtorrent`, and primary project/protocol documentation only. “Existing”
means the redesign plan records it as implemented; this note did not audit the
implementation or execute builds. “Implement now” means a low-risk, user-visible
change the current design can support. “Safe follow-up” means technically
possible but needs a separate UX/engine contract. “Unavailable” means the
requested semantics are not provided by the current supported APIs.

Implementation update (2026-10-26): the prior aggressive socket/session pack was
replaced with library defaults and supported controls (200 peers, 64 KiB/s upload,
unlimited download by default). The installed VLC 3.0.24 scene-filter spike
produced a valid 512px PNG; bounded asynchronous capture is now wired into the
controller and has a dedicated HTTP/decoder validation gate. No VLC runtime is
bundled. The matrix below is a research backlog, not a claim that all items ship.

## Answers to the requested technical questions

### Change the save folder, including to another drive

The current design’s behavior—apply the selected root to new downloads and keep
active sessions at their original roots—is the safest immediate UX. libtorrent
2.0.11 also has `torrent_handle::move_storage()`: it asynchronously moves a
torrent’s data and updates its storage path, including copying between drives.
The official reference warns that it blocks other disk I/O during the move and
can lower transfer rates; it also documents move success/failure alerts and
important caveats around files outside the torrent’s save directory. Thus a
future “Move active download” action is supported, but should be explicit,
serialized/queued, ownership-checked, and update persisted paths only after the
success alert. Do not implement it as a raw filesystem copy behind libtorrent.

### Delete watched pieces once playback is 5–10 minutes ahead

There is no supported public per-piece “forget / delete backing bytes” operation
in the pinned torrent-handle API. `piece_priority(0)` controls whether a piece
will be selected for download; it does not evict a verified piece already in
storage. Deleting or truncating data files behind a live torrent can invalidate
piece-picker/storage state and race VLC reads. The BEP 3 wire protocol describes
request, piece, and cancel messages; it does not define a mechanism for a
downloader to retract previously acquired bytes. The sound immediate behavior
is what the plan has: bound the *future request/deadline window* and purge whole
session data after stop. Retain a 5–10 minute rolling window only as a research
item requiring an application-owned cache/custom-storage design or library
support; it is not a small safe patch.

### Maximize download bandwidth and avoid seeding during download

No setting guarantees maximum throughput. Peer availability/quality, swarm
health, ISP/network path, latency, disk performance, CPU, and protocol overhead
can dominate. The plan’s many overrides change tradeoffs; they do not prove a
faster download for this user’s network. In particular, 500 connections can
increase peer/socket/FD/kernel-resource overhead without yielding 500 useful
peers. A configured 4 MiB send and receive socket buffer is a per-socket buffer
setting, not a throughput multiplier; actual memory pressure depends on OS
buffer accounting/autotuning and connected peers. Do not describe 500 plus 4 MiB
as a safe universal profile or multiply it into a guaranteed allocation.

The libtorrent setting `upload_rate_limit = 0` means **unlimited**, not zero
upload. A finite low cap (such as the documented plan’s 64 KiB/s) limits upload
but still uploads. `torrent_handle::set_max_uploads(0)` also means unlimited:
the pinned `torrent.cpp` converts nonpositive values to `(1 << 24) - 1`.
Neither zero setting is a verified zero-upload mode. Pausing stops downloads too.
Keep a bounded low upload cap as the default and offer a clearly labeled
“minimize upload” mode only if its tradeoff is explicit. Measure a few
representative swarms before promoting any preset as faster.

### Persist a real video preview thumbnail

The installed VLC runtime makes a second decoder dependency unnecessary in
principle. VLC includes a scene video filter, and its command-line/module
options can be used to decode a frame to an image. A practical implementation
can start an isolated VLC child through `QProcess` against a local media path,
request one scene/snapshot to a temporary file, enforce a short deadline with a
`QTimer`, kill on timeout, validate image decode and dimensions, then atomically
replace the library thumbnail. Use a `QStringList` argument vector and an
absolute known VLC executable; never interpolate torrent filenames into a
shell command. Run asynchronously so the GUI does not block. This is a
supported VLC capability plus Qt process capability; exact CLI flags vary by
VLC build and should be verified against the separately installed VLC runtime. Trigger
only once selected media has enough local bytes to decode; partial or encrypted,
unsupported, or audio-only media must fall back to generated art. Bound child
runtime, output size, and concurrent jobs because media parsing is untrusted
input. The source supports the scene filter, but official user docs found in
this pass did not specify a stable headless CLI recipe, so treat that command
line as a small installed-runtime spike before wiring it into the library.

## Prioritized feature and answer matrix

Priority is user value versus implementation risk. Status reflects the redesign
plan’s claims and research findings; it is not a source-code audit.

| # | Priority | Feature / answer | Status | Technical risk / next validation |
|---:|:---:|---|---|---|
| 1 | P0 | Choose a different save folder for future downloads, including another drive | Existing | Confirm the setting stores the new root, checks writability, and active rows retain their own `save_path`. |
| 2 | P0 | Move an already active download across drives | Safe follow-up | `move_storage()` is async and blocks disk I/O; handle success/failure alerts, prevent conflicting moves, and persist path only on success. |
| 3 | P0 | Delete all session data after stopping/removing a retained library item | Existing | Use libtorrent `remove_torrent(delete_files)` where appropriate; app purge must continue checking ownership markers and links. |
| 4 | P0 | Delete watched pieces 5–10 min behind playback | Unavailable | No public piece eviction API; priority 0 is download selection, not erase. Requires custom storage/cache semantics or library capability. |
| 5 | P0 | Bound streaming read-ahead/deadlines around playhead | Existing | Verify window is bounded by bytes/pieces, old deadlines clear, and pause/seek/reset paths do not leave stale urgency. |
| 6 | P0 | Persist real decoded poster thumbnails | Implemented / tested | Use installed VLC scene/snapshot capability in async isolated `QProcess`; first validate exact 3.0.24 headless flags and partial-file behavior. |
| 7 | P0 | Keep generated art fallback for audio-only, partial, or unsupported media | Existing | Retain deterministic fallback and replace atomically only after a valid real image has decoded. |
| 8 | P0 | No seeding/upload while still downloading | Safe follow-up | `upload_rate_limit=0` is unlimited. A zero unchoke cap can hurt tit-for-tat; compare with a low finite cap on real swarms. |
| 9 | P0 | Fastest download / preset | Implement now | Label current values as a bounded profile, not “maximum”; collect download, upload, peer, CPU, and disk metrics for comparisons. |
| 10 | P1 | Sensible connection limit for different machines/networks | Safe follow-up | 500 is not inherently faster; preserve a conservative default and validate ranges against peer quality, FD limits, and memory. |
| 11 | P1 | Avoid 4 MiB socket buffer as universal speed claim | Implement now | libtorrent says 0 uses OS defaults; 4 MiB may raise kernel pressure across many sockets and platform behavior differs. Benchmark before keeping. |
| 12 | P1 | Live upload cap while downloading | Existing | Correctly label in KiB/s; note any nonzero value still permits payload upload. |
| 13 | P1 | Pause/resume a session without losing library metadata | Existing | Check pause state, engine alerts, and relaunch mapping remain consistent. |
| 14 | P1 | Restart a retained library entry from saved magnet/file index | Existing | Validate stale/expired metadata and missing storage paths produce actionable errors. |
| 15 | P1 | Reveal a session’s actual folder after root changes | Existing | Resolve active session path first and historical `save_path` second; folder may have been moved externally. |
| 16 | P1 | Set a separate download cap | Implemented / tested | libtorrent supports global and per-torrent download limits; user-facing setting should explain aggregate versus per-session semantics. |
| 17 | P1 | Avoid downloading unwanted files in multi-file torrents | Safe follow-up | File priorities exist, but changing priorities can cause storage/partfile I/O; don’t offer destructive exclusions without tests. |
| 18 | P1 | Stream file selection and prioritize selected file | Existing | Verify file priorities plus piece deadline scheduling agree; file-priority updates reset piece priorities. |
| 19 | P1 | Detect disk-full / low space before accepting downloads | Existing | Preserve quota admission, probe selected volume rather than only system volume, surface write errors as the source of truth. |
| 20 | P1 | Recover safely when a cache root is unavailable at startup | Safe follow-up | Keep recorded roots, detect unavailable volumes, and avoid silently remapping active data to a different root. |
| 21 | P2 | Per-torrent upload/connection controls | Safe follow-up | libtorrent exposes per-torrent caps; add only if users need a session-specific exception to the global profile. |
| 22 | P2 | Show live peer count and current download/upload rates | Safe follow-up | Surface existing status snapshots; sampling should be throttled and distinguish zero peers from stale status. |
| 23 | P2 | Pause automatically when VLC closes, then optionally seed for a timer | Existing | Verify VLC lifecycle detection and timer cancellation when replay starts. |
| 24 | P2 | Keep session data while removing only a library card | Safe follow-up | Distinguish “forget from library” and destructive purge in labels/confirmation; avoid deletion on a non-destructive remove path. |
| 25 | P2 | Queue multiple torrents while limiting simultaneous activity | Safe follow-up | `active_downloads` applies only to auto-managed torrents; redesign says torrents are non-auto-managed, so libtorrent queue settings are inert. App queue must enforce concurrency itself. |

## Sources (primary)

- Pinned source: `.tools/libtorrent` is the project’s libtorrent 2.0.11 checkout;
  inspect `include/libtorrent/torrent_handle.hpp`, `include/libtorrent/settings_pack.hpp`,
  and `src/settings_pack.cpp` for the exact pinned API and defaults. The checkout
  must be read as source, not executed.
- [libtorrent 2.0 reference: torrent_handle](https://libtorrent.org/reference-Torrent_Handle.html)
  — `move_storage`, pause/resume, piece/file priorities, uploads and deadlines.
- [libtorrent 2.0 reference: settings](https://libtorrent.org/reference-Settings.html)
  — `upload_rate_limit` zero semantics, socket buffer defaults, connection limits,
  timeouts and request queues.
- [libtorrent tuning reference](https://libtorrent.org/tuning-ref.html)
  — peer-count, connection and socket-buffer tradeoffs; explicitly says upload
  limit zero is unlimited.
- [libtorrent reference: remove torrent](https://libtorrent.org/single-page-ref.html#remove_torrent)
  — asynchronous removal and optional whole-torrent `delete_files` behavior.
- [BEP 3: The BitTorrent Protocol Specification](https://www.bittorrent.org/beps/bep_0003.html)
  — standard piece request/cancel/data messages and usual 16 KiB blocks.
- [BEP 6: Fast Extension](https://www.bittorrent.org/beps/bep_0006.html)
  — chokes, rejects, allowed-fast and upload interaction; no deletion semantics.
- [VideoLAN VLC source: scene video filter](https://code.videolan.org/videolan/vlc/-/blob/02a7b108f6aec2a266026c40be55ec3104d846a7/modules/video_filter/scene.c)
  — primary implementation evidence for frame-to-image scene capture.
- [Qt 6 QProcess](https://doc.qt.io/qt-6/qprocess.html) and
  [Qt 6 QTimer](https://doc.qt.io/qt-6/qtimer.html) — async child-process lifecycle,
  output handling, and bounded timeout control. Qt warns synchronous waits on
  the GUI thread can freeze the interface.
