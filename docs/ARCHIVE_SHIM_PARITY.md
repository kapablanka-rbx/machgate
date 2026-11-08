# Archive shim export parity

The compiled archive shim's 849 function/data names are recorded in
[`tests/fixtures/archive_shim_exports.txt`](../tests/fixtures/archive_shim_exports.txt).
All 849 are now defined by MachGate libraries and resolvable through
`libmachgate_apple_compat.so`. The GLIBC version metadata symbol is excluded.
This is export parity; partial implementations and unsupported services below
are not complete macOS behavior.

[`archive-shim-parity.tsv`](archive-shim-parity.tsv) records every name and its
owning libraries. The current overlap-aware counts are:

| Library | Archive names defined | Responsibility |
| --- | ---: | --- |
| `libsystem_shim.so` | 594 | Core CLI imports, Darwin ABI conversion, shared autorelease pools, allocation ownership, filesystem and atomic operations. |
| `libmachgate_foundation.so` | 66 | Archive Objective-C classes/runtime helpers, collections, data, JSON, search paths, and the new NSString/NSProcessInfo implementation. |
| `libmachgate_icu.so` | 174 | Public ICU C interfaces backed by ICU 74.2, with Darwin entry adaptation. |
| `libmachgate_apple_compat.so` | 39 | Composition, polling FSEvents, unavailable service surfaces, and explicit unsupported Mach IPC/thread-state entries. |

Some runtime names are shared, so the counts do not sum to 849. The composite
owns an import resolver which deliberately exposes its three dependencies;
Native Linkage still rejects accidental resolvers inherited from unrelated
libraries. Foundation and ICU can also be mapped independently.

## Changes supported by behavior tests

- Blocks now copy escaped stack captures onto the heap, retain copied blocks,
  share forwarded `__block` storage, and run compiler copy/dispose helpers.
  These exports had remained stubs despite matching the archive symbol list.
  `src/shim/blocks_compat.c` owns the ABI and lifetime mechanism; a port supplies
  provider object retain/release callbacks. Darwin fixtures exercise nested
  blocks, byref storage, and captured Foundation string lifetime. Seven focused
  engine protocol/timer cases pass with flags off/on after this and the
  superclass fix; this is not a complete engine-suite result.
- `objc_msgSendSuper2` fallback no longer restarts lookup at the receiver's
  class. A guest subclass calling `[super init]` reaches the native root
  implementation without recursively reentering its own initializer. Existing
  guest superclass argument/return ABI checks remain green.
- Adjacent guest Mach-O dylibs now load through `@executable_path` and
  `@loader_path`, with validated mapping, classic/chained fixups, guest import
  binding, syscall/unwind preparation, and dependency initializer order.
  The loader uses dedicated ports/adapters, without FBX-specific patches.
- Guest TLS now has one template and host storage key per image, shared by
  that image's descriptors. Regressions cover distinct main/library values,
  initializer access, multiple descriptor spans, native thread isolation,
  alignment, teardown, and cached launches.

- Allocation ownership bookkeeping now uses the archive's 32 shards,
  multiplicative pointer hash, per-shard futex locks, and tombstone compaction.
  `src/shim/allocation_ledger.c` owns the bookkeeping; allocation and guest C++
  allocator policy remain in the existing shim. An eight-worker regression
  exercises 800,000 live allocations, cross-thread realloc/free, and churn.
  The previous global table exceeded 20 seconds; the port completed in 1.4 seconds.
- `sysctlbyname("hw.cputype")` returns the four-byte XNU `CPU_TYPE_ARM64` value
  with size-query support. This corrects CPU discovery but has not resolved the
  sampled NCNN convolution crashes.
- `getOsVersion` now receives a real NSProcessInfo singleton, a three-element
  64-bit version structure through `x8`, and a formatted NSString with a valid
  UTF-8 representation. The archive wrote three 32-bit fields into that 24-byte
  result. The compatibility version is `15.0.0`, inherited from the archive;
  it describes neither the Linux kernel nor a complete macOS environment.
- Darwin Objective-C variadic formatting reads anonymous arguments from the
  guest stack. Message dispatch preserves scalar, floating, indirect-result,
  and spilled arguments when invoking guest implementations or superclass
  methods. Legacy `shim_objc_*` entry names have functional adapters too.
- NSString tracks both UTF-8 bytes and UTF-16 code units. Conversion handles
  invalid input, Latin-1, ASCII rejection, explicit UTF-16/32 byte orders,
  embedded NULs, substring boundaries, and NSData byte lengths.
- Archive arrays/dictionaries retain stable identities while growing; current
  strings interoperate with archive collection and JSON methods. Weak slots
  zero when a current-provider string is destroyed. A shared autorelease port
  keeps pool tokens consistent across providers.
- A generic Native Provider section notification runs after import binding on
  every launch. Foundation consumes `__objc_classlist`; `objc_getClass` works
  before direct use of a guest class, including with a warm preparation cache.
  Core/domain code contains no Foundation class knowledge.
- All 174 ICU names use real implementations. Tests cover case expansion,
  normalization, IDNA, graphemes, locale-sensitive formatting, floating-point
  parameters and results, eight-argument calls, and preservation of `x18`.
  ICU's GNU C++ dependencies are isolated from the guest's LLVM libc++abi in
  a separate ELF namespace. A mixed-runtime guest exception regression tests
  both dependency orders and cached launches. The initial port lacked this
  isolation and crashed during C++ exception matching despite export parity.
- Filesystem status uses the XNU layout and translated flags/errors. CPU lookup
  writes a full `size_t`. Atomic results have the correct width and semantics.
  Registered mmap allocations are unmapped by shim `free`, instead of being
  passed to the heap allocator. The legacy CPU override affects guest queries.
- Dispatch group notifications, function callbacks, context/finalizer and data
  access are restored. Wall-clock and monotonic deadlines select their proper
  clocks. Delayed callback and semaphore timeout regressions cover the former
  clock mismatch. Initializer context helpers feed existing signal diagnostics.

## Build and select

The normal CMake build includes the three separate providers. It requires ICU
74 development files. `-DMACHGATE_BUILD_APPLE_PROVIDERS=OFF` builds the CLI core
without these optional dependencies. No provider is linked into the MachGate
domain, application, or executable.

Example map entries, using absolute paths to the build directory:

```text
libSystem.B = /path/to/build/libmachgate_apple_compat.so
CoreFoundation = /path/to/build/libmachgate_apple_compat.so
CoreServices = /path/to/build/libmachgate_apple_compat.so
Foundation = /path/to/build/libmachgate_foundation.so
libobjc = /path/to/build/libmachgate_foundation.so
libobjc.A = /path/to/build/libmachgate_foundation.so
libicucore = /path/to/build/libmachgate_icu.so
libicucore.A = /path/to/build/libmachgate_icu.so
```

The sibling engine launcher generates these selections by detecting libraries
beside its selected system shim. Alphabetic dylib versions need explicit map
entries. Existing libc++ selection is preserved. Regeneration happens on the
next launcher run; tests use isolated maps.

`MACHGATE_SYSTEM_SHIM_LIB` takes precedence over the `libSystem.B` map entry.
The launcher now carries the selected composite provider in both its map and
environment, for discovery and worker launches. Previously the environment
selected the base shim and hid composite-only exports. An actual Mach-O guest
now binds all 849 archive names in strict mode, with caching disabled, cold,
and warm; host `dlsym` coverage alone did not catch this integration bug.

## Remaining limits

- Guest dylib loading currently supports adjacent `@executable_path` and
  `@loader_path` dependencies. Complete `@rpath` search, re-export/weak
  coalescing, guest dylib `dlsym`, and runtime load/unload semantics remain
  incomplete. TLS teardown assumes the supported single guest thread and no
  live access during image release. Shared-cache TLV formats and legacy TLV
  initializer sections remain unvalidated.
- Mach IPC, thread suspension/state and exception-port operations return
  `KERN_NOT_SUPPORTED` (46) without changing output buffers. They are outside
  the core shim. Unsupported mutex fairness returns Darwin `ENOTSUP` (45).
- FSEvents preserves the archive's snapshot/diff mechanism driven by
  `FSEventStreamFlushSync`. It has no asynchronous watcher/run-loop pump,
  persistent journal, complete rename semantics, or context ownership support.
  Its tests cover create/modify/remove, stop, and invalidation.
- IOHID reports no devices/events. Run-loop compatibility exports do not
  implement an event loop. `LSOpenCFURLRef` reports unsupported (-4). No GUI,
  graphics, application launch or game-specific implementation was restored.
- Dispatch queues retain the existing synchronous execution model. Delayed
  callbacks use native timer threads; complete guest thread/TLS semantics and
  libdispatch queue scheduling are not established. Dispatch sources remain
  unsupported.
- Foundation is a selected archive method surface. Archive objects generally
  live for the process lifetime; current strings have reference counting.
  Full exceptions, canonical Unicode comparison, arbitrary format specifiers,
  complete encoding/BOM semantics, and arbitrary Objective-C categories,
  `+load`/`+initialize`, or runtime mutation are not implemented.
- Existing CoreFoundation implementations were not replaced wholesale by
  Apple's CF. Export parity does not establish full toll-free bridging or all
  Foundation/CoreFoundation selector and encoding semantics.
- ICU objects and locale data belong to ICU 74.2. Cross-version opaque objects,
  ICU C++ ABI, Apple-private APIs and guest UText callbacks are not validated.

## Verification

On November 8, the targeted xattr/environment/symbol round rechecked the actual
archive `build/libsystem_shim.so` against the fixture and current providers:
**849/849 exports, zero missing, no fixture drift**. Both strict guest binding
and host export checks passed in the final **113/113** core regression suite.
This verifies symbol coverage, not full implementation parity.

The selected optimized engine probes improved from **18/47 to 47/47 passing**:
Crashpad diagnostics 18/21 to 21/21, and Backtrace/symbol-cache tests 0/13 to
13/13 on each platform. Expanded TDS checks passed **102/102** with both flag
modes, zero crashes/timeouts and metrics disabled. These include four
native-module cases per platform beyond the controlled before/after sample.
No full-corpus improvement or complete archive behavior parity is claimed.

The initial normal build passed **85/85 MachGate test scripts**, **42/42 engine
launcher tests**, and five focused engine cases. That validation missed an ICU
dependency regression: GNU RTTI interposed the guest's LLVM C++ runtime. The
next full engine run produced widespread exception-handling crashes.

After isolating ICU's host runtime, the separate `build/icu-isolation` build
passed **86/86 MachGate test scripts**, including syscall-range coverage and
cache regressions. Ten of twelve sampled engine crash cases passed; an FBX
import case still crashed and an audio case exceeded the 30-second probe limit.
The new regression fails against the initial provider and passes after the
change. Two existing test races were also corrected: a `grep -q` pipeline could
report SIGPIPE, and memory-statistics checks compared different live snapshots.
No full engine-suite pass rate is claimed.

Subsequently, `build/archive-parity` passed **88/88 MachGate scripts** and the
engine launcher passed **46/46 tests**, including selection of the composite
provider. Three sampled cases that timed out in the active corpus passed as
isolated probes after the allocation port. These are not equivalent load
conditions and do not establish a full-suite runtime.

After the dependency/TLS batch, the normal `build/` was rebuilt and passed
**91/91 MachGate scripts**, including architecture dependency checks. Two FBX
reproducers now pass with the normal runtime; NCNN/Loom and VideoMock remain
unresolved. The binary is ready for the user's external validation run. See
`../kapa-prompts/machgate/current-run-fixes.md` for evidence and build status.

Detailed local logs and the five case names are in
`../kapa-prompts/machgate/apple-port-results.md`; the subsequent crash diagnosis
and checks are in `../kapa-prompts/machgate/icu-runtime-fix-results.md`.
Apple source revisions and
the exact files reviewed are in [Apple ABI references](APPLE_ABI_REFERENCES.md).
