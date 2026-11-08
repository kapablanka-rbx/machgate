#!/bin/bash
# Test: attr operations remain memory-safe during concurrent destroy/reinit.
set -euo pipefail

cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
probe_directory="$(mktemp -d)"
trap 'rm -rf "$probe_directory"' EXIT

cc -Wall -Wextra -Werror -O2 -g -fsanitize=address \
    -o "$probe_directory/pthread_attr_lifecycle_probe" \
    tests/fixtures/pthread_attr_lifecycle_probe.c -ldl -lpthread

ASAN_OPTIONS=detect_leaks=0 \
LD_LIBRARY_PATH="$BUILD_DIR" "$probe_directory/pthread_attr_lifecycle_probe" \
    "$BUILD_DIR/libsystem_shim.so"
