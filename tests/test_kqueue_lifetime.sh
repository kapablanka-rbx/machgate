#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}" python3 - <<'PY'
import ctypes as c
import os
lib = c.CDLL(os.path.join(os.environ['BUILD_DIR'], 'libsystem_shim.so'))
class Event(c.Structure):
    _fields_ = [('ident', c.c_uint64), ('filter', c.c_int16), ('flags', c.c_uint16),
                ('fflags', c.c_uint32), ('data', c.c_int64), ('udata', c.c_void_p)]
class Time(c.Structure):
    _fields_ = [('seconds', c.c_long), ('nanoseconds', c.c_long)]
lib.kevent.argtypes = [c.c_int, c.POINTER(Event), c.c_int, c.POINTER(Event), c.c_int, c.POINTER(Time)]
def add(queue, userdata):
    event = Event(13, -10, 1, 0x01000000, 0, userdata)
    assert lib.kevent(queue, c.byref(event), 1, None, 0, None) == 0
def get(queue):
    event = Event()
    count = lib.kevent(queue, None, 0, c.byref(event), 1, c.byref(Time()))
    return count, event.udata
queue = lib.kqueue()
add(queue, 0x1234)
assert lib.close(queue) == 0
replacement = lib.kqueue()
assert replacement == queue
assert get(replacement)[0] == 0, 'closed queue retained stale userdata'
alias = lib.dup(replacement)
assert alias >= 0
add(alias, 0x2345)
assert lib.close(replacement) == 0
replacement = lib.kqueue()
assert get(replacement)[0] == 0, 'reused descriptor aliases a different live queue'
assert get(alias) == (1, 0x2345)
lib.close(alias)
lib.close(replacement)
for index in range(1500):
    queue = lib.kqueue()
    assert queue >= 0, ('queue registry exhausted', index)
    assert lib.close(queue) == 0
print('kqueue lifetime: stale userdata, descriptor reuse with live duplicate, 1500 lifecycles passed')
PY
