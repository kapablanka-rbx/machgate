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
    for symbol in dladdr _dyld_image_count _dyld_get_image_header _dyld_get_image_name _dyld_get_image_vmaddr_slide backtrace_symbols backtrace_symbols_fd pipe close read free strcmp strstr; do
        printf '.globl _%s\n_%s:\n ret\n' "$symbol" "$symbol"
    done
} > "$WORK_DIR/imports.s"
printf '.text\n.globl _dependency_value\n_dependency_value:\n mov x0, #5\n ret\n' > "$WORK_DIR/dependency.s"
"$CLANG" -target arm64-apple-macos11 -c "$WORK_DIR/imports.s" -o "$WORK_DIR/imports.o"
"$CLANG" -target arm64-apple-macos11 -c "$WORK_DIR/dependency.s" -o "$WORK_DIR/dependency.o"
"$LINKER" -arch arm64 -platform_version macos 11 11 -dylib \
    -install_name /usr/lib/libSystem.B.dylib -o "$WORK_DIR/libSystem.dylib" "$WORK_DIR/imports.o"
"$LINKER" -arch arm64 -platform_version macos 11 11 -dylib \
    -install_name @executable_path/dependency.dylib -o "$WORK_DIR/dependency.dylib" "$WORK_DIR/dependency.o"
"$CLANG" -target arm64-apple-macos11 -O2 -ffreestanding -fno-builtin -fno-stack-protector \
    -c tests/fixtures/darwin_image_symbols.c -o "$WORK_DIR/guest.o"
"$LINKER" -arch arm64 -platform_version macos 11 11 -e _main \
    -o "$WORK_DIR/guest" "$WORK_DIR/guest.o" "$WORK_DIR/libSystem.dylib" "$WORK_DIR/dependency.dylib"
for cache in 0 1 1; do
    MACHGATE_STARTUP_CACHE="$cache" MACHGATE_SYSTEM_SHIM_LIB="$BUILD_DIR/libsystem_shim.so" \
        "$BUILD_DIR/machgate" "$WORK_DIR/guest"
done
