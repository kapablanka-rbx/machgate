#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
probe="$(mktemp)"
trap 'rm -f "$probe"' EXIT
"${CC:-cc}" -Wall -Wextra -Werror tests/arm_feature_sysctl_test.c -ldl -o "$probe"
"$probe" "$BUILD_DIR/libsystem_shim.so"
