#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT
CLANG="$(command -v clang || command -v clang-18)"
LINKER="$(command -v ld64.lld || command -v ld64.lld-18)"
printf '.text\n.globl ___error\n___error:\n ret\n.globl _pthread_self\n_pthread_self:\n ret\n.globl _pthread_setname_np\n_pthread_setname_np:\n ret\n.globl _pthread_getname_np\n_pthread_getname_np:\n ret\n.globl dyld_stub_binder\ndyld_stub_binder:\n ret\n' > "$WORK_DIR/imports.s"
for platform in macos ios; do
    "$CLANG" -target "arm64-apple-${platform}14" -c "$WORK_DIR/imports.s" -o "$WORK_DIR/imports.o"
    "$LINKER" -arch arm64 -platform_version "$platform" 14 14 -dylib \
        -install_name /usr/lib/libSystem.B.dylib -o "$WORK_DIR/libSystem.dylib" "$WORK_DIR/imports.o"
    "$CLANG" -target "arm64-apple-${platform}14" -O2 -ffreestanding -fno-builtin -fno-stack-protector \
        -c tests/fixtures/darwin_thread_name.c -o "$WORK_DIR/guest.o"
    "$LINKER" -arch arm64 -platform_version "$platform" 14 14 -e _main \
        -o "$WORK_DIR/guest" "$WORK_DIR/guest.o" "$WORK_DIR/libSystem.dylib"
    for mode in stub strict; do
        for cache in 0 1 1; do
            MACHGATE_UNRESOLVED_IMPORTS="$mode" MACHGATE_STARTUP_CACHE="$cache" \
                MACHGATE_SYSTEM_SHIM_LIB="$BUILD_DIR/libsystem_shim.so" \
                "$BUILD_DIR/machgate" "$WORK_DIR/guest"
        done
    done
done
echo 'Darwin thread names: full names, length limits, truncation and reset passed'
