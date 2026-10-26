# Integration contracts

C++20, Qt 6.8.3, libtorrent 2.0.11. The GUI thread owns QML, controller, SQLite,
external-player processes and optional Qt network/process integrations. A worker
QThread owns all torrent handles, libtorrent alert consumption and loopback HTTP.
Commands cross threads through queued invocations; snapshots are copied values.

## Engine and storage

`TorrentEngine` implements `ByteSource`. Slots: add(input), reopen(id,input),
select(id,index), pause(id,bool), remove(id,deleteData), purge(id), restore(),
save(), stop(), setCacheLimitBytes(bytes), setNetworkLimits(peers,uploadKiB,
downloadKiB=0), setDiscovery(dht,lsd,portMapping), setStreamAheadBytes(bytes),
setStorageRoot(base), adoptStorageRoots(bases). `storageRoot()` returns the
canonical managed `<base>/torrents` root and must be read on the worker thread.

Snapshots include id/name/state/progress/rate/uploadRate/uploaded/peers,
downloaded/total/buffer/error/files, selected/input/magnet/savePath,
selectedName/selectedSize and the configured network limits. File entries expose
index/name/size/playable. IDs are UUIDs; stream IDs are opaque `id/index` values.
The canonical magnet includes trackers and is cached after validated metadata.

`streamReady(id,streamId)` follows a snapshot exposing the selected file.
`sessionAdded(id,input)` supports matching a queued saved selection. `reopen`
retains the saved UUID, uses owned retained metadata/data when present and can
recreate purged media under that same identity. Duplicate/in-flight removal is
rejected. Active sessions restore their original storage roots.

`removalFinished(id,deleteData,success)` acknowledges remove/purge completion.
The controller deletes library/artwork only after a successful purge. Owned
markers, direct-child paths, and link/reparse-point checks protect other files.
Known roots are retained without an arbitrary count cap. New folder settings
are persisted only after worker acceptance; live sessions are not moved.

Network limits are clamped 20..1000 peers and 0..102400 KiB/s per direction;
0 means unlimited. New defaults are 200peers, 64 KiB/s upload, unlimited download,
DHT/LSD on and UPnP/NAT-PMP off. Existing stored preferences remain respected.
Read-ahead is 8 MiB..8 GiB, additionally capped 4096 pieces. It bounds network
priority, not total disk storage. Requested verified disk reads cover only the
HTTP/startup demand (up to 2 MiB per scheduling request), not all future deadlines.
The verified RAM cache remains 64 MiB. Quota/free-space pressure pauses/admission
rejects data rather than deleting active pieces. No exact playback time or
five-minute rolling disk eviction is inferred from byte offsets.

Resume state and magnet fallback are atomic. Original .torrent metadata is
retained when available. v2 sparse-Merkle state is not serialized through the
unsafe torrent-writer path. Session teardown is deferred beyond the current
session-owned alert batch. `stopped()` completes owned teardown.

## HTTP

`HttpServer(ByteSource*)`: start(), stop(), url(streamId). Loopback-only random
port, unpredictable token, bounded range buffering and nonblocking verified
reads. It exposes no filesystem endpoint. Tests exercise full/range/seek,
authentication, malformed requests, slow/pending reads and torrent integration.

## Controller and library

QML context `app` is `ApplicationController`. Header properties/invokables are
the authoritative contract; qml_smoke's MockApp must stay synchronized.

`sessions` contains active engine rows. `library` merges those with persisted
history; `active` and `available` have distinct meanings. `available` means the
selected file exists with nonzero bytes under its saved root, not that it is
fully verified or offline-playable. Missing payload is dimmed. History retains
magnet/input, UUID, selected index, title/file path and artwork until explicit
entry deletion; there is no silent 100/300-entry pruning.

Relaunch and queued replay use reopen(id,input) with a saved selection.
Pending reconnects are deduplicated and expire after 90 seconds with a request
generation guard. Active playback is explicit; adding input does not open a
player. VLC, mpv and MPC-HC/BE use dedicated-instance argument lists. Process
exit pauses the torrent; optional further seeding requires the Seed action and
is time-bounded. Default seed duration is 0.

Settings persist in transactional SQLite JSON. Corrupt stored values prevent
writes rather than replacing the damaged database. New keys cover playerPath,
reducedMotion, generatePreviews, downloadLimitKiB, discovery controls,
searchFeed and searchKeyProtected. Feed API keys are removed from persisted URLs
and encrypted with Windows-account DPAPI. Blank API-key input preserves the
existing key; no secret is prefilled in QML. Reduced motion defaults to the
Windows client-area animation preference until explicitly set.

`--diagnostic-data-dir` isolates validation/benchmark state and disables startup
network checks; it is used by tooling, not a settings migration mechanism.

## Artwork

`ownedThumbnailPath(data,id)` hashes IDs before constructing PNG names.
Legacy UUID artwork is migrated inside the owned thumbnail folder.
`writeArtworkImage` uses QSaveFile; generated art never overwrites custom/frame
art just because a title changes. Custom input is limited to 20 MiB, 4096x4096
and decoded at at most 512px. Output keeps its aspect ratio.

Optional installed-VLC capture uses the existing tokenized HTTP endpoint,
one owned decoder at a time and at most 16 queued jobs. Metadata arrival can
select a video for preview preparation without launching a player; paused
sessions and saved selections are respected. Capture samples 30–90 seconds in,
rejects blank frames, and falls back to the opening if seeking fails. Each
attempt has a 12-second deadline; one buffered retry runs after 30 seconds.
Output is dimension/size validated before atomic replacement. URLs/tokens are
transient; deletion cancels pending captures. Audio/unsupported/failed captures
keep local generated artwork. `thumbnailKind` is generated/frame/custom.

## Optional integrations and QML

SearchService, UpdateService and NetworkAssistant have detailed contracts,
limits, licensing/source references and verification in INTEGRATIONS.md.
Startup always checks stable updates on normal launches; a newer compatible
installer is downloaded once per version/run or reused after digest validation.
Installation is explicit and blocked while active sessions/players exist.
Cloudflare startup only reads status; connect/disconnect/mode require actions.

Pages are Library, Queue, Search, Settings. MediaCover starts circular and
expands on image readiness, with reduced-motion behavior. Saved covers and
results are virtualized; image decoding is bounded. HelpDot works on hover and
keyboard focus; controls preserve accessible names and visible focus.
Stable test-facing names include navLibrary/navQueue/navSearch/navSettings,
libraryEntries/libraryRepeater, sessionCard_<id>,fileChoice_<id>,libraryCard_<id>.

## Verification and ownership

CTest covers HTTP, torrent rules, app storage/art, local swarm integration,
recovery, lifecycle, services, controller and QML. The optional installed-VLC
preview target exercises real decoding through HTTP. Native x64/ARM64 packages
remain architecture-specific; old preview artifacts must not be relabeled v1.
