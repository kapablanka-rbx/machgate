# MachGate CI crash triage — attack list

Source data: TC builds 76636196 (loop 8, flags=off) and 76672144 (loop 9,
flags=off, full log). Baseline: 32,375 tests, 28,545 passed (88.1%),
942 failed, 548 crashed, 2,358 timed out. Build 76766415 (flags=both) is
running and will expand this list with the Flags=On population.

Config: Client_SageTeams_Buck2Migration_Buck2Specs_Buck2Native_Buck2machgateCommonTestsArm64

## P0 — Memory corruption in syscall writeback

### Symptom A: guest stack canary violations (313 events)

```
*** stack smashing detected ***: terminated
[rotest] Test process crashed (SIGABRT, exit code -6)
```

| Driver | Events |
|--------|--------|
| RbxTransportProtocol.UnitTest | 261 |
| RbxTransportRna.UnitTest | 48 |
| RbxTransportIo.UnitTest | 4 |

The guest's stack protector fires: a translated syscall or shim function
writes past a guest stack buffer. All three drivers are the RakNet/UDP
transport path, so the suspect is socket address marshaling on the
recvfrom / recvmsg / getsockopt / accept paths writing back more bytes
than the guest buffer holds, or a sockaddr struct copy with a fixed
128-byte Darwin size against a shorter guest allocation.

Where to look: src/shim/libsystem_shim.c, socket ABI translation section
(sockaddr marshaling, recvfrom/recvmsg/accept/getsockopt writeback).

### Symptom B: glibc heap corruption asserts (6 events)

```
assertion failed: (old_top == initial_top (av) && old_size == 0)
```

Same corruption class hitting the host malloc metadata instead of the
canary. Investigate together with Symptom A; both point at a length
mismatch in a copyout path.

Reproduce: bin/repro-test.sh against RbxTransportProtocol.UnitTest with
MACHGATE tracing enabled; the failing tests are in the driver's first
suite batch and crash within seconds of startup.

Acceptance: RbxTransportProtocol crashed 108 -> 0; stack smashing events
313 -> 0; expected to also unlock part of the 2,358 timeout bucket
(same transport path).

## P1 — AppKit pasteboard stub returns a bogus object

### Evidence

```
resolver: non-GUI data stub: _NSPasteboardTypeString from AppKit
[rotest] Test process crashed (SIGSEGV, exit code -11)
```

2,624 resolver hits on the single symbol _NSPasteboardTypeString,
followed by SIGSEGV in:

| Driver | Crashed |
|--------|---------|
| AssetImportIntegration.UnitTest | 112 |
| AssetImport.UnitTest | 104 |
| FbxImport.UnitTest | 48 |
| AssetImportSdk.UnitTest | 19 |

Crashing tests include FbxParser_Tests/EnableFeaturesTestIgnoreAnim,
FbxSceneMeshParserTests/GetsMeshTest, ImportDataTest/*,
InstanceBuilderTests/* (the remainder of each crashed suite reports
NOT_STARTED).

The stub hands back a dummy that guest code uses as a real CFString /
NSString object and dereferences it.

Fix: provide a functional _NSPasteboardTypeString (a real
CFString/NSString instance following the existing NSProcessInfo/NSString
class-object pattern in the ObjC messaging shim), or make the resolver
fail the lookup cleanly so callers see a null instead of garbage.

Acceptance: asset-import cluster 283 crashes -> 0.

## P1 — FMOD audio initialization fails (233 assertion failures)

```
FmodManager.cpp (1647): ASSERTION FAILED: result == FMOD_OK
FMODPlaybackChannelGroup.cpp (123): error: in "SoundServiceClientTest/SynchronousDisconnectUndoesConnect"
```

App.Audio.UnitTest: 444 run, 128 passed, 233 failed, 73 crashed.
FMOD's init path returns non-OK under MachGate. Audit the audio entry
points FMOD probes (CoreAudio AudioObject* / AudioUnit /
kAudioDeviceProperty* syscalls and their property-size writebacks) and
verify the sizes copied into guest property-address buffers.

Acceptance: App.Audio failed 233 -> 0 or near.

## P2 — IPv6 listener rejected (HTTP cluster)

```
HttpServer.cpp (863): fatal error: in "HTTPSuite/Misc/HttpServerIpv6Listener": HttpError: InvalidUrl
```

Http.UnitTest: 353 run, 139 passed, 84 failed, 127 timed out. The IPv6
listener path fails; verify sockaddr_in6 marshaling, AF_INET6 socket
creation, and bind/accept for v6 addresses in the socket translation.

Acceptance: Http failed 84 -> 0.

## P2 — Network wait loops (2,358 timeouts)

Dominant families (from timeout-event extraction):

| Family | Suites | Events |
|--------|--------|--------|
| RakNet streaming / replicator | TagRuleStreamingTest, ReplicatorGcJobV2*, ReplicatorStreamJobV2, ModelStreamingV2*, IntegrityCheckedProcessor* | >1,100 |
| RNA send/receive | RnaSendReceiveTest, FrustumAndFociTest, SocialCounterpartyManagerTest | ~250 |
| HTTP/token | HTTPSuite, TokenHttpRequestTestSuite | ~280 |
| Service loops | MarketplaceServiceTest, TextScraperTest, CloudExecutionServiceTest, UniverseChatMessageManagerTest | ~180 |

Driver spans show the tail mechanism: rotest kills a hung test at the
30s cap, restarts the driver, re-runs the remainder, hangs again
("Restarting driver CSG.UnitTest (suite ManifoldRegressionTests) with
79 remaining test(s) after timeout"). Network.UnitTest alone
accumulates 1,023 timeouts.

Suspects, in order:
1. recvfrom/recvmsg blocking on what the guest set as non-blocking
   (fcntl O_NONBLOCK translation on sockets)
2. kqueue ENOSYS -> event loops waiting forever
3. pthread_cond_timedwait Darwin abstime vs Linux relative timespec

Likely overlaps P0: the same transport drivers dominate both.

Acceptance: timeouts 2,358 -> under 500 in the first pass.

## Reproduction and workflow

- Local corpus: the same test binaries run under
  bin/run-all-parallel.sh (macOS set) — RbxTransportProtocol, App.Audio,
  Http, AssetImport drivers are all in the corpus
- Single test: bin/repro-test.sh (isolated reproducer with env-controlled
  tracing)
- Crash diagnostics already in MachGate: SIGSEGV FP-chain walk and exit
  backtrace (src/trampoline.c) — enable via the repro script's trace env
- After each fix: push MachGate, cut a release, bump MACHGATE_VERSION on
  the TC config, trigger the build; compare against
  docs/CI_BASELINE_20260922.md

## Priority order

1. P0 stack/heap corruption — memory-corruption bug in our translation
   layer; may unblock the timeout bucket too
2. P1 pasteboard stub — single symbol, mechanical fix, 283 crashes
3. P1 FMOD — audit property writeback sizes
4. P2 IPv6 + wait loops
