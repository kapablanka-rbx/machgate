# MachGate domain model

## Purpose

MachGate is an ARM64 guest-execution system. It prepares an ARM64 Mach-O
command-line executable, mediates the guest Darwin ABI at the boundary of an
ARM64 Linux host, and transfers control to the prepared Guest Process.

The core bounded context is **Darwin ABI Translation**. It preserves the
guest-visible Darwin contract while selecting and executing explicit Linux host
operations. Executable Imaging, Native Linkage, Guest Execution, Guest VM,
Compatibility Runtime, and Process Continuity are supporting contexts around
that core. **Darwin ABI mediation** is the broader capability delivered by
these contexts together, not a second name for the core context.

This document is the source of the project's ubiquitous language. Source names,
tests, diagnostics, and design discussions should use these terms consistently.
The model is derived from public Mach-O, Darwin, ARM64, and Linux contracts plus
MachGate behavior tests.

## Ubiquitous language

| Term | Definition | Not this |
| --- | --- | --- |
| Guest | The ARM64 Mach-O program whose observable contract is Darwin. | A native Linux process or library. |
| Host | The ARM64 Linux process and kernel executing MachGate. | The guest ABI. |
| Guest Executable | The input file supplied by the user. It may be thin or contain an ARM64 fat slice. | A recursively loaded application bundle. |
| Slice | The selected ARM64 member of a fat Guest Executable, or the complete thin file. | An unchecked file range. |
| Guest Image | An opaque validated executable or dependency dylib mapped into host memory, with a published read-only identity and validated adapter view. | Raw file bytes, a public mutable record, or a partially validated mapping. |
| Guest Address Range | A half-open guest-visible interval `[start, end)`. | An unchecked pointer and length pair. |
| Image Preparation | The use case that validates, maps, links, patches, and otherwise makes a Guest Image safe to enter. | Guest execution itself. |
| Preparation Artifact | Reusable, derived output of Image Preparation for a particular Guest Executable slice and MachGate runtime revision. Persistence is an adapter concern; every process rebuilds its live provider pointers and Branch Islands. | A live Guest Process snapshot or a rewritten original executable. |
| Guest Process | A prepared main Guest Image and its dependency images together with guest arguments, environment, executable path, stack, and lifecycle state. | A host child process. |
| Entry Point | The validated address selected from `LC_MAIN` or a supported `LC_UNIXTHREAD`. | An arbitrary symbol. |
| Native Provider | An explicitly selected host shared object that satisfies a supported guest import surface. | General dyld discovery. |
| Guest Dependency | A validated ARM64 Mach-O dylib loaded through a supported install-name path and retained for the Guest Process. | An ambient host ELF library. |
| Native Dependency | A guest load-command declaration naming an imported library. | A host library selected by coincidence. |
| Provider Decision | The explicit `Unmapped`, `Map`, `Skip`, `Stub`, or `Partial` policy for one Native Dependency. `Partial` selects a provider but fails closed for imports outside its declared surface. | A nullable handle that collapses distinct policies. |
| Import Binding | A validated guest pointer slot plus provider ordinal, symbol, and addend. Applying it writes a resolved address or the typed fail-closed value selected by its Provider Decision. | Loading arbitrary Mach-O dependencies. |
| Guest Definition | A real non-stub external symbol definition in the Guest Image. | An undefined import or symbol stub. |
| Compatibility Runtime | The host-native symbols that implement the supported imported Darwin runtime surface. | A complete Apple framework implementation. |
| Guest Variadic Argument List | The Darwin ARM64 stack area or character-pointer `va_list` containing anonymous arguments. It must be translated before a Linux variadic implementation consumes it. | A Linux AAPCS64 `va_list` with interchangeable layout. |
| Process-local Guest Semaphore | A Compatibility Runtime synchronization identity backed by host process state and valid only inside one MachGate process. | A Mach port, Mach IPC endpoint, or cross-process semaphore. |
| Patch Site | An exact guest instruction or function entry selected for replacement. | A byte-pattern match in data. |
| Syscall Site | A Patch Site containing aligned `svc #0x80` inside a declared executable instruction section. | The same byte pattern in data. |
| Branch Island | Nearby generated ARM64 code that preserves guest state and enters the Syscall Gateway. | A guest function or a link-register call target. |
| Syscall Frame | The saved guest `x0`-`x18`, `x30`, condition flags, and continuation address at a Syscall Site. | Host thread state in general. |
| Guest Syscall | A request identified by the Darwin syscall number in guest `x16`, with arguments beginning at `x0` as defined by that syscall; the current frame supplies argument slots through `x7`. | A Linux syscall number with similar spelling. |
| Host Operation | The Linux action selected after explicit ABI translation. | Direct forwarding of guest values. |
| Syscall Outcome | Either a guest-visible success value with carry clear or a Darwin errno with carry set. | A raw negative Linux errno. |
| ABI Translation | Explicit conversion of numbers, flags, structures, results, and errors between guest and host contracts. | Casting two similarly named structures. |
| Guest Address Space | The mapped memory mediated for the Guest Process by Guest VM operations. | All mappings in the host process or a claim that MachGate keeps a complete shadow page table. |
| Guest Mapping | One region created or removed through the Guest VM boundary, with guest-visible range, protection, and mapping semantics. | An arbitrary host allocation. |
| Process Replacement | Replacing the current Guest Process with another Mach-O command-line executable through MachGate. | Native `execve` of Mach-O bytes. |
| External Probe | A pinned public ARM64 Mach-O CLI binary executed as compatibility evidence. | A unit test or a claim of complete compatibility. |

## Bounded contexts

### Executable Imaging

Executable Imaging owns thin/fat selection, Mach-O validation, segment mapping,
protection, entry-point selection, and creation of the Guest Image.

Its aggregate is the **Guest Image**. A Guest Image exists only after the whole
input image has been validated and mapped successfully. Failure leaves no
partially mapped aggregate behind.

Executable Imaging owns the lifetime of the optional preparation-artifact cache.
The application signals completion of Image Preparation through its execution
port. Persistence, file identities, cache directories, checksums, and atomic
publication remain in adapters. A missing, invalid, or unwritable cache cannot
change the guest contract. Native Linkage, unwind registration, and Darwin ABI
Translation each interpret their own derived artifacts and restore process-local
addresses before use.

### Native Linkage

Native Linkage owns supported import parsing, dependency-to-provider policy,
symbol resolution, bind-location validation, and explicit function
redirection. It consumes a Guest Image and may modify only validated locations
inside it.

Native Linkage is a supporting context. It resolves selected native providers
and supported adjacent Guest Dependencies. It does not implement the complete
dyld search, re-export, or runtime `dlopen` contract.

Guest Dependencies use the same validated imaging and preparation boundaries
as the main executable. Their mapping, path lookup, dependency graph, and
process lifetime belong to adapters. Application code coordinates preparation
and dependency-before-dependent initialization through ports. Dependency
images require `MH_DYLIB`; the main image still requires `MH_EXECUTE`.

After import binding it can publish validated mapped sections through the
Native Provider image port. A selected provider interprets its own metadata;
for example, the separate Foundation provider registers guest classes. This
notification runs on every launch, including preparation-cache hits. Provider
composition uses an explicitly owned import resolver to expose declared
dependencies without weakening resolver-ownership checks.

Flat and weak lookup first honor a Guest Definition, then search explicitly
selected dependency providers in load-command order, and only then consult the
ambient host namespace. This keeps a host symbol with the same spelling from
bypassing the selected Darwin compatibility provider.

`Unmapped` rejects required non-lazy imports. `Map` selects one native provider
and requires its non-lazy data surface. A missing `Map` import may receive the
fail-on-call trap only when the symbol is proven to have an indirect-symbol
entry in a real `S_SYMBOL_STUBS` section. `Skip` writes an unavailable null
value. `Stub` writes a trap that terminates if called. `Partial` selects a
provider for its supported symbols, writes null for a missing ordinary data
binding, and writes the fail-on-call trap for a missing function binding in a
lazy-pointer section or with a proven symbol-stub entry. Section metadata, not
bind-stream placement alone, distinguishes function imports because producers
can encode lazy pointers or a stub's GOT target as ordinary or chained binds. A
`Partial` decision is reserved for a command-line provider whose
supported and deliberately unavailable surfaces share one dependency; it does
not authorize a compatibility implementation of the unavailable API.

### Darwin ABI Translation

Darwin ABI Translation owns Syscall Site discovery, Branch Island generation,
Syscall Frame preservation, Guest Syscall dispatch, structure and flag
conversion, Host Operation selection, and Syscall Outcome encoding.

The Guest Syscall is the request value. The Syscall Outcome is the result value.
Linux calls and constants are infrastructure behind the translation boundary;
they are not part of the guest-facing domain model.

### Guest Execution

Guest Execution owns the **Guest Process** lifecycle. It coordinates Image
Preparation, guarded stack allocation, guest argument state, static initializer
ordering, working-directory selection, and final entry.

This is the application context. Its target boundary orchestrates the other
contexts without parsing Mach-O structures or translating individual syscalls.

### Compatibility Runtime

Compatibility Runtime owns the explicitly supported imported runtime symbols
needed by command-line guests. Its capabilities are organized into focused
modules while retaining one versioned public export surface. It adapts guest
contracts to Linux facilities and may call the same lower-level translation
services as the syscall path when the contracts are identical.

Variadic adapters translate a Guest Variadic Argument List into explicit host
argument-list state, including stdio lists and stack arguments for translated
file-control requests. Task-policy requests are validated and fail closed when
the host cannot provide the requested Darwin contract. A normal return from
guest entry flushes host stdio before preserving the guest status in raw process
termination; `_exit` remains unflushed. This is not a claim of complete Darwin
`atexit` lifecycle semantics.

It is an anti-corruption layer, not part of the core domain and not a promise of
GUI, framework, or general macOS compatibility.

Optional Foundation, ICU and composite Apple providers live under `providers/`.
They depend on narrow runtime ports and the native shim, while the domain and
application have no dependency on them. The shared provider-lifetime port owns
deferred releases; providers keep ownership of object representations. See the
[archive export inventory](ARCHIVE_SHIM_PARITY.md) for tested contracts and
explicitly unavailable services.

### Guest VM

Guest VM owns mediation of Guest Mapping operations shared by direct guest VM
syscalls and deliberately interposed host runtime mappings. It publishes a
narrow mapping interface to Darwin ABI Translation and the VM host adapter.
Where Darwin and Linux unmapping semantics differ, it keeps an explicit bounded
registry of sub-page anonymous mappings. Registry exhaustion is a mapping
failure, never a silently untracked success. MachGate does not claim a complete
shadow page table for every host mapping.

### Process Continuity

Process Continuity owns Process Replacement. It recognizes supported guest
targets, preserves the required runtime environment, constructs the new
MachGate invocation, and reports Darwin errors when replacement fails.

## Context map

```text
CLI adapter
    |
    v
Guest Execution ----------------------> Executable Imaging
    |                                         |
    |                                         v
    +----------------------------------> Guest Image
    |
    +----------------------------------> Native Linkage
    |                                         |
    |                                         v
    |                               selected Native Provider
    |                                         |
    |                                         v
    |                               Compatibility Runtime
    |
    +----------------------------------> Darwin ABI Translation
    |                                         |
    |                                         v
    |                                      Guest VM
    |
    +----------------------------------> guest entry

Darwin ABI Translation -----> Process Continuity -----> Linux host operations
Compatibility Runtime ------> Process Continuity
Compatibility Runtime ------> Guest VM and Linux host facilities
```

Executable Imaging publishes the Guest Image language. Native Linkage, Guest
Execution, and Darwin ABI Translation conform to that model. Compatibility
Runtime is kept behind an anti-corruption boundary so imported Darwin names do
not shape the core model. Process Continuity is consumed by both direct syscall
translation and the compatible imported `execve` surface.

## Contract invariants

1. A main Guest Image requires a validated ARM64 `MH_EXECUTE` slice; a
   Guest Dependency requires a validated ARM64 `MH_DYLIB` slice.
2. Every file range, command range, mapped interval, and address calculation is
   validated before use.
3. Image Preparation completes before any guest initializer or entry point is
   called.
4. Every Syscall Site is instruction-aligned and belongs to a declared
   executable instruction section.
5. Every direct branch reaches its Branch Island without changing guest `x30`.
6. A Syscall Frame preserves all promised guest registers and condition flags.
7. Every Guest Syscall either produces a translated Syscall Outcome or Darwin
   `ENOSYS`; a raw host ABI error never crosses the boundary.
8. Native Linkage writes only to validated bind locations and uses only an
   explicitly selected Native Provider or validated Guest Dependency.
9. Unsupported GUI, framework, Mach IPC, and child-process behavior must not be
   disguised as successful domain behavior.
10. Successful Process Replacement does not return; failure returns a Darwin
    error and preserves required loader state.
11. A normal return from guest entry flushes host stdio and terminates with the
    returned guest status; immediate `_exit` does not flush buffered stdio.

The initial Guest Process has one guarded main stack and one guest entry thread.
Compatibility Runtime currently exposes selected host-backed pthread behavior,
but that does not claim complete Darwin thread lifecycle or synchronization
semantics.

## Current implementation alignment

| Area | Current state | Target boundary or remaining gap |
| --- | --- | --- |
| Guest Image language | `src/domain/guest_image.h` publishes an opaque `machgate_guest_image`. Executable Imaging alone constructs and destroys it; adapters consume explicit scalar queries and a validated integration view. | Replace uses of the validated integration view with narrower context queries when a context needs less than the complete validated header. |
| Guest Process orchestration | `src/application/guest_session.c` owns Prepare and Run Guest orchestration through `ports/guest_execution.h`; `machgate.c` is the CLI adapter. Raw initializer and TLV inspection are outside the application layer. `machgate_guest_process` enforces lifecycle stages and publishes one versioned runtime-state contract to the Compatibility Runtime. | Keep new orchestration dependencies behind ports and add lifecycle stages only when behavior requires them. |
| Compatibility Runtime capabilities | Assertion, basic runtime, errno, guest process, host facts, locale/string, time, unwind, sysctl, filesystem metadata, file ABI, file operations, dispatch, process-local guest synchronization, fail-closed notification, CoreFoundation additions, Security additions, pthread synchronization, resource-limit imports, provider guest import resolver ownership, thread-local/TLV behavior, variadic ABI translation, task-policy rejection, and process termination are focused compiled modules. | Continue shrinking `libsystem_shim.c` while preserving the tested in-scope export surface. |
| Thread-local image storage | `adapters/guest_thread_local.c` validates Mach-O templates and descriptors. The native thread-local image port registers one storage key per image; `shim/thread_local_compat.c` owns host keys, allocation, alignment, and teardown. | Application and domain code contain no Mach-O TLS layout or pthread key mechanics. Dynamic unload with live guest threads is not supported. |
| Allocation ownership | `src/shim/allocation_ledger.c` owns process-local pointer records, sizes, zones, and guest C++ ownership flags. Records are partitioned into 32 independently locked tables; Linux futex and mapping details stay inside the Compatibility Runtime. | Allocation/free routing remains separate from the ledger. The domain and application layers do not depend on allocator table structure or locking. |
| Dynamic unwind registration | `domain/dwarf_unwind.c` decodes a single DWARF frame's code interval without OS operations. `ports/native_unwind.h` exposes guest registration and removal; `adapters/dynamic_unwind.c` owns registration lifetime and Linux unwind-index translation. | Darwin registers one FDE. Host GCC section registration remains separate; guest metadata is never rewritten or scanned for a terminating record. |
| Target-excluded runtime surface | Fake-success Mach controls, FSEvents, CFRunLoop execution, speculative ICU, and other unused framework shims have been removed from the implementation and export list. The fail-closed `_dispatch_source_type_mach_recv` availability marker remains exported but always refuses source creation, like the retained default-mode string. Process-local guest semaphores do not provide ports or Mach IPC. | Keep unsupported AppKit, IOHID, GUI, and Mach IPC imports behind fail-closed Provider Decisions instead of adding success implementations. |
| Pthread synchronization | `pthread_sync_compat.c` owns mutex, condition-variable, and read-write-lock adapters, including guest static-initializer signatures and Darwin errno results. | Pthread identity, keys, once, attributes, lifecycle, and extensions remain in `libsystem_shim.c` and require later capability extraction. |
| Guest VM | Syscall and interposed mapping paths share implementation machinery. The bounded sub-page mapping registry fails explicitly with `ENOMEM` when full. | Keep the model scoped to mediated operations and explicitly registered semantic differences; do not imply a complete shadow address-space ledger. |
| Native provider policy | Provider routing represents `Unmapped`, `Map`, `Skip`, `Stub`, and evidence-limited `Partial` as distinct decisions; versioned install names use exact policy before a versionless fallback. Flat and weak lookup searches Guest Definitions and selected dependency providers before the ambient host namespace. Missing Partial-provider data binds null. Lazy-pointer targets and symbols proven by a real `S_SYMBOL_STUBS` indirect entry bind to a fail-on-call trap, including regular-stream and chained encodings; strict mapped data-only imports remain errors. | Keep compiler-runtime fallback limited to individually reviewed public compiler ABI symbols. |
| Child processes and guest threads | Host-backed spawn, wait, `vfork`, and pthread lifecycle behavior remains because current CLI probes and behavior tests require those symbols. | Reconcile this provisional compatibility behavior with the initial-target policy before calling the single-thread/no-child constraint enforced. |

## Layers and dependency rule

| Layer | Responsibility | Current code | Target dependency |
| --- | --- | --- | --- |
| Domain | Guest Image, Guest Process, Process Launch Context, Guest Syscall, Syscall Outcome, Provider Decision, and invariants | `src/domain/` contains opaque identity, value types, lifecycle transitions, and outcome encoding. | Depends only on C value types. |
| Application | Prepare and run a Guest Process in the required order. | `src/application/guest_session.c` owns startup orchestration and is compiled as `machgate_application`. | Depends on domain types and ports. |
| Ports | Operations required by application and domain policies. | `src/ports/guest_execution.h` is the application port; `guest_image_view.h` is the validated adapter integration view. | Describe application capabilities without Linux or raw Mach-O details. |
| Adapters | Mach-O parsing, ARM64 emission, Linux syscalls, `dlopen`, CLI, and native shim. | `adapters/` (guest execution and unwinding), `loader.c`, `native_imports.c`, `trampoline.c`, `syscall/`, `vm_interpose*`, and `shim/`. | May depend inward; core layers do not depend outward. |

The extraction rule is conservative: rename implementation-shaped shared data
to domain language, move orchestration out of the CLI adapter, and introduce
narrow context interfaces before moving large files. Behavior changes require
tests and are not mixed with structural extraction.

## Application workflow

The **Prepare and Run Guest** use case is:

1. create a Guest Image from the Guest Executable;
2. bind supported imports through Native Linkage;
3. register guest unwind surfaces;
4. apply explicitly configured native redirections;
5. patch every supported Syscall Site;
6. prepare the guest runtime image, including TLV storage extents;
7. allocate the guarded guest stack;
8. establish Guest Process arguments, environment, and executable identity;
9. connect optional guest allocator hooks;
10. run static initializers on the guest stack;
11. select the guest working directory; and
12. enter the Guest Image at its Entry Point.

The sequence is part of the application contract. Adapters may change without
changing it.

## Modeling rules for new work

- Start a feature with a glossary change when it introduces a new concept.
- Put policy and invariants in the owning bounded context.
- Translate at context boundaries; do not leak Linux representations into the
  guest contract.
- Name tests after domain behavior, not implementation functions.
- Treat an External Probe failure as evidence to classify by bounded context
  before choosing a fix.
- Keep the syscall-number source split as an implementation detail of Darwin
  ABI Translation; numeric ranges are not separate domains.
- Record the public contract source used for new ABI facts.

## Extraction status and sequence

Completed structural steps:

1. Publish `machgate_guest_image` as the shared Guest Image language and remove
   the implementation-shaped `load_results` name.
2. Move Prepare and Run Guest orchestration behind the Guest Execution
   application interface so `machgate.c` remains a CLI adapter.
3. Split Compatibility Runtime capabilities through filesystem metadata, file
   ABI, File Operations, Dispatch, pthread synchronization, CoreFoundation and
   Security additions, unwind, thread-local/TLV, variadic ABI translation,
   task-policy rejection, and process termination into dedicated compiled
   modules with focused behavior tests.
4. Remove target-excluded fake-success Mach controls, FSEvents, CFRunLoop
   execution,
   speculative ICU, and Mach-receive dispatch surfaces. Replace the previously
   removed semaphore placeholders with tested process-local synchronization,
   without adding Mach ports or messaging.
5. Make Guest Image opaque, move raw image preparation out of Guest Execution,
   and enforce the application dependency rule with an executable architecture
   test.
6. Model Guest Process lifecycle explicitly and replace unrelated exported
   globals with one versioned runtime-state contract.
7. Publish Guest Syscall, Syscall Frame, and Syscall Outcome domain types while
   retaining the tested numeric-range routers as infrastructure.
8. Make bounded Guest VM registry exhaustion an explicit mapping failure.

Work in progress and next boundaries:

1. Extract pthread identity, keys, once, attributes, lifecycle, and extensions
   from `libsystem_shim.c`; synchronization already has its own boundary.
2. Replace broad validated-header view usage in supporting adapters with narrow
   Guest Image queries when doing so clarifies a real context contract.
3. Group syscall capabilities by domain while retaining numeric ranges only as
   an infrastructure router.
4. Make the selected Native Provider a richer value when policy needs metadata
   beyond the explicit Provider Decision kind and path.
