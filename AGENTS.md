# MachGate instructions

## Project goal

MachGate extends Machismo to run ARM64 Mach-O command-line executables inside ARM64 Linux containers.

The main new feature is Darwin-to-Linux syscall translation for Mach-O binaries that contain statically linked Darwin libc or libSystem code and execute Darwin syscalls directly.

## Base project

This is a fork of Machismo.

Preserve Machismo's existing:

- Mach-O loader
- Fat-binary ARM64 selection
- Segment mapping
- Stack setup
- Commpage setup
- Chained fixup resolver
- Mach-O dylib loader
- Function trampoline system
- Nearby executable branch-island machinery
- Existing tests

Do not rewrite the loader from scratch.

## Target

Initial supported target:

- Guest format: ARM64 Mach-O
- Host architecture: ARM64 / aarch64
- Host operating system: Linux
- Deployment target: Linux containers
- Workload: command-line executables only

Initial constraints:

- Single guest thread
- No GUI
- No AppKit
- No Metal
- No CoreGraphics
- No WindowServer
- No Mach IPC
- No child-process support
- No x86-64 support

## Core technical problem

Darwin ARM64 syscall wrappers use:

- x16 for the Darwin syscall or trap number
- x0, x1, x2, etc. for syscall arguments
- svc #0x80 to enter XNU
- carry flag clear on success
- carry flag set on failure

Linux does not implement this ABI.

A statically linked Darwin libc bypasses imported-symbol resolution, so symbol trampolines alone are not enough.

## Chosen design

Do not depend on Mach-O symbol names for syscall interception.

Scan executable Mach-O instruction sections before guest execution.

For every Darwin svc #0x80 instruction:

1. Allocate a nearby branch island.
2. Replace the four-byte svc instruction with a direct ARM64 branch to the island.
3. Preserve the guest register state.
4. Preserve condition flags.
5. Switch to a private host stack if needed.
6. Dispatch based on the Darwin syscall number in x16.
7. Translate Darwin arguments, flags, structures, return values, and errno values to Linux semantics.
8. Restore guest state.
9. Resume at the instruction after the original svc.

Use a direct branch, not branch-with-link, so the guest link register is not destroyed.

## Milestone order

### Milestone 1

Add a failing static ARM64 Mach-O fixture named darwin_exit42.

The fixture must:

- Use Darwin syscall convention.
- Put Darwin SYS_exit in x16.
- Put exit code 42 in x0.
- Execute svc #0x80.
- Not rely on imported libSystem symbols.

Add a test proving the current loader does not handle it.

Do not implement syscall translation in this milestone.

### Milestone 2

Implement the smallest Darwin syscall gateway needed to make darwin_exit42 pass.

Only implement Darwin exit.

Do not implement write, open, mmap, threads, signals, Mach traps, or cleanup refactors.

### Milestone 3

Add a static ARM64 Mach-O fixture named darwin_write_stdout.

The fixture must:

- Use Darwin syscall convention.
- Write a fixed string to stdout.
- Exit with status 0.

Implement only the Darwin write syscall needed to make that fixture pass.

### Later milestones

Implement these only after exit and write pass:

- read
- open
- close
- lseek
- fstat
- mmap
- munmap
- mprotect

Defer:

- signals
- threads
- kqueue
- sockets
- fork
- execve
- posix_spawn
- Mach ports
- Mach messages

## Build commands

Configure:

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

Build:

cmake --build build --parallel

Verbose single-thread build for debugging:

cmake --build build --verbose -j1

Run tests:

bash tests/fixtures/build_fixtures.sh
bash tests/run_tests.sh

## Building game-engine common-tests for macOS (cross-compilation on Linux)

Run from the game-engine checkout:

  Tools/Util/gobot run buck2/build common-tests/UnitTests macos/arm64 --preset ci xcode-toolchain=cross-compilation build-mode=remote materializations=all

Prerequisites: gobot pkg sync --use xcode

Available xcode versions: 16.2, 26.3 (default), 26.4 — override with xcode-version=N.N

## Development rules

- Make small commits.
- Keep each change narrowly scoped.
- Add a failing test before implementing behavior.
- Run tests after each change.
- Do not do unrelated refactors.
- Do not remove license or attribution notices.
- Do not remove existing Machismo behavior unless a test proves it is obsolete.
- Log unknown Darwin syscall numbers with guest PC and argument registers.
- Return Darwin ENOSYS for unsupported syscalls.
- Prefer raw Linux syscalls in the backend instead of glibc wrappers when implementing syscall translation.

## Code style

- Use descriptive, intention-revealing names.
- No single-letter variable names except loop counters.
- Keep functions short.
- Use early returns.
- Prefer clarity over cleverness.
- Do not add inline comments.
- Name return-value variables result.
- Preserve local file formatting.
