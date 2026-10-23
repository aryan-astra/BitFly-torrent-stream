# BitFly interface design

BitFly is a compact Windows desktop utility for choosing torrent media and handing playback to VLC. The interface keeps the next action close to the stream and gives transfer state enough detail to explain why playback may be waiting.

## Visual language

The palette uses warm near-white surfaces, dark graphite navigation, quiet green as the playback action, and text labels alongside state colors. Segoe UI follows the Windows desktop, thin separators and small radii keep the interface restrained, and controls retain native Qt Quick focus and keyboard behavior. The navigation marks are drawn vector paths, and reusable colors, typography, and radius values live in `qml/Theme.qml`.

## Screen structure

- **Stream** accepts a magnet link or torrent file, then shows each real session with its state, progress, transfer rate, peers, buffer, and playable file selection. Play, queue, pause, and removal actions stay with that session.
- **Queue** shows the ordered play list and exposes move, remove, and play controls.
- **History** lets a user relaunch a prior stream.
- **Settings** controls the VLC executable, cache location and quota, seed duration, and update check.
- **Diagnostics** expands in the Stream view and reports actual cache usage and update text from the controller.

When playback ends, the user chooses Keep, Delete data, or Seed. The controller owns those session outcomes and the configured seed timer.

## Layout and accessibility

The window starts at 1080 × 720 and supports a compact 780 × 540 minimum. The left navigation remains fixed while the active page scrolls. Torrent names and file paths elide within their layout cells; session lists scroll instead of growing the window. Controls use Qt Quick Controls and remain keyboard focusable, visible labels explain icon-only navigation and queue actions, and focus changes are emphasized on the input field. Error states include text, and transfer state is shown by both a label and color.

The UI reads controller snapshots and invokes the app's public API; it does not invent session, queue, history, or diagnostic data. Playback progress is the controller's stream-buffer metric, not a VLC playback-position estimate.

## Validation limits

The QML source targets Qt 6.8.3 Quick, Controls, Layouts, and Dialogs. `qmllint` parses the QML without warnings. An offscreen Qt 6.8.3 harness loaded `Main.qml` with a contract-shaped mock controller and rendered empty, populated, queue, history, and settings states at 900 × 600; the Stream view was also rendered at 125%, 150%, and 200% scale. The Windows offscreen plugin needed `QT_QPA_FONTDIR` set to the system fonts directory to render Segoe UI. This validates the layouts with representative data; real controller integration, native file dialogs, keyboard focus traversal, and hardware DPI behavior still need a run in the packaged Windows app.
