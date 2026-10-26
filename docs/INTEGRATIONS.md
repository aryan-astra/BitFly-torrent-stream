# Search, Cloudflare, and updates

These integrations use Qt's existing network/process APIs. BitFly does not add a
browser engine, a Python plugin runner, a .NET service, or an embedded VPN stack.
The service code is implemented; controller/QML integration and final validation
are tracked in `REDESIGN_PLAN.md`.

## Torrent search

`SearchService` accepts a configured Torznab feed from an installed Jackett or
Prowlarr instance and its API key. The aggregator maintains the individual
indexers; BitFly reads its standard RSS/XML search response. A user's chosen
aggregator/feed controls which sites are searched. This cannot search every
torrent on the Internet or bypass a source's account, availability, or access
requirements.

- Jackett: copy a Torznab feed URL from its dashboard. Its aggregate feed can
  search configured indexers; individual feeds also work.
- Prowlarr: use an indexer's Torznab feed URL, not the REST `/api/v1/search`
  endpoint. The service implements Torznab, not Prowlarr's JSON API.
- Local HTTP is accepted only for `localhost` or a loopback IP. Remote feeds
  require HTTPS. Credentials in URL user-info are rejected. API keys are sent
  only to the configured origin and are never logged by the service.
- Results are capped at 100 and 2 MiB. Queries time out after 30 seconds. DTDs,
  invalid feeds, and external download redirects are rejected.
- Valid magnets go directly to the existing torrent engine. Torrent metadata
  may be downloaded from the configured origin, capped at 8 MiB, and saved in an
  owned temporary directory. The engine still validates the torrent itself.
- No executable third-party search plugins are loaded into BitFly. Search is
  optional: magnet and `.torrent` input remain available without an aggregator.

Sources: [Jackett API](https://github.com/Jackett/Jackett/wiki/Jackett-API),
[Jackett project and query syntax](https://github.com/Jackett/Jackett),
[Prowlarr search documentation](https://wiki.servarr.com/en/prowlarr/search).

## Cloudflare client

`NetworkAssistant` bridges an independently installed official Windows client.
It locates `warp-cli.exe` in the standard Cloudflare Program Files directory,
reads status asynchronously, and exposes explicit connect/disconnect and mode
actions. It never registers a device, accepts terms, changes mode, or connects
merely because BitFly starts. These commands affect the device's networking,
not only BitFly; organization policies can prohibit them.

The local installed client's `--help` and `mode --help` were inspected on
2026-10-26. Its accepted modes are `doh`, `dot`, `warp`, `warp+doh`, `warp+dot`,
`tunnel_only`, and `proxy`. Invalid strings never reach the process. Processes
have a 10-second timeout and bounded output; no shell is invoked. The only live
command used during development was read-only `status`, which reported an
existing healthy connection. No connect/disconnect/mode change was tested on
the developer's active connection.

DNS-only mode encrypts DNS requests; it does not tunnel torrent traffic. Traffic
modes route traffic according to the client's configuration and policies. They
do not guarantee higher torrent speeds. A proxy mode also requires the torrent
client to use that proxy; selecting it alone does not route BitFly's traffic.

The Cloudflare binary is not bundled. Its published application terms grant a
personal, non-transferable use license and reserve rights outside that grant;
they do not establish permission for BitFly to redistribute the client. The app instead
opens Cloudflare's official download/setup page, or its installed GUI for
registration and policy settings. A separate installed service also has its own
RAM/disk usage, which must not be excluded from claims about a bundled product.

Sources: [official client architecture and CLI](https://developers.cloudflare.com/cloudflare-one/team-and-resources/devices/cloudflare-one-client/),
[official Windows setup](https://developers.cloudflare.com/warp-client/get-started/windows/),
[mode descriptions](https://developers.cloudflare.com/warp-client/warp-modes/),
[official releases](https://developers.cloudflare.com/cloudflare-one/team-and-resources/devices/cloudflare-one-client/download/),
[application terms, sections 2–3](https://www.cloudflare.com/application/terms/).

## Application updates

`UpdateService::check()` checks GitHub's stable `releases/latest` endpoint. The
controller calls it once on every process startup and for manual checks; there
is no persisted hourly throttle. Concurrent requests are coalesced. Draft and
prerelease versions never become installation candidates.

The service selects the exact x64 or ARM64 setup filename for a newer stable
semantic version. Its download URL must identify this repository's release.
The GitHub API must supply the asset's SHA-256 digest and expected size. Without
those fields, the app offers the official release page instead of installing.

When a newer compatible release is found, the controller downloads it in the
background once per version per app run. Failed downloads can be retried with
the download action. Installation still requires an explicit action. The service
streams to a `QSaveFile`
without buffering the installer in RAM, allows only HTTPS redirects to specific
GitHub asset hosts, caps size at 256 MiB, and checks both size and SHA-256 before
committing. Progress notifications are limited to ten per second. It rechecks
the saved installer immediately before launch. One
cached installer per architecture is retained instead of accumulating versions.
On later launches, a cached installer matching the release's size and digest is
reused instead of downloaded again.
The installer runs visibly with `/CURRENTUSER /NORESTART`; BitFly must stop its
owned streams/processes cleanly before exiting for installation. No unattended
overwrite or forced shutdown is performed by the service.

This verifies integrity against the official HTTPS GitHub API. It is **not** an
independent publisher-signature verification scheme: the application currently
has no embedded release-signing key or Authenticode publisher identity. A
compromised release account remains a trust risk. Future silent, unattended
installation should require a separately managed signing identity and a tested
rollback/restart path. No new signing trust is implied by local signed Git
commits or by the checksum file shipped alongside releases.

Source: [GitHub release assets API and digest field](https://docs.github.com/en/rest/releases/assets#get-a-release-asset).

## Validation

`services_tests` exercises stable-version and exact-asset selection, digest and
URL rejection, feed limits and DTD rejection, result parsing, a real local HTTP
search, encoded queries, magnet activation, provider-origin redirects, explicit
cancellation, installer tampering before launch, and the CLI mode allowlist.
It does not change the host network,
download or execute a production update, or require a running aggregator.

Validated locally on Windows 11 with the pinned Qt 6.8.3 / GCC 13.1.0 kit:
`services_tests` passed 11 checks with zero failures or skips (837 ms). The final
serial target build succeeded. Development logs are under `.tools/` and are not
release artifacts. The installed Cloudflare CLI's read-only help/status commands
were also verified; connection and mode changes were intentionally not exercised
on the developer's active network.
