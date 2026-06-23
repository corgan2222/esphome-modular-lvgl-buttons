#!/usr/bin/env bash
# Clawdmeter SDL screenshot suite — REAL-DATA variant of run.sh.
#
# run.sh renders the layout with no HA, so every tile sits at 0% / "warming up".
# This variant flips the harness' opt-in synthetic feed on (`-s sim_feed 1`) and
# lets the window run for a long settle, so the on-device burn-rate / time-to-100
# / stats / reset-clock tiles fill with real numbers before the screenshot.
#
# Only the two GRID harnesses carry the feed, so this renders just those two.
# Output: screenshots/grid-data.png + screenshots/grid-portrait-data.png.
#
#   bash run_data.sh                 # render both grid layouts with live data
#   bash run_data.sh portrait        # only rows whose name matches the arg
#   SETTLE=240 bash run_data.sh      # override the per-row settle (default 200s)
#
# Same env knobs as run.sh: ESPHOME, PY, BUILD, DISPLAY. Reuses the same SDL
# build (config name clawdmeter-sdl); rows compile+run+capture sequentially.

set -u

# --- paths (override via env) ------------------------------------------------
ESPHOME="${ESPHOME:-$HOME/esphome-venv/bin/esphome}"
PY="${PY:-$HOME/esphome-venv/bin/python}"
BUILD="${BUILD:-$HOME/clawdsdl}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HARNESSES="$HERE/harnesses"
CAP="$HERE/capture.py"
OUT="$HERE/screenshots"
BIN="$BUILD/.esphome/build/clawdmeter-sdl/.pioenvs/clawdmeter-sdl/program"
PROG_PAT='[c]lawdmeter-sdl/program'   # bracket trick: never matches this script

# Long settle so the 20s feed accumulates a >=120s window and the engine leaves
# "warming up" (it needs >=2 samples spanning CLAWD_MIN_WINDOW_MS = 120000ms).
SETTLE="${SETTLE:-200}"

export DISPLAY="${DISPLAY:-:0}"
mkdir -p "$OUT"

# --- display matrix ----------------------------------------------------------
# name                 harness        W     H
DISPLAYS=(
  "grid-data           grid           800   480"
  "grid-portrait-data  grid_portrait  480   800"
)

filter="${1:-}"

kill_prog() { pkill -f "$PROG_PAT" 2>/dev/null; sleep 0.5; }

fail=0
for row in "${DISPLAYS[@]}"; do
  read -r name harness w h <<<"$row"
  [ -n "$filter" ] && [[ "$name" != *"$filter"* ]] && continue

  echo "==== $name  (${w}x${h}, $harness, sim_feed=1, settle=${SETTLE}s) ===="
  cp "$HARNESSES/sdl_$harness.yaml" "$BUILD/sdl_$harness.yaml" || { fail=1; continue; }

  if ! ( cd "$BUILD" && "$ESPHOME" -s screen_width "$w" -s screen_height "$h" \
          -s sim_feed 1 compile "sdl_$harness.yaml" >/tmp/clawd_compile.log 2>&1 ); then
    echo "  COMPILE FAILED — see /tmp/clawd_compile.log"; tail -15 /tmp/clawd_compile.log
    fail=1; continue
  fi

  kill_prog
  ( cd "$(dirname "$BIN")" && ./program >/tmp/clawd_run.log 2>&1 & )

  "$PY" "$CAP" --name clawdmeter-sdl --settle "$SETTLE" --out "$OUT/$name.png" \
    || { echo "  CAPTURE FAILED"; fail=1; }

  kill_prog
done

echo
echo "screenshots -> $OUT"
ls -1 "$OUT"/*-data.png 2>/dev/null
exit $fail
