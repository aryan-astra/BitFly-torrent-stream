![BitFly — a little less waiting. Paper clouds, film frames and a coral magnet in a blue sky.](docs/branding/bitfly-banner.png)

# <img src="qml/assets/bitfly-mark.png" width="34" height="34" alt=""> BitFly

**Press play before the download is done.**

BitFly brings a torrent stream to your desktop media player before the whole file has downloaded. Paste a magnet link, choose the video, and press **Play**. BitFly fetches the pieces ahead while VLC, mpv or MPC-HC/BE handles the screen.

[![Stable release](https://img.shields.io/github/v/release/aryan-astra/BitFly-torrent-stream?label=stable&style=flat-square&color=ad4739)](https://github.com/aryan-astra/BitFly-torrent-stream/releases/latest) [![Windows builds](https://github.com/aryan-astra/BitFly-torrent-stream/actions/workflows/windows.yml/badge.svg)](https://github.com/aryan-astra/BitFly-torrent-stream/actions/workflows/windows.yml) [![Windows x64 and ARM64](https://img.shields.io/badge/Windows-x64%20%7C%20ARM64-779bb1?style=flat-square)](https://github.com/aryan-astra/BitFly-torrent-stream/releases/latest) [![MIT license](https://img.shields.io/badge/license-MIT-303744?style=flat-square)](LICENSE)

## Get BitFly

**[Download the latest stable release →](https://github.com/aryan-astra/BitFly-torrent-stream/releases/latest)**

| Windows PC | Installer | Portable ZIP | SHA-256 |
| --- | --- | --- | --- |
| **x64** · Intel / AMD | [Download](https://github.com/aryan-astra/BitFly-torrent-stream/releases/download/v1.0.3/BitFly-1.0.3-windows-x64-setup.exe) | [Download](https://github.com/aryan-astra/BitFly-torrent-stream/releases/download/v1.0.3/BitFly-1.0.3-windows-x64.zip) | [Checksum](https://github.com/aryan-astra/BitFly-torrent-stream/releases/download/v1.0.3/BitFly-1.0.3-windows-x64-SHA256SUMS.txt) |
| **ARM64** · Windows on Arm | [Download](https://github.com/aryan-astra/BitFly-torrent-stream/releases/download/v1.0.3/BitFly-1.0.3-windows-arm64-setup.exe) | [Download](https://github.com/aryan-astra/BitFly-torrent-stream/releases/download/v1.0.3/BitFly-1.0.3-windows-arm64.zip) | [Checksum](https://github.com/aryan-astra/BitFly-torrent-stream/releases/download/v1.0.3/BitFly-1.0.3-windows-arm64-SHA256SUMS.txt) |

The installer works per user; the portable ZIP runs without installation. Both keep settings and library data in your Windows user profile. The ZIP needs to be replaced manually to stay portable when updating.

Requires **Windows 10 1809+ on x64, or Windows 11 on x64 / ARM64**, and an installed media player. VLC 3.x is recommended and is required for video-frame previews. Players are installed separately.

<details>
<summary>Verify a download's SHA-256</summary>

Download the matching checksum file, then compare its value in PowerShell:

```powershell
Get-FileHash .\BitFly-1.0.3-windows-x64.zip -Algorithm SHA256
Get-Content .\BitFly-1.0.3-windows-x64-SHA256SUMS.txt
```

</details>

## Get going in a minute

1. Paste a magnet link or choose **Open .torrent**.
2. Select the video file you want to watch.
3. Press **Play**. BitFly opens your media player when the first pieces are ready, then downloads ahead as it plays.

BitFly supports **v1, v2 and hybrid torrents**. Playback depends on the swarm: healthy peers help; a quiet or unavailable swarm can leave you waiting.

## A little home for what you watch

![BitFly's cloud-themed library with covers, saved streams and sidebar controls.](docs/screenshots/v1.0.3/library.png)

*Sample library entries shown in the app.*

- **Active sessions.** File selection, progress, buffer state and player controls in one place.
- **Saved streams.** Magnets, selected files and artwork survive a restart. Stream an entry again from the library, or remove it and its artwork.
- **Cover previews.** Use a frame captured by VLC or choose your own artwork. Covers open from a circle into a square or landscape shape as they load.
- **Queue.** Add and reorder streams, then pick up where you left off after a restart.

## Make it yours

| In Settings | What it does |
| --- | --- |
| **Player** | Choose VLC, mpv, or MPC-HC/BE. Playback stays in that external app. |
| **Storage** | Choose a download folder and cache quota. A folder change applies to new sessions. |
| **Connections** | Set download and upload limits, peer slots, discovery and read-ahead. |
| **Search** | Connect your own Jackett or Prowlarr Torznab feed. BitFly does not provide a torrent catalog. |
| **Cloudflare** | Control a separately installed compatible client. Nothing is bundled or connected automatically. |
| **Updates** | Check at startup and download verified updates in the background. You choose when to install. |

<details>
<summary>Can BitFly delete played pieces to save disk space?</summary>

Not currently. Read-ahead controls which pieces receive priority; it does not turn the download into a rolling, self-deleting buffer. Selected media must fit the available disk space and configured quota. A new download folder applies to future sessions.

</details>

## For developers

**C++20 · Qt 6.8.3 / Qt Quick · libtorrent 2.0.11 · Boost 1.86 · OpenSSL · SQLite**

Local development uses MinGW 13.1. GitHub Actions builds and tests native MSVC packages for x64 and ARM64. Use a compiler that matches your Qt kit.

<details>
<summary>Build locally with the MinGW Qt kit</summary>

Install the matching Qt kit and dependencies first, then adjust these paths:

```powershell
git clone https://github.com/aryan-astra/BitFly-torrent-stream.git
cd BitFly-torrent-stream
$env:PATH="C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.8.3\mingw_64\bin;$env:PATH"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/mingw_64 `
  -DBITFLY_LIBTORRENT_SOURCE=C:/deps/libtorrent `
  -DBITFLY_BOOST_ROOT=C:/deps/boost_1_86_0 `
  -DOPENSSL_ROOT_DIR=C:/deps/openssl
cmake --build build --parallel 1
ctest --test-dir build --output-on-failure
.\build\BitFly.exe
```

The [build guide](docs/BUILD.md) covers dependency setup and other toolchains.

</details>

[Integrations](docs/INTEGRATIONS.md) · [Architecture](docs/ARCHITECTURE.md) · [Testing](docs/TESTING.md) · [Contributing](CONTRIBUTING.md) · [Release process](docs/RELEASE.md) · [Third-party notices](THIRD_PARTY_NOTICES.md)

The stable x64 and ARM64 release passed hosted build, test and package checks. Local streaming tests have shown intermittent range timeouts; Windows 10, ARM64 playback and a real installer upgrade still need hardware validation. See [validation notes](docs/STATUS.md).

No BitFly account, ads or telemetry. Your library stays on your PC. Torrent peers, trackers, configured search services and GitHub update checks still make their normal network connections.

Made by [Aryan Shukla](https://github.com/aryan-astra), with C++ and a little cloud cover. [MIT licensed](LICENSE); dependency licenses and redistribution notes are in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
