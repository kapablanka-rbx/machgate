# MachGate — ARM64 Mach-O CLI Loader for ARM64 Linux

MachGate runs **Apple Silicon ARM64 Mach-O command-line binaries** inside
**ARM64 Linux containers**.

MachGate is a GPLv3-or-later fork of Machismo, which itself builds on Darling's
Mach-O loader work. Machismo proved the core idea: Apple Silicon Mach-O binaries
already contain native ARM64 instructions, so on ARM64 Linux the hard part is
not CPU emulation. The hard part is loading Mach-O correctly and translating the
Darwin ABI surface those binaries expect.

MachGate preserves the inherited Machismo/Darling loader foundation: Mach-O
parsing, fat-binary ARM64 selection, segment mapping, chained fixups, dylib
loading, trampolines, commpage setup, stack setup, and ARM instruction patching.
On top of that foundation, MachGate adds a large Linux-container runtime layer
for real command-line workloads:

- Darwin-to-Linux syscall translation for statically linked Darwin libc /
  libSystem code that executes `svc #0x80` directly
- a broad `libSystem.B.dylib` shim for imported libc/libSystem calls
- Darwin errno, signal, file, process, VM, pthread, and time compatibility paths
- Mach-O guest re-exec support for Go/Rust process wrappers
- host/glibc VM interposition for runtime paths that bypass imported symbols
- Apple/Darwin C++ ABI adapters and libc++ compatibility work
- TLV, initializer, exception-frame, atomics, and ARM feature compatibility
- Docker/QEMU runner scripts, GitHub release packaging, and CI workflows
- loop-engineered external corpus testing against real public macOS ARM64
  binaries

Current release: **v0.3.17**

- GitHub release: <https://github.com/kapabl/machgate/releases/tag/v0.3.17>
- Download: `machgate-0.3.17-linux-arm64.tar.gz`
- Latest validation:
  - 57 / 57 original external ARM64 macOS CLI probes pass
  - 13 / 13 Rust expansion probes pass
  - 29 / 29 ARM64 unit tests pass

Based on components from the [Darling](https://github.com/darlinghq/darling)
project, extracted and adapted for standalone use.

## Scope

Supported target:

- Guest format: ARM64 Mach-O
- Host architecture: ARM64 / aarch64
- Host OS: Linux
- Workload: command-line executables

Current constraints:

- no GUI frameworks
- no AppKit, Metal, CoreGraphics, WindowServer, or GUI event loop support
- no x86-64 support
- child-process support exists for the CLI/runtime paths currently exercised by
  the corpus, but this is not a full Darwin process model

## How It Works

The syscall interception path is documented with instruction-level diagrams in
[docs/SYSCALL_PATCHING.md](docs/SYSCALL_PATCHING.md). The short version: MachGate
finds each ARM64 Darwin `svc #0x80` instruction, replaces that single four-byte
instruction with `b <nearby island>`, saves guest registers and flags in the
island, translates the Darwin syscall ABI to Linux behavior in C, restores the
Darwin-visible result, then branches back to the instruction after the original
`svc`.

```
┌─────────────────────────────────────────────────┐
│ Mach-O CLI Binary (arm64 native code)           │
└────────────┬────────────────────────────────────┘
             │ loader + fixups + syscall patching
             ▼
┌─────────────────────────────────────────────────┐
│ MachGate Runtime                                │
│  • maps Mach-O segments                         │
│  • selects arm64 slices from fat binaries       │
│  • patches chained fixups and imports           │
│  • patches Darwin svc #0x80 sites               │
│  • installs branch islands for syscall gateway  │
│  • sets up stack, apple/env vectors, commpage   │
└────────────┬────────────────────────────────────┘
             ▼
┌─────────────────────────────────────────────────┐
│ ABI Translation Surface                         │
│  • Darwin syscall gateway                       │
│  • libSystem shim                               │
│  • Mach-O dylib loader                          │
│  • native Linux .so mapping                     │
│  • Mach-O dylib loading for Apple-ABI deps      │
│  • trampoline and VM interpose machinery        │
└────────────┬────────────────────────────────────┘
             ▼
┌─────────────────────────────────────────────────┐
│ Native ARM64 Linux execution                    │
└─────────────────────────────────────────────────┘
```

**MachGate handles the hard parts:**

- **Fat/universal binary support** — extracts the arm64 slice
- **Mach-O segment mapping** with correct memory protections
- **Chained fixup resolution** — patches the GOT to redirect dylib imports to native Linux `.so` libraries
- **Darwin syscall translation** — patches ARM64 `svc #0x80` instructions and dispatches Darwin syscall numbers to Linux-compatible behavior
- **Darwin errno and status translation** — maps return values, carry flag semantics, errno values, wait statuses, and selected structures
- **C++ ABI translation** — Apple ARM64 constructors return `this`, Linux doesn't; MachGate wraps calls with ABI adapters
- **`std::string` layout compatibility** — builds libc++ with Apple's alternate SSO layout
- **pthread ABI translation** — detects macOS mutex/condvar/rwlock signatures and reinitializes them for Linux
- **Zero-initializing malloc** — macOS malloc returns zeroed pages; shim wraps `malloc → calloc` to match
- **Mach-O dylib loading** — loads `.dylib` dependencies as Mach-O when native substitutes won't work (vtable layout differences, etc.)
- **Symbol trampolining** — redirects statically-linked library calls to native `.so` implementations via 4-byte branch islands
- **Function replacements** — override specific guest functions with native implementations via `override_lib` config
- **ARMv8.0 compatibility** — emulates ARMv8.1 LSE atomics and downgrades ARMv8.3 RCPC instructions for older cores
- **C++ exception handling** — converts Apple compact unwind to DWARF `.eh_frame`
- **Inherited game support** — preserves Machismo's KMSDRM, SDL/bgfx, and game-specific replacement paths
- **Thread-local variables** — implements `_tlv_bootstrap` for Mach-O TLV descriptors
- **GDB debug symbols** — registers Mach-O symbols with GDB's JIT interface for readable backtraces
- **Binary patching** — applies instruction-level patches from config files
- **Commpage emulation** — maps the macOS commpage at `0xFFFFFC000`
- **libSystem shim** — maps Apple `libSystem.B.dylib` functions to glibc equivalents
- **Go/Rust CLI runtime compatibility** — covers real external binaries such as
  `packer`, `terraform`, `delta`, `nu`, `bun`, `node`, `yazi`, `mise`, and
  `jj`

## Building

### Prerequisites

- aarch64 Linux (native, not cross-compiled)
- GCC or Clang
- CMake 3.16+
- Ninja
- LLVM tools for fixtures (`llvm-mc`, `ld64.lld`, `llvm-lipo`)

### Build the loader

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build --parallel
```

This produces:
- `machgate` — the Mach-O loader executable
- `libsystem_shim.so` — Apple libSystem.B.dylib compatibility shim
- `libmachgate_vm_interpose.so` — VM syscall interposer used for selected host/glibc bypass paths
- `wrapgen` — ELF wrapper code generator (utility)

### ARM64 Docker/QEMU build used by CI

The repository's CI and release workflows build inside an ARM64 Docker image
under QEMU:

```bash
docker build --platform linux/arm64 \
  -t machgate-arm64-toolchain \
  -f .github/docker/arm64-toolchain.Dockerfile .

docker run --rm --platform linux/arm64 \
  -v "$PWD:/work" \
  -w /work \
  machgate-arm64-toolchain \
  bash -lc '
    cmake -S . -B build-arm64 -G Ninja -DCMAKE_BUILD_TYPE=Debug
    cmake --build build-arm64 --parallel
    BUILD_DIR=/work/build-arm64 bash tests/run_tests.sh
  '
```

### Publishing a release

MachGate releases are published by the GitHub Actions release workflow, not by
hand-copying a local `dist/` tarball. When cutting a release, commit the intended
changes, push them to `kapabl/machgate`, push an annotated `vX.Y.Z` tag, then
wait for `.github/workflows/release.yml` to build, test, package, and publish
the GitHub release assets.

See [docs/RELEASE_WORKFLOW.md](docs/RELEASE_WORKFLOW.md) for the exact release
checklist.

## Usage

```bash
./machgate /path/to/macho-binary [args...]
```

### Run on an Intel / x86-64 Linux machine with Docker

MachGate itself is an ARM64 Linux program. On an Intel host, run it inside an
ARM64 Docker container through QEMU/binfmt.

One-time host setup if your Docker installation does not already support
`--platform linux/arm64`:

```bash
docker run --privileged --rm tonistiigi/binfmt --install arm64
```

If the container fails with `exec /usr/bin/bash: exec format error`, the host
does not have ARM64 binfmt/QEMU execution registered yet. Run the same
`tonistiigi/binfmt` command above once, then retry the MachGate runner.

Run the published release against a local ARM64 macOS CLI binary:

```bash
bin/run-macho-docker.sh /path/to/macos-arm64-binary
```

By default the script downloads the latest GitHub release. To pin a specific
release:

```bash
MACHGATE_VERSION=0.3.17 bin/run-macho-docker.sh /path/to/macos-arm64-binary
```

To run with a local Linux ARM64 MachGate build or unpacked release instead of
downloading from GitHub:

```bash
MACHGATE_LOCAL_DIR=/path/to/machgate-arm64-dir bin/run-macho-docker.sh /path/to/macos-arm64-binary
```

`MACHGATE_LOCAL_DIR` may point to an unpacked release directory containing
`bin/machgate` and `lib/libsystem_shim.so`, or to a build directory containing
`machgate` and `libsystem_shim.so`.

To run with a local release tarball instead of downloading from GitHub:

```bash
MACHGATE_TARBALL=/path/to/machgate-0.3.17-linux-arm64.tar.gz bin/run-macho-docker.sh /path/to/macos-arm64-binary
```

To diagnose startup hangs in static constructors, enable LC_MAIN tracing:

```bash
MACHGATE_VERBOSE=1 MACHGATE_TRACE_LCMAIN=1 MACHGATE_TARBALL=/path/to/machgate-0.3.17-linux-arm64.tar.gz bin/run-macho-docker.sh /path/to/macos-arm64-binary
```

To diagnose C++ static-initializer progress without flooding the terminal,
enable the compact C++ initializer trace:

```bash
MACHGATE_VERBOSE=1 MACHGATE_TRACE_LCMAIN=1 MACHGATE_TRACE_SIGNALS=1 MACHGATE_TRACE_CXX_INIT=1 MACHGATE_TARBALL=/path/to/machgate-0.3.17-linux-arm64.tar.gz bin/run-macho-docker.sh /path/to/macos-arm64-binary
```

Use `MACHGATE_TRACE_CXX_INIT=full` only when you need per-initializer
`__MergedGlobals` and `__cxa_guard_*` diagnostics.

To diagnose allocator accounting failures without dumping every allocation,
filter the allocator trace to the failing size:

```bash
MACHGATE_VERBOSE=1 MACHGATE_TRACE_ALLOC=1 MACHGATE_TRACE_ALLOC_SIZE=72 MACHGATE_TARBALL=/path/to/machgate-0.3.26-linux-arm64.tar.gz bin/run-macho-docker.sh /path/to/macos-arm64-binary
```

To stop a stuck guest after a fixed interval while keeping live logs:

```bash
MACHGATE_TIMEOUT=120 MACHGATE_VERBOSE=1 MACHGATE_TRACE_LCMAIN=1 MACHGATE_TARBALL=/path/to/machgate-0.3.17-linux-arm64.tar.gz bin/run-macho-docker.sh /path/to/macos-arm64-binary
```

Pass any guest arguments after the binary path:

```bash
bin/run-macho-docker.sh /path/to/macos-arm64-binary --version
```

The script:

1. starts `ubuntu:24.04` as `linux/arm64`
2. downloads the published MachGate release
3. generates a default `machgate.conf` and `dylib_map.conf`
4. mounts the binary's directory at `/input`
5. runs `/opt/machgate/bin/machgate /input/<binary> ...`
6. streams the guest stdout/stderr and prints the guest exit status

This path is slower than native ARM64 because Docker is emulating the ARM64
Linux container, but it is useful for testing from an Intel workstation.

Some C++ Mach-O binaries also need an Apple-ABI-compatible `libc++.so.1`.
Release tarballs include MachGate's Apple-ABI libc++ build and its required
`libatomic.so.1` runtime dependency under `lib/`, and the Docker helper maps
it automatically as `libc++.1.dylib` when present. Most Go/Rust/static CLI
probes in the current corpus do not need that extra mapping.

When using an older release tarball or a custom local build without bundled
libc++, pass a local libc++ build explicitly:

```bash
MACHGATE_LIBCXX=/path/to/build-libcxx/lib/libc++.so.1 \
MACHGATE_TARBALL=/path/to/machgate-0.3.17-linux-arm64.tar.gz \
bin/run-macho-docker.sh /path/to/macos-arm64-cxx-binary
```

MachGate config discovery:

1. `$MACHGATE_CONFIG` environment variable
2. `machgate.conf` in the current directory
3. `machgate.conf` next to the binary

### Configuration

MachGate uses an INI-style config file:

```ini
[general]
dylib_map = dylib_map.conf
patches = patches/game.conf

[trampoline.sdl2]
lib = libSDL2-2.0.so.0
prefix = _SDL_

[trampoline.bgfx]
lib = ./build-bgfx/libbgfx-shared.so
prefix = _bgfx_
prefix = __ZN4bgfx
init_wrapper = true
renderer = opengles

[trampoline.steam_noop]
lib = STUB
prefix = __ZN3wos5steam

[trampoline.game]
override_lib = ./libgame_patches.so
```

### Dylib mapping

The `dylib_map.conf` file maps macOS dylib names to Linux equivalents:

```conf
# Map to native Linux .so
libsfml-graphics = ./build-sfml/lib/libsfml-graphics.so
libz.1 = libz.so

# Load as Mach-O dylib (preserves Apple ABI)
libsfml-audio = MACHO:./libsfml-audio.2.5.dylib

# Stub out (all symbols return 0)
libsteam_api = STUB

# Skip (leave symbols unresolved)
Carbon = SKIP
```

### Environment variables

| Variable | Purpose |
|----------|---------|
| `MACHGATE_CONFIG` | Path to config file (set to `none` to disable) |
| `MACHGATE_HOME` | Override game userdata/save location |
| `MACHGATE_DYLIB_MAP` | Path to dylib mapping file |
| `MACHGATE_PATCHES` | Path to binary patches file |
| `MACHGATE_TRAMPOLINE_LIB` | Native .so for symbol trampolining |
| `MACHGATE_TRAMPOLINE_PREFIX` | Symbol prefix for trampolining |
| `MACHGATE_VERBOSE_BINDS` | Log all bind resolutions |
| `MACHGATE_LUA_PROFILE` | Enable LuaJIT `jit.p` profiler, write output to given path |
| `MACHGATE_LUA_JITV` | Enable JIT verbose logging (trace compile/abort) |
| `MACHGATE_TRACE_SYSCALL` | Trace selected Darwin syscall gateway activity |
| `MACHGATE_TRACE_WAIT` | Trace wait/exit process status translation |
| `MACHGATE_TRACE_SIGNALS` | Trace signal shim translation paths |
| `MACHGATE_TRACE_CXX_INIT` | `1` for compact C++ initializer progress; `full` for per-initializer guard and `__MergedGlobals` diagnostics |
| `MACHGATE_TRACE_ALLOC` | Trace allocator ledger misses; use `2`, `all`, or `full` for every shim allocation event |
| `MACHGATE_TRACE_ALLOC_SIZE` | With `MACHGATE_TRACE_ALLOC=1`, trace only allocation events matching this size |
| `MACHGATE_TRACE_ALLOC_MISMATCH` | Dump recent allocator provenance on unknown-pointer free/realloc or guest abort |
| `MACHGATE_LIBCXX` | Docker helper path to Apple-ABI `libc++.so.1` for `libc++.1.dylib` mapping |
| `MACHGATE_EXTERNAL_MAP_LIBCXX` | Enable external-test libc++ mapping |
| `MACHGATE_ENABLE_HOST_SIGCHLD_HANDLER` | Diagnostic opt-in for installing the host Linux SIGCHLD handler |

## Testing

```bash
cmake --build build --parallel
bash tests/fixtures/build_fixtures.sh
bash tests/run_tests.sh
```

External real-binary probes are opt-in:

```bash
MACHGATE_RUN_EXTERNAL=1 \
BUILD_DIR="$PWD/build" \
MACHGATE_EXTERNAL_MANIFEST="$PWD/tests/external/arm64_macho_cli_manifest.txt" \
MACHGATE_EXTERNAL_TIMEOUT=120 \
bash tests/test_external_macho_cli.sh
```

Current documented validation is tracked in:

- `docs/EXTERNAL_MACHO_LIVE_STATUS.md`
- `docs/EXTERNAL_MACHO_UNIFIED_STATUS.md`
- `docs/RUST_EXPANSION_LIVE_STATUS.md`
- `docs/PACKER_SIGCHLD_FIX.md`

## Project Structure

```
machgate/
├── src/                    Core loader source
│   ├── machgate.c          Main entry point
│   ├── loader.c/h          Mach-O parsing and segment mapping
│   ├── stack.c             Mach-O stack layout setup
│   ├── commpage.c/h        macOS commpage emulation
│   ├── macho_defs.h        Mach-O structure definitions
│   ├── resolver.c/h        Chained fixup resolution
│   ├── dylib_loader.c/h    Mach-O dylib loading
│   ├── trampoline.c/h      Symbol trampolining system
│   ├── patcher.c/h         Binary instruction patcher
│   ├── gdb_jit.c/h         GDB JIT debug symbol registration
│   ├── eh_frame.c/h        Compact unwind → DWARF conversion
│   ├── lse_emul.c/h        ARMv8.1 LSE atomic emulation
│   ├── lua_entity_opt.c/h  Lua ECS metamethod optimization
│   ├── config.c/h          INI config file parser
│   ├── bgfx_shim.c/h       bgfx renderer integration shim
│   ├── stubs/              Apple ABI type definitions
│   ├── elfcalls/           ELF bridging interface
│   ├── syscall/            Darwin syscall gateway and syscall-family translators
│   └── shim/               libSystem.B.dylib compatibility shim
├── scripts/                External library build scripts
├── patches/                Build-time patches (libc++, bgfx)
├── tools/                  Utility programs
├── tests/                  Test suite
├── examples/               Game configs + game-specific patches source
├── extern/                 Git submodules (bgfx, bimg, bx, llvm-project, sfml, LuaJIT)
└── CMakeLists.txt
```

## Key ABI Differences Handled

| Issue | macOS ARM64 | Linux ARM64 | MachGate Fix |
|-------|-------------|-------------|--------------|
| Constructor return | Returns `this` in x0 | Returns void | ABI adapter trampolines |
| `std::string` layout | Alternate SSO | Standard SSO | Apple-ABI libc++ build |
| `pthread_mutex_t` init | Signature `0x32AAABA7` | All zeros | Three-layer detection & fixup |
| `pthread_cond_t` init | Signature `0x3CB0B1BB` | All zeros | Detection in libc++ + shim |
| `malloc` zero-init | Returns zeroed pages | May return stale data | `malloc → calloc` wrapper in shim |
| Weak symbols | Searches main executable | dlsym only | Mach-O symbol table lookup |
| `__init_offsets` | Run by dyld | Not automatic | Explicit execution |
| TLV descriptors | `_tlv_bootstrap` thunk | `__thread` keyword | Custom `_tlv_bootstrap` |
| Exception handling | Compact unwind | DWARF `.eh_frame` | Compact → DWARF converter |
| `uint64_t` mangling | `y` (unsigned long long) | `m` (unsigned long) | Mangling fallback |
| RCPC (LDAPR) | ARMv8.3 baseline | ARMv8.0 on Cortex-A35 | In-place bit flip to LDAR |
| LSE atomics | ARMv8.1 baseline | ARMv8.0 on Cortex-A35 | Branch islands with LDXR/STXR loops |
| Syscall ABI | x16 + `svc #0x80`, carry flag errno | Linux syscall ABI | Instruction patch to syscall gateway |
| Signals | Darwin signal numbers | Linux signal numbers | shim translation and targeted runtime policies |
| `wait4` status | Darwin status layout | Linux status layout | status translation in shim/syscall gateway |

## License

This project is licensed under the GNU General Public License v3.0 or later; see
[LICENSE](LICENSE).

MachGate is a substantially modified fork of Machismo. It keeps attribution to
the original Machismo/Darling loader work while making MachGate the user-facing
runtime, executable, package, Docker, release, and documentation name. See
[ATTRIBUTION.md](ATTRIBUTION.md) for the project lineage and modification
summary.
