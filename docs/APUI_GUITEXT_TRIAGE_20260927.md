# App.UI.UnitTest GuiTextTest census triage — 2026-09-27

Census run: `App.UI.UnitTest` spawned 57 processes (peak 54) for 218 groups,
47 cases CRASHED in the `.Flags` variant, 2 FAILED plain
(`GuiTextTest/TextFits_ShouldWorkInGeneral`,
`GuiTextTest/TextTruncate_AtEnd_ShouldWork`), 1 timed out
(`TextFits_ShouldWorkInGeneral`, stale group timeout after the first crash).

## Verdict

Not a MachGate regression. The 47 `.Flags` crashes are one engine-side
hard crash that kills the daemon process mid-group; every not-yet-completed
case in that group is then reported CRASHED. The 2 plain FAILED are the same
engine test bugs in their non-fatal form. The engine fixed this test family
on master; the research branch the census built from predates the fix.

## Root cause chain

1. `GuiTextTest/TextFits_ShouldWorkInGeneral` (and
   `TextTruncate_AtEnd_ShouldWork`) call, in this order, on the plain test
   thread with no DataModel lock held:
   - `dataModel->create<RBX::TextService>()` —
     `ServiceProvider::create` asserts `canCreateService()` in
     `Client/AppCore/include/AppCore/Service.h:200` (the thread does not hold
     the DM write lock). Plain mode: `BaseGlobalFixture::handleDebugAssert`
     reports it as a CHECK failure and the test FAILS (matches census plain
     results and the Sep-22 baseline row "2 failed").
   - `RBX::Creatable::create_DEPRECATED<RBX::ScreenGui>()` — falls back to
     `EngineContext::getThreadLocal()`, which is null because no
     `ScopedEngineContextRef` is alive on that thread
     (`Client/EngineContext/src/EngineContextUseState.cpp:11`).
2. `RBX::Object::Object` sees the null context
   (`Client/AppCore/src/Object.cpp:60-90`). Under `--fflags=true`,
   `DFFlag::InstanceNullEngineContextHardCrash` is forced true (it is not in
   `kFlagOverrideIgnoreList` and its name passes the debug/test/dev prefix
   exclusion in `RBX::Test::isFlagExcluded`), so `RBXCRASH("NullEngineContextHard")`
   fires and aborts the process. Verified in gdb: the flag reads 1 at the
   crash and the backtrace lands in `RBX::Object::Object` via
   `ScreenGui::ScreenGui`.
3. The daemon runs one group per process; the abort takes down the process
   and the remaining ~47 GuiTextTest `.Flags` cases in the group are marked
   CRASHED. Individual re-runs of the "crashed" cases that do not use the
   deprecated pattern pass (PreloadFontReturnsReasonableValues,
   MaxVisibleGraphemes_Basic, LineHeight_FloatingNumber_ShouldCalculateAccurately:
   all pass solo under `--fflags=true`) — confirming group fallout, not 47
   independent defects.

## Evidence it is not a MachGate regression

- Reproduced identically (`RBXCRASH: NullEngineContextHard` from
  `TextTruncate_AtEnd_ShouldWork --fflags=true`) on builds of:
  `4557471` (Sep-22 CI-baseline commit), `4c8158e`, `56d2db9`, `294d8a7`,
  and current master HEAD. The crash is bit-for-bit the same engine abort.
- The Sep-22 baseline recorded App.UI 0 crashed because that census did not
  exercise the flags-on variants of this family (its 73 App.UI timeouts
  swallowed them); the new census runs `flags: both`, exposing the
  pre-existing engine behavior.
- The STM/EngineContext TLS machinery works under MachGate:
  `GuiTextTest/UIScaleTest/AutomaticSizeWithUIScaledTextInUIListLayout`
  takes `DataModel::UnitTestLock`, sets the thread-local EngineContext
  through `ScopedEngineContextRef`, then calls `dataModel->create<TextService>()`
  — it PASSES under `--fflags=true`. `writeRequestingThread ==
  Thread::currentThreadId()` and the `contextThreadLocal` STM slot both
  round-trip correctly.
- Families that avoid the deprecated creation pattern are green under
  `--fflags=true` on current MachGate: GuiImageStyling + GuiTextStyling +
  GuiObjectStyling + FrameStyling + GuiButtonStyling = 110/110 (1499 assertions).

## Engine-side fix (already on master)

`1d91e54ce52c` "Remove FFlag::UseTextEngineModule as true #flag-removal
(#173150)" deleted the exact `dataModel->create<RBX::TextService>()` calls
from TextFits/TextTruncate/and siblings in `Client/App/ui/tests/GuiTextTest.cpp`
(TextService creation moved into TextEngineModule). The research branch
census tree still carries the pre-cleanup tests, so every flags-on run of this
family will keep aborting until the branch picks up that change or cherry-picks
the test-side hunks. No MachGate change can or should alter this: the abort is
the engine's intended hard-crash contract for null-EngineContext construction.

## Repro

```
cd /tmp && timeout 120 env LD_LIBRARY_PATH=/opt/machgate/lib \
  MACHGATE_CONFIG=/tmp/machgate/machgate.conf /opt/machgate/bin/machgate \
  .../App.UI.UnitTest --run_test=GuiTextTest/TextTruncate_AtEnd_ShouldWork --fflags=true
# -> SIGABRT, RBXCRASH: NullEngineContextHard (gdb: RBX::Object::Object <- Instance <- ScreenGui)
```
