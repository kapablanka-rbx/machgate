# MachGate Status — macOS & iOS Common-Tests + TeamCity Pipeline

## Table 1 — macOS (common-tests-macos-arm64-release, 36 drivers)

--help: **36/36 PASS** | --list-content: **36/36 PASS**

| Driver | help | list | Tests | Detail |
|---|---|---|---|---|
| App | PASS | PASS | STALL | hang at AnalyticsService_Client_NotAvailable (#45) |
| App_Group | PASS | PASS | STALL* | 237 pass, crash at Linebacker-excluded SetPropertySlots |
| App.Script | PASS | PASS | STALL | hang at AsyncCallback...RegressionTest (#57) |
| AssetProvider | PASS | PASS | **PASS** | 34 assertions / 9 cases |
| AssetProviderCache | PASS | PASS | **PASS** | 152 / 19 |
| AssetProviderCommon | PASS | PASS | STALL | hang at GeneralPollFlowTest (#1) |
| AssetProviderHttp | PASS | PASS | STALL | ValidConfig_MakesProbeRequests (#27) |
| AssetProviderWorkflow | PASS | PASS | STALL | ShouldSendBatch_BatchSizeLimit (#26) |
| Base_Group | PASS | PASS | STALL | NamedMutex_MultithreadedIncrement (#636) |
| Core | PASS | PASS | STALL | IndexerTest (#961) |
| fmod | PASS | PASS | **PASS** | 164 / 27 |
| Geometry_Base_Group | PASS | PASS | **PASS** | 11,145,387 assertions / 456 cases |
| Http | PASS | PASS | STALL | UseProtocolNotMigratingCookie (#25) |
| Installer.MacOS.Player | PASS | PASS | **PASS** | 5 / 5 |
| Installer.MacOS.Studio | PASS | PASS | **PASS** | 5 / 5 |
| JoinTicketCrypto | PASS | PASS | **PASS** | 23 / 20 |
| Luau | PASS | PASS | STALL? | passes serially (5,421 tests); suspected watchdog false reap |
| Luau.CLI | PASS | PASS | **PASS** | doctest 112/112 |
| Luau.Conformance | PASS | PASS | **PARTIAL** | doctest 301/302 |
| LuauApiTraceProbe | PASS | PASS | **PASS** | no assertions |
| Network | PASS | PASS | STALL | InitialSend (#6) |
| OpenSSL | PASS | PASS | **PASS** | 6 / 1 |
| OpenTelemetryTracing | PASS | PASS | STALL | AllowlistParsing... (#10) |
| PerformanceTelemetry | PASS | PASS | **PASS** | 197 / 9 |
| Physics | PASS | PASS | STALL | meshExtraction... (#157) |
| Physics_Base_Group | PASS | PASS | **PASS** | 1,594,175 assertions / 656 cases |
| RbxStorage | PASS | PASS | STALL | MultiThreadedUsage (#113) |
| RbxStorageBench | PASS | PASS | **PASS** | — |
| RealtimeProtocol | PASS | PASS | **PASS** | 62 / 17 |
| RobloxTelemetry | PASS | PASS | **PARTIAL** | 193/200 (7 failed) |
| ScriptServices | PASS | PASS | STALL | frontend_can_set_luau_solver... (#8) |
| SignalRCore | PASS | PASS | STALL | StartStop (#5) |
| Sqlite3 | PASS | PASS | **PASS** | 161 / 5 |
| VideoStream | PASS | PASS | STALL | VideoTest_VideoStreamNoVideo (#21) |
| Voice.Server | PASS | PASS | **PASS** | 11,174 / 640 |
| Voice.Stratus | PASS | PASS | **PASS** | 50 / 16 |

macOS totals: **17 PASS / 2 PARTIAL / 17 STALL**

---

## Table 2 — iOS (common-tests-ios-arm64-release, 32 drivers)

--help: **32/32 PASS** | --list-content: **32/32 PASS**

| Driver | help | list | Tests | Detail |
|---|---|---|---|---|
| App | PASS | PASS | STALL | same class as macOS |
| App_Group | PASS | PASS | STALL | |
| App.Script | PASS | PASS | STALL | |
| AssetProvider | PASS | PASS | **PASS** | 34 / 9 |
| AssetProviderCache | PASS | PASS | **PASS** | 152 / 19 |
| AssetProviderCommon | PASS | PASS | STALL | GeneralPollFlowTest (#1) |
| AssetProviderHttp | PASS | PASS | STALL | |
| AssetProviderWorkflow | PASS | PASS | STALL | |
| Base_Group | PASS | PASS | STALL | |
| Core | PASS | PASS | STALL | IndexerTest (#959) — same as macOS |
| fmod | PASS | PASS | **PASS** | 164 / 27 |
| Geometry_Base_Group | PASS | PASS | STALL | Tetra (#2) — passes on macOS |
| Http | PASS | PASS | STALL | |
| Luau | PASS | PASS | STALL | doctest silent; suspected false reap |
| LuauApiTraceProbe | PASS | PASS | **PASS** | |
| Luau.CLI | PASS | PASS | **PARTIAL** | doctest 25/112 passed — 87 FAILED (real failures) |
| Luau.Conformance | PASS | PASS | **PARTIAL** | doctest 301/302 |
| Network | PASS | PASS | STALL | |
| OpenSSL | PASS | PASS | STALL | OpenSSLNoConfigFile (#1) — passes on macOS |
| OpenTelemetryTracing | PASS | PASS | STALL | |
| PerformanceTelemetry | PASS | PASS | **PASS** | 197 / 9 |
| Physics | PASS | PASS | STALL | |
| Physics_Base_Group | PASS | PASS | STALL | KDTreeClosestPolygon (#42) — passes on macOS |
| RbxStorage | PASS | PASS | STALL | |
| RbxStorageBench | PASS | PASS | **PASS** | |
| RealtimeProtocol | PASS | PASS | STALL | 0 tests entered — hangs during init — passes on macOS |
| RobloxTelemetry | PASS | PASS | **PARTIAL** | 193/200 |
| RobloxTest | PASS | PASS | SKIP | no .UnitTest binary |
| ScriptServices | PASS | PASS | STALL | |
| SignalRCore | PASS | PASS | STALL | |
| Sqlite3 | PASS | PASS | **PASS** | 161 / 5 |
| VideoStream | PASS | PASS | STALL | |

iOS totals: **7 PASS / 3 PARTIAL / 21 STALL / 1 SKIP**

---

## TeamCity pipeline progress

| Layer | Status |
|---|---|
| gobot task resolution on arm64 | ✓ fixed (os.yaml) |
| pkg sync on arm64 agent | ✓ working (rbox.yaml fixes) |
| buck2 target resolution (242 targets) | ✓ working |
| protoc codegen | ✓ fixed (arch-split + version) |
| grpc plugin codegen | ✓ fixed (arch-split + version) |
| go-pkg tool | ✓ fixed (trampoline) |
| apple-clang host tools (llvm-ar, etc.) | **blocked — x86_64 only** |
| build-mode=remote workaround | Build 76553097 queued |
| rotest + machgate test execution | Not yet reached |
