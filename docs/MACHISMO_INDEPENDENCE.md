# MachGate Independence Plan

## Purpose

MachGate is 71% new code. 29% is inherited from Machismo (GPL v3), which
itself derives from Darling. This document describes what it would take to
sever that dependency entirely, write clean-room replacements from public
Apple and LLVM sources, and unlock the ability to change the license.

## Current state

- Total source: ~43,000 lines
- MachGate-new: ~30,600 lines (71%) — syscall gateway, libSystem shim, VM
  interposition, C++ ABI adapters, external corpus, CI, release tooling
- Machismo-inherited: ~12,500 lines (29%) — Mach-O loader, resolver,
  trampoline, eh_frame, ISA emulation, GDB JIT, dylib loader, stack/commpage,
  config, game-specific shims (bgfx/SDL/Lua)

## Why the GPL constraint exists

Machismo is GPL v3. MachGate inherited that license because it is a fork.
The 29% inherited code carries the GPL v3 obligation. To change the license
(e.g., to Apache 2.0 or MIT), every line of Machismo-derived code must be
replaced with clean-room implementations written from public specifications
and open-source references that are not GPL.

## Public reference sources (non-GPL)

All references needed for a clean-room rewrite are available under
permissive licenses:

- **LLVM `compiler-rt` and `lld/MachO/`** — Apache 2.0 with LLVM exception.
  Forked at `kapablanka-rbx/llvm-project`.
  - `lld/MachO/SyntheticSections.cpp` — chained fixup encoding
  - `lld/MachO/InputFiles.cpp` — Mach-O load command parsing
  - `lld/MachO/Target.cpp` — ARM64 branch relocation
  - `compiler-rt/lib/unwind/` — DWARF unwind tables
  - `llvm/lib/Target/AArch64/` — ARM64 instruction encoding

- **XNU** — APSL 2.0.
  Forked at `kapablanka-rbx/xnu`.
  - `bsd/kern/syscalls.master` — syscall numbers
  - `bsd/sys/errno.h` — Darwin errno values
  - `osfmk/mach/` — Mach VM structures
  - `bsd/sys/mman.h`, `bsd/sys/stat.h`, `bsd/sys/fcntl.h` — Darwin structs

- **Apple dyld** — APSL 2.0.
  Available at `apple-oss-distributions/dyld`.
  - `dyld/MachOFile.cpp` — Mach-O parsing
  - `dyld/ChainedFixups.cpp` — chained fixup format
  - `dyld/DyldAPIs.cpp` — dyld initialization

- **Apple cctools** — APSL 2.0.
  Available at `apple-oss-distributions/cctools`.
  - `libmacho/` — Mach-O structure definitions
  - `as/` — ARM64 assembler

- **ARM Architecture Reference Manual** — public specification.
  ARMv8.0/8.1/8.3 instruction encoding for LSE and RCPC.

## What needs to be replaced

### 1. Mach-O loader (`loader.c`, `loader.h`, `macho_defs.h`)

~2,500 lines. The Mach-O parser: fat binary selection, LC_LOAD_DYLINKER,
LC_SEGMENT_64 parsing, memory mapping with correct protections, zero-fill,
LC_MAIN/LC_UNIXTHREAD entry.

References:
- `lld/MachO/InputFiles.cpp` — load command parsing
- `lld/MachO/MachORuntime.cpp` — segment mapping
- `dyld/MachOFile.cpp` — fat binary, LC commands
- XNU `bsd/kern/mach_loader.c` — kernel-side loader

### 2. Chained fixup resolver (`resolver.c`, `resolver.h`)

~1,500 lines. Two fixup formats:
- LC_DYLD_CHAINED_FIXUPS — format 6 (PTR_64_OFFSET) and format 2 (PTR_64)
- LC_DYLD_INFO_ONLY — rebase/bind opcode interpreter, ULEB128/SLEB128

References:
- `lld/MachO/SyntheticSections.cpp` — fixup encoding
- `dyld/ChainedFixups.cpp` — fixup decoding
- `dyld/MachOFile.cpp` — bind/rebase opcodes

### 3. Trampoline system (`trampoline.c`, `trampoline.h`)

~1,000 lines. 4-byte branch islands for redirecting statically-linked
function calls to native Linux `.so` implementations. Includes STUB mode,
override mechanism, C++ mangling fallback (`y` <-> `m`).

References:
- ARM64 `B` instruction encoding (ARM ARM)
- `llvm/lib/Target/AArch64/AsmParser/` — instruction encoding
- `lld/MachO/Target.cpp` — branch relocation

### 4. eh_frame converter (`eh_frame.c`, `eh_frame.h`)

~1,500 lines. Converts Apple compact unwind encodings to DWARF `.eh_frame`
FDEs. Registers via `__register_frame` and `_dl_find_object` hook.

References:
- `compiler-rt/lib/unwind/` — DWARF unwind
- `lld/MachO/UnwindInfoSection.cpp` — compact unwind format
- XNU `osfmk/mach-o/compact_unwind_encoding.h` — encoding spec

### 5. ISA emulation (`isa_emul.c`, `isa_emul.h`)

~1,000 lines. Two features:
- ARMv8.1 LSE atomic instructions -> LDXR/STXR branch islands
- ARMv8.3 RCPC (LDAPR) -> LDAR in-place bit flip

References:
- ARM Architecture Reference Manual (public spec)
- `llvm/lib/Target/AArch64/` — instruction encoding and patterns

### 6. GDB JIT symbols (`gdb_jit.c`, `gdb_jit.h`)

~500 lines. Synthesizes an in-memory ELF from Mach-O symbol table for GDB
JIT debug interface (`__jit_debug_register_code`).

References:
- `llvm/include/llvm/ExecutionEngine/JITLink/` — JIT ELF
- GDB JIT interface documentation (public)

### 7. Dylib loader (`dylib_loader.c`, `dylib_loader.h`)

~1,000 lines. Loads `.dylib` dependencies as Mach-O when native substitutes
have ABI mismatches. Maps segments, parses LC_SYMTAB, resolves chained
fixups, runs initializers.

References:
- `dyld/MachOFile.cpp` — Mach-O loading
- `dyld/DyldAPIs.cpp` — initializer execution

### 8. Stack and commpage (`stack.c`, `commpage.c`, `commpage.h`)

~500 lines. Darwin stack layout (argc/argv/envp/apple vector), commpage
mapping at `0xFFFFFC000`.

References:
- XNU `bsd/kern/` — stack setup
- `dyld/dyldInitialization.cpp` — stack/argv layout
- XNU `osfmk/arm/commpage/` — commpage layout

### 9. Config parser (`config.c`, `config.h`)

~500 lines. INI-style config file parser for `machgate.conf`.

No external reference needed — this is a simple INI parser.

### 10. Game-specific code (delete, do not replace)

~3,000 lines. Not needed for the CLI/test use case:
- `bgfx_shim.c/h` — game renderer
- `lua_entity_opt.c/h` — Lua ECS optimization
- `sdl_window_shim.c/h` — SDL window
- `examples/` — game configs (NecroDancer, Sugar)

## What stays as-is

All MachGate-new code (~30,600 lines) is original work and already owned by
MachGate. It is not affected by the Machismo GPL v3 license:

- `src/syscall/` — syscall gateway and 669-syscall dispatchers
- `src/shim/libsystem_shim.c` — libSystem.B.dylib compatibility shim
- `src/vm_interpose.c` — VM interposition
- `src/elfcalls/` — ELF bridging
- `bin/run-common-tests.sh`, `bin/run-macho-docker.sh` — runner scripts
- `tests/` — test suite and fixtures
- `docs/` — documentation
- `.github/` — CI and release workflows

## Implementation strategy

### Phase 1: Delete game-specific code

Remove `bgfx_shim`, `lua_entity_opt`, `sdl_window_shim`, `examples/`.
~3,000 lines deleted. No functionality lost for CLI/test use case.

### Phase 2: Replace config parser

Simple INI parser. ~500 lines. No Mach-O knowledge needed.

### Phase 3: Replace ISA emulation

Mechanical translation from the ARM Architecture Reference Manual.
~1,000 lines. LSE and RCPC encoding patterns are well-documented.

### Phase 4: Replace GDB JIT symbols

In-memory ELF synthesis. ~500 lines. Uses GDB JIT public interface.

### Phase 5: Replace stack and commpage

Darwin stack layout and commpage. ~500 lines. Well-documented in XNU and
dyld source.

### Phase 6: Replace eh_frame converter

Compact unwind to DWARF. ~1,500 lines. References in `compiler-rt/lib/unwind/`
and `lld/MachO/UnwindInfoSection.cpp`.

### Phase 7: Replace trampoline system

Branch island generation. ~1,000 lines. ARM64 `B` encoding is public.

### Phase 8: Replace dylib loader

Mach-O dylib mapping. ~1,000 lines. References in dyld source.

### Phase 9: Replace chained fixup resolver

The hardest piece. Two fixup formats with many edge cases.
~1,500 lines. References in `dyld/ChainedFixups.cpp` and
`lld/MachO/SyntheticSections.cpp`.

### Phase 10: Replace Mach-O loader

The foundation. ~2,500 lines. References in `lld/MachO/InputFiles.cpp`,
`dyld/MachOFile.cpp`, and XNU `bsd/kern/mach_loader.c`.

## After all phases complete

- Zero Machismo-derived code remains
- All code is clean-room from public specs (Apache 2.0, APSL 2.0, ARM spec)
- License can change to Apache 2.0 or any permissive license
- `ATTRIBUTION.md` updated to credit Machismo as historical inspiration only
- No GPL v3 obligation remains

## License compatibility of reference sources

| Source | License | Can we use it for clean-room? |
|--------|---------|-------------------------------|
| LLVM `compiler-rt`, `lld` | Apache 2.0 + LLVM exception | Yes |
| XNU | APSL 2.0 | Yes (APSL is a weak copyleft, compatible with clean-room if you read the code and write your own implementation) |
| Apple `dyld` | APSL 2.0 | Yes (same as XNU) |
| Apple `cctools` | APSL 2.0 | Yes |
| ARM ARM | Public specification | Yes |

Note: APSL 2.0 is a weak copyleft. Reading the source and writing an
independent implementation is permitted. The resulting code is not
subject to APSL terms. This is not legal advice — confirm with counsel
before changing the license.

## Effort estimate

- Total: ~10,000 lines of clean-room replacement
- Phases 1-5 (delete + simple replacements): days
- Phases 6-8 (eh_frame, trampoline, dylib loader): days
- Phases 9-10 (resolver, loader): days — the Mach-O format is public and
  well-documented in `lld/MachO/`, `dyld`, and XNU headers. The loader parses
  load commands and maps segments. The resolver walks fixup chains. Both are
  format parsing against published specs, not algorithm invention.

The real heavy lifting was always in the MachGate code — the 669-syscall
gateway, the libSystem shim, the C++ ABI translation, the eh_frame converter.
That 71% is already written and owned by MachGate.
