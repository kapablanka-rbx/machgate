#!/bin/bash
set -euo pipefail

ROOT="${MACHGATE_ROOT:-$(cd "$(dirname "$0")/.." && pwd)}"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
SHIM="$BUILD_DIR/libsystem_shim.so"

[ -f "$SHIM" ] || { echo "missing libsystem_shim.so: $SHIM" >&2; exit 1; }

require_symbol()
{
    local symbol="$1"
    if ! readelf --dyn-syms --wide "$SHIM" | awk -v symbol="${symbol}@@GLIBC_2.17" '$NF == symbol { found = 1 } END { exit !found }'; then
        echo "missing versioned allocator symbol: $symbol@@GLIBC_2.17" >&2
        exit 1
    fi
}

for symbol in \
    malloc free calloc realloc posix_memalign memalign aligned_alloc valloc \
    malloc_size malloc_good_size malloc_default_zone malloc_create_zone \
    malloc_destroy_zone malloc_set_zone_name malloc_get_zone_name \
    malloc_zone_from_ptr malloc_get_all_zones malloc_num_zones malloc_zones \
    malloc_zone_malloc malloc_zone_calloc malloc_zone_valloc malloc_zone_free \
    malloc_zone_realloc malloc_zone_destroy malloc_zone_size \
    malloc_zone_batch_malloc malloc_zone_batch_free malloc_zone_memalign \
    malloc_zone_free_definite_size malloc_zone_pressure_relief \
    malloc_zone_claimed_address malloc_zone_malloc_with_options \
    malloc_zone_register malloc_zone_unregister \
    machgate_shim_set_guest_cxx_allocators \
    machgate_shim_guest_operator_new machgate_shim_guest_operator_new_array \
    machgate_shim_guest_operator_new_aligned \
    machgate_shim_guest_operator_new_array_aligned \
    machgate_shim_guest_operator_new_nothrow \
    machgate_shim_guest_operator_new_array_nothrow \
    machgate_shim_guest_operator_new_aligned_nothrow \
    machgate_shim_guest_operator_new_array_aligned_nothrow \
    machgate_shim_guest_operator_delete machgate_shim_guest_operator_delete_array \
    machgate_shim_guest_operator_delete_sized \
    machgate_shim_guest_operator_delete_array_sized \
    machgate_shim_guest_operator_delete_aligned \
    machgate_shim_guest_operator_delete_array_aligned \
    machgate_shim_guest_operator_delete_sized_aligned \
    machgate_shim_guest_operator_delete_array_sized_aligned \
    machgate_shim_guest_operator_delete_nothrow \
    machgate_shim_guest_operator_delete_array_nothrow \
    machgate_shim_guest_operator_delete_aligned_nothrow \
    machgate_shim_guest_operator_delete_array_aligned_nothrow \
    _Znwm _Znam _ZnwmRKSt9nothrow_t _ZnamRKSt9nothrow_t \
    _ZnwmSt11align_val_t _ZnamSt11align_val_t \
    _ZnwmSt11align_val_tRKSt9nothrow_t _ZnamSt11align_val_tRKSt9nothrow_t \
    _ZdlPv _ZdaPv _ZdlPvm _ZdaPvm \
    _ZdlPvRKSt9nothrow_t _ZdaPvRKSt9nothrow_t \
    _ZdlPvSt11align_val_t _ZdaPvSt11align_val_t \
    _ZdlPvmSt11align_val_t _ZdaPvmSt11align_val_t \
    _ZdlPvSt11align_val_tRKSt9nothrow_t _ZdaPvSt11align_val_tRKSt9nothrow_t
do
    require_symbol "$symbol"
done
