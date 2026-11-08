# iOS repair batch — 8 November 2026

Completed the requested ten-iteration batch. The last complete external run remains
**95,111 / 95,486 passed; 287 failures, 15 crashes, 11 timeouts, 62 not started**.
No new full-corpus run was performed. **The remaining 15 crashes have no measured
end-to-end reduction.** Targeted results below must not be extrapolated to a new
full-run pass count.

## Measured iterations

P = passed, F = failed, C = crashed, T = timed out, NS = not started.
Both flag modes were selected except iteration 2.

| Iteration | Selection | Before | After | Result |
| --- | --- | --- | --- | --- |
| 1 | Cookie secure storage | 0P, 22F, 4C / 26 | 26P / 26 | Added Apple's combined `objc_alloc_init` dispatch. This fix was already included in the latest full-run baseline. |
| 2 | Camera mock, flags off | 1C / 1 | 1C / 1 | GDB found uninitialized `videoFormat` after no frames decode; poisoned rehash size, not real allocation exhaustion. |
| 3 | Tombstone | 31P, 1C / 32 | 30P, 2C / 32 | Queued callback retains `this`; test reset can delete it before callback unlock. No engine patch made. |
| 4 | Animation stream | 8P, 1C, 3 unreported / 12 | 8P, 2C, 2NS / 12 | Engine abort on mismatched DataModel/ThreadLocal EngineContext; test nests two client locks. |
| 5 | Surface controller | 10P, 2C / 12 | 10P, 2C / 12 | Fixed Objective-C exception ABI; original placeholder SIGSEGV is gone, but render assertion escapes worker and triggers engine `UnhandledException`. |
| 6 | MPSC task queue | 26P, 2T / 28 | 28P / 28 | Timeouts did not reproduce; one-CPU debugger check also passed. No queue change. |
| 7 | Async | 37P, 1T / 38 | 38P / 38 | Timeout did not reproduce. No Async change. |
| 8 | CombineAnimationEvents | 2T / 2 | 2T / 2 | Fixed private ulock waiter counts, with eight passing contract tests. Network test still waits for animation-update bit 2 after clients connect and exchange events. |
| 9 | Secured text box reset | 1P, 1T / 2 | 1P, 1T / 2 | Flags-on timeout persists. |
| 10 | Five remaining timeout cases | 5P, 5T / 10 | 7P, 3T / 10 | Subscription and editable-image streaming pass. Audio, rewarded ads and tracker replication still time out with flags on. |

The timeout selections total **69/80 -> 74/80 passing**, with **11 -> 6 reproduced
timeouts**. Five timeouts did not reproduce in isolation; this is not proof that
they are fixed under full-run load. Iteration 1 uses the earlier focused cookie
run; iterations 6–10 use the latest complete-run baseline.

## Verified MachGate changes

- Foundation provider: `objc_alloc_init` allocates and dispatches `init`, matching
  objc4 `NSObject.mm` and LLVM's optimized allocation call generation.
- Foundation provider: Objective-C exception personality, begin/end catch,
  rethrow and terminate use the guest C++ runtime. Objective-C typeinfo uses
  Apple's layout and the vtable consumed by LLVM libcxxabi. A Mach-O regression
  rejects a C++ exception in an Objective-C typed catch, runs `@finally` once,
  and catches the exception through its C++ base class. Both dependency orders
  and disabled/cold/warm startup caches pass.
- Linux user-lock adapter: successful private waits return the number of
  remaining waiters, as XNU requires. Apple libplatform needs a positive return
  to preserve subsequent unfair-lock wakeups. Error/mismatch cleanup and fork
  isolation are tested. The archive also returns zero unconditionally here.
- Release packaging: includes the three providers, ICU and its runtime
  dependencies, notices and licenses. Extracted-package C++/Objective-C checks
  run with the installed runner inside the toolchain container.

These changes preserve the translation/application/provider boundaries. No
Contacts, graphics, video-decoder or game-specific compatibility code was added
to the core. Objective-C throwing and alternative exception handlers are still
unsupported; shared ulock waits still lack a cross-process waiter count.

## Remaining crash evidence

- Camera: missing decoded frames reaches an uninitialized optional format.
- Contacts: mocks instantiate `CNMutableContact`, `CNLabeledValue` and
  `CNPhoneNumber`; their properties are unsupported and yield empty results.
  The test then traps after its checks fail. These classes are absent from the
  archive shim too.
- Surface controller: render counter fails to advance after resume; the failed
  requirement escapes a worker. Fixing unwinding alone does not fix rendering.
- Tombstone: asynchronous object lifetime race described above.
- Animation: DataModel/context mismatch described above.
- ClassChecks and Analytics: latest crash logs contain subsystem-not-started,
  launch-intent and replication-manager assertions. No validated fix in this batch.

Do not equate export inventory parity with API correctness or old archive pass
rates: the current binaries and framework paths differ. No tests were disabled,
no expected outputs were relaxed, and timeout limits were not increased.

## Validation and evidence

- Fixture build: passed.
- MachGate suite: **109/109 passed** after the runtime fixes.
- Installed runner/provider checks: Objective-C exception and ICU/C++ isolation
  passed with packaged dependencies.
- Workflow YAML parsed; staged diff whitespace check passed.
- Filtered TDS used the normal MachGate binary, optimized iOS guests,
  `--worker-type=machgate`, both flag modes and `--rerun-count=0`.
- Existing accumulated MachGate runtime/provider changes from this session are
  included in the push; it is larger than these ten iterations alone.

Detailed traces, GDB output, command logs and `status.json` are retained at
`../kapa-prompts/machgate/ios-batch2/`. Related standalone debugger logs are in
`../kapa-prompts/machgate/ios-discovery-*.log`. The full baseline was copied before
filtered runs overwrote the scheduler trace.

Follow-ups: fix the remaining engine/framework causes within their owning
repositories/providers, reproduce intermittent timeouts under comparable load,
then profile the verified runtime with perf/GDB before claiming speed gains.
Backport candidates for the archive are allocation/init, exception ABI and
private ulock waiter counts, with their behavioral regressions.
