#!/bin/bash
# Test: libsystem_shim.so can be loaded and key Apple-specific symbols resolve
set -e
cd "$(dirname "$0")/.."
MACHGATE_ROOT="${MACHGATE_ROOT:-$(pwd)}"
BUILD_DIR="${BUILD_DIR:-$MACHGATE_ROOT/build}"

[ -f "$BUILD_DIR/libsystem_shim.so" ] || { echo "libsystem_shim.so not built"; exit 1; }

# Test that the .so loads without errors
BUILD_DIR="$BUILD_DIR" LD_LIBRARY_PATH="$BUILD_DIR" python3 -c "
import ctypes, sys, os, subprocess, threading, time

build_dir = os.environ['BUILD_DIR']
lib = ctypes.CDLL(os.path.join(build_dir, 'libsystem_shim.so'))

# mach_absolute_time should return a nonzero nanosecond timestamp
lib.mach_absolute_time.restype = ctypes.c_uint64
t1 = lib.mach_absolute_time()
t2 = lib.mach_absolute_time()
assert t1 > 0, f'mach_absolute_time returned 0'
assert t2 >= t1, f'mach_absolute_time went backwards: {t2} < {t1}'

# mach_timebase_info should return {1, 1}
class TimebaseInfo(ctypes.Structure):
    _fields_ = [('numer', ctypes.c_uint32), ('denom', ctypes.c_uint32)]
info = TimebaseInfo()
lib.mach_timebase_info(ctypes.byref(info))
assert info.numer == 1, f'timebase numer={info.numer}, expected 1'
assert info.denom == 1, f'timebase denom={info.denom}, expected 1'

# __error should return a valid pointer (errno location)
lib.__error.restype = ctypes.POINTER(ctypes.c_int)
p = lib.__error()
assert p, '__error returned NULL'

lib.__cxa_guard_acquire.argtypes = [ctypes.POINTER(ctypes.c_uint64)]
lib.__cxa_guard_acquire.restype = ctypes.c_int
lib.__cxa_guard_release.argtypes = [ctypes.POINTER(ctypes.c_uint64)]
lib.__cxa_guard_abort.argtypes = [ctypes.POINTER(ctypes.c_uint64)]

guard = ctypes.c_uint64(0)
assert lib.__cxa_guard_acquire(ctypes.byref(guard)) == 1, '__cxa_guard_acquire did not request initialization'
assert ((guard.value >> 8) & 0xff) & 0x02, f'guard pending byte not set: {guard.value:#x}'
contended_result = []

def contend_guard():
    contended_result.append(lib.__cxa_guard_acquire(ctypes.byref(guard)))

thread = threading.Thread(target=contend_guard)
thread.start()
time.sleep(0.05)
assert thread.is_alive(), 'contended __cxa_guard_acquire returned before release'
lib.__cxa_guard_release(ctypes.byref(guard))
thread.join(2.0)
assert not thread.is_alive(), 'contended __cxa_guard_acquire did not wake after release'
assert contended_result == [0], f'contended __cxa_guard_acquire result={contended_result}, expected [0]'
assert guard.value & 0x01, f'guard complete byte not set after release: {guard.value:#x}'

abort_guard = ctypes.c_uint64(0)
assert lib.__cxa_guard_acquire(ctypes.byref(abort_guard)) == 1, '__cxa_guard_acquire failed for abort guard'
lib.__cxa_guard_abort(ctypes.byref(abort_guard))
assert lib.__cxa_guard_acquire(ctypes.byref(abort_guard)) == 1, '__cxa_guard_abort did not reset pending state'
lib.__cxa_guard_release(ctypes.byref(abort_guard))

recursive_script = '''
import ctypes, os
lib = ctypes.CDLL(os.path.join(os.environ['BUILD_DIR'], 'libsystem_shim.so'))
lib.__cxa_guard_acquire.argtypes = [ctypes.POINTER(ctypes.c_uint64)]
lib.__cxa_guard_acquire.restype = ctypes.c_int
guard = ctypes.c_uint64(0)
assert lib.__cxa_guard_acquire(ctypes.byref(guard)) == 1
lib.__cxa_guard_acquire(ctypes.byref(guard))
'''
recursive = subprocess.run([sys.executable, '-c', recursive_script],
                           env=os.environ.copy(), capture_output=True, text=True)
assert recursive.returncode != 0, 'same-thread recursive __cxa_guard_acquire did not abort'
assert 'recursive __cxa_guard_acquire' in recursive.stderr, recursive.stderr

# __sincosf_stret should compute sin/cos correctly
class Float2(ctypes.Structure):
    _fields_ = [('sinval', ctypes.c_float), ('cosval', ctypes.c_float)]
lib.__sincosf_stret.restype = Float2
result = lib.__sincosf_stret(ctypes.c_float(0.0))
assert abs(result.sinval - 0.0) < 1e-6, f'sin(0)={result.sinval}'
assert abs(result.cosval - 1.0) < 1e-6, f'cos(0)={result.cosval}'

# __exp10f
lib.__exp10f.restype = ctypes.c_float
val = lib.__exp10f(ctypes.c_float(2.0))
assert abs(val - 100.0) < 0.01, f'__exp10f(2)={val}, expected 100'

# _NSGetExecutablePath should return something via /proc/self/exe
buf = ctypes.create_string_buffer(1024)
bufsize = ctypes.c_uint32(1024)
ret = lib._NSGetExecutablePath(buf, ctypes.byref(bufsize))
assert ret == 0, f'_NSGetExecutablePath returned {ret}'
path = buf.value.decode()
assert len(path) > 0, '_NSGetExecutablePath returned empty path'

# stdio globals should be non-null after constructor runs
# (ctypes can't easily check FILE* globals, but we can check they exist)
# Just verify the symbols exist
assert hasattr(lib, '__stdinp'), 'missing __stdinp'
assert hasattr(lib, '__stdoutp'), 'missing __stdoutp'
assert hasattr(lib, '__stderrp'), 'missing __stderrp'

# memset_pattern4
buf = ctypes.create_string_buffer(16)
pattern = (ctypes.c_uint8 * 4)(0xDE, 0xAD, 0xBE, 0xEF)
lib.memset_pattern4(buf, pattern, 16)
assert buf.raw == b'\\xDE\\xAD\\xBE\\xEF' * 4, f'memset_pattern4 failed: {buf.raw.hex()}'

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
stress_ptrs = [lib.malloc_zone_malloc(custom_zone_ptr, 25) for _ in range(270000)]
assert all(stress_ptrs), 'custom zone stress allocation failed'
for ptr in stress_ptrs:
    assert lib.malloc_zone_size(custom_zone_ptr, ptr) == 25, 'custom zone stress size was not preserved'
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
