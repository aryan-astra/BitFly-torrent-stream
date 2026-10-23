# Testing

Build with BUILD_TESTING=ON (default). `ctest --test-dir build --output-on-failure`
runs range/protocol, scheduler/path, version and SQLite persistence tests.
Tests use local deterministic data. No public swarm is needed for unit/protocol
tests. The Qt application must also be rendered: `BitFly.exe --screenshot path.png`
captures an actual implemented window and exits with a failure code if capture
fails. Set QT_SCALE_FACTOR to 1, 1.25, 1.5 and 2 for DPI review.

Acceptance still requires a generated-media local torrent seeded from another
libtorrent session, verified HTTP bytes through beginning/middle/end seeks,
and actual VLC decode evidence. Do not confuse a launcher test with playback.
ARM64 needs native compile, PE header validation, execution and packaged launch.
See STATUS.md for checks that have actually run and their results.

Before a stable release: run prolonged concurrent playback/cancellation,
restart/resume, disk-full and corrupted metadata scenarios; inspect error,
dense-content, loading and empty QML states; verify keyboard and screen-reader
behavior on Windows. Run clang-tidy/ASan where the selected toolchain supports
them, audit dependency licenses and validate installer/uninstall user-data
preservation. The lack of any of these checks must remain visible in status.
