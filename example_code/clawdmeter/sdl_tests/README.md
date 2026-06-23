# Clawdmeter SDL screenshot tests

Render the Clawdmeter UI for **every supported display** in an SDL desktop
window — no ESP hardware required — and save a reference PNG per display. Used
to eyeball layout changes across all aspect ratios before flashing anything.

```
sdl_tests/
  run.sh              data-driven runner: one row per display, compiles + captures
  run_data.sh         like run.sh, synthetic feed on (sim_feed=1) -> grid*-data.png
  capture.py          grabs the SDL window via Xlib, optional two-page swipe
  harnesses/          reusable SDL configs (one per layout family)
    sdl_single.yaml       all-in-one remote.yaml, single page
    sdl_single_small.yaml single page, small-screen tuning (320x240 sunton)
    sdl_square.yaml       all-in-one remote.yaml, TWO pages (creature / stats)
    sdl_grid.yaml         individual tiles, 4x4 grid, landscape
    sdl_grid_portrait.yaml individual tiles, 4x4 grid, portrait
  screenshots/        output PNGs (one per display; _page1/_page2 for square)
```

## Why SDL

ESPHome can target an SDL "host" platform that opens a desktop window instead of
driving a panel. We compile the same layout the device uses, run the resulting
native binary under WSLg, and screenshot the window. This catches layout bugs
(overflow, overlap, off-screen widgets) on any resolution in seconds.

## One-time setup (WSL Ubuntu-24.04 + WSLg)

The runner lives on the Windows filesystem but **must run inside WSL** — the
SDL build is Linux-native and needs WSLg's X server for the window.

1. **WSL with WSLg** (Windows 11, or Win10 with the WSLg-enabled WSL). Confirm
   `echo $DISPLAY` prints `:0` inside the distro.
2. **ESPHome venv** at `~/esphome-venv`:
   ```bash
   python3 -m venv ~/esphome-venv
   ~/esphome-venv/bin/pip install esphome python-xlib pillow
   ```
   (`python-xlib` + `pillow` are needed by `capture.py`.)
3. **SDL build deps**: `sudo apt install build-essential libsdl2-dev`.
4. **Build dir `~/clawdsdl`** — a WSL-local build dir keeps the SDL build cache
   off the Windows mount. Symlink the repo in:
   ```bash
   mkdir -p ~/clawdsdl
   ln -s "/mnt/d/Coding/git-corgan/esphome_modular_lvgl_buttons/esphome-modular-lvgl-buttons" \
         ~/clawdsdl/esphome-modular-lvgl-buttons
   ```
   `run.sh` copies the active harness into `~/clawdsdl/` before each compile, so
   the `esphome-modular-lvgl-buttons/...` includes resolve via the symlink. The
   SDL harnesses carry no `!secret`/WiFi, so no `secrets.yaml` is needed.

## Running

From WSL (or via `wsl.exe -d Ubuntu-24.04 -- bash -lc '...'` from Windows):

```bash
export DISPLAY=:0
bash run.sh                 # render EVERY display
bash run.sh grid            # only the grid rows (jc4880p443-grid*)
bash run.sh s3-4848         # a single display by name substring
```

The `$1` filter is a substring match on the display **name** column.

Output PNGs land in `screenshots/`. Square (two-page) displays produce
`<name>_page1.png` (creature) and `<name>_page2.png` (stats).

## The display matrix

Edit the `DISPLAYS=( ... )` array in `run.sh`. Each row is:

```
name                          harness        W     H     pages
```

* **harness** picks `harnesses/sdl_<harness>.yaml` (single / square / grid /
  grid_portrait). Many displays share one harness; only W/H differ.
* **W/H** are passed as `-s screen_width N -s screen_height N`.
* **pages = 2** triggers the swipe-and-capture-page-2 path (square displays).

## How a harness works

Each harness is a normal ESPHome config with the **device-specific bits swapped
for SDL equivalents** so the *layout* is identical to the real device:

| Device config            | SDL harness                         |
|--------------------------|-------------------------------------|
| `hardware/<panel>.yaml`  | `hardware/SDL-lvgl.yaml`            |
| `common/sensors_base.yaml` | `common/sensors_base_sdl.yaml`   |
| `common/backlight_time.yaml` | `common/time_homeassistant.yaml`|
| `pages/loading.yaml`     | **dropped** (see gotchas)           |

Plus, every harness adds:

* `external_components: github://pr#12312` (font/image/lvgl) — required to build.
* `api:` present — the `homeassistant` platform sensors need it to be valid.
  They stay *unavailable* on the host → everything renders at 0%, which is what
  we want for a neutral layout shot.
* `on_boot` priority `-200` → `lvgl.page.show: main_page`, so we never
  accidentally capture an overlay/info page.
* HA entity ids are **placeholders** in the grid harnesses — the host never
  connects to HA. Swap them for your own ids on a real device.

### Single vs. square vs. grid

* **single** / **square** drive the all-in-one `ui/clawdmeter/remote.yaml`.
  Square passes `stats_page_id: stats_page` (+ `clawd_creature_h`/`clawd_stats_h`
  `92%`, `clawd_stats_align: CENTER`) to split the creature onto page 1 and the
  stats onto page 2, navigated by `common/swipe_navigation.yaml`.
* **single_small** is `single` with the small-screen tuning the 320x240 sunton
  device config uses: a 14px label font + `clawd_creature_h: 46%` /
  `clawd_stats_h: 50%`, so the labels don't wrap and the stats panel isn't
  cramped under the creature. Keep it in sync with the sunton device YAML.
* **grid** / **grid_portrait** mirror the
  `grid/guition-esp32-p4-jc4880p443*.yaml` device configs: the engine
  + creature + the backend rate/clock/anim packages + the on-display stats panel,
  each placing itself into a cell of a 4x4 page grid.

## capture.py

Grabs the running SDL window straight off the X server.

```bash
python3 capture.py --name clawdmeter-sdl --out shot.png            # single page
python3 capture.py --name clawdmeter-sdl --pages 2 --out shot.png  # -> shot_page1.png + shot_page2.png
python3 capture.py --pages 2 --swipe prev ...                      # use page.previous
```

* The window is found by a DFS over the X tree for `WM_NAME == clawdmeter-sdl`.
* It is grabbed via `win.get_image(... ZPixmap)` + `BGRX` → RGB. A root/`mss`
  grab returns **black** for these XWayland windows, hence the direct drawable
  grab.
* `--pages 2` captures page 1, synthesises a swipe via XTEST, then captures
  page 2.

## Gotchas learned the hard way

* **`pkill -f 'clawdmeter-sdl/program'` kills itself** — its own command line
  matches the pattern. Use the bracket trick: `pkill -f '[c]lawdmeter-sdl/program'`.
* **The host binary buffers stdout** when not a TTY, so a backgrounded run's log
  file comes out **empty**. Don't rely on the program's own logs; verify via the
  screenshot instead.
* **Drop `pages/loading.yaml`** — its `top_layer` overlay is only cleared once a
  real HA connection reports ready. On the host it never clears and covers the UI.
* **LVGL swipe needs distance AND velocity.** A slow drag is *not* detected as a
  gesture. `capture.py` swipes fast: ~6 steps, ~10 ms each, 10%→90% of width.
* **Swipe direction:** creature(page 1)→stats(page 2) is `lvgl.page.next` =
  `on_swipe_right` = a **left→right** finger drag (`--swipe next`, the default).
  `--swipe prev` is the right→left drag = `page.previous`.
* **Run inside WSL.** From Windows, wrap commands in
  `wsl.exe -d Ubuntu-24.04 -- bash -lc '...'` and `export DISPLAY=:0`.
