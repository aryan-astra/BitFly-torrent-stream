# Testing

Build with `BUILD_TESTING=ON` (the default). CTest registers five suites:

| Test | What it covers |
| --- | --- |
| `http` | Byte ranges, request validation, token routing, host/absolute-target rejection, disconnect handling |
| `torrent` | Scheduler deadlines, path handling, and deterministic streaming rules |
| `app` | Version parsing and transactional SQLite persistence, including future-schema refusal |
| `integration` | Real libtorrent sessions against a local throttled seeder, with optional VLC decode |
| `qml_smoke` | Headless load and interaction checks for the actual `Main.qml` |

```powershell
ctest --test-dir build --output-on-failure
```

`http`, `torrent`, `app` and `qml_smoke` use local deterministic data and need no
public swarm. The local evidence run excluded the long `integration` suite and
passed the other four in 2.46 seconds. CI runs all five. `qml_smoke` loads the QML
with a contract-shaped mock controller, checks navigation, stable delegate
identity, selection preservation, and QML warnings, then renders a frame. It is a
smoke test, not proof of native file dialogs, keyboard traversal, or
screen-reader behavior. Set `QT_SCALE_FACTOR` to 1, 1.25, 1.5 and 2 when
reviewing DPI. The real application must also be rendered:
`BitFly.exe --screenshot path.png` captures an actual implemented window and
exits with a failure code if capture fails.

The integration suite generates an 8 MiB PCM WAV and v1, v2 and hybrid torrents,
starts a separate throttled loopback libtorrent seeder, and checks verified
beginning/middle/end HTTP ranges and restart/resume. The v1 magnet and v2 cases
resolve magnet metadata. Set `BITFLY_TEST_VLC` to the installed `vlc.exe` path to
additionally decode one second through the real HTTP URL and through a local-file
baseline, then require both decoded WAVs to match at 91,244 bytes. A launcher
test alone is not playback evidence. Tests need loopback TCP connections. The CI
integration run is allowed 300 seconds.

On Windows, for a targeted diagnostic run:

```powershell
$env:BITFLY_TEST_VLC = 'C:/Program Files/VideoLAN/VLC/vlc.exe'
./build/integration_tests.exe realLibtorrentStreamsRangesAndRestores:v1magnet -o build/integration-v1magnet.txt,txt
```

The local VLC-decode evidence was 6/6 in 144,725 ms across v1 torrent, v1 magnet,
v2 magnet, and hybrid torrent modes. That result is recorded in
[STATUS.md](STATUS.md); it is not reproduced by a launcher-only run.

To smoke-test a staged or installed package, run the application with a probe
path and a bounded lifetime:

```powershell
./BitFly.exe --startup-probe "$env:TEMP/bitfly-startup-probe.json" --quit-after 5000
```

Success requires exit code 0 within 30 seconds and a probe file containing a
non-negative `firstFrameMs`. This proves that the packaged application created
its window and rendered a first frame, then exited cleanly. It does not prove
streaming, VLC decoding or installer behavior. CI runs this check for both x64
and ARM64 immediately after packaging.

ARM64 requires native compilation, PE header validation, execution and a
packaged launch. CI performs these on `windows-11-arm`; physical ARM64 VLC
playback remains outstanding. See STATUS.md for checks that have actually run
and their results.

Before a stable release: run prolonged concurrent playback/cancellation,
restart/resume, disk-full and corrupted metadata scenarios; inspect error,
dense-content, loading and empty QML states; verify keyboard and screen-reader
behavior on Windows. Run clang-tidy/ASan where the selected toolchain supports
them, audit dependency licenses and validate installer/uninstall user-data
preservation. The lack of any of these checks must remain visible in status.
