#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT
CLANG="$(command -v clang || command -v clang-18)"
LINKER="$(command -v ld64.lld || command -v ld64.lld-18)"
for SYMBOL in __Block_copy __Block_release __Block_object_assign __Block_object_dispose \
    __NSConcreteStackBlock __NSConcreteGlobalBlock _memcpy _memmove dyld_stub_binder; do
    printf '.text\n.globl %s\n%s:\n ret\n' "$SYMBOL" "$SYMBOL"
done > "$WORK_DIR/imports.s"
"$CLANG" -target arm64-apple-macos11 -c "$WORK_DIR/imports.s" -o "$WORK_DIR/imports.o"
"$LINKER" -arch arm64 -platform_version macos 11.0 11.0 -dylib \
    -install_name /usr/lib/libSystem.B.dylib -o "$WORK_DIR/libSystem.B.dylib" "$WORK_DIR/imports.o"
"$CLANG" -target arm64-apple-macos11 -O1 -fblocks -fno-stack-protector \
    -c tests/fixtures/darwin_blocks_lifetime.c -o "$WORK_DIR/guest.o"
"$LINKER" -arch arm64 -platform_version macos 11.0 11.0 -e _main \
    -o "$WORK_DIR/guest" "$WORK_DIR/guest.o" "$WORK_DIR/libSystem.B.dylib"
for CACHE in 0 1 1; do
    MACHGATE_STARTUP_CACHE="$CACHE" MACHGATE_SYSTEM_SHIM_LIB="$BUILD_DIR/libsystem_shim.so" \
        "$BUILD_DIR/machgate" "$WORK_DIR/guest"
done
printf '%s\n' 'Darwin Blocks: escaped stack captures, shared byref storage, nested blocks and copy/release passed'
