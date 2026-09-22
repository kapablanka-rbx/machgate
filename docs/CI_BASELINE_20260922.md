# MachGate CI Baseline — game-engine common-tests macos/arm64 via buck2 + rotest --machgate

First complete CI execution on the ARM64 Linux agent.
TC build: Client_SageTeams_Buck2Migration_Buck2Specs_Buck2Native_Buck2machgateCommonTestsArm64
- Build 76610364 (loop 7): full pipeline proven, pass 1 complete, killed by 3h timeout during serial rerun
- Build 76636196 (loop 8): rerun-count=0, complete pass 1 with summary table, cancelled post-summary

Pipeline: buck2 build build-mode=remote materializations=all (14,961 commands, 447 remote, 0 local, 0 failed) -> rotest --machgate -> machgate executes Mach-O test binaries on Linux/ARM64.

## Totals

| Metric | Value |
|--------|-------|
| Tests discovered | 32,375 in 240 drivers |
| Succeeded | 28,484 (88.0%) |
| Failed (assertion) | 945 |
| Crashed | 553 |
| Timed out (30s cap) | 2,393 |
| Drivers fully green | 153 / 240 |

## Failure triage

### Timed out (2,393) — network wait loops

All dominant families block on socket/replication I/O:

| Family | Representative suites | Timeout events |
|--------|----------------------|----------------|
| RakNet streaming / replicator | TagRuleStreamingTest, ReplicatorGcJobV2*, ReplicatorStreamJobV2, ModelStreamingV2*, IntegrityCheckedProcessor* | >1,100 |
| RNA send/receive | RnaSendReceiveTest, FrustumAndFociTest, SocialCounterpartyManagerTest | ~250 |
| HTTP/token | HTTPSuite, TokenHttpRequestTestSuite | ~280 |
| Service loops | MarketplaceServiceTest, TextScraperTest, CloudExecutionServiceTest, UniverseChatMessageManagerTest | ~180 |
## Green drivers
153 of 240 drivers pass every test, including LuauUnitTestsOldSolver (5,421/5,421), Physics.Core (140/140), UI.Styling (422/422), AppCore (227/227), VoroCD (100/100).

| Network.UnitTest | 1951 | 909 | 16 | 3 | 1023 |
| App.Audio.UnitTest | 444 | 128 | 233 | 73 | 10 |
| App.Economy.UnitTest | 755 | 473 | 68 | 0 | 214 |
| Http.UnitTest | 353 | 139 | 84 | 3 | 127 |
| RbxTransportRna.UnitTest | 275 | 97 | 15 | 24 | 139 |
| ScriptServices.UnitTest | 199 | 39 | 0 | 0 | 160 |
| App.Communication.UnitTest | 208 | 48 | 0 | 1 | 159 |
| RbxTransportProtocol.UnitTest | 291 | 152 | 29 | 108 | 2 |
| App.Internationalization.UnitTest | 707 | 574 | 18 | 0 | 115 |
| AssetImportIntegration.UnitTest | 141 | 29 | 0 | 112 | 0 |
| AssetImport.UnitTest | 154 | 50 | 0 | 104 | 0 |
| AppBridge.UnitTest | 512 | 429 | 66 | 2 | 15 |
| App.v8datamodel.UnitTest | 1885 | 1804 | 28 | 4 | 49 |
| RbxTransportIo.UnitTest | 190 | 114 | 36 | 8 | 32 |
| App.UI.UnitTest | 1414 | 1339 | 2 | 0 | 73 |
| App.Ads.UnitTest | 213 | 160 | 32 | 2 | 19 |
| FbxImport.UnitTest | 68 | 17 | 3 | 48 | 0 |
| RbxTransportHttp.UnitTest | 46 | 10 | 36 | 0 | 0 |
| Payments.UnitTest | 89 | 54 | 2 | 0 | 33 |
| CloudExecutionService.UnitTest | 37 | 3 | 0 | 0 | 34 |
| App.generativeAI.UnitTest | 91 | 60 | 31 | 0 | 0 |
| RobloxTest.PhysicsQuickTests.Dual | 321 | 293 | 18 | 2 | 8 |
| RobloxTest.PhysicsQuickTests.Primal | 300 | 276 | 14 | 0 | 10 |
| OAuth2.UnitTest | 27 | 3 | 23 | 0 | 1 |
| VideoStream.UnitTest | 37 | 14 | 21 | 0 | 2 |
| App.Utility.UnitTest | 204 | 181 | 0 | 0 | 23 |
| VoiceChatV2.VoiceApiClient.UnitTest | 41 | 19 | 1 | 0 | 21 |
| EditorSourceService.UnitTest | 82 | 60 | 22 | 0 | 0 |
| App.ExperienceJoin.UnitTest | 53 | 33 | 13 | 0 | 7 |
| AssetImportSdk.UnitTest | 24 | 5 | 0 | 19 | 0 |
| KeyValueStorage.UnitTest | 25 | 11 | 1 | 13 | 0 |
| Base.UnitTest | 537 | 523 | 10 | 4 | 0 |
| AssetProviderWorkflow.UnitTest | 54 | 41 | 0 | 0 | 13 |
| AvatarGeneration.UnitTest | 17 | 5 | 0 | 2 | 10 |
| RealtimeClient.UnitTest | 114 | 103 | 4 | 0 | 7 |
| PerformanceControl.UnitTest | 363 | 352 | 1 | 4 | 6 |
| DataModelPatcher.UnitTest | 95 | 84 | 8 | 0 | 3 |
| App.Input.UnitTest | 346 | 335 | 0 | 0 | 11 |
| RbxNN.UnitTest | 40 | 30 | 9 | 1 | 0 |
| FileWatcher.UnitTest | 15 | 5 | 10 | 0 | 0 |
| EditableMeshData.UnitTest | 389 | 379 | 5 | 0 | 5 |
| App.Script.UnitTest | 1290 | 1280 | 5 | 0 | 5 |
| App.generativeAI.Desktop.UnitTest | 21 | 11 | 10 | 0 | 0 |
| SignalR.UnitTest | 9 | 0 | 9 | 0 | 0 |
| RbxTransportSystem.UnitTest | 109 | 100 | 5 | 1 | 3 |
| LuauLocalDebug.UnitTest | 74 | 65 | 3 | 0 | 6 |
| AssetImportUpload.UnitTest | 45 | 36 | 0 | 0 | 9 |
| RobloxTelemetry.UnitTest | 202 | 195 | 7 | 0 | 0 |
| JsonRpc.UnitTest | 90 | 83 | 7 | 0 | 0 |
| GltfExport.UnitTest | 81 | 74 | 2 | 0 | 5 |
| SignalRCore.UnitTest | 10 | 4 | 0 | 0 | 6 |
| RbxTransportStunPopProbe.UnitTest | 39 | 33 | 0 | 6 | 0 |
| RbxTransportDummyService.UnitTest | 6 | 0 | 6 | 0 | 0 |
| RbxStorage.UnitTest | 310 | 304 | 3 | 2 | 1 |
| Installer.Shared.UnitTest | 6 | 0 | 0 | 6 | 0 |
| AssetProviderHttp.UnitTest | 57 | 52 | 0 | 0 | 5 |
| Session.UnitTest | 105 | 101 | 4 | 0 | 0 |
| Text2Mesh.UnitTest | 3 | 0 | 0 | 0 | 3 |
| PinShortcut.UnitTest | 5 | 2 | 3 | 0 | 0 |
| CreationDBService.UnitTest | 4 | 1 | 0 | 0 | 3 |
| AccountProtocol.UnitTest | 6 | 3 | 3 | 0 | 0 |
| Text.UnitTest | 53 | 51 | 2 | 0 | 0 |
| OpenTelemetryTracing.UnitTest | 45 | 43 | 0 | 0 | 2 |
| LuauConformanceTestsO0 | 302 | 300 | 1 | 0 | 1 |
| LuauConformanceTestsCodegen | 302 | 300 | 1 | 0 | 1 |
| GfxRender.UnitTest | 344 | 342 | 1 | 0 | 1 |
| CSG.UnitTest | 407 | 405 | 1 | 0 | 1 |
| AssetProviderCommon.UnitTest | 50 | 48 | 0 | 0 | 2 |
| AppPlatformQoSEmergency.UnitTest | 14 | 12 | 0 | 0 | 2 |
| App.ExperienceStateCapture.UnitTest | 113 | 111 | 0 | 0 | 2 |
| App.AgeVerification.UnitTest | 17 | 15 | 2 | 0 | 0 |
| ScriptScannerService.UnitTest | 7 | 6 | 1 | 0 | 0 |
| Rupp.UnitTest | 37 | 36 | 1 | 0 | 0 |
| ReflectionChecks.UnitTest | 9 | 8 | 0 | 0 | 1 |
| NetworkCore.UnitTest | 44 | 43 | 1 | 0 | 0 |
| LuauConformanceTestsO2 | 302 | 301 | 1 | 0 | 0 |
| LoadSettings.UnitTest | 82 | 81 | 1 | 0 | 0 |
| LiveScriptingServices.UnitTest | 51 | 50 | 1 | 0 | 0 |
| GltfImport.UnitTest | 43 | 42 | 0 | 0 | 1 |
| FacialAgeEstimationProtocol.UnitTest | 7 | 6 | 0 | 1 | 0 |
| ExternalContentSharing.UnitTest | 6 | 5 | 1 | 0 | 0 |
| EngineContext.UnitTest | 59 | 58 | 1 | 0 | 0 |
| ConnectivityV2.UnitTest | 80 | 79 | 0 | 0 | 1 |
| Call.UnitTest | 6 | 5 | 1 | 0 | 0 |
| App.Tween.UnitTest | 15 | 14 | 1 | 0 | 0 |
| App.RequestOrchestration.UnitTest | 98 | 97 | 1 | 0 | 0 |
| ActivityHistoryEventService.UnitTest | 1 | 0 | 0 | 0 | 1 |
|  Driver  |  Run  |  Passed  |  Failed  |  Crashed  |  TimedOut  |
| --- | --- | --- | --- | --- | --- |
