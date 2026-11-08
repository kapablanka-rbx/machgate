#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
map_file="$(mktemp)"
test_binary="$(mktemp)"
trap 'rm -f "$map_file" "$test_binary"' EXIT
"${CC:-cc}" -std=gnu11 -Wall -Wextra -Werror -D_GNU_SOURCE -Isrc \
    src/shim/interface_addresses_compat.c src/adapters/linux_socket_state.c src/syscall/darwin_socket.c \
    tests/interface_addresses_compat_test.c \
    -Wl,--wrap=getifaddrs,--wrap=freeifaddrs,--wrap=calloc -o "$test_binary"
"$test_binary"
printf 'libSystem.B = %s/libsystem_shim.so\n' "$BUILD_DIR" > "$map_file"
MACHGATE_DYLIB_MAP="$map_file" "$BUILD_DIR/machgate" tests/fixtures/darwin_interface_addresses
