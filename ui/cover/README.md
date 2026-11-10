[← Back to main README](../../README.md)

# ui/cover

Rolling shutter / blind / gate / garage door tile with full-screen detail page. Detail page has up / stop / down buttons, an optional "my position" preset button, and — for covers that report position — a live position slider.

"Statefull" (position-reporting) vs "stateless" is **detected at runtime**: from the HA `supported_features` bitmask (remote) or the ESPHome cover component's traits (local) — no vars needed.

- **Statefull** covers get a percentage readout + draggable position slider on the detail page, and the tile shows the percentage and colors itself open/closed accordingly.
- **Stateless** covers (e.g. `assumed_state: true` shutters/gates) only get up/stop/down — no position is knowable, so the tile stays blank and permanently neutral grey rather than guessing OPEN/CLOSED from an assumed state.

The my-position button is independent of statefull/stateless — it only appears if you configure `my_position_target`.

## Files

| File | Purpose |
|---|---|
| `local.yaml` | ESPHome cover component (defined in the same device YAML) |
| `remote.yaml` | Home Assistant cover entity |
| `detail.yaml` | Shared detail page — included automatically, do not include directly |

## Variables

### Common (both variants)

| Variable | Required | Description |
|---|---|---|
| `uid` | ✅ | Unique identifier |
| `entity_id` | ✅ | ESPHome cover component ID (local) or HA entity e.g. `"cover.living_room_shutter"` (remote) |
| `row` | ✅ | Grid row position (0-based) |
| `column` | ✅ | Grid column position (0-based) |
| `text` | ✅ | Label shown on tile and detail page header |
| `icon` | ✅ | MDI glyph e.g. `$mdi_window_shutter` |
| `row_span` | — | Number of rows to span (default: `1`) |
| `column_span` | — | Number of columns to span (default: `1`) |
| `page_id` | — | Parent page ID (default: `main_page`) |
| `my_position_target` | — | "My position" preset target. Local: an ESPHome script ID. Remote: an HA entity (script/scene/button/...). Omit to hide the button. |
| `invert_position` | — | Remote only. Set `"true"` if this entity's `current_position` attribute reports 100=closed/0=open instead of HA's standard 0=closed/100=open. Some cover integrations don't follow the standard. Default `"false"`. |

Up/stop/down always drive the main `entity_id` via native `cover.open`/`cover.close`/`cover.stop` (local) or `cover.open_cover`/`cover.close_cover`/`cover.stop_cover` (remote) — this covers both real position-reporting covers and assumed-state ones (e.g. a `cover.template` backed by relay pulses or RF scripts), since HA still exposes those as a normal `cover.*` entity. Only `my_position_target` can point at a different entity, since a "my position" preset is inherently a script/scene/button, not a cover.

## Usage

```yaml
# Remote — unified HA cover entity, statefull, with a my-position script
bedroom_blind: !include
  file: esphome-modular-lvgl-buttons/ui/cover/remote.yaml
  vars:
    uid: bedroom_blind
    entity_id: "cover.bedroom_blind"
    row: 0
    column: 0
    text: "Bedroom Blind"
    icon: $mdi_blinds
    my_position_target: "script.bedroom_blind_my"

# Remote — stateless (assumed_state) cover, e.g. an RF-driven gate exposed
# as a cover.template entity, no my-position
garage_door: !include
  file: esphome-modular-lvgl-buttons/ui/cover/remote.yaml
  vars:
    uid: garage_door
    entity_id: "cover.garage_door"
    row: 0
    column: 1
    text: "Garage"
    icon: $mdi_garage

# Local — ESPHome cover component (e.g. time_based, template)
shutter: !include
  file: esphome-modular-lvgl-buttons/ui/cover/local.yaml
  vars:
    uid: shutter
    entity_id: living_room_shutter   # your cover: component id
    row: 1
    column: 0
    text: "Living Room"
    icon: $mdi_window_shutter
    my_position_target: living_room_shutter_my   # your script: id
```

## Required glyphs

This project doesn't auto-include glyphs — every device declares its own `mdi_icons_40` font with an explicit `glyphs:` list, and a missing glyph just renders blank (no compile error). Two separate sources need glyphs:

**1. The tile icon** — whatever you pass as the `icon` var (e.g. `$mdi_window_shutter`, `$mdi_blinds`, `$mdi_garage`, `$mdi_gate`). Visible in your device YAML, easy to remember.

**2. The detail page icons** — hardcoded inside `detail.yaml` itself, invisible from your device YAML, easy to forget:

| Detail page element | Glyph |
|---|---|
| Back button (top-right) | `$mdi_chevron_left` |
| Up button | `$mdi_arrow_up_bold` |
| Stop button | `$mdi_pause` |
| Down button | `$mdi_arrow_down_bold` |
| My-position button | `$mdi_star` |

Add both sets to your device `font:` block:

```yaml
font:
  - file: 'https://github.com/Templarian/MaterialDesign-Webfont/raw/v7.4.47/fonts/materialdesignicons-webfont.ttf'
    id: mdi_icons_40
    size: 40
    bpp: 8
    glyphs:
      # detail page (always required)
      - $mdi_chevron_left
      - $mdi_arrow_up_bold
      - $mdi_pause
      - $mdi_arrow_down_bold
      - $mdi_star
      # your tile icon(s)
      - $mdi_window_shutter
```

## Detail page layout

The detail page shows one of two mutually exclusive control layouts, switched automatically by the statefull/stateless detection — never both at once.

**Statefull** (position known):
- **Position label** — small percentage readout above the slider
- **Position slider** (vertical, centered, fills most of the available height) — drag and release to set an exact position. Knob position tracks openness normally (top = 100%/open, bottom = 0%/closed), but the filled portion represents coverage: anchored at the top, growing downward as it closes, fully white and fully extended to the bottom at 0%/closed
- **My-position button** — to the right of the slider, vertically centered on it (mirrors the stateless layout's stop+my relationship); only shown if `my_position_target` is configured
- No up/stop/down — the slider is the sole control

**Stateless** (no position feedback):
- **Up** — top, horizontally centered
- **Stop** — center, aligned on the same vertical axis as Up/Down
- **My-position button** — to the right of Stop; only shown if `my_position_target` is configured (doesn't shift Stop when hidden — it's positioned independently, not via a shared flex row)
- **Down** — bottom, horizontally centered

**Both modes:**
- **Top-right button** — back to parent page

## Notes

- Statefull detection: remote reads bit 4 (`SET_POSITION`) of the HA `supported_features` attribute; local reads `get_traits().get_supports_position()` from the ESPHome cover component.
- My-position works in both modes, gated only by whether `my_position_target` is configured — but it's rendered by two separate widgets internally (one beside Stop for stateless, one beside the slider for statefull) since only one control layout is ever visible at a time.
- **Statefull tiles**: the top-right label mirrors the position slider's percentage, and the whole tile (background/icon/label) colors itself grey when fully closed (position 0) and idle, on-color otherwise.
- **Stateless tiles**: the top-right label is left blank and the tile stays permanently neutral (`$button_off_color` / `$icon_off_color` / `$label_off_color`) — position is fundamentally unknown, so the UI doesn't guess OPEN/CLOSED from an assumed HA state.
- **Remote statefull position tracking**: `${uid}_current_position` is driven exclusively by the `current_position` attribute sensor, normalized to 0=closed/100=open at ingest via `invert_position` (and de-normalized back to the device's native convention when sending `cover.set_cover_position`) — every other consumer (tile coloring, slider) always sees the normalized value and never has to know about the device's actual convention. The coarse open/closed domain-state guess (used to approximate position for stateless covers) is gated to stateless covers only — on a statefull cover it would otherwise overwrite the real position with 100/0 every time HA reports `"opening"`/`"closed"` mid-transit, making the slider jump to the wrong spot independently of the actual position.
