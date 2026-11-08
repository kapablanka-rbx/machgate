#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/.."
MACHGATE_ROOT="${MACHGATE_ROOT:-$(pwd)}"
BUILD_DIR="${BUILD_DIR:-$MACHGATE_ROOT/build}"
FIXTURE_DIR="$MACHGATE_ROOT/tests/fixtures"
FIXTURE="$FIXTURE_DIR/darwin_kqueue_psynch"
LLVM_MC="${LLVM_MC:-$(command -v llvm-mc || command -v llvm-mc-18 || true)}"
LD64_LLD="${LD64_LLD:-$(command -v ld64.lld || command -v ld64.lld-18 || true)}"

[ -n "$LLVM_MC" ] || { echo "llvm-mc not found" >&2; exit 1; }
[ -n "$LD64_LLD" ] || { echo "ld64.lld not found" >&2; exit 1; }

"$LLVM_MC" -triple arm64-apple-macos11 -filetype=obj \
    -o "$FIXTURE_DIR/darwin_kqueue_psynch.o" \
    "$FIXTURE_DIR/darwin_kqueue_psynch.s"
"$LD64_LLD" -arch arm64 -platform_version macos 11.0.0 11.0.0 \
    -o "$FIXTURE" -e _main "$FIXTURE_DIR/darwin_kqueue_psynch.o"
rm -f "$FIXTURE_DIR/darwin_kqueue_psynch.o"

cleanup() {
    rm -f "$FIXTURE_DIR/darwin_kqueue_psynch.sock"
}
trap cleanup EXIT
cleanup
output="$(cd "$FIXTURE_DIR" && "$BUILD_DIR/machgate" darwin_kqueue_psynch)"
[ "$output" = "kqueue-psynch-ok" ]

BUILD_DIR="$BUILD_DIR" python3 - <<'PY'
import ctypes
import os

lib = ctypes.CDLL(os.path.join(os.environ['BUILD_DIR'], 'libsystem_shim.so'),
                  use_errno=True)

class DarwinKevent(ctypes.Structure):
    _fields_ = [
        ('ident', ctypes.c_uint64),
        ('filter', ctypes.c_int16),
        ('flags', ctypes.c_uint16),
        ('fflags', ctypes.c_uint32),
        ('data', ctypes.c_int64),
        ('udata', ctypes.c_void_p),
    ]

class Timespec(ctypes.Structure):
    _fields_ = [('tv_sec', ctypes.c_long), ('tv_nsec', ctypes.c_long)]

assert ctypes.sizeof(DarwinKevent) == 32
assert DarwinKevent.filter.offset == 8
assert DarwinKevent.flags.offset == 10
assert DarwinKevent.fflags.offset == 12
assert DarwinKevent.data.offset == 16
assert DarwinKevent.udata.offset == 24

lib.kqueue.restype = ctypes.c_int
lib.kevent.argtypes = [
    ctypes.c_int, ctypes.POINTER(DarwinKevent), ctypes.c_int,
    ctypes.POINTER(DarwinKevent), ctypes.c_int, ctypes.POINTER(Timespec),
]
lib.kevent.restype = ctypes.c_int
lib.close.argtypes = [ctypes.c_int]
lib.close.restype = ctypes.c_int
lib.dup.argtypes = [ctypes.c_int]
lib.dup.restype = ctypes.c_int
lib.dup2.argtypes = [ctypes.c_int, ctypes.c_int]
lib.dup2.restype = ctypes.c_int
class DarwinKevent64(ctypes.Structure):
    _fields_ = DarwinKevent._fields_ + [('ext', ctypes.c_uint64 * 2)]

assert ctypes.sizeof(DarwinKevent64) == 48
assert DarwinKevent64.ext.offset == 32
lib.kevent64.argtypes = [
    ctypes.c_int, ctypes.POINTER(DarwinKevent64), ctypes.c_int,
    ctypes.POINTER(DarwinKevent64), ctypes.c_int, ctypes.c_uint,
    ctypes.POINTER(Timespec),
]
lib.kevent64.restype = ctypes.c_int

queue = lib.kqueue()
assert queue >= 0
try:
    descriptors = (ctypes.c_int * 2)()
    assert lib.pipe(descriptors) == 0
    try:
        assert os.write(descriptors[1], b'abc') == 3
        change = DarwinKevent(descriptors[0], -1, 1, 0, 0, 0x1234)
        event = DarwinKevent()
        assert lib.kevent(queue, ctypes.byref(change), 1,
                          ctypes.byref(event), 1,
                          ctypes.byref(Timespec(0, 0))) == 1
        assert event.ident == descriptors[0]
        assert event.filter == -1
        assert event.data == 3
        assert event.udata == 0x1234
        assert lib.kevent(queue, None, 0, ctypes.byref(event), 1,
                          ctypes.byref(Timespec(0, 0))) == 1

        alias = lib.dup(queue)
        assert alias >= 0
        assert lib.close(alias) == 0
        receipt = DarwinKevent(descriptors[0], -1, 1 | 0x40, 0, 0, 0x5678)
        events = (DarwinKevent * 2)()
        assert lib.kevent(queue, ctypes.byref(receipt), 1, events, 2,
                          ctypes.byref(Timespec(0, 0))) == 2
        assert events[0].flags & 0x4000
        assert events[0].data == 0
        assert events[1].ident == descriptors[0]
        assert events[1].data == 3
    finally:
        assert lib.close(descriptors[0]) == 0
        assert lib.close(descriptors[1]) == 0
finally:
    assert lib.close(queue) == 0

queue = lib.kqueue()
assert queue >= 0
high_alias = lib.dup(queue)
assert high_alias >= 0
assert lib.close(queue) == 0
assert lib.kevent64(high_alias, None, 0, None, 0, 0,
                    ctypes.byref(Timespec(0, 0))) == 0
ctypes.set_errno(0)
assert lib.kevent64(high_alias, None, 0, None, 0, 1,
                    ctypes.byref(Timespec(0, 0))) == -1
assert ctypes.get_errno() == 38
assert lib.close(high_alias) == 0
ctypes.set_errno(0)
assert lib.kevent64(high_alias, None, 0, None, 0, 0,
                    ctypes.byref(Timespec(0, 0))) == -1
assert ctypes.get_errno() == 9

queue = lib.kqueue()
assert queue >= 0
descriptors = (ctypes.c_int * 2)()
assert lib.pipe(descriptors) == 0
try:
    assert os.write(descriptors[1], b'abcd') == 4
    changes = (DarwinKevent64 * 2)()
    changes[0] = DarwinKevent64(descriptors[0], -1, 1, 0, 0, 0x1234,
                                (0xaaaa, 0xbbbb))
    events = (DarwinKevent64 * 2)()
    assert lib.kevent64(queue, changes, 1, events, 2, 0,
                        ctypes.byref(Timespec(0, 0))) == 1
    assert events[0].ident == descriptors[0]
    assert events[0].data == 4
    assert list(events[0].ext) == [0xaaaa, 0xbbbb]
    assert events[1].ident == 0

    alias = lib.dup2(queue, 1100)
    assert alias == 1100
    assert lib.close(queue) == 0
    assert lib.kevent64(alias, None, 0, events, 1, 0,
                        ctypes.byref(Timespec(0, 0))) == 1
    assert lib.close(alias) == 0
    ctypes.set_errno(0)
    assert lib.kevent64(alias, None, 0, events, 1, 0,
                        ctypes.byref(Timespec(0, 0))) == -1
    assert ctypes.get_errno() == 9
finally:
    assert lib.close(descriptors[0]) == 0
    assert lib.close(descriptors[1]) == 0
PY

echo "kqueue/psynch regression tests passed"
