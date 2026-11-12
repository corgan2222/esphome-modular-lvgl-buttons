#!/usr/bin/env bash
# Validate the example configs with `esphome config`: modular and the
# Clawdmeter grid and all-in-one configs. test_configs.sh covers modular only
# and can also compile; this script is the broad, fast check. advanced/ is left
# out: several of its upstream examples do not validate (a missing file, stale
# widget IDs, YAML syntax errors).
#
# Run it from the directory that contains the esphome-modular-lvgl-buttons
# checkout, the same place test_configs.sh expects, or from the checkout
# itself (the pre-push hook does that):
#
#   bash esphome-modular-lvgl-buttons/testing/validate_examples.sh
#   JOBS=8 ESPHOME=~/venv/bin/esphome bash esphome-modular-lvgl-buttons/testing/validate_examples.sh
#
# Without a secrets.yaml next to the checkout, the script writes one with
# made-up values and removes it at the end. Never flash an image built with it.
# The SDL preview configs need SDL2 (sdl2-config) even for validation; without
# it, as on Windows, they are skipped and reported as such.
set -euo pipefail

REPO_DIR="esphome-modular-lvgl-buttons"
DIRS=(modular clawdmeter/grid clawdmeter/all-in-one)
JOBS="${JOBS:-4}"
ESPHOME="${ESPHOME:-esphome}"

if [ -d example_code ] && [ "$(basename "$PWD")" = "$REPO_DIR" ]; then
  cd ..
fi
if [ ! -d "$REPO_DIR/example_code" ]; then
  echo "ERROR: run this from the directory that contains $REPO_DIR/"
  exit 1
fi

made_secrets=false
copies=()
cleanup() {
  rm -f "${copies[@]}"
  if $made_secrets; then rm -f secrets.yaml; fi
}
trap cleanup EXIT

if [ ! -f secrets.yaml ]; then
  made_secrets=true
  cat > secrets.yaml <<'EOF'
# Made-up values for validation only.
wifi_ssid: "ci-ssid"
wifi_password: "ci-password"
ap_password: "ci-ap-password"
ota_password: "ci-ota-password"
api_encryption_key: "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="
latitude: "50.0"
longitude: "10.0"
envoy_id: "000000000000"
enpower_id: "000000000000"
EOF
fi

have_sdl=false
if command -v sdl2-config >/dev/null 2>&1; then have_sdl=true; fi
skipped=()

# Configs include the packages as esphome-modular-lvgl-buttons/..., relative to
# their own location, so each one is copied next to the checkout first.
for d in "${DIRS[@]}"; do
  for f in "$REPO_DIR/example_code/$d"/*.yaml; do
    [ -f "$f" ] || continue
    if ! $have_sdl && [[ "$(basename "$f")" == SDL-* ]]; then
      skipped+=("$d/$(basename "$f")")
      continue
    fi
    copy="validate__${d//\//_}__$(basename "$f")"
    cp "$f" "$copy"
    copies+=("$copy")
  done
done

check() {
  local out
  if out=$("$ESPHOME" config "$1" 2>&1); then
    echo "PASS $1"
  else
    printf 'FAIL %s\n%s\n' "$1" "$(tail -n 15 <<<"$out" | sed 's/^/    /')"
  fi
}
export -f check
export ESPHOME

results=$(printf '%s\n' "${copies[@]}" | xargs -P "$JOBS" -I{} bash -c 'check "$1"' _ {})
echo "$results"

total=${#copies[@]}
failed=$(grep -c '^FAIL ' <<<"$results" || true)
echo "========================================="
echo "Validated $total configs: $((total - failed)) passed, $failed failed"
if [ ${#skipped[@]} -gt 0 ]; then
  echo "Skipped without SDL2: ${skipped[*]}"
fi
echo "========================================="
[ "$failed" -eq 0 ]
