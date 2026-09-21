# MachGate Local Run Results — macOS & iOS Common-Tests

All runs local (x86_64 host, ARM64 containers via QEMU), machgate build @
`49e027a` (thread-exit TLV/TSD destructors, ObjC messaging, TLV sizing,
Darwin socket translation, TSD split-brain fix, pthread_cpu_number_np).

Probes:
- `--help` / `--list-content` — driver loads, Mach-O parses, dylibs resolve
- Test runs — full suite per driver, parallel sweep (16 lanes, 120s stall
  watchdog), latest post-fix results

## macOS — common-tests-macos-arm64-release (36 drivers)

--help: 36/36 PASS — --list-content: 36/36 PASS

Test totals: 17 PASS / 2 PARTIAL / 17 STALL

| Driver | help | list | Test verdict | Detail |
|---|---|---|---|---|
| App | PASS | PASS | STALL | hang at `AnalyticsService_Client_NotAvailable` (#45) |
| App_Group | PASS | PASS | STALL* | 237 pass, crash at `ClassChecks/SetPropertySlots` (officially Linebacker-excluded) |
| App.Script | PASS | PASS | STALL | hang at `AsyncCallbackIncorrectlyRunningInParallelRegressionTest` (#57) |
| AssetProvider | PASS | PASS | PASS | 34 assertions / 9 cases |
| AssetProviderCache | PASS | PASS | PASS | 152 / 19 |
| AssetProviderCommon | PASS | PASS | STALL | hang at `GeneralPollFlowTest` (test #1 — PollableHttpRequest loop) |
| AssetProviderHttp | PASS | PASS | STALL | `ValidConfig_MakesProbeRequests` (#27) |
| AssetProviderWorkflow | PASS | PASS | STALL | `ShouldSendBatch_BatchSizeLimit` (#26) |
| Base_Group | PASS | PASS | STALL | `NamedMutex_MultithreadedIncrement` (#636) |
| Core | PASS | PASS | STALL | `IndexerTest` (#961) |
| fmod | PASS | PASS | PASS | 164 / 27 |
| Geometry_Base_Group | PASS | PASS | PASS | 11,145,387 assertions / 456 cases |
| Http | PASS | PASS | STALL | `UseProtocolNotMigratingCookie` (#25) |
| Installer.MacOS.Player | PASS | PASS | PASS | 5 / 5 |
| Installer.MacOS.Studio | PASS | PASS | PASS | 5 / 5 |
| JoinTicketCrypto | PASS | PASS | PASS | 23 / 20 |
| Luau | PASS | PASS | STALL? | doctest (silent until summary) — passes serially (5,421 tests); suspected watchdog false reap |
| Luau.CLI | PASS | PASS | PASS | doctest 112/112 |
| Luau.Conformance | PASS | PASS | PARTIAL | doctest 301/302 — 1 failed |
| LuauApiTraceProbe | PASS | PASS | PASS | no assertions |
| Network | PASS | PASS | STALL | `InitialSend` (#6) |
| OpenSSL | PASS | PASS | PASS | 6 / 1 |
| OpenTelemetryTracing | PASS | PASS | STALL | `AllowlistParsingHandlesWhitespaceMultipleAndWildcard` (#10) |
| PerformanceTelemetry | PASS | PASS | PASS | 197 / 9 |
| Physics | PASS | PASS | STALL | `meshExtractionPositionVariantWithoutFix` (#157); fast-FAIL before fixes |
| Physics_Base_Group | PASS | PASS | PASS | 1,594,175 assertions / 656 cases — TIMEOUT before the TLV destructor fix |
| RbxStorage | PASS | PASS | STALL | `MultiThreadedUsage` (#113) |
| RbxStorageBench | PASS | PASS | PASS | — |
| RealtimeProtocol | PASS | PASS | PASS | 62 / 17 — from the original DataModelFixture crash class |
| RobloxTelemetry | PASS | PASS | PARTIAL | 193/200 — 7 failed |
| ScriptServices | PASS | PASS | STALL | `frontend_can_set_luau_solver_selection_correctly` (#8) |
| SignalRCore | PASS | PASS | STALL | `StartStop` (#5) |
| Sqlite3 | PASS | PASS | PASS | 161 / 5 |
| VideoStream | PASS | PASS | STALL | `VideoTest_VideoStreamNoVideo` (#21) |
| Voice.Server | PASS | PASS | PASS | 11,174 / 640 — TIMEOUT before the TLV destructor fix |
| Voice.Stratus | PASS | PASS | PASS | 50 / 16 |

(*) App_Group's STALL is the officially-excluded test poisoning the rest of
its suite, not a MachGate hang — see COMMON_TESTS_PROGRESS.md.

## iOS — common-tests-ios-arm64-release (32 drivers)

--help: 32/32 PASS — --list-content: 32/32 PASS

| Driver | help | list | Test run |
|---|---|---|---|
| App | PASS | PASS | not run |
| App_Group | PASS | PASS | not run |
| App.Script | PASS | PASS | not run |
| AssetProvider | PASS | PASS | not run |
| AssetProviderCache | PASS | PASS | not run |
| AssetProviderCommon | PASS | PASS | not run |
| AssetProviderHttp | PASS | PASS | not run |
| AssetProviderWorkflow | PASS | PASS | not run |
| Base_Group | PASS | PASS | not run |
| Core | PASS | PASS | not run |
| fmod | PASS | PASS | not run |
| Geometry_Base_Group | PASS | PASS | not run |
| Http | PASS | PASS | not run |
| Luau | PASS | PASS | not run |
| LuauApiTraceProbe | PASS | PASS | not run |
| Luau.CLI | PASS | PASS | not run |
| Luau.Conformance | PASS | PASS | not run |
| Network | PASS | PASS | not run |
| OpenSSL | PASS | PASS | not run |
| OpenTelemetryTracing | PASS | PASS | not run |
| PerformanceTelemetry | PASS | PASS | not run |
| Physics | PASS | PASS | not run |
| Physics_Base_Group | PASS | PASS | not run |
| RbxStorage | PASS | PASS | not run |
| RbxStorageBench | PASS | PASS | not run |
| RealtimeProtocol | PASS | PASS | not run |
| RobloxTelemetry | PASS | PASS | not run |
| RobloxTest | PASS | PASS | not run |
| ScriptServices | PASS | PASS | not run |
| SignalRCore | PASS | PASS | not run |
| Sqlite3 | PASS | PASS | not run |
| VideoStream | PASS | PASS | not run |

iOS test suites never executed. iOS binaries link UIKit, CoreGraphics,
Metal, QuartzCore, AudioToolbox, CoreVideo — all currently unmapped in the
dylib map — so test execution is an unknown until run.

## Remaining macOS hang classes

1. Network/poll: AssetProviderCommon+Http+Workflow, Network, Http,
   SignalRCore — suspect shim socket connect/poll semantics
2. Multithreaded sync: RbxStorage, Base_Group, App.Script — suspect
   ulock/futex wait-wake translation
3. App teardown telemetry: App
4. One-offs: Core, Physics, ScriptServices, VideoStream,
   OpenTelemetryTracing
