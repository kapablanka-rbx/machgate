#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"

BUILD_DIR="$BUILD_DIR" LD_LIBRARY_PATH="$BUILD_DIR" python3 - <<'PY'
import ctypes
import os
import pwd

lib = ctypes.CDLL(os.path.join(os.environ['BUILD_DIR'], 'libsystem_shim.so'))

class DarwinPasswd(ctypes.Structure):
    _fields_ = [
        ('pw_name', ctypes.c_void_p),
        ('pw_passwd', ctypes.c_void_p),
        ('pw_uid', ctypes.c_uint32),
        ('pw_gid', ctypes.c_uint32),
        ('pw_change', ctypes.c_int64),
        ('pw_class', ctypes.c_void_p),
        ('pw_gecos', ctypes.c_void_p),
        ('pw_dir', ctypes.c_void_p),
        ('pw_shell', ctypes.c_void_p),
        ('pw_expire', ctypes.c_int64),
    ]

assert ctypes.sizeof(DarwinPasswd) == 72
assert DarwinPasswd.pw_expire.offset == 64
uid = os.getuid()
expected = pwd.getpwuid(uid)

lib.getpwuid.argtypes = [ctypes.c_uint32]
lib.getpwuid.restype = ctypes.POINTER(DarwinPasswd)
entry = lib.getpwuid(uid)
assert entry
assert entry.contents.pw_uid == uid
assert entry.contents.pw_gid == expected.pw_gid
assert ctypes.string_at(entry.contents.pw_name).decode() == expected.pw_name
assert ctypes.string_at(entry.contents.pw_dir).decode() == expected.pw_dir
assert ctypes.string_at(entry.contents.pw_shell).decode() == expected.pw_shell
assert entry.contents.pw_change == 0
assert entry.contents.pw_expire == 0
assert entry.contents.pw_class is None

lib.getpwnam.argtypes = [ctypes.c_char_p]
lib.getpwnam.restype = ctypes.POINTER(DarwinPasswd)
entry = lib.getpwnam(expected.pw_name.encode())
assert entry and entry.contents.pw_uid == uid

lib.getpwuid_r.argtypes = [
    ctypes.c_uint32, ctypes.POINTER(DarwinPasswd), ctypes.c_char_p,
    ctypes.c_size_t, ctypes.POINTER(ctypes.POINTER(DarwinPasswd)),
]
lib.getpwuid_r.restype = ctypes.c_int
lib.getpwnam_r.argtypes = [
    ctypes.c_char_p, ctypes.POINTER(DarwinPasswd), ctypes.c_char_p,
    ctypes.c_size_t, ctypes.POINTER(ctypes.POINTER(DarwinPasswd)),
]
lib.getpwnam_r.restype = ctypes.c_int
getpwuid_r = lib.getpwuid_r
getpwnam_r = lib.getpwnam_r

for lookup, key in [(getpwuid_r, uid), (getpwnam_r, expected.pw_name.encode())]:
    output = DarwinPasswd()
    result = ctypes.POINTER(DarwinPasswd)()
    buffer = ctypes.create_string_buffer(4096)
    assert lookup(key, ctypes.byref(output), buffer, len(buffer),
                  ctypes.byref(result)) == 0
    assert result
    assert ctypes.addressof(result.contents) == ctypes.addressof(output)
    assert output.pw_uid == uid
    assert ctypes.string_at(output.pw_name).decode() == expected.pw_name
    assert ctypes.string_at(output.pw_dir).decode() == expected.pw_dir
    assert ctypes.string_at(output.pw_shell).decode() == expected.pw_shell
    for field in ['pw_name', 'pw_passwd', 'pw_gecos', 'pw_dir', 'pw_shell']:
        address = getattr(output, field)
        if address is not None:
            assert ctypes.addressof(buffer) <= address < ctypes.addressof(buffer) + len(buffer)

missing = ctypes.POINTER(DarwinPasswd)(ctypes.cast(1, ctypes.c_void_p))
assert getpwuid_r(uid, None, None, 0, ctypes.byref(missing)) == 22
PY
