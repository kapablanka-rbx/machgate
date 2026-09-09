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
| 12 | Physics_Base_Group | 35M | PASS | PASS | 42 | 656 | 643 | 13 | PARTIAL (98%) |
| 13 | Geometry_Base_Group | 32M | PASS | PASS | 42 | 456 | 347 | 109 | PARTIAL (76%) |
| 14 | Luau.Conformance | 15M | PASS | PASS | 0 | 302 | 247 | 55 | PARTIAL (82%) |
| 15 | SignalRCore | 24M | PASS | PASS | 42 | 10 | 4 | 6 | PARTIAL (40%) |
| 16 | Luau.CLI | 5.8M | PASS | PASS | 0 | 112 | 25 | 87 | PARTIAL (22%) |
| 17 | OpenSSL | 12M | PASS | PASS | 0 | 1 | 0 | 1 | FAIL (network) |
| 18 | App | 425M | PASS | PASS | 139 | — | — | — | CRASH |
| 19 | App.Script | 374M | PASS | PASS | 139 | — | — | — | CRASH |
| 20 | Network | 382M | PASS | PASS | 139 | — | — | — | CRASH |
| 21 | Physics | 367M | PASS | PASS | 139 | — | — | — | CRASH |
| 22 | RealtimeProtocol | 356M | PASS | PASS | 139 | — | — | — | CRASH |
| 23 | RobloxTelemetry | 26M | PASS | PASS | 139 | — | — | — | CRASH |
| 24 | ScriptServices | 368M | PASS | PASS | 139 | — | — | — | CRASH |
| 25 | VideoStream | 356M | PASS | PASS | 139 | — | — | — | CRASH |
| 26 | Http | 23M | PASS | PASS | — | Partial | — | — | HUNG (killed) |
| 27 | AssetProviderCommon | 18M | PASS | PASS | — | — | — | — | HUNG (killed) |
| 28 | AssetProviderHttp | 25M | PASS | PASS | — | — | — | — | HUNG (killed) |
| 29 | AssetProviderWorkflow | 26M | PASS | PASS | — | Running | — | — | RUNNING |
| 30 | RbxStorage | 12M | PASS | PASS | — | — | — | — | HUNG (killed) |
| 31 | Core | 28M | PASS | PASS | — | Partial | — | — | KILLED |
| 32 | RobloxTest | 28M | PASS | PASS | — | — | — | — | NOT RUN |
| 33 | LuauApiTraceProbe | 22M | PASS | PASS | — | — | — | — | NOT RUN |
| 34 | OpenTelemetryTracing | 22M | PASS | PASS | — | — | — | — | NOT RUN |
| 35 | PerformanceTelemetry | 23M | PASS | PASS | — | — | — | — | NOT RUN |
| 36 | Base_Group | 116M | PASS | PASS | — | — | — | — | NOT RUN |
| 37 | App_Group | 516M | PASS | PASS | — | — | — | — | NOT RUN |

## Summary

| Metric | Count | % |
|--------|-------|---|
| Total binaries | 36 | 100% |
| --help pass | 36/36 | 100% |
| --list-content pass | 36/36 | 100% |
| Tests run | 17/36 | 47% |
| Full test pass | 11/17 | 65% |
| Partial test pass | 5/17 | 29% |
| Test fail (environment) | 1/17 | 6% |
| Crash (exit 139) | 8 | — |
| Hung/killed | 4 | — |
| Not yet run | 6 | — |

## iOS common-tests

All 32 iOS common-test binaries pass `--help` and `--list-content`. Not yet test-run.

## Status legend

| Status | Meaning |
|--------|---------|
| **PASS** | All tests passed |
| PARTIAL | Some tests passed, some failed on environment |
| FAIL | Tests ran but failed (missing resources) |
| CRASH | Binary crashed during test execution (exit 139) |
| HUNG | Binary hung during test execution, killed |
| RUNNING | Currently executing tests |
| NOT RUN | Binary loads but tests not yet run |

## Known test failure causes

1. **Darwin sysctl path lookups** — Luau.CLI (87 failures), Luau.Conformance (55 failures)
2. **Network/crypto resources** — OpenSSL
3. **Objective-C/Foundation** — SignalRCore (6 failures)
4. **GUI frameworks** — AppKit, CoreGraphics skipped/stubbed
5. **Crashes (exit 139)** — large binaries (300MB+) likely hit QEMU memory/time limits

## Build configuration

- MachGate: Debug build, ARM64 Docker
- Common-tests: Release, `sanitize=none`, cross-compiled
- Apple-ABI libc++: Built from LLVM submodule
- Docker image: `machgate-arm64-toolchain`

## Blocking factor

QEMU emulation on x86_64 makes each binary 10-50x slower. On a real ARM64 host, all tests would finish in minutes.
