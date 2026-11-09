#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
LIBCXX="${MACHGATE_TEST_LIBCXX:-/opt/machgate/lib/libc++.so.1}"
if [ ! -f "$LIBCXX" ]; then
    if [ "${MACHGATE_REQUIRE_LIBCXX:-0}" = 1 ]; then
        printf 'Missing required C++ runtime: %s\n' "$LIBCXX" >&2
        exit 1
    fi
    echo 'SKIP: Darwin stold requires MACHGATE_TEST_LIBCXX'
    exit 0
fi
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT
CLANGXX="$(command -v clang++ || command -v clang++-18)"
LINKER="$(command -v ld64.lld || command -v ld64.lld-18)"
NM="$(command -v llvm-nm || command -v llvm-nm-18)"
printf 'libc++.1 = %s\n' "$LIBCXX" > "$WORK_DIR/providers.conf"
for platform in macos ios; do
    "$CLANGXX" -target "arm64-apple-${platform}14" -O2 -fno-builtin \
        -fno-stack-protector -fexceptions -funwind-tables -nostdlib \
        -c tests/fixtures/darwin_stold.cpp -o "$WORK_DIR/guest.o"
    "$NM" --undefined-only --just-symbol-name "$WORK_DIR/guest.o" | \
        awk '{print ".globl " $1; print $1 ":"; print "ret"}' > "$WORK_DIR/imports.s"
    printf '.globl dyld_stub_binder\ndyld_stub_binder:\nret\n' >> "$WORK_DIR/imports.s"
    "$CLANGXX" -target "arm64-apple-${platform}14" -c "$WORK_DIR/imports.s" -o "$WORK_DIR/imports.o"
    "$LINKER" -arch arm64 -platform_version "$platform" 14 14 -dylib \
        -install_name /usr/lib/libc++.1.dylib -o "$WORK_DIR/libc++.dylib" "$WORK_DIR/imports.o"
    "$LINKER" -arch arm64 -platform_version "$platform" 14 14 -e _main \
        -o "$WORK_DIR/guest" "$WORK_DIR/guest.o" "$WORK_DIR/libc++.dylib"
    for cache in 0 1 1; do
        MACHGATE_DYLIB_MAP="$WORK_DIR/providers.conf" \
            MACHGATE_STARTUP_CACHE="$cache" MACHGATE_STARTUP_CACHE_DIR="$WORK_DIR/cache" \
            MACHGATE_SYSTEM_SHIM_LIB="$BUILD_DIR/libsystem_shim.so" \
            LD_LIBRARY_PATH="$(dirname "$LIBCXX")" \
            "$BUILD_DIR/machgate" "$WORK_DIR/guest" || {
                result=$?
                printf 'Darwin stold guest failed: platform=%s cache=%s status=%s\n' \
                    "$platform" "$cache" "$result" >&2
                exit "$result"
            }
    done
done
echo 'Darwin stold: binary64 return, consumed input, exceptions and wide strings passed'
