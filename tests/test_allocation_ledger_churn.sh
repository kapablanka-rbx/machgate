#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT
cc -O2 -Wall -Wextra -Werror tests/allocation_ledger_churn_test.c \
    -pthread -ldl -o "$WORK_DIR/probe"
timeout -k 2 20 "$WORK_DIR/probe" "$BUILD_DIR/libsystem_shim.so"
