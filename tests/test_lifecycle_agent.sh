#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT
CLANG="$(command -v clang || command -v clang-18)"
LINKER="$(command -v ld64.lld || command -v ld64.lld-18)"
"$CLANG" -target arm64-apple-macos11 -c tests/fixtures/darwin_lifecycle_agent_imports.s \
    -o "$WORK_DIR/imports.o"
"$LINKER" -arch arm64 -platform_version macos 11.0.0 11.0.0 -dylib \
    -install_name /usr/lib/libSystem.B.dylib -o "$WORK_DIR/libSystem.dylib" \
    "$WORK_DIR/imports.o"
"$CLANG" -target arm64-apple-macos11 -O2 -ffreestanding -fno-builtin \
    -fno-stack-protector -c tests/fixtures/darwin_lifecycle_agent.c \
    -o "$WORK_DIR/guest.o"
"$LINKER" -arch arm64 -platform_version macos 11.0.0 11.0.0 -e _main \
    -o "$WORK_DIR/guest" "$WORK_DIR/guest.o" "$WORK_DIR/libSystem.dylib"
for mode in stub strict; do
    for cache in 0 1 1; do
        MACHGATE_UNRESOLVED_IMPORTS="$mode" MACHGATE_STARTUP_CACHE="$cache" \
            MACHGATE_SYSTEM_SHIM_LIB="$BUILD_DIR/libsystem_shim.so" \
            "$BUILD_DIR/machgate" "$WORK_DIR/guest"
    done
done
echo 'Darwin unavailable network metrics preserve errors, buffers and interface addresses'
