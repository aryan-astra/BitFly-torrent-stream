# Validation status

Implementation and validation are in progress. This file is updated with actual
execution evidence before handoff. The application is a development preview;
an initial source push is not a stable release.

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
- Torrent engine syntax-only compile passed; full linking and local seeded
  integration/VLC decode tests remain pending.
- Native x64/ARM64 CI and portable/installer packaging configured; artifacts
  are not considered verified until actual build, test and PE validation succeed.

Still-required release gates include real VLC decode, metadata/seek/resume
integration, native architecture packages, clean package launch and installer
testing, sustained concurrency, disk-full/crash recovery, screen-reader review,
license audit, and performance measurements. There is no Authenticode signing
certificate configured for BitFly; Git signing does not sign binaries.
