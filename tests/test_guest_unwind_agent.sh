#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
MACHGATE_BINARY="${MACHGATE_TEST_BINARY:-$BUILD_DIR/machgate}"
LIBCXX="${MACHGATE_TEST_LIBCXX:-/opt/machgate/lib/libc++.so.1}"
if [ ! -f "$LIBCXX" ]; then
    if [ "${MACHGATE_REQUIRE_LIBCXX:-0}" = 1 ]; then
        printf '%s\n' "Missing required C++ runtime: $LIBCXX" >&2
        exit 1
    fi
    printf '%s\n' 'SKIP: guest unwind scenarios require MACHGATE_TEST_LIBCXX'
    exit 0
fi
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT
ulimit -c 0

CLANGXX="$(command -v clang++ || command -v clang++-18)"
LINKER="$(command -v ld64.lld || command -v ld64.lld-18)"
LLVM_MC="$(command -v llvm-mc || command -v llvm-mc-18)"
LLVM_OBJDUMP="$(command -v llvm-objdump || command -v llvm-objdump-18)"

"$CLANGXX" -target arm64-apple-macos11 -O2 -fno-builtin -fno-stack-protector \
    -fexceptions -funwind-tables -nostdlib \
    -c tests/fixtures/darwin_unwind_agent_pairs.cpp -o "$WORK_DIR/guest.o"
"$LLVM_MC" -triple arm64-apple-macos11 -filetype=obj \
    -o "$WORK_DIR/imports.o" tests/fixtures/darwin_unwind_agent_imports.s
"$LINKER" -arch arm64 -platform_version macos 11.0.0 11.0.0 -dylib \
    -install_name /usr/lib/libc++.1.dylib -o "$WORK_DIR/libc++.1.dylib" \
    "$WORK_DIR/imports.o"
"$LINKER" -arch arm64 -platform_version macos 11.0.0 11.0.0 -e _main \
    -o "$WORK_DIR/guest" "$WORK_DIR/guest.o" "$WORK_DIR/libc++.1.dylib"

unwind_encodings="$("$LLVM_OBJDUMP" --macho --unwind-info "$WORK_DIR/guest")"
for required_encoding in 0x54000001 0x54000101 0x02000000; do
    if ! grep -q "$required_encoding" <<<"$unwind_encodings"; then
        printf '%s\n' "guest compact unwind is missing encoding $required_encoding" >&2
        exit 1
    fi
done

printf 'libc++.1 = %s\n' "$LIBCXX" > "$WORK_DIR/providers.conf"

expected_output="start
deep-chain
deep-caught
pair-rethrow
inner-caught
pair-caught
exception-ptr
after-capture
rethrown-caught
done"

for CACHE in 0 1 1; do
    set +e
    actual_output="$(
        MACHGATE_STARTUP_CACHE="$CACHE" MACHGATE_STARTUP_CACHE_DIR="$WORK_DIR/cache" \
        MACHGATE_DYLIB_MAP="$WORK_DIR/providers.conf" \
        MACHGATE_SYSTEM_SHIM_LIB="$BUILD_DIR/libsystem_shim.so" \
        LD_LIBRARY_PATH="$(dirname "$LIBCXX")" \
        timeout -k 2 20 "$MACHGATE_BINARY" "$WORK_DIR/guest"
    )"
    exit_status=$?
    set -e
    if [ "$exit_status" -ne 42 ]; then
        printf '%s\n' "guest exit status $exit_status (expected 42), cache=$CACHE" >&2
        exit 1
    fi
    if [ "$actual_output" != "$expected_output" ]; then
        printf '%s\n' "unexpected guest output, cache=$CACHE:" >&2
        printf '%s\n' "$actual_output" >&2
        exit 1
    fi
done

printf '%s\n' 'guest unwind: exception scenarios pass through pair-saving frames, rethrow, and exception_ptr capture'
