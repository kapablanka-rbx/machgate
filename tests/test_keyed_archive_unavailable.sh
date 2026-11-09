#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT
"${CC:-cc}" -std=gnu11 -O2 -ffunction-sections -fdata-sections \
    -I src -I providers/foundation tests/keyed_archive_unavailable_test.c \
    -Wl,--gc-sections -ldl -lm -pthread -o "$work_dir/test"
"$work_dir/test"
printf '%s\n' 'Unsupported keyed archives return no object and an explicit error'
