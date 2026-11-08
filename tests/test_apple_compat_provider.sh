#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
python3 - "${MACHGATE_TEST_APPLE_PROVIDER:-$BUILD_DIR/libmachgate_apple_compat.so}" <<'PY'
import ctypes as c
from pathlib import Path
import sys
import tempfile

provider = c.CDLL(sys.argv[1])
def function(name, result, *arguments):
    value = getattr(provider, name)
    value.restype = result
    value.argtypes = arguments
    return value

pointer = c.c_void_p
array = function('CFArrayCreate', pointer, pointer, c.POINTER(pointer), c.c_long, pointer)
callback_type = c.CFUNCTYPE(None, pointer, pointer, c.c_size_t, c.POINTER(c.c_char_p),
                           c.POINTER(c.c_uint32), c.POINTER(c.c_uint64))
events = []
@callback_type
def callback(stream, info, count, paths, flags, ids):
    events.extend((paths[i].decode(), flags[i], ids[i]) for i in range(count))
create = function('FSEventStreamCreate', pointer, pointer, callback_type, pointer,
                  pointer, c.c_uint64, c.c_double, c.c_uint32)
start = function('FSEventStreamStart', c.c_int, pointer)
flush = function('FSEventStreamFlushSync', None, pointer)
stop = function('FSEventStreamStop', None, pointer)
invalidate = function('FSEventStreamInvalidate', None, pointer)
release = function('FSEventStreamRelease', None, pointer)
with tempfile.TemporaryDirectory() as directory:
    text = c.create_string_buffer(directory.encode())
    paths = array(None, (pointer * 1)(c.cast(text, pointer)), 1, None)
    stream = create(None, callback, None, paths, 0xffffffffffffffff, 0, 0x10)
    assert stream and start(stream)
    target = Path(directory) / 'event.txt'
    target.write_text('first')
    flush(stream)
    assert any(path == str(target) and flags & 0x100 for path, flags, _ in events), events
    events.clear()
    target.write_text('second and longer')
    flush(stream)
    assert any(path == str(target) and flags & 0x1000 for path, flags, _ in events), events
    events.clear()
    target.unlink()
    flush(stream)
    assert any(path == str(target) and flags & 0x200 for path, flags, _ in events), events
    stop(stream)
    invalidate(stream)
    assert not start(stream)
    release(stream)

client = function('IOHIDEventSystemClientCreate', pointer, pointer)(None)
services = function('IOHIDEventSystemClientCopyServices', pointer, pointer)(client)
assert function('CFArrayGetCount', c.c_long, pointer)(services) == 0
assert function('IOHIDServiceClientCopyEvent', pointer, pointer, c.c_int64, c.c_int32, c.c_int64)(None, 0, 0, 0) is None
assert function('CFRunLoopGetCurrent', pointer)()
print('Apple compatibility provider: filesystem event creation, modification, removal and unavailable services passed')
PY
