#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/.."

MACHGATE_ROOT="$(pwd)"
BUILD_DIR="${BUILD_DIR:-$MACHGATE_ROOT/build}"
case "$BUILD_DIR" in
    /*) ;;
    *) BUILD_DIR="$MACHGATE_ROOT/$BUILD_DIR" ;;
esac
FIXTURE="tests/fixtures/darwin_file_variadic_abi"
TEMP_DIR="tests/fixtures"

[ -f "$FIXTURE" ] || bash tests/fixtures/build_fixtures.sh

cleanup() {
    rm -f "$TEMP_DIR"/darwin_file_{open,openat}_mode.tmp \
          "$TEMP_DIR"/darwin_file_{import,raw_92,raw_406}_{lock,preallocate}.tmp
}
trap cleanup EXIT
cleanup

(
    cd "$TEMP_DIR"
    MACHGATE_SYSTEM_SHIM_LIB="$BUILD_DIR/libsystem_shim.so" \
        "$BUILD_DIR/machgate" "$(basename "$FIXTURE")" >/dev/null 2>&1
)

expected_mode="$(printf '%03o' "$((0640 & ~$(umask)))")"
[ "$(stat -c '%a' "$TEMP_DIR/darwin_file_open_mode.tmp")" = "$expected_mode" ]
[ "$(stat -c '%a' "$TEMP_DIR/darwin_file_openat_mode.tmp")" = "$expected_mode" ]
[ "$(stat -c '%s' "$TEMP_DIR/darwin_file_import_preallocate.tmp")" = "5120" ]
[ "$(stat -c '%s' "$TEMP_DIR/darwin_file_raw_92_preallocate.tmp")" = "5120" ]
[ "$(stat -c '%s' "$TEMP_DIR/darwin_file_raw_406_preallocate.tmp")" = "5120" ]
