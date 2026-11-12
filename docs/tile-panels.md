# Tile panel guide

How to lay out, style and preview a tile panel. The entity types are listed in the [main README](../README.md). Working configs per board are in `example_code/modular/`.

## Variables every tile takes

| Variable | Required | Description |
|---|---|---|
| `uid` | yes | Unique identifier. It must be unique across your entire config |
| `entity_id` | yes | ESPHome component ID (local) or Home Assistant entity such as `"light.foo"` (remote) |
| `row` | yes | Grid row, 0-based |
| `column` | yes | Grid column, 0-based |
| `text` | yes | Tile label |
| `icon` | yes | MDI icon glyph such as `$mdi_lightbulb` |
| `row_span` | no | Rows to span, default `1` |
| `column_span` | no | Columns to span, default `1` |
| `page_id` | no | Parent page ID, default `main_page` |

Each type adds its own variables. They are documented in `ui/<type>/README.md`. `sensor_button` has no README and documents them in the header of `ui/sensor_button/remote.yaml`.

## Grid layout and pages

Pages use LVGL's grid layout. `layout: NxM` creates N rows and M columns.

| Layout | Tiles | Good for |
|---|---|---|
| `2x2` | 4 | Small displays |
| `2x3` | 6 | Portrait or compact |
| `3x3` | 9 | 480×480 square displays |
| `4x4` | 16 | Large landscape displays |

Place a tile with `row` and `column`. Use `row_span` and `column_span` to make a tile span several cells.

For more pages, add them to the `lvgl.pages` list and include `swipe_navigation.yaml` on each. Set `page_id` on a tile to route it to its page:

```yaml
lvgl:
  pages:
  - id: main_page
    layout: 3x3
    styles: page_style
    <<: !include esphome-modular-lvgl-buttons/common/swipe_navigation.yaml
  - id: lights_page
    layout: 2x3
    styles: page_style
    <<: !include esphome-modular-lvgl-buttons/common/swipe_navigation.yaml
```

## Theme and icons

The theme lives in `common/theme/`. One include, `common/theme/index.yaml`, brings colors, fonts, MDI glyph substitutions and LVGL styles. The [theme README](../common/theme/README.md) lists the theme variables with their defaults, the named colors and the fonts.

To debug a layout, swap to the debug variant. It adds red outlines to all widgets:

```yaml
# theme: !include esphome-modular-lvgl-buttons/common/theme/index.yaml
  theme: !include esphome-modular-lvgl-buttons/common/theme/index_debug.yaml
```

Icons are [Material Design Icons](https://materialdesignicons.com/), used through substitution variables: `icon: $mdi_lightbulb`.

**List every icon you use as a glyph in the `font:` block of your device config.** A missing glyph shows as a blank square, and the build does not fail. The theme README shows that block. Each detail page type needs specific glyphs, listed in the README of the type.

## Your own images

Every `image:` block in this repository uses the platform form introduced in ESPHome 2026.7.0. An `image:` block in your own device YAML must be a list, and each entry must carry `platform: file`:

```yaml
image:
  - platform: file
    type: RGB565
    transparency: alpha_channel
    file: my_logo.png
    id: my_logo
    resize: 100x100
```

Two failure modes are worth knowing. In both, the error message does not point at the file you need to fix.

**A mapping-shaped `image:` block silently deletes every image the packages define.** Anything under `image:` that is not a list replaces the package images instead of adding to them. A bare `defaults:` key is enough. ESPHome merges two lists by concatenating them, but a mapping wins outright. The result is a cascade of `Couldn't find ID 'ha_img'`, `'esphome_img'` and `'hue_ring_img'` errors. They point into `pages/loading.yaml` and `ui/popup/popup_system.yaml`, where the IDs are defined correctly.

**Do not fall back to the form from before 2026.7.** List entries without `platform:`, mixed with this repository's platform-tagged entries, switch off ESPHome's automatic migration. The error is `'image' requires a 'platform' key but it was not specified`.

## Desktop preview with SDL

You can test a UI on macOS or Linux without flashing hardware. Install SDL first:

```bash
brew install sdl2              # macOS
sudo apt install libsdl2-dev   # Ubuntu, Debian
```

Use the SDL hardware file instead of a real board. It builds for ESPHome's `host` platform:

```yaml
packages:
  hardware: !include esphome-modular-lvgl-buttons/hardware/SDL-lvgl.yaml
  sensors:  !include esphome-modular-lvgl-buttons/common/sensors_base_sdl.yaml
```

Then run `esphome run your-config.yaml`. A window opens and simulates the display. `example_code/modular/SDL-lvgl-display_modular_480px.yaml` is a full working SDL config.

The Clawdmeter layouts have their own SDL screenshot runner, described in the [sdl_tests README](../example_code/clawdmeter/sdl_tests/README.md).

## Feature modules

More modules under `ui/` for specific integrations. `example_code/advanced/` has full working configs for them.

| Module | What it does |
|---|---|
| `ui/clawdmeter/` | Clawdmeter, the Claude usage display. See its [README](../ui/clawdmeter/README.md). |
| `ui/clock/flip_clock.yaml` | Gluqlo-style flip clock widget |
| `ui/weather/today.yaml` | Current weather tile from a Home Assistant weather entity |
| `ui/weather/forecast.yaml` | 4-day forecast widget through `weather.get_forecasts` |
| `ui/weather/pirate_weather.yaml` | Weather from the Pirate Weather API, without Home Assistant. `ui/weather/aqi_pill.yaml` adds an AQI pill. |
| `ui/solar/` | Enphase and solar production and consumption monitoring |
| `ui/tides/tide_update.yaml` | NOAA tide clock with gauge display |
| `ui/tides/NOAA_tide_update.yaml` | Tide data from the NOAA CO-OPS API with Home Assistant sensors: level, percentage, high and low times |
| `ui/tides/local_tide_update.yaml` | Offline tide prediction on the device. A drop-in replacement for the NOAA file. |
| `pages/loading_standalone.yaml` | Neutral boot screen for a device without Home Assistant |

With the ESPHome CLI, the solar and tides modules need `pip install cairosvg` for SVG images.

Variables of `NOAA_tide_update.yaml`: `noaa_station_id`, `noaa_unit_system` (`english` or `metric`) and `noaa_unit_of_measurement` (`ft` or `m`). `local_tide_update.yaml` takes `tide_station_data` or `tide_station_id`. The two Home Assistant weather modules are documented in the [weather README](../ui/weather/README.md).
