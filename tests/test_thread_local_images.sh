#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT
cc -std=c11 -Wall -Wextra -Werror -Isrc tests/thread_local_images_test.c \
    -o "$WORK_DIR/probe" -ldl -pthread
timeout -k 2 10 "$WORK_DIR/probe" "$BUILD_DIR/libsystem_shim.so"
