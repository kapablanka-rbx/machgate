# iOS targeted follow-up

The latest complete-run baseline remains **95,131 / 95,486 passed**,
with **271 failures, 14 crashes, 7 timeouts and 63 not started**. This batch
did not run the corpus or establish a new crash total.

| Iteration | Target | Before | After | Outcome |
| --- | --- | --- | --- | --- |
| 1 | Darwin mutex attribute types | 1/4 guest behavior cases passed | 4/4 passed with startup caching disabled, cold and warm; native provider regression passed | Fixed guest import translation. |
| 2 | MPSC concurrent-empty-queue rendezvous | Unmodified MachGate timed out at 12 seconds with six guest CPUs | Modified MachGate also timed out; diagnostic eight-CPU run passed in 21.064 ms | Reproducer isolated; timeout remains unresolved. |

## Mutex repair

Darwin defines ERRORCHECK as 1 and RECURSIVE as 2; Linux reverses those
values. MachGate previously resolved the guest attribute functions directly
to Linux. An explicitly initialized recursive guest mutex therefore failed
to relock; an error-checking mutex incorrectly allowed recursive locking.
Linux also accepted its adaptive mutex type 3, which Darwin rejects.

Guest-only get/set adapters translate these enum values and errors. Native
ELF providers continue calling Linux pthread functions with Linux values.
No private Apple attribute layout is interpreted or copied into a Linux
structure. The fixture uses Apple's public LP64 opaque storage sizes.

The archive has no equivalent get/set adapters. Its broader recursive
static-mutex workaround does not correct explicit guest attribute types.
Apple libpthread and the archive's LLVM libc++ threading support were
checked; source locations are recorded in `APPLE_ABI_REFERENCES.md`.

## MPSC timeout evidence

The exact case is
`Base_Group.UnitTest/MpscTaskQueue_/exactly_one_post_returns_true_from_concurrent_empty_queue`.
Both flags modes timed out in the latest full run; these direct diagnostics
used flags on and the same optimized iOS guest.

The debugger captured the main test still yielding in its `readyCount < 8`
loop. Worker stacks were in STM scheduler switching, yielding and timing
code. The unchanged build also timed out, so the new mutex adapter did not
introduce this reproduction. Setting the existing diagnostic CPU override
to eight let the same test complete with its assertion passing.

Both current MachGate and archive default iOS guests to six CPUs. This
experiment identifies a CPU-count-sensitive rendezvous, not a proven Darwin
translation defect or a scheduler fix. The default CPU policy, test
assertion and timeout were left intact. A source-level investigation of
the engine/STM scheduling behavior is still needed.

## Validation and artifacts

- Isolated build: `build/ios-contract`.
- Fixture build succeeded; MachGate regression suite: **110/110 passed**.
- `git diff --check` passed.
- Evidence directory: `../kapa-prompts/machgate/ios-batch3/`.
- Debugger and direct test logs:
  `../kapa-prompts/machgate/ios-discovery-Base_Group.UnitTest-batch3-mpsc-*.log`.

A concurrent external macOS run was detected. The filtered Gobot launch was
interrupted, and subsequent diagnostics used direct isolated processes.
The live `build/machgate` binary was not rebuilt. No engine source was
changed in this batch.
