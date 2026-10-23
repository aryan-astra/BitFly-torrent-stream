# Security policy

BitFly is a development preview. No stable version has a supported security
maintenance commitment yet. Do not publish release claims before the validation
gates in docs/RELEASE.md pass.

Report vulnerabilities through the repository's private GitHub security advisory
flow when enabled. If it is unavailable, contact the maintainer using their
GitHub profile before sending exploit details. Do not post private torrents,
credentials or personal paths in public issues.

Security boundaries: untrusted torrent metadata, libtorrent peers/trackers,
loopback HTTP, managed cache roots, VLC executable selection, SQLite persistence
and update responses. HTTP routes have unpredictable bearer tokens and never
accept filesystem paths. Local processes with access to your account can still
inspect your process, tokens and files; BitFly does not claim isolation from a
compromised local account. Updates only notify and link to official releases.
