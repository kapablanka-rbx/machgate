#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
MACHGATE_BINARY="${MACHGATE_TEST_BINARY:-$BUILD_DIR/machgate}"
PROVIDER="$BUILD_DIR/libmachgate_foundation.so"
LIBCXX="${MACHGATE_TEST_LIBCXX:-/opt/machgate/lib/libc++.so.1}"
if [ ! -f "$LIBCXX" ]; then
    if [ "${MACHGATE_REQUIRE_LIBCXX:-0}" = 1 ]; then
        printf '%s\n' "Missing required C++ runtime: $LIBCXX" >&2
        exit 1
    fi
    printf '%s\n' 'SKIP: Objective-C exception guest requires MACHGATE_TEST_LIBCXX'
    exit 0
fi
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT
ulimit -c 0

CLANGXX="$(command -v clang++ || command -v clang++-18)"
LINKER="$(command -v ld64.lld || command -v ld64.lld-18)"
"$CLANGXX" -target arm64-apple-ios14 -O1 -fno-stack-protector \
    -c providers/foundation/tests/exception_personality.cpp -o "$WORK_DIR/guest.o"
"$CLANGXX" -target arm64-apple-ios14 -c providers/icu/tests/cxx_imports.S \
    -o "$WORK_DIR/cxx_imports.o"
"$LINKER" -arch arm64 -platform_version ios 14.0 14.0 -dylib \
    -install_name /usr/lib/libc++.1.dylib -o "$WORK_DIR/libc++.1.dylib" \
    "$WORK_DIR/cxx_imports.o"
"$CLANGXX" -target arm64-apple-ios14 -fobjc-exceptions -fexceptions -O1 -c providers/foundation/tests/exception_frame.m -o "$WORK_DIR/frame.o"
cat > "$WORK_DIR/objc_imports.s" <<'ASM'
.text
.globl ___objc_personality_v0
.globl _objc_begin_catch
.globl _objc_end_catch
.globl _objc_terminate
.globl _objc_exception_rethrow
___objc_personality_v0:
_objc_begin_catch:
_objc_end_catch:
_objc_exception_rethrow:
_objc_terminate:
 ret
.data
.globl _OBJC_EHTYPE_$_NSException
_OBJC_EHTYPE_$_NSException:
 .space 24
.globl __objc_empty_cache
__objc_empty_cache:
 .space 16
.globl _objc_ehtype_vtable
_objc_ehtype_vtable:
 .space 104
ASM
"$CLANGXX" -target arm64-apple-ios14 -c "$WORK_DIR/objc_imports.s" \
    -o "$WORK_DIR/objc_imports.o"
"$LINKER" -arch arm64 -platform_version ios 14.0 14.0 -dylib \
    -install_name /usr/lib/libobjc.A.dylib -o "$WORK_DIR/libobjc.A.dylib" \
    "$WORK_DIR/objc_imports.o"
printf 'libobjc.A = %s\nlibc++.1 = %s\n' "$PROVIDER" "$LIBCXX" > "$WORK_DIR/providers.conf"

for FIRST in objc.A c++.1; do
    SECOND=c++.1
    if [ "$FIRST" = c++.1 ]; then SECOND=objc.A; fi
    "$LINKER" -arch arm64 -platform_version ios 14.0 14.0 -e _main \
        -o "$WORK_DIR/guest-$FIRST" "$WORK_DIR/guest.o" "$WORK_DIR/frame.o" \
        "$WORK_DIR/lib$FIRST.dylib" "$WORK_DIR/lib$SECOND.dylib"
    for CACHE in 0 1 1; do
        LD_LIBRARY_PATH="$BUILD_DIR:$(dirname "$LIBCXX")${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
        MACHGATE_STARTUP_CACHE="$CACHE" MACHGATE_STARTUP_CACHE_DIR="$WORK_DIR/cache" \
        MACHGATE_DYLIB_MAP="$WORK_DIR/providers.conf" \
        MACHGATE_SYSTEM_SHIM_LIB="$BUILD_DIR/libsystem_shim.so" \
        timeout -k 2 10 "$MACHGATE_BINARY" "$WORK_DIR/guest-$FIRST"
    done
done
printf '%s\n' 'Objective-C unwind: C++ exception traverses finally and reaches base catch in both load orders'
