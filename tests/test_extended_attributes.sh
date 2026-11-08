#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT
CLANG="$(command -v clang || command -v clang-18)"
LINKER="$(command -v ld64.lld || command -v ld64.lld-18)"
{
    printf '.text\n.globl dyld_stub_binder\ndyld_stub_binder:\n ret\n'
    for symbol in __error open close strcmp getxattr fgetxattr setxattr fsetxattr removexattr fremovexattr listxattr flistxattr; do
        printf '.globl _%s\n_%s:\n ret\n' "$symbol" "$symbol"
    done
} > "$WORK_DIR/imports.s"
"$CLANG" -target arm64-apple-macos11 -c "$WORK_DIR/imports.s" -o "$WORK_DIR/imports.o"
"$LINKER" -arch arm64 -platform_version macos 11 11 -dylib \
    -install_name /usr/lib/libSystem.B.dylib -o "$WORK_DIR/libSystem.dylib" "$WORK_DIR/imports.o"
"$CLANG" -target arm64-apple-macos11 -O2 -ffreestanding -fno-builtin -fno-stack-protector \
    -c tests/fixtures/darwin_extended_attributes.c -o "$WORK_DIR/guest.o"
"$LINKER" -arch arm64 -platform_version macos 11 11 -e _main \
    -o "$WORK_DIR/guest" "$WORK_DIR/guest.o" "$WORK_DIR/libSystem.dylib"
failed=0
for direct in 0 1; do
    for cache in 0 1 1; do
        fixture="$(mktemp "$WORK_DIR/file.XXXXXX")"
        if MACHGATE_STARTUP_CACHE="$cache" MACHGATE_SYSTEM_SHIM_LIB="$BUILD_DIR/libsystem_shim.so" \
            "$BUILD_DIR/machgate" "$WORK_DIR/guest" "$fixture" "$direct"; then
            printf 'PASS: xattr direct=%s cache=%s\n' "$direct" "$cache"
        else
            result=$?
            printf 'FAIL: xattr direct=%s cache=%s exit=%s\n' "$direct" "$cache" "$result"
            failed=$((failed + 1))
        fi
    done
done
test "$failed" = 0
