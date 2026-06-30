# Clawdmeter — animated Claude usage display

An animated pixel-art creature plus a stats panel for ESPHome/LVGL displays that
visualises your **Claude usage**, fed from **Home Assistant**. A feature of the
fork [**corgan2222/esphome-modular-lvgl-buttons**](https://github.com/corgan2222/esphome-modular-lvgl-buttons).
Requires **ESPHome 2026.6.0+**.

The creature animates on-device while a stats panel renders session/weekly usage,
extra credits, reset clocks, burn rate, time-to-100 %, an ETA clock, a Runway
verdict and a colour-coded pace frame. Layouts ship for portrait, landscape and
square panels from 320×240 up to 800×1280.

The layout is fully resolution-agnostic: the page splits into a top creature
region and a bottom stats panel, both sized by percentage. The engine measures
the creature region at boot and fits a square pixel-art canvas into it, so the
creature scales correctly on every supported display and both orientations — no
hardcoded pixels.

## Data source: the ha-clawdmeter integration

Clawdmeter reads everything from the Home Assistant integration
[**corgan2222/ha-clawdmeter**](https://github.com/corgan2222/ha-clawdmeter). The
integration polls the Anthropic usage API (default every **300 s**, configurable
60–3600) and exposes **~29 entities per Claude account** under a single HA device
named `Claude <name> (<plan>)`, slug `claude_<name>_<plan>`.

It provides **both** the raw usage values **and** every derived metric,
precomputed server-side:

- **Raw** — session / weekly / extra usage %, extra usage credits, session and
  weekly reset timestamps (ISO-8601 UTC).
- **Derived** — burn rate (5 min / 30 min / per-minute), usage rate, time-to-limit,
  limit ETA, runway pace + margin, a "limit reached before reset" binary, a
  `pace_frame` enum, an `animation_group` enum and `session_reset_in`.

**The ESP is render-only.** It reads these precomputed entities and draws them; it
does not derive burn rate, time-to-100 % or the Runway verdict itself.

## Examples

Ready-to-flash device configs live in
[`example_code/clawdmeter/`](../../example_code/clawdmeter/). Copy the closest
one, set `ha_account` to your integration's device slug in the `substitutions:`
block, pick a language pack, and flash:

| Example | Layout |
|---|---|
| [`grid/…-jc4880p443.yaml`](../../example_code/clawdmeter/grid/guition-esp32-p4-jc4880p443.yaml) | Modular grid tiles, **landscape** (creature left, stats right) — incl. the [pace frame](#pace-frame-pace_frameyaml) |
| [`grid/…-jc4880p443-portrait.yaml`](../../example_code/clawdmeter/grid/guition-esp32-p4-jc4880p443-portrait.yaml) | Modular grid tiles, **portrait** (creature top, stats below), `design: modern` — **the live device**. Incl. the [pace frame](#pace-frame-pace_frameyaml) |
| [`all-in-one/…-jc4880p443.yaml`](../../example_code/clawdmeter/all-in-one/guition-esp32-p4-jc4880p443.yaml) | All-in-one full-screen `remote.yaml` |

See [Minimal include](#minimal-include) for the compact `remote.yaml` build,
[Designs and multi-format layouts](#designs-and-multi-format-layouts) for the
`classic`/`modern` page-1 looks and the nine modern formats, and
[Architecture](#architecture) for how the pieces fit together.

## All-in-one vs. modular grid

There are two ways to assemble the Clawdmeter. Pick by the trade-off between *how
little you wire* and *how much it shows*.

| | **All-in-one** (`remote.yaml`) | **Modular grid** (individual tiles) |
|---|---|---|
| **Include** | one super-include (`uid: clawd`) | the grid tile includes (`engine.yaml` once, then `creature.yaml`, `stats_panel.yaml`, `session_reset_clock.yaml`×2, `pace_frame.yaml`, …) plus [`render_from_integration.yaml`](render_from_integration.yaml) for the data binding |
| **Layout** | fixed two-region split — creature on top, stats below — sized by **percentage**, so it fits any resolution and both orientations with no cell wiring | explicit `layout: 4x4` grid; **you** place each tile with `row`/`column`/`*_span` → one config per orientation (landscape **and** portrait variants) |
| **HA inputs** | thin **5-entity** [data contract](#home-assistant-data-contract): session/weekly % + two reset timestamps + a status line | one **`ha_account`** slug → six raw `ha_*` inputs **plus** all precomputed metrics, wired by [`render_from_integration.yaml`](render_from_integration.yaml) |
| **What it shows** | creature + session/weekly usage bars + reset countdowns + status | everything in all-in-one **plus** burn rate (5 m/30 m), time-to-100 % + ETA clock, the Runway verdict, the breathing [pace frame](#pace-frame-pace_frameyaml), extra-usage bar and an animation picker |
| **Rendered on device** | the creature animation (from `session_pct`) | the creature animation (from `session_pct`); every metric — burn rate, time-to-100 %, ETA, Runway verdict, pace band — arrives **precomputed from the integration** and is mirrored onto the layout ids by [`render_from_integration.yaml`](render_from_integration.yaml) |
| **Square displays** | optional 2-page split via `stats_page_id` (creature page + stats page) | — |
| **Best for** | the quickest drop-in: one include, any resolution, both orientations, minimal HA wiring | a full dashboard with every metric, the on-display Runway hit-card and the colour-coded pace frame |

The Runway hit-card and the pace frame ship **only** on the grid builds — the
all-in-one layout doesn't render a pace band. The live device is the grid-portrait
example.

## Designs and multi-format layouts

The grid build's **page-1 look** is chosen at **compile time** by a single
`design:` substitution in the device config — the build pulls the matching
layout file via a dynamic include:

```yaml
substitutions:
  design: modern   # classic | modern | modern_landscape | modern_square | modern_720 | modern_1280 | modern_320x240 | modern_320x480 | modern_480x272 | modern_720x1280

packages:
  clawd_layout: !include
    file: esphome-modular-lvgl-buttons/ui/clawdmeter/layout_${design}.yaml
```

Two design families:

- **`classic`** (`layout_classic.yaml`) — the original creature-on-top,
  stats-panel-below look described above.
- **`modern`** — a dark "card" redesign that ships in **nine pixel-tuned
  formats**, one layout file per resolution/orientation. They are deliberately
  **not** responsive: each file is hand-tuned for its format, but all carry the
  **same widget IDs and the same refresh contract**, so the engine, sensors and
  scripts are identical across formats — only the geometry differs. The smallest
  format (320×240) is a **compact reduced** variant — it drops the weekly/extra
  rows that won't fit and shows the core session card set.

| `design:` value | Layout file | Resolution | Example board (shipped config) |
|---|---|---|---|
| `modern` | `layout_modern.yaml` | 480×800 portrait | Guition ESP32-P4 JC4880P443 (portrait) — **the live device** |
| `modern_landscape` | `layout_modern_landscape.yaml` | 800×480 landscape | Guition ESP32-P4 JC4880P443 (landscape) |
| `modern_square` | `layout_modern_square.yaml` | 480×480 square | Guition ESP32-S3-4848S040 |
| `modern_720` | `layout_modern_720.yaml` | 720×720 square | Waveshare ESP32-P4-86-Panel |
| `modern_1280` | `layout_modern_1280.yaml` | 800×1280 portrait | Guition ESP32-P4 JC8012P4A1 |
| `modern_320x240` | `layout_modern_320x240.yaml` | 320×240 landscape · **compact** | Sunton ESP32-2432S028 |
| `modern_320x480` | `layout_modern_320x480.yaml` | 320×480 portrait | Guition ESP32-JC8048W535 |
| `modern_480x272` | `layout_modern_480x272.yaml` | 480×272 landscape | Guition ESP32-JC4827W543 |
| `modern_720x1280` | `layout_modern_720x1280.yaml` | 720×1280 portrait | Waveshare ESP32-P4 WiFi6 Touch LCD 7 |

All nine formats ship as ready-to-flash grid device configs in
[`example_code/clawdmeter/grid/`](../../example_code/clawdmeter/grid/) (the
480×800 portrait is the live device); point any other board's grid config at a
format by setting `design:`. All nine are rendered headless in the
[SDL preview](../../example_code/clawdmeter/sdl_tests/) (`sdl_modern*` harnesses)
on every layout change.

### Modern layouts at a glance

| modern · 480×800 | modern_landscape · 800×480 | modern_square · 480×480 |
|:---:|:---:|:---:|
| <img src="../../example_code/clawdmeter/sdl_tests/screenshots/modern-data.png" width="170" alt="modern 480×800 portrait"> | <img src="../../example_code/clawdmeter/sdl_tests/screenshots/modern-landscape-data.png" width="300" alt="modern landscape 800×480"> | <img src="../../example_code/clawdmeter/sdl_tests/screenshots/modern-square-data.png" width="200" alt="modern square 480×480"> |

| modern_720 · 720×720 | modern_1280 · 800×1280 |
|:---:|:---:|
| <img src="../../example_code/clawdmeter/sdl_tests/screenshots/modern-720-data.png" width="240" alt="modern 720×720 square"> | <img src="../../example_code/clawdmeter/sdl_tests/screenshots/modern-1280-data.png" width="150" alt="modern 800×1280 portrait"> |

| modern_320x240 · 320×240 (compact) | modern_480x272 · 480×272 |
|:---:|:---:|
| <img src="../../example_code/clawdmeter/sdl_tests/screenshots/modern-320x240-data.png" width="220" alt="modern 320×240 landscape, compact"> | <img src="../../example_code/clawdmeter/sdl_tests/screenshots/modern-480x272-data.png" width="280" alt="modern 480×272 landscape"> |

| modern_320x480 · 320×480 | modern_720x1280 · 720×1280 |
|:---:|:---:|
| <img src="../../example_code/clawdmeter/sdl_tests/screenshots/modern-320x480-data.png" width="130" alt="modern 320×480 portrait"> | <img src="../../example_code/clawdmeter/sdl_tests/screenshots/modern-720x1280-data.png" width="140" alt="modern 720×1280 portrait"> |

> Screenshots are the SDL desktop renders driven by a synthetic usage feed — see
> [`sdl_tests/`](../../example_code/clawdmeter/sdl_tests/).

## Quick start

1. Install the [ha-clawdmeter](https://github.com/corgan2222/ha-clawdmeter)
   integration in Home Assistant and add your Claude account. It creates one
   `Claude <name> (<plan>)` device.
2. Copy the closest device config from
   [`example_code/clawdmeter/`](../../example_code/clawdmeter/).
3. In its `substitutions:` block set the single binding var **`ha_account`** to
   the integration's device slug, e.g. `claude_jane_pro`.
4. Flash with ESPHome **2026.6.0+** (`esphome run <config>.yaml`).

### Finding your `ha_account` slug

In HA → **Developer Tools → States**, filter `sensor.claude` and take the common
prefix before `_session_usage`. The format is `claude_<name>_<plan>`
(e.g. `claude_jane_pro`, `claude_stefan_max`).

## Entity binding

Every entity resolves as `sensor.${ha_account}_<suffix>` (and
`binary_sensor.${ha_account}_<suffix>` for binaries), so setting `ha_account` once
wires the whole device. Entity ids are derived from each sensor's **name**, so a
few suffixes are not the obvious key:

| HA sensor (name)             | entity_id suffix             |
|------------------------------|------------------------------|
| Weekly usage                 | `weekly_usage`               |
| Extra usage credits          | `extra_usage_credits`        |
| Burn rate (5 min)            | `burn_rate_5_min`            |
| Burn rate (30 min)           | `burn_rate_30_min`           |
| Time to limit                | `time_to_limit`              |
| Session limit ETA            | `session_limit_eta`          |
| Runway margin                | `runway_margin`              |
| Pace frame                   | `pace_frame`                 |
| Limit reached before reset   | `limit_reached_before_reset` (binary) |
| Session reset                | `session_reset`              |
| Weekly reset                 | `weekly_reset`               |

The grid configs surface the six **raw** inputs as their own substitutions, all
derived from `ha_account`:

| Substitution      | Resolves to                              | Meaning                  |
|-------------------|------------------------------------------|--------------------------|
| `ha_session_usage`| `sensor.${ha_account}_session_usage`     | session limit used, %    |
| `ha_week_usage`   | `sensor.${ha_account}_weekly_usage`      | weekly limit used, %     |
| `ha_extra_usage`  | `sensor.${ha_account}_extra_usage`       | extra usage, %           |
| `ha_extra_credits`| `sensor.${ha_account}_extra_usage_credits` | extra usage, credits   |
| `ha_session_reset`| `sensor.${ha_account}_session_reset`     | session reset, timestamp |
| `ha_weekly_reset` | `sensor.${ha_account}_weekly_reset`      | weekly reset, timestamp  |

The advanced precomputed metrics are wired by the
[`render_from_integration.yaml`](render_from_integration.yaml) include (below);
they need no per-entity substitutions — just `ha_account`. Override a single line
only if HA deduplicated an entity (`…_2`).

## Architecture

### The integration → device mirror

The grid configs include
[`render_from_integration.yaml`](render_from_integration.yaml) (vars:
`ha_account`). It mirrors the integration's precomputed entities onto the exact
ids the layout draws **by id**, so the layout and pace-frame files stay generic:

| Device id              | Source                                              | Rendered as                                  |
|------------------------|-----------------------------------------------------|----------------------------------------------|
| `clawd_burn5_rate`     | `sensor.${ha_account}_burn_rate_5_min`              | burn rate, %/h                               |
| `clawd_burn30_rate`    | `sensor.${ha_account}_burn_rate_30_min`             | burn rate, %/h                               |
| `clawd_ttl_time_to_100`| `sensor.${ha_account}_time_to_limit`                | "in N min" text                              |
| `clawd_ttl_eta_clock`  | `sensor.${ha_account}_session_limit_eta`            | ISO instant → local `HH:MM` on-device        |
| `clawd_rw_runway`      | `limit_reached_before_reset` binary + `runway_margin` | verdict (`Yes · Nm early` / `No · Nm to spare` / `No`) |
| `clawd_rw_runway_ratio`| `limit_reached_before_reset` binary                 | hit-card colour ratio (`0.0` / `2.0`)        |
| `clawd_pf_band`        | `sensor.${ha_account}_pace_frame` enum              | pace band `0/1/2` (green / orange / red)     |

The pace-frame border include reads `clawd_pf_band` with `green_max: 0.5` /
`orange_max: 1.5`, so `green → band 0`, `orange → band 1`, `red → band 2`. All
mirror sensors are `internal: true` — the integration already exposes the real
values in HA, so no duplicate device entities are created.

### What runs on the device

A handful of things have no integration equivalent and are computed/rendered on
the ESP:

- **Creature animation** —
  [`clawdmeter_engine.h`](clawdmeter_engine.h), a header-only render engine on a
  ~50 ms tick, driven by the session usage %. The engine picks the animation
  group itself from the sampled `session_pct` (see
  [Usage → animation mapping](#usage--animation-mapping)).
- **History charts** —
  [`charts_page.yaml`](charts_page.yaml) samples the usage/credits values into
  on-device ring buffers (90 samples, ≈12 h) every `chart_sample_min` minutes and
  draws scrolling usage and credits graphs. HA exposes no time-series entity, so
  the buffers start empty and fill in from the right.
- **Reset clocks** —
  [`session_reset_clock.yaml`](session_reset_clock.yaml) (included as
  `clawd_reset` / `clawd_wreset`) parses the integration's ISO reset timestamps
  into an absolute wall-clock `HH:MM` plus a smooth live "reset in N min"
  countdown, localised to the device timezone.
- **The LVGL repaint + page-cycle loop.**

### When which animation is shown

The creature's mood tracks **how fast you're burning tokens**, not the absolute
percentage: the engine keeps a ring buffer of `session_pct` samples, computes a
usage rate, and maps it to one of four animation groups (faster burn → livelier
group), with a warm-up guard right after boot/reset. The full group table is in
[Usage → animation mapping](#usage--animation-mapping) below.

## Usage → animation mapping

`clawdmeter_engine.h` keeps a ring buffer of recent `session_pct` samples and
computes a usage **rate** (% per minute) over the window. The rate selects an
animation group; within a group the engine rotates through its animations every
20 s. A drop in `session_pct` of more than 5 points is treated as a session
reset and clears the window.

| Group | Rate (%/min) | Mood | Example animations |
|---|---|---|---|
| 0 idle | warming up / `< 0.10` | calm | sleep, idle breathe, blink, wink |
| 1 normal | `0.10 – 0.20` | working | look around, think, coding |
| 2 active | `0.20 – 0.33` | busy | sway, surprise, bounce |
| 3 heavy | `≥ 0.33` | maxed | bounce DJ, sway DJ, djmix |

The window needs at least 2 samples spanning ≥ 2 minutes before it leaves idle,
so the creature stays calm right after boot or a reset.

## Pace frame (`pace_frame.yaml`)

An optional colour-coded **breathing border** around the creature tile, driven by
the precomputed **pace band** (`clawd_pf_band`, mirrored from the integration's
`pace_frame` enum). It turns the creature's own frame into an at-a-glance status
light: the calmer the runway, the calmer the frame.

```
pace band (clawd_pf_band) ──► pace_frame.yaml ──► creature-tile border
   no value / NaN  -> no border
   band 0 (green)  -> green  (0x43A047)  head-room     — solid, never breathes
   band 1 (orange) -> orange (0xFFB300)  cutting close — breathes (slow)
   band 2 (red)    -> red    (0xE53935)  at/over limit — breathes (fast)
```

It's **pure YAML** — no engine or C++ change. It only restyles an *existing*
LVGL tile (the creature's outer tile, `${creature_uid}_creature_root`), so it
needs both the mirrored pace band and the modular `creature.yaml` tile (for the
target). That's why it ships only on the two **modular grid** examples —
[landscape](../../example_code/clawdmeter/grid/guition-esp32-p4-jc4880p443.yaml)
and [portrait](../../example_code/clawdmeter/grid/guition-esp32-p4-jc4880p443-portrait.yaml).
The all-in-one `remote.yaml` builds don't carry a pace band, so they can't use it.

```yaml
clawd_pace_frame: !include
  file: esphome-modular-lvgl-buttons/ui/clawdmeter/pace_frame.yaml
  vars:
    uid: clawd_pf
    ratio_id: clawd_pf_band            # 0/1/2 band, mirrored from the integration's pace_frame enum
    target_id: clawd_c_creature_root   # ${creature_uid}_creature_root from creature.yaml
    green_max: "0.5"                   # band 0 (<0.5)
    orange_max: "1.5"                  # band 1 (<1.5); band 2 (>=1.5) is red
    show_from: "orange"                # both examples ship this: hide the calm green frame
```

| Var | Required | Meaning |
|---|---|---|
| `uid` | yes | namespace prefix for this include's ids + switch |
| `ratio_id` | yes | id of the pace-band sensor (`clawd_pf_band`) |
| `target_id` | yes | id of the LVGL tile to frame (`${creature_uid}_creature_root`) |
| `enabled` | no | `"false"` compiles the logic to a no-op (default `"true"`) |
| `width` | no | border thickness in px (default `3`) |
| `green_max` / `orange_max` | no | band thresholds (file defaults `0.90` / `1.00`; the grid configs set `0.5` / `1.5` to bucket the integer band) |
| `show_from` | no | lowest band that draws: `"green"` (all bands) / `"orange"` (warn from orange up) / `"red"` (only at/over the limit) |
| `breathe_*` | no | pulse tuning: `breathe_ms_slow` (1400) / `breathe_ms_fast` (700) half-periods, `breathe_min_opa` (60) dim floor, `breathe_default` (`"true"`) |

**The whole frame is runtime-toggleable.** An exposed switch — `name:`
**"Pace Frame"** (`breathe_switch_name`), object id `${uid}_pace_breathe_sw` —
is a *visibility master*, not just a pulse toggle: switch **off** hides the
**entire** frame (no border at all); switch **on** shows it — orange/red
breathing, green solid. Green never breathes either way. Toggle it live from
Home Assistant without a reflash.

> **Default-off elsewhere.** Configs that don't include `pace_frame.yaml` are
> unaffected. Drop the include (or set `enabled: "false"`) to remove the frame.

## Engine C API (`clawdmeter_engine.h`)

| Function | Purpose |
|---|---|
| `clawd_init(lv_obj_t* parent, int w, int h)` | Create the creature canvas on `parent`. The canvas is sized from `parent`'s *measured* content box (resolution-agnostic); `w`/`h` are only a fallback if the layout isn't resolved yet. Call from `on_boot` (priority `-100`, after LVGL is up). |
| `clawd_tick()` | Advance the animation. Call from a 50 ms `interval`. |
| `clawd_set_usage(session_pct, session_reset_mins, weekly_pct, weekly_reset_mins)` | Feed a new sample; only `session_pct` is used. Call from the session sensor's `on_value`. |
| `clawd_usage_sample(float)` / `clawd_usage_group()` | Lower-level sampler / current group accessor (used by the host test). |

> Do **not** add C++ accessor functions named after the HA sensor IDs
> (`clawd_session_pct`, `clawd_weekly_pct`, …): ESPHome generates global objects
> with those exact names from the `id:` fields, and they would collide.

## Files

| File | Purpose |
|---|---|
| `remote.yaml` | The **all-in-one** component: HA sensors, LVGL chrome (canvas + usage bars + labels), animation driver. Include this from a device config. |
| `render_from_integration.yaml` | The **grid data binding** (vars: `ha_account`): mirrors the integration's precomputed entities onto the exact ids the layout reads (`clawd_burn5_rate`, `clawd_ttl_*`, `clawd_rw_*`, `clawd_pf_band`). |
| `engine.yaml`, `creature.yaml`, `stats_panel.yaml`, `usage_bar.yaml`, `session_reset_clock.yaml`, `anim_select.yaml` (+ the `*_tile.yaml` wrappers) and `lang/{en,de,es,fr}.yaml` | The **modular grid** building blocks — one include per tile, placed into grid cells (see the [comparison table](#all-in-one-vs-modular-grid)) — plus the language packs. |
| `layout_classic.yaml`, `layout_modern*.yaml` | Page-1 **layout** files selected by the `design:` substitution — `classic` (creature + stats panel) and the nine `modern` formats. See [Designs and multi-format layouts](#designs-and-multi-format-layouts). |
| `pace_frame.yaml` | The breathing creature-tile border driven by the precomputed pace band (`clawd_pf_band`). See [Pace frame](#pace-frame-pace_frameyaml). |
| `charts_page.yaml` | Optional, design-agnostic **charts page** — usage/credits history graphs, sampled every `chart_sample_min` minutes (90-sample ring, ~12 h). |
| `clawdmeter_engine.h` | Header-only render + animation engine, pulled in via `esphome.includes`. Exposes a small [C API](#engine-c-api-clawdmeter_engineh). |
| `splash_animations.h` | Vendored, generated pixel-art animation data (13 animations, 20×20 grid, RGB565 palette). |
| `tests/test_usage_rate.cpp` | Host (g++) unit test for the usage-rate state machine. |
| `tests/test_convert_counts.js` | Node parity test: header animation count matches the source index. |

## Customisation

Adapt a config **only** through its `substitutions:` block — set `ha_account`,
optionally override a single `ha_*` line, pick a `design:` and a language pack.
The `ui/clawdmeter/*` files are shared logic and are not edited per device.

| Substitution      | Purpose                                                        |
|-------------------|---------------------------------------------------------------|
| `ha_account`      | Integration device slug — the one required binding.           |
| `design`          | Page-1 layout: `classic` / `modern` (and the modern formats). |
| `chart_sample_min`| Charts sampling cadence in minutes (default `8`, ≈12 h ring). |
| `ha_*` (six)      | Per-entity overrides; default to `ha_account` (rarely needed).|

Language is a compile-time pack include from
[`lang/`](lang/) (`en` / `de` / `fr` / `es`; English is the built-in default),
providing every `t_*` string the layout and metric files reference.

## Minimal include

Add the all-in-one component to a device config (see
`example_code/clawdmeter/all-in-one/guition-esp32-p4-jc4880p443.yaml`):

```yaml
packages:
  clawdmeter: !include
    file: esphome-modular-lvgl-buttons/ui/clawdmeter/remote.yaml
    vars:
      uid: clawd
```

The component extends `main_page` by default; pass `page_id` to attach it to a
different page.

## Home Assistant data contract

`remote.yaml` reads these entities. Override the defaults with the matching
`*_entity` vars at include time, pointing them at your ha-clawdmeter device
(e.g. `sensor.${ha_account}_session_usage`).

| Var | Default entity | Type | Meaning |
|---|---|---|---|
| `session_pct_entity` | `sensor.SET_ME_session_usage` | 0–100 | % of the current session limit used. **Drives the animation.** |
| `session_reset_entity` | `sensor.SET_ME_session_reset` | ISO-8601 timestamp | Instant the session limit resets, e.g. `2026-06-17T06:30:00+00:00`. |
| `weekly_pct_entity` | `sensor.SET_ME_weekly_usage` | 0–100 | % of the weekly limit used. |
| `weekly_reset_entity` | `sensor.SET_ME_weekly_reset` | ISO-8601 timestamp | Instant the weekly limit resets. |
| `status_entity` | `sensor.SET_ME_status` | text | Free-form status line shown at the bottom (optional). |

The `*_reset_entity` sensors carry the next reset **instant** as an ISO-8601 UTC
string; the device converts it to minutes-from-now on-device via
`clawd_iso_minutes_from()` (needs a time component with id `system_time`). The
countdown is formatted as `resets in Hh MMm` (session) and `resets in Dd HHh`
(weekly), and is omitted when the timestamp is missing/unparseable. Only
`session_pct` feeds the animation engine; the rest are display-only.

### Optional layout / colour vars

| Var | Default | Meaning |
|---|---|---|
| `clawd_creature_h` | `60%` | Height of the top creature region. |
| `clawd_stats_h` | `38%` | Height of the bottom stats panel. |
| `clawd_bar_low` | `0x43A047` (green) | Bar fill colour when `< 50%`. |
| `clawd_bar_mid` | `0xFFB300` (amber) | Bar fill colour `50–80%`. |
| `clawd_bar_high` | `0xE53935` (red) | Bar fill colour `≥ 80%`. |

## Build

Standard `esphome compile` / `esphome run`. One gotcha: **PlatformIO/ESP-IDF
reject build paths that contain whitespace.** If your checkout sits under a path
with a space, build from a space-free directory — e.g. junction the repo into one:

```powershell
New-Item -ItemType Junction -Path C:\build\esphome-modular-lvgl-buttons `
  -Target "C:\path with space\esphome-modular-lvgl-buttons"
# put the device yaml + secrets.yaml next to the junction, then: esphome compile clawd.yaml
```

## Tests

```bash
# usage-rate state machine (host build)
g++ -std=c++17 ui/clawdmeter/tests/test_usage_rate.cpp -o /tmp/urt && /tmp/urt

# animation-data parity
node ui/clawdmeter/tests/test_convert_counts.js
```

## Credits & license

Part of the
[**esphome-modular-lvgl-buttons**](https://github.com/corgan2222/esphome-modular-lvgl-buttons)
library, under its **MIT license** (see [LICENSE](../../LICENSE)).

| Project | Role |
|---|---|
| [HermannBjorgvin/Clawdmeter](https://github.com/HermannBjorgvin/Clawdmeter) | Original Clawdmeter — creature concept and pixel-art animations. |
| [corgan2222/ha-clawdmeter](https://github.com/corgan2222/ha-clawdmeter) | Home Assistant integration that supplies all usage entities. |
| [agillis/esphome-modular-lvgl-buttons](https://github.com/agillis/esphome-modular-lvgl-buttons) | Upstream modular LVGL button library. |

© the respective authors. Pixel-art animation data is vendored from the original
Clawdmeter project under its own terms.
