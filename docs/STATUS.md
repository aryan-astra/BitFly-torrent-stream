# Validation status

BitFly 0.1.0 is a development preview. Updated 2026-10-23 on Windows x64.
This records execution evidence, not production certification.

## Local x64 build and streaming

- Qt 6.8.3 with MinGW 13.1, libtorrent 2.0.11, Boost 1.86 and OpenSSL 3.6.5.
  The Release build links the application and all five test executables.
- The independent QML fixture harness rendered at 100%, 125%, 150% and 200%
  scaling, plus the 780x540 minimum window. Navigation, stable delegate
  identity, selection preservation and the warning gate passed. Fixtures use
  synthetic data.
- CTest, excluding the separately-run integration binary, passed 4/4 in 2.46
  seconds on the final tree. The HTTP disconnect-polling test was hardened
  against a one-poll teardown race and passed 30 consecutive runs. Host-header
  and absolute-target hardening passed its focused suite.
- The full real-torrent integration suite passed 6/6 in 144,725 ms with
  external VLC 3.0. v1 torrent, v1 magnet, v2 magnet and hybrid torrent modes
  each verified beginning/middle/end HTTP ranges and restart/resume, then
  decoded one second through both a local-file baseline and the tokenized HTTP
  URL. All eight decodes produced 91,244 bytes.
- Downloaded-at-stream-ready bytes and seek-range verification times were:
  v1 4,800,512 / 1,899 ms; v1 magnet 4,145,152 / 951 ms; v2 3,244,032 /
  3,071 ms; hybrid 3,260,416 / 6,015 ms.
- The actual application rendered: [native screenshot](screenshots/windows-x64.png).
- The packaged application launched and rendered with PATH limited to the
  stage and Windows directories. Direct subprocess capture filled its 1350x900
  frame; hidden-window capture had a DPI artifact and is not UI evidence.
- Per-user x64 installer smoke: install, installed app and uninstall all
  exited 0; the application was removed and the user-data sentinel preserved.
- clang-format 18.1.8, QML lint and Cppcheck 2.18 exhaustive warning and
  portability analysis passed.

## Performance

The static QML interface uses Qt's software scene graph, adopted after three
actual empty-session runs on Windows build 26300, Intel family 6 model 154,
with about 1.25–1.31 GiB system memory available. First frame was 958, 710 and
718 ms (median 718 ms); peak working set 129.5–130.3 MiB; private committed
memory 80.0–80.9 MiB; idle CPU about 0.015 s over the sampled 2 s windows.
Executable: 11,417,474 bytes. The previous hardware renderer measured a 1021 ms
median, 184.7–190.5 MiB working set and about 141–142 MiB private committed
memory. These are warm-filesystem short runs, not controlled cold-boot or
active-stream evidence. Raw runs are in
[windows-x64-software-benchmark.json](validation/windows-x64-software-benchmark.json).

## CI evidence

Two successful GitHub Actions runs provide the final evidence:

- [39973094582](https://github.com/aryan-astra/BitFly-torrent-stream/actions/runs/39973094582)
  at `f7b3706` passed source quality, x64 and ARM64. The release job was
  skipped because no tag existed. x64 passed 5/5 CTest including the
  real-libtorrent integration test (147.23 s), validated 97 PE files and
  rendered its staged first frame in 3,130 ms. ARM64 passed 5/5, validated 93
  PE files and rendered in 2,620 ms. Both smoke runs exited 0.
- [39977378333](https://github.com/aryan-astra/BitFly-torrent-stream/actions/runs/39977378333)
  at signed tag `v0.1.0-preview.1` (`9c27337`) rebuilt and retested both
  architectures: 5/5 CTest each, 97 x64 and 93 ARM64 PE files, staged first
  frames in 2,061 and 2,295 ms, and clean exits.

The ARM64 Visual Studio redistributable contains an x64 copy of
`vcruntime140_1.dll`, a known Microsoft packaging defect. Packaging excluded
that file and proved that no staged ARM64 PE imports it. The ARM64 Qt libraries
import `VCRUNTIME140.dll`, not `VCRUNTIME140_1.dll`.

Both CI artifact archives were downloaded and inspected locally. The x64 ZIP
had 1,461 entries and 97 PE files, all x64. The ARM64 ZIP had 1,457 entries and
93 PE files, all AA64. CI smoke proves first-frame rendering and clean process
exit; VLC decoding was proven by the local integration suite, not by CI.

### Preview release

[v0.1.0-preview.1](https://github.com/aryan-astra/BitFly-torrent-stream/releases/tag/v0.1.0-preview.1)
is published as a prerelease. Both builds passed, but the release job's publish
step initially failed because it had no repository checkout for
`gh release create --verify-tag`. The verified assets from that run were
uploaded through the GitHub API and downloaded again to confirm their hashes.
The workflow now checks out the tag before publishing.

| Release asset | Bytes | SHA-256 |
| --- | ---: | --- |
| x64 ZIP | 44,397,803 | `5194D64575ADC52702350B32336540561DC4D90CEF6FBC1831E0913FD76AB99F` |
| x64 setup | 29,683,549 | `E79BC566436DBA4CC2F3E62CFF8628582536A3A5E30AC991DAB4F4DCA32768BB` |
| ARM64 ZIP | 30,653,290 | `6AD0AC5E3A0D9E504751181C63451927445CF41C8F3EA27B1A6727CAFF1223AC` |
| ARM64 setup | 20,610,258 | `07D344B2D85371BF9BFCA18C06CF32DAA26C769E22740F9783C8792958485872` |

Each architecture also publishes a `SHA256SUMS` asset. The four downloaded
release binaries matched those files.

## Features and limitations

Implemented: C++20/Qt Quick UI, libtorrent worker, bounded verified-piece cache,
loopback byte ranges, magnets/torrent files, media selection and natural order,
owned external VLC processes, multiple sessions, queue/history, cache quota
admission, atomic resume persistence, timed seeding, retention, confirmed
deletion, settings, diagnostics and manual bounded update checks.

The scheduler responds to requested bytes and download rate; it does not estimate
media bitrate or promise uninterrupted playback. Cache quotas pause/admit
downloads instead of evicting active libtorrent pieces. History records VLC
launches, not watched positions. Separate subtitle discovery/attachment, exact
VLC playback tracking and automatic watched-episode detection are unavailable.
VLC can use its own subtitle controls.

## Distribution and Git

A clean local MinGW x64 stage was packaged with 84 validated x64 PE files. The
portable ZIP is 39,327,259 bytes
(`071DA6D24239E21EE93A633636E0EAD38C9197650C86916A7D7AB0EF4B2ED62A`);
the installer is 26,993,968 bytes
(`EDBF464558A6D5867777F4D234FB8B6DE94F5233890F20384B4C3B767F22B1F1`).
The staged application smoke test rendered in 1,747 ms and exited 0. The MSVC
CI packages above separately carry their own SHA-256 files.

Commits `499cf97`, `c3e95f5`, `de825fe`, `4372645`, `1267b74`, `7e3175c`,
`f7b3706`, `9c27337` and `9003a06` use the existing Aryan Shukla GPG identity.
GitHub reported `verified=true, reason=valid` for all nine, as it did for
earlier commits `31d5c14`, `8e1334c`, `0ef4cc6` and `5c8bfe6`. The annotated
tag `v0.1.0-preview.1` is GPG-signed and GitHub also reports it verified. No
key was generated or replaced. No Authenticode certificate is configured:
BitFly executables and installers are unsigned.

No stable release has been published. Preview 1 is explicitly marked as a
prerelease and does not change the incomplete gates below.

## Incomplete release gates

Real-torrent streaming is proven on this x64 host: v1 files, v1 magnets, v2
magnets and hybrid torrents each produced verified ranges and decoded audio
through the actual tokenized HTTP URL, with external VLC used for decoding.
This is one local seeder and one codec fixture, not a public-swarm or long-run
guarantee.

Other incomplete gates: physical ARM64 VLC playback and Windows 10 hardware
testing, sustained concurrent playback, disk-full/crash fault injection,
screen-reader review, active-stream and controlled cold-start benchmarks,
sanitizer coverage and a complete binary dependency/license audit. Native ARM64
compilation, packaging and first-frame smoke testing are now proven in CI, but
that runner is not a substitute for physical-device playback validation. License
texts and available Qt SPDX metadata are included; file-existence checks alone
do not constitute a compliance audit. The initial-release definition of done is
not met.
