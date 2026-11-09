#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
MACHGATE_BINARY="${MACHGATE_TEST_BINARY:-$BUILD_DIR/machgate}"
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT

CLANG="$(command -v clang || command -v clang-18)"
LINKER="$(command -v ld64.lld || command -v ld64.lld-18)"

"$CLANG" -target arm64-apple-macos11 -O2 -fno-builtin -fno-stack-protector \
    -nostdlib -c tests/fixtures/darwin_keychain_agent.c -o "$WORK_DIR/guest.o"
"$CLANG" -target arm64-apple-macos11 -c \
    tests/fixtures/darwin_keychain_agent_imports.s -o "$WORK_DIR/imports.o"
"$LINKER" -arch arm64 -platform_version macos 11.0.0 11.0.0 -dylib \
    -install_name /usr/lib/libSystem.B.dylib -o "$WORK_DIR/libSystem.dylib" \
    "$WORK_DIR/imports.o"
"$LINKER" -arch arm64 -platform_version macos 11.0.0 11.0.0 -e _main \
    -o "$WORK_DIR/guest" "$WORK_DIR/guest.o" "$WORK_DIR/libSystem.dylib"

expected_output='keychain generic-password add/copy/update/delete behavior passed'
for CACHE in 0 1 1; do
    set +e
    actual_output="$(
        MACHGATE_STARTUP_CACHE="$CACHE" MACHGATE_STARTUP_CACHE_DIR="$WORK_DIR/cache" \
        MACHGATE_SYSTEM_SHIM_LIB="$BUILD_DIR/libsystem_shim.so" \
        timeout -k 2 20 "$MACHGATE_BINARY" "$WORK_DIR/guest"
    )"
    exit_status=$?
    set -e
    if [ "$exit_status" -ne 0 ]; then
        printf '%s\n' "guest exit status $exit_status (expected 0), cache=$CACHE" >&2
        printf '%s\n' "$actual_output" >&2
        exit 1
    fi
    if [ "$actual_output" != "$expected_output" ]; then
        printf '%s\n' "unexpected guest output, cache=$CACHE:" >&2
        printf '%s\n' "$actual_output" >&2
        exit 1
    fi
done

printf '%s\n' 'Keychain: in-memory generic-password CRUD, UTF-8 accounts, long and embedded-NUL data passed'
