#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT
CLANG="$(command -v clang || command -v clang-18)"
LINKER="$(command -v ld64.lld || command -v ld64.lld-18)"
cat > "$WORK_DIR/imports.s" <<'ASM'
.text
.globl dyld_stub_binder
dyld_stub_binder:
 ret
.globl _objc_msgSend
_objc_msgSend:
 ret
.globl _objc_alloc
_objc_alloc:
 ret
.globl _objc_alloc_init
_objc_alloc_init:
 ret
.globl _objc_getClass
_objc_getClass:
 ret
.data
.globl _OBJC_CLASS_$_NSString
_OBJC_CLASS_$_NSString:
 .quad 0
.globl _OBJC_CLASS_$_NSCharacterSet
_OBJC_CLASS_$_NSCharacterSet:
 .quad 0
.globl _OBJC_CLASS_$_NSURLComponents
_OBJC_CLASS_$_NSURLComponents:
 .quad 0
ASM
"$CLANG" -target arm64-apple-ios14 -c "$WORK_DIR/imports.s" -o "$WORK_DIR/imports.o"
"$LINKER" -arch arm64 -platform_version ios 14.0 14.0 -dylib \
    -install_name /System/Library/Frameworks/Foundation.framework/Foundation \
    -o "$WORK_DIR/Foundation.dylib" "$WORK_DIR/imports.o"
"$CLANG" -target arm64-apple-ios14 -O2 -fno-builtin -fno-stack-protector \
    -c providers/foundation/tests/allocation_init.m -o "$WORK_DIR/guest.o"
"$LINKER" -arch arm64 -platform_version ios 14.0 14.0 -e _main \
    -o "$WORK_DIR/guest" "$WORK_DIR/guest.o" "$WORK_DIR/Foundation.dylib"
printf 'Foundation = %s\n' "$BUILD_DIR/libmachgate_foundation.so" > "$WORK_DIR/providers.conf"
for CACHE in 0 1 1; do
    MACHGATE_STARTUP_CACHE="$CACHE" MACHGATE_DYLIB_MAP="$WORK_DIR/providers.conf" \
    MACHGATE_SYSTEM_SHIM_LIB="$BUILD_DIR/libsystem_shim.so" \
    "$BUILD_DIR/machgate" "$WORK_DIR/guest"
done
printf '%s\n' 'Foundation allocation: combined alloc/init, nil class and string trimming passed'
