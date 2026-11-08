#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
LLVM_MC="${LLVM_MC:-$(command -v llvm-mc || command -v llvm-mc-18)}"
LD64_LLD="${LD64_LLD:-$(command -v ld64.lld || command -v ld64.lld-18)}"
fixture_directory="$BUILD_DIR/dynamic-unwind-fixtures"
mkdir -p "$fixture_directory"
"${CC:-cc}" -std=gnu11 -O2 -Wall -Wextra -Werror -D_GNU_SOURCE -Isrc -rdynamic \
    tests/dynamic_unwind_test.c src/domain/dwarf_unwind.c src/adapters/dynamic_unwind.c \
    -ldl -pthread -o "$fixture_directory/native-test"
"$fixture_directory/native-test"
"$LLVM_MC" -triple arm64-apple-macos11 -filetype=obj \
    tests/fixtures/dynamic_unwind_stub.s -o "$fixture_directory/stub.o"
"$LD64_LLD" -arch arm64 -platform_version macos 11.0.0 11.0.0 \
    -dylib -install_name /usr/lib/libSystem.B.dylib \
    "$fixture_directory/stub.o" -o "$fixture_directory/stub.dylib"
"$LLVM_MC" -triple arm64-apple-macos11 -filetype=obj \
    tests/fixtures/darwin_dynamic_unwind.s -o "$fixture_directory/guest.o"
"$LD64_LLD" -arch arm64 -platform_version macos 11.0.0 11.0.0 \
    -e _main "$fixture_directory/guest.o" "$fixture_directory/stub.dylib" \
    -o "$fixture_directory/guest"
printf 'libSystem.B = %s/libsystem_shim.so\n' "$BUILD_DIR" > "$fixture_directory/map.conf"
MACHGATE_DYLIB_MAP="$fixture_directory/map.conf" MACHGATE_UNRESOLVED_IMPORTS=strict \
    "$BUILD_DIR/machgate" "$fixture_directory/guest"
