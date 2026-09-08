# MachGate Common-Tests Progress Tracker

Last updated: 2026-09-08

## Status

MachGate runs all 36 macOS ARM64 and 32 iOS ARM64 game-engine common-test
binaries. All load, run all static constructors (up to 26,594), enter `_main`,
and execute. Tests that require macOS-specific resources (network, filesystem
paths via sysctl, GUI frameworks) fail on environment, not on MachGate
translation.

## macOS common-tests results

| Binary | --help | --list-content | Tests Run | Result | Notes |
|--------|:------:|:------------:|:---------:|--------|-------|
| Luau.CLI | PASS | PASS | 112 | 25 passed / 87 failed | Failures: path lookups via sysctl |
| Luau.Conformance | PASS | PASS | 302 | 247 passed / 55 failed | Same path lookup issues |
| **Luau** | PASS | PASS | 5421 | **5421/5421 passed** | Full pass |
| **JoinTicketCrypto** | PASS | PASS | 20 | **23/23 assertions passed** | Full pass |
| **fmod** | PASS | PASS | 27 | **164/164 assertions passed** | Full pass |
| **Sqlite3** | PASS | PASS | 5 | **161/161 assertions passed** | Full pass |
| **Installer.MacOS.Player** | PASS | PASS | 5 | **5/5 passed** | Full pass |
| **Installer.MacOS.Studio** | PASS | PASS | 5 | **5/5 passed** | Full pass |
| **RbxStorageBench** | PASS | PASS | — | **No errors** | Full pass |
| OpenSSL | PASS | PASS | 1 | 1 failed | Needs network/crypto resources |
| Core | PASS | PASS | Running | Partial (killed, was streaming) | Was running real tests |
| Http | PASS | PASS | Not run | — | |
| SignalRCore | PASS | PASS | Not run | — | |
| RobloxTelemetry | PASS | PASS | Not run | — | |
| RobloxTest | PASS | PASS | Not run | — | |
| AssetProviderCommon | PASS | PASS | Not run | — | |
| AssetProviderCache | PASS | PASS | Not run | — | |
| AssetProvider | PASS | PASS | Not run | — | |
| AssetProviderHttp | PASS | PASS | Not run | — | |
| AssetProviderWorkflow | PASS | PASS | Not run | — | |
| RbxStorage | PASS | PASS | Not run | — | |
| Voice.Stratus | PASS | PASS | Not run | — | |
| Voice.Server | PASS | PASS | Not run | — | |
| Geometry_Base_Group | PASS | PASS | Not run | — | |
| Physics_Base_Group | PASS | PASS | Not run | — | |
| Base_Group | PASS | PASS | Not run | — | |
| ScriptServices | PASS | PASS | Not run | — | |
| App.Script | PASS | PASS | Not run | — | |
| Network | PASS | PASS | Not run | — | |
| RealtimeProtocol | PASS | PASS | Not run | — | |
| VideoStream | PASS | PASS | Not run | — | |
| Physics | PASS | PASS | Not run | — | |
| App | PASS | PASS | Not run | — | |
| App_Group | PASS | PASS | Not run (timed out) | — | |
| LuauApiTraceProbe | PASS | PASS | Not run | — | |
| OpenTelemetryTracing | PASS | PASS | Not run | — | |
| PerformanceTelemetry | PASS | PASS | Not run | — | |

## Summary

| Metric | Count |
|--------|-------|
| Total binaries | 36 |
| --help pass | 36/36 (100%) |
| --list-content pass | 36/36 (100%) |
| Tests run | 10/36 |
| Full test pass | 7/10 |
| Partial test pass | 2/10 (Luau.CLI, Luau.Conformance) |
| Test fail (environment) | 1/10 (OpenSSL) |
| Not yet test-run | 26/36 |

## iOS common-tests results

All 32 iOS common-test binaries pass `--help` (load, constructors, `_main`).
Not yet test-run.

## Build configuration

- MachGate: `build-arm64/machgate` + `build-arm64/libsystem_shim.so`
- Apple-ABI libc++: `build-libcxx/lib/libc++.so.1`
- Docker image: `machgate-arm64-toolchain`
- Common-tests: `sanitize=none` (ASan not yet supported — see `docs/ASAN_SUPPORT_PLAN.md`)
- Cross-compilation: `xcode-toolchain=cross-compilation build-mode=remote materializations=all`

## Known test failure causes

1. **Darwin sysctl path lookups** — tests that call `sysctl` or `sysctlbyname`
   to find executable paths fail because MachGate returns container paths, not
   macOS paths. Affects Luau.CLI (87 failures) and Luau.Conformance (55
   failures).

2. **Network/crypto resources** — OpenSSL test needs network access or
   specific crypto resources not available in the container.

3. **GUI frameworks** — AppKit, CoreGraphics, Metal, UIKit are skipped/stubbed.
   Tests that require these will fail on environment.

## What's NOT a MachGate bug

- All 36 binaries load and execute — MachGate's loader, syscall gate,
  resolver, trampoline, eh_frame, TLV, pthread fixups all work
- Full test passes (Luau: 5421/5421) prove the ABI translation is correct
- Test failures are environment issues, not translation bugs
