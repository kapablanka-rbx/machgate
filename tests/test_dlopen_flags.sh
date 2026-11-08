#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
FIXTURE="tests/fixtures/darwin_dlopen_flags"
MAP_FILE="$(mktemp)"
trap 'rm -f "$MAP_FILE"' EXIT
[ -f "$FIXTURE" ] || bash tests/fixtures/build_fixtures.sh
cc -Wall -Wextra -Werror -fPIC -shared -o "$BUILD_DIR/libdlopen_marker.so" \
    tests/fixtures/dlopen_marker.c
cc -Wall -Wextra -Werror -o "$BUILD_DIR/dlopen_flags_probe" \
    tests/fixtures/dlopen_flags_probe.c -ldl
"$BUILD_DIR/dlopen_flags_probe" "$BUILD_DIR/libsystem_shim.so" \
    "$BUILD_DIR/libdlopen_marker.so"
printf 'libSystem.B = %s/libsystem_shim.so\n' "$BUILD_DIR" > "$MAP_FILE"
MACHGATE_DYLIB_MAP="$MAP_FILE" MACHGATE_UNRESOLVED_IMPORTS=strict \
    "$BUILD_DIR/machgate" "$FIXTURE" "$BUILD_DIR/libdlopen_marker.so"
