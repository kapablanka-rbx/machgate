#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
PROVIDER="${MACHGATE_TEST_ICU_PROVIDER:-$BUILD_DIR/libmachgate_icu.so}"
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT
CLANG="$(command -v clang || command -v clang-18)"
LINKER="$(command -v ld64.lld || command -v ld64.lld-18)"
cc -Wall -Wextra -Werror providers/icu/tests/icu_exports.c -ldl -o "$WORK_DIR/exports"
"$WORK_DIR/exports" "$PROVIDER"
cc -Wall -Wextra -Werror -ffixed-x18 providers/icu/tests/icu_behavior.c \
    providers/icu/tests/register_preservation.S "$PROVIDER" \
    -Wl,-rpath,"$(dirname "$PROVIDER")" -o "$WORK_DIR/native"
"$CLANG" -target arm64-apple-macos11 -c providers/icu/tests/imports.S -o "$WORK_DIR/imports.o"
"$LINKER" -arch arm64 -platform_version macos 11.0 11.0 -dylib \
    -install_name /usr/lib/libicucore.A.dylib -o "$WORK_DIR/libicucore.A.dylib" "$WORK_DIR/imports.o"
"$CLANG" -target arm64-apple-macos11 -O2 -fno-builtin -fno-stack-protector \
    -c providers/icu/tests/icu_behavior.c -o "$WORK_DIR/guest.o"
"$CLANG" -target arm64-apple-macos11 -c providers/icu/tests/register_preservation.S \
    -o "$WORK_DIR/registers.o"
"$LINKER" -arch arm64 -platform_version macos 11.0 11.0 -e _main \
    -o "$WORK_DIR/guest" "$WORK_DIR/guest.o" "$WORK_DIR/registers.o" "$WORK_DIR/libicucore.A.dylib"
printf 'libicucore.A = %s\n' "$PROVIDER" > "$WORK_DIR/providers.conf"
for CACHE in 0 1 1; do
    MACHGATE_STARTUP_CACHE="$CACHE" MACHGATE_DYLIB_MAP="$WORK_DIR/providers.conf" \
    MACHGATE_SYSTEM_SHIM_LIB="$BUILD_DIR/libsystem_shim.so" \
    "$BUILD_DIR/machgate" "$WORK_DIR/guest"
done
"$WORK_DIR/native"
printf '%s\n' 'ICU provider: Unicode casing, normalization, IDNA, graphemes, numbers, dates, and version passed'
