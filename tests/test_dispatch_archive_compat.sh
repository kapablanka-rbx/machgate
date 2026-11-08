#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
python3 - "$BUILD_DIR/libsystem_shim.so" <<'PY'
import ctypes as c
import sys
import threading
import time

shim = c.CDLL(sys.argv[1])
pointer = c.c_void_p
def function(name, result, *arguments):
    value = getattr(shim, name)
    value.restype = result
    value.argtypes = arguments
    return value
callback_type = c.CFUNCTYPE(None, pointer)
calls = []
@callback_type
def callback(value):
    calls.append(value)
queue = function('dispatch_queue_create_with_target', pointer, c.c_char_p, pointer, pointer)(b'archive.parity', None, None)
assert function('dispatch_queue_get_label', c.c_char_p, pointer)(queue) == b'archive.parity'
function('dispatch_set_context', None, pointer, pointer)(queue, 37)
assert function('dispatch_get_context', pointer, pointer)(queue) == 37
function('dispatch_set_finalizer_f', None, pointer, callback_type)(queue, callback)
for name in ('dispatch_async_f', 'dispatch_sync_f'):
    function(name, None, pointer, pointer, callback_type)(queue, 11, callback)
assert calls == [11, 11]

class Block(c.Structure):
    _fields_ = [('isa', pointer), ('flags', c.c_uint32), ('reserved', c.c_uint32),
                ('invoke', callback_type), ('descriptor', pointer)]
block = Block(None, 1 << 28, 0, callback, None)
group = function('dispatch_group_create', pointer)()
enter = function('dispatch_group_enter', None, pointer)
leave = function('dispatch_group_leave', None, pointer)
notify = function('dispatch_group_notify', None, pointer, pointer, pointer)
enter(group)
enter(group)
notify(group, queue, c.byref(block))
notify(group, queue, c.byref(block))
leave(group)
assert len(calls) == 2
leave(group)
assert len(calls) == 4
release = function('dispatch_release', None, pointer)
release(group)

ready = threading.Event()
@callback_type
def delayed(value):
    assert value == 91
    ready.set()
deadline = function('dispatch_time', c.c_uint64, c.c_uint64, c.c_int64)(0, 30_000_000)
started = time.monotonic()
function('dispatch_after_f', None, c.c_uint64, pointer, pointer, callback_type)(deadline, queue, 91, delayed)
assert ready.wait(2)
assert time.monotonic() - started >= .02
ready.clear()
walltime = function('dispatch_walltime', c.c_uint64, pointer, c.c_int64)
deadline = walltime(None, 30_000_000)
started = time.monotonic()
function('dispatch_after_f', None, c.c_uint64, pointer, pointer, callback_type)(deadline, queue, 91, delayed)
assert ready.wait(2), 'wall-clock dispatch deadline was interpreted as monotonic time'
assert time.monotonic() - started >= .02
assert c.c_int64(deadline).value < -1
adjust = function('dispatch_time', c.c_uint64, c.c_uint64, c.c_int64)
assert adjust(deadline, 100) == deadline - 100
assert adjust((1 << 63) - 2, 100) == (1 << 64) - 1
assert adjust(5, -100) == 1
semaphore = function('dispatch_semaphore_create', pointer, c.c_long)(0)
started = time.monotonic()
assert function('dispatch_semaphore_wait', c.c_long, pointer, c.c_uint64)(semaphore, walltime(None, 10_000_000)) != 0
assert .005 <= time.monotonic() - started < 1
release(semaphore)
release(queue)
assert calls[-1] == 37

data_bytes = c.create_string_buffer(b'abc')
data = function('dispatch_data_create', pointer, pointer, c.c_size_t, pointer, pointer)(data_bytes, 3, None, None)
size = c.c_size_t()
result = function('machgate_dispatch_data_bytes', pointer, pointer, c.POINTER(c.c_size_t))(data, c.byref(size))
assert size.value == 3 and c.string_at(result, size.value) == b'abc'
release(data)
print('Dispatch archive exports: callbacks, group notifications, delayed execution, labels/context/finalizers and data passed')
PY
