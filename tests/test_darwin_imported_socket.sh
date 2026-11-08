#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
FIXTURE="tests/fixtures/darwin_imported_socket"
map_file="$(mktemp)"
trap 'rm -f "$map_file"' EXIT

[ -f "$FIXTURE" ] || bash tests/fixtures/build_fixtures.sh
printf 'libSystem.B = %s/libsystem_shim.so\n' "$BUILD_DIR" > "$map_file"
status=0
MACHGATE_DYLIB_MAP="$map_file" "$BUILD_DIR/machgate" "$FIXTURE" || status=$?
[ "$status" -eq 0 ]
