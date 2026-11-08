#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
probe_directory="$(mktemp -d)"
trap 'rm -rf "$probe_directory"' EXIT
llvm-mc-18 -triple arm64-apple-macos11 -filetype=obj \
    tests/fixtures/tlv_context_provider_stub.s -o "$probe_directory/stub.o"
ld64.lld-18 -arch arm64 -platform_version macos 11.0 11.0 -dylib \
    -install_name /usr/lib/libSystem.B.dylib \
    -o "$probe_directory/stub.dylib" "$probe_directory/stub.o"
clang -target arm64-apple-macos11 -x c -O2 -fno-builtin \
    -fno-stack-protector -nostdlib -c tests/fixtures/darwin_pthread_key_abi.c \
    -o "$probe_directory/guest.o"
ld64.lld-18 -arch arm64 -platform_version macos 11.0 11.0 \
    -o "$probe_directory/guest" -e _main \
    "$probe_directory/guest.o" "$probe_directory/stub.dylib"
printf 'libSystem.B = %s/libsystem_shim.so\n' "$BUILD_DIR" > "$probe_directory/map"
MACHGATE_SYSTEM_SHIM_LIB="$BUILD_DIR/libsystem_shim.so" \
    MACHGATE_DYLIB_MAP="$probe_directory/map" MACHGATE_STARTUP_CACHE=0 \
    "$BUILD_DIR/machgate" "$probe_directory/guest"
