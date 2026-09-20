# ASan Binaries Plan

Status: plan — not implemented. Blocks: `sanitize:address` corpus builds.

## Blocker (documented in docs/NEXT_STEPS.md)

1. MachGate has no mapping for `libclang_rt.asan_osx_dynamic.dylib` — every
   ASan runtime symbol resolves to NULL, so the first instrumented memory
   access in the first static constructor calls through a NULL pointer and
   SIGSEGVs.
2. Darwin ASan shadow-memory layout differs from Linux layout, so mapping a
   Linux runtime would not be ABI-correct either.

Workaround today: build the corpus with `sanitize=none`.

## Why fix it

CI parity: the official test matrix runs ASan variants (game-engine ships
`Client/dependencies/stm/macos-arm64-asan/`, the daemon-scheduler Linux runs
used ASan builds with crash-poison guards). Our `sanitize=none` corpus cannot
detect what CI's ASan runs detect.

## Local assets (all present on this machine)

| Asset | Location | Role |
|---|---|---|
| Apple's prebuilt ASan runtime dylib | `~/.pkg_cache/apple-clang-rt-macos/19+224.*/pkg/libclang_rt.asan_osx_dynamic.dylib` (fat x86_64+arm64, 2.4MB, TLV descriptors) | the real runtime — Approach A |
| Full compiler-rt ASan sources | `extern/llvm-project/compiler-rt/lib/asan/` (submodule, `darwin-abi-compat` fork) | instrumented-code interface `___asan_*`, shadow semantics — Approach B |
| Apple clang toolchain | `~/.pkg_cache/apple-clang/` | build ASan-instrumented fixtures |
| macOS SDK | `~/.pkg_cache/xcode-sdk-macosx/26.2.*/` | fixture compilation |
| dyld TLV/init contract | `../apple-oss-reference/dyld/` | how the runtime initializes against dyld |
| xnu | `../apple-oss-reference/xnu/` | marginal — mmap semantics only |

## Approach A (spike first, est. 2-3 days): load the real runtime as a guest dylib

`libclang_rt.asan_osx_dynamic.dylib` is itself an ARM64 Mach-O — MachGate can
load it through the existing Mach-O dylib loader. The runtime then runs *inside*
the guest and provides `___asan_shadow_memory_dynamic_address` itself, so the
Darwin shadow layout is set up inside the guest's 4 GiB VA with no re-layout.

The runtime's own `mmap`/`pthread`/TLV usage flows through the syscall gate and
`libsystem_shim` (hardened: TLV destructors, TSD, ObjC messaging).

Steps:
1. Copy the arm64 slice into the container layout; add a `dylib_map` entry:
   `libclang_rt.asan_osx_dynamic = <real dylib>`.
2. Build a minimal fixture with the cached apple-clang + `-fsanitize=address`
   (one clean global write; one poisoned access) — add to `tests/fixtures/`
   following the failing-test-first rule.
3. Run under machgate; expect runtime init through dylib loader + resolver.
4. Triage known risks:
   - Shadow `MAP_FIXED` placement vs the guest 4 GiB VA and the executable at
     0x100000000 (must go through the syscall gate's mmap translation).
   - dyld interposition features the runtime may use (`DYLD_INTERPOSE`,
     malloc zone hooks) — resolver stubs may be needed.
   - Runtime TLV (has TLV descriptors) — shim support now exists.
   - Allocator interposition conflicts with the guest C++ allocator bridge.
5. Success criterion: fixture runs clean; poisoned access produces an ASan
   report through the runtime; then gate on one ASan-built corpus binary.

If A hits a hard blocker (interposition machinery we cannot emulate), fall to B.

## Approach B (fallback, est. 1-2 weeks): ASan shim from compiler-rt sources

Export the `___asan_*` interface (see `asan_interface_internal.h` in the
compiler-rt submodule) from a new `libasan_shim.so`, mapped via `dylib_map`:

- `___asan_shadow_memory_dynamic_address` — our own variable; shadow base =
  one mmap'd region of guest VA / 8 (guest spans 4 GiB → 512 MiB shadow).
- Report functions (`___asan_report_load/store*`) — print + abort.
- Interceptors (`___asan_memcpy/memmove/memset`) — checked implementations.
- Poison/unpoison, `___asan_stack_malloc_*`, `___asan_handle_no_return` —
  shadow-byte bookkeeping on our region.
- All shim code uninstrumented; shadow bytes maintained by the shim.

Approach B duplicates runtime semantics we do not own; A gets them for free.

## Sequencing

1. A-spike (this week): fixture + dylib_map + triage.
2. If clean: run one ASan-built corpus binary (available in the
   `macos-arm64-asan` dep tree builds), compare with CI expectations.
3. Else: B, scoped to the symbol set the corpus actually imports (extract
   from the binary's bind info, not the full interface).

## Notes

- ASan builds are larger, so the ±128 MiB island problem co-occurs
  symptomatically; that issue is tracked separately in
  docs/BIG_BINARY_BRANCH_RANGE_PLAN.md and is not caused by ASan.
- LSE emulation on ASan binaries follows MACHGATE_LSE_EMULATION as everywhere
  else (see `src/machgate.c`).
