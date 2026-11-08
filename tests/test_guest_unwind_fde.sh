#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/.."

TEST_BINARY="$(mktemp)"
trap 'rm -f "$TEST_BINARY"' EXIT

"${CC:-cc}" -D_GNU_SOURCE -std=gnu11 -Wall -Wextra -Isrc \
    tests/guest_unwind_fde_test.c src/adapters/startup_cache.c \
    src/adapters/dynamic_unwind.c src/domain/dwarf_unwind.c -ldl -pthread -o "$TEST_BINARY"
"$TEST_BINARY"
