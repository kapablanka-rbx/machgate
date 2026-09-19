# MachGate Test-Driver Fix Loop Plan

Status: approved, pre-execution
Last updated: 2026-09-19

## Objective

Drive every common-test driver binary from its current state to a full
test-suite PASS, using an evidence-driven, bounded fix loop. The loop runs
per crash class, not per binary, and every fix must have a verified root
cause before implementation.

## Current state entering the loop

- 36/36 binaries pass `--help` (load + constructors + `_main`)
- `--list-content` works (Catch2 `--list-content`, doctest
  `--list-test-cases`)
- Fixes already landed on master tonight:
  1. `getpwuid`/`getpwnam` Darwin-layout marshaling in the shim (7 SIGSEGV binaries fixed)
  2. chdir to binary directory before static constructors (App_Group Client-folder abort fixed)
  3. Canonical container paths + rw engine mount in runner scripts (Core EROFS abort fixed)
  4. LSE emulation skipped when host executes atomics natively (`HWCAP_ATOMICS`)
  5. Darwin socket ABI translation in the shim (AF_INET6 30→10, sockaddr `sin_len` layout, SO_* options) — bind verified working via strace
- First full test run: App crashed in `AnalyticsServiceTests` — every
  test constructing a `DataModelFixture` dies (server listener path); the
  socket bind layer of that crash is fixed, next layer pending

## Ground rules

1. **No fix without a verified root cause.** Evidence means: QEMU_STRACE
   signature, crash PC correlated against actual mmap bases, named function
   pair from disassembly, and a guest-source explanation of the full causal
   chain. The getpwuid fix is the model: strace showed
   getuid→nscd→/etc/passwd→close→SIGSEGV; disassembly named strlen/assign;
   guest source showed `pw_dir` at Darwin offset 48 into a 48-byte Linux
   struct.
2. **One fix per iteration.** No shotgun fixes. If two gaps are found,
   fix the one the crash evidence names, re-run, then decide.
3. **Fast reproducers are mandatory when available.** Filtered runs give
   23-second iterations versus 10-15 minute binary loads. Catch2 binaries:
   `--run_test=Suite/Case` (equals-form only — the boost wrapper drops
   space-separated values and silently runs everything). Doctest binaries:
   `--run_test=<name>` is likewise equals-form; the Luau family accepts
   `-tc=<filter>`.
4. **Crash classes, not binaries.** If N binaries crash at the same
   signature (as the 7 formerly-getpwuid-crashing binaries did, and as the
   8 DataModelFixture-constructing binaries do now), the class gets one
   diagnosis and one fix, verified across all affected binaries.
5. **Fix in the correct layer.**
   - Imported libSystem symbol with Darwin ABI → shim (`src/shim/libsystem_shim.c` + `.ver`)
   - Raw `svc #0x80` path → syscall gate (`src/syscall/`)
   - Guest process setup ordering → loader (`src/machgate.c`)
   - Environment (missing files, no network) → runner script mounts/config,
     never a MachGate hack
6. **Every verified fix is its own commit**, in the proven chain style
   (`getpwuid` → `chdir` → `rw-mount` → `socket-translation`), pushed to
   master after verification.
7. **Bounded retries.** Maximum 4 iterations per crash class. After the
   4th failed iteration, mark the class HARD-STOPPER with a complete
   evidence log (strace, disassembly, attempts, failure reasons, next
   suspected subsystem) and move to the next class.
8. **Timeouts on every run.** Host-side `timeout` wrapper (30 min default,
   `TEST_TIMEOUT_SECONDS` overridable), `--ulimit core=0` in container —
   QEMU wedges after guest crashes are known and must not stall the loop.

## Phase 0 — Baseline collection

One full corpus run of `bin/run-all-tests.sh` (timeout-guarded), classifying
every binary:

| Classification | Meaning | Action |
|---|---|---|
| PASS | full suite green | none |
| PARTIAL | suite ran; some tests failed | examine failures — environment vs translation |
| CRASH | deterministic SIGSEGV/abort | enters Phase 2 |
| TIMEOUT | crash-hang or slow | strace to reclassify as CRASH or slow-PASS |

Output: master results table + per-binary logs (`/tmp/machgate-test-logs/`).
Duration: expect 2-4 hours under QEMU; big binaries dominate.

## Phase 1 — Cheap wins first

Fixes already diagnosed by agent research, requiring no crash loop:

1. **Export the implemented-but-hidden CF stubs** — `CFUUIDCreate`,
   `CFUUIDCreateString`, `CFStringGetCString`, `CFRelease`,
   `CFNumberCreate`, `CFDictionary*`, `CFTimeZone*` are implemented in
   `src/shim/libsystem_shim.c` but hidden by `libsystem_shim.ver`. Guests
   importing them get return-0 stubs → uninitialized buffers downstream.
   One `.ver` commit.
2. Re-run CRASH binaries' fast reproducers. This alone may flip several
   classes (the analytics GUID path needs `CFUUIDCreate`).

Phase 1 is executed before Phase 0's full run when the cheapest fixes may
collapse entire crash classes, making the baseline cheaper and truer.

## Phase 2 — The fix loop

```
for each crash class (ordered by affected-binary count, then repro speed):
  for iteration k = 1..4:
    REPRODUCE  — fast filter run where possible; QEMU_STRACE=1 +
                 MACHGATE_TRACE_SIGNALS=1; full-binary run only when no
                 filter exists
    ANALYZE    — 4 parallel agents, proven angle pattern:
                   • Localizer: correlate crash PC vs mmap bases from
                     strace; name the host function pair via disassembly
                   • Source analyst: walk the guest call tree to the Darwin
                     API on the failing path
                   • API auditor: check against the struct-layout gap table
                     (the getpwuid-class audit)
                   • Isolator: reduce to the minimal reproducer test case;
                     measure per-iteration cost
    PROPOSE    — agents present candidate fixes with evidence; when the
                 fix is non-obvious, spawn 2 independent fix-proposers
                 with full context and require each to argue its case
    ADJUDICATE — main agent picks the winning argument on: strength of
                 strace signature, disassembly proof, complete causal-chain
                 explanation, minimal blast radius
    FIX        — implement in the correct layer; rebuild build-arm64
    VERIFY     — fast reproducer:
                   PASS → exit loop for this class
                   CRASH → carry all evidence into iteration k+1
  after 4 failed iterations → HARD-STOPPER: record evidence log,
  affected binaries, attempts, and next suspected subsystem; move on
```

After each class exits the loop (fixed or HARD-STOPPER): run the affected
binaries' **full suites** — the reproducer is for iteration speed, the
full run is the acceptance gate.

### Known crash classes entering Phase 2 (expected order of attack)

1. **DataModelFixture class** — App, App_Group, Network, Physics,
   RealtimeProtocol, ScriptServices, VideoStream, App.Script (all construct
   `DataModelFixture`; 23s reproducer:
   `--run_test=AnalyticsServiceTests/AutoCreateAnalyticsService`).
   Socket-bind layer fixed; next suspected layers: STM TSD key 135 NULL on
   main thread (`pthread_getspecific(135)` → NULL → `std::string`), then CF
   GUID stubs (Phase 1 may pre-fix this).
2. **Partial-failure classes** — Luau.CLI / Luau.Conformance (Darwin
   sysctl path lookups), SignalRCore (`httpServer->listen(0)` — needs
   working loopback sockets; the socket shim may have fixed this),
   OpenSSL (network-dependent tests).

### Known live gaps (from the struct-ABI audit, fix-on-demand when a class's evidence names them)

Imported by the corpus, unshimmed, layouts differ — the same class as
getpwuid, fixed on demand when crash evidence names one:
`getrusage`, `scandir`, `statfs`/`fstatfs`, `getrlimit`/`setrlimit`
(constant shift), `tcgetattr`/`tcsetattr`, `utimes` family (timeval
width), `select` (timeval width), `getcontext` family, `sendmsg`/`recvmsg`
(msghdr), `getnameinfo`, `getifaddrs`, `qsort_r` (thunk/arg order).

## Phase 3 — Verification and bookkeeping

- After every class exits: full binary suite run for each affected binary
- After all classes: full corpus re-run
- Update `docs/COMMON_TESTS_PROGRESS.md` after every verified fix
- Each fix = one commit + push; HARD-STOPPERs get an evidence section in
  the progress doc

## Stop conditions

- Binary/class fixed → next class
- 4 iterations exhausted → HARD-STOPPER, move on
- Environment-only failure → environment fix (mount/config change in
  runner), documented, never a MachGate source hack
- The loop ends when every binary is PASS, PARTIAL-with-environment-causes,
  or HARD-STOPPER with evidence

## Execution mechanics

- **Main agent** (this one): orchestrates phases, spawns agents,
  adjudicates proposals, implements fixes, rebuilds, verifies, commits
- **Sub-agents**: the 4-angle analysis pattern per class; fix-proposers
  when needed; one agent per affected binary for cross-verification when a
  class fix lands
- **User**: kicks off long runs, watches streaming output, approves phase
  transitions

## Session learnings baked into this plan

- QEMU post-crash wedges are routine — timeout guards everywhere, kill
  wedged containers between runs
- binfmt/QEMU registration can drop (`exec format error`) — re-register
  with `docker run --privileged --rm tonistiigi/binfmt --install arm64`,
  verify with `docker run --rm --platform linux/arm64 ubuntu:24.04 uname -m`
- Pipeline exit-status bugs (`| tail; echo $?` reports tail's status) —
  all runners use `PIPESTATUS[0]`; all verification reads the docker exit
  code directly
- Strace files reach hundreds of MB — agents grep, never read whole
- The binary path in `build/buck2/` is a symlink into `buck-out/` with
  absolute host paths — containers must mount the repo at its real path
