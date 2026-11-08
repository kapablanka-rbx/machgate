#!/bin/bash
# Test: guest popen() of a Mach-O guest re-execs through the MachGate loader.
set -euo pipefail

cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
FIXTURE="tests/fixtures/darwin_popen_exit42"
map_file="$(mktemp)"
trap 'rm -f "$map_file"' EXIT

[ -f "$FIXTURE" ] || bash tests/fixtures/build_fixtures.sh
printf 'libSystem.B = %s/libsystem_shim.so\n' "$BUILD_DIR" > "$map_file"

status=0
(cd tests/fixtures && MACHGATE_DYLIB_MAP="$map_file" \
    "$BUILD_DIR/machgate" darwin_popen_exit42 2>/dev/null) || status=$?
[ "$status" -eq 42 ]
