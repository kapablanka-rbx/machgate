#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/.."

TEST_BINARY="$(mktemp)"
trap 'rm -f "$TEST_BINARY"' EXIT

"${CC:-cc}" -std=gnu11 -Wall -Wextra -Werror -D_GNU_SOURCE -Isrc \
    src/domain/guest_syscall.c \
    src/adapters/linux_socket_state.c src/syscall/darwin_socket.c \
    src/adapters/linux_user_lock.c src/syscall/darwin_ulock.c \
    src/syscall/syscall_range_000_099.c \
    src/syscall/syscall_range_100_199.c \
    src/syscall/syscall_range_400_plus.c \
    src/syscall/syscall_gate.c \
    src/adapters/startup_cache.c \
    tests/darwin_raw_socket_dispatch_test.c \
    -Wl,--wrap=syscall \
    -Wl,--defsym,machgate_verbose=0 \
    -Wl,--defsym,machgate_kqueue_fd_did_close=0 \
    -Wl,--defsym,machgate_kqueue_fd_did_duplicate=0 \
    -Wl,--defsym,machgate_pool_alloc=0 \
    -Wl,--defsym,machgate_guest_image_validated_header=0 \
    -Wl,--defsym,machgate_guest_image_slide=0 \
    -Wl,--defsym,machgate_guest_image_startup_cache=0 \
    -Wl,--defsym,machgate_darwin_flock_to_linux=0 \
    -Wl,--defsym,machgate_darwin_preallocate=0 \
    -Wl,--defsym,machgate_linux_flock_to_darwin=0 \
    -Wl,--defsym,machgate_dispatch_darwin_mmap=0 \
    -Wl,--defsym,machgate_dispatch_darwin_munmap=0 \
    -Wl,--defsym,machgate_execve_macho_guest=0 \
    -Wl,--defsym,machgate_syscall_write_no_sigpipe=0 \
    -Wl,--defsym,machgate_syscall_writev_no_sigpipe=0 \
    -Wl,--defsym,syscall_range_200_399_dispatch=0 \
    -o "$TEST_BINARY"
"$TEST_BINARY"
