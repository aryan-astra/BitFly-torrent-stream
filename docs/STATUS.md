# Validation status

BitFly 0.1.0 is a development preview. Updated 2026-10-23 on Windows x64.
This records execution evidence, not production certification.

- Repository cloned from the empty official public repository.
- Existing GPG identity preserved; initial commit verified locally and by GitHub.
- Qt 6.8.3, matching MinGW 13.1 and libtorrent 2.0.11 source acquired.
- CMake configuration completed successfully on Windows x64.
- Independent QML fixture harness rendered at 100%, 125%, 150% and 200% scaling,
  plus 780x540 minimum window. Navigation, stable delegate identity, selection
  preservation and warning gate passed. Fixtures use synthetic data.
- Focused HTTP suite: 11 passed, 0 failed (945 ms), including the corrected
  error-response re-entry regression. Counts include setup/cleanup entries.
- Focused persistence/version suite: 5 passed, 0 failed (262 ms), including
  refusing a future database schema without changing it.
- Focused scheduler/path suite: 4 passed, 0 failed (25 ms), including range
  overflow, piece bounds and Windows unsafe filenames.
- Controller and main source compilation passed in an isolated object target.
- Full native Release build linked BitFly and all five test executables, using
  Qt MinGW 13.1, Boost 1.86 and OpenSSL 3.6.5.
- CTest HTTP, torrent rules, persistence/version and QML suites passed 4/4 in
  2.36 seconds on the final working tree. The HTTP disconnect-polling test was
  hardened against a one-poll teardown race and passed 30 consecutive runs.
  Additional HTTP Host hardening passed its focused CTest run.
- Full real-torrent integration suite passed 6/6 in 146,439 ms with VLC 3.0:
  v1 torrent, v1 magnet, v2 magnet and hybrid torrent each verified beginning/
  middle/end HTTP ranges, restart/resume, and decoded one second twice (local
  file baseline and the tokenized HTTP URL), both 91,244 bytes. Downloaded at
  stream-ready was 4,898,816 / 4,210,688 / 3,244,032 / 2,752,512 bytes and
  seek-range verification took 1,060 / 1,528 / 3,519 / 3,010 ms respectively.
- Actual application rendered: [native screenshot](screenshots/windows-x64.png).
- Packaged application launched and rendered with PATH limited to the stage and
  Windows directories. Direct subprocess capture filled its 1350x900 frame.
  Hidden-window capture had a DPI artifact and is not used as UI evidence.
- Per-user x64 installer smoke: install, installed app and uninstall all exit 0;
  application removed and user-data sentinel preserved.
- clang-format 18.1.8, QML lint and Cppcheck 2.18 exhaustive warning/portability
  analysis passed. GitHub source-quality checks passed at commit 5c8bfe6.
- Native x64/ARM64 CI and portable/installer packaging configured; artifacts
  are not considered verified until actual build, test and PE validation succeed.

## Performance

The static QML interface uses Qt's software scene graph, adopted after three
actual empty-session runs on Windows build 26300, Intel family 6 model 154, with
about 1.25–1.31 GiB system memory available. First frame was 958, 710 and 718 ms
(median 718 ms); peak working set 129.5–130.3 MiB; private committed memory
80.0–80.9 MiB; idle CPU about 0.015 s over the sampled 2 s windows. Executable:
11,417,474 bytes. The previous hardware renderer measured a 1021 ms median,
184.7–190.5 MiB working set and about 141–142 MiB private committed memory.
These are warm-filesystem short runs, not controlled cold-boot or active-stream
evidence. Raw runs are in
[windows-x64-software-benchmark.json](validation/windows-x64-software-benchmark.json).

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

Local x64 ZIP and installer contain actual executable/runtime files; 94 staged
PE files were validated as x64 and SHA-256 checksums written. These packages are
provisional until final integration fixes are repackaged. Native x64/ARM64 CI
builds are in progress; no ARM64 artifact has been verified. No stable release
has been published.

Commits 31d5c14, 8e1334c, 0ef4cc6 and 5c8bfe6 use the existing Aryan Shukla GPG
identity. GitHub explicitly reported verified=true, reason=valid for 31d5c14,
0ef4cc6 and 5c8bfe6. No key was generated or replaced. No Authenticode certificate
is configured: BitFly executables and installers are unsigned.

## Incomplete release gates

Real-torrent streaming is now proven on this x64 host: v1 files, v1 magnets,
v2 magnets and hybrid torrents each produced verified ranges and decoded audio
through the actual tokenized HTTP URL, with external VLC used for decoding.
This is one local seeder and one codec fixture, not a public-swarm or long-run
guarantee.

Other incomplete gates: native ARM64 build/package/run, Windows 10 hardware,
ARM64 VLC, sustained concurrent playback, disk-full/crash fault injection,
screen-reader review, active-stream and controlled cold-start benchmarks,
sanitizer coverage and complete binary dependency/license audit. License texts
and available Qt SPDX metadata are included; file-existence checks alone do not
constitute a compliance audit. The initial-release definition of done is not met.
