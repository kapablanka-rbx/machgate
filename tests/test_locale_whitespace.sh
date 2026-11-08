#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"

BUILD_DIR="$BUILD_DIR" LD_LIBRARY_PATH="$BUILD_DIR" python3 - <<'PY'
import ctypes
import os

lib = ctypes.CDLL(os.path.join(os.environ['BUILD_DIR'], 'libsystem_shim.so'))
lib.__maskrune.argtypes = [ctypes.c_int, ctypes.c_ulong]
lib.__maskrune.restype = ctypes.c_int

class RuneLocale(ctypes.Structure):
    _fields_ = [('prefix', ctypes.c_ubyte * 0x3c),
                ('runetype', ctypes.c_uint32 * 256)]

locale = RuneLocale.in_dll(lib, '_DefaultRuneLocale')
control, space, blank = 0x200, 0x4000, 0x20000
mask = control | space | blank
for character in range(128):
    expected = control if character < 32 or character == 127 else 0
    if character in b' \t\n\v\f\r':
        expected |= space
    if character in b' \t':
        expected |= blank
    actual = lib.__maskrune(character, mask)
    assert actual == expected, (character, hex(actual), hex(expected))
    assert locale.runetype[character] & mask == expected
assert lib.__maskrune(-1, mask) == 0
assert lib.__maskrune(256, mask) == 0
print('Darwin ASCII control, space and blank classification passed')
PY
