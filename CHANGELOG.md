# Changelog

All notable user-visible changes are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow
[Semantic Versioning](https://semver.org/spec/v2.0.0.html). The first stable release is being prepared as 1.0.0.

## 1.0.2 — local automatic-preview reliability fix

- Prefetch the end of MP4/MOV containers and include it in startup readiness.
- Preserve pending verified reads across overlapping playback/preview seeks.
- Allow 45 seconds per capture attempt and retry as download data advances,
  with increasing backoff rather than stopping permanently after one retry.
- Show generating/waiting/paused/unavailable preview status on torrent cards.
- Include the application version in the window title.
- Add partial H.264 MP4, interrupted-capture recovery and overlapping-read tests.

## 1.0.1 — local thumbnail and readability fix

- Prepare video previews when metadata loads without launching the player.
- Sample 30–90 seconds into video, reject black/blank frames, fall back after
  failed seeks and retry once after buffering. Existing custom artwork is kept.
- Enlarge small text, darken secondary labels, soften panels and give links
  an opaque surface for contrast over the sky background.
- Verify the full local-swarm-to-controller-to-VLC-to-saved-PNG path, black
  intros, later seeks and past-end fallback.

## 1.0.0 — release candidate (not yet published)

### Added

- White tactile interface, original B-monogram icon, illustrated sidebar, compact
  explained settings, reduced motion and animated media covers.
- Persistent relaunchable media library, retained artwork, custom images and
  optional previews through installed VLC.
- Preferred external VLC/mpv/MPC player and optional Torznab search integration.
- Installed Cloudflare client controls without bundling or startup reconfiguration.
- Stable update checks on each launch, background size/hash-verified downloads
  and explicit installer action.

### Changed

- New downloads can use another selected folder while existing sessions retain
  their storage paths across restarts.
- Bounded stream scheduling and verified reads avoid speculative cache churn.
- Post-play seeding defaults to 0 and requires an explicit choice.


### Fixed

- Refuse writes after detecting a malformed saved setting, preserving queue,
  history and preferences instead of overwriting them with fallback values.
- Preserve the SQLite schema version when database initialization fails.

## 0.1.0-preview.1 - 2026-10-23

First public development preview, published as a GitHub prerelease for Windows
x64 and Windows 11 ARM64. Binaries are unsigned.

### Added

- Native C++20/Qt Quick application with a compact Stream, Queue, History,
  Settings, and Diagnostics interface.
- Magnet-link and `.torrent` input, including BitTorrent v1, v2, and hybrid
  torrents, with playable-file selection.
- libtorrent 2.0.11 streaming worker with range-driven piece deadlines and a
  bounded verified-piece cache.
- Token-protected loopback HTTP byte-range server used by external VLC.
- Owned external VLC processes, multiple sessions, pause/remove controls, and
  queue advancement when an owned VLC process exits.
- Persistent queue, recent history, preferences, and atomically written torrent
  resume state in local SQLite storage.
- Managed cache quota admission, retention, user-confirmed deletion, and bounded
  temporary seeding after playback.
- Native x64 and ARM64 Windows CI builds, PE architecture validation, portable
  ZIPs, per-user Inno Setup installers, SHA-256 manifests, and packaged-app
  first-frame smoke tests.
- GPG-signed source history and signed release tag.

### Known limitations

- Physical Windows 11 ARM64 VLC playback and Windows 10 hardware validation have
  not been performed.
- The streaming evidence uses one local seeder and one PCM WAV fixture; public
  swarms, sustained concurrent playback, and long-run behavior are not proven.
- Binaries have no Authenticode signature and no stable security-maintenance
  commitment.
- Exact VLC playback positions are not tracked or restored, and there is no
  in-app torrent search, catalog, subtitle discovery, or DRM circumvention.

See [docs/STATUS.md](docs/STATUS.md) for executed evidence and the remaining
release gates.
