# Darwin contract parity worklist

The archive is a capability inventory. Apple's published ABI and source, plus
the pinned local LLVM checkout, are the authority for observable behavior.
An exported name alone does not establish its calling convention, error
contract, ownership, initialization order, or lifetime. The supported target
remains ARM64 command-line guests on ARM64 Linux.

Latest core validation (2026-11-08): the isolated `build/ios-contract` build
passes 114/114 MachGate scripts, including regressions for ASCII whitespace
flags and TASK_VM_INFO revision/count handling. Those regressions fail on
the published v0.3.58 shim. TASK_VM_INFO now implements revisions 0/1 using
the archive's RSS-based estimates; complete XNU ledger accounting remains
unsupported. This is not completion of this worklist.

Historical baselines supplied by the user (not yet reproduced with this
checkout): macOS ARM64, about 5 minutes, 52K tests, 99.6% passing, 150–230
failures; iOS ARM64, about 7 minutes, 68.6K tests, 99.4% passing, 370–455
failures. Record artifact/configuration and registration counts with final
measurements so changes in the corpus are visible.

| Area | Contract authority | Current evidence | Remaining work |
| --- | --- | --- | --- |
| Allocation ownership | Apple libmalloc/Libc allocation interfaces; LLVM allocator callers | Size/zone/guest-C++ ownership tests; archive's sharded tracker passes concurrent growth and churn | Review allocation policy separately from the tracker, especially guest/host ownership crossings |
| Guest VM | XNU VM and BSD mapping interfaces | Existing direct/imported mapping and protection regressions | Inventory archive VM differences and verify alignment, fixed mappings, protection, errno, and teardown |
| Libc / stdio | Apple Libc and Darwin ARM64 ABI | String/variadic/file-ABI regressions and all inventoried exports | Audit actual implementations; guest code inspecting Darwin FILE layout is not covered by mapping calls to glibc |
| Character classification / task memory | Apple Libc rune tables; XNU task_info.h and task.c | All ASCII control/space/blank flags; TASK_VM_INFO revisions 0/1, invalid/count boundaries and live footprint growth | Full locale tables and XNU memory ledgers/later task-info revisions remain incomplete |
| libc++ / libc++abi | Pinned local LLVM libcxx/libcxxabi sources | Apple-ABI libc++ overlay; exception and register-preservation regressions; GNU ICU runtime isolated | Audit allocation, RTTI/unwind, exception ownership, locale, and pthread ABI boundaries |
| TLS / pthread | dyld TLV descriptors; Apple libpthread and Libc termination | Per-image templates and shared descriptor keys; native thread isolation, alignment, termination, and teardown regressions | Runtime unload with live threads, legacy TLV initializer sections, and shared-cache formats remain unvalidated |
| Blocks / Objective-C dispatch | LLVM BlocksRuntime; Apple objc4 super lookup and root initialization | Escaped/nested/byref Blocks and captured string lifetime tests; native superclass fallback regression; seven focused engine cases pass with flags off/on | Full weak/GC behavior, newer layout variants and remaining framework behaviors are unvalidated; exported names previously hid no-op Blocks implementations |
| Sockets | XNU socket headers/BSD implementations | Raw/imported sockaddr, flags, errno, and message conversion regressions | Compare archive socket surfaces and verify uncovered options, ancillary data, and output lengths |
| Guest dylibs | dyld loading/fixup/initializer behavior; LLVM MachO formats | Adjacent guest dylibs now map, rebase, bind, prepare and initialize through dedicated ports/adapters; the original FBX reproducer passes | `@rpath` search, re-exports/weak coalescing, guest dylib dynamic lookup, and full runtime load/unload semantics remain incomplete |
| CPU / commpage / instructions | XNU machine/sysctl/commpage; LLVM ARM64 encoding | Named ARM64 CPU type and 18 host-backed ARM features; commpage constants and CPU policy; cached syscall/TPIDRRO adaptation | Commpage clock/text, complete feature/MIB namespace, and archive older-host ISA emulation remain incomplete |
| Provider integration | dyld import semantics and explicit MachGate provider policy | 849 strict guest bindings; launcher uses the selected composite provider | Preserve strict/lazy/weak behavior while adding guest dependencies; do not confuse export parity with complete Foundation/framework behavior |

The guest dylib regression fixtures use small independent Mach-O libraries with data rebasing, imported calls,
dependency initialization order, raw Darwin syscalls, and cached launches.
Application orchestration should consume ports; ELF operations, path lookup,
Mach-O layouts, and mapping mechanics remain in their adapters/loader.

For each change, first reproduce the contract failure with a focused test,
then implement and rerun that test plus directly affected regressions. Keep
the full engine corpus for the end. Record unresolved behavior explicitly;
do not claim Apple's complete runtime or the archive's historical pass rate
without a matching verification run.

Pinned source references are in [APPLE_ABI_REFERENCES.md](APPLE_ABI_REFERENCES.md).
Current crash evidence and isolated-build status are in
`../kapa-prompts/machgate/current-run-fixes.md`.
