# Testing

Build with BUILD_TESTING=ON (default). `ctest --test-dir build --output-on-failure`
runs range/protocol, scheduler/path, version and SQLite persistence tests.
Tests use local deterministic data. No public swarm is needed for unit/protocol
tests. The Qt application must also be rendered: `BitFly.exe --screenshot path.png`
captures an actual implemented window and exits with a failure code if capture
fails. Set QT_SCALE_FACTOR to 1, 1.25, 1.5 and 2 for DPI review.

The integration suite generates an 8 MiB PCM WAV and v1, v2 and hybrid torrents,
starts a separate throttled loopback libtorrent seeder, and checks verified
beginning/middle/end HTTP ranges and restart/resume. The v1 magnet and v2 cases
resolve magnet metadata. Set `BITFLY_TEST_VLC` to the installed `vlc.exe` path to
additionally decode one second through the real HTTP URL and through a local-file
baseline, then require both decoded WAVs to match at 91,244 bytes. A launcher
test alone is not playback evidence. Tests need loopback TCP connections.

On Windows, for a targeted diagnostic run:

```powershell
$env:BITFLY_TEST_VLC = 'C:/Program Files/VideoLAN/VLC/vlc.exe'
./build/integration_tests.exe realLibtorrentStreamsRangesAndRestores:v1magnet -o build/integration-v1magnet.txt,txt
```

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
