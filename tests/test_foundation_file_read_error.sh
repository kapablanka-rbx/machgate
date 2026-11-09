#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT
CLANG="$(command -v clang || command -v clang-18)"
LINKER="$(command -v ld64.lld || command -v ld64.lld-18)"
"$CLANG" -target arm64-apple-macos11 -c providers/foundation/archive/tests/imports.s -o "$WORK_DIR/imports.o"
"$LINKER" -arch arm64 -platform_version macos 11.0 11.0 -dylib \
    -install_name /System/Library/Frameworks/Foundation.framework/Versions/C/Foundation \
    -o "$WORK_DIR/Foundation.dylib" "$WORK_DIR/imports.o"
"$CLANG" -target arm64-apple-macos11 -O2 -fno-builtin -fno-stack-protector \
    -c providers/foundation/tests/file_read_error.m -o "$WORK_DIR/guest.o"
"$LINKER" -arch arm64 -platform_version macos 11.0 11.0 -e _main \
    -o "$WORK_DIR/guest" "$WORK_DIR/guest.o" "$WORK_DIR/Foundation.dylib"
printf 'Foundation = %s\n' "$BUILD_DIR/libmachgate_foundation.so" > "$WORK_DIR/providers.conf"
MACHGATE_STARTUP_CACHE=0 MACHGATE_DYLIB_MAP="$WORK_DIR/providers.conf" \
    MACHGATE_SYSTEM_SHIM_LIB="$BUILD_DIR/libsystem_shim.so" \
    "$BUILD_DIR/machgate" "$WORK_DIR/guest"
printf '%s\n' 'Foundation failed file reads return a populated NSError'
