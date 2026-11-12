#!/usr/bin/env bash
# Build and run the Clawdmeter engine unit test, the same way CI does.
# Run it from the root of the checkout:
#
#   bash testing/unit_tests.sh
#
# On Windows, Git Bash has no g++. The script then runs itself inside WSL,
# which needs a default distribution with g++ installed.
set -euo pipefail

if ! command -v g++ >/dev/null 2>&1; then
  if command -v wsl.exe >/dev/null 2>&1; then
    exec wsl.exe -e bash testing/unit_tests.sh
  fi
  echo "ERROR: g++ not found, and no WSL to fall back to"
  exit 1
fi

out=$(mktemp)
trap 'rm -f "$out"' EXIT
g++ -std=c++17 -Wall -Wextra -Werror ui/clawdmeter/tests/test_usage_rate.cpp -o "$out"
"$out"
