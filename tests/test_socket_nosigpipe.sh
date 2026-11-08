#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}" python3 - <<'PY'
import ctypes as c
import os
import signal
import socket
lib = c.CDLL(os.path.join(os.environ['BUILD_DIR'], 'libsystem_shim.so'), use_errno=True)
lib.setsockopt.argtypes = [c.c_int, c.c_int, c.c_int, c.c_void_p, c.c_uint]
lib.getsockopt.argtypes = [c.c_int, c.c_int, c.c_int, c.c_void_p, c.POINTER(c.c_uint)]
def get(fd):
    value, size = c.c_int(-1), c.c_uint(4)
    assert lib.getsockopt(fd, 0xffff, 0x1022, c.byref(value), c.byref(size)) == 0
    assert size.value == 4
    return value.value
def set(fd, value):
    return lib.setsockopt(fd, 0xffff, 0x1022, c.byref(c.c_int(value)), 4)
pair = (c.c_int * 2)()
assert lib.socketpair(1, 1, 0, pair) == 0
assert get(pair[0]) == 0
assert set(pair[0], 1) == 0 and get(pair[0]) != 0
duplicate = lib.dup(pair[0])
assert get(duplicate) != 0
assert set(duplicate, 0) == 0 and get(pair[0]) == 0
assert set(duplicate, 1) == 0
lib.close(pair[0])
assert get(duplicate) != 0
signals = []
previous = signal.signal(signal.SIGPIPE, lambda *args: signals.append(1))
lib.close(pair[1])
assert lib.send(duplicate, b'x', 1, 0) == -1
assert c.get_errno() == 32 and not signals
assert lib.write(duplicate, b'x', 1) == -1 and not signals
assert set(duplicate, 0) == 0
assert lib.send(duplicate, b'x', 1, 0) == -1 and signals
signals.clear()
assert lib.write(duplicate, b'x', 1) == -1 and signals
signal.signal(signal.SIGPIPE, previous)
lib.close(duplicate)
assert set(-1, 1) == -1 and c.get_errno() == 9
with socket.socket() as listener, socket.socket() as client:
    listener.bind(('127.0.0.1', 0))
    listener.listen(1)
    assert set(listener.fileno(), 1) == 0
    client.connect(listener.getsockname())
    accepted = lib.accept(listener.fileno(), None, None)
    assert accepted >= 0
    assert get(accepted) != 0, 'accepted socket lost SO_NOSIGPIPE'
    lib.close(accepted)
    lib.close(listener.detach())
print('SO_NOSIGPIPE: default, readback, aliases, clearing, SIGPIPE suppression and EBADF passed')
PY
