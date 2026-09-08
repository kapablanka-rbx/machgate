# ASan Support Plan for MachGate

## Problem

Game-engine common-test binaries built with `sanitize: address` link against
`libclang_rt.asan_osx_dynamic.dylib`. Every memory access is instrumented —
the compiled code checks a shadow byte before each load/store and calls into
the ASan runtime. Without the runtime mapped, the key symbol
`___asan_shadow_memory_dynamic_address` resolves to NULL and the first
instrumented access segfaults.

## Key insight

We do NOT need a real ASan implementation. We need a **compatibility shim**
that makes the instrumented code think ASan is running, while letting all
memory accesses pass through unchecked. If all shadow bytes are 0
(unpoisoned), every shadow check passes and the binary runs normally.

## How ASan instrumentation works

The compiler inserts this before each memory access:

```
shadow_byte = *(addr >> 3 + __asan_shadow_memory_dynamic_address)
if shadow_byte != 0:
    call __asan_report_load(addr, size)  // or __asan_loadN
```

If `shadow_byte` is always 0, the check always passes. No false positives,
no aborts, no crashes.

## Shadow memory layout

From `asan_mapping.h`:
- Apple ARM64 (macOS/iOS) uses `ASAN_SHADOW_OFFSET_DYNAMIC`
- Shadow scale = 3 (8 bytes of memory -> 1 byte of shadow)
- Formula: `shadow_addr = (addr >> 3) + shadow_offset`
- `__asan_shadow_memory_dynamic_address` holds the shadow offset

For MachGate:
1. mmap a large zeroed region for shadow memory at a fixed Linux address
2. Set `__asan_shadow_memory_dynamic_address` to that region's base
3. All shadow bytes are 0 (unpoisoned) — every check passes

## Symbols to implement

From `asan_interface_internal.h`, the instrumented code calls these.
Grouped by priority:

### Phase 1: Boot-critical (without these, the binary crashes immediately)

- `__asan_shadow_memory_dynamic_address` — global uptr, set to shadow base
- `__asan_option_detect_stack_use_after_return` — global int, set to 0
- `__asan_init()` — one-time init, no-op (shadow already mapped)
- `__asan_version_mismatch_check()` — no-op
- `__asan_before_dynamic_init(const char *module_name)` — no-op
- `__asan_after_dynamic_init()` — no-op
- `__asan_register_globals(__asan_global *globals, uptr n)` — no-op
- `__asan_unregister_globals(__asan_global *globals, uptr n)` — no-op
- `__asan_register_image_globals(uptr *flag)` — no-op
- `__asan_unregister_image_globals(uptr *flag)` — no-op

### Phase 2: Memory access checks (called by instrumented code)

- `__asan_load1` through `__asan_load16` — check shadow, no-op if 0
- `__asan_store1` through `__asan_store16` — check shadow, no-op if 0
- `__asan_loadN(uptr p, uptr size)` — check shadow, no-op if 0
- `__asan_storeN(uptr p, uptr size)` — check shadow, no-op if 0
- `__asan_load1_noabort` through `__asan_load16_noabort` — same, never abort
- `__asan_store1_noabort` through `__asan_store16_noabort` — same
- `__asan_loadN_noabort` / `__asan_storeN_noabort` — same

### Phase 3: Shadow manipulation (called by instrumented code)

- `__asan_set_shadow_00` through `__asan_set_shadow_07` — memset shadow region
- `__asan_set_shadow_f1`, `f2`, `f3`, `f5`, `f8` — memset shadow region
- `__asan_poison_stack_memory(uptr addr, uptr size)` — set shadow to 0xf5
- `__asan_unpoison_stack_memory(uptr addr, uptr size)` — set shadow to 0
- `__asan_poison_memory_region(void *addr, uptr size)` — set shadow
- `__asan_unpoison_memory_region(void *addr, uptr size)` — set shadow to 0
- `__asan_alloca_poison(uptr addr, uptr size)` — poison alloca redzone
- `__asan_allocas_unpoison(uptr top, uptr bottom)` — unpoison alloca area

### Phase 4: C++ support

- `__asan_poison_cxx_array_cookie(uptr p)` — no-op
- `__asan_load_cxx_array_cookie(uptr *p)` — return *p
- `__asan_poison_intra_object_redzone(uptr p, uptr size)` — no-op
- `__asan_unpoison_intra_object_redzone(uptr p, uptr size)` — no-op
- `__asan_handle_no_return()` — no-op
- `__asan_handle_vfork(void *sp)` — no-op

### Phase 5: Memory functions (intercepted by ASan)

- `__asan_memcpy(void *dst, const void *src, uptr size)` — call real memcpy
- `__asan_memset(void *s, int c, uptr n)` — call real memset
- `__asan_memmove(void *dest, const void *src, uptr n)` — call real memmove

### Phase 6: Reporting and queries (can stub minimally)

- `__asan_report_error(...)` — log and continue (or abort)
- `__asan_address_is_poisoned(void *addr)` — return 0
- `__asan_region_is_poisoned(uptr beg, uptr size)` — return 0
- `__asan_report_present()` — return 0
- `__asan_get_report_*()` — return 0
- `__asan_describe_address(uptr addr)` — no-op
- `__asan_locate_address(...)` — return NULL
- `__asan_get_alloc_stack(...)` — return 0
- `__asan_get_free_stack(...)` — return 0
- `__asan_get_shadow_mapping(uptr *scale, uptr *offset)` — set scale=3, offset=shadow base
- `__asan_set_death_callback(...)` — no-op
- `__asan_set_error_report_callback(...)` — no-op
- `__asan_on_error()` — no-op
- `__asan_print_accumulated_stats()` — no-op
- `__asan_default_options()` — return NULL
- `__asan_default_suppressions()` — return NULL
- `__asan_update_allocation_context(void *addr)` — return 0

### Phase 7: Experimental ASan (may not be called)

- `__asan_exp_load1` through `__asan_exp_load16` — same as load
- `__asan_exp_store1` through `__asan_exp_store16` — same as store
- `__asan_exp_loadN` / `__asan_exp_storeN` — same

## Total symbol count

~120 functions + 2 global variables.

## Implementation plan

### Step 1: Create `src/asan/asan_shim.c`

A single C file implementing all ~120 functions as no-ops or minimal stubs.
Shadow memory is a large mmap'd zeroed region. All checks pass because all
shadow bytes are 0.

### Step 2: Set up shadow memory in MachGate loader

Before guest execution, in `machgate.c`:
1. `mmap` a large zeroed region (e.g., 1 GB) at a fixed address
2. Export `__asan_shadow_memory_dynamic_address` pointing to it
3. Export `__asan_option_detect_stack_use_after_return = 0`

### Step 3: Map `libclang_rt.asan_osx_dynamic.dylib` to the shim

In `dylib_map.conf`, add:
```
libclang_rt.asan_osx_dynamic = /path/to/libasan_shim.so
```

Or compile the shim into `libsystem_shim.so` so it's already loaded.

### Step 4: Build and test

1. Build game-engine common-tests WITH `sanitize: address`
2. Run through MachGate
3. Verify no crashes from ASan symbol resolution

### Step 5: Verify shadow checks pass

The instrumented code does:
```c
shadow = *(addr >> 3 + shadow_base)
if (shadow != 0) report_error()
```

Since our shadow region is zeroed, `shadow` is always 0, and the check
always passes. No false positives.

## What this does NOT do

- No actual memory error detection (ASan is a no-op shim)
- No stack-use-after-return detection
- No use-after-scope detection
- No global variable redzone checking
- No leak detection

This is a **compatibility layer**, not a sanitizer. The goal is to run
ASan-instrumented binaries through MachGate without crashing, not to
detect memory errors on Linux.

## Alternative: real ASan on Linux

A future enhancement could implement real shadow memory poisoning:
- Poison stack redzones on function entry/exit
- Poison global redzones on registration
- Poison heap redzones on malloc/free
- Report errors via the MachGate signal handler

This would require intercepting malloc/free, tracking stack frames, and
implementing the full ASan allocator. Much more work, but possible.
