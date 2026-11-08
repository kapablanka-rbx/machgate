#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT
CLANG="$(command -v clang || command -v clang-18)"
LINKER="$(command -v ld64.lld || command -v ld64.lld-18)"
symbols=(pthread_mutexattr_init pthread_mutexattr_destroy pthread_mutexattr_settype
    pthread_mutexattr_gettype pthread_mutex_init pthread_mutex_destroy pthread_mutex_lock
    pthread_mutex_trylock pthread_mutex_unlock)
{
    printf '.text\n.globl dyld_stub_binder\ndyld_stub_binder:\n ret\n'
    for symbol in "${symbols[@]}"; do
        printf '.globl _%s\n_%s:\n ret\n' "$symbol" "$symbol"
    done
} > "$WORK_DIR/imports.s"
"$CLANG" -target arm64-apple-ios14 -c "$WORK_DIR/imports.s" -o "$WORK_DIR/imports.o"
"$LINKER" -arch arm64 -platform_version ios 14.0 14.0 -dylib \
    -install_name /usr/lib/libSystem.B.dylib -o "$WORK_DIR/libSystem.dylib" "$WORK_DIR/imports.o"
"$CLANG" -target arm64-apple-ios14 -O2 -fno-builtin -fno-stack-protector \
    -c tests/fixtures/pthread_mutex_attributes.c -o "$WORK_DIR/guest.o"
"$LINKER" -arch arm64 -platform_version ios 14.0 14.0 -e _main \
    -o "$WORK_DIR/guest" "$WORK_DIR/guest.o" "$WORK_DIR/libSystem.dylib"
failed=0
for cache in 0 1 1; do
    for type in 0 1 2 3; do
        if MACHGATE_STARTUP_CACHE="$cache" MACHGATE_SYSTEM_SHIM_LIB="$BUILD_DIR/libsystem_shim.so" \
            timeout 5 "$BUILD_DIR/machgate" "$WORK_DIR/guest" "$type"; then
            printf 'PASS: mutex type %s, cache %s\n' "$type" "$cache"
        else
            result=$?
            printf 'FAIL: mutex type %s, cache %s, exit %s\n' "$type" "$cache" "$result"
            failed=$((failed + 1))
        fi
    done
done
test "$failed" = 0
BUILD_DIR="$BUILD_DIR" python3 - <<'PY'
import ctypes
import os

shim = ctypes.CDLL(os.path.join(os.environ['BUILD_DIR'], 'libsystem_shim.so'))
attributes = (ctypes.c_long * 2)()
mutex = (ctypes.c_long * 8)()
for name in ('pthread_mutexattr_init', 'pthread_mutexattr_destroy',
             'pthread_mutex_lock', 'pthread_mutex_trylock', 'pthread_mutex_unlock',
             'pthread_mutex_destroy'):
    getattr(shim, name).argtypes = [ctypes.c_void_p]
shim.pthread_mutexattr_settype.argtypes = [ctypes.c_void_p, ctypes.c_int]
shim.pthread_mutex_init.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
assert shim.pthread_mutexattr_init(attributes) == 0
assert shim.pthread_mutexattr_settype(attributes, 1) == 0
assert shim.pthread_mutex_init(mutex, attributes) == 0
assert shim.pthread_mutexattr_destroy(attributes) == 0
assert shim.pthread_mutex_lock(mutex) == 0
assert shim.pthread_mutex_trylock(mutex) == 0
assert shim.pthread_mutex_unlock(mutex) == 0
assert shim.pthread_mutex_unlock(mutex) == 0
assert shim.pthread_mutex_destroy(mutex) == 0
print('PASS: native provider recursive mutex keeps Linux enum semantics')
PY
