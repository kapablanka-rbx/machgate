#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
MACHGATE_BINARY="${MACHGATE_TEST_BINARY:-$BUILD_DIR/machgate}"
PROVIDER="${MACHGATE_TEST_ICU_PROVIDER:-$BUILD_DIR/libmachgate_icu.so}"
LIBCXX="${MACHGATE_TEST_LIBCXX:-/opt/machgate/lib/libc++.so.1}"
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT
ulimit -c 0

cc -Wall -Wextra -Werror providers/icu/tests/runtime_visibility.c -ldl \
    -o "$WORK_DIR/visibility"
if [ ! -f "$LIBCXX" ]; then
    "$WORK_DIR/visibility" "$PROVIDER"
    printf '%s\n' 'ICU runtime visibility passed; guest exception check needs MACHGATE_TEST_LIBCXX'
    exit 0
fi

CLANGXX="$(command -v clang++ || command -v clang++-18)"
LINKER="$(command -v ld64.lld || command -v ld64.lld-18)"
"$CLANGXX" -target arm64-apple-macos11 -O1 -fno-stack-protector \
    -c providers/icu/tests/cxx_exception.cpp -o "$WORK_DIR/guest.o"
"$CLANGXX" -target arm64-apple-macos11 -c providers/icu/tests/cxx_imports.S \
    -o "$WORK_DIR/cxx_imports.o"
"$LINKER" -arch arm64 -platform_version macos 11.0 11.0 -dylib \
    -install_name /usr/lib/libc++.1.dylib -o "$WORK_DIR/libc++.1.dylib" \
    "$WORK_DIR/cxx_imports.o"
"$CLANGXX" -target arm64-apple-macos11 -c providers/icu/tests/imports.S \
    -o "$WORK_DIR/icu_imports.o"
"$LINKER" -arch arm64 -platform_version macos 11.0 11.0 -dylib \
    -install_name /usr/lib/libicucore.A.dylib -o "$WORK_DIR/libicucore.A.dylib" \
    "$WORK_DIR/icu_imports.o"
printf 'libicucore.A = %s\nlibc++.1 = %s\n' "$PROVIDER" "$LIBCXX" > "$WORK_DIR/providers.conf"

for FIRST in icucore.A c++.1; do
    SECOND=c++.1
    if [ "$FIRST" = c++.1 ]; then SECOND=icucore.A; fi
    "$LINKER" -arch arm64 -platform_version macos 11.0 11.0 -e _main \
        -o "$WORK_DIR/guest-$FIRST" "$WORK_DIR/guest.o" \
        "$WORK_DIR/lib$FIRST.dylib" "$WORK_DIR/lib$SECOND.dylib"
    for CACHE in 0 1 1; do
        LD_LIBRARY_PATH="$(dirname "$LIBCXX")${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
        MACHGATE_STARTUP_CACHE="$CACHE" MACHGATE_STARTUP_CACHE_DIR="$WORK_DIR/cache" \
        MACHGATE_DYLIB_MAP="$WORK_DIR/providers.conf" \
        MACHGATE_SYSTEM_SHIM_LIB="$BUILD_DIR/libsystem_shim.so" \
        timeout -k 2 10 "$MACHGATE_BINARY" "$WORK_DIR/guest-$FIRST"
    done
done
"$WORK_DIR/visibility" "$PROVIDER"
printf '%s\n' 'ICU and libc++: derived exception caught through its base in both load orders'
