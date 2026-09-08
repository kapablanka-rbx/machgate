# MachGate Next Steps

## iOS common-tests

iOS ARM64 Mach-O binaries use the same Mach-O format as macOS ARM64. MachGate's
loader, syscall gate, and resolver are platform-agnostic — they map segments,
patch `svc #0x80`, and resolve dylibs regardless of whether the binary targets
macOS or iOS.

Build iOS common-tests with cross-compilation:

```
gobot pkg sync --use xcode
Tools/Util/gobot run buck2/build common-tests/UnitTests ios/arm64 --preset ci xcode-toolchain=cross-compilation build-mode=remote materializations=all sanitize=none
```

Open questions:

- What iOS frameworks do the common-tests link? Likely `UIKit`, `Foundation`,
  `CoreFoundation` — more GUI-heavy than macOS CLI tests.
- Can we stub/skip iOS frameworks the same way we skip `AppKit`/`Foundation`
  for macOS?
- Do iOS binaries use any iOS-specific syscalls not in the current Darwin
  syscall table (669 rows from XNU `syscalls.master`)?
- iOS binaries may link `UIKit`, `CoreGraphics`, `Metal`, `QuartzCore`,
  `AudioToolbox`, `CoreVideo` — all currently unmapped. Need to determine
  which are needed for CLI test execution vs which can be stubbed.

## ASan incompatibility

ASan (`sanitize: address`) is incompatible with MachGate and must be disabled
with `sanitize=none`.

Why:

- ASan instruments every memory access at compile time.
- Each instrumented load/store calls into `libclang_rt.asan_osx_dynamic.dylib`.
- The key symbol `___asan_shadow_memory_dynamic_address` is a pointer to the
  Darwin shadow memory base.
- MachGate has no mapping for `libclang_rt.asan_osx_dynamic.dylib` — all its
  symbols resolve to NULL.
- The first instrumented memory access in the first static constructor calls
  through a NULL function pointer → immediate SIGSEGV.
- Darwin ASan shadow memory layout differs from Linux ASan shadow memory
  layout, so a simple dylib mapping would not work even if the runtime were
  available.

All game-engine common-test builds for MachGate must use `sanitize=none`.

## Release workflow status

The release workflow (`.github/workflows/release.yml`) is set up for
`kapablanka-rbx/machgate`. It triggers on `v*` tag pushes and runs on
`ubuntu-24.04-arm` GitHub Actions runners.

To cut a release:

```bash
git push origin master
git tag -a vX.Y.Z -m "MachGate vX.Y.Z"
git push origin vX.Y.Z
gh run list -R kapablanka-rbx/machgate --workflow release.yml --limit 5
gh run watch -R kapablanka-rbx/machgate <run-id>
gh release view vX.Y.Z -R kapablanka-rbx/machgate --json url,assets
```

As of 2026-09-08, `v0.4.0` was tagged but the release workflow has not yet
triggered. Possible causes:

- `ubuntu-24.04-arm` runner not available for the `kapablanka-rbx` account.
- GitHub Actions minutes/billing not enabled for the account.
- Need to check repo Actions settings at
  `https://github.com/kapablanka-rbx/machgate/settings/actions`.

## `timeout` command incompatibility

The `timeout` command in `bin/run-macho-docker.sh` installs signal handlers
that interfere with MachGate's signal/altstack handling under QEMU. All 7
"crashing" game-engine common-test binaries were actually working — they only
crashed because of `timeout`.

When running game-engine common-tests, use `bin/run-common-tests.sh` (which
does not use `timeout`) or run `machgate` directly without `timeout`.

## Working recipe for game-engine common-tests

Prerequisites:

- MachGate built for ARM64: `build-arm64/machgate` + `build-arm64/libsystem_shim.so`
- Apple-ABI libc++ built: `build-libcxx/lib/libc++.so.1`
- Docker image: `machgate-arm64-toolchain` (has `libatomic1` which libc++ needs)
- game-engine common-tests built with `sanitize=none`

Run a single binary:

```bash
bin/run-common-tests.sh Sqlite3
```

Run all binaries:

```bash
bin/run-common-tests.sh
```
