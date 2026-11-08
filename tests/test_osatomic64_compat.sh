#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

cc -Wall -Wextra -O2 -o "$tmpdir/osatomic64_probe" \
    tests/fixtures/osatomic64_probe.c -ldl -pthread
output="$(cd "$BUILD_DIR" && LD_LIBRARY_PATH="$BUILD_DIR" \
    "$tmpdir/osatomic64_probe")"
[ "$output" = "osatomic64-ok" ]
