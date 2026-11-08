#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"

BUILD_DIR="$BUILD_DIR" LD_LIBRARY_PATH="$BUILD_DIR" python3 - <<'PY'
import ctypes
import os

lib = ctypes.CDLL(os.path.join(os.environ['BUILD_DIR'], 'libsystem_shim.so'))
host = 0x104
task = ctypes.c_uint32.in_dll(lib, 'mach_task_self_').value

class TaskBasicInfo64(ctypes.Structure):
    _pack_ = 4
    _fields_ = [
        ('suspend_count', ctypes.c_int32),
        ('virtual_size', ctypes.c_uint64),
        ('resident_size', ctypes.c_uint64),
        ('user_seconds', ctypes.c_int32),
        ('user_microseconds', ctypes.c_int32),
        ('system_seconds', ctypes.c_int32),
        ('system_microseconds', ctypes.c_int32),
        ('policy', ctypes.c_int32),
    ]

assert ctypes.sizeof(TaskBasicInfo64) == 40
info = TaskBasicInfo64()
count = ctypes.c_uint32(ctypes.sizeof(info) // 4)
lib.task_info.argtypes = [
    ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p,
    ctypes.POINTER(ctypes.c_uint32),
]
lib.task_info.restype = ctypes.c_int
assert lib.task_info(task, 18, ctypes.byref(info), ctypes.byref(count)) == 0
assert count.value == 10
assert info.virtual_size > 0
assert info.resident_size > 0
assert info.policy == 1

sentinel = (ctypes.c_uint32 * 10)(*([0x5a5a5a5a] * 10))
short_count = ctypes.c_uint32(9)
assert lib.task_info(task, 18, sentinel, ctypes.byref(short_count)) == 5
assert list(sentinel) == [0x5a5a5a5a] * 10
assert short_count.value == 9
assert lib.task_info(task + 1, 18, ctypes.byref(info), ctypes.byref(count)) == 4
assert lib.task_info(task, 0, None, None) == 46

class TaskVMInfoRev1(ctypes.Structure):
    _pack_ = 4
    _fields_ = [
        ('virtual_size', ctypes.c_uint64),
        ('region_count', ctypes.c_int32),
        ('page_size', ctypes.c_int32),
    ] + [(name, ctypes.c_uint64) for name in (
        'resident_size', 'resident_size_peak', 'device', 'device_peak',
        'internal', 'internal_peak', 'external', 'external_peak',
        'reusable', 'reusable_peak', 'purgeable_volatile_pmap',
        'purgeable_volatile_resident', 'purgeable_volatile_virtual',
        'compressed', 'compressed_peak', 'compressed_lifetime',
        'phys_footprint',
    )]

assert ctypes.sizeof(TaskVMInfoRev1) == 152
assert TaskVMInfoRev1.phys_footprint.offset == 144
for supplied_count, expected_count in ((36, 36), (37, 36), (38, 38), (128, 38)):
    storage = (ctypes.c_uint32 * 128)(*([0x5a5a5a5a] * 128))
    count = ctypes.c_uint32(supplied_count)
    assert lib.task_info(task, 22, storage, ctypes.byref(count)) == 0
    assert count.value == expected_count
    vm_info = TaskVMInfoRev1.from_buffer(storage)
    assert vm_info.virtual_size >= vm_info.resident_size > 0
    assert vm_info.resident_size_peak >= vm_info.resident_size
    assert vm_info.page_size == os.sysconf('SC_PAGESIZE')
    if expected_count >= 38:
        assert vm_info.phys_footprint > 0
    assert list(storage)[expected_count:] == [0x5a5a5a5a] * (128 - expected_count)

for supplied_count in (0, 35):
    storage = (ctypes.c_uint32 * 128)(*([0x5a5a5a5a] * 128))
    count = ctypes.c_uint32(supplied_count)
    assert lib.task_info(task, 22, storage, ctypes.byref(count)) == 4
    assert count.value == supplied_count
    assert list(storage) == [0x5a5a5a5a] * 128

count = ctypes.c_uint32(38)
assert lib.task_info(task + 1, 22, storage, ctypes.byref(count)) == 4
assert lib.task_info(task, 22, None, ctypes.byref(count)) == 4
assert lib.task_info(task, 22, storage, None) == 4

def physical_footprint():
    vm_info = TaskVMInfoRev1()
    count = ctypes.c_uint32(38)
    assert lib.task_info(task, 22, ctypes.byref(vm_info), ctypes.byref(count)) == 0
    return vm_info.phys_footprint

import mmap
before = physical_footprint()
with mmap.mmap(-1, 8 * 1024 * 1024) as allocation:
    for offset in range(0, len(allocation), os.sysconf('SC_PAGESIZE')):
        allocation[offset] = 1
    assert physical_footprint() >= before + 4 * 1024 * 1024

class VMStatistics64(ctypes.Structure):
    _pack_ = 4
    _fields_ = [
        ('free_count', ctypes.c_uint32),
        ('active_count', ctypes.c_uint32),
        ('inactive_count', ctypes.c_uint32),
        ('wire_count', ctypes.c_uint32),
        ('zero_fill_count', ctypes.c_uint64),
        ('reactivations', ctypes.c_uint64),
        ('pageins', ctypes.c_uint64),
        ('pageouts', ctypes.c_uint64),
        ('faults', ctypes.c_uint64),
        ('cow_faults', ctypes.c_uint64),
        ('lookups', ctypes.c_uint64),
        ('hits', ctypes.c_uint64),
        ('purges', ctypes.c_uint64),
        ('purgeable_count', ctypes.c_uint32),
        ('speculative_count', ctypes.c_uint32),
        ('decompressions', ctypes.c_uint64),
        ('compressions', ctypes.c_uint64),
        ('swapins', ctypes.c_uint64),
        ('swapouts', ctypes.c_uint64),
        ('compressor_page_count', ctypes.c_uint32),
        ('throttled_count', ctypes.c_uint32),
        ('external_page_count', ctypes.c_uint32),
        ('internal_page_count', ctypes.c_uint32),
        ('total_uncompressed_pages_in_compressor', ctypes.c_uint64),
        ('swapped_count', ctypes.c_uint64),
    ]

assert ctypes.sizeof(VMStatistics64) == 160
vm64 = VMStatistics64()
count.value = ctypes.sizeof(vm64) // 4
lib.host_statistics64.argtypes = [
    ctypes.c_uint32, ctypes.c_int, ctypes.c_void_p,
    ctypes.POINTER(ctypes.c_uint32),
]
lib.host_statistics64.restype = ctypes.c_int
assert lib.host_statistics64(host, 4, ctypes.byref(vm64), ctypes.byref(count)) == 0
assert count.value == 40
assert vm64.free_count > 0

class VMStatistics(ctypes.Structure):
    _fields_ = [(name, ctypes.c_uint32) for name in [
        'free_count', 'active_count', 'inactive_count', 'wire_count',
        'zero_fill_count', 'reactivations', 'pageins', 'pageouts', 'faults',
        'cow_faults', 'lookups', 'hits', 'purgeable_count', 'purges',
        'speculative_count',
    ]]

vm32 = VMStatistics()
count.value = ctypes.sizeof(vm32) // 4
lib.host_statistics.argtypes = lib.host_statistics64.argtypes
lib.host_statistics.restype = ctypes.c_int
assert lib.host_statistics(host, 2, ctypes.byref(vm32), ctypes.byref(count)) == 0
assert count.value == 15
for statistics in (vm32, vm64):
    assert statistics.free_count > 0
    assert statistics.active_count > 0
    assert statistics.free_count + statistics.active_count == os.sysconf('SC_PHYS_PAGES')

page_size = ctypes.c_uint64(0xdeadbeef00000000)
lib.host_page_size.argtypes = [ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint64)]
lib.host_page_size.restype = ctypes.c_int
assert lib.host_page_size(host, ctypes.byref(page_size)) == 0
assert page_size.value == os.sysconf('SC_PAGE_SIZE')
assert ctypes.c_uint64.in_dll(lib, 'vm_page_size').value == page_size.value
assert ctypes.c_uint64.in_dll(lib, 'vm_kernel_page_size').value == page_size.value
assert lib.host_page_size(host, None) == 4
PY
