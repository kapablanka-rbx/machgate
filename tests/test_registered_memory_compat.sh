#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
python3 - "$BUILD_DIR/libsystem_shim.so" <<'PY'
import ctypes as c
import os
import sys
shim = c.CDLL(sys.argv[1])
host = c.CDLL(None, use_errno=True)
shim.mmap_registry_add.argtypes = [c.c_void_p, c.c_size_t]
shim.free.argtypes = [c.c_void_p]
host.mmap.argtypes = [c.c_void_p, c.c_size_t, c.c_int, c.c_int, c.c_int, c.c_long]
host.mmap.restype = c.c_void_p
host.mincore.argtypes = [c.c_void_p, c.c_size_t, c.c_void_p]
size = os.sysconf('SC_PAGE_SIZE')
region = host.mmap(None, size, 3, 0x22, -1, 0)
assert region != c.c_void_p(-1).value
c.memset(region, 17, size)
shim.mmap_registry_add(region, size)
shim.free(region)
assert host.mincore(region, size, c.byref(c.c_ubyte())) == -1 and c.get_errno() == 12
print('Registered mmap allocation releases through shim free')
PY
