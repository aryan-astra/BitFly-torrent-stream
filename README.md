# BitFly

[![Release](https://img.shields.io/github/v/release/aryan-astra/BitFly-torrent-stream?include_prereleases&label=release&style=flat-square)](https://github.com/aryan-astra/BitFly-torrent-stream/releases)
[![Windows CI](https://github.com/aryan-astra/BitFly-torrent-stream/actions/workflows/windows.yml/badge.svg?branch=main)](https://github.com/aryan-astra/BitFly-torrent-stream/actions/workflows/windows.yml)
[![Source quality](https://github.com/aryan-astra/BitFly-torrent-stream/actions/workflows/source-checks.yml/badge.svg?branch=main)](https://github.com/aryan-astra/BitFly-torrent-stream/actions/workflows/source-checks.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue?style=flat-square)](LICENSE)

BitFly is a native Windows application for streaming torrent video in your
installed [VLC Media Player](https://www.videolan.org/vlc/). Open a magnet link or
a `.torrent` file, choose a media file, and VLC plays it from a local
token-protected HTTP stream while libtorrent fetches the pieces that playback
needs.

![BitFly running on Windows 11 x64](docs/screenshots/windows-x64.png)

> **Development preview.** `v0.1.0-preview.1` is published for evaluation, not as
> a stable or production-certified release. The binaries are not Authenticode
> signed, so Windows may show a publisher warning. See
> [validation status](docs/STATUS.md) for executed checks, evidence, and the
> remaining release gates.

## Download

The current prerelease is [v0.1.0-preview.1](https://github.com/aryan-astra/BitFly-torrent-stream/releases/tag/v0.1.0-preview.1).

| Platform | Installer (per-user) | Portable ZIP | Checksums |
| --- | --- | --- | --- |
| Windows x64 | [BitFly 0.1.0 x64 setup (29.7 MB)](https://github.com/aryan-astra/BitFly-torrent-stream/releases/download/v0.1.0-preview.1/BitFly-0.1.0-windows-x64-setup.exe) | [BitFly 0.1.0 x64 ZIP (44.4 MB)](https://github.com/aryan-astra/BitFly-torrent-stream/releases/download/v0.1.0-preview.1/BitFly-0.1.0-windows-x64.zip) | [x64 SHA-256](https://github.com/aryan-astra/BitFly-torrent-stream/releases/download/v0.1.0-preview.1/BitFly-0.1.0-windows-x64-SHA256SUMS.txt) |
| Windows ARM64 | [BitFly 0.1.0 ARM64 setup (20.6 MB)](https://github.com/aryan-astra/BitFly-torrent-stream/releases/download/v0.1.0-preview.1/BitFly-0.1.0-windows-arm64-setup.exe) | [BitFly 0.1.0 ARM64 ZIP (30.7 MB)](https://github.com/aryan-astra/BitFly-torrent-stream/releases/download/v0.1.0-preview.1/BitFly-0.1.0-windows-arm64.zip) | [ARM64 SHA-256](https://github.com/aryan-astra/BitFly-torrent-stream/releases/download/v0.1.0-preview.1/BitFly-0.1.0-windows-arm64-SHA256SUMS.txt) |

The installer runs without administrator rights, installs under
`%LOCALAPPDATA%\Programs\BitFly`, and can be removed from Windows Settings. The
portable ZIP contains the same application and needs no installation. Unpack it
to a folder you control and run `BitFly.exe`.

Verify a download before running it:

```powershell
Get-FileHash .\BitFly-0.1.0-windows-x64.zip -Algorithm SHA256
Get-Content .\BitFly-0.1.0-windows-x64-SHA256SUMS.txt
```

## Requirements

- Windows 10 version 1809 or later (x64), Windows 11 (x64), or Windows 11 on
  ARM64. Windows 11 x64 is the validated development host; the other targets
  have the validation gaps recorded in [STATUS.md](docs/STATUS.md).
- [VLC Media Player](https://www.videolan.org/vlc/) 3.x for the matching
  architecture. VLC is **required** and is not bundled, downloaded, or installed
  by BitFly. BitFly detects `vlc.exe`; you can select it manually in Settings.
- Media you are authorized to access and share. BitFly has no search service,
  content catalog, or DRM circumvention.

## Use

1. Install or unpack BitFly, then start it. If VLC is not detected, choose
   `vlc.exe` in **Settings**.
2. Paste a magnet link on the **Stream** page, or choose **Open torrent file**.
3. Select a playable file and press **Play**. *Preparing* means BitFly is still
   retrieving the pieces needed to start. VLC opens once the initial verified
   range is available.
4. Playback continues while BitFly downloads ahead of VLC's requests. Seeking
   changes the requested byte ranges and reprioritizes pieces.

Use **Queue** to line up files for the next VLC session and **History** to relaunch
a previous stream. Queue and history survive a restart. State is saved
transactionally, and torrent resume data is reused where available; playback
always requires an explicit launch.

When an owned VLC window closes, BitFly asks whether to **Keep** the data,
**Delete** it, or **Seed** temporarily. Keeping data pauses network activity;
deleting requires confirmation and affects only that managed cache entry; the
default seeding period is 15 minutes and is configurable in Settings.

## Features

- Magnet links and `.torrent` files, including v1, v2, and hybrid torrents.
- Range-driven piece scheduling with verified-only reads; HTTP never serves
  unverified bytes or invents data when the swarm is slow.
- A loopback HTTP server with an unpredictable per-session token, strict host
  validation, and no filesystem paths in URLs.
- Playable-file selection, multiple sessions, pausing, and removal.
- Persistent queue, recent history, preferences, and libtorrent resume state in
  local SQLite storage.
- Cache quota admission, bounded temporary seeding, retained data, and
  user-confirmed deletion of managed data.
- Native C++20 and Qt Quick interface. The static UI uses Qt's software scene
  graph: about 130 MiB peak working set and a 718 ms median first frame on the
  recorded x64 host.
- No telemetry, account, cloud backend, or automatic binary updates.

## How it works

```text
VLC  ──HTTP byte ranges──▶  loopback server  ──▶  scheduler + verified cache  ──▶  libtorrent
```

The Qt Quick UI, controller, and persistence run on the GUI thread. One worker
thread owns libtorrent, the piece cache, scheduling, and the HTTP server.
Owned VLC processes are launched with the local stream URL and remain external to
BitFly. See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the ADR and
[docs/CONTRACTS.md](docs/CONTRACTS.md) for interface boundaries.

## Build and test

BitFly builds with CMake, Qt 6.8.3, libtorrent 2.0.11, Boost 1.86, and OpenSSL.
Local development uses MinGW 13.1; CI builds native MSVC x64 and ARM64 packages on
Windows runners.

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/mingw_64 `
  -DBITFLY_LIBTORRENT_SOURCE=C:/deps/libtorrent `
  -DBITFLY_BOOST_ROOT=C:/deps/boost_1_86_0 `
  -DOPENSSL_ROOT_DIR=C:/deps/openssl
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

See [docs/BUILD.md](docs/BUILD.md) for dependency versions and alternative
toolchains, and [docs/TESTING.md](docs/TESTING.md) for the HTTP, scheduler,
persistence, QML, integration, VLC-decode, and packaged-app checks. The
integration suite uses local deterministic torrents and does not require a public
swarm.

## Status and limitations

This preview is honest about what has and has not been proven:

- Local x64 validation includes real-torrent streaming in v1, v1-magnet,
  v2-magnet, and hybrid modes, verified HTTP ranges, restart/resume, and audio
  decoded through the actual tokenized HTTP URL by external VLC.
- x64 and ARM64 builds, packaging, PE architecture checks, installers, and
  first-frame smoke tests pass in CI.
- Physical Windows 11 ARM64 VLC playback, physical Windows 10 testing, sustained
  concurrent playback, disk-full/crash fault injection, screen-reader review,
  and controlled cold-start benchmarks remain outstanding.
- The streaming evidence comes from one local seeder and one media fixture, not a
  public swarm at scale.
- Binaries are unsigned and have no stable security-maintenance commitment yet.
- Exact VLC playback position is not tracked or restored. BitFly does not bundle
  a player, index torrents, find subtitles, or bypass DRM.
- The in-app update check consults GitHub's latest **stable** release, so it
  deliberately ignores this prerelease.

The authoritative evidence and release gates are in [docs/STATUS.md](docs/STATUS.md).
Do not treat a screenshot or a successful launch as proof of a platform or
codec path that is not listed there.

## Contributing

Read [CONTRIBUTING.md](CONTRIBUTING.md) before opening a pull request. Changes
need focused tests and a clear description of user-visible behavior. Report
security issues privately according to [SECURITY.md](SECURITY.md), and never post
private torrents, credentials, or personal paths in a public issue.

## License

BitFly is released under the [MIT License](LICENSE). Dependencies keep their own
licenses; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and
[docs/RELEASE.md](docs/RELEASE.md) for packaging and redistribution details.
