# Validation status

## 1.0.4 release preparation — 2026-10-29

The latest main build at `2d070b0` completed the hosted Windows workflow
successfully: [run 40739304303](https://github.com/aryan-astra/BitFly-torrent-stream/actions/runs/40739304303).
Main pushes build and test packages but intentionally skip release publication.
The public stable release therefore remained v1.0.3 despite the newer source.

Version 1.0.4 updates CMake and Windows resource metadata for the VLC position
resume and preview-latency fixes below. Its signed version tag will use the same
source-quality, x64/ARM64 tests, package and smoke-check gates before publication.
No launch video, music or private publishing brief is included in the source.

## VLC playback resume and preview latency — 2026-10-28

VLC now reports real playback seconds through a random-port, authenticated
loopback HTTP interface. Positions are stored locally per magnet and file and
passed to VLC on relaunch. Magnet identity parsing now handles the leading `?`
and ignores tracker changes. The player's Qt display environment is isolated
from BitFly's test configuration. Other players do not report position yet.

The installed-VLC suite passed all 11 cases in 94.154 seconds, including a real
torrent seek, SQLite progress readback and a reopened VLC seeking to the saved
position. Black-intro rejection, later sampling, partial MP4 tail metadata,
automatic retry, cancellation and artwork preservation passed. Preview capture
uses a shorter network cache, denser scene sampling and 150 ms frame checks;
this does not remove swarm or container-metadata delays, and no comparative
public-swarm latency benchmark was performed.

HTTP, app, services, controller and QML smoke groups passed in 13.28 seconds.
The x64 executable rebuilt, formatting passed, and the existing packaging script
created an x64 ZIP under `dist/resume-preview`. With SDK paths removed, its
application initialized a separate SQLite profile, rendered an inspected library
screenshot and exited normally. No startup timing benchmark was taken.
This local build is not a new published release. Signing was verified with the
configured production key after restoring its cache from Windows account
encryption; the signing setup is local and is not included in the repository.

## 1.0.3 preview layout and release verification — 2026-10-26

Preview cards now fill their available width and their layout follows square or
landscape artwork, including the circle transition and reduced-motion behavior.
Synthetic [library](screenshots/v1.0.3/library.png) and
[active-session](screenshots/v1.0.3/session.png) renders were inspected. Existing
1000-entry virtualization, keyboard navigation and help focus tests passed.

The updater targets the running app folder and uses normal shutdown to persist
sessions and close players. Its startup check and verified background download
remain automatic; installation is a visible action. A new controller regression
covers unavailable updates with active/paused sessions and preserved history.
Five focused test groups passed in 14.28 seconds. Formatting and full application
warning/portability analysis passed.

The first complete suite passed 11 of 12 groups in 401.40 seconds. Controller
failed without assertion output, then passed six diagnostic reruns (all eight
cases). The next complete run passed 11 of 12 groups in 428.31 seconds; controller
passed but recovery failed. Explicit per-group Qt result files then captured a
v2 byte-range timeout in the initial HTTP range loop, before resume corruption.
A standalone three-row recovery run passed in 118.889 seconds and a focused v2
row passed in 42.263 seconds. These reruns do not resolve the intermittent failure.
The console-target flag alone did not restore captured output on this host; Qt
file logs and failure-only CI artifact uploads now preserve diagnostics. Range
assertions include bounds, received bytes, snapshots and bounded peer diagnostics.
All evidence is retained under [validation/v1.0.3](validation/v1.0.3).

Signed production commits and signed tag `v1.0.3` at `8b579a0` are pushed. Subsequent
main commits change tests, diagnostic uploads and documentation only. The
[cloud release workflow](https://github.com/aryan-astra/BitFly-torrent-stream/actions/runs/40341995240)
completed successfully on x64 and ARM64, including build, tests, package smoke,
checksums and quality analysis. GitHub published the non-prerelease
[stable v1.0.3 release](https://github.com/aryan-astra/BitFly-torrent-stream/releases/tag/v1.0.3)
with installers, ZIPs and checksum manifests for both architectures. Publication
finished before the local hold request; a cancellation attempt returned409 because
the run was already complete. No tag was moved and no release asset was removed.

The cloud chat, “Monitor CI Progress”, has the reproduced local timeout and is
continuing verification/investigation independently. There is no blanket claim
that the app or test suite is issue-free. Production installer replacement and
relaunch were not executed locally; installation remains explicit after automatic
checking and verified download. The production `UpdateService::parseRelease`
parser was also exercised against GitHub’s actual latest-release JSON: version
1.0.2 accepts the published1.0.3 installer, size and SHA256 for x64 and ARM64.
This checks live manifest compatibility, not production installer execution.


## 1.0.2 automatic preview recovery — 2026-10-26

Partial MP4/MOV streams now prefetch the final 1 MiB (piece-aligned) for container
metadata and include it in startup readiness. Concurrent HTTP reads retain pending
verified replies when another client seeks. VLC capture gets 45 seconds per attempt;
failed captures retry after additional downloaded data, with 30-second to 5-minute
backoff. Cards show generating, waiting, paused or unavailable preview status, and
the native window title identifies the running version.

All 12 CTest groups passed in 378.70 seconds. New regressions cover a throttled
H.264 MP4 with an end-of-file index while less than half downloaded, automatic
recovery after an interrupted capture, and overlapping stream reads. Existing
swarm, recovery, storage, controller and QML tests also passed. Exhaustive warning/
portability static analysis and C++ formatting passed. Evidence is under
[validation/v1.0.2](validation/v1.0.2).

The installer and portable ZIP were rebuilt and both SHA256 entries verified.
The extracted ZIP rendered successfully using only packaged DLLs on PATH and the
matching SDK offscreen validation plugin, with isolated diagnostic state. The
[preview status screenshot](screenshots/v1.0.2/preview-status.png) uses test fixtures;
the [empty library](screenshots/v1.0.2/library.png) comes from the extracted app.
The production installer was not executed.

The exact movie in the reported screenshot was not downloaded or tested. Tests use
synthetic local media and installed VLC; visual frame filtering remains a heuristic.
No GitHub push or release publication occurred.

## 1.0.1 thumbnail and readability verification — 2026-10-26

Video metadata now prepares a preview without pressing Play. Paused sessions,
saved file selections and custom artwork remain respected. Capture samples a
random 30–90 seconds into video, rejects near-black/white/uniform frames, and
falls back if a seek fails. One delayed retry handles initially unready data.
This is a visual-quality heuristic, not a studio-logo recognition model.

Nine focused test groups passed; the final controller/preview rerun also passed.
The added end-to-end test uses a real local torrent seeder, controller selection,
HTTP stream and installed VLC to persist a non-black movie frame without player
launch. Additional cases cover a black intro, later seek, past-end fallback,
blank-frame rejection and image URLs with cache-revision queries. Exhaustive
application static checks and full C++ formatting passed. Evidence is under
[validation/v1.0.1](validation/v1.0.1).

Small labels were enlarged to 12px minimum, body controls to 14–15px, secondary
text darkened, pure-white panels softened and link surfaces made opaque. Main
text/panel combinations pass 4.5:1 contrast. Minimum 940x640 renders and final
[extracted portable screenshot](screenshots/v1.0.1/library.png) were inspected.
The installer and ZIP were rebuilt, their checksums verified, and the extracted
ZIP rendered using packaged DLLs plus the matching offscreen validation plugin.
No production installer was executed; no GitHub push or publication occurred.



## Local v1 redesign verification — 2026-10-26

The audited 1.0.0 source was a local candidate. Nothing was pushed or published.
The original B monogram was retained at the user's request; the white interface
now uses the selected darker sky/paper-cloud background and 3D sidebar icons.

All 12 CTest groups passed across the final focused/swarm runs. The four normal
swarm formats passed in 169.19 s; damaged v1/v2-resume and quota recovery passed
in 119.43 s; storage lifecycle regressions passed in 2.59 s. Controller/artwork
integration, real installed-VLC HTTP preview capture/cancellation, optional
services, keyboard/tooltips/cover morph and 1000-entry library virtualization
passed. Final controller changes and final artwork were rechecked separately.
Formatting and exhaustive warning/portability cppcheck completed with exit 0.
Evidence: [validation/v1](validation/v1).

Three isolated empty-session software-renderer runs recorded about 72.4 MiB peak
working set and 46.3–49.6 MiB peak private commit, with 567 ms median first frame.
This excludes active transfers, external players, preview decoding and startup
update/Cloudflare status checks; it is not a cold-boot or whole-stream budget.
The machine was under memory pressure. See the benchmark JSON for raw values.

The local x64 installer is 28,030,320 bytes; portable ZIP 39,970,619 bytes; staged
files 123,018,080 bytes. Qt deployment, architecture checks, Inno compilation and
checksums completed. Compiler-tool runtimes unnecessary to deployment were
excluded by supplying only the needed OpenSSL runtime directory. The package's
Windows plugin booted and exited cleanly with SDK runtime paths removed.
Because a hidden Windows window does not render a frame, the packaged QML/image
render was separately checked using the matching SDK offscreen plugin, with
only packaged DLLs on PATH. This distinction is intentional; it does not claim
an exposed-window Windows first-frame measurement.

Actual current-app render: [screenshots/v1/packaged-library.png](screenshots/v1/packaged-library.png).
The installer was built and hashed, not installed over the user's application.
Fresh ARM64 CI, physical-device tests and publisher signing remain future
release gates. Earlier preview records below describe their historical source.



BitFly 0.1.0-preview.1 is a development preview. Updated 2026-10-25 on Windows
x64. This records execution evidence, not production certification. The
authoritative preview is GitHub release `422010008`, published
2026-10-23T15:32:38Z as prerelease `v0.1.0-preview.1`. The redesign described in
[REDESIGN_PLAN.md](REDESIGN_PLAN.md) is unreleased work in the local tree; it
does not change the preview assets, tag, or binaries recorded below.

## Redesign validation (2026-10-25, incomplete)

The redesign replaces Stream/History with a Library of bright live sessions and
dimmed saved posters, persists each entry's magnet and generated cover art, moves
new sessions to a configurable storage root, adds hover help to every setting,
and applies a tuned libtorrent profile with bounded stream-ahead.

Evidence collected so far:

- The engine's missing ordering contract is fixed by construction: a session
  snapshot is published before `streamReady` is emitted. The ARM64 `recovery`
  failure was consistent with a listener observing `streamReady` before the
  selected file existed in `sessionsChanged`.
- Repeated three-case sequences (`realLibtorrentStreamsRangesAndRestores:v1`,
  `:v1magnet`, `:v2`) were run against both the HEAD engine and the redesigned
  engine. HEAD produced 4/4 clean sequences. The redesigned engine produced
  15/16, 10/10, 13/14, 4/4 and (at last observation) 5/5 clean sequences across
  separate runs. Console-bound QtTest output was captured with `-o <file>,txt`.
- The one observed redesign failure was an access violation (`0xC0000005`)
  during teardown or the next case's hash check. GDB reported a heap block
  modified after free, with the detected stack in `libtorrent::set_piece_hashes`
  or an asio thread. That is a detection point, not the write. No usable crash
  dump was captured; WER LocalDumps wrote nothing because the process exits
  outside WER handling.
- One teardown suspect was removed: resume data may carry sparse merkle trees,
  and `write_torrent_file_buf` walks them unsafely. Teardown now clears
  `merkle_trees`, `verified_leaf_hashes` and `merkle_tree_mask`, then writes v2
  with `allow_missing_piece_layer`. libtorrent explicitly permits a v2
  `.torrent` without piece layers. This is a defensible fix for one path, not a
  proven root cause of the intermittent failure.
- `app_tests` passed with the new deterministic cover-art test,
  `coverArtIsDeterministicAndPersists`; `qml_smoke` passed offscreen against the
  updated mock contract and produced Library, Queue and Settings screenshots.
  `bitfly_platform` and the full `BitFly` target built and linked with Qt 6.8.3
  MinGW x64.
- The final stress run observed 5/5 clean sequences before the session lost
  shell access; the remaining sequences and the post-magnet-cache rebuild were
  not observed. The counts above are therefore provisional.

Not yet valid for this tree: a post-change `recovery` run, a
`v2-corrupt-resume` run against the piece-layer-free fallback, a full `ctest`
run including `integration` and `recovery`, `qmllint`, formatting, and a
packaged first-frame smoke. No redesign test was skipped, weakened, or deleted.
The redesign is not committed in the evidence above; the preview release and its
six assets are unchanged.

## Continuation validation (2026-10-23)

Three added local fault scenarios passed (5 QtTest entries including setup and
cleanup, 158,290 ms): corrupt v1 resume, corrupt v2 resume, and zero-quota
admission followed by raising the quota and explicitly selecting the media.
The corrupt-resume cases overwrite `.fastresume` after shutdown, observe the
stored-metadata fallback in a fresh engine, and verify restored HTTP bytes.
The quota case verifies no downloaded bytes or ready signal before increasing
the limit. These runs did not enable the optional VLC decode check.

This is focused resume-corruption and quota-admission evidence, not physical
disk-full, process-kill, or sustained concurrency validation. Only the test
target was rebuilt; the application executable and published assets were not.

On 2026-10-24, SQLite fault tests reproduced four failures in the pre-fix store:
three malformed-setting cases and a failed-initialization schema-version change.
The unreleased fix reports invalid stored JSON and refuses further writes,
preserving both the damaged row and unrelated saved values. Initialization now
returns on failure before advancing `user_version`. The affected platform
library and `app_tests` target compiled; all 9 QtTest entries passed in 891 ms,
including the existing persistence and future-schema tests. This source fix is
not present in the unchanged preview release binaries.

## Local x64 build and streaming

- Qt 6.8.3 with MinGW 13.1, libtorrent 2.0.11, Boost 1.86 and OpenSSL 3.6.5.
  The Release build links the application and all five test executables.
- The independent QML fixture harness rendered at 100%, 125%, 150% and 200%
  scaling, plus the 780x540 minimum window. Navigation, stable delegate
  identity, selection preservation and the warning gate passed. Fixtures use
  synthetic data.
- At the preview checkpoint, CTest registered five suites: `http`, `torrent`, `app`, `integration` and
  `qml_smoke`. Excluding the separately-run integration binary, the other four
  passed 4/4 in 2.46 seconds on the final tree. The HTTP disconnect-polling test
  was hardened against a one-poll teardown race and passed 30 consecutive runs.
  Host-header and absolute-target hardening passed its focused suite.
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

Three GitHub Actions runs are relevant. Their overall conclusions are distinct
from the conclusions of individual jobs:

- [39981098649](https://github.com/aryan-astra/BitFly-torrent-stream/actions/runs/39981098649)
  at `2ea1197` is the latest `main` run at the preview evidence snapshot. Source
  quality, x64 and ARM64 passed. Each architecture passed 5/5 CTest and
  validated 97 x64 and 93 ARM64 PE files. The release job was skipped because
  the ref is not a tag; the run's overall conclusion is success.
- [39973094582](https://github.com/aryan-astra/BitFly-torrent-stream/actions/runs/39973094582)
  at `f7b3706` is the last fully successful pre-tag run. Source quality, x64 and
  ARM64 passed. x64 passed 5/5 CTest including the real-libtorrent integration
  test (147.23 s), validated 97 PE files and rendered its staged first frame in
  3,130 ms. ARM64 passed 5/5, validated 93 PE files and rendered in 2,620 ms.
  Both smoke runs exited 0. The release job was skipped because no tag existed.
- [39977378333](https://github.com/aryan-astra/BitFly-torrent-stream/actions/runs/39977378333)
  at signed tag `v0.1.0-preview.1` (`9c27337`) passed source quality, x64 and
  ARM64, including 5/5 CTest each, 97 x64 and 93 ARM64 PE files, staged first
  frames in 2,061 and 2,295 ms, and clean exits. The release job's publish step
  failed; the run's overall conclusion is failure. See the preview-release
  section below.

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
is published as a prerelease. It contains the six assets built by run
`39977378333`. The release job's publish step initially failed because it had no
repository checkout for `gh release create --verify-tag`. The verified assets
from that run were uploaded through the GitHub API and downloaded again to
confirm their hashes. Workflow commit `9003a06` adds the checkout; that corrected
automatic publish path has not yet been exercised by a new tag. The existing
signed tag must not be moved or deleted to test it.

| Release asset | Bytes | SHA-256 |
| --- | ---: | --- |
| x64 ZIP | 44,397,803 | `5194D64575ADC52702350B32336540561DC4D90CEF6FBC1831E0913FD76AB99F` |
| x64 setup | 29,683,549 | `E79BC566436DBA4CC2F3E62CFF8628582536A3A5E30AC991DAB4F4DCA32768BB` |
| ARM64 ZIP | 30,653,290 | `6AD0AC5E3A0D9E504751181C63451927445CF41C8F3EA27B1A6727CAFF1223AC` |
| ARM64 setup | 20,610,258 | `07D344B2D85371BF9BFCA18C06CF32DAA26C769E22740F9783C8792958485872` |

Each architecture also publishes a `SHA256SUMS` asset. The four downloaded
release binaries matched those files, and GitHub's asset digests match the
recorded hashes. The downloaded x64 portable package also launched with PATH
restricted to the package and Windows directories, rendered a first frame, and
exited 0.

The in-app update checker queries GitHub's `/releases/latest` endpoint, which
excludes prereleases. Preview 1 therefore produces no stable-update offer.

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
The staged application smoke test rendered in 1,747 ms and exited 0. These
MinGW artifacts were not uploaded; the GitHub release assets are the separately
built MSVC packages listed above.

At the preview evidence snapshot (`2ea1197`), `main` had 15 commits. All 15 used
the existing Aryan Shukla GPG identity, and GitHub reported
`verified=true, reason=valid` for each. The annotated tag
`v0.1.0-preview.1` is GPG-signed and GitHub also reports it verified. The signing
key fingerprint is `A17E14BFC29390B8A296B3CF95FCCAC560D38AD1`. Documentation
commits added after that snapshot use the same identity. No key was generated or
replaced. No AI or co-author trailers are present.

No Authenticode certificate is configured: BitFly executables and installers are
unsigned. `Get-AuthenticodeSignature` reports `NotSigned` for both architectures'
outputs.

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
compilation, packaging and first-frame smoke testing are proven in CI, but that
runner is not a substitute for physical-device playback validation. License
texts and available Qt SPDX metadata are included; file-existence checks alone
do not constitute a compliance audit. The corrected tag-triggered release
publish path has not run on a new tag. The initial-release definition of done is
not met.
