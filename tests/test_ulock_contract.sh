#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}" python3 - <<'PY'
import ctypes as c
import mmap
import multiprocessing as mp
import os
import threading
import time

lib = c.CDLL(os.path.join(os.environ['BUILD_DIR'], 'libsystem_shim.so'), use_errno=True)
wait = lib.__ulock_wait2
wait.argtypes = [c.c_uint32, c.c_void_p, c.c_uint64, c.c_uint64, c.c_uint64]
wake = lib.__ulock_wake
wake.argtypes = [c.c_uint32, c.c_void_p, c.c_uint64]
NO_ERRNO = 0x01000000

def mismatch():
    value = c.c_uint32(3)
    assert wait(1, c.byref(value), 4, 1000000, 0) >= 0, 'changed value must succeed'
    assert wait(1, c.byref(value), (1 << 32) | 3, 1000000, 0) >= 0

def wide_mismatch():
    value = c.c_uint64(1 << 32)
    assert wait(5, c.byref(value), 0, 1000000, 0) >= 0, 'must compare upper 32 bits'

def deadline():
    value = c.c_uint32(0)
    started = time.monotonic()
    result = wait(1 | NO_ERRNO | 0x02000000, c.byref(value), 0,
                  time.monotonic_ns() + 10000000, 0)
    assert result == -60 and time.monotonic() - started < 0.2

def errors():
    value = c.c_uint32(0)
    c.set_errno(123)
    assert wait(1 | NO_ERRNO, c.byref(value), 0, 1000000, 0) == -60
    assert c.get_errno() == 123, 'ULF_NO_ERRNO changed errno'
    assert wait(1, c.byref(value), 0, 1000000, 0) == -1
    assert c.get_errno() == 60, 'timeout errno must be Darwin ETIMEDOUT'
    assert wait(1 | NO_ERRNO, None, 0, 1, 0) == -22
    assert wait(99 | NO_ERRNO, c.byref(value), 0, 1, 0) == -22
    assert wait(1 | NO_ERRNO | 0x100, c.byref(value), 0, 1, 0) == -22
    assert wake(1 | NO_ERRNO, c.byref(value), 0) == -2, 'empty wake must report ENOENT'

def shared_wait():
    storage = mmap.mmap(-1, 4096, flags=mmap.MAP_SHARED, prot=mmap.PROT_READ | mmap.PROT_WRITE)
    value = c.c_uint32.from_buffer(storage)
    receiving, sending = mp.Pipe(duplex=False)
    def child():
        sending.send(wait(3 | NO_ERRNO, c.byref(value), 0, 200000000, 0))
    process = mp.Process(target=child)
    process.start()
    time.sleep(0.025)
    assert wake(3 | NO_ERRNO, c.byref(value), 0) >= 0
    process.join(0.4)
    assert not process.is_alive()
    assert receiving.recv() >= 0, 'shared wake did not reach another process'

def wide_wake():
    value = c.c_uint64(1 << 32)
    results = []
    thread = threading.Thread(target=lambda: results.append(wait(5 | NO_ERRNO, c.byref(value), value.value, 200000000, 0)))
    thread.start()
    time.sleep(0.025)
    value.value = 2 << 32
    assert wake(5 | NO_ERRNO, c.byref(value), 0) >= 0
    thread.join(0.4)
    assert results and results[0] >= 0

def remaining_waiters():
    value = c.c_uint32(0)
    results = []
    threads = [threading.Thread(target=lambda: results.append(
        wait(1 | NO_ERRNO, c.byref(value), 0, 300000000, 0))) for _ in range(2)]
    for thread in threads:
        thread.start()
    time.sleep(0.025)
    assert wake(1 | NO_ERRNO, c.byref(value), 0) >= 0
    deadline = time.monotonic() + 0.1
    while not results and time.monotonic() < deadline:
        time.sleep(0.001)
    first_result = results[0] if results else None
    wake(1 | NO_ERRNO | 0x100, c.byref(value), 0)
    for thread in threads:
        thread.join(0.4)
    assert first_result == 1, f'woken waiter lost the next wake: {results}'
    assert sorted(results) == [0, 1], results

def fork_discards_parent_waiters():
    value = c.c_uint32(0)
    results = []
    thread = threading.Thread(target=lambda: results.append(
        wait(1 | NO_ERRNO, c.byref(value), 0, 300000000, 0)))
    thread.start()
    time.sleep(0.025)
    assert wait(1 | NO_ERRNO, c.byref(value), 1, 1000000, 0) == 1
    child = os.fork()
    if child == 0:
        result = wait(1 | NO_ERRNO, c.byref(value), 1, 1000000, 0)
        os._exit(0 if result == 0 else 1)
    _, status = os.waitpid(child, 0)
    wake(1 | NO_ERRNO, c.byref(value), 0)
    thread.join(0.4)
    assert status == 0, 'child inherited parent waiter count'
    assert results == [0], results

failures = []
for test in (mismatch, wide_mismatch, deadline, errors, shared_wait, wide_wake, remaining_waiters, fork_discards_parent_waiters):
    process = mp.Process(target=test)
    process.start()
    process.join(0.8)
    if process.is_alive():
        process.kill()
        process.join()
    if process.exitcode:
        failures.append(test.__name__)
    print(test.__name__, 'PASS' if process.exitcode == 0 else 'FAIL', flush=True)
assert not failures, failures
PY
