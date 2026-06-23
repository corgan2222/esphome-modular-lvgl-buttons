#!/usr/bin/env bash
# Clawdmeter example config/compile test — validates every device example in
# this folder against the real ESPHome toolchain. Catches broken includes,
# missing substitutions and (with --compile) actual build errors. Use it after
# touching the shared UI, the language packs or any example wiring.
#
# Runs inside WSL (Ubuntu). It reuses the same work dir as the flash procedure:
# a directory that contains the `esphome-modular-lvgl-buttons` repo symlink and
# a real secrets.yaml, so the examples' `!include esphome-modular-lvgl-buttons/…`
# paths resolve. Each example is copied in as a temp file and removed after.
# secrets.yaml and the symlink are never written.
#
#   bash test_examples.sh                 # fast: `esphome config` per example
#   bash test_examples.sh --compile       # slow: full `esphome compile` per example
#   bash test_examples.sh jc4880          # only examples whose name matches the arg
#   bash test_examples.sh --compile grid  # compile only the grid examples
#
# Override paths via env: ESPHOME=… WORK=…  (WORK must hold the repo symlink +
# secrets.yaml; defaults to ~/clawdflash, the dir used for OTA flashing).

set -u

ESPHOME="${ESPHOME:-$HOME/esphome-venv/bin/esphome}"
WORK="${WORK:-$HOME/clawdflash}"
SYMLINK="esphome-modular-lvgl-buttons"
EXAMPLES="$WORK/$SYMLINK/example_code/clawdmeter"
TMP="$WORK/_cfgtest.yaml"
LOG="/tmp/clawd_example_test.log"

mode="config"
filter=""
for arg in "$@"; do
  case "$arg" in
    --compile) mode="compile" ;;
    --config)  mode="config" ;;
    *)         filter="$arg" ;;
  esac
done

# --- sanity checks -----------------------------------------------------------
[ -x "$ESPHOME" ]      || { echo "esphome not found at $ESPHOME"; exit 2; }
[ -L "$WORK/$SYMLINK" ] || [ -d "$WORK/$SYMLINK" ] || {
  echo "work dir $WORK has no '$SYMLINK' symlink — see flash procedure"; exit 2; }
[ -f "$WORK/secrets.yaml" ] || { echo "no secrets.yaml in $WORK"; exit 2; }
[ -d "$EXAMPLES" ]     || { echo "examples dir not found: $EXAMPLES"; exit 2; }

cleanup() { rm -f "$TMP"; }
trap cleanup EXIT

# --- run ---------------------------------------------------------------------
pass=0; fail=0; failed_names=()
echo "mode: esphome $mode   work: $WORK"
echo

# Recurse into the category subfolders (all-in-one/, grid/, …); skip sdl_tests/
# (those are SDL harness fragments, not standalone device configs). The name is
# subdir-qualified (e.g. "grid/guition-…") so `--compile grid` filters by folder
# and the jc4827 basename that exists in BOTH folders doesn't collide.
while IFS= read -r f; do
  rel="${f#"$EXAMPLES"/}"
  name="${rel%.yaml}"
  [ -n "$filter" ] && [[ "$name" != *"$filter"* ]] && continue

  printf '%-52s ' "$name"
  cp "$f" "$TMP"
  if ( cd "$WORK" && "$ESPHOME" "$mode" "_cfgtest.yaml" ) >"$LOG" 2>&1; then
    echo "OK"; pass=$((pass+1))
  else
    echo "FAIL"; fail=$((fail+1)); failed_names+=("$name")
    sed -n '/ERROR\|Error\|error:/p' "$LOG" | head -8 | sed 's/^/    /'
  fi
  rm -f "$TMP"
done < <(find "$EXAMPLES" -name '*.yaml' -not -path '*/sdl_tests/*' | sort)

echo
echo "==== $pass passed, $fail failed ===="
if [ "$fail" -gt 0 ]; then
  printf '  - %s\n' "${failed_names[@]}"
  echo "  (full log of last failure: $LOG)"
fi
exit $((fail > 0 ? 1 : 0))
