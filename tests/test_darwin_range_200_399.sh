#!/bin/bash
# Test: representative Darwin syscalls 200-399 via svc #0x80.
set -e
cd "$(dirname "$0")/.."

MACHGATE_ROOT="${MACHGATE_ROOT:-$(pwd)}"
BUILD_DIR="${BUILD_DIR:-$MACHGATE_ROOT/build}"
FIXTURE_DIR="$MACHGATE_ROOT/tests/fixtures"
LLVM_MC="${LLVM_MC:-$(command -v llvm-mc || command -v llvm-mc-18 || true)}"
LD64_LLD="${LD64_LLD:-$(command -v ld64.lld || command -v ld64.lld-18 || true)}"
CC="${CC:-cc}"

[ -n "$LLVM_MC" ] || { echo "llvm-mc not found" >&2; exit 1; }
[ -n "$LD64_LLD" ] || { echo "ld64.lld not found" >&2; exit 1; }

"$CC" -D_GNU_SOURCE -Isrc -Wall -Werror -c src/syscall/syscall_range_200_399.c \
    -o /tmp/machgate_syscall_range_200_399.o

"$LLVM_MC" -triple arm64-apple-macos11 -filetype=obj \
    -o "$FIXTURE_DIR/darwin_range_200_399.o" \
    "$FIXTURE_DIR/darwin_range_200_399.s"
"$LD64_LLD" -arch arm64 -platform_version macos 11.0.0 11.0.0 \
    -o "$FIXTURE_DIR/darwin_range_200_399" -e _main \
    "$FIXTURE_DIR/darwin_range_200_399.o"
rm -f "$FIXTURE_DIR/darwin_range_200_399.o"

if ! grep -q "syscall_range_200_399_dispatch" src/syscall/syscall_gate.c 2>/dev/null; then
    echo "syscall_range_200_399_dispatch is not integrated into src/syscall/syscall_gate.c" >&2
    exit 1
fi

rm -rf "$FIXTURE_DIR/range_200_399.tmp" \
       "$FIXTURE_DIR/range_200_399.done" \
       "$FIXTURE_DIR/range_200_399.protected" \
       "$FIXTURE_DIR/range_200_399.dir" \
       "$FIXTURE_DIR/range_200_399.fifo" \
       "$FIXTURE_DIR/range_200_399.sendfile" \
       "$FIXTURE_DIR/range_200_399.copyfile" \
       "$FIXTURE_DIR/range_200_399.exchange_a" \
       "$FIXTURE_DIR/range_200_399.exchange_b" \
       /dev/shm/machgate_range_200_399
printf "xxxxx" > "$FIXTURE_DIR/range_200_399.done"

if ! command -v setfattr >/dev/null 2>&1 || \
   ! ( touch "$FIXTURE_DIR/range_200_399.xattrtest" 2>/dev/null && \
       setfattr -n user.test -v "ok" "$FIXTURE_DIR/range_200_399.xattrtest" 2>/dev/null ); then
    echo "SKIP: xattrs not supported on this filesystem"
    rm -f "$FIXTURE_DIR/range_200_399.xattrtest"
    exit 0
fi
rm -f "$FIXTURE_DIR/range_200_399.xattrtest"

output="$(cd "$FIXTURE_DIR" && "$BUILD_DIR/machgate" darwin_range_200_399 2>/dev/null)"
[ "$output" = "range-200-ok" ]
[ "$(wc -c < "$FIXTURE_DIR/range_200_399.tmp")" -eq 5 ]
[ "$(wc -c < "$FIXTURE_DIR/range_200_399.done")" -eq 5 ]
[ "$(wc -c < "$FIXTURE_DIR/range_200_399.sendfile")" -eq 5 ]
[ "$(cat "$FIXTURE_DIR/range_200_399.copyfile")" = "range" ]
[ "$(cat "$FIXTURE_DIR/range_200_399.exchange_a")" = "right" ]
[ "$(cat "$FIXTURE_DIR/range_200_399.exchange_b")" = "left!" ]
[ ! -e "$FIXTURE_DIR/range_200_399.protected" ]
[ ! -e "$FIXTURE_DIR/range_200_399.dir" ]
[ ! -e "$FIXTURE_DIR/range_200_399.fifo" ]
[ ! -e /dev/shm/machgate_range_200_399 ]
