# MachGate

[![CI](https://github.com/kapabl/machgate/actions/workflows/ci.yml/badge.svg)](https://github.com/kapabl/machgate/actions/workflows/ci.yml)
[![External Mach-O CLI](https://github.com/kapabl/machgate/actions/workflows/external-macho.yml/badge.svg)](https://github.com/kapabl/machgate/actions/workflows/external-macho.yml)
[![Repository review](https://github.com/kapabl/machgate/actions/workflows/repository-review.yml/badge.svg)](https://github.com/kapabl/machgate/actions/workflows/repository-review.yml)

MachGate runs ARM64 Mach-O command-line executables directly on ARM64 Linux.
Its central feature is a Darwin-to-Linux syscall gateway for guest code that
issues `svc #0x80` directly, including statically linked runtimes that do not
call imported libSystem functions.

The current scope is intentionally narrow:

- ARM64 Mach-O executables on ARM64 Linux
- thin executables and ARM64 slices from fat binaries
- command-line workloads
- a single guest thread
- direct Darwin syscall translation

MachGate's core does not target x86-64 guests, graphical applications, Apple
frameworks, Mach IPC, or general macOS application compatibility.

## Verification status

| Gate | Result | Where |
| --- | --- | --- |
| Unit and behavior suite | 86/86 passed locally | ARM64 Linux; [archive port evidence and engine limits](docs/ARCHIVE_SHIM_PARITY.md) |
| Extended external corpus gate | 57/57 passed, 0 crashes, 0 harness failures | `external-macho.yml`, weekly and on demand |
| Bitcoin Core runner gate | 7/7 passed (versions 25.2 through 31.0) | focused manifest, on demand |
| Architecture layer policy | enforced on every change | `repository-review.yml` |
| ASan-instrumented guests | mapped-provider surface test | `tests/test_libclang_rt_asan_provider.sh` |

The 57-probe corpus runs real, checksum-pinned public ARM64 Mach-O
executables (ripgrep, jq, node, nvim, duckdb, sqlite3, protoc, the Kubernetes
and IaC toolchains, and more) through the gateway. The immutable evidence
record of the completed clean-room baseline is archived with the project's
review documents.

## How it works

MachGate maps the executable's segments at their requested virtual addresses,
selects an `LC_MAIN` or `LC_UNIXTHREAD` entry point, and creates a guarded guest
stack. Before entering guest code it scans executable instruction sections for
Darwin `svc #0x80` instructions and replaces each one with a direct ARM64
branch to a nearby generated island.

The island preserves guest registers and flags, calls the syscall dispatcher,
then resumes at the instruction after the original `svc`. The dispatcher
translates syscall numbers, arguments, structures, flags, results, and errno
values between Darwin and Linux conventions. Unsupported calls return Darwin
`ENOSYS`.

MachGate also includes focused compatibility components:

- classic and chained Mach-O import binding to an explicitly selected native
  provider
- a native libSystem shim for imported command-line runtime functions
- guest C++ allocator hook discovery
- an optional prefix-based native trampoline mechanism
- a VM interposer for host mappings that must participate in guest VM tracking

See [syscall patching](docs/SYSCALL_PATCHING.md) for the gateway design and
[the syscall table](docs/DARWIN_SYSCALL_TABLE_STATUS.md) for dispatcher coverage.
The [domain model](docs/DOMAIN_MODEL.md) defines the ubiquitous language,
bounded contexts, invariants, and application workflow.
The [Image Preparation cache](docs/STARTUP_CACHE.md) reuses derived startup
artifacts across launches without modifying the Guest Executable.
The optional [Apple compatibility providers](docs/ARCHIVE_SHIM_PARITY.md)
restore the archive's 849 exported names through separate Foundation, ICU and
composite libraries. Their ABI checks reference
[Apple OSS and the local LLVM sources](docs/APPLE_ABI_REFERENCES.md).
The [external corpus status](docs/EXTERNAL_CORPUS_STATUS.md) classifies the
current compatibility baseline by bounded context.
[ASan-instrumented guest support](docs/ASAN_SUPPORT.md) documents the mapped
provider surface for guests built with `-fsanitize=address`.

## Build

MachGate must be built for an ARM64 Linux target.

```sh
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build --parallel
```

The build produces:

- `build/machgate`
- `build/libsystem_shim.so`
- `build/libmachgate_vm_interpose.so`

The default build also produces `libmachgate_foundation.so`,
`libmachgate_icu.so`, and `libmachgate_apple_compat.so` in `build/`, requiring
ICU 74 development files. Disable them with
`-DMACHGATE_BUILD_APPLE_PROVIDERS=OFF` for a core-only build. Providers are
selected through the dependency map; see their compatibility limits above.

Run a guest with:

```sh
build/machgate path/to/program [arguments...]
```

Use `-v` for loader diagnostics.

## Test

The fixture builder needs an LLVM toolchain capable of producing ARM64 Mach-O
objects and executables.

```sh
bash tests/fixtures/build_fixtures.sh
bash tests/run_tests.sh
```

Tests cover thin and fat loading, entry-point handling, static initializers,
syscall ranges, VM operations, process re-execution, the shim surface, allocator
hooks, classic and chained libSystem imports, and native trampolines.

## Optional runtime settings

`MACHGATE_SYSTEM_SHIM_LIB` names the native shim used for dynamic libSystem and
guest allocator bridge tests.

`MACHGATE_DYLIB_MAP` names a `key = native-library` map for other Mach-O
dependencies. Keys use the dependency basename without the `.dylib` suffix.

`MACHGATE_GUEST_PLATFORM` can be set to `macos` or `ios` to override the
platform recorded by the guest's `LC_BUILD_VERSION` command.

`MACHGATE_TRAMPOLINE_LIB` and `MACHGATE_TRAMPOLINE_PREFIX` enable explicit
prefix-based redirection of guest definitions to symbols in a native shared
library.

`MACHGATE_TRACE_CXX_ALLOCATORS` prints the number of guest C++ allocation hooks
connected to the shim.

`MACHGATE_TRACE_SHIM` enables shim diagnostics. Its value is read on first use
and retained for the process lifetime; restart the process to change it.

Additional narrowly scoped trace variables live next to the subsystem that
implements them. They are diagnostics, not a stable public interface.

## Repository layout

```text
src/machgate.c                 startup and guest entry
src/domain/                    guest image, process, syscall, launch context, and provider decisions
src/application/               guest session workflow
src/adapters/                  guest execution and unwinding adapters
src/ports/                     adapter interfaces
src/loader.c                   thin/fat validation and segment mapping
src/instruction_adaptation.c   guest instruction scanning and adaptation
src/native_imports.c           classic and chained import binding
src/native_provider_map.c      native provider selection and dependency mapping
src/guest_symbols.c            guest symbol lookup
src/guest_allocator_bridge.c   guest-to-shim allocator hooks
src/native_structor_adapter.c  guest initializer adaptation for native providers
src/trampoline.c               optional native symbol redirection
src/vm_interpose.c             guest VM tracking interposer
src/syscall/                   Darwin syscall patching and translation
src/shim/                      native libSystem compatibility surface
tests/                         fixture and behavior tests
docs/                          current architecture and maintenance notes
```

## License

The MachGate license has not yet been determined. All rights are reserved by
the project authors until a license is chosen.
