#!/usr/bin/env bash
# Clawdmeter SDL screenshot suite — renders the layout for every supported
# display in an SDL desktop window (no hardware) and saves a reference PNG.
#
# Runs inside WSL (Ubuntu) with WSLg. See README.md for the one-time setup
# (esphome venv, ~/clawdsdl build dir with the repo symlinked in, secrets.yaml).
#
#   bash run.sh            # render every display
#   bash run.sh square     # render only rows whose name matches "square"/<arg>
#
# Each display reuses the same SDL build (config name clawdmeter-sdl); only the
# harness file and the -s screen_width/height overrides change per row.

set -u

# --- paths (override via env) ------------------------------------------------
ESPHOME="${ESPHOME:-$HOME/esphome-venv/bin/esphome}"
PY="${PY:-$HOME/esphome-venv/bin/python}"
BUILD="${BUILD:-$HOME/clawdsdl}"
# Directory that holds this script (works whether called by path or sourced).
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HARNESSES="$HERE/harnesses"
CAP="$HERE/capture.py"
OUT="$HERE/screenshots"
BIN="$BUILD/.esphome/build/clawdmeter-sdl/.pioenvs/clawdmeter-sdl/program"
PROG_PAT='[c]lawdmeter-sdl/program'   # bracket trick: never matches this script

export DISPLAY="${DISPLAY:-:0}"
mkdir -p "$OUT"

# --- display matrix ----------------------------------------------------------
# name                          harness        W     H     pages
DISPLAYS=(
  "jc4827w543                   single         480   272   1"
  "jc8048w535                   single         800   480   1"
  "jc4880p443                   single         800   480   1"
  "jc8012p4a1                   single         800   1280  1"
  "sunton-2432s028              single_small   320   240   1"
  "waveshare-wifi6-touch-lcd-7  single         1024  600   1"
  "s3-4848s040                  square         480   480   2"
  "waveshare-86-panel           square         720   720   2"
  "jc4880p443-grid              grid           800   480   1"
  "jc4880p443-grid-portrait     grid_portrait  480   800   1"
)

filter="${1:-}"

kill_prog() { pkill -f "$PROG_PAT" 2>/dev/null; sleep 0.5; }

fail=0
for row in "${DISPLAYS[@]}"; do
  read -r name harness w h pages <<<"$row"
  [ -n "$filter" ] && [[ "$name" != *"$filter"* ]] && continue

  echo "==== $name  (${w}x${h}, $harness, ${pages}p) ===================="
  cp "$HARNESSES/sdl_$harness.yaml" "$BUILD/sdl_$harness.yaml" || { fail=1; continue; }

  if ! ( cd "$BUILD" && "$ESPHOME" -s screen_width "$w" -s screen_height "$h" \
          compile "sdl_$harness.yaml" >/tmp/clawd_compile.log 2>&1 ); then
    echo "  COMPILE FAILED — see /tmp/clawd_compile.log"; tail -15 /tmp/clawd_compile.log
    fail=1; continue
  fi

  kill_prog
  ( cd "$(dirname "$BIN")" && ./program >/tmp/clawd_run.log 2>&1 & )

  args=(--name clawdmeter-sdl --settle 3 --out "$OUT/$name.png")
  [ "$pages" = "2" ] && args+=(--pages 2 --swipe next)
  "$PY" "$CAP" "${args[@]}" || { echo "  CAPTURE FAILED"; fail=1; }

  kill_prog
done

echo
echo "screenshots -> $OUT"
ls -1 "$OUT"/*.png 2>/dev/null
exit $fail
