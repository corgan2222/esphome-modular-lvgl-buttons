[← Back to main README](../../README.md)

# ui/media_player

Remote-only media player tile with full-screen detail page. Tile shows now-playing track info with a play-state indicator dot (green when playing, gray when idle). Short-click toggles play/pause. Long-press opens the detail page.

## Files

| File | Purpose |
|---|---|
| `remote.yaml` | Home Assistant media_player entity |
| `detail.yaml` | Full-screen playback UI — included automatically, do not include directly |
| `pin_lock.yaml` | Optional PIN lock — gates access to the detail page (planned) |

## Variables

| Variable | Required | Description |
|---|---|---|
| `uid` | ✅ | Unique identifier |
| `entity_id` | ✅ | HA entity string e.g. `"media_player.spotify"` |
| `row` | ✅ | Grid row position (0-based) |
| `column` | ✅ | Grid column position (0-based) |
| `text` | ✅ | Label shown on tile and detail page header |
| `icon` | ✅ | MDI glyph e.g. `$mdi_spotify` or `$mdi_music_note` |
| `row_span` | — | Number of rows to span (default: `1`) |
| `column_span` | — | Number of columns to span (default: `1`) |
| `page_id` | — | Parent page ID (default: `main_page`) |
| `pin_code` | — | 4-digit PIN to lock the detail page (default: none — detail page opens freely) |

## Usage

```yaml
# Basic — no PIN lock
spotify_tile: !include
  file: esphome-modular-lvgl-buttons/ui/media_player/remote.yaml
  vars:
    uid: spotify
    entity_id: "media_player.spotify"
    row: 0
    column: 0
    text: "Spotify"
    icon: $mdi_spotify

# With PIN lock on the detail page
kids_speaker: !include
  file: esphome-modular-lvgl-buttons/ui/media_player/remote.yaml
  vars:
    uid: kids_speaker
    entity_id: "media_player.kids_room"
    row: 0
    column: 1
    text: "Kids Room"
    icon: $mdi_speaker
    pin_code: "1234"
```

## Required glyphs

Add to your device `font:` block:

```
$mdi_chevron_left   $mdi_music_note   $mdi_play   $mdi_pause
$mdi_skip_previous  $mdi_skip_next    $mdi_shuffle
$mdi_repeat         $mdi_repeat_once  $mdi_volume_high
```

If using PIN lock, also add:

```
$mdi_lock   $mdi_backspace
```

## Detail page layout

- **Header** — back button (top-left) + title label (top-center)
- **Now playing card** — large circular icon (120×120) with glow shadow, track name and artist below
- **Progress bar** — horizontal bar showing playback position as percentage
- **Transport controls** — shuffle, previous, play/pause (large circular), next, repeat
- **Volume** — icon + horizontal slider at bottom

## PIN lock (planned)

When `pin_code` is set (e.g. `pin_code: "1234"`), long-pressing the tile opens a PIN entry page instead of the detail page. The PIN page shows:

- A lock icon and "Enter PIN" header
- 4 dot indicators (filled as digits are entered)
- A 3×4 numeric keypad (1-9, backspace, 0, confirm)
- Auto-submits when the 4th digit is entered

On correct PIN entry, the detail page opens. On incorrect entry, the dots flash red and reset. The PIN re-locks automatically when the user navigates back from the detail page.

When `pin_code` is omitted or empty, the long-press opens the detail page directly — no changes to existing behavior.

## Notes

- This is a **remote-only** entity type — there is no `local.yaml`. Media players are inherently Home Assistant entities.
- The tile label switches to the current track title while playing, and reverts to the configured `text` when idle.
- Volume changes are debounced (500ms) to avoid flooding HA with rapid updates while dragging the slider.
- Progress is tracked locally (incremented every second while playing) and re-synced from HA when the `media_position` attribute updates.
