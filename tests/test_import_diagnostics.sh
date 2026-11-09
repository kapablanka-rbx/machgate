#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT
printf 'libMachGateFixture = UNMAPPED\n' > "$WORK_DIR/map"
for fixture in dynamic_partial_used_data dynamic_chained_data_missing; do
    for cache in 0 1 1; do
        MACHGATE_DYLIB_MAP="$WORK_DIR/map" MACHGATE_UNRESOLVED_IMPORTS=stub \
            MACHGATE_VERBOSE=0 MACHGATE_LOG_STARTUP=0 MACHGATE_STARTUP_CACHE="$cache" \
            "$BUILD_DIR/machgate" "tests/fixtures/$fixture" > "$WORK_DIR/output" 2> "$WORK_DIR/error"
        [ ! -s "$WORK_DIR/output" ]
        [ ! -s "$WORK_DIR/error" ]
        MACHGATE_DYLIB_MAP="$WORK_DIR/map" MACHGATE_UNRESOLVED_IMPORTS=stub \
            MACHGATE_VERBOSE=1 MACHGATE_STARTUP_CACHE="$cache" \
            "$BUILD_DIR/machgate" "tests/fixtures/$fixture" > "$WORK_DIR/output" 2> "$WORK_DIR/error"
        rg -q 'unresolved import .* uses compatibility storage' "$WORK_DIR/error"
        result=0
        MACHGATE_DYLIB_MAP="$WORK_DIR/map" MACHGATE_UNRESOLVED_IMPORTS=strict \
            MACHGATE_VERBOSE=0 MACHGATE_LOG_STARTUP=0 MACHGATE_STARTUP_CACHE="$cache" \
            "$BUILD_DIR/machgate" "tests/fixtures/$fixture" > "$WORK_DIR/output" 2> "$WORK_DIR/error" || result=$?
        [ "$result" -eq 1 ]
        rg -q 'native symbol .* was not found' "$WORK_DIR/error"
    done
done
echo 'Import notes obey loader diagnostics; strict errors remain visible'
