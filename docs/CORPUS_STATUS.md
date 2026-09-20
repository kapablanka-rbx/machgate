# Common-Test Driver Corpus Status

Sweep: 2026-09-20, `bin/run-all-parallel.sh` (16 lanes, 120s stall watchdog,
1800s ceiling), machgate @ 49e027a (thread-exit TLV/TSD destructors +
TLV sizing + ObjC messaging + sockets + pthread_cpu_number_np in).

## Totals

17 PASS · 2 PARTIAL · 17 STALL (of 36 drivers) — wall ~25 min.

## Driver table

| Driver | Verdict | Tests in | Last / hang test | Notes |
|---|---|---|---|---|
| App | STALL | 45 | `AnalyticsService_Client_NotAvailable` | deterministic hang |
| App_Group | STALL* | 238 | `ClassChecks/SetPropertySlots` | 237 pass, then crash at Linebacker-EXCLUDED test + QEMU wedge; see COMMON_TESTS_PROGRESS.md |
| App.Script | STALL | 57 | `AsyncCallbackIncorrectlyRunningInParallelRegressionTest` | deterministic hang |
| AssetProvider | PASS | 9 | — | 34 assertions |
| AssetProviderCache | PASS | 19 | — | 152 assertions |
| AssetProviderCommon | STALL | 1 | `GeneralPollFlowTest` | hangs on FIRST test — PollableHttpRequest poll loop |
| AssetProviderHttp | STALL | 27 | `ValidConfig_MakesProbeRequests` | probe/poll class |
| AssetProviderWorkflow | STALL | 26 | `ShouldSendBatch_BatchSizeLimit` | batch-send/poll class |
| Base_Group | STALL | 636 | `NamedMutex_MultithreadedIncrement` | multithreaded sync hang at #636 |
| Core | STALL | 961 | `IndexerTest` | |
| fmod | PASS | 27 | — | 164 assertions |
| Geometry_Base_Group | PASS | 456 | — | **11,145,387 assertions** |
| Http | STALL | 25 | `UseProtocolNotMigratingCookie` | |
| Installer.MacOS.Player | PASS | 5 | — | |
| Installer.MacOS.Studio | PASS | 5 | — | |
| JoinTicketCrypto | PASS | 20 | — | 23 assertions |
| Luau | STALL? | 0 | (doctest — silent until summary) | **passed 5421/5421 in serial run** — suspected watchdog false reap; rerun with v2 watchdog |
| Luau.CLI | PASS | — | doctest | 112/112 |
| Luau.Conformance | PARTIAL | — | doctest | 301/302 — 1 failed |
| LuauApiTraceProbe | PASS | 1 | — | |
| Network | STALL | 6 | `InitialSend` | network send class |
| OpenSSL | PASS | 1 | — | |
| OpenTelemetryTracing | STALL | 10 | `AllowlistParsingHandlesWhitespaceMultipleAndWildcard` | |
| PerformanceTelemetry | PASS | 9 | — | 197 assertions |
| Physics | STALL | 157 | `meshExtractionPositionVariantWithoutFix` | was fast-FAIL in serial run #3 — behavior differs |
| Physics_Base_Group | PASS | 656 | — | **1,594,175 assertions** — was TIMEOUT pre-destructor-fix |
| RbxStorage | STALL | 113 | `MultiThreadedUsage` | multithreaded class |
| RbxStorageBench | PASS | — | — | |
| RealtimeProtocol | PASS | 17 | — | 62 assertions — from original crash class, now passes |
| RobloxTelemetry | PARTIAL | 200 | — | 193/200 — 7 failed |
| ScriptServices | STALL | 8 | `frontend_can_set_luau_solver_selection_correctly` | |
| SignalRCore | STALL | 5 | `StartStop` | |
| Sqlite3 | PASS | 5 | — | 161 assertions |
| VideoStream | STALL | 21 | `VideoTest_VideoStreamNoVideo` | |
| Voice.Server | PASS | 640 | — | **11,174 assertions** — was TIMEOUT pre-destructor-fix |
| Voice.Stratus | PASS | 16 | — | 50 assertions |

(*) App_Group STALL is the officially-excluded test poisoning the rest of its
suite, not a MachGate hang.

## Hang-class clusters (from hang points)

1. **Network/poll/send**: AssetProviderCommon+Http+Workflow, Network
   (`InitialSend`), Http (`UseProtocolNotMigratingCookie`), SignalRCore
   (`StartStop`) — prime suspect: shim socket `connect`/`poll` semantics.
2. **Multithreaded sync**: RbxStorage (`MultiThreadedUsage`), Base_Group
   (`NamedMutex_MultithreadedIncrement`), App.Script
   (`AsyncCallbackIncorrectlyRunningInParallelRegressionTest`) — suspect:
   futex/ulock wait-wake translation.
3. **App teardown telemetry**: App (`AnalyticsService_Client_NotAvailable`).
4. **One-offs**: Core (`IndexerTest`), Physics (`meshExtraction...`), ScriptServices,
   VideoStream, OpenTelemetryTracing.

## Known measurement caveats

- Stall watchdog v1 (output-based) can reap silent doctest drivers (Luau).
  v2 (no-output AND low-CPU) planned — rerun STALL set to separate real hangs
  from false reaps.
- App_Group's true status: ~237 of its tests pass before the excluded crash.

## History

- Serial run #3 (pre-destructor-fix): 11 PASS / 2 PARTIAL / 13 TIMEOUT / 1 FAIL
- Parallel sweep (post-fix, this table): 17 PASS / 2 PARTIAL / 17 STALL
  — net new passes: Physics_Base_Group, Voice.Server, Voice.Stratus,
  Geometry_Base_Group (ran in parallel lanes), RbxStorageBench, Sqlite3,
  OpenTelemetryTracing→stalled instead... see table.
