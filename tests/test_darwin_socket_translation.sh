#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/.."

TEST_BINARY="$(mktemp)"
trap 'rm -f "$TEST_BINARY"' EXIT

"${CC:-cc}" -std=gnu11 -Wall -Wextra -Werror -D_GNU_SOURCE -Isrc \
    src/adapters/linux_socket_state.c src/syscall/darwin_socket.c tests/darwin_socket_translation_test.c \
    -o "$TEST_BINARY"
"$TEST_BINARY"
