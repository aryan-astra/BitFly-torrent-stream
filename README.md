<p align="center">
  <img src="docs/branding/bitfly-banner.png" alt="BitFly — a little less waiting. Paper clouds, film frames and a coral magnet in a blue sky." width="100%">
</p>

<h1 align="center"><img src="qml/assets/bitfly-mark.png" width="38" height="38" alt=""> BitFly</h1>

<p align="center"><strong>Press play before the download is done.</strong><br>Bring a magnet link, choose your video, and start watching while BitFly fetches the pieces ahead of your player.</p>

<p align="center">A small, native Windows app made with C++ and Qt Quick. Your player handles the screen; BitFly handles the stream.</p>

<p align="center">
  <a href="https://github.com/aryan-astra/BitFly-torrent-stream/releases/latest"><img src="https://img.shields.io/github/v/release/aryan-astra/BitFly-torrent-stream?label=stable&style=flat-square&color=ad4739" alt="Stable release"></a>
  <a href="https://github.com/aryan-astra/BitFly-torrent-stream/actions/workflows/windows.yml"><img src="https://github.com/aryan-astra/BitFly-torrent-stream/actions/workflows/windows.yml/badge.svg" alt="Windows builds"></a>
  <a href="https://github.com/aryan-astra/BitFly-torrent-stream/releases/latest"><img src="https://img.shields.io/badge/Windows-x64%20%7C%20ARM64-779bb1?style=flat-square" alt="Windows x64 and ARM64"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-303744?style=flat-square" alt="MIT license"></a>
</p>

<p align="center"><a href="#get-bitfly">Download</a> · <a href="#your-first-stream">Get started</a> · <a href="#your-library">Your library</a> · <a href="#for-developers">Build</a></p>

<h2 align="center" id="get-bitfly">Get BitFly</h2>

<p align="center"><strong><a href="https://github.com/aryan-astra/BitFly-torrent-stream/releases/latest">Download the latest stable release →</a></strong></p>

<table align="center">
  <thead><tr><th align="center">Windows PC</th><th align="center">Installer</th><th align="center">Portable ZIP</th><th align="center">SHA-256</th></tr></thead>
  <tbody>
    <tr><td align="center"><strong>x64</strong><br>Intel / AMD</td><td align="center"><a href="https://github.com/aryan-astra/BitFly-torrent-stream/releases/download/v1.0.3/BitFly-1.0.3-windows-x64-setup.exe">Download</a></td><td align="center"><a href="https://github.com/aryan-astra/BitFly-torrent-stream/releases/download/v1.0.3/BitFly-1.0.3-windows-x64.zip">Download</a></td><td align="center"><a href="https://github.com/aryan-astra/BitFly-torrent-stream/releases/download/v1.0.3/BitFly-1.0.3-windows-x64-SHA256SUMS.txt">Checksum</a></td></tr>
    <tr><td align="center"><strong>ARM64</strong><br>Windows on Arm</td><td align="center"><a href="https://github.com/aryan-astra/BitFly-torrent-stream/releases/download/v1.0.3/BitFly-1.0.3-windows-arm64-setup.exe">Download</a></td><td align="center"><a href="https://github.com/aryan-astra/BitFly-torrent-stream/releases/download/v1.0.3/BitFly-1.0.3-windows-arm64.zip">Download</a></td><td align="center"><a href="https://github.com/aryan-astra/BitFly-torrent-stream/releases/download/v1.0.3/BitFly-1.0.3-windows-arm64-SHA256SUMS.txt">Checksum</a></td></tr>
  </tbody>
</table>

<p align="center">Requires Windows 10 1809+ on x64, or Windows 11 on x64 / ARM64, plus an installed media player. VLC 3.x is recommended and enables video-frame previews. VLC, mpv and MPC-HC/BE are supported.</p>

<details>
<summary>Verify a download</summary>

Download the matching checksum file and compare it in PowerShell:

```powershell
Get-FileHash .\BitFly-1.0.3-windows-x64.zip -Algorithm SHA256
Get-Content .\BitFly-1.0.3-windows-x64-SHA256SUMS.txt
```

</details>

<h2 align="center" id="your-first-stream">Your first stream</h2>

<p align="center"><strong>1 · Add a link</strong><br>Paste a magnet link or open a <code>.torrent</code> file.</p>
<p align="center"><strong>2 · Choose the video</strong><br>Select the media file you want to watch.</p>
<p align="center"><strong>3 · Press Play</strong><br>BitFly opens your player when the first pieces are ready, then downloads ahead as you watch.</p>

<p align="center">BitFly supports v1, v2 and hybrid torrents. Playback speed depends on the peers available in the swarm.</p>

<h2 align="center" id="your-library">Your library</h2>

<p align="center"><img src="docs/screenshots/v1.0.3/library.png" alt="BitFly's cloud-themed library with covers, saved streams and sidebar controls" width="900"></p>
<p align="center"><em>Sample library entries shown in the app.</em></p>

<p align="center"><strong>Active sessions</strong> keep file selection, progress, buffering and playback controls together.</p>
<p align="center"><strong>Saved streams</strong> remember magnet links, selected files and artwork across restarts. Stream them again from the library; remove an entry and its saved artwork together.</p>
<p align="center"><strong>Cover previews</strong> can use a frame captured by VLC or artwork you choose. Covers expand from a circle into a square or landscape shape as they load.</p>
<p align="center"><strong>The queue</strong> lets you add and reorder streams, and keeps its place when you restart BitFly.</p>

<h2 align="center" id="make-it-yours">Make it yours</h2>

<table align="center">
  <thead><tr><th align="center">Setting</th><th align="center">What it does</th></tr></thead>
  <tbody>
    <tr><td align="center">Player</td><td align="center">Choose VLC, mpv or MPC-HC/BE. Playback stays in that app.</td></tr>
    <tr><td align="center">Storage</td><td align="center">Choose the download folder and cache quota. Folder changes apply to new sessions.</td></tr>
    <tr><td align="center">Connections</td><td align="center">Set download and upload limits, peer slots, discovery and read-ahead.</td></tr>
    <tr><td align="center">Search</td><td align="center">Connect your own Jackett or Prowlarr Torznab feed.</td></tr>
    <tr><td align="center">Updates</td><td align="center">BitFly checks at startup and downloads verified updates; you choose when to install.</td></tr>
  </tbody>
</table>

<p align="center">BitFly does not provide a torrent catalog. Cloudflare client controls are available only for a separately installed compatible client.</p>

<details>
<summary>How does storage work?</summary>

BitFly does not delete already-downloaded pieces as playback moves forward. The selected media must fit your available disk space and configured quota. You can change the download folder for future sessions or remove data you no longer need.

</details>

<details>
<summary>Is the portable version fully portable?</summary>

The ZIP runs without installation, but its settings and library stay in your Windows user profile. In-app updates use the installer; replace the ZIP manually to continue using the portable build without registering it as an installed app.

</details>

<h2 align="center" id="for-developers">For developers</h2>

<p align="center">C++20 · Qt 6.8.3 / Qt Quick · libtorrent 2.0.11 · Boost 1.86 · OpenSSL · SQLite</p>

<p align="center">Local development uses MinGW 13.1. GitHub Actions builds and tests native MSVC packages for x64 and ARM64.</p>

<details>
<summary>Build locally with the MinGW Qt kit</summary>

Install the matching Qt kit and dependencies, then adjust the paths for your machine:

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

See [the build guide](docs/BUILD.md) for dependency setup and other toolchains.

</details>

<p align="center"><a href="docs/INTEGRATIONS.md">Integrations</a> · <a href="docs/ARCHITECTURE.md">Architecture</a> · <a href="docs/TESTING.md">Testing</a> · <a href="CONTRIBUTING.md">Contributing</a> · <a href="docs/RELEASE.md">Releases</a> · <a href="THIRD_PARTY_NOTICES.md">Third-party notices</a></p>

<p align="center">The stable x64 and ARM64 release passed hosted build, test and package checks. Some local streaming tests have shown intermittent range timeouts; Windows 10, ARM64 playback and a real installer upgrade still need hardware validation. See <a href="docs/STATUS.md">validation notes</a>.</p>

<p align="center">No BitFly account, ads or telemetry. Your library stays on your PC; peers, trackers, search providers and GitHub update checks use their normal network connections.</p>

<p align="center">Made by <a href="https://github.com/aryan-astra">Aryan Shukla</a>, with C++ and a little cloud cover.<br><a href="LICENSE">MIT licensed</a> · Dependency details in <a href="THIRD_PARTY_NOTICES.md">third-party notices</a>.</p>
