#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
FIXTURE="tests/fixtures/darwin_dynamic_lookup"
MAP_FILE="$(mktemp)"
trap 'rm -f "$MAP_FILE"' EXIT

[ -f "$FIXTURE" ] || bash tests/fixtures/build_fixtures.sh
printf 'libSystem.B = %s/libsystem_shim.so\n' "$BUILD_DIR" > "$MAP_FILE"

status=0
MACHGATE_DYLIB_MAP="$MAP_FILE" MACHGATE_UNRESOLVED_IMPORTS=strict \
    "$BUILD_DIR/machgate" "$FIXTURE" || status=$?
if [ "$status" -ne 0 ]; then
    printf 'Darwin dynamic lookup failed at check %d\n' "$status" >&2
    exit 1
fi

if [ -f "$BUILD_DIR/libmachgate_apple_compat.so" ]; then
    printf 'libSystem.B = %s/libmachgate_apple_compat.so\n' "$BUILD_DIR" > "$MAP_FILE"
    MACHGATE_DYLIB_MAP="$MAP_FILE" MACHGATE_UNRESOLVED_IMPORTS=strict \
        "$BUILD_DIR/machgate" "$FIXTURE"
fi

cc -Wall -Wextra -Werror -I src -o "$BUILD_DIR/guest_dynamic_lookup_provider_test" \
    tests/guest_dynamic_lookup_provider_test.c -ldl
"$BUILD_DIR/guest_dynamic_lookup_provider_test" "$BUILD_DIR/libsystem_shim.so"
