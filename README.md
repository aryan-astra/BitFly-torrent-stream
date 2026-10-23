# BitFly

A native Windows application for streaming torrent media in your installed VLC
Media Player. Open a magnet link or a `.torrent` file, select media, and let VLC
request verified bytes while libtorrent downloads the pieces it needs.

**Development preview, not a certified production release.** See
[validation status](docs/STATUS.md) for executed checks, artifacts and gaps.

Targets: Windows 10 1809+ x64, Windows 11 x64, and Windows 11 ARM64. Each target
requires its own validation. VLC is required and is not silently installed or
bundled. Install it from [VideoLAN](https://www.videolan.org/vlc/), or select your
existing `vlc.exe` in Settings.

## Use

Paste a magnet link on the Stream page, or choose **Open torrent file**. Choose
a playable file and press Play. Preparing means required pieces are still being
retrieved. VLC opens when the initial verified range is available. Seeking
changes HTTP byte demand and reprioritizes pieces. Performance depends on swarm
availability, network conditions, file layout and media bitrate; unavailable
pieces cannot be manufactured. Containers with metadata at the end may need
additional buffering.

Queue files for the next VLC session; reorder or remove them on the Queue page.
Closing an owned VLC instance advances the queue, even if playback was closed
early. History records launch attempts that successfully created a VLC process;
it does not prove that VLC decoded the media. Exact VLC playback positions are
not collected or invented. Restart restores torrent resume state, queue and
history; playback requires an explicit launch.

After VLC exits, choose Keep, Delete or temporary seeding. Default seeding lasts
15 minutes and can be changed in Settings. Keep pauses network activity. Delete
requires a confirmation in the interface and affects only that managed cache.
Close a session's VLC window before removing it. Cache data is stored under
your Windows user profile, including for portable builds. Settings can select a
different cache folder (effective after restart); existing data is not moved.

BitTorrent shares your network address with swarm participants and can upload
downloaded pieces. Use media you are authorized to access and share. BitFly has
no torrent search service, content catalog, telemetry or accounts. Diagnostics
and history remain local. See [privacy and troubleshooting](docs/OPERATIONS.md).

## Development

See [build instructions](docs/BUILD.md), [architecture](docs/ARCHITECTURE.md),
[interface contracts](docs/CONTRACTS.md), [design system](docs/DESIGN.md),
[testing](docs/TESTING.md), [release process](docs/RELEASE.md),
[third-party notices](THIRD_PARTY_NOTICES.md), and [contributing](CONTRIBUTING.md).

No binary or screenshot should be taken as validation of a platform unless it is
listed as executed in the status report. See [releases](https://github.com/aryan-astra/BitFly-torrent-stream/releases)
for published artifacts when available.
