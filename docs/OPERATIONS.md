# Privacy and troubleshooting

No telemetry, remote account or cloud backend is used. SQLite stores local
settings, queue and recent playback titles. libtorrent resume state can contain
torrent names, tracker URLs (possibly private passkeys), peer information and
paths. Treat the entire application data directory as private. Do not attach
raw databases/resume files to public issues. There is no log exporter yet.

The default per-user data directory is `%LOCALAPPDATA%\BitFly\BitFly`. It holds
`sessions.sqlite` and managed media/resume data under `cache\torrents`. Portable
builds use the same per-user location. A different cache folder selected in
Settings applies after restart and does not move or erase the previous folder.

BitTorrent exposes your IP address to peers and trackers. Downloading can also
upload verified pieces. Pause/remove sessions to stop their network activity.
The app does not promise anonymity. Public discovery is handled by libtorrent;
private torrent restrictions are preserved.

If VLC is missing, install it from VideoLAN and choose the installed executable
in Settings. A VLC process starting does not prove successful decoding. If it
opens but cannot play, inspect VLC's own messages and check swarm health.
BitFly cannot supply unavailable pieces or guarantee that all containers start
equally quickly. HTTP requests time out instead of filling gaps with bad data.

After interruption, relaunch the saved torrent explicitly. Exact playback time
is not restored. Keep data if you want another playback attempt. Cache folder
changes apply on restart and never move or erase the previous folder. Never
manually remove active session files. Disk quota pauses are recovered by freeing
space, raising a justified quota or removing an inactive cache through the app.

Updates check GitHub's latest stable release at every app launch and on manual
request; prereleases are ignored. New compatible installers download automatically
and are verified against GitHub's expected size and SHA-256. Install update applies
the verified installer to the running app's folder, after normal session-saving
shutdown; it does not require deleting active sessions. Installation remains a
visible action and registers that folder as a per-user installation, including
when launched from an extracted ZIP. An offline/API error does not prevent local
playback. Invalid/newer database schemas produce a visible error; back up your
data before downgrading versions.

In unreleased source builds, an invalid stored JSON setting also produces an
error and blocks further database writes. This preserves the damaged row and
other saved settings instead of overwriting them with defaults. Close BitFly and
back up `sessions.sqlite` and any remaining `-wal`/`-shm` sidecars before repairing
the database. No automatic reset or deletion is performed. The published
`v0.1.0-preview.1` binaries do not yet contain this safeguard.
