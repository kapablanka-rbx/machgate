#!/bin/bash
# Test: libsystem_shim.so can be loaded and key Apple-specific symbols resolve
set -e
cd "$(dirname "$0")/.."
MACHGATE_ROOT="${MACHGATE_ROOT:-$(pwd)}"
BUILD_DIR="${BUILD_DIR:-$MACHGATE_ROOT/build}"

[ -f "$BUILD_DIR/libsystem_shim.so" ] || { echo "libsystem_shim.so not built"; exit 1; }

# Test that the .so loads without errors
BUILD_DIR="$BUILD_DIR" LD_LIBRARY_PATH="$BUILD_DIR" python3 -c "
import ctypes, errno as host_errno, fcntl as host_fcntl, os, resource, signal, subprocess, sys, tempfile, threading, time

build_dir = os.environ['BUILD_DIR']
lib = ctypes.CDLL(os.path.join(build_dir, 'libsystem_shim.so'))

for removed_symbol in [
    'IOHIDEventGetFloatValue',
    'IOHIDEventSystemClientCreate',
    'IOHIDServiceClientCopyEvent',
    'LSOpenCFURLRef',
    'NSStartSearchPathEnumeration',
    'NSGetNextSearchPathEnumeration',
    'CFRunLoopGetCurrent',
    'CFRunLoopIsWaiting',
    'CFRunLoopRun',
    'CFRunLoopStop',
    'CFRunLoopWakeUp',
    'FSEventStreamCreate',
    'FSEventStreamStart',
    'FSEventsGetCurrentEventId',
    'mach_msg_server_once',
    'mach_port_allocate',
    'mach_port_construct',
    'mach_port_insert_right',
    'mach_port_set_attributes',
    'thread_get_state',
    'thread_info',
    'thread_resume',
    'thread_set_exception_ports',
    'thread_suspend',
    'u_charDirection',
    'ubrk_open',
    'ucal_open',
    'ucol_open',
    'udat_open',
    'uidna_openUTS46',
    'uloc_getDefault',
    'unorm2_normalize',
    'unum_open',
    'ures_open',
    'utext_setup',
]:
    assert not hasattr(lib, removed_symbol), f'unexpected target-excluded symbol: {removed_symbol}'

class MachNdrRecord(ctypes.Structure):
    _fields_ = [
        ('mig_version', ctypes.c_ubyte),
        ('interface_version', ctypes.c_ubyte),
        ('reserved', ctypes.c_ubyte),
        ('protocol', ctypes.c_ubyte),
        ('integer_representation', ctypes.c_ubyte),
        ('character_representation', ctypes.c_ubyte),
        ('float_representation', ctypes.c_ubyte),
        ('trailing_reserved', ctypes.c_ubyte),
    ]

ndr_record = MachNdrRecord.in_dll(lib, 'NDR_record')
assert bytes(ndr_record) == bytes([0, 0, 0, 0, 1, 0, 0, 0])

io_main_port = ctypes.c_uint32.in_dll(lib, 'kIOMainPortDefault')
assert io_main_port.value == 0
lib.IOServiceMatching.argtypes = [ctypes.c_char_p]
lib.IOServiceMatching.restype = ctypes.c_void_p
lib.IOServiceGetMatchingService.argtypes = [ctypes.c_uint32, ctypes.c_void_p]
lib.IOServiceGetMatchingService.restype = ctypes.c_uint32
matching_service = lib.IOServiceMatching(b'IOPlatformDevice')
assert matching_service
assert lib.IOServiceGetMatchingService(io_main_port.value, matching_service) == 0
lib.CFRelease.argtypes = [ctypes.c_void_p]
lib.CFRelease(matching_service)

lib.notify_register_file_descriptor.argtypes = [
    ctypes.c_char_p, ctypes.POINTER(ctypes.c_int), ctypes.c_int,
    ctypes.POINTER(ctypes.c_int),
]
lib.notify_register_file_descriptor.restype = ctypes.c_uint32
lib.notify_cancel.argtypes = [ctypes.c_int]
lib.notify_cancel.restype = ctypes.c_uint32
lib.notify_is_valid_token.argtypes = [ctypes.c_int]
lib.notify_is_valid_token.restype = ctypes.c_int
notify_fd = ctypes.c_int(42)
notify_token = ctypes.c_int(42)
assert lib.notify_register_file_descriptor(
    b'org.machgate.unsupported', ctypes.byref(notify_fd), 0,
    ctypes.byref(notify_token)
) == 1000000
assert notify_fd.value == -1
assert notify_token.value == -1
assert lib.notify_cancel(notify_token.value) == 2
assert lib.notify_is_valid_token(notify_token.value) == 0

class MachTimespec(ctypes.Structure):
    _fields_ = [('tv_sec', ctypes.c_uint32), ('tv_nsec', ctypes.c_int32)]

lib.semaphore_create.argtypes = [
    ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint32), ctypes.c_int,
    ctypes.c_int,
]
lib.semaphore_create.restype = ctypes.c_int32
lib.semaphore_destroy.argtypes = [ctypes.c_uint32, ctypes.c_uint32]
lib.semaphore_destroy.restype = ctypes.c_int32
lib.semaphore_signal.argtypes = [ctypes.c_uint32]
lib.semaphore_signal.restype = ctypes.c_int32
lib.semaphore_wait.argtypes = [ctypes.c_uint32]
lib.semaphore_wait.restype = ctypes.c_int32
lib.semaphore_timedwait.argtypes = [ctypes.c_uint32, MachTimespec]
lib.semaphore_timedwait.restype = ctypes.c_int32
mach_semaphore = ctypes.c_uint32()
assert lib.semaphore_create(0, ctypes.byref(mach_semaphore), 0, 0) == 0
assert mach_semaphore.value != 0
assert lib.semaphore_timedwait(mach_semaphore, MachTimespec(0, 0)) == 49
assert lib.semaphore_signal(mach_semaphore) == 0
assert lib.semaphore_wait(mach_semaphore) == 0
assert lib.semaphore_destroy(0, mach_semaphore) == 0
assert lib.semaphore_signal(mach_semaphore) == 4

lib.getenv.argtypes = [ctypes.c_char_p]
lib.getenv.restype = ctypes.c_char_p
expected_home = os.environ['HOME'].encode()
assert lib.getenv(b'HOME') == expected_home, 'getenv changed HOME instead of preserving the guest environment'

cf_null = ctypes.c_void_p.in_dll(lib, 'kCFNull')
assert cf_null.value, 'kCFNull was not initialized'
lib.CFStringCreateWithCStringNoCopy.argtypes = [
    ctypes.c_void_p, ctypes.c_char_p, ctypes.c_uint32, ctypes.c_void_p
]
lib.CFStringCreateWithCStringNoCopy.restype = ctypes.c_void_p
no_copy_string = lib.CFStringCreateWithCStringNoCopy(
    None, b'shim-nocopy', 0, None
)
assert ctypes.string_at(no_copy_string) == b'shim-nocopy'

random_bytes = (ctypes.c_uint8 * 16)()
lib.SecRandomCopyBytes.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p]
assert lib.SecRandomCopyBytes(None, len(random_bytes), random_bytes) == 0
assert any(random_bytes), 'SecRandomCopyBytes returned an all-zero sample'
trust_certificates = ctypes.c_void_p(1)
lib.SecTrustSettingsCopyCertificates.argtypes = [
    ctypes.c_int, ctypes.POINTER(ctypes.c_void_p)
]
assert lib.SecTrustSettingsCopyCertificates(
    0, ctypes.byref(trust_certificates)
) == -25300
assert trust_certificates.value is None

lib.mach_absolute_time.restype = ctypes.c_uint64
absolute_time_samples = [lib.mach_absolute_time() for _ in range(2)]
assert absolute_time_samples[0] != 0, 'absolute time must be nonzero'
assert absolute_time_samples == sorted(absolute_time_samples), (
    f'absolute time was not monotonic: {absolute_time_samples}'
)

class TimebaseInfo(ctypes.Structure):
    _fields_ = [('numer', ctypes.c_uint32), ('denom', ctypes.c_uint32)]

timebase = TimebaseInfo()
assert lib.mach_timebase_info(ctypes.byref(timebase)) == 0
assert (timebase.numer, timebase.denom) == (1, 1), (
    f'unexpected timebase: {(timebase.numer, timebase.denom)}'
)
assert lib.mach_timebase_info(None) == 4, 'null timebase output was not rejected'

lib.clock_gettime_nsec_np.argtypes = [ctypes.c_int]
lib.clock_gettime_nsec_np.restype = ctypes.c_uint64
realtime_ns = lib.clock_gettime_nsec_np(0)
expected_realtime_ns = time.time_ns()
assert abs(realtime_ns - expected_realtime_ns) < 5_000_000_000, (
    f'realtime clock differs by too much: {realtime_ns} vs {expected_realtime_ns}'
)
process_ns = lib.clock_gettime_nsec_np(12)
thread_ns = lib.clock_gettime_nsec_np(16)
assert process_ns > 0, 'process CPU clock returned zero'
assert thread_ns > 0, 'thread CPU clock returned zero'

lib.task_info.restype = ctypes.c_int
task_self = ctypes.c_uint32.in_dll(lib, 'mach_task_self_').value
assert lib.task_info(task_self, 0, None, None) == 46, 'unknown task_info flavor did not fail explicitly'
lib.task_policy_set.argtypes = [
    ctypes.c_uint32, ctypes.c_uint32, ctypes.POINTER(ctypes.c_int32),
    ctypes.c_uint32,
]
lib.task_policy_set.restype = ctypes.c_int32
default_application = ctypes.c_int32(7)
assert lib.task_policy_set(
    task_self, 1, ctypes.byref(default_application), 1
) == 46, 'unsupported task category policy reported success'
assert lib.task_policy_set(
    task_self, 1, None, 1
) == 4, 'null task category policy was not rejected'
assert lib.task_policy_set(
    task_self, 1, ctypes.byref(default_application), 0
) == 4, 'invalid task category policy count was not rejected'
assert lib.task_policy_set(
    task_self + 1, 1, ctypes.byref(default_application), 1
) == 4, 'non-current task policy was not rejected'
lib.mach_msg.restype = ctypes.c_int
assert lib.mach_msg(None, 1, 0, 0, 0, 0, 0) == 0x10000003, (
    'unsupported Mach send did not report an invalid destination'
)
assert lib.mach_msg(None, 2, 0, 0, 0, 0, 0) == 0x10004002, (
    'unsupported Mach receive did not report an invalid receive name'
)
lib.mach_port_deallocate.restype = ctypes.c_int
assert lib.mach_port_deallocate(0, 0) == 46, (
    'unsupported port deallocation did not fail explicitly'
)

# __error should return a valid pointer (errno location)
lib.__error.restype = ctypes.POINTER(ctypes.c_int)
p = lib.__error()
assert p, '__error returned NULL'

class DarwinRlimit(ctypes.Structure):
    _fields_ = [
        ('current', ctypes.c_uint64),
        ('maximum', ctypes.c_uint64),
    ]

darwin_infinity = (1 << 63) - 1

def darwin_limit(host_limit):
    return darwin_infinity if host_limit == resource.RLIM_INFINITY else host_limit

darwin_getrlimit = lib.machgate_darwin_getrlimit
darwin_getrlimit.argtypes = [ctypes.c_int, ctypes.POINTER(DarwinRlimit)]
darwin_getrlimit.restype = ctypes.c_int
darwin_setrlimit = lib.machgate_darwin_setrlimit
darwin_setrlimit.argtypes = [ctypes.c_int, ctypes.POINTER(DarwinRlimit)]
darwin_setrlimit.restype = ctypes.c_int

for darwin_resource, host_resource in [
    (0, resource.RLIMIT_CPU),
    (1, resource.RLIMIT_FSIZE),
    (2, resource.RLIMIT_DATA),
    (3, resource.RLIMIT_STACK),
    (4, resource.RLIMIT_CORE),
    (5, resource.RLIMIT_AS),
    (6, resource.RLIMIT_MEMLOCK),
    (7, resource.RLIMIT_NPROC),
    (8, resource.RLIMIT_NOFILE),
]:
    host_limits = resource.getrlimit(host_resource)
    limits = DarwinRlimit()
    assert darwin_getrlimit(darwin_resource, ctypes.byref(limits)) == 0
    assert limits.current == darwin_limit(host_limits[0]), (
        f'Darwin resource {darwin_resource} returned current {limits.current}, '
        f'expected {darwin_limit(host_limits[0])}'
    )
    assert limits.maximum == darwin_limit(host_limits[1]), (
        f'Darwin resource {darwin_resource} returned maximum {limits.maximum}, '
        f'expected {darwin_limit(host_limits[1])}'
    )

nofile_limits = DarwinRlimit()
assert darwin_getrlimit(8, ctypes.byref(nofile_limits)) == 0
assert darwin_setrlimit(8, ctypes.byref(nofile_limits)) == 0
roundtrip_nofile_limits = DarwinRlimit()
assert darwin_getrlimit(8, ctypes.byref(roundtrip_nofile_limits)) == 0
assert roundtrip_nofile_limits.current == nofile_limits.current
assert roundtrip_nofile_limits.maximum == nofile_limits.maximum

host_cpu_limits = resource.getrlimit(resource.RLIMIT_CPU)
if host_cpu_limits == (resource.RLIM_INFINITY, resource.RLIM_INFINITY):
    unlimited_limits = DarwinRlimit(darwin_infinity, darwin_infinity)
    assert darwin_setrlimit(0, ctypes.byref(unlimited_limits)) == 0
    assert resource.getrlimit(resource.RLIMIT_CPU) == host_cpu_limits

p.contents.value = 0
assert darwin_getrlimit(9, ctypes.byref(nofile_limits)) == -1
assert p.contents.value == host_errno.EINVAL

p.contents.value = 0
assert darwin_getrlimit(8, None) == -1
assert p.contents.value == host_errno.EFAULT

lib.sysctlbyname.argtypes = [
    ctypes.c_char_p, ctypes.c_void_p, ctypes.POINTER(ctypes.c_size_t),
    ctypes.c_void_p, ctypes.c_size_t,
]
lib.sysctlbyname.restype = ctypes.c_int
lib.sysctl.argtypes = [
    ctypes.POINTER(ctypes.c_int), ctypes.c_uint, ctypes.c_void_p,
    ctypes.POINTER(ctypes.c_size_t), ctypes.c_void_p, ctypes.c_size_t,
]
lib.sysctl.restype = ctypes.c_int
lib.sysctlnametomib.argtypes = [
    ctypes.c_char_p, ctypes.POINTER(ctypes.c_int),
    ctypes.POINTER(ctypes.c_size_t),
]
lib.sysctlnametomib.restype = ctypes.c_int

cpu_type_size = ctypes.c_size_t(0)
assert lib.sysctlbyname(b'hw.cputype', None, ctypes.byref(cpu_type_size), None, 0) == 0
assert cpu_type_size.value == ctypes.sizeof(ctypes.c_int32)
cpu_type = (ctypes.c_int32 * 2)(-1, 0x12345678)
assert lib.sysctlbyname(
    b'hw.cputype', cpu_type, ctypes.byref(cpu_type_size), None, 0
) == 0
assert cpu_type[0] == 0x0100000c, 'ARM64 guests must discover CPU_TYPE_ARM64'
assert cpu_type[1] == 0x12345678
assert cpu_type_size.value == ctypes.sizeof(ctypes.c_int32)

cpu_size = ctypes.c_size_t()
assert lib.sysctlbyname(b'hw.ncpu', None, ctypes.byref(cpu_size), None, 0) == 0
assert cpu_size.value == ctypes.sizeof(ctypes.c_int)
cpu_count = ctypes.c_int()
assert lib.sysctlbyname(
    b'hw.ncpu', ctypes.byref(cpu_count), ctypes.byref(cpu_size), None, 0
) == 0
assert cpu_count.value >= 1, f'hw.ncpu returned {cpu_count.value}'

page_size = ctypes.c_int()
page_size_length = ctypes.c_size_t(ctypes.sizeof(page_size))
assert lib.sysctlbyname(
    b'hw.pagesize', ctypes.byref(page_size), ctypes.byref(page_size_length), None, 0
) == 0
assert page_size.value >= 4096, f'hw.pagesize returned {page_size.value}'

memory_size_buffer = ctypes.create_string_buffer(1)
memory_size_length = ctypes.c_size_t(1)
p.contents.value = 0
assert lib.sysctlbyname(
    b'hw.memsize', memory_size_buffer, ctypes.byref(memory_size_length), None, 0
) == -1
assert p.contents.value == host_errno.ENOMEM
assert memory_size_length.value == ctypes.sizeof(ctypes.c_uint64)

p.contents.value = 0
unknown_length = ctypes.c_size_t()
assert lib.sysctlbyname(
    b'machgate.unknown', None, ctypes.byref(unknown_length), None, 0
) == -1
assert p.contents.value == host_errno.ENOENT

mib = (ctypes.c_int * 2)()
mib_length = ctypes.c_size_t(2)
assert lib.sysctlnametomib(b'hw.ncpu', mib, ctypes.byref(mib_length)) == 0
assert list(mib) == [6, 3]
assert mib_length.value == 2

short_mib_length = ctypes.c_size_t(1)
p.contents.value = 0
assert lib.sysctlnametomib(
    b'hw.ncpu', mib, ctypes.byref(short_mib_length)
) == -1
assert p.contents.value == host_errno.ENOMEM
assert short_mib_length.value == 2

mib_cpu_count = ctypes.c_int()
mib_cpu_size = ctypes.c_size_t(ctypes.sizeof(mib_cpu_count))
assert lib.sysctl(
    mib, 2, ctypes.byref(mib_cpu_count), ctypes.byref(mib_cpu_size), None, 0
) == 0
assert mib_cpu_count.value == cpu_count.value

darwin_kinfo_proc_size = 0x288
darwin_p_traced = 0x00000800
darwin_p_flag_offset = 32
darwin_p_pid_offset = 40
process_mib = (ctypes.c_int * 4)(1, 14, 1, os.getpid())
process_info = ctypes.create_string_buffer(darwin_kinfo_proc_size)
ctypes.memset(process_info, 0xa5, darwin_kinfo_proc_size)
process_info_size = ctypes.c_size_t(darwin_kinfo_proc_size)
assert lib.sysctl(
    process_mib, 4, process_info, ctypes.byref(process_info_size), None, 0
) == 0
assert process_info_size.value == darwin_kinfo_proc_size
with open('/proc/self/status') as process_status:
    tracer_pid = next(
        int(line.split(':', 1)[1])
        for line in process_status
        if line.startswith('TracerPid:')
    )
expected_process_flags = darwin_p_traced if tracer_pid else 0
returned_process_flags = ctypes.c_int32.from_buffer(
    process_info, darwin_p_flag_offset
).value
returned_process_pid = ctypes.c_int32.from_buffer(
    process_info, darwin_p_pid_offset
).value
assert returned_process_flags == expected_process_flags, (
    f'kern.proc.pid returned flags {returned_process_flags:#x}, '
    f'expected {expected_process_flags:#x}'
)
assert returned_process_pid == os.getpid(), (
    f'kern.proc.pid returned pid {returned_process_pid}, expected {os.getpid()}'
)
expected_process_info = bytearray(darwin_kinfo_proc_size)
expected_process_info[darwin_p_flag_offset:darwin_p_flag_offset + 4] = (
    expected_process_flags.to_bytes(4, byteorder=sys.byteorder)
)
expected_process_info[darwin_p_pid_offset:darwin_p_pid_offset + 4] = (
    os.getpid().to_bytes(4, byteorder=sys.byteorder, signed=True)
)
assert process_info.raw == bytes(expected_process_info), (
    'kern.proc.pid returned fields outside the minimal process representation'
)

process_info_size = ctypes.c_size_t()
assert lib.sysctl(
    process_mib, 4, None, ctypes.byref(process_info_size), None, 0
) == 0
assert process_info_size.value == darwin_kinfo_proc_size

short_process_info = ctypes.create_string_buffer(darwin_kinfo_proc_size - 1)
short_process_info_size = ctypes.c_size_t(len(short_process_info))
p.contents.value = 0
assert lib.sysctl(
    process_mib, 4, short_process_info,
    ctypes.byref(short_process_info_size), None, 0
) == -1
assert p.contents.value == host_errno.ENOMEM
assert short_process_info_size.value == darwin_kinfo_proc_size

p.contents.value = 0
assert lib.sysctl(process_mib, 4, process_info, None, None, 0) == -1
assert p.contents.value == host_errno.EINVAL

unsupported_process_mib = (ctypes.c_int * 4)(1, 14, 0, os.getpid())
p.contents.value = 0
assert lib.sysctl(
    unsupported_process_mib, 4, process_info,
    ctypes.byref(process_info_size), None, 0
) == -1
assert p.contents.value == host_errno.ENOENT

other_process_mib = (ctypes.c_int * 4)(1, 14, 1, os.getpid() + 1)
p.contents.value = 0
assert lib.sysctl(
    other_process_mib, 4, process_info,
    ctypes.byref(process_info_size), None, 0
) == -1
assert p.contents.value == host_errno.ESRCH

replacement_process_info = ctypes.c_int(1)
process_info_size = ctypes.c_size_t(darwin_kinfo_proc_size)
p.contents.value = 0
assert lib.sysctl(
    process_mib, 4, process_info, ctypes.byref(process_info_size),
    ctypes.byref(replacement_process_info), ctypes.sizeof(replacement_process_info)
) == -1
assert p.contents.value == host_errno.EPERM

traced_process_probe = '''
import ctypes, os, sys

host_libc = ctypes.CDLL(None, use_errno=True)
host_libc.ptrace.argtypes = [
    ctypes.c_uint, ctypes.c_uint, ctypes.c_void_p, ctypes.c_void_p,
]
host_libc.ptrace.restype = ctypes.c_long
if host_libc.ptrace(0, 0, None, None) != 0:
    sys.exit(77)

lib = ctypes.CDLL(os.path.join(sys.argv[1], 'libsystem_shim.so'))
lib.sysctl.argtypes = [
    ctypes.POINTER(ctypes.c_int), ctypes.c_uint, ctypes.c_void_p,
    ctypes.POINTER(ctypes.c_size_t), ctypes.c_void_p, ctypes.c_size_t,
]
lib.sysctl.restype = ctypes.c_int
process_mib = (ctypes.c_int * 4)(1, 14, 1, os.getpid())
process_info = ctypes.create_string_buffer(0x288)
process_info_size = ctypes.c_size_t(len(process_info))
assert lib.sysctl(
    process_mib, 4, process_info, ctypes.byref(process_info_size), None, 0
) == 0
assert ctypes.c_int32.from_buffer(process_info, 32).value == 0x00000800
assert ctypes.c_int32.from_buffer(process_info, 40).value == os.getpid()
'''
traced_process_result = subprocess.run(
    [sys.executable, '-c', traced_process_probe, build_dir],
    capture_output=True, text=True,
)
assert traced_process_result.returncode in (0, 77), (
    'traced kern.proc.pid probe failed:\n'
    + traced_process_result.stdout + traced_process_result.stderr
)

uname_buffer = ctypes.create_string_buffer(256 * 5)
lib.uname.argtypes = [ctypes.c_void_p]
lib.uname.restype = ctypes.c_int
assert lib.uname(uname_buffer) == 0
uname_fields = [
    uname_buffer.raw[offset:offset + 256].split(b'\0', 1)[0]
    for offset in range(0, 256 * 5, 256)
]
assert uname_fields[0] == b'Darwin'
assert uname_fields[1], 'uname returned an empty host name'
assert uname_fields[2] == b'24.6.0'
assert b'MachGate' in uname_fields[3]
assert uname_fields[4] == b'arm64'

class FileTimespec(ctypes.Structure):
    _fields_ = [('seconds', ctypes.c_long), ('nanoseconds', ctypes.c_long)]

class DarwinFileStatus(ctypes.Structure):
    _fields_ = [
        ('device', ctypes.c_int32),
        ('mode', ctypes.c_uint16),
        ('link_count', ctypes.c_uint16),
        ('inode', ctypes.c_uint64),
        ('user_id', ctypes.c_uint32),
        ('group_id', ctypes.c_uint32),
        ('special_device', ctypes.c_int32),
        ('alignment_padding', ctypes.c_int32),
        ('access_time', FileTimespec),
        ('modification_time', FileTimespec),
        ('status_change_time', FileTimespec),
        ('creation_time', FileTimespec),
        ('byte_size', ctypes.c_int64),
        ('allocated_blocks', ctypes.c_int64),
        ('preferred_block_size', ctypes.c_int32),
        ('flags', ctypes.c_uint32),
        ('generation', ctypes.c_uint32),
        ('reserved', ctypes.c_int32),
        ('extended_reserved', ctypes.c_int64 * 2),
    ]

assert ctypes.sizeof(DarwinFileStatus) == 144
assert DarwinFileStatus.mode.offset == 4
assert DarwinFileStatus.inode.offset == 8
assert DarwinFileStatus.access_time.offset == 32
assert DarwinFileStatus.byte_size.offset == 96
assert DarwinFileStatus.extended_reserved.offset == 128

lib.stat.argtypes = [ctypes.c_char_p, ctypes.POINTER(DarwinFileStatus)]
lib.stat.restype = ctypes.c_int
lib.fstat.argtypes = [ctypes.c_int, ctypes.POINTER(DarwinFileStatus)]
lib.fstat.restype = ctypes.c_int
lib.lstat.argtypes = [ctypes.c_char_p, ctypes.POINTER(DarwinFileStatus)]
lib.lstat.restype = ctypes.c_int
lib.fstatat.argtypes = [
    ctypes.c_int, ctypes.c_char_p, ctypes.POINTER(DarwinFileStatus), ctypes.c_int,
]
lib.fstatat.restype = ctypes.c_int

with tempfile.TemporaryDirectory() as metadata_directory:
    file_path = os.path.join(metadata_directory, 'entry.txt')
    link_path = os.path.join(metadata_directory, 'entry-link')
    payload = b'machgate-metadata'
    with open(file_path, 'wb') as metadata_file:
        metadata_file.write(payload)
    os.symlink('entry.txt', link_path)

    file_status = DarwinFileStatus()
    assert lib.stat(file_path.encode(), ctypes.byref(file_status)) == 0
    assert file_status.byte_size == len(payload)
    assert file_status.mode & 0o170000 == 0o100000
    assert file_status.link_count >= 1
    assert file_status.user_id == os.getuid()
    assert file_status.group_id == os.getgid()
    assert file_status.alignment_padding == 0
    assert file_status.flags == 0
    assert file_status.generation == 0
    assert file_status.reserved == 0
    assert list(file_status.extended_reserved) == [0, 0]

    descriptor = os.open(file_path, os.O_RDONLY)
    try:
        descriptor_status = DarwinFileStatus()
        assert lib.fstat(descriptor, ctypes.byref(descriptor_status)) == 0
        assert descriptor_status.inode == file_status.inode
        assert descriptor_status.byte_size == file_status.byte_size
    finally:
        os.close(descriptor)

    followed_status = DarwinFileStatus()
    link_status = DarwinFileStatus()
    assert lib.stat(link_path.encode(), ctypes.byref(followed_status)) == 0
    assert lib.lstat(link_path.encode(), ctypes.byref(link_status)) == 0
    assert followed_status.mode & 0o170000 == 0o100000
    assert link_status.mode & 0o170000 == 0o120000

    fstatat_status = DarwinFileStatus()
    assert lib.fstatat(
        -2, link_path.encode(), ctypes.byref(fstatat_status), 0x0020
    ) == 0
    assert fstatat_status.mode & 0o170000 == 0o120000

    p.contents.value = 0
    assert lib.stat(
        os.path.join(metadata_directory, 'missing').encode(),
        ctypes.byref(DarwinFileStatus()),
    ) == -1
    assert p.contents.value == host_errno.ENOENT

    p.contents.value = 0
    assert lib.stat(file_path.encode(), None) == -1
    assert p.contents.value == host_errno.EFAULT

    class DarwinDirectoryEntry(ctypes.Structure):
        _fields_ = [
            ('inode', ctypes.c_uint64),
            ('seek_offset', ctypes.c_uint64),
            ('record_length', ctypes.c_uint16),
            ('name_length', ctypes.c_uint16),
            ('type', ctypes.c_uint8),
            ('name', ctypes.c_char * 1024),
            ('alignment_padding', ctypes.c_char * 3),
        ]

    assert ctypes.sizeof(DarwinDirectoryEntry) == 1048
    assert DarwinDirectoryEntry.record_length.offset == 16
    assert DarwinDirectoryEntry.type.offset == 20
    assert DarwinDirectoryEntry.name.offset == 21

    directory_libc = ctypes.CDLL('libc.so.6')
    directory_libc.opendir.argtypes = [ctypes.c_char_p]
    directory_libc.opendir.restype = ctypes.c_void_p
    directory_libc.closedir.argtypes = [ctypes.c_void_p]
    directory_libc.closedir.restype = ctypes.c_int
    lib.readdir.argtypes = [ctypes.c_void_p]
    lib.readdir.restype = ctypes.POINTER(DarwinDirectoryEntry)
    lib.readdir_r.argtypes = [
        ctypes.c_void_p, ctypes.POINTER(DarwinDirectoryEntry),
        ctypes.POINTER(ctypes.c_void_p),
    ]
    lib.readdir_r.restype = ctypes.c_int

    directory = directory_libc.opendir(metadata_directory.encode())
    assert directory
    try:
        observed_entry = None
        while True:
            entry_pointer = lib.readdir(directory)
            if not entry_pointer:
                break
            entry = entry_pointer.contents
            if bytes(entry.name) == b'entry.txt':
                observed_entry = entry
                break
        assert observed_entry is not None, 'readdir omitted the fixture entry'
        assert observed_entry.name_length == len(b'entry.txt')
        expected_record_length = (25 + len(b'entry.txt') + 7) & ~7
        assert observed_entry.record_length == expected_record_length
        assert observed_entry.type == 8
    finally:
        assert directory_libc.closedir(directory) == 0

    directory = directory_libc.opendir(metadata_directory.encode())
    assert directory
    try:
        reentrant_entry = DarwinDirectoryEntry()
        reentrant_result = ctypes.c_void_p()
        while True:
            reentrant_status = lib.readdir_r(
                directory, ctypes.byref(reentrant_entry), ctypes.byref(reentrant_result)
            )
            assert reentrant_status == 0
            if not reentrant_result.value:
                break
            if bytes(reentrant_entry.name) == b'entry-link':
                break
        assert reentrant_result.value
        assert reentrant_entry.type == 10
        assert lib.readdir_r(directory, None, ctypes.byref(reentrant_result)) == host_errno.EINVAL
    finally:
        assert directory_libc.closedir(directory) == 0

lib.open.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_uint]
lib.open.restype = ctypes.c_int
lib.openat.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_uint]
lib.openat.restype = ctypes.c_int
lib.close.argtypes = [ctypes.c_int]
lib.close.restype = ctypes.c_int
lib.machgate_darwin_fcntl_fixed.argtypes = [
    ctypes.c_int, ctypes.c_int, ctypes.c_ulong,
]
lib.machgate_darwin_fcntl_fixed.restype = ctypes.c_int
lib.machgate_darwin_ioctl_fixed.argtypes = [
    ctypes.c_int, ctypes.c_ulong, ctypes.c_ulong,
]
lib.machgate_darwin_ioctl_fixed.restype = ctypes.c_int
lib.renameatx_np.argtypes = [
    ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_uint,
]
lib.renameatx_np.restype = ctypes.c_int
lib.dup.argtypes = [ctypes.c_int]
lib.dup.restype = ctypes.c_int
lib.pipe.argtypes = [ctypes.POINTER(ctypes.c_int)]
lib.pipe.restype = ctypes.c_int
lib.pipe2.argtypes = [ctypes.POINTER(ctypes.c_int), ctypes.c_int]
lib.pipe2.restype = ctypes.c_int

DARWIN_O_RDWR = 0x00000002
DARWIN_O_NONBLOCK = 0x00000004
DARWIN_O_APPEND = 0x00000008
DARWIN_O_CREAT = 0x00000200
DARWIN_O_TRUNC = 0x00000400
DARWIN_O_CLOEXEC = 0x01000000
DARWIN_F_GETFL = 3
DARWIN_F_SETFL = 4
DARWIN_F_GETPATH = 50
DARWIN_F_DUPFD_CLOEXEC = 67

with tempfile.TemporaryDirectory() as file_operation_directory:
    first_path = os.path.join(file_operation_directory, 'created.txt')
    second_path = os.path.join(file_operation_directory, 'renamed.txt')
    descriptor = lib.open(
        first_path.encode(), DARWIN_O_RDWR | DARWIN_O_CREAT | DARWIN_O_TRUNC,
        0o640,
    )
    assert descriptor >= 0
    try:
        assert os.write(descriptor, b'file-abi') == len(b'file-abi')
        open_flags = lib.machgate_darwin_fcntl_fixed(
            descriptor, DARWIN_F_GETFL, 0
        )
        assert open_flags & 0x3 == DARWIN_O_RDWR
        assert lib.machgate_darwin_fcntl_fixed(
            descriptor, DARWIN_F_SETFL, DARWIN_O_RDWR | DARWIN_O_APPEND
        ) == 0
        assert lib.machgate_darwin_fcntl_fixed(
            descriptor, DARWIN_F_GETFL, 0
        ) & DARWIN_O_APPEND

        descriptor_path = ctypes.create_string_buffer(1024)
        assert lib.machgate_darwin_fcntl_fixed(
            descriptor, DARWIN_F_GETPATH, ctypes.addressof(descriptor_path)
        ) == 0
        assert descriptor_path.value.decode() == first_path

        cloexec_duplicate = lib.machgate_darwin_fcntl_fixed(
            descriptor, DARWIN_F_DUPFD_CLOEXEC, 0
        )
        assert cloexec_duplicate >= 0
        assert host_fcntl.fcntl(
            cloexec_duplicate, host_fcntl.F_GETFD
        ) & host_fcntl.FD_CLOEXEC
        assert lib.close(cloexec_duplicate) == 0
    finally:
        assert lib.close(descriptor) == 0

    openat_descriptor = lib.openat(
        -2, second_path.encode(), DARWIN_O_RDWR | DARWIN_O_CREAT, 0o600
    )
    assert openat_descriptor >= 0
    assert lib.close(openat_descriptor) == 0
    assert lib.renameatx_np(-2, second_path.encode(), -2, first_path.encode(), 0) == 0
    p.contents.value = 0
    assert lib.renameatx_np(
        -2, first_path.encode(), -2, second_path.encode(), 0x80
    ) == -1
    assert p.contents.value == host_errno.ENOTSUP

pipe_descriptors = (ctypes.c_int * 2)()
assert lib.pipe(pipe_descriptors) == 0
try:
    assert os.write(pipe_descriptors[1], b'abc') == 3
    readable_bytes = ctypes.c_int()
    assert lib.machgate_darwin_ioctl_fixed(
        pipe_descriptors[0], 0x4004667f, ctypes.addressof(readable_bytes)
    ) == 0
    assert readable_bytes.value == 3
    duplicated_pipe = lib.dup(pipe_descriptors[0])
    assert duplicated_pipe >= 0
    assert lib.close(duplicated_pipe) == 0
    p.contents.value = 0
    assert lib.machgate_darwin_ioctl_fixed(
        pipe_descriptors[0], 0x12345678, 0
    ) == -1
    assert p.contents.value == host_errno.ENOSYS
finally:
    assert lib.close(pipe_descriptors[0]) == 0
    assert lib.close(pipe_descriptors[1]) == 0

pipe2_descriptors = (ctypes.c_int * 2)()
assert lib.pipe2(
    pipe2_descriptors, DARWIN_O_NONBLOCK | DARWIN_O_CLOEXEC
) == 0
try:
    for pipe_descriptor in pipe2_descriptors:
        assert host_fcntl.fcntl(
            pipe_descriptor, host_fcntl.F_GETFD
        ) & host_fcntl.FD_CLOEXEC
        assert host_fcntl.fcntl(
            pipe_descriptor, host_fcntl.F_GETFL
        ) & os.O_NONBLOCK
finally:
    assert lib.close(pipe2_descriptors[0]) == 0
    assert lib.close(pipe2_descriptors[1]) == 0

class TlvDescriptor(ctypes.Structure):
    _fields_ = [
        ('resolver', ctypes.c_void_p),
        ('key', ctypes.c_size_t),
        ('offset', ctypes.c_size_t),
    ]

lib._tlv_bootstrap_impl.argtypes = [ctypes.POINTER(TlvDescriptor)]
lib._tlv_bootstrap_impl.restype = ctypes.c_void_p
lib._tlv_bootstrap.argtypes = [ctypes.POINTER(TlvDescriptor)]
lib._tlv_bootstrap.restype = ctypes.c_void_p
tlv_image_base = ctypes.c_void_p.in_dll(lib, '__tlv_image_base')
tlv_image_size = ctypes.c_size_t.in_dll(lib, '__tlv_image_size')
tlv_bss_size = ctypes.c_size_t.in_dll(lib, '__tlv_bss_size')
tlv_initial_bytes = (ctypes.c_uint8 * 4)(1, 2, 3, 4)
tlv_image_base.value = ctypes.addressof(tlv_initial_bytes)
tlv_image_size.value = len(tlv_initial_bytes)
tlv_bss_size.value = 4

tlv_descriptor = TlvDescriptor(None, 0, 2)
main_tlv_address = lib._tlv_bootstrap_impl(ctypes.byref(tlv_descriptor))
assert main_tlv_address
assert tlv_descriptor.key != 0
assert list((ctypes.c_uint8 * 6).from_address(main_tlv_address)) == [3, 4, 0, 0, 0, 0]
assert lib._tlv_bootstrap(ctypes.byref(tlv_descriptor)) == main_tlv_address
(ctypes.c_uint8 * 1).from_address(main_tlv_address)[0] = 99

thread_tlv_results = []

def read_thread_local_image():
    address = lib._tlv_bootstrap(ctypes.byref(tlv_descriptor))
    thread_tlv_results.append((
        address,
        list((ctypes.c_uint8 * 6).from_address(address)),
    ))

tlv_thread = threading.Thread(target=read_thread_local_image)
tlv_thread.start()
tlv_thread.join(1.0)
assert not tlv_thread.is_alive(), 'TLV worker did not finish'
assert len(thread_tlv_results) == 1
assert thread_tlv_results[0][0] != main_tlv_address
assert thread_tlv_results[0][1] == [3, 4, 0, 0, 0, 0]

large_offset_descriptor = TlvDescriptor(None, 0, 5000)
large_offset_address = lib._tlv_bootstrap_impl(
    ctypes.byref(large_offset_descriptor)
)
assert large_offset_address
assert ctypes.c_uint8.from_address(large_offset_address).value == 0

p.contents.value = 0
assert not lib._tlv_bootstrap_impl(None)
assert p.contents.value == host_errno.EINVAL

lib.dispatch_semaphore_create.argtypes = [ctypes.c_long]
lib.dispatch_semaphore_create.restype = ctypes.c_void_p
lib.dispatch_semaphore_wait.argtypes = [ctypes.c_void_p, ctypes.c_uint64]
lib.dispatch_semaphore_wait.restype = ctypes.c_long
lib.dispatch_semaphore_signal.argtypes = [ctypes.c_void_p]
lib.dispatch_semaphore_signal.restype = ctypes.c_long
lib.dispatch_time.argtypes = [ctypes.c_uint64, ctypes.c_int64]
lib.dispatch_time.restype = ctypes.c_uint64
lib.dispatch_walltime.argtypes = [ctypes.POINTER(FileTimespec), ctypes.c_int64]
lib.dispatch_walltime.restype = ctypes.c_uint64
lib.dispatch_retain.argtypes = [ctypes.c_void_p]
lib.dispatch_release.argtypes = [ctypes.c_void_p]

dispatch_semaphore = lib.dispatch_semaphore_create(0)
assert dispatch_semaphore
assert lib.dispatch_semaphore_wait(dispatch_semaphore, 0) != 0
assert lib.dispatch_semaphore_signal(dispatch_semaphore) == 0
assert lib.dispatch_semaphore_wait(dispatch_semaphore, 0) == 0
expired_dispatch_deadline = lib.dispatch_time(0, 1_000_000)
assert expired_dispatch_deadline > 0
assert lib.dispatch_semaphore_wait(
    dispatch_semaphore, expired_dispatch_deadline
) != 0

def signal_dispatch_semaphore():
    time.sleep(0.01)
    assert lib.dispatch_semaphore_signal(dispatch_semaphore) == 0

dispatch_signaler = threading.Thread(target=signal_dispatch_semaphore)
dispatch_signaler.start()
assert lib.dispatch_semaphore_wait(dispatch_semaphore, 0xffffffffffffffff) == 0
dispatch_signaler.join(1.0)
assert not dispatch_signaler.is_alive()
lib.dispatch_retain(dispatch_semaphore)
lib.dispatch_release(dispatch_semaphore)
lib.dispatch_release(dispatch_semaphore)

lib.dispatch_get_global_queue.argtypes = [ctypes.c_long, ctypes.c_ulong]
lib.dispatch_get_global_queue.restype = ctypes.c_void_p
lib.dispatch_queue_create.argtypes = [ctypes.c_char_p, ctypes.c_void_p]
lib.dispatch_queue_create.restype = ctypes.c_void_p
dispatch_global_queue = lib.dispatch_get_global_queue(0, 0)
assert dispatch_global_queue
dispatch_queue = lib.dispatch_queue_create(b'machgate.test.queue', None)
assert dispatch_queue

BLOCK_INVOKE = ctypes.CFUNCTYPE(None, ctypes.c_void_p)

class DispatchBlock(ctypes.Structure):
    _fields_ = [
        ('isa', ctypes.c_void_p),
        ('flags', ctypes.c_uint32),
        ('reserved', ctypes.c_uint32),
        ('invoke', BLOCK_INVOKE),
        ('descriptor', ctypes.c_void_p),
    ]

block_invocations = ctypes.c_int()

@BLOCK_INVOKE
def count_block_invocation(block_pointer):
    block_invocations.value += 1

dispatch_block = DispatchBlock(None, 0, 0, count_block_invocation, None)
lib.dispatch_sync.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
lib.dispatch_async.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
lib.dispatch_sync(dispatch_queue, ctypes.byref(dispatch_block))
lib.dispatch_async(dispatch_queue, ctypes.byref(dispatch_block))
assert block_invocations.value == 2

DISPATCH_FUNCTION = ctypes.CFUNCTYPE(None, ctypes.c_void_p)
once_invocations = ctypes.c_int()

@DISPATCH_FUNCTION
def count_once_invocation(context):
    time.sleep(0.005)
    once_invocations.value += 1

lib.dispatch_once_f.argtypes = [
    ctypes.POINTER(ctypes.c_long), ctypes.c_void_p, DISPATCH_FUNCTION,
]
once_predicate = ctypes.c_long()
once_threads = [
    threading.Thread(
        target=lambda: lib.dispatch_once_f(
            ctypes.byref(once_predicate), None, count_once_invocation
        )
    )
    for _ in range(8)
]
for once_thread in once_threads:
    once_thread.start()
for once_thread in once_threads:
    once_thread.join(1.0)
assert all(not once_thread.is_alive() for once_thread in once_threads)
assert once_invocations.value == 1
assert once_predicate.value == -1

lib.dispatch_data_create.argtypes = [
    ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p, ctypes.c_void_p,
]
lib.dispatch_data_create.restype = ctypes.c_void_p
dispatch_data_bytes = ctypes.create_string_buffer(b'data')
dispatch_data = lib.dispatch_data_create(
    dispatch_data_bytes, 4, dispatch_queue, None
)
assert dispatch_data
lib.dispatch_release(dispatch_data)

lib.dispatch_source_create.argtypes = [
    ctypes.c_void_p, ctypes.c_size_t, ctypes.c_size_t, ctypes.c_void_p,
]
lib.dispatch_source_create.restype = ctypes.c_void_p
p.contents.value = 0
mach_receive_source_type = ctypes.c_int.in_dll(
    lib, '_dispatch_source_type_mach_recv'
)
assert not lib.dispatch_source_create(
    ctypes.byref(mach_receive_source_type), 0, 0, dispatch_queue
)
assert p.contents.value == host_errno.ENOTSUP

assert lib.dispatch_walltime(None, 0) > 0
lib.dispatch_release(dispatch_queue)
lib.dispatch_release(dispatch_global_queue)

lib.__cxa_guard_acquire.argtypes = [ctypes.POINTER(ctypes.c_uint64)]
lib.__cxa_guard_acquire.restype = ctypes.c_int
lib.__cxa_guard_release.argtypes = [ctypes.POINTER(ctypes.c_uint64)]
lib.__cxa_guard_abort.argtypes = [ctypes.POINTER(ctypes.c_uint64)]

guard = ctypes.c_uint64(0)
assert lib.__cxa_guard_acquire(ctypes.byref(guard)) == 1, '__cxa_guard_acquire did not request initialization'

recursive_script = f'''
import ctypes, os
lib = ctypes.CDLL(os.path.join({build_dir!r}, \"libsystem_shim.so\"))
lib.__cxa_guard_acquire.argtypes = [ctypes.POINTER(ctypes.c_uint64)]
lib.__cxa_guard_acquire.restype = ctypes.c_int
guard = ctypes.c_uint64(0)
assert lib.__cxa_guard_acquire(ctypes.byref(guard)) == 1
lib.__cxa_guard_acquire(ctypes.byref(guard))
'''
recursive_result = subprocess.run([sys.executable, '-c', recursive_script])
assert recursive_result.returncode == -signal.SIGABRT, (
    f'recursive __cxa_guard_acquire returned {recursive_result.returncode}, expected SIGABRT'
)

wait_guard = ctypes.c_uint64(0)
wait_result = []
waiter_started = threading.Event()
assert lib.__cxa_guard_acquire(ctypes.byref(wait_guard)) == 1, 'wait guard initial acquire failed'

def wait_for_guard():
    waiter_started.set()
    wait_result.append(lib.__cxa_guard_acquire(ctypes.byref(wait_guard)))

waiter = threading.Thread(target=wait_for_guard)
waiter.start()
assert waiter_started.wait(1.0), 'waiter did not start'
time.sleep(0.05)
assert not wait_result, 'contended __cxa_guard_acquire returned before release'
lib.__cxa_guard_release(ctypes.byref(wait_guard))
waiter.join(1.0)
assert wait_result == [0], f'contended __cxa_guard_acquire result={wait_result}'

lib.__cxa_guard_release(ctypes.byref(guard))
assert lib.__cxa_guard_acquire(ctypes.byref(guard)) == 0, 'released __cxa_guard_acquire should report initialized'

abort_guard = ctypes.c_uint64(0)
assert lib.__cxa_guard_acquire(ctypes.byref(abort_guard)) == 1, '__cxa_guard_acquire failed for abort guard'
lib.__cxa_guard_abort(ctypes.byref(abort_guard))
assert lib.__cxa_guard_acquire(ctypes.byref(abort_guard)) == 1, '__cxa_guard_abort did not reset pending state'
lib.__cxa_guard_release(ctypes.byref(abort_guard))

assert_script = f'''
import ctypes, os
lib = ctypes.CDLL(os.path.join({build_dir!r}, \"libsystem_shim.so\"))
lib.__assert_rtn.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p]
lib.__assert_rtn(b\"test_fn\", b\"fixture.c\", 37, b\"value != 0\")
'''
assert_result = subprocess.run(
    [sys.executable, '-c', assert_script], capture_output=True, text=True
)
assert assert_result.returncode == -signal.SIGABRT, (
    f'__assert_rtn returned {assert_result.returncode}, expected SIGABRT'
)
for token in ['test_fn', 'fixture.c', '37', 'value != 0']:
    assert token in assert_result.stderr, (
        f'__assert_rtn diagnostic omitted {token!r}: {assert_result.stderr!r}'
    )

null_assert_script = f'''
import ctypes, os
lib = ctypes.CDLL(os.path.join({build_dir!r}, \"libsystem_shim.so\"))
lib.__assert_rtn.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p]
lib.__assert_rtn(None, b\"null.c\", 9, b\"failed\")
'''
null_assert_result = subprocess.run(
    [sys.executable, '-c', null_assert_script], capture_output=True, text=True
)
assert null_assert_result.returncode == -signal.SIGABRT, (
    f'null-function __assert_rtn returned {null_assert_result.returncode}'
)
for token in ['<unknown>', 'null.c', '9', 'failed']:
    assert token in null_assert_result.stderr, (
        f'null-function diagnostic omitted {token!r}: {null_assert_result.stderr!r}'
    )

UNWIND_TRACE = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p)
unwind_frames = []

@UNWIND_TRACE
def record_unwind_frame(context, argument):
    unwind_frames.append(context)
    return 0

lib._Unwind_Backtrace.argtypes = [UNWIND_TRACE, ctypes.c_void_p]
lib._Unwind_Backtrace.restype = ctypes.c_int
unwind_result = lib._Unwind_Backtrace(record_unwind_frame, None)
assert unwind_frames, '_Unwind_Backtrace did not visit a stack frame'
assert unwind_result != 3, '_Unwind_Backtrace reported fatal phase-one failure'
for unwind_symbol in (
    '_Unwind_DeleteException',
    '_Unwind_GetCFA',
    '_Unwind_GetDataRelBase',
    '_Unwind_GetIP',
    '_Unwind_GetIPInfo',
    '_Unwind_GetLanguageSpecificData',
    '_Unwind_GetRegionStart',
    '_Unwind_GetTextRelBase',
    '_Unwind_RaiseException',
    '_Unwind_Resume',
    '_Unwind_SetGR',
    '_Unwind_SetIP',
):
    assert hasattr(lib, unwind_symbol), f'missing standard unwind adapter: {unwind_symbol}'

lib.vfork.restype = ctypes.c_int
vfork_child = lib.vfork()
if vfork_child == 0:
    os._exit(0)
assert vfork_child > 0, f'vfork compatibility call failed: {vfork_child}'
waited_child, wait_status = os.waitpid(vfork_child, 0)
assert waited_child == vfork_child, 'vfork compatibility call returned wrong child'
assert os.waitstatus_to_exitcode(wait_status) == 0, 'vfork child did not exit cleanly'

pthread_t = ctypes.c_ulong
PTHREAD_START = ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.c_void_p)
lib.pthread_create.argtypes = [ctypes.POINTER(pthread_t), ctypes.c_void_p, PTHREAD_START, ctypes.c_void_p]
lib.pthread_create.restype = ctypes.c_int
lib.pthread_threadid_np.argtypes = [pthread_t, ctypes.POINTER(ctypes.c_uint64)]
lib.pthread_threadid_np.restype = ctypes.c_int
lib.pthread_mach_thread_np.argtypes = [pthread_t]
lib.pthread_mach_thread_np.restype = ctypes.c_uint32
lib.pthread_join.argtypes = [pthread_t, ctypes.POINTER(ctypes.c_void_p)]
lib.pthread_join.restype = ctypes.c_int
parent_thread_id = ctypes.c_uint64()
assert lib.pthread_threadid_np(pthread_t(0), ctypes.byref(parent_thread_id)) == 0
child_thread_id = ctypes.c_uint64()
child_registered = threading.Event()

@PTHREAD_START
def pthread_identity_start(arg):
    assert lib.pthread_threadid_np(pthread_t(0), ctypes.byref(child_thread_id)) == 0
    child_registered.set()
    return None

child_thread = pthread_t()
assert lib.pthread_create(ctypes.byref(child_thread), None, pthread_identity_start, None) == 0
initial_child_id = ctypes.c_uint64()
assert lib.pthread_threadid_np(child_thread, ctypes.byref(initial_child_id)) == 0
assert child_registered.wait(1.0), 'child pthread identity was not registered'
queried_child_id = ctypes.c_uint64()
assert lib.pthread_threadid_np(child_thread, ctypes.byref(queried_child_id)) == 0
assert lib.pthread_mach_thread_np(child_thread) == (child_thread_id.value & 0xffffffff), (
    'pthread_mach_thread_np(child) did not preserve child identity'
)
retval = ctypes.c_void_p()
assert lib.pthread_join(child_thread, ctypes.byref(retval)) == 0
assert initial_child_id.value == queried_child_id.value, (
    f'pthread_threadid_np(child) changed from {initial_child_id.value} to {queried_child_id.value}'
)
assert queried_child_id.value == child_thread_id.value, (
    f'pthread_threadid_np(child)={queried_child_id.value}, expected {child_thread_id.value}'
)
assert queried_child_id.value != parent_thread_id.value, 'child pthread_t resolved to parent thread id'

lib.__ulock_wait2.argtypes = [
    ctypes.c_uint32, ctypes.c_void_p, ctypes.c_uint64, ctypes.c_uint64, ctypes.c_uint64
]
lib.__ulock_wait2.restype = ctypes.c_int
DARWIN_UL_COMPARE_AND_WAIT = 1
DARWIN_ULF_NO_ERRNO = 0x01000000
ulock_word = ctypes.c_uint32(1)
assert lib.__ulock_wait2(
    DARWIN_UL_COMPARE_AND_WAIT | DARWIN_ULF_NO_ERRNO,
    ctypes.byref(ulock_word), 2, 0, 0
) >= 0, '__ulock_wait2 must succeed when the compared value changed'

class FloatPair(ctypes.Structure):
    _fields_ = [('first', ctypes.c_float), ('second', ctypes.c_float)]

lib.__sincosf_stret.restype = FloatPair
zero_pair = lib.__sincosf_stret(ctypes.c_float())
assert abs(zero_pair.first) < 1e-6
assert abs(zero_pair.second - 1.0) < 1e-6

lib.__exp10f.restype = ctypes.c_float
power_of_ten = lib.__exp10f(ctypes.c_float(2))
assert abs(power_of_ten - 100) < 0.01, f'exp10 result={power_of_ten}'

executable_buffer = ctypes.create_string_buffer(1024)
executable_capacity = ctypes.c_uint32(len(executable_buffer))
path_result = lib._NSGetExecutablePath(
    executable_buffer, ctypes.byref(executable_capacity)
)
assert path_result == 0, f'executable path result={path_result}'
executable_path = executable_buffer.value.decode()
assert executable_path, 'executable path was empty'
small_path = ctypes.create_string_buffer(1)
small_path_size = ctypes.c_uint32(1)
assert lib._NSGetExecutablePath(
    small_path, ctypes.byref(small_path_size)
) == -1, '_NSGetExecutablePath accepted a short buffer'
assert small_path_size.value == len(executable_path) + 1, (
    f'_NSGetExecutablePath required size={small_path_size.value}'
)

lib._NSGetArgc.restype = ctypes.POINTER(ctypes.c_int)
assert lib._NSGetArgc().contents.value == 0, 'fallback guest argc was not zero'
lib.getprogname.restype = ctypes.c_char_p
assert lib.getprogname() == b'machgate', 'fallback program name changed'

pid_path = ctypes.create_string_buffer(4096)
lib.proc_pidpath.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_uint32]
lib.proc_pidpath.restype = ctypes.c_int
pid_path_length = lib.proc_pidpath(os.getpid(), pid_path, len(pid_path))
assert pid_path_length > 0, 'proc_pidpath failed for the current process'
assert pid_path_length == len(pid_path.value), 'proc_pidpath length mismatch'

for stdio_symbol in ('__stdinp', '__stdoutp', '__stderrp'):
    assert hasattr(lib, stdio_symbol), f'missing {stdio_symbol}'

pattern4_bytes = bytes.fromhex('DE AD BE EF')
pattern4_buffer = ctypes.create_string_buffer(16)
pattern4 = (ctypes.c_uint8 * len(pattern4_bytes))(*pattern4_bytes)
lib.memset_pattern4(pattern4_buffer, pattern4, len(pattern4_buffer))
assert pattern4_buffer.raw == pattern4_bytes * 4, pattern4_buffer.raw.hex()

pattern8_buffer = ctypes.create_string_buffer(19)
pattern8 = (ctypes.c_uint8 * 8)(*range(8))
lib.memset_pattern8(pattern8_buffer, pattern8, len(pattern8_buffer))
assert pattern8_buffer.raw == bytes(range(8)) * 2 + bytes(range(3)), (
    f'memset_pattern8 failed: {pattern8_buffer.raw.hex()}'
)

pattern16_buffer = ctypes.create_string_buffer(35)
pattern16 = (ctypes.c_uint8 * 16)(*range(16))
lib.memset_pattern16(pattern16_buffer, pattern16, len(pattern16_buffer))
assert pattern16_buffer.raw == bytes(range(16)) * 2 + bytes(range(3)), (
    f'memset_pattern16 failed: {pattern16_buffer.raw.hex()}'
)

lib.strlcpy.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t]
lib.strlcpy.restype = ctypes.c_size_t
copy_buffer = ctypes.create_string_buffer(5)
assert lib.strlcpy(copy_buffer, b'abcdef', len(copy_buffer)) == 6
assert copy_buffer.value == b'abcd', f'strlcpy result={copy_buffer.value!r}'

lib.strlcat.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t]
lib.strlcat.restype = ctypes.c_size_t
append_buffer = ctypes.create_string_buffer(5)
append_buffer.value = b'ab'
assert lib.strlcat(append_buffer, b'cdef', len(append_buffer)) == 6
assert append_buffer.value == b'abcd', f'strlcat result={append_buffer.value!r}'

lib.posix_memalign.argtypes = [ctypes.POINTER(ctypes.c_void_p), ctypes.c_size_t, ctypes.c_size_t]
lib.posix_memalign.restype = ctypes.c_int
lib.memalign.argtypes = [ctypes.c_size_t, ctypes.c_size_t]
lib.memalign.restype = ctypes.c_void_p
lib.aligned_alloc.argtypes = [ctypes.c_size_t, ctypes.c_size_t]
lib.aligned_alloc.restype = ctypes.c_void_p
lib.valloc.argtypes = [ctypes.c_size_t]
lib.valloc.restype = ctypes.c_void_p
lib.machgate_shim_memalign.argtypes = [ctypes.c_size_t, ctypes.c_size_t]
lib.machgate_shim_memalign.restype = ctypes.c_void_p
lib.malloc_zone_memalign.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_size_t]
lib.malloc_zone_memalign.restype = ctypes.c_void_p
lib.malloc_default_zone.restype = ctypes.c_void_p
lib.malloc_create_zone.argtypes = [ctypes.c_size_t, ctypes.c_uint]
lib.malloc_create_zone.restype = ctypes.c_void_p
lib.malloc_get_zone_name.argtypes = [ctypes.c_void_p]
lib.malloc_get_zone_name.restype = ctypes.c_char_p
lib.malloc_zone_from_ptr.argtypes = [ctypes.c_void_p]
lib.malloc_zone_from_ptr.restype = ctypes.c_void_p
lib.malloc_zone_malloc.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
lib.malloc_zone_malloc.restype = ctypes.c_void_p
lib.malloc_zone_calloc.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_size_t]
lib.malloc_zone_calloc.restype = ctypes.c_void_p
lib.malloc_zone_realloc.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t]
lib.malloc_zone_realloc.restype = ctypes.c_void_p
lib.malloc_zone_free.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
lib.malloc_zone_size.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
lib.malloc_zone_size.restype = ctypes.c_size_t
lib.malloc_zone_valloc.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
lib.malloc_zone_valloc.restype = ctypes.c_void_p
lib.malloc_zone_claimed_address.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
lib.malloc_zone_claimed_address.restype = ctypes.c_int
lib.malloc_zone_register.argtypes = [ctypes.c_void_p]
lib.malloc_zone_unregister.argtypes = [ctypes.c_void_p]
lib.malloc_zone_free_definite_size.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t]
lib.malloc.argtypes = [ctypes.c_size_t]
lib.malloc.restype = ctypes.c_void_p
lib.calloc.argtypes = [ctypes.c_size_t, ctypes.c_size_t]
lib.calloc.restype = ctypes.c_void_p
lib.realloc.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
lib.realloc.restype = ctypes.c_void_p
lib.free.argtypes = [ctypes.c_void_p]
lib.malloc_size.argtypes = [ctypes.c_void_p]
lib.malloc_size.restype = ctypes.c_size_t
lib.malloc_good_size.argtypes = [ctypes.c_size_t]
lib.malloc_good_size.restype = ctypes.c_size_t
operator_new = getattr(lib, '_Znwm')
operator_new.argtypes = [ctypes.c_size_t]
operator_new.restype = ctypes.c_void_p
operator_new_aligned = getattr(lib, '_ZnwmSt11align_val_t')
operator_new_aligned.argtypes = [ctypes.c_size_t, ctypes.c_size_t]
operator_new_aligned.restype = ctypes.c_void_p
operator_delete_sized = getattr(lib, '_ZdlPvm')
operator_delete_sized.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
operator_delete_aligned_sized = getattr(lib, '_ZdlPvmSt11align_val_t')
operator_delete_aligned_sized.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_size_t]

for requested_size in [1, 17, 57, 73, 169, 281, 1681, 4097, 56713]:
    good_size = lib.malloc_good_size(requested_size)
    assert good_size >= requested_size, f'malloc_good_size({requested_size}) returned {good_size}'
    sized_ptr = lib.malloc(requested_size)
    assert sized_ptr, f'malloc({requested_size}) failed'
    actual_size = lib.malloc_size(sized_ptr)
    assert actual_size == good_size, (
        f'malloc_good_size({requested_size})={good_size}, '
        f'malloc_size(malloc({requested_size}))={actual_size}'
    )
    lib.free(sized_ptr)

ledger_stress_ptrs = [lib.malloc(25) for _ in range(300000)]
for ptr in ledger_stress_ptrs:
    assert ptr, 'ledger stress malloc failed'
    assert lib.malloc_size(ptr) >= 25, 'malloc_size failed during ledger stress'
for ptr in ledger_stress_ptrs:
    lib.free(ptr)

heap_ptr = lib.malloc(72)
assert heap_ptr, 'malloc failed'
assert lib.malloc_size(heap_ptr) >= 72, 'malloc_size returned too little for malloc allocation'
lib.free(heap_ptr)

calloc_ptr = lib.calloc(3, 17)
assert calloc_ptr, 'calloc failed'
assert lib.malloc_size(calloc_ptr) >= 51, 'malloc_size returned too little for calloc allocation'
realloc_ptr = lib.realloc(calloc_ptr, 123)
assert realloc_ptr, 'realloc failed'
assert lib.malloc_size(realloc_ptr) >= 123, 'malloc_size returned too little after realloc'
lib.free(realloc_ptr)

aligned = ctypes.c_void_p()
assert lib.posix_memalign(ctypes.byref(aligned), 64, 129) == 0, 'posix_memalign failed'
assert aligned.value and aligned.value % 64 == 0, f'posix_memalign returned unaligned pointer {aligned.value:#x}'
assert lib.malloc_size(aligned) >= 129, 'malloc_size returned too little for aligned allocation'
lib.free(aligned)

memalign_ptr = lib.machgate_shim_memalign(64, 72)
assert memalign_ptr and memalign_ptr % 64 == 0, f'memalign returned unaligned pointer {memalign_ptr:#x}'
assert lib.malloc_size(memalign_ptr) >= 72, 'malloc_size returned too little for memalign allocation'
lib.free(memalign_ptr)

direct_memalign_ptr = lib.memalign(64, 72)
assert direct_memalign_ptr and direct_memalign_ptr % 64 == 0, f'direct memalign returned unaligned pointer {direct_memalign_ptr:#x}'
assert lib.malloc_size(direct_memalign_ptr) >= 72, 'malloc_size returned too little for direct memalign allocation'
lib.free(direct_memalign_ptr)

direct_aligned_ptr = lib.aligned_alloc(64, 72)
assert direct_aligned_ptr and direct_aligned_ptr % 64 == 0, f'aligned_alloc returned unaligned pointer {direct_aligned_ptr:#x}'
assert lib.malloc_size(direct_aligned_ptr) >= 72, 'malloc_size returned too little for aligned_alloc allocation'
lib.free(direct_aligned_ptr)

valloc_ptr = lib.valloc(72)
assert valloc_ptr, 'valloc failed'
assert lib.malloc_size(valloc_ptr) >= 72, 'malloc_size returned too little for valloc allocation'
lib.free(valloc_ptr)

zone_memalign_ptr = lib.malloc_zone_memalign(None, 64, 72)
assert zone_memalign_ptr and zone_memalign_ptr % 64 == 0, f'malloc_zone_memalign returned unaligned pointer {zone_memalign_ptr:#x}'
assert lib.malloc_size(zone_memalign_ptr) >= 72, 'malloc_size returned too little for malloc_zone_memalign allocation'
lib.free(zone_memalign_ptr)

default_zone = lib.malloc_default_zone()
assert default_zone, 'malloc_default_zone returned NULL'
assert lib.malloc_create_zone(0, 0) == default_zone, 'malloc_create_zone did not return the default zone'
assert lib.malloc_get_zone_name(default_zone), 'malloc_get_zone_name returned NULL'

zone_ptr = lib.malloc_zone_malloc(default_zone, 72)
assert zone_ptr, 'malloc_zone_malloc failed'
assert lib.malloc_zone_size(default_zone, zone_ptr) >= 72, 'malloc_zone_size returned too little'
assert lib.malloc_zone_from_ptr(zone_ptr) == default_zone, 'malloc_zone_from_ptr did not return default zone'
assert lib.malloc_zone_claimed_address(default_zone, zone_ptr) == 1, 'malloc_zone_claimed_address did not claim live allocation'
zone_realloc_ptr = lib.malloc_zone_realloc(default_zone, zone_ptr, 144)
assert zone_realloc_ptr, 'malloc_zone_realloc failed'
assert lib.malloc_zone_size(default_zone, zone_realloc_ptr) >= 144, 'malloc_zone_size returned too little after realloc'
lib.malloc_zone_free(default_zone, zone_realloc_ptr)

zone_calloc_ptr = lib.malloc_zone_calloc(default_zone, 2, 36)
assert zone_calloc_ptr, 'malloc_zone_calloc failed'
assert lib.malloc_zone_size(default_zone, zone_calloc_ptr) >= 72, 'malloc_zone_size returned too little for zone calloc'
lib.malloc_zone_free(default_zone, zone_calloc_ptr)

zone_valloc_ptr = lib.malloc_zone_valloc(default_zone, 72)
assert zone_valloc_ptr, 'malloc_zone_valloc failed'
assert lib.malloc_zone_size(default_zone, zone_valloc_ptr) >= 72, 'malloc_zone_size returned too little for zone valloc'
lib.malloc_zone_free(default_zone, zone_valloc_ptr)

host_libc = ctypes.CDLL('libc.so.6')
host_libc.malloc.argtypes = [ctypes.c_size_t]
host_libc.malloc.restype = ctypes.c_void_p
host_libc.calloc.argtypes = [ctypes.c_size_t, ctypes.c_size_t]
host_libc.calloc.restype = ctypes.c_void_p
host_libc.realloc.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
host_libc.realloc.restype = ctypes.c_void_p
host_libc.free.argtypes = [ctypes.c_void_p]
host_libc.posix_memalign.argtypes = [ctypes.POINTER(ctypes.c_void_p), ctypes.c_size_t, ctypes.c_size_t]
host_libc.posix_memalign.restype = ctypes.c_int

shim_reuse_ptrs = [lib.malloc(72) for _ in range(64)]
for ptr in shim_reuse_ptrs:
    assert ptr, 'reuse-prime malloc failed'
    lib.free(ptr)
host_reuse_ptrs = [host_libc.malloc(72) for _ in range(64)]
reused_ptrs = set(shim_reuse_ptrs) & set(host_reuse_ptrs)
assert reused_ptrs, 'host allocator did not reuse any shim-freed addresses'
for ptr in reused_ptrs:
    assert lib.malloc_size(ptr) >= 72, f'malloc_size returned zero for reused live address {ptr:#x}'
for ptr in host_reuse_ptrs:
    host_libc.free(ptr)

for _ in range(6000):
    churn_ptr = lib.malloc(25)
    assert churn_ptr, 'churn malloc failed'
    lib.free(churn_ptr)
churn_ptr = lib.malloc(25)
assert churn_ptr, 'post-churn malloc failed'
assert lib.malloc_size(churn_ptr) >= 25, 'post-churn malloc_size returned too little'
lib.free(churn_ptr)

ZONE_SIZE = ctypes.CFUNCTYPE(ctypes.c_size_t, ctypes.c_void_p, ctypes.c_void_p)
ZONE_MALLOC = ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t)
ZONE_CALLOC = ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_size_t)
ZONE_VALLOC = ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t)
ZONE_FREE = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_void_p)
ZONE_REALLOC = ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t)
ZONE_DESTROY = ctypes.CFUNCTYPE(None, ctypes.c_void_p)
ZONE_BATCH_MALLOC = ctypes.CFUNCTYPE(ctypes.c_uint, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_void_p), ctypes.c_uint)
ZONE_BATCH_FREE = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p), ctypes.c_uint)
ZONE_MEMALIGN = ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_size_t)
ZONE_FREE_DEFINITE_SIZE = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t)
ZONE_PRESSURE_RELIEF = ctypes.CFUNCTYPE(ctypes.c_size_t, ctypes.c_void_p, ctypes.c_size_t)
ZONE_CLAIMED_ADDRESS = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p)
ZONE_MALLOC_WITH_OPTIONS = ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_size_t, ctypes.c_uint)

class CustomMallocZone(ctypes.Structure):
    _fields_ = [
        ('reserved1', ctypes.c_void_p),
        ('reserved2', ctypes.c_void_p),
        ('size', ZONE_SIZE),
        ('malloc', ZONE_MALLOC),
        ('calloc', ZONE_CALLOC),
        ('valloc', ZONE_VALLOC),
        ('free', ZONE_FREE),
        ('realloc', ZONE_REALLOC),
        ('destroy', ZONE_DESTROY),
        ('zone_name', ctypes.c_char_p),
        ('batch_malloc', ZONE_BATCH_MALLOC),
        ('batch_free', ZONE_BATCH_FREE),
        ('introspect', ctypes.c_void_p),
        ('version', ctypes.c_uint),
        ('memalign', ZONE_MEMALIGN),
        ('free_definite_size', ZONE_FREE_DEFINITE_SIZE),
        ('pressure_relief', ZONE_PRESSURE_RELIEF),
        ('claimed_address', ZONE_CLAIMED_ADDRESS),
        ('try_free_default', ZONE_FREE),
        ('malloc_with_options', ZONE_MALLOC_WITH_OPTIONS),
        ('aligned_malloc', ZONE_MEMALIGN),
    ]

custom_sizes = {}
custom_counts = {'malloc': 0, 'calloc': 0, 'realloc': 0, 'free': 0, 'free_definite_size': 0, 'memalign': 0}

@ZONE_SIZE
def custom_size(zone, ptr):
    return custom_sizes.get(ptr, 0)

@ZONE_MALLOC
def custom_malloc(zone, size):
    custom_counts['malloc'] += 1
    ptr = host_libc.malloc(size)
    if ptr:
        custom_sizes[ptr] = size
    return ptr

@ZONE_CALLOC
def custom_calloc(zone, count, size):
    custom_counts['calloc'] += 1
    total = count * size
    ptr = host_libc.calloc(count, size)
    if ptr:
        custom_sizes[ptr] = total
    return ptr

@ZONE_VALLOC
def custom_valloc(zone, size):
    return custom_malloc(zone, size)

@ZONE_FREE
def custom_free(zone, ptr):
    custom_counts['free'] += 1
    expected_size = custom_sizes.get(ptr, 0)
    if expected_size:
        observed_size = lib.malloc_size(ptr)
        assert observed_size >= expected_size, (
            f'shim forgot custom-zone allocation before free callback: '
            f'observed={observed_size}, expected>={expected_size}'
        )
    custom_sizes.pop(ptr, None)
    host_libc.free(ptr)

@ZONE_REALLOC
def custom_realloc(zone, ptr, size):
    custom_counts['realloc'] += 1
    custom_sizes.pop(ptr, None)
    new_ptr = host_libc.realloc(ptr, size)
    if new_ptr:
        custom_sizes[new_ptr] = size
    return new_ptr

@ZONE_DESTROY
def custom_destroy(zone):
    pass

@ZONE_BATCH_MALLOC
def custom_batch_malloc(zone, size, results, count):
    allocated = 0
    for index in range(count):
        ptr = custom_malloc(zone, size)
        if not ptr:
            break
        results[index] = ptr
        allocated += 1
    return allocated

@ZONE_BATCH_FREE
def custom_batch_free(zone, pointers, count):
    for index in range(count):
        custom_free(zone, pointers[index])

@ZONE_MEMALIGN
def custom_memalign(zone, alignment, size):
    custom_counts['memalign'] += 1
    raw = ctypes.c_void_p()
    if host_libc.posix_memalign(ctypes.byref(raw), alignment, size) != 0:
        return None
    custom_sizes[raw.value] = size
    return raw.value

@ZONE_FREE_DEFINITE_SIZE
def custom_free_definite_size(zone, ptr, size):
    custom_counts['free_definite_size'] += 1
    custom_free(zone, ptr)

@ZONE_PRESSURE_RELIEF
def custom_pressure_relief(zone, goal):
    return 0

@ZONE_CLAIMED_ADDRESS
def custom_claimed_address(zone, ptr):
    return 1 if ptr in custom_sizes else 0

@ZONE_MALLOC_WITH_OPTIONS
def custom_malloc_with_options(zone, alignment, size, options):
    if alignment:
        return custom_memalign(zone, alignment, size)
    return custom_malloc(zone, size)

custom_zone = CustomMallocZone(
    None, None, custom_size, custom_malloc, custom_calloc, custom_valloc,
    custom_free, custom_realloc, custom_destroy, b'custom test zone',
    custom_batch_malloc, custom_batch_free, None, 9, custom_memalign,
    custom_free_definite_size, custom_pressure_relief, custom_claimed_address,
    custom_free, custom_malloc_with_options, custom_memalign)
custom_zone_ptr = ctypes.cast(ctypes.byref(custom_zone), ctypes.c_void_p)

lib.malloc_zone_register(custom_zone_ptr)
assert lib.malloc_get_zone_name(custom_zone_ptr) == b'custom test zone', 'custom zone name not preserved'

stress_free_start = custom_counts['free']
stress_ptrs = [lib.malloc_zone_malloc(custom_zone_ptr, 72) for _ in range(20000)]
assert all(stress_ptrs), 'custom zone stress allocation failed'
for ptr in stress_ptrs:
    assert lib.malloc_zone_size(custom_zone_ptr, ptr) == 72, 'custom zone stress size was not preserved'
for ptr in stress_ptrs:
    lib.malloc_zone_free(None, ptr)
assert custom_counts['free'] == stress_free_start + len(stress_ptrs), 'custom zone ownership was lost under allocation-table pressure'

custom_malloc_start = custom_counts['malloc']
custom_ptr = lib.malloc_zone_malloc(custom_zone_ptr, 72)
assert custom_ptr and custom_counts['malloc'] == custom_malloc_start + 1, 'custom malloc zone callback was not used'
assert lib.malloc_zone_size(custom_zone_ptr, custom_ptr) == 72, 'custom zone size callback was not used'
assert lib.malloc_zone_from_ptr(custom_ptr) == custom_zone_ptr.value, 'custom zone ownership was not recorded'
assert lib.malloc_zone_claimed_address(custom_zone_ptr, custom_ptr) == 1, 'custom zone did not claim allocation'
lib.malloc_zone_free(None, custom_ptr)
assert custom_counts['free'] == stress_free_start + len(stress_ptrs) + 1, 'custom zone free callback was not used through recorded ownership'

custom_ptr = lib.malloc_zone_calloc(custom_zone_ptr, 2, 36)
assert custom_ptr and custom_counts['calloc'] == 1, 'custom calloc zone callback was not used'
custom_ptr = lib.malloc_zone_realloc(None, custom_ptr, 144)
assert custom_ptr and custom_counts['realloc'] == 1, 'custom realloc zone callback was not used through recorded ownership'
lib.malloc_zone_free_definite_size(None, custom_ptr, 144)
assert custom_counts['free_definite_size'] == 1, 'custom free_definite_size callback was not used'

custom_aligned_ptr = lib.malloc_zone_memalign(custom_zone_ptr, 64, 72)
assert custom_aligned_ptr and custom_aligned_ptr % 64 == 0, 'custom memalign zone callback failed'
assert custom_counts['memalign'] >= 1, 'custom memalign zone callback was not used'
lib.malloc_zone_unregister(custom_zone_ptr)
lib.malloc_zone_free(custom_zone_ptr, custom_aligned_ptr)

new_ptr = operator_new(72)
assert new_ptr, 'operator new failed'
assert lib.malloc_size(new_ptr) >= 72, 'malloc_size returned too little for operator new allocation'
operator_delete_sized(new_ptr, 72)

aligned_new_ptr = operator_new_aligned(72, 64)
assert aligned_new_ptr and aligned_new_ptr % 64 == 0, f'aligned operator new returned unaligned pointer {aligned_new_ptr:#x}'
assert lib.malloc_size(aligned_new_ptr) >= 72, 'malloc_size returned too little for aligned operator new allocation'
operator_delete_aligned_sized(aligned_new_ptr, 72, 64)

small_alignment_new_ptr = operator_new_aligned(129, 1)
assert small_alignment_new_ptr, 'aligned operator new rejected a sub-pointer alignment'
assert lib.malloc_size(small_alignment_new_ptr) >= 129, 'malloc_size returned too little for small-alignment operator new allocation'
operator_delete_aligned_sized(small_alignment_new_ptr, 129, 1)

# Darwin ctype masks must match Apple's <ctype.h>. Boost.Test validates
# names such as auto_start_dbg through std::isalnum, which reaches ___maskrune.
lib.__maskrune.argtypes = [ctypes.c_int, ctypes.c_ulong]
lib.__maskrune.restype = ctypes.c_int
CTYPE_A = 0x00000100
CTYPE_D = 0x00000400
CTYPE_P = 0x00002000
CTYPE_S = 0x00004000
CTYPE_U = 0x00008000
for ch in b'auto_start_dbg':
    valid = bool(lib.__maskrune(ch, CTYPE_A | CTYPE_D)) or chr(ch) in '+_?'
    assert valid, f'Boost.Test parameter character {chr(ch)!r} classified invalid'
assert lib.__maskrune(ord('Z'), CTYPE_A | CTYPE_U), 'uppercase alpha mask failed'
assert lib.__maskrune(ord('7'), CTYPE_D), 'digit mask failed'
assert lib.__maskrune(ord('_'), CTYPE_P), 'punctuation mask failed'
assert lib.__maskrune(ord(' '), CTYPE_S), 'space mask failed'

# Darwin pthread_once_t starts with a signature value that glibc's pthread_once
# does not understand. The shim must consume that state itself so C++/libc++
# one-time initialization does not fall through to host pthread semantics.
CALLBACK = ctypes.CFUNCTYPE(None)
once_state = ctypes.c_int(0x30B1BCBA)
once_calls = ctypes.c_int(0)

@CALLBACK
def once_init():
    once_calls.value += 1

lib.pthread_once.argtypes = [ctypes.c_void_p, CALLBACK]
lib.pthread_once.restype = ctypes.c_int
assert lib.pthread_once(ctypes.byref(once_state), once_init) == 0, 'pthread_once first call failed'
assert lib.pthread_once(ctypes.byref(once_state), once_init) == 0, 'pthread_once second call failed'
assert once_calls.value == 1, f'pthread_once init count={once_calls.value}, expected 1'
assert once_state.value == 2, f'pthread_once state={once_state.value:#x}, expected done state 2'

class Timespec(ctypes.Structure):
    _fields_ = [('tv_sec', ctypes.c_long), ('tv_nsec', ctypes.c_long)]

mutex = ctypes.create_string_buffer(64)
cond = ctypes.create_string_buffer(64)
deadline = Timespec(0, 0)
lib.pthread_mutex_init.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
lib.pthread_mutex_lock.argtypes = [ctypes.c_void_p]
lib.pthread_mutex_unlock.argtypes = [ctypes.c_void_p]
lib.pthread_mutex_destroy.argtypes = [ctypes.c_void_p]
lib.pthread_cond_init.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
lib.pthread_cond_timedwait.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.POINTER(Timespec)]
lib.pthread_cond_destroy.argtypes = [ctypes.c_void_p]
assert lib.pthread_mutex_init(ctypes.byref(mutex), None) == 0, 'pthread_mutex_init failed'
assert lib.pthread_cond_init(ctypes.byref(cond), None) == 0, 'pthread_cond_init failed'
assert lib.pthread_mutex_lock(ctypes.byref(mutex)) == 0, 'pthread_mutex_lock failed'
timedwait_result = lib.pthread_cond_timedwait(ctypes.byref(cond), ctypes.byref(mutex), ctypes.byref(deadline))
assert lib.pthread_mutex_unlock(ctypes.byref(mutex)) == 0, 'pthread_mutex_unlock failed'
assert lib.pthread_cond_destroy(ctypes.byref(cond)) == 0, 'pthread_cond_destroy failed'
assert lib.pthread_mutex_destroy(ctypes.byref(mutex)) == 0, 'pthread_mutex_destroy failed'
assert timedwait_result == 60, f'pthread_cond_timedwait returned {timedwait_result}, expected Darwin ETIMEDOUT 60'

lib.pthread_mutex_trylock.argtypes = [ctypes.c_void_p]
lib.pthread_cond_signal.argtypes = [ctypes.c_void_p]
lib.pthread_rwlock_rdlock.argtypes = [ctypes.c_void_p]
lib.pthread_rwlock_wrlock.argtypes = [ctypes.c_void_p]
lib.pthread_rwlock_unlock.argtypes = [ctypes.c_void_p]

recursive_mutex = ctypes.create_string_buffer(64)
ctypes.c_uint32.from_buffer(recursive_mutex).value = 0x32AAABA2
assert lib.pthread_mutex_lock(ctypes.byref(recursive_mutex)) == 0, 'static recursive mutex first lock failed'
assert lib.pthread_mutex_lock(ctypes.byref(recursive_mutex)) == 0, 'static recursive mutex second lock failed'
assert lib.pthread_mutex_unlock(ctypes.byref(recursive_mutex)) == 0, 'static recursive mutex first unlock failed'
assert lib.pthread_mutex_unlock(ctypes.byref(recursive_mutex)) == 0, 'static recursive mutex second unlock failed'
assert lib.pthread_mutex_destroy(ctypes.byref(recursive_mutex)) == 0, 'static recursive mutex destroy failed'

error_check_mutex = ctypes.create_string_buffer(64)
ctypes.c_uint32.from_buffer(error_check_mutex).value = 0x32AAABA1
assert lib.pthread_mutex_lock(ctypes.byref(error_check_mutex)) == 0, 'static error-check mutex first lock failed'
assert lib.pthread_mutex_lock(ctypes.byref(error_check_mutex)) == 11, 'static error-check mutex did not return Darwin EDEADLK'
assert lib.pthread_mutex_unlock(ctypes.byref(error_check_mutex)) == 0, 'static error-check mutex unlock failed'
assert lib.pthread_mutex_destroy(ctypes.byref(error_check_mutex)) == 0, 'static error-check mutex destroy failed'

first_fit_mutex = ctypes.create_string_buffer(64)
ctypes.c_uint32.from_buffer(first_fit_mutex).value = 0x32AAABA3
assert lib.pthread_mutex_lock(ctypes.byref(first_fit_mutex)) == 0, 'static first-fit mutex lock failed'
assert lib.pthread_mutex_unlock(ctypes.byref(first_fit_mutex)) == 0, 'static first-fit mutex unlock failed'
assert lib.pthread_mutex_destroy(ctypes.byref(first_fit_mutex)) == 0, 'static first-fit mutex destroy failed'

default_mutex = ctypes.create_string_buffer(64)
ctypes.c_uint32.from_buffer(default_mutex).value = 0x32AAABA7
assert lib.pthread_mutex_lock(ctypes.byref(default_mutex)) == 0, 'static default mutex lock failed'
assert lib.pthread_mutex_trylock(ctypes.byref(default_mutex)) == 16, 'static default mutex did not return EBUSY'
assert lib.pthread_mutex_unlock(ctypes.byref(default_mutex)) == 0, 'static default mutex unlock failed'
assert lib.pthread_mutex_destroy(ctypes.byref(default_mutex)) == 0, 'static default mutex destroy failed'

static_condition = ctypes.create_string_buffer(64)
ctypes.c_uint32.from_buffer(static_condition).value = 0x3CB0B1BB
assert lib.pthread_cond_signal(ctypes.byref(static_condition)) == 0, 'static condition signal failed'
assert lib.pthread_cond_destroy(ctypes.byref(static_condition)) == 0, 'static condition destroy failed'

provider_mutex = ctypes.create_string_buffer(64)
provider_condition = ctypes.create_string_buffer(64)
ctypes.c_uint32.from_buffer(provider_condition).value = 0x3CB0B1BB
provider_mutex_init = lib.machgate_darwin_pthread_mutex_init
provider_mutex_init.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
provider_mutex_destroy = lib.machgate_darwin_pthread_mutex_destroy
provider_mutex_destroy.argtypes = [ctypes.c_void_p]
provider_mutex_lock = lib.machgate_darwin_pthread_mutex_lock
provider_mutex_lock.argtypes = [ctypes.c_void_p]
provider_mutex_trylock = lib.machgate_darwin_pthread_mutex_trylock
provider_mutex_trylock.argtypes = [ctypes.c_void_p]
provider_mutex_unlock = lib.machgate_darwin_pthread_mutex_unlock
provider_mutex_unlock.argtypes = [ctypes.c_void_p]
provider_condition_signal = lib.machgate_darwin_pthread_cond_signal
provider_condition_signal.argtypes = [ctypes.c_void_p]
provider_condition_broadcast = lib.machgate_darwin_pthread_cond_broadcast
provider_condition_broadcast.argtypes = [ctypes.c_void_p]
provider_condition_wait = lib.machgate_darwin_pthread_cond_wait
provider_condition_wait.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
provider_condition_timedwait = lib.machgate_darwin_pthread_cond_timedwait
provider_condition_timedwait.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.POINTER(Timespec)]
provider_condition_clockwait = lib.machgate_darwin_pthread_cond_clockwait
provider_condition_clockwait.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.POINTER(Timespec)]
provider_condition_destroy = lib.machgate_darwin_pthread_cond_destroy
provider_condition_destroy.argtypes = [ctypes.c_void_p]
assert provider_mutex_init(ctypes.byref(provider_mutex), None) == 0, 'provider mutex init failed'
assert provider_mutex_lock(ctypes.byref(provider_mutex)) == 0, 'provider mutex lock failed'
assert provider_mutex_trylock(ctypes.byref(provider_mutex)) == host_errno.EBUSY, 'provider mutex trylock did not return Linux EBUSY'
assert provider_mutex_unlock(ctypes.byref(provider_mutex)) == 0, 'provider mutex unlock failed'
assert provider_mutex_lock(ctypes.byref(provider_mutex)) == 0, 'provider mutex relock failed'
provider_timedwait_result = provider_condition_timedwait(
    ctypes.byref(provider_condition), ctypes.byref(provider_mutex), ctypes.byref(deadline))
assert provider_timedwait_result == host_errno.ETIMEDOUT, 'provider timedwait did not return Linux ETIMEDOUT'
provider_clockwait_result = provider_condition_clockwait(
    ctypes.byref(provider_condition), ctypes.byref(provider_mutex), 1, ctypes.byref(deadline))
assert provider_clockwait_result == host_errno.ETIMEDOUT, 'provider clockwait did not return Linux ETIMEDOUT'
assert provider_condition_signal(ctypes.byref(provider_condition)) == 0, 'provider condition signal failed'
assert provider_condition_broadcast(ctypes.byref(provider_condition)) == 0, 'provider condition broadcast failed'
assert provider_mutex_unlock(ctypes.byref(provider_mutex)) == 0, 'provider mutex timedwait unlock failed'
assert provider_condition_destroy(ctypes.byref(provider_condition)) == 0, 'provider condition destroy failed'
assert provider_mutex_destroy(ctypes.byref(provider_mutex)) == 0, 'provider mutex destroy failed'

provider_recursive_mutex = ctypes.create_string_buffer(64)
ctypes.c_uint32.from_buffer(provider_recursive_mutex).value = 0x32AAABA2
assert provider_mutex_lock(ctypes.byref(provider_recursive_mutex)) == 0, 'provider static recursive mutex first lock failed'
assert provider_mutex_lock(ctypes.byref(provider_recursive_mutex)) == 0, 'provider static recursive mutex second lock failed'
assert provider_mutex_unlock(ctypes.byref(provider_recursive_mutex)) == 0, 'provider static recursive mutex first unlock failed'
assert provider_mutex_unlock(ctypes.byref(provider_recursive_mutex)) == 0, 'provider static recursive mutex second unlock failed'
assert provider_mutex_destroy(ctypes.byref(provider_recursive_mutex)) == 0, 'provider static recursive mutex destroy failed'

provider_error_check_mutex = ctypes.create_string_buffer(64)
ctypes.c_uint32.from_buffer(provider_error_check_mutex).value = 0x32AAABA1
assert provider_mutex_lock(ctypes.byref(provider_error_check_mutex)) == 0, 'provider static error-check mutex first lock failed'
assert provider_mutex_lock(ctypes.byref(provider_error_check_mutex)) == host_errno.EDEADLK, 'provider mutex did not preserve Linux errno'
assert provider_mutex_unlock(ctypes.byref(provider_error_check_mutex)) == 0, 'provider static error-check mutex unlock failed'
assert provider_mutex_destroy(ctypes.byref(provider_error_check_mutex)) == 0, 'provider static error-check mutex destroy failed'

provider_wait_mutex = ctypes.create_string_buffer(64)
provider_wait_condition = ctypes.create_string_buffer(64)
ctypes.c_uint32.from_buffer(provider_wait_condition).value = 0x3CB0B1BB
provider_waiter_ready = threading.Event()
provider_waiter_results = []
assert provider_mutex_init(ctypes.byref(provider_wait_mutex), None) == 0, 'provider wait mutex init failed'

def provider_waiter():
    provider_waiter_results.append(provider_mutex_lock(ctypes.byref(provider_wait_mutex)))
    provider_waiter_ready.set()
    provider_waiter_results.append(provider_condition_wait(
        ctypes.byref(provider_wait_condition), ctypes.byref(provider_wait_mutex)))
    provider_waiter_results.append(provider_mutex_unlock(ctypes.byref(provider_wait_mutex)))

provider_waiter_thread = threading.Thread(target=provider_waiter)
provider_waiter_thread.start()
assert provider_waiter_ready.wait(timeout=5), 'provider condition waiter did not start'
assert provider_mutex_lock(ctypes.byref(provider_wait_mutex)) == 0, 'provider condition signal lock failed'
assert provider_condition_signal(ctypes.byref(provider_wait_condition)) == 0, 'provider condition wake signal failed'
assert provider_mutex_unlock(ctypes.byref(provider_wait_mutex)) == 0, 'provider condition signal unlock failed'
provider_waiter_thread.join(timeout=5)
assert not provider_waiter_thread.is_alive(), 'provider condition waiter did not wake'
assert provider_waiter_results == [0, 0, 0], f'provider condition wait results={provider_waiter_results}'
assert provider_condition_destroy(ctypes.byref(provider_wait_condition)) == 0, 'provider wait condition destroy failed'
assert provider_mutex_destroy(ctypes.byref(provider_wait_mutex)) == 0, 'provider wait mutex destroy failed'

provider_concurrent_mutex = ctypes.create_string_buffer(64)
ctypes.c_uint32.from_buffer(provider_concurrent_mutex).value = 0x32AAABA7
provider_concurrent_barrier = threading.Barrier(8)
provider_concurrent_results = []

def provider_concurrent_worker():
    provider_concurrent_barrier.wait()
    result = provider_mutex_lock(ctypes.byref(provider_concurrent_mutex))
    if result == 0:
        result = provider_mutex_unlock(ctypes.byref(provider_concurrent_mutex))
    provider_concurrent_results.append(result)

provider_concurrent_threads = [threading.Thread(target=provider_concurrent_worker) for _ in range(8)]
for provider_thread in provider_concurrent_threads:
    provider_thread.start()
for provider_thread in provider_concurrent_threads:
    provider_thread.join(timeout=5)
assert all(not provider_thread.is_alive() for provider_thread in provider_concurrent_threads), 'provider concurrent first use hung'
assert provider_concurrent_results == [0] * 8, f'provider concurrent results={provider_concurrent_results}'
assert provider_mutex_destroy(ctypes.byref(provider_concurrent_mutex)) == 0, 'provider concurrent mutex destroy failed'

static_read_lock = ctypes.create_string_buffer(256)
ctypes.c_uint32.from_buffer(static_read_lock).value = 0x2DA8B3B4
assert lib.pthread_rwlock_rdlock(ctypes.byref(static_read_lock)) == 0, 'static rwlock read lock failed'
assert lib.pthread_rwlock_unlock(ctypes.byref(static_read_lock)) == 0, 'static rwlock read unlock failed'

static_write_lock = ctypes.create_string_buffer(256)
ctypes.c_uint32.from_buffer(static_write_lock).value = 0x2DA8B3B4
assert lib.pthread_rwlock_wrlock(ctypes.byref(static_write_lock)) == 0, 'static rwlock write lock failed'
assert lib.pthread_rwlock_unlock(ctypes.byref(static_write_lock)) == 0, 'static rwlock write unlock failed'

GUEST_NEW = ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.c_size_t)
GUEST_NEW_NOTHROW = ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p)
GUEST_DELETE = ctypes.CFUNCTYPE(None, ctypes.c_void_p)
guest_delete_calls = ctypes.c_int(0)
guest_new_nothrow_calls = ctypes.c_int(0)
guest_new_backing = ctypes.c_void_p()
guest_new_nothrow_backing = ctypes.c_void_p()
guest_delete_observed_size = ctypes.c_size_t(0)

@GUEST_NEW
def fake_guest_new(size):
    guest_new_backing.value = host_libc.malloc(size)
    return guest_new_backing.value

@GUEST_NEW_NOTHROW
def fake_guest_new_nothrow(size, nothrow_arg):
    guest_new_nothrow_calls.value += 1
    guest_new_nothrow_backing.value = host_libc.malloc(size)
    return guest_new_nothrow_backing.value

@GUEST_DELETE
def fake_guest_delete(ptr):
    guest_delete_calls.value += 1
    if ptr == guest_new_backing.value:
        guest_delete_observed_size.value = lib.malloc_size(ptr)

lib.machgate_shim_set_guest_cxx_allocators.argtypes = [ctypes.c_void_p] * 20
lib.machgate_shim_guest_operator_new.argtypes = [ctypes.c_size_t]
lib.machgate_shim_guest_operator_new.restype = ctypes.c_void_p
lib.machgate_shim_guest_operator_new_nothrow.argtypes = [ctypes.c_size_t, ctypes.c_void_p]
lib.machgate_shim_guest_operator_new_nothrow.restype = ctypes.c_void_p
lib.machgate_shim_guest_operator_delete.argtypes = [ctypes.c_void_p]
lib.machgate_shim_guest_operator_delete_array.argtypes = [ctypes.c_void_p]
lib.machgate_shim_set_guest_cxx_allocators(
    ctypes.cast(fake_guest_new, ctypes.c_void_p), None, None, None,
    ctypes.cast(fake_guest_delete, ctypes.c_void_p), None, None, None,
    None, None, None, None,
    ctypes.cast(fake_guest_new_nothrow, ctypes.c_void_p), None, None, None,
    None, None, None, None)

guest_bridge_ptr = lib.machgate_shim_guest_operator_new(72)
assert guest_bridge_ptr == guest_new_backing.value, 'guest C++ bridge did not return fake guest allocation'
assert lib.malloc_size(guest_bridge_ptr) >= 72, 'malloc_size returned 0 or too small for live guest allocation'
lib.machgate_shim_guest_operator_delete(guest_bridge_ptr)
assert guest_delete_calls.value == 1, 'guest deleter was not called'
assert guest_delete_observed_size.value >= 72, f'malloc_size inside guest deleter was {guest_delete_observed_size.value}, expected >= 72'
assert lib.malloc_size(guest_bridge_ptr) >= 72, 'shim retired guest ownership after forwarding to guest deleter'

guest_bridge_nothrow_ptr = lib.machgate_shim_guest_operator_new_nothrow(80, None)
assert guest_bridge_nothrow_ptr == guest_new_nothrow_backing.value, 'guest nothrow C++ bridge did not call fake guest nothrow allocation'
assert guest_new_nothrow_calls.value == 1, 'guest nothrow allocator was not called'
lib.machgate_shim_guest_operator_delete(guest_bridge_nothrow_ptr)
assert guest_delete_calls.value == 2, 'guest deleter was not called for nothrow allocation'

direct_guest_buffer = ctypes.create_string_buffer(96)
direct_guest_ptr = ctypes.addressof(direct_guest_buffer)
lib.machgate_shim_guest_operator_delete(direct_guest_ptr)
assert guest_delete_calls.value == 3, 'unknown direct guest C++ pointer did not route to guest deleter'

array_fallback_buffer = ctypes.create_string_buffer(104)
array_fallback_ptr = ctypes.addressof(array_fallback_buffer)
lib.machgate_shim_guest_operator_delete_array(array_fallback_ptr)
assert guest_delete_calls.value == 4, 'array delete did not fall back to scalar guest deleter'
lib.machgate_shim_set_guest_cxx_allocators(None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None)

print('All libsystem_shim symbol tests passed')
"
