# BitFly engineering handoff

**Snapshot date:** 2026-10-23
**Repository:** <https://github.com/aryan-astra/BitFly-torrent-stream>
**Default branch:** `main`
**Product version:** `0.1.0` (published only as prerelease `v0.1.0-preview.1`)
**Preview evidence base:** `2ea1197c2518e7db2465598891b5d97b5da91984`
**Documentation tip before this file:** `6770f3406cf564e13052921d6285198b45d1b332`
**Local checkout used for validation:** `X:\Projects\Personal\BitFly`
**Audience:** an engineering agent or maintainer taking over BitFly without the prior conversation.

This document is self-contained technical context, not a release announcement. Trust the
source, tests, workflows, and linked evidence over any summary here. If this document
and the code disagree, the code wins; update this document in the same change that
resolves the disagreement.

---

## A. Purpose and scope

BitFly is a native Windows desktop application that streams BitTorrent media into the
user's separately installed VLC Media Player. The user supplies a magnet link or a
`.torrent` file, selects a playable file, and VLC plays a local HTTP URL. A worker thread
owned by BitFly runs libtorrent, prioritizes the pieces demanded by VLC's HTTP byte
ranges, verifies pieces, and serves only verified bytes.

The project is deliberately narrow:

- It is a streaming bridge and session manager, not a torrent client with a built-in
  player.
- It does not search for, index, recommend, or host content.
- It does not bypass DRM, paywalls, or access controls.
- It has no accounts, telemetry, cloud backend, or automatic binary update.
- It targets Windows only, using Qt 6.8.3 and the native Windows VLC build.

The current deliverable is a **development preview**. The source builds, tests, packages,
and publishes prerelease binaries, but the stable-release definition of done in
[STATUS.md](STATUS.md) is not met. The next owner should preserve that distinction.

## B. Repository, ownership, and provenance

- GitHub repository: <https://github.com/aryan-astra/BitFly-torrent-stream>
- Default branch: `main`
- License: MIT (`LICENSE`)
- Owner and Git author: Aryan Shukla `<ryanxastra@gmail.com>`
- GPG signing key: `A17E14BFC29390B8A296B3CF95FCCAC560D38AD1`
- `commit.gpgsign` is `true` in the local checkout.
- No AI or co-author trailers are used.
- Annotated tag `v0.1.0-preview.1` points to `9c27337` and is GPG-signed.
- GitHub release ID `422010008` is a prerelease published `2026-10-23T15:32:38Z`.
- No stable release or Authenticode certificate exists.

At the preview evidence snapshot, `main` contained 15 commits:

| Commit | Subject |
| --- | --- |
| `31d5c14` | `docs: establish native streaming architecture and contracts` |
| `8e1334c` | `feat: implement native torrent streaming application` |
| `0ef4cc6` | `fix: harden streaming lifecycle and preserve live interface state` |
| `5c8bfe6` | `fix(ci): harden native packaging and analysis gates` |
| `499cf97` | `fix(stream): unblock magnet startup and harden session recovery` |
| `c3e95f5` | `fix(http): reject rebinding hosts and absolute request targets` |
| `de825fe` | `perf(ui): render the static interface with the software scene graph` |
| `4372645` | `build: support MSVC and modern CMake policy defaults` |
| `1267b74` | `fix(bench): sample idle CPU only after the first frame` |
| `7e3175c` | `docs: record verified streaming, performance and test evidence` |
| `f7b3706` | `fix(package): reject wrong-architecture runtime DLLs` |
| `9c27337` | `docs: record final CI and package validation` |
| `9003a06` | `ci: checkout the tag before publishing a release` |
| `486d419` | `docs: record preview release assets and verification` |
| `2ea1197` | `docs: record release package launch smoke` |

All 15 were GPG-signed locally and GitHub reported `verified=true, reason=valid` for
each. The documentation pass after that snapshot adds signed commits for the README,
changelog, reconciled docs, and this handoff; check their GitHub signature status after
pushing.

The tag is immutable history. Do not move, delete, or recreate it. Do not rewrite,
rebase, or force-push `main`. Preserve the owner's author identity and signing key.

## C. State at handoff

| Area | State | Authoritative evidence |
| --- | --- | --- |
| Source tree | Complete, builds locally and in CI | Local build and CI runs below |
| Local x64 build | Release build succeeds with MinGW 13.1 | `build/BitFly.exe`, 11,417,474 bytes |
| Local unit/protocol tests | 4/4 non-integration suites in 2.46 s | [STATUS.md](STATUS.md) |
| Local full VLC integration | 6/6 in 144,725 ms; all four modes decoded 91,244 bytes | [STATUS.md](STATUS.md) |
| CI quality | Passing | Runs `39981098649`, `39973094582` |
| CI x64 | Passing, 5/5 tests, 97 PE files | Runs `39981098649`, `39973094582` |
| CI ARM64 | Passing natively, 5/5 tests, 93 PE files | Runs `39981098649`, `39973094582` |
| Packaged launch smoke | Passing for both architectures | CI logs and [STATUS.md](STATUS.md) |
| Release | Prerelease `v0.1.0-preview.1`, six assets | Release `422010008` |
| Automatic release publish | Fixed in `9003a06`; not yet exercised by a new tag | [RELEASE.md](RELEASE.md) |
| Stable release | Not published; gates unmet | [STATUS.md](STATUS.md) |
| Commit/tag signing | GPG-signed and GitHub-verified | Git and GitHub |
| Binary signing | None; Windows reports `NotSigned` | Authenticode inspection |
| Documentation | README and docs reconciled in this pass | README, CHANGELOG, docs |

The working tree was clean at `2ea1197` before the documentation pass. `build/` and
`dist/` are gitignored and contain local artifacts that must not be committed.

## D. Product behavior and supported targets

### User flow

1. BitFly starts with an empty Stream page and detects `vlc.exe` from the registry,
   standard Program Files locations, or a previously selected path.
2. The user pastes a magnet link or opens a local `.torrent` file.
3. After metadata arrives, BitFly lists playable and non-playable files; the user selects
   a media file.
4. Playback is requested. BitFly starts the stream HTTP server, asks libtorrent for the
   pieces needed by VLC's first ranges, and waits until a verified range can be served.
5. BitFly launches a dedicated VLC process with
   `--no-one-instance --play-and-exit <tokenized loopback URL>`.
6. VLC issues HTTP range requests. BitFly reprioritizes piece deadlines from that demand.
7. When the owned VLC process exits, BitFly offers **Keep**, **Delete**, or **Seed**.
   Queue entries advance when the process exits successfully.

### Session outcomes

- **Keep:** stop network activity for that session and retain its managed data.
- **Delete:** after confirmation, remove that session's managed data. The app refuses
  removal while its VLC process is still running.
- **Seed:** keep the torrent active for the configured number of minutes (default 15,
  maximum 1440) and then pause it. Seeding is bounded and user-controlled.

History records successful VLC process creation, not watched position or completion.
There is no exact playback-position tracking, resume-to-timestamp, subtitle discovery,
watched-episode detection, or playback analytics.

### Intended targets

- Windows 10 version 1809 or later, x64.
- Windows 11, x64.
- Windows 11 on ARM64.

Windows 11 x64 is the validated development host. Windows 10 hardware and physical
Windows 11 ARM64 VLC playback remain unvalidated. The supported-target statement in
`docs/ARCHITECTURE.md` is a Qt/toolchain statement, not proof of hardware validation.

### Explicit non-goals

- No torrent search, content catalog, recommendations, or hosted index.
- No embedded media player, bundled VLC, or codec pack.
- No DRM circumvention or access-control bypass.
- No anonymous networking promise; BitTorrent exposes the user's address to peers and
  trackers.
- No account, telemetry, analytics, or cloud synchronization.
- No silent downloads or automatic installation of updates.
- No inference of VLC playback position.

## E. Architecture

```text
QML UI (GUI thread)
    │ copied QVariant snapshots / queued invocations
    ▼
ApplicationController (GUI thread)
    ├─ SQLite SessionStore
    ├─ QNetworkAccessManager update check
    └─ owned QProcess VLC instances
             │
             │ queued calls
             ▼
QThread "BitFly engine and HTTP"
    ├─ TorrentEngine (libtorrent 2.0.11, scheduling, resume, cache quota)
    └─ HttpServer (loopback, tokenized byte ranges)
             │
             ▼
        libtorrent network/disk threads
```

Key rules:

- QML, `ApplicationController`, SQLite settings/queue/history, update checks, and VLC
  process ownership live on the GUI thread.
- One worker thread owns libtorrent, the alert timer, the verified-piece cache, and the
  HTTP server. All calls into that thread are queued.
- The UI receives copied `QVariantList`/`QVariantMap` snapshots. QML never reads live
  engine objects.
- `ByteSource` is the boundary between HTTP and torrent code. It exposes opaque stream
  IDs, file length/MIME description, demand notification, and nonblocking verified reads.
  HTTP never receives a filesystem path.
- HTTP never blocks waiting for a piece. If bytes are not verified yet, the client waits
  until a poll or demand update can serve them, within timeout limits.
- The static QML interface uses Qt's **software scene graph** (`QSGRendererInterface::Software`).
  This reduces renderer memory on the target utility UI. It is a deliberate performance
  decision, not a fallback.
- VLC is external. The absolute `vlc.exe` path and argument list are passed to `QProcess`;
  no shell is invoked. Standard output and error are discarded.
- Shutdown is asynchronous: stop HTTP, stop libtorrent, persist, terminate owned VLC
  processes, wait with a bounded timeout.

The architecture decision record is [ARCHITECTURE.md](ARCHITECTURE.md); interface
contracts are [CONTRACTS.md](CONTRACTS.md); visual behavior is [DESIGN.md](DESIGN.md).

## F. Source and build map

| Path | Responsibility |
| --- | --- |
| `src/main.cpp` | Application startup, software renderer, QML loading, screenshot/startup-probe modes |
| `src/app/ApplicationController.{h,cpp}` | GUI-thread controller, VLC processes, queue/history, settings, update check, session actions |
| `src/app/SessionStore.{h,cpp}` | SQLite connection, schema version, transactional JSON key/value persistence |
| `src/app/Platform.{h,cpp}` | VLC discovery/validation and stable semantic-version parsing |
| `src/core/ByteSource.h` | Pure interface between HTTP and the torrent engine |
| `src/torrent/TorrentEngine.{h,cpp}` | libtorrent session, add/restore, metadata validation, scheduling, verified cache, resume, quota, deletion |
| `src/torrent/StreamingRules.{h,cpp}` | Deterministic deadline/path calculations with unit tests |
| `src/stream/HttpServer.{h,cpp}` | Loopback HTTP/1.0/1.1 server, token route, byte ranges, hardening, client timeouts |
| `qml/Main.qml` | Stream, Queue, History, Settings, Diagnostics pages and navigation |
| `qml/SessionCard.qml` | Per-session state, progress, rate, peers, file selection, actions |
| `qml/Theme.qml` | Colors, typography, radii, singleton design tokens |
| `qml/NavItem.qml`, `NavIcon.qml`, `QuietButton.qml`, `LinkButton.qml`, `SettingSpinBox.qml` | Reusable controls and drawn navigation icons |
| `tests/http_tests.cpp` | HTTP ranges, parsing, host/target rejection, disconnects, timeouts |
| `tests/torrent_tests.cpp` | Scheduler/path rules |
| `tests/app_tests.cpp` | Version parsing and SQLite behavior |
| `tests/integration_tests.cpp` | Local tracker/seeder, v1/v2/hybrid torrents, magnets, ranges, resume, optional VLC decode |
| `tests/qml_smoke.cpp` | Headless QML load, navigation, delegate identity, selection, warning gate |
| `scripts/package.ps1` | windeployqt, runtime deployment, PE validation, license/SBOM staging, ZIP, Inno Setup |
| `packaging/BitFly.iss` | Per-user installer definition |
| `packaging/BitFly.rc`, `BitFly.ico` | Windows resources and application icon |
| `.github/workflows/windows.yml` | Quality dependency, x64/ARM64 build, test, package, smoke, release |
| `.github/workflows/source-checks.yml` | Formatting, Cppcheck, license presence, diff checks |
| `docs/` | Status, testing, build, release, architecture, contracts, design, operations, this handoff |

CMake targets:

- `BitFly` — GUI executable.
- `bitfly_stream` — HTTP server static library.
- `bitfly_torrent` — torrent engine static library.
- `bitfly_rules` — pure scheduling/path rules.
- `bitfly_platform` — VLC discovery, version parsing, SQLite store.
- `http_tests`, `torrent_tests`, `app_tests`, `integration_tests`, `qml_smoke`.
- `bitfly_torrent` links libtorrent from either `BITFLY_LIBTORRENT_SOURCE` or the
  installed `LibtorrentRasterbar` package.

## G. Build and toolchain

### Local development environment

The validated local checkout is `X:\Projects\Personal\BitFly`. Dependencies are
repository-local and gitignored under `.tools/`:

- Qt: `.tools/Qt/6.8.3/mingw_64`
- MinGW: `.tools/Qt/Tools/mingw1310_64`
- libtorrent source: `.tools/libtorrent` (2.0.11)
- Boost headers: `.tools/boost_1_86_0`
- OpenSSL: `.tools/openssl/ucrt64`
- Ninja: `C:/mingw64/bin/ninja.exe`
- CMake build directory: `build/` (Ninja, Release)
- Inno Setup: `.tools/InnoSetup`

The build cache currently records:

```text
CMAKE_BUILD_TYPE=Release
CMAKE_GENERATOR=Ninja
CMAKE_MAKE_PROGRAM=C:/mingw64/bin/ninja.exe
CMAKE_CXX_COMPILER=X:/Projects/Personal/BitFly/.tools/Qt/Tools/mingw1310_64/bin/g++.exe
CMAKE_PREFIX_PATH=X:/Projects/Personal/BitFly/.tools/Qt/6.8.3/mingw_64
BITFLY_LIBTORRENT_SOURCE=X:/Projects/Personal/BitFly/.tools/libtorrent
BITFLY_BOOST_ROOT=X:/Projects/Personal/BitFly/.tools/boost_1_86_0
OPENSSL_ROOT_DIR=X:/Projects/Personal/BitFly/.tools/openssl/ucrt64
```

Reconfigure command:

```powershell
$env:PATH = "X:\Projects\Personal\BitFly\.tools\Qt\Tools\mingw1310_64\bin;" +
            "X:\Projects\Personal\BitFly\.tools\Qt\6.8.3\mingw_64\bin;" + $env:PATH
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH=X:/Projects/Personal/BitFly/.tools/Qt/6.8.3/mingw_64 `
  -DBITFLY_LIBTORRENT_SOURCE=X:/Projects/Personal/BitFly/.tools/libtorrent `
  -DBITFLY_BOOST_ROOT=X:/Projects/Personal/BitFly/.tools/boost_1_86_0 `
  -DOPENSSL_ROOT_DIR=X:/Projects/Personal/BitFly/.tools/openssl/ucrt64
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
.\build\BitFly.exe
```

`docs/BUILD.md` contains the portable `C:/Qt` and `C:/deps` form. Do not mix an
arbitrary machine MinGW with the Qt MinGW kit; compiler, C++ runtime, Qt, libtorrent, and
OpenSSL must match the target architecture. The app does not ship a Python or JavaScript
runtime.

### CI toolchain

- Qt 6.8.3 installed with `aqtinstall==3.3.0`.
- x64: `win64_msvc2022_64` on `windows-2022`, Visual Studio 17 2022, v143.
- ARM64: `win64_msvc2022_arm64` on `windows-11-arm`, Visual Studio 18 2026, v143.
- vcpkg pinned at `84bab45d415d22042bd0b9081aea57f362da3f35` (2025.12.12 baseline),
  resolving libtorrent 2.0.11.
- Inno Setup 6.4.3, downloaded from the official release and Authenticode-verified
  before installation.
- Actions are pinned to full commit SHAs. Keep that property when editing workflows.

Supported dependency pins: Qt 6.8.3, libtorrent 2.0.11 (source commit
`9d7443f467147d1784fb7516d2a882db1abb5a8b`), Boost 1.86.0, OpenSSL 3.x selected by
the toolchain. `THIRD_PARTY_NOTICES.md` records licenses and sources.

## H. Testing and validation

CTest registers five suites:

```powershell
ctest --test-dir build --output-on-failure
```

| Test | Coverage |
| --- | --- |
| `http` | Range parsing, token route, host/absolute-target rejection, duplicate headers, content-length rules, disconnect/timeout behavior |
| `torrent` | Deadline/scheduler calculations and path rules |
| `app` | Stable-version parsing, SQLite transactions, future-schema refusal |
| `integration` | Real libtorrent v1/v2/hybrid/magnet sessions against a local throttled seeder, ranges, resume, optional VLC decode |
| `qml_smoke` | Actual `Main.qml` load, navigation, stable delegate identity, selection preservation, QML warning gate, frame capture |

Local evidence on the final engineering tree:

- The four non-integration tests passed 4/4 in 2.46 s.
- The full integration suite passed 6/6 in 144,725 ms with external VLC 3.0.
- v1 torrent, v1 magnet, v2 magnet, and hybrid modes each verified
  beginning/middle/end HTTP ranges and restart/resume.
- Each mode decoded one second through a local-file baseline and the tokenized HTTP URL;
  all eight decodes produced 91,244 bytes.
- Downloaded-at-ready / seek verification: v1 4,800,512 bytes / 1,899 ms; v1 magnet
  4,145,152 / 951 ms; v2 3,244,032 / 3,071 ms; hybrid 3,260,416 / 6,015 ms.
- HTTP disconnect polling was hardened against a one-poll teardown race and passed 30
  consecutive runs.
- `clang-format` 18.1.8, `qmllint`, and Cppcheck 2.18 exhaustive warning/portability
  analysis passed.

Targeted integration run:

```powershell
$env:BITFLY_TEST_VLC = 'C:/Program Files/VideoLAN/VLC/vlc.exe'
./build/integration_tests.exe realLibtorrentStreamsRangesAndRestores:v1magnet `
  -o build/integration-v1magnet.txt,txt
```

The integration suite generates an 8 MiB PCM WAV and its own torrents, starts a separate
loopback seeder and tracker, and needs loopback TCP. A VLC process that merely starts is
not playback evidence. Only a successful decode with matching output counts.

Packaged-app smoke test:

```powershell
./BitFly.exe --startup-probe "$env:TEMP/bitfly-startup-probe.json" --quit-after 5000
```

Success means exit code 0 within 30 seconds and a probe JSON with a non-negative
`firstFrameMs`. CI performs this after packaging for both architectures. It proves
window creation and first-frame rendering, not streaming or VLC decode.

Do not delete, skip, weaken, or mock away an existing test to make a change pass. Add a
focused regression test for every behavioral fix. Keep the local integration suite
runnable without a public swarm.

## I. CI workflows and exact runs

### `Source quality`

Runs on Ubuntu 24.04 through `windows.yml`'s reusable `quality` job or directly:

- Installs `clang-format==18.1.8`.
- Runs `clang-format --dry-run --Werror` on every tracked `*.cpp`/`*.h`.
- Runs Cppcheck with warning/portability analysis, exhaustive level when supported.
- Records performance suggestions as an artifact (not a gate).
- Checks required license files and `git diff --check`.

### `Windows CI and release`

- Pull requests and pushes to `main`: quality plus x64/ARM64 build jobs.
- Pushes with a `v*` tag: the same jobs plus the release job.
- `workflow_dispatch` can repeat the workflow against a version tag.
- Each architecture job installs pinned Qt/vcpkg/Inno Setup, builds Release, runs all
  five CTest suites, installs to a clean stage, runs `scripts/package.ps1`, smoke-tests
  the staged app, and uploads `dist/*` as `BitFly-x64` or `BitFly-arm64` (14-day
  retention).
- The release job requires `quality` and `build`, checks out the tag with full history,
  downloads both artifact sets from the same run, requires at least six files, and creates
  the GitHub Release with `gh release create --verify-tag --generate-notes`. A hyphen in
  the tag adds `--prerelease`.

### Runs that matter

| Run | Ref | Conclusion | Details |
| --- | --- | --- | --- |
| [39973094582](https://github.com/aryan-astra/BitFly-torrent-stream/actions/runs/39973094582) | `f7b3706` | Overall success; release skipped | Quality/x64/ARM64 passed. x64 5/5 CTest including 147.23 s integration, 97 PE files, 3,130 ms first frame. ARM64 5/5, 93 PE files, 2,620 ms. |
| [39977378333](https://github.com/aryan-astra/BitFly-torrent-stream/actions/runs/39977378333) | Tag `v0.1.0-preview.1` (`9c27337`) | Overall failure; build jobs passed | Quality/x64/ARM64 passed. x64/ARM64 5/5 CTest, 97/93 PE files, 2,061/2,295 ms first frames. Release publish failed for missing checkout. |
| [39981098649](https://github.com/aryan-astra/BitFly-torrent-stream/actions/runs/39981098649) | `2ea1197` | Overall success; release skipped | Quality/x64/ARM64 passed, 5/5 each, 97/93 PE files. Tag-only release job correctly skipped. |

The first two listed runs are the evidence behind the published assets; the third is the
latest pre-documentation `main` validation. All 15 pre-handoff commits carry verified
GitHub signatures.

## J. Packaging and published artifacts

`scripts/package.ps1` is the only supported packaging path. It:

1. Resolves the version from `CMakeLists.txt` when not supplied.
2. Runs `windeployqt` with release deployment and excludes unused SQL drivers/QML
   tooling.
3. Copies runtime DLLs from vcpkg or explicit `-RuntimeDir` directories.
4. Validates every copied DLL's PE machine type (`0x8664` for x64, `0xAA64` for ARM64).
5. Requires OpenSSL `ssl`/`crypto` DLLs and, for vcpkg builds, libtorrent.
6. Copies the architecture-matched MSVC `Microsoft.VC143.CRT` files for MSVC builds.
7. Excludes the x64 `vcruntime140_1.dll` erroneously present in Microsoft's ARM64
   redistributable, then proves no staged ARM64 PE imports it.
8. Stages Qt SPDX/SBOM records, project licenses, vcpkg licenses, and a provenance note.
9. Produces the portable ZIP, SHA-256 manifest, and Inno Setup per-user installer.

Published release assets (verified by download and hash):

| Architecture | Asset | Bytes | SHA-256 |
| --- | --- | ---: | --- |
| x64 | `BitFly-0.1.0-windows-x64.zip` | 44,397,803 | `5194D64575ADC52702350B32336540561DC4D90CEF6FBC1831E0913FD76AB99F` |
| x64 | `BitFly-0.1.0-windows-x64-setup.exe` | 29,683,549 | `E79BC566436DBA4CC2F3E62CFF8628582536A3A5E30AC991DAB4F4DCA32768BB` |
| ARM64 | `BitFly-0.1.0-windows-arm64.zip` | 30,653,290 | `6AD0AC5E3A0D9E504751181C63451927445CF41C8F3EA27B1A6727CAFF1223AC` |
| ARM64 | `BitFly-0.1.0-windows-arm64-setup.exe` | 20,610,258 | `07D344B2D85371BF9BFCA18C06CF32DAA26C769E22740F9783C8792958485872` |
| x64 | `BitFly-0.1.0-windows-x64-SHA256SUMS.txt` | 198 | Asset digest recorded by GitHub |
| ARM64 | `BitFly-0.1.0-windows-arm64-SHA256SUMS.txt` | 202 | Asset digest recorded by GitHub |

Release: <https://github.com/aryan-astra/BitFly-torrent-stream/releases/tag/v0.1.0-preview.1>

The installer is per-user (`PrivilegesRequired=lowest`), installs under
`%LOCALAPPDATA%\Programs\BitFly`, creates optional desktop/start-menu shortcuts, and
uninstalls without deleting user data. The portable ZIP is self-contained apart from VLC.

A separate local MinGW x64 package exists only in `dist/` and was not uploaded:

- ZIP: 39,327,259 bytes, SHA-256
  `071DA6D24239E21EE93A633636E0EAD38C9197650C86916A7D7AB0EF4B2ED62A`
- Setup: 26,993,968 bytes, SHA-256
  `EDBF464558A6D5867777F4D234FB8B6DE94F5233890F20384B4C3B767F22B1F1`
- 84 validated x64 PE files.
- Staged launch smoke rendered in 1,747 ms and exited 0.

Never upload those local artifacts to a release. Published binaries must come from the
archived CI run, and their hashes must match the manifest.

## K. Release history and recovery

1. `9c27337` was committed and annotated tag `v0.1.0-preview.1` was created with an
   existing GPG key.
2. Pushing the tag started run `39977378333`. Quality, x64, and ARM64 jobs passed.
3. The release job failed because it had no repository checkout, so
   `gh release create --verify-tag` could not verify the tag.
4. The six already-built artifacts from that run were uploaded through the GitHub API,
   creating prerelease `422010008`.
5. The assets were downloaded again. All four binaries matched their SHA-256 files and
   GitHub's asset digests. The x64 portable package launched and exited 0.
6. `9003a06` added an explicit tag checkout to the release job.
7. No new tag has been created, so the corrected automatic publish path remains
   unexercised. Do not move the existing tag to test it.
8. `main` advanced through release-evidence and documentation commits; the tag still
   points at `9c27337`, three commits behind the preview evidence snapshot.

The manual API upload is a documented recovery, not the normal process. A future release
should use the workflow. The release job creates a new release and does not update an
existing one, so a retry against an existing release will fail until the conflict is
resolved deliberately. `--generate-notes` means future release notes are generated from
history, unlike the hand-written preview body.

Before any future tag: re-read [RELEASE.md](RELEASE.md), run all local tests, verify both
CI architectures, create a signed annotated tag, and watch the release job. Do not create
a stable release until the gates in section P are closed.

## L. Security and privacy model

### Assets and boundaries

- Untrusted torrent metadata and peer/tracker input.
- Loopback HTTP requests from local processes and VLC.
- Managed cache roots and filesystem paths.
- User-selected `vlc.exe`.
- Local SQLite database and resume files.
- GitHub update responses.

### HTTP hardening

The server listens only on `QHostAddress::LocalHost` at a random port. It generates 32
random bytes, encodes them as a base64url token, and exposes only
`/v1/<token>/<base64url-stream-id>`. Additional properties:

- Maximum 32 local clients; non-loopback peers are aborted.
- 16 KiB maximum request headers; 15-second header timeout; 120-second body-stall
  timeout; 25 ms polling.
- GET/HEAD and HTTP/1.0/1.1 only; required `Host` for HTTP/1.1.
- Host must be `localhost` or `127.0.0.1`, optional matching port; absolute targets,
  query strings, fragments, authorities, and malformed paths are rejected.
- Duplicate headers, non-zero Content-Length, Transfer-Encoding, control characters,
  and oversized requests are rejected.
- Only single `bytes=` ranges are parsed; suffix ranges supported; end clamped to size.
- Stream IDs are opaque; the server never interprets them as filesystem paths.
- Reads come only from `ByteSource::readVerified`; unverified or unavailable bytes are
  never served or synthesized.
- Response buffers and demand windows are bounded (64 KiB reads, 1 MiB demand window,
  128 KiB socket queue).

### Torrent and filesystem hardening

- Cache root rejects symbolic links and is created under the per-user data directory.
- Per-session directories require a matching `.bitfly-session` ownership marker before
  restore/removal.
- Torrent metadata limits: 32 MiB metadata, 10,000 files, 2,000,000 pieces, 16 MiB piece
  size, 256 sessions.
- Invalid, duplicate (case-folded), escaping, or symlink file paths are rejected.
- Disk admission checks quota and free space; active media is not evicted to fake space.
- Resume files are written atomically and size-bounded.
- Deletion is explicit, confirmed, and restricted to the managed session directory.
  Retained data is marked with `.retained` and is not silently restored.

### VLC and updates

- VLC must be an absolute path to a file named `vlc.exe`; discovery is registry,
  Program Files, and saved-path based.
- Each session uses a separate VLC process with `--no-one-instance --play-and-exit`.
- No shell interpolation.
- The update checker queries the fixed official GitHub `/releases/latest` endpoint with
  a 10-second timeout, a 256 KiB response cap, same-origin redirects, and stable-semver
  validation. It only displays a message/link; it never downloads or executes a binary.
- Because the endpoint excludes prereleases, the current preview reports no stable
  update.

### Privacy and binary trust

No telemetry, accounts, or cloud backend. SQLite stores local settings, queue, and up to
100 recent launch titles; resume state can contain torrent names, tracker URLs (including
private passkeys), peers, and paths. Treat the data directory as private and never attach
raw databases or resume files to public issues.

The binaries are **not Authenticode-signed**. Windows SmartScreen or publisher warnings
are expected. Users should verify SHA-256 values. There is no stable security-maintenance
commitment; report vulnerabilities through GitHub's private security advisory flow when
available or the maintainer's GitHub profile otherwise.

## M. Data and storage

Default per-user root:

```text
%LOCALAPPDATA%\BitFly\BitFly\
  sessions.sqlite
  cache\
    torrents\
      <session-uuid>\
        .bitfly-session
        .fastresume
        .retained            (only for retained data)
        <torrent files>
```

The portable build uses the same per-user location; it is not portable-data. A custom
cache root selected in Settings applies after restart. Existing data is never moved or
deleted by changing the setting.

- SQLite schema version: 1, WAL mode, `settings(key TEXT PRIMARY KEY, value TEXT)` with
  compact JSON values. A future schema is refused rather than downgraded.
- Persisted keys: `cachePath`, `vlcPath`, `cacheLimitGiB`, `seedMinutes`, `advanced`,
  `history`, `queue`, `lastUpdateCheck`.
- Queue maximum: 200 entries. History maximum: 100 entries.
- Cache quota: default 20 GiB, 1–1024 GiB. Separate in-memory verified-piece cache:
  64 MiB.
- Seed duration: default 15 minutes, 0–1440. Zero disables temporary seeding.
- Active session maximum: 256.
- Resume files: `.fastresume`, atomically replaced; absence or rejection falls back to
  metadata and restarts the session.
- Uninstall preserves user data. Manual removal of active session files is unsupported.

## N. Performance

Three warm x64 empty-session runs on Windows build 26300, Intel family 6 model 154, with
about 1.25–1.31 GiB available system memory:

| Run | First frame | Peak working set | Peak private committed | Idle CPU |
| --- | ---: | ---: | ---: | ---: |
| 1 | 958 ms | 135,798,784 B | 83,922,944 B | 0.000 s / 2.001 s |
| 2 | 710 ms | 136,433,664 B | 84,774,912 B | 0.015625 s / 2.004 s |
| 3 | 718 ms | 136,601,600 B | 84,320,256 B | 0.03125 s / 2.004 s |

Median first frame: 718 ms. Peak working set: about 129.5–130.3 MiB. Peak private
committed: about 80.0–80.9 MiB. Executable: 11,417,474 bytes. The previous hardware
renderer measured a 1,021 ms median and 184.7–190.5 MiB working set; the software scene
graph is the deliberate choice.

These are warm-filesystem short runs, not controlled cold-boot, active-stream, or
long-duration measurements. Raw data:
[windows-x64-software-benchmark.json](validation/windows-x64-software-benchmark.json).
The initial hardware comparison is
[windows-x64-initial-benchmark.json](validation/windows-x64-initial-benchmark.json).

Do not replace the software renderer or add GPU effects without measuring first-frame
latency, working set, private committed memory, and idle CPU on the same class of
machine. Do not claim a cold-start number from these runs.

## O. Evidence index

| Evidence | Location |
| --- | --- |
| Authoritative validation and gates | [STATUS.md](STATUS.md) |
| Test procedures and limits | [TESTING.md](TESTING.md) |
| Build instructions | [BUILD.md](BUILD.md) |
| Release process and preview recovery | [RELEASE.md](RELEASE.md) |
| Architecture decision | [ARCHITECTURE.md](ARCHITECTURE.md) |
| Interface contracts | [CONTRACTS.md](CONTRACTS.md) |
| Interface design | [DESIGN.md](DESIGN.md) |
| Privacy/troubleshooting | [OPERATIONS.md](OPERATIONS.md) |
| Software-renderer benchmark | [validation/windows-x64-software-benchmark.json](validation/windows-x64-software-benchmark.json) |
| Initial hardware benchmark | [validation/windows-x64-initial-benchmark.json](validation/windows-x64-initial-benchmark.json) |
| Authentic application screenshot | [screenshots/windows-x64.png](screenshots/windows-x64.png) |
| Preview release | <https://github.com/aryan-astra/BitFly-torrent-stream/releases/tag/v0.1.0-preview.1> |
| Final pre-tag CI | <https://github.com/aryan-astra/BitFly-torrent-stream/actions/runs/39973094582> |
| Tag CI | <https://github.com/aryan-astra/BitFly-torrent-stream/actions/runs/39977378333> |
| Latest pre-docs CI | <https://github.com/aryan-astra/BitFly-torrent-stream/actions/runs/39981098649> |

The screenshot is a direct capture of the implemented application at 1350 × 900 on the
validated x64 host, showing the empty Stream state with VLC detected. It is not a mock or
a design render. Hidden-window capture had a DPI artifact and is not evidence. Do not
replace this with a synthetic composite and call it an application screenshot.

No raw CI logs are checked in. The links above are the public source of truth. If a link
expires or permissions change, record that fact rather than inventing a result.

## P. Known gaps, risks, and release gates

Priority order for a future stable release:

| Priority | Gap | Impact | Next action |
| --- | --- | --- | --- |
| P0 | Corrected automatic publish path untested | Next tag may fail at publish again | Exercise on a new, intentional tag; do not move preview 1 |
| P0 | No physical Windows 11 ARM64 VLC playback | ARM64 is built/smoked but not playback-proven | Test on ARM64 hardware with matching VLC |
| P0 | No physical Windows 10 validation | Minimum-OS claim is not hardware-proven | Test install, launch, and playback on Windows 10 1809+ |
| P1 | Single local seeder and one PCM WAV fixture | No public-swarm or codec coverage | Test public swarms and H.264/AAC/HEVC containers |
| P1 | No sustained concurrent playback/long-run test | Resource leaks and cancellation bugs unknown | Run multi-hour, multi-session scenarios |
| P1 | No disk-full/crash fault injection | Recovery behavior unproven | Simulate quota/disk exhaustion and process kill |
| P1 | No screen-reader or full keyboard review | Accessibility unproven | NVDA/Narrator and keyboard-only pass |
| P1 | No Authenticode certificate | SmartScreen/publisher warnings | Provision signing and sign installers/executables |
| P2 | No cold-start or active-stream benchmark | Performance claims limited to warm idle | Add controlled measurements |
| P2 | No sanitizer coverage | Memory/thread defects may remain | ASan/UBSan or toolchain-equivalent run |
| P2 | No complete dependency/license audit | Compliance risk | Review shipped SPDX/SBOM against notices |
| P2 | Update checker ignores prereleases | Preview installs see no update offer | Intentional; document, do not "fix" without a policy |
| P3 | Exact playback position not tracked | No resume-to-timestamp | Out of scope unless product direction changes |
| P3 | No subtitles/search/catalog | Deliberate scope | Do not add without an explicit product decision |

Stable release definition of done (from `STATUS.md`): physical ARM64 and Windows 10
playback, sustained concurrency, fault injection, accessibility, cold-start and
active-stream performance, sanitizer coverage, license audit, and the remaining
validation gates must pass, with evidence recorded. Do not label a build "stable" because
CI is green.

## Q. Decisions and invariants

Preserve these unless the owner explicitly changes direction:

1. **External VLC only.** No embedded player, bundled VLC, or codec pack.
2. **No content discovery.** No search, catalog, index, recommendations, or DRM bypass.
3. **Verified bytes only.** HTTP must never serve unverified or synthetic data.
4. **One worker thread** owns libtorrent, HTTP, cache, and scheduling. Keep blocking I/O
   off the GUI thread.
5. **Software scene graph** for the static UI. It is a measured memory decision.
6. **Magnet startup must not set `default_dont_download`.** That flag made libtorrent
   treat a magnet as an upload-only seed before metadata arrived. It is valid only for
   paused `.torrent`/resume inputs during configuration.
7. **Cache admission, not active eviction.** Pause/admit downloads when quota or disk is
   tight; never delete active media to pretend the quota is satisfied.
8. **History means launch attempts.** Do not present it as watch history or playback
   position.
9. **ARM64 CRT exception.** Exclude the x64 `vcruntime140_1.dll` from Microsoft's ARM64
   redist and prove no staged ARM64 PE imports it. Do not weaken PE architecture checks.
10. **Owner-signed commits.** Preserve author, GPG identity, and signed history. Agents
    do not commit or push without explicit owner authorization, and never expose keys.
11. **Preview stays prerelease** until gates close. The update checker intentionally
    queries stable releases only.
12. **Do not upload local builds.** Only CI artifacts from a successful run belong on a
    release.
13. **Tags are immutable.** Never move or delete a signed published tag.
14. **Per-user data.** Default root is `%LOCALAPPDATA%\BitFly\BitFly`; portable builds do
    not relocate it.
15. **No telemetry or accounts.** Local diagnostics only.

## R. Operating rules for the next agent

Start every session with:

```powershell
git status --short --branch
git log --oneline -20
git tag -n
```

Then read `docs/STATUS.md`, `docs/TESTING.md`, and `docs/RELEASE.md` before changing
behavior or making claims.

Working rules:

- Preserve uncommitted changes. Investigate unfamiliar files before deleting or
  overwriting them.
- Make small, logical changes with focused tests. Do not rewrite working software or
  history.
- Build and run the relevant tests before saying something passes. Use the exact evidence
  vocabulary: executed, observed, inferred, planned.
- Update `docs/STATUS.md` when validation evidence changes, and distinguish historical
  runs from current status. Never turn a planned check into a passed check.
- Keep README claims tied to published assets or recorded evidence. Do not invent
  download names, sizes, hashes, screenshots, OS support, or performance numbers.
- If changing packaging, test both architectures and keep PE validation and the ARM64
  CRT exception intact.
- If changing HTTP, torrent metadata handling, cache deletion, SQLite schema, or VLC
  launch behavior, treat it as a security-sensitive change and add adversarial tests.
- If changing CI, keep actions SHA-pinned and do not weaken release gating.
- Do not create a tag or release without the owner's explicit instruction. If authorized,
  use a signed annotated tag and wait for both architecture jobs and quality to pass.
- Never print, commit, or embed tokens, signing keys, credentials, private torrent data,
  or personal paths in public artifacts.
- Do not force-push, reset published history, amend published commits, or bypass signing.
- When documentation and behavior disagree, fix the documentation in the same change.

Committing (only with owner authorization):

```powershell
git add <specific paths>
git commit -m "<type>: <summary>" -m "<why and evidence>"
git log -1 --format='%H %G? %s'
```

All commits must show `G` locally and GitHub `verified=true, reason=valid` after push.
There are no AI/co-author trailers.

## S. Recommended next steps

1. **Close stable-release gates.** Start with physical Windows 11 ARM64 VLC playback and
   Windows 10 hardware validation. Record exact OS build, VLC version, media, and observed
   results in `STATUS.md`.
2. **Exercise the corrected release job** on the next intentionally created preview tag.
   Confirm checkout, artifact download, six-asset count, prerelease flag, generated notes,
   and hash verification. Do not reuse the existing tag.
3. **Expand streaming evidence** to public swarms and common codecs (H.264/AAC, HEVC,
   multi-file, metadata-at-end containers), then long-running concurrent sessions.
4. **Fault injection:** disk full, cache quota exhaustion, killed VLC, killed app,
   corrupted resume, corrupted SQLite, and network loss. Verify no data corruption and no
   silent deletion.
5. **Accessibility:** keyboard-only traversal, focus order, screen-reader names/states,
   high contrast, and 125–200% DPI.
6. **Performance:** controlled cold-start and active-stream measurements; keep the
   software-renderer baseline.
7. **Authenticode signing** and a documented key/certificate custody process.
8. **Supply chain:** complete the license/SBOM audit and document the Qt LGPL dynamic
   linking obligations.
9. **Public polish:** keep README download links and hashes synchronized with the latest
   release. The repository description/topics should describe the product without
   overclaiming support. Do not publish a screenshot that is not an authentic capture.
10. **Only then** consider a stable `v0.1.0` release, with a new signed tag and a full
    validation report.

## T. Reference index and quick commands

Repository and release:

- <https://github.com/aryan-astra/BitFly-torrent-stream>
- <https://github.com/aryan-astra/BitFly-torrent-stream/releases/tag/v0.1.0-preview.1>
- <https://github.com/aryan-astra/BitFly-torrent-stream/actions>

Build and test:

```powershell
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
.\build\BitFly.exe
```

Packaged smoke:

```powershell
.\build\stage\BitFly.exe --startup-probe "$env:TEMP\bitfly-probe.json" --quit-after 5000
Get-Content "$env:TEMP\bitfly-probe.json"
```

Release asset verification:

```powershell
$base = 'https://github.com/aryan-astra/BitFly-torrent-stream/releases/download/v0.1.0-preview.1'
Invoke-WebRequest "$base/BitFly-0.1.0-windows-x64.zip" -OutFile BitFly-x64.zip
Invoke-WebRequest "$base/BitFly-0.1.0-windows-x64-SHA256SUMS.txt" -OutFile sums.txt
Get-FileHash .\BitFly-x64.zip -Algorithm SHA256
Get-Content .\sums.txt
```

External references used by the project:

- Qt 6.8 Windows deployment: <https://doc.qt.io/qt-6.8/windows-deployment.html>
- Qt 6.8 licensing: <https://doc.qt.io/qt-6.8/licensing.html>
- Qt third-party components: <https://doc.qt.io/qt-6.8/licenses-used-in-qt.html>
- libtorrent streaming: <https://www.libtorrent.org/streaming.html>
- libtorrent 2.0.11 source: <https://github.com/arvidn/libtorrent/tree/v2.0.11>
- VLC: <https://www.videolan.org/vlc/>
- Inno Setup: <https://jrsoftware.org/isinfo.php>

**Handoff integrity check:** before acting, confirm the branch, tip, tag, release, and
working-tree state with Git and the GitHub links above. If a fact here is stale, update
this document rather than working around it silently.
