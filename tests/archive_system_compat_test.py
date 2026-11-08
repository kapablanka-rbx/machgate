import ctypes
import os
from pathlib import Path
import stat
import sys
import tempfile


class SymbolInfo(ctypes.Structure):
    _fields_ = [("filename", ctypes.c_char_p), ("base", ctypes.c_void_p),
                ("name", ctypes.c_char_p), ("address", ctypes.c_void_p)]


class FileStatus(ctypes.Structure):
    _fields_ = [("device", ctypes.c_int32), ("mode", ctypes.c_uint16),
                ("links", ctypes.c_uint16), ("inode", ctypes.c_uint64),
                ("uid", ctypes.c_uint32), ("gid", ctypes.c_uint32),
                ("special_device", ctypes.c_int32), ("padding", ctypes.c_int32),
                ("times", ctypes.c_int64 * 8), ("size", ctypes.c_int64),
                ("blocks", ctypes.c_int64), ("block_size", ctypes.c_int32),
                ("flags", ctypes.c_uint32), ("generation", ctypes.c_uint32),
                ("reserved", ctypes.c_int32), ("extra", ctypes.c_int64 * 2)]


class FilesystemStatus(ctypes.Structure):
    _fields_ = [("block_size", ctypes.c_uint32), ("io_size", ctypes.c_int32),
                ("blocks", ctypes.c_uint64), ("free_blocks", ctypes.c_uint64),
                ("available_blocks", ctypes.c_uint64), ("files", ctypes.c_uint64),
                ("free_files", ctypes.c_uint64), ("fsid", ctypes.c_int32 * 2),
                ("owner", ctypes.c_uint32), ("type", ctypes.c_uint32),
                ("flags", ctypes.c_uint32), ("subtype", ctypes.c_uint32),
                ("type_name", ctypes.c_char * 16),
                ("mounted_on", ctypes.c_char * 1024),
                ("mounted_from", ctypes.c_char * 1024),
                ("extended_flags", ctypes.c_uint32),
                ("reserved", ctypes.c_uint32 * 7)]


shim_path = Path(sys.argv[1]).resolve()
shim = ctypes.CDLL(str(shim_path), use_errno=True)
compat_path = Path(sys.argv[2]).resolve()
compat = ctypes.CDLL(str(compat_path), use_errno=True)
host = ctypes.CDLL(None, use_errno=True)
host.dladdr.argtypes = [ctypes.c_void_p, ctypes.POINTER(SymbolInfo)]
host.dladdr.restype = ctypes.c_int
exports = [
    "OSAtomicAdd64", "OSAtomicCompareAndSwap64", "OSAtomicDecrement64",
    "OSAtomicIncrement64", "_Exit", "quick_exit", "stat_darwin",
    "fstat_darwin", "lstat_darwin", "fstatat_darwin", "statfs", "fstatfs",
    "pthread_cpu_number_np", "pthread_mutexattr_setpolicy_np",
    "mach_msg_server_once", "mach_port_allocate", "mach_port_construct",
    "mach_port_insert_right", "mach_port_set_attributes", "thread_get_state",
    "thread_info", "thread_resume", "thread_set_exception_ports", "thread_suspend",
]
missing = []
for name in exports:
    try:
        function = getattr(compat if name.startswith(("mach_", "thread_")) else shim, name)
    except AttributeError:
        missing.append(name)
        continue
    information = SymbolInfo()
    assert host.dladdr(ctypes.cast(function, ctypes.c_void_p),
                       ctypes.byref(information)) != 0
    expected_owner = compat_path if name.startswith(("mach_", "thread_")) else shim_path
    if Path(os.fsdecode(information.filename)).resolve() != expected_owner:
        missing.append(name + " (resolved outside shim)")
assert not missing, "missing owned exports: " + ", ".join(missing)

integer_pointer = ctypes.POINTER(ctypes.c_int64)
shim.OSAtomicAdd64.argtypes = [ctypes.c_int64, integer_pointer]
shim.OSAtomicAdd64.restype = ctypes.c_int64
for name in ["OSAtomicIncrement64", "OSAtomicDecrement64"]:
    getattr(shim, name).argtypes = [integer_pointer]
    getattr(shim, name).restype = ctypes.c_int64
shim.OSAtomicCompareAndSwap64.argtypes = [ctypes.c_int64, ctypes.c_int64,
                                        integer_pointer]
shim.OSAtomicCompareAndSwap64.restype = ctypes.c_bool
value = ctypes.c_int64(0x100000001)
assert shim.OSAtomicAdd64(0x100000002, ctypes.byref(value)) == 0x200000003
assert value.value == 0x200000003
assert shim.OSAtomicIncrement64(ctypes.byref(value)) == 0x200000004
assert shim.OSAtomicDecrement64(ctypes.byref(value)) == 0x200000003
assert not shim.OSAtomicCompareAndSwap64(7, 9, ctypes.byref(value))
assert value.value == 0x200000003
assert shim.OSAtomicCompareAndSwap64(0x200000003, -7, ctypes.byref(value))
assert value.value == -7
value.value = (1 << 63) - 1
assert shim.OSAtomicIncrement64(ctypes.byref(value)) == -(1 << 63)
assert shim.OSAtomicDecrement64(ctypes.byref(value)) == (1 << 63) - 1

assert ctypes.sizeof(FileStatus) == 144
assert ctypes.sizeof(FilesystemStatus) == 2168
shim.stat_darwin.argtypes = [ctypes.c_char_p, ctypes.POINTER(FileStatus)]
shim.lstat_darwin.argtypes = shim.stat_darwin.argtypes
shim.fstat_darwin.argtypes = [ctypes.c_int, ctypes.POINTER(FileStatus)]
shim.fstatat_darwin.argtypes = [ctypes.c_int, ctypes.c_char_p,
                              ctypes.POINTER(FileStatus), ctypes.c_int]
shim.statfs.argtypes = [ctypes.c_char_p, ctypes.POINTER(FilesystemStatus)]
shim.fstatfs.argtypes = [ctypes.c_int, ctypes.POINTER(FilesystemStatus)]

with tempfile.TemporaryDirectory(prefix="machgate-archive-system-") as directory:
    path = Path(directory) / "metadata"
    path.write_bytes(b"Darwin metadata\n")
    link = Path(directory) / "link"
    link.symlink_to(path.name)
    descriptor = os.open(path, os.O_RDONLY)
    directory_descriptor = os.open(directory, os.O_RDONLY | os.O_DIRECTORY)
    try:
        expected = path.stat()
        for operation in [
            lambda result: shim.stat_darwin(os.fsencode(path), result),
            lambda result: shim.fstat_darwin(descriptor, result),
            lambda result: shim.fstatat_darwin(directory_descriptor,
                                               b"metadata", result, 0),
            lambda result: shim.fstatat_darwin(-2, os.fsencode(path), result, 0),
        ]:
            result = FileStatus()
            assert operation(ctypes.byref(result)) == 0
            assert result.mode == expected.st_mode
            assert result.inode == expected.st_ino
            assert result.size == expected.st_size
            assert result.uid == expected.st_uid and result.gid == expected.st_gid
            assert not any(result.extra) and result.reserved == 0
        for operation in [
            lambda result: shim.lstat_darwin(os.fsencode(link), result),
            lambda result: shim.fstatat_darwin(directory_descriptor,
                                               b"link", result, 0x20),
        ]:
            result = FileStatus()
            assert operation(ctypes.byref(result)) == 0
            assert stat.S_ISLNK(result.mode)
        result = FileStatus()
        assert shim.fstat_darwin(-1, ctypes.byref(result)) == -1
        assert ctypes.get_errno() == 9
        assert shim.stat_darwin(os.fsencode(path), None) == -1
        assert ctypes.get_errno() == 14

        expected_fs = os.fstatvfs(descriptor)
        for operation in [
            lambda result: shim.statfs(os.fsencode(path), result),
            lambda result: shim.fstatfs(descriptor, result),
        ]:
            guarded = ctypes.create_string_buffer(2184)
            ctypes.memset(guarded, 0xA5, ctypes.sizeof(guarded))
            result = ctypes.cast(guarded, ctypes.POINTER(FilesystemStatus))
            assert operation(result) == 0
            assert guarded.raw[2168:] == bytes([0xA5]) * 16
            assert result.contents.block_size == expected_fs.f_frsize
            assert result.contents.blocks == expected_fs.f_blocks
            assert result.contents.free_blocks <= result.contents.blocks
            assert result.contents.files == expected_fs.f_files
            assert result.contents.type_name
            assert result.contents.mounted_on.startswith(b"/")
            assert result.contents.mounted_from
            for host_flag, guest_flag in [(os.ST_RDONLY, 1), (os.ST_NOSUID, 8),
                                           (os.ST_NODEV, 16), (os.ST_NOEXEC, 4)]:
                assert bool(result.contents.flags & guest_flag) == bool(
                    expected_fs.f_flag & host_flag)
            assert not any(result.contents.reserved)
        fs_result = FilesystemStatus()
        assert shim.fstatfs(-1, ctypes.byref(fs_result)) == -1
        assert ctypes.get_errno() == 9
        assert shim.statfs(os.fsencode(path), None) == -1
        assert ctypes.get_errno() == 14
        long_path = os.fsencode(directory) + b"/" + b"a" * 300
        assert shim.statfs(long_path, ctypes.byref(fs_result)) == -1
        assert ctypes.get_errno() == 63
    finally:
        os.close(directory_descriptor)
        os.close(descriptor)

cpu = ctypes.c_size_t(-1)
shim.pthread_cpu_number_np.argtypes = [ctypes.POINTER(ctypes.c_size_t)]
assert shim.pthread_cpu_number_np(ctypes.byref(cpu)) == 0
assert cpu.value in os.sched_getaffinity(0)
ctypes.set_errno(0)
assert shim.pthread_cpu_number_np(None) == 22
assert ctypes.get_errno() == 22
attributes = (ctypes.c_ubyte * 16)(*([0xA5] * 16))
shim.pthread_mutexattr_setpolicy_np.argtypes = [ctypes.c_void_p, ctypes.c_int]
assert shim.pthread_mutexattr_setpolicy_np(attributes, 1) == 45
assert shim.pthread_mutexattr_setpolicy_np(attributes, 3) == 45
assert bytes(attributes) == bytes([0xA5]) * 16
assert shim.pthread_mutexattr_setpolicy_np(None, 1) == 22
for policy in [-1, 0, 2, 4]:
    assert shim.pthread_mutexattr_setpolicy_np(attributes, policy) == 22

untouched = ctypes.c_uint32(0xABCDEF01)
count = ctypes.c_uint32(1)
for name in ["thread_info", "thread_get_state"]:
    function = getattr(compat, name)
    function.argtypes = [ctypes.c_uint32, ctypes.c_int, ctypes.c_void_p,
                         ctypes.POINTER(ctypes.c_uint32)]
    assert function(1, 3, ctypes.byref(untouched), ctypes.byref(count)) == 46
    assert untouched.value == 0xABCDEF01 and count.value == 1
for name in ["thread_suspend", "thread_resume"]:
    assert getattr(compat, name)(1) == 46
assert compat.thread_set_exception_ports(1, 0, 0, 0, 0) == 46
assert compat.mach_msg_server_once(None, 0, 1, 0) == 46
assert compat.mach_port_allocate(1, 1, ctypes.byref(untouched)) == 46
assert compat.mach_port_construct(1, None, 0, ctypes.byref(untouched)) == 46
assert untouched.value == 0xABCDEF01
assert compat.mach_port_insert_right(1, 2, 3, 4) == 46
assert compat.mach_port_set_attributes(1, 2, 0, None, 0) == 46

callback_type = ctypes.CFUNCTYPE(None)
host.__cxa_at_quick_exit.argtypes = [callback_type, ctypes.c_void_p]
for name in ["_Exit", "quick_exit"]:
    read_end, write_end = os.pipe()
    child = os.fork()
    if child == 0:
        os.close(read_end)
        callback = callback_type(lambda: os.write(write_end, b"quick-callback"))
        if host.__cxa_at_quick_exit(callback, None) != 0:
            os._exit(99)
        getattr(shim, name)(42)
        os._exit(98)
    os.close(write_end)
    output = os.read(read_end, 64)
    os.close(read_end)
    _, status = os.waitpid(child, 0)
    assert os.waitstatus_to_exitcode(status) == 42, (name, status)
    assert output == (b"quick-callback" if name == "quick_exit" else b""), name

print("archive system compatibility passed (24 owned exports)")
