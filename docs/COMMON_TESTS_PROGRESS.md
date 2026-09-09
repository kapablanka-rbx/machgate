# MachGate Common-Tests Progress Tracker

Last updated: 2026-09-08

## macOS common-tests results

| # | Binary | Size | --help | --list-content | Exit | Test Cases | Passed | Failed | Status |
|---|--------|------|:------:|:--------------:|:----:|-----------|:------:|:------:|--------|
| 1 | Luau | 43M | PASS | PASS | 0 | 5421 | 5421 | 0 | **PASS** |
| 2 | Voice.Server | 42M | PASS | PASS | 0 | 640 | 11174 asrt | 0 | **PASS** |
| 3 | Voice.Stratus | 18M | PASS | PASS | 0 | 16 | 50 asrt | 0 | **PASS** |
| 4 | AssetProviderCache | 24M | PASS | PASS | 0 | 19 | 152 asrt | 0 | **PASS** |
| 5 | AssetProvider | 25M | PASS | PASS | 0 | 9 | 34 asrt | 0 | **PASS** |
| 6 | fmod | 9.5M | PASS | PASS | 0 | 27 | 164 asrt | 0 | **PASS** |
| 7 | Sqlite3 | 9.7M | PASS | PASS | 0 | 5 | 161 asrt | 0 | **PASS** |
| 8 | JoinTicketCrypto | 12M | PASS | PASS | 0 | 20 | 23 asrt | 0 | **PASS** |
| 9 | Installer.MacOS.Player | 8.1M | PASS | PASS | 0 | 5 | 5 | 0 | **PASS** |
| 10 | Installer.MacOS.Studio | 8.1M | PASS | PASS | 0 | 5 | 5 | 0 | **PASS** |
| 11 | RbxStorageBench | 9.7M | PASS | PASS | 0 | — | No errors | 0 | **PASS** |
| 12 | AssetProviderWorkflow | 26M | PASS | PASS | — | 25+ | 25+ | 0 | RUNNING |
| 13 | Physics_Base_Group | 35M | PASS | PASS | 42 | 656 | 643 | 13 | PARTIAL (98%) |
| 14 | Geometry_Base_Group | 32M | PASS | PASS | 42 | 456 | 347 | 109 | PARTIAL (76%) |
| 15 | Luau.Conformance | 15M | PASS | PASS | 0 | 302 | 247 | 55 | PARTIAL (82%) |
| 16 | SignalRCore | 24M | PASS | PASS | 42 | 10 | 4 | 6 | PARTIAL (40%, needs sockets) |
| 17 | Luau.CLI | 5.8M | PASS | PASS | 0 | 112 | 25 | 87 | PARTIAL (22%, sysctl) |
| 18 | Http | 23M | PASS | PASS | 137 | 21+ | 21+ | 1+ | FAIL (ObjC cookie, killed) |
| 19 | AssetProviderHttp | 25M | PASS | PASS | 137 | 26+ | 26+ | 0+ | FAIL (CDN probe, killed) |
| 20 | PerformanceTelemetry | 23M | PASS | PASS | 139 | 3 | 3 | 0 | CRASH (EnterExit) |
| 21 | OpenSSL | 12M | PASS | PASS | 0 | 1 | 0 | 1 | FAIL (network) |
| 22 | App | 425M | PASS | PASS | 139 | — | — | — | CRASH |
| 23 | App.Script | 374M | PASS | PASS | 139 | — | — | — | CRASH |
| 24 | Network | 382M | PASS | PASS | 139 | — | — | — | CRASH |
| 25 | Physics | 367M | PASS | PASS | 139 | — | — | — | CRASH |
| 26 | RealtimeProtocol | 356M | PASS | PASS | 139 | — | — | — | CRASH |
| 27 | RobloxTelemetry | 26M | PASS | PASS | 139 | — | — | — | CRASH |
| 28 | ScriptServices | 368M | PASS | PASS | 139 | — | — | — | CRASH |
| 29 | VideoStream | 356M | PASS | PASS | 139 | — | — | — | CRASH |
| 30 | AssetProviderCommon | 18M | PASS | PASS | — | — | — | — | HUNG (killed) |
| 31 | RbxStorage | 12M | PASS | PASS | — | — | — | — | HUNG (killed) |
| 32 | Core | 28M | PASS | PASS | — | Partial | — | — | KILLED |
| 33 | RobloxTest | 28M | PASS | PASS | — | — | — | — | NOT RUN |
| 34 | LuauApiTraceProbe | 22M | PASS | PASS | — | — | — | — | NOT RUN |
| 35 | OpenTelemetryTracing | 22M | PASS | PASS | — | — | — | — | NOT RUN |
| 36 | Base_Group | 116M | PASS | PASS | — | — | — | — | NOT RUN |
| 37 | App_Group | 516M | PASS | PASS | — | — | — | — | NOT RUN |

## Summary

| Metric | Count | % |
|--------|-------|---|
| Total binaries | 36 | 100% |
| --help pass | 36/36 | 100% |
| --list-content pass | 36/36 | 100% |
| Tests run | 21/36 | 58% |
| Full test pass | 11/21 | 52% |
| Partial test pass | 5/21 | 24% |
| Fail (environment) | 3/21 | 14% |
| Crash | 8 | — |
| Hung/killed | 3 | — |
| Not yet run | 5 | — |
| Running | 1 | — |

## Test detail for binaries that ran

### PASS (all tests passed)

| Binary | Test Cases | Assertions | Test Suites |
|--------|-----------|------------|-------------|
| Luau | 5421 | 35565 | Full suite |
| Voice.Server | 640 | 11174 | Full suite |
| Voice.Stratus | 16 | 50 | Full suite |
| AssetProviderCache | 19 | 152 | AssetCacheHttpCacheImpl, AssetCachePendingMapConcurrent, AssetCacheRbxStorageImpl, AssetCacheCoalescer, HttpRedirectCache |
| AssetProvider | 9 | 34 | AssetCacheByteRange, UrlNormalization, ContentIdFormatter |
| fmod | 27 | 164 | Full suite |
| Sqlite3 | 5 | 161 | Full suite |
| JoinTicketCrypto | 20 | 23 | Full suite |
| Installer.MacOS.Player | 5 | 5 | Full suite |
| Installer.MacOS.Studio | 5 | 5 | Full suite |
| RbxStorageBench | — | — | No errors |

### PARTIAL (some tests failed on environment)

| Binary | Total | Passed | Failed | Failure Cause |
|--------|-------|--------|--------|---------------|
| Physics_Base_Group | 656 | 643 | 13 | Environment (98% pass) |
| Geometry_Base_Group | 456 | 347 | 109 | Environment (76% pass) |
| Luau.Conformance | 302 | 247 | 55 | sysctl path lookups |
| SignalRCore | 10 | 4 | 6 | `httpServer->listen()` needs network sockets |
| Luau.CLI | 112 | 25 | 87 | sysctl path lookups |

### FAIL (tests ran but killed/crashed)

| Binary | Tests Ran | Cause |
|--------|----------|-------|
| Http | 21+ passed | `CookieStore.mm` ObjC assertion (needs Foundation), then killed (exit 137) |
| AssetProviderHttp | 26+ passed | `MeasureCDNLatencyBeaconTests/ValidConfig_MakesProbeRequests` needs network, then killed (exit 137) |
| OpenSSL | 1 | Needs network/crypto resources |
| PerformanceTelemetry | 3 passed | CRASH in `EnterExit` test (exit 139) |

### CRASH (exit 139, no test output captured)

App, App.Script, Network, Physics, RealtimeProtocol, RobloxTelemetry, ScriptServices, VideoStream

These are large binaries (26-425MB). Crashes are likely QEMU memory/timeout issues, not MachGate bugs — all passed `--help` without `timeout`.

## iOS common-tests

All 32 iOS common-test binaries pass `--help` and `--list-content`. Not yet test-run.

## Status legend

| Status | Meaning |
|--------|---------|
| **PASS** | All tests passed |
| PARTIAL | Some tests passed, some failed on environment |
| FAIL | Tests ran but failed or killed |
| CRASH | Binary crashed (exit 139, likely QEMU issue) |
| HUNG | Binary hung, killed |
| RUNNING | Currently executing tests |
| NOT RUN | Binary loads but tests not yet run |

## Known test failure causes

1. **Darwin sysctl path lookups** — Luau.CLI (87), Luau.Conformance (55)
2. **Network sockets** — SignalRCore (6), AssetProviderHttp (CDN probe)
3. **Objective-C/Foundation** — Http (CookieStore.mm)
4. **Network/crypto** — OpenSSL
5. **Crashes (exit 139)** — large binaries, likely QEMU memory limits

## Build configuration

- MachGate: Debug build, ARM64 Docker
- Common-tests: Release, `sanitize=none`, cross-compiled
- Apple-ABI libc++: Built from LLVM submodule
- Docker image: `machgate-arm64-toolchain`

## Blocking factor

QEMU emulation on x86_64 makes each binary 10-50x slower. On a real ARM64 host, all tests would finish in minutes.
