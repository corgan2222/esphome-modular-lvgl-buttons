# Clawdmeter — animated Claude token-usage display

ESPHome port of [Clawdmeter](https://github.com/HermannBjorgvin/Clawdmeter). Shows
Claude token usage as a pixel-art creature whose animation changes with how fast
you are burning tokens, plus session/weekly usage and reset times shown both
graphically (arcs) and as text.

The original firmware pulled usage over BLE from a Python daemon. This port drops
the daemon: it reads usage from **Home Assistant sensors** via the repo's
`remote` transport (the `homeassistant` platform). Feed those sensors from
whatever scrapes your usage (HA template sensor, REST sensor, the
[ccusage](https://github.com/ryoppippi/ccusage) exporter, etc.).

## Files

| File | Purpose |
|---|---|
| `remote.yaml` | The component: HA sensors, LVGL chrome (canvas + arcs + labels), animation driver. Include this from a device config. |
| `clawdmeter_engine.h` | Header-only render + usage-rate engine, pulled in via `esphome.includes`. Exposes a small C API. |
| `splash_animations.h` | Vendored, generated pixel-art animation data (13 animations, 20×20 grid, RGB565 palette). |
| `tests/test_usage_rate.cpp` | Host (g++) unit test for the usage-rate state machine. |
| `tests/test_convert_counts.js` | Node parity test: header animation count matches the source index. |

## Home Assistant data contract

`remote.yaml` reads these entities. Override the defaults with the matching
`*_entity` vars at include time.

| Var | Default entity | Type | Meaning |
|---|---|---|---|
| `session_pct_entity` | `sensor.clawdmeter_session_pct` | 0–100 | % of the current session limit used. **Drives the animation.** |
| `session_reset_entity` | `sensor.clawdmeter_session_reset_mins` | minutes | Time until the session limit resets. |
| `weekly_pct_entity` | `sensor.clawdmeter_weekly_pct` | 0–100 | % of the weekly limit used. |
| `weekly_reset_entity` | `sensor.clawdmeter_weekly_reset_mins` | minutes | Time until the weekly limit resets. |
| `status_entity` | `text_sensor.clawdmeter_status` | text | Free-form status line shown at the bottom. |

Reset minutes are formatted for display as `Hh MMm` (session) and `Dd HHh`
(weekly). Only `session_pct` feeds the rate machine; the rest are display-only and
are read straight from the HA sensors in YAML.

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

The window needs at least 2 samples spanning ≥ 4 minutes before it leaves idle,
so the creature stays calm right after boot or a reset.

## Engine C API (`clawdmeter_engine.h`)

| Function | Purpose |
|---|---|
| `clawd_init(lv_obj_t* parent, int w, int h)` | Create the creature canvas on `parent`, sized for a `w×h` screen. Call from `on_boot` (priority `-100`, after LVGL is up). |
| `clawd_tick()` | Advance the animation. Call from a 50 ms `interval`. |
| `clawd_set_usage(session_pct, session_reset, weekly_pct, weekly_reset)` | Feed a new sample; only `session_pct` is used. Call from the session sensor's `on_value`. |
| `clawd_usage_sample(float)` / `clawd_usage_group()` | Lower-level sampler / current group accessor (used by the host test). |

> Do **not** add C++ accessor functions named after the HA sensor IDs
> (`clawd_session_pct`, `clawd_weekly_pct`, …): ESPHome generates global objects
> with those exact names from the `id:` fields, and they would collide.

## Usage

Add to a device config (see
`example_code/guition-esp32-p4-jc4880p443-clawdmeter.yaml`):

```yaml
packages:
  clawdmeter: !include
    file: esphome-modular-lvgl-buttons/ui/clawdmeter/remote.yaml
    vars:
      uid: clawd
```

The component extends `main_page` by default; pass `page_id` to attach it to a
different page.

## Building

Standard `esphome compile`. One gotcha: **PlatformIO/ESP-IDF reject build paths
that contain whitespace.** If your checkout sits under a path with a space, build
from a space-free directory — e.g. junction the repo into one:

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
