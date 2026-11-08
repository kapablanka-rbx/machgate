#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
python3 - "$BUILD_DIR" <<'PY'
import ctypes as c
import os
import sys
from pathlib import Path
build = Path(sys.argv[1])
shim = c.CDLL(str(build / 'libsystem_shim.so'))
override = c.c_int.in_dll(shim, 'machgate_shim_guest_ncpu')
shim.sysconf.argtypes = [c.c_int]
shim.sysconf.restype = c.c_long
shim.sysctlbyname.argtypes = [c.c_char_p, c.c_void_p, c.c_void_p, c.c_void_p, c.c_size_t]
override.value = 3
for name in (b'hw.ncpu', b'hw.logicalcpu', b'hw.physicalcpu'):
    value, size = c.c_int(), c.c_size_t(4)
    assert shim.sysctlbyname(name, c.byref(value), c.byref(size), None, 0) == 0
    assert value.value == 3
assert shim.sysconf(57) == 3 and shim.sysconf(58) == 3
override.value = 0
assert shim.sysconf(58) == os.sysconf('SC_NPROCESSORS_ONLN')
foundation = c.CDLL(str(build / 'libmachgate_foundation.so'))
send = foundation.shim_objc_msgSend_impl
send.argtypes = [c.c_void_p, c.c_char_p] + [c.c_size_t] * 6 + [c.c_void_p, c.c_void_p]
send.restype = c.c_void_p
foundation.objc_getClass.argtypes = [c.c_char_p]
foundation.objc_getClass.restype = c.c_void_p
info = send(foundation.objc_getClass(b'NSProcessInfo'), b'processInfo', 0, 0, 0, 0, 0, 0, None, None)
version = (c.c_int64 * 3)(-1, -1, -1)
send(info, b'operatingSystemVersion', 0, 0, 0, 0, 0, 0, version, None)
assert list(version) == [15, 0, 0]
text = c.create_string_buffer(b'42.5')
string = send(foundation.objc_getClass(b'NSString'), b'stringWithUTF8String:', c.addressof(text), 0, 0, 0, 0, 0, None, None)
class Result(c.Structure):
    _fields_ = [('pointer', c.c_void_p), ('floating', c.c_double)]
structured = foundation.shim_objc_msgSend_struct_impl
structured.argtypes, structured.restype = send.argtypes, Result
assert structured(string, b'doubleValue', 0, 0, 0, 0, 0, 0, None, None).floating == 42.5
callback_type = c.CFUNCTYPE(c.c_size_t, *([c.c_size_t] * 10))
callback = callback_type(lambda *args: sum(args))
registers, stack = (c.c_size_t * 6)(3, 4, 5, 6, 7, 8), (c.c_size_t * 16)(9, 10)
forward = foundation.shim_objc_msgSend_call_guest_imp
forward.argtypes = [c.c_void_p] * 6
forward.restype = c.c_size_t
assert forward(callback, 1, 2, None, registers, stack) == 55
print('Archive helper contracts: guest CPU override, version structure, floating result and stack forwarding passed')
PY
cc -Wall -Wextra -Werror -Isrc tests/initializer_context_test.c \
    src/shim/initializer_context.c -o "$BUILD_DIR/initializer_context_test"
"$BUILD_DIR/initializer_context_test" 2> "$BUILD_DIR/initializer-context.log"
test "$(wc -l < "$BUILD_DIR/initializer-context.log")" -eq 1
rg -q 'kind=pointer progress=2/3 index=1 address=0x1234' "$BUILD_DIR/initializer-context.log"
