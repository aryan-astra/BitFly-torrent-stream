# BitFly interface design

## Direction

A small personal media shelf, on warm white paper. The interface combines the
user's portfolio preference for tactile detail with a compact desktop layout.
The previous graphite/amber dashboard direction was rejected.

`qml/Theme.qml` defines warm white `#fcfbf8`, ivory rail `#f4f2ed`, white cards,
dark ink `#282a30`, muted ink `#62646b`, and coral actions `#ad4739`. Sky and mint
appear in illustrations. Georgia gives headings a distinctive editorial voice;
Segoe UI keeps file names, paths, and controls readable without shipping fonts.
Radii are 8/12/18 px, transitions 140 ms, and the one-time cover unfold 420 ms.
There is no perpetual decorative animation or browser runtime.

## Navigation and layout

The 190 px sidebar contains Library, Queue, Search, and Settings, plus current
storage usage and external-player readiness. The default window is 1240 × 800;
940 × 640 is the minimum. Content scrolls inside the window and long paths wrap
or elide instead of pushing controls outside their rows.

The library starts with the magnet input and an Open .torrent action. Active
sessions expose name, state, transfer and buffer information, file selection,
Play, pause/resume, and an artwork/files menu. Playback is always external.
Opening a torrent does not itself open the media player.

Saved entries appear below active sessions in a virtualized GridView. Titles
and actions sit below the artwork, so the image is not obscured by gradients or
duplicated overlaid text. Retained files are bright; entries without local data
are dimmed and say that downloading is needed. Existence of files is not a claim
that they are fully verified or playable offline. Removing downloaded data keeps
the entry and artwork; removing the entry asks for confirmation.

`MediaCover.qml` starts as a small circular placeholder. Once its image loads,
it expands into a rounded square or a bounded landscape rectangle. Artwork has
bounded decoding, asynchronous loading, and shared image caching. Corner paint
keeps the effect compatible with Qt's software renderer without allocating a
second full-image masking texture. The reduced-motion preference replaces these
transitions and hover movement with immediate state changes.

Queue rows allow reordering, removal, and playback. Search uses a configured
Torznab feed from Jackett/Prowlarr; it never claims to search every torrent site.
The connection form has visible labels and an empty password field: saved API
keys are never sent back to the interface. Search has initial setup, busy/cancel,
results, no-results/status, and recoverable-message states. Results are virtualized.

## Settings

Compact groups cover Storage, Streaming, Network, Discovery, Playback,
Look & feel, Cloudflare connection, and Updates. Every setting has a focusable
question mark whose hover/focus tooltip explains the effect and limits.

- Folder changes apply to new sessions; active sessions keep their own roots.
- Read-ahead prioritizes data requests. It is not a rolling disk-space limit.
- Download/upload caps explicitly explain that zero means unlimited.
- DHT, local discovery, and optional port mapping are real engine controls.
- A supported external player can be selected; VLC remains the default.
- Video previews and reduced motion are optional.
- Cloudflare is a separately installed client with system-wide effects. The UI
  offers status, opening/downloading the official client, and explicit controls.
  DNS-only mode does not tunnel torrent traffic.
- Stable-update checks run at each app launch and download newer installers in
  the background. Download progress and installation have separate states.
  No install action is shown until the controller reports a verified installer.

## Accessibility and motion

Qt Quick Controls provide focus and keyboard semantics. Icon buttons have names
and tooltips; help works through keyboard focus as well as hover. Focus has a
visible coral outline. Transfer and availability states always have text.
Animations respond to actions, settle quickly, and use `Theme.reducedMotion`.
Sidebar icons gently tilt/lift on hover and compress on press; buttons compress
on press. The illustrations are decorative and ignored by assistive technology.

## Generated artwork

The built-in image-generation tool produced four transparent navigation assets:
`qml/assets/nav-library.png`, `nav-queue.png`, `nav-search.png`, and
`nav-settings.png`. Each is decoded at 64 px for its 32 px display size.
The user retained the original flat B monogram (`bitfly-mark.png`) and chose
a darker blend of soft cloudscape and paper-cloud art (`sky-clouds.png`).
The background
is decoded at a bounded 1280 px and the small mark at 84 px.

Shared final icon prompt: a single desktop sidebar navigation icon for BitFly;
premium simple tactile 3D illustration, subtly paper-ceramic material, soft
top-left studio lighting, restrained contact shading, ivory/coral/sky/mint,
front three-quarter view, clear chunky silhouette readable at 32 pixels,
centered single object occupying 80% of a square, fully transparent background,
no tile, text, letters, logo, or watermark. Subjects were respectively three
stacked media sleeves with an inset play triangle; a raised three-line playlist;
a magnifying glass with sky lens/coral handle; and a six-tooth ivory gear.

## Validation record

The `qml_smoke` harness uses clearly synthetic fixtures, loads the real QML, and
fails on runtime QML warnings. It checks navigation, stable session identity
across snapshot refreshes, file selection, saved-library counts, the cover
transition with reduced motion, and search query forwarding without secret
prefill. Image fixtures do not claim to represent live torrent downloads.

The final QML regression checks and offscreen visual review passed, including
minimum 940x640, 150% scale, focused help, cover transitions and a 1000-row shelf.
See STATUS.md and validation/v1 for evidence. Native dialogs, a screen-reader session, physical multi-monitor
DPI behavior, and actual GPU rendering remain separate checks; screenshots from
the offscreen software harness cannot establish those properties.

## 1.0.1 readability correction

Body controls use 14–15px text; metadata uses at least 12px. Primary and secondary
text are darker, and panels use a cool off-white rather than pure white. All
three main text colours exceed 4.5:1 against surface, raised and rail backgrounds.
Link controls have an opaque panel surface so sky imagery cannot reduce their
contrast. The original B mark and the user's cloud artwork are retained.
