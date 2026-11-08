#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}" python3 - <<'PY'
import ctypes as c
import os
import time

lib = c.CDLL(os.path.join(os.environ['BUILD_DIR'], 'libsystem_shim.so'), use_errno=True)
class Event(c.Structure):
    _fields_ = [('ident', c.c_uint64), ('filter', c.c_int16), ('flags', c.c_uint16),
                ('fflags', c.c_uint32), ('data', c.c_int64), ('udata', c.c_void_p)]
class Time(c.Structure):
    _fields_ = [('seconds', c.c_long), ('nanoseconds', c.c_long)]
lib.kevent.argtypes = [c.c_int, c.POINTER(Event), c.c_int, c.POINTER(Event), c.c_int, c.POINTER(Time)]
queue = lib.kqueue()
assert queue >= 0
def change(ident, flags, units=0, value=0, userdata=0):
    event = Event(ident, -7, flags, units, value, userdata)
    return lib.kevent(queue, c.byref(event), 1, None, 0, None)
def receive(wait=0.1):
    event = Event()
    timeout = Time(int(wait), int((wait % 1) * 1e9))
    result = lib.kevent(queue, None, 0, c.byref(event), 1, c.byref(timeout))
    return result, event
try:
    for ident, units, value in [(1, 0, 2), (2, 2, 2000), (3, 4, 2000000)]:
        assert change(ident, 1, units, value, 0x1234) == 0
        result, event = receive()
        assert result == 1, ('timer did not fire', ident, result)
        assert (event.ident, event.filter, event.udata) == (ident, -7, 0x1234)
        assert event.data >= 1 and event.flags & 0x20
        time.sleep(0.01)
        result, event = receive(0)
        assert result == 1 and event.data >= 2, ('lost expirations', event.data)
        assert change(ident, 2) == 0
        assert receive(0)[0] == 0
    assert change(4, 1 | 0x10, 2, 1000) == 0
    assert receive()[0] == 1
    assert receive(0.005)[0] == 0
    assert change(5, 1, 4 | 8, time.time_ns() - 1000000) == 0
    assert receive()[0] == 1
    assert receive(0)[0] == 0
    assert change(6, 1 | 8, 2, 1000, 0x5678) == 0
    assert receive(0.005)[0] == 0
    assert change(6, 4) == 0
    result, event = receive()
    assert result == 1 and event.udata == 0x5678
    assert change(6, 2) == 0
    assert change(7, 1, 1 | 2, 1) == -1
    for ident, value in [(8, -1), (9, 0)]:
        assert change(ident, 1, 1, value) == 0
        assert receive(0.02)[0] == 1, ('nonpositive timer must fire immediately', value)
        assert receive(0.02)[0] == 0, ('nonpositive timer must not repeat', value)
        assert change(ident, 2) == 0
    print('kqueue timers: milliseconds, microseconds, nanoseconds, accumulation, deletion, one-shot, absolute, enable/disable passed')
finally:
    lib.close(queue)
PY
timer_test=$(mktemp)
trap 'rm -f "$timer_test"' EXIT
cc -Wall -Wextra -Isrc tests/linux_event_timer_test.c src/adapters/linux_event_timer.c \
    -Wl,--wrap=syscall -o "$timer_test"
"$timer_test"
