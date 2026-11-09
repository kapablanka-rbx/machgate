#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
test_binary="$(mktemp)"
trap 'rm -f "$test_binary"' EXIT
"${CC:-cc}" -std=gnu11 -Wall -Wextra -Werror -Isrc \
    src/shim/sysctl_compat.c src/shim/host_facts.c \
    src/shim/guest_hardware_compat.c tests/network_metrics_compat_test.c \
    -o "$test_binary"
"$test_binary"
echo 'Unavailable network metrics preserve output and supported sysctls'
