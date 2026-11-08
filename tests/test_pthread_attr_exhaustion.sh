#!/bin/bash
# Test: leaked Darwin attrs do not exhaust translation and raw attrs never reach glibc.
set -euo pipefail

cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
probe_directory="$(mktemp -d)"
trap 'rm -rf "$probe_directory"' EXIT

cc -Wall -Wextra -Werror -o "$probe_directory/pthread_attr_exhaustion_probe" \
    tests/fixtures/pthread_attr_exhaustion_probe.c -ldl -lpthread

LD_LIBRARY_PATH="$BUILD_DIR" "$probe_directory/pthread_attr_exhaustion_probe" \
    "$BUILD_DIR/libsystem_shim.so"
