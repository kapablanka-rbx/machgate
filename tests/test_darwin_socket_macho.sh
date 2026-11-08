#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/.."
MACHGATE_ROOT="${MACHGATE_ROOT:-$(pwd)}"
BUILD_DIR="${BUILD_DIR:-$MACHGATE_ROOT/build}"
FIXTURE_DIR="$MACHGATE_ROOT/tests/fixtures"
FIXTURE="$FIXTURE_DIR/darwin_socket_translation"
SOURCE="$FIXTURE_DIR/darwin_socket_translation.s"
LLVM_MC="${LLVM_MC:-$(command -v llvm-mc || command -v llvm-mc-18 || true)}"
LD64_LLD="${LD64_LLD:-$(command -v ld64.lld || command -v ld64.lld-18 || true)}"

[ -n "$LLVM_MC" ] || { echo "llvm-mc not found" >&2; exit 1; }
[ -n "$LD64_LLD" ] || { echo "ld64.lld not found" >&2; exit 1; }
trap 'rm -f "$FIXTURE_DIR/darwin_socket_translation.o"' EXIT

"$LLVM_MC" -triple arm64-apple-macos11 -filetype=obj \
    -o "$FIXTURE_DIR/darwin_socket_translation.o" "$SOURCE"
"$LD64_LLD" -arch arm64 -platform_version macos 11.0.0 11.0.0 \
    -o "$FIXTURE" -e _main "$FIXTURE_DIR/darwin_socket_translation.o"

output="$("$BUILD_DIR/machgate" "$FIXTURE")"
[ "$output" = "socket-translation" ]
