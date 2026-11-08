#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
test_binary="$(mktemp)"
trap 'rm -f "$test_binary"' EXIT
"${CC:-cc}" -std=gnu11 -Wall -Wextra -Werror -D_GNU_SOURCE -Isrc \
    src/instruction_adaptation.c src/adapters/startup_cache.c \
    tests/instruction_adaptation_large_image_test.c -o "$test_binary"
"$test_binary"
