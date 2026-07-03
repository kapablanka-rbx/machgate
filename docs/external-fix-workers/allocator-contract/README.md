# Allocator Runtime Contract Audit

Purpose: close the private C++ post-`_main` allocator failure generically, not one crash at a time.

Current private failure shape:

- all `707 / 707` static constructors complete
- MachGate reaches `_main`
- guest aborts in `Memory.cpp(884) trackDeallocate`
- message: `rest=0, size=72`
- `MACHGATE_TRACE_ALLOC_SIZE=72` on `v0.3.26` did not show a matching shim allocation event before the abort

Generic contract target:

All allocation/free paths reachable from Mach-O guest code, mapped Apple-ABI `libc++.so.1`, mapped `libc++abi.so.1`, `libsystem_shim.so`, Darwin malloc zones, C++ operator new/delete, and Darwin `_libc_default_*` callback tables must enter one MachGate ownership model.

Running external audits:

- Grok: broad allocator/runtime contract audit with parallel hypotheses
- Claude: source-level codebase audit and missing tests
- Agy: independent Google-agent audit of the same contract

Reports are written next to this file and under `/tmp/machgate-agent-reports/allocator-contract/`.

## Applied result

The implementation passes fixed the generic allocator ABI gaps identified by the
audits:

- direct `memalign`, `aligned_alloc`, and `valloc` now enter the shim ledger
- flat Mach-O allocator lookup prefers mapped libSystem instead of host
  `RTLD_DEFAULT`
- deferred C++ operator binds use the same MachGate allocation hooks as normal
  binds
- Darwin malloc-zone query and callback symbols are exported explicitly
- custom Darwin malloc zones dispatch through their callbacks instead of being
  treated as the default host allocator
- the shim allocation ledger records zone ownership so `malloc_zone_from_ptr`
  and default-zone frees can route custom-zone allocations back to the owner
- the packaged Apple-ABI `libc++.so.1` and `libc++abi.so.1` use a scoped
  allocator overlay, so their internal ELF allocation and C++ operator
  new/delete paths route through `libsystem_shim.so`
- `tests/test_allocator_export_surface.sh` verifies the versioned export
  surface
- `tests/test_libsystem_shim.sh` now exercises malloc-zone allocation and query
  paths
- `tests/test_libcxx_allocator_overlay.sh` verifies the packaged libc++ and
  libc++abi do not retain direct allocator/operator references to host glibc

Verification completed:

- native shim tests pass
- native allocator export test passes
- native libc++ allocator overlay test passes
- ARM64 Docker suite passes `31 / 31`

Next external validation target: private C++ test runner with the next release
tarball.
