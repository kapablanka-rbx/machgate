#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
SHIM_PATH="${SHIM_PATH:-$BUILD_DIR/libsystem_shim.so}"
python3 tests/archive_system_compat_test.py "$SHIM_PATH" "$BUILD_DIR/libmachgate_apple_compat.so"

TEST_DIR="$BUILD_DIR/archive-system-test"
mkdir -p "$TEST_DIR"
cc -Wall -Wextra -Werror -O2 -ffixed-x18 tests/fixtures/archive_quick_exit_abi.c \
    -o "$TEST_DIR/quick-exit-abi" -ldl
set +e
callback_result="$("$TEST_DIR/quick-exit-abi" "$SHIM_PATH")"
exit_result=$?
set -e
test "$exit_result" = 42
test "$callback_result" = 1
llvm-mc-18 -triple=arm64-apple-macos11 -filetype=obj \
    tests/fixtures/archive_system_imports.s -o "$TEST_DIR/imports.o"
ld64.lld-18 -dylib -arch arm64 -platform_version macos 11.0 11.0 \
    -install_name /usr/lib/libSystem.B.dylib -o "$TEST_DIR/libSystem.B.dylib" \
    "$TEST_DIR/imports.o"
llvm-mc-18 -triple=arm64-apple-macos11 -filetype=obj \
    tests/fixtures/archive_system_abi.s -o "$TEST_DIR/guest.o"
ld64.lld-18 -arch arm64 -platform_version macos 11.0 11.0 -e _main \
    -o "$TEST_DIR/guest" "$TEST_DIR/guest.o" "$TEST_DIR/libSystem.B.dylib"
printf 'libSystem.B = %s\n' "$SHIM_PATH" > "$TEST_DIR/dylib-map.conf"
MACHGATE_DYLIB_MAP="$TEST_DIR/dylib-map.conf" MACHGATE_SYSTEM_SHIM_LIB="$SHIM_PATH" \
    MACHGATE_UNRESOLVED_IMPORTS=strict \
    "$BUILD_DIR/machgate" "$TEST_DIR/guest"
