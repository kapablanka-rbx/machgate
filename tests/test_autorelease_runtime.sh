#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
export BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
python3 - <<'PY'
import ctypes
import os
from pathlib import Path

runtime = ctypes.CDLL(str(Path(os.environ['BUILD_DIR']).resolve() / 'libsystem_shim.so'))
push = runtime.objc_autoreleasePoolPush
push.restype = ctypes.c_void_p
pop = runtime.objc_autoreleasePoolPop
pop.argtypes = [ctypes.c_void_p]
release_type = ctypes.CFUNCTYPE(None, ctypes.c_void_p)
defer = runtime.machgate_defer_release_v1
defer.argtypes = [ctypes.c_void_p, release_type]
defer.restype = ctypes.c_int
released = []
release = release_type(lambda pointer: released.append(pointer))
assert defer(1, release) == 0
outer = push()
assert outer
assert defer(11, release) == 1
inner = push()
assert inner and inner != outer
assert defer(22, release) == 1
assert defer(33, release) == 1
pop(inner)
assert released == [33, 22], released
assert defer(44, release) == 1
pop(outer)
assert released == [33, 22, 44, 11], released
assert defer(55, release) == 0
print('autorelease runtime: shared callback ownership and nested LIFO pools passed')
PY
