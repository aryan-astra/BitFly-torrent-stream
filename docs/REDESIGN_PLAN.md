# BitFly v1 work record

Updated 2026-10-26. This is a working checklist, not a release announcement.
Existing OpenCode changes were retained and audited. The original dark design
was rejected; the white tactile direction below replaces it.

## Accepted product direction

- C++20 / Qt Quick; no Electron, embedded browser, or embedded video player.
- Warm white paper, restrained coral/sky/mint, original B-monogram branding,
  soft 3D navigation images, expressive Georgia headings and readable controls.
- Compact comfortable layout: Library, Queue, Search, Settings sidebar;
  magnet input and active streams above saved media.
- Circle placeholders unfold into square/landscape covers on image readiness.
  Hover/press motion is brief; respect reduced-motion preference.
- Priorities: low RAM, visual richness, installed size, startup. Baseline 8GB RAM.
- VLC remains default; allow installed mpv and MPC-HC/BE with dedicated instances.

## Phase 1 — engine reliability and storage

- [x] Implement stable-ID retained/purged torrent reopen.
- [x] Preserve canonical magnets, including trackers, for library relaunch.
- [x] Restore active sessions from their original download root after root changes.
- [x] Wait for asynchronous torrent removal before cleaning owned directories.
- [x] Defer session teardown until the owned alert batch is consumed.
- [x] Remove unsafe sparse-Merkle torrent serialization; persist magnet fallback.
- [x] Replace unmeasured aggressive network overrides with supported controls.
- [x] Separate requested verified disk reads from the network read-ahead window.
- [x] Finish corrupt-v2-resume metadata fallback regression and final full run.
- [x] Verify controller applies folder changes only after engine acceptance.

New folders affect new downloads; active files keep their original location.
Rolling deletion of watched pieces is not implemented: priority 0does not
invalidate verified storage. Safe custom storage requires separate engineering.
Upload 0means unlimited, and maximum throughput without reciprocity is not
promised. Default post-play seeding is 0and requires an explicit Seed action.

## Phase 2 — library, artwork, and interface

- [x] Implement persistent magnets/selection and remove silent history caps.
- [x] Derive owned artwork filenames from hashed IDs; atomically replace PNGs.
- [x] Retain artwork when payload is removed; delete it after entry removal succeeds.
- [x] Add local custom images and bounded one-at-a-time installed-VLC frame capture.
- [x] Implement white UI, generated background/branding/navigation, compact help rows.
- [x] Virtualize saved covers and search results; bound decoded image dimensions.
- [x] Add preferred external player and reduced-motion controls.
- [x] Compile controller integration and verify persistence/artwork regression tests.
- [x] Verify real HTTP-to-VLC preview capture and cancellation.
- [x] Render and inspect populated/empty/search/settings/queue, minimum size and DPI.
- [x] Verify tooltip focus, selection, cover morph, and a 1000-entry virtualized shelf.

## Phase 3 — optional integrations

- [x] Implement Torznab search, magnets and same-origin torrent downloads.
- [x] Encrypt persisted search keys with Windows account DPAPI; sanitize feed URLs.
- [x] Implement installed Cloudflare client status, explicit connection/mode controls.
- [x] Implement per-launch stable update checks and background verified downloads.
- [x] Require explicit installer action after active sessions are stopped.
- [x] Pass local HTTP/parser/digest/redirect/cancellation/tampering service tests.
- [x] Confirm packaged UI resources and all optional settings reflect real state.

Search requires a user-configured Jackett/Prowlarr Torznab feed; it does not
search every site. Cloudflare remains a separate official installation because
redistribution rights have not been established. DNS-only mode does not tunnel
media traffic. Development does not change the host's active network connection.
Update integrity uses the official HTTPS GitHub API digest, not an independent
publisher signature. See INTEGRATIONS.md for trust boundaries and primary sources.

## Phase 4 — local validation and release preparation

- [x] Build all targets serially with the matching Qt/MinGW 13 runtime.
- [x] Pass HTTP, unit, lifecycle, normal swarm and damaged-resume/quota recovery tests.
- [x] Pass controller, service, preview and QML regression checks.
- [x] Measure isolated rendered startup/idle memory and CPU; state what it excludes.
- [x] Build/package x64 and smoke-test the staged executable/resources/installer.
- [x] Reconcile architecture/contracts/status/changelog with observed results.
- [ ] Create signed commits and inspect their complete diff.

Publication follow-up (outside the active local-only goal): fresh x64/ARM64 CI
and a stable/latest GitHub release with installers, ZIPs and checksums.

The user requested a normal v1 release, but the active goal explicitly retains
local-only commits and prohibits publishing. The candidate is prepared locally. Do not overwrite the preview tag or mislabel old
binaries as v1. Publication must use the new verified source/artifacts.

## Follow-up features

FEATURE_RESEARCH.md contains 25 researched candidates. Highest-value follow-ups:
explicit live-download moves, per-session bandwidth controls, richer swarm
health, subtitle-file pairing, and storage forecasts. None is silently advertised
as implemented. A rolling disk cache needs a separate verified storage design.

## Development tools

scripts/test-local.ps1 pins SDK plugin/runtime paths and disables Qt command-line
GUI help boxes. It prevents staged windows-only plugins being used for offscreen
tests. scripts/make-icon.py deterministically builds all six original B-monogram ICO sizes.
Original generated artwork stays in the Codex image output; optimized project
assets and prompt provenance are recorded in DESIGN.md.
