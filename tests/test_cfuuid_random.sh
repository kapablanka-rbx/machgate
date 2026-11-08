#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"

BUILD_DIR="$BUILD_DIR" LD_LIBRARY_PATH="$BUILD_DIR" python3 - <<'PY'
import ctypes
import os
import re
import uuid

lib = ctypes.CDLL(os.path.join(os.environ['BUILD_DIR'], 'libsystem_shim.so'))
lib.CFUUIDCreate.argtypes = [ctypes.c_void_p]
lib.CFUUIDCreate.restype = ctypes.c_void_p
lib.CFUUIDCreateString.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
lib.CFUUIDCreateString.restype = ctypes.c_void_p
lib.CFRelease.argtypes = [ctypes.c_void_p]

values = []
for _ in range(32):
    uuid_object = lib.CFUUIDCreate(None)
    assert uuid_object
    raw = ctypes.string_at(uuid_object)
    assert re.fullmatch(
        rb'[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}',
        raw,
    ), raw
    assert uuid.UUID(raw.decode()).version == 4
    string = lib.CFUUIDCreateString(None, uuid_object)
    assert string
    assert ctypes.string_at(string) == raw
    lib.CFRelease(string)
    lib.CFRelease(uuid_object)
    values.append(raw)

assert len(set(values)) == len(values), 'CFUUIDCreate repeated a UUID sample'
PY
