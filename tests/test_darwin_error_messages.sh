#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
probe_directory="$(mktemp -d)"
trap 'rm -rf "$probe_directory"' EXIT
printf '.text\n.globl _strerror\n_strerror:\n ret\n.globl _strerror_r\n_strerror_r:\n ret\n.globl ___error\n___error:\n ret\n.globl dyld_stub_binder\ndyld_stub_binder:\n ret\n' > "$probe_directory/stub.s"
llvm-mc-18 -triple arm64-apple-macos11 -filetype=obj \
    "$probe_directory/stub.s" -o "$probe_directory/stub.o"
ld64.lld-18 -arch arm64 -platform_version macos 11.0 11.0 -dylib \
    -install_name /usr/lib/libSystem.B.dylib \
    -o "$probe_directory/stub.dylib" "$probe_directory/stub.o"
clang -target arm64-apple-macos11 -x c -O2 -fno-builtin \
    -fno-stack-protector -nostdlib -c tests/fixtures/darwin_error_messages.c \
    -o "$probe_directory/guest.o"
ld64.lld-18 -arch arm64 -platform_version macos 11.0 11.0 \
    -o "$probe_directory/guest" -e _main \
    "$probe_directory/guest.o" "$probe_directory/stub.dylib"
printf 'libSystem.B = %s/libsystem_shim.so\n' "$BUILD_DIR" > "$probe_directory/map"
MACHGATE_SYSTEM_SHIM_LIB="$BUILD_DIR/libsystem_shim.so" \
    MACHGATE_DYLIB_MAP="$probe_directory/map" MACHGATE_STARTUP_CACHE=0 \
    "$BUILD_DIR/machgate" "$probe_directory/guest"
