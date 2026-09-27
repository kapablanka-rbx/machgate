# App.Internationalization flags-variant triage — 2026-09-27

Investigation of the 101 bad outcomes in App.Internationalization from the
2026-09-27 01:13 recording (`/tmp/machgate-results.json`):

- 63 CRASHED in `.Flags` (CloudLocalizationTableTests 59 + LocalizationServiceTests 4)
- scatter FAILED/TIMEOUT/NOT_STARTED in both variants

## Root causes

### 1. Mach memory-stats stubs returned zero free memory (fixed)

`MemoryStats::freeMemoryBytes()` on Darwin reads
`host_statistics64(HOST_VM_INFO64)` (`vm_statistics64` page counts), and
`usedMemoryBytes()` reads `task_info(TASK_VM_INFO)->phys_footprint` /
`host_page_size`. The shim had:

- `host_statistics64` — memset-zero stub returning KERN_SUCCESS
- `task_info` — KERN_FAILURE stub
- `host_page_size` — not exported at all

So `freeMemoryBytes() == 0` and `usedMemoryBytes() == 0`. Two consequences:

- `LocalizationServiceTests/MemoryPressure_FlagEnabled_EnoughMemory_FieldIsFalse`
  failed in both variants: `free > used * 0` is `0 > 0` = false, so
  `dynamicTranslationDisabledDueToMemory` came out true instead of false
  (LocalizationService.cpp:2342).
- `Base.UnitTest/MemoryStatsTests/FreeMemory` and `/UsedMemory` failed in
  both variants (nonzero asserts).

Fix: real implementations in `src/shim/libsystem_shim.c`:
`host_statistics64(HOST_VM_INFO64)` fills a Darwin-layout
`vm_statistics64` from Linux `sysinfo()` (free/active/inactive/wire/
purgeable/external page counts); `task_info(TASK_VM_INFO)` fills
virtual_size (statm), page_size, resident_size/phys_footprint (VmRSS from
`/proc/self/status`); `host_page_size` returns `_SC_PAGESIZE`.

### 2. Null-EngineContext hard crash is a known engine test bug, not machgate

The 63 `.Flags` CRASHED all trace (gdb + llvm-symbolizer-18) to
`RBXCRASH("NullEngineContextHard")` in `RBX::Object::Object`
(AppCore/Object.cpp:90), reached from
`Creatable::create_DEPRECATED<CloudLocalizationTable>` in exactly two
tests:

- `CloudLocalizationTableTests/TimesoutBeforeManifestIsLoaded`
  (CloudLocalizationTableTest.cpp:216, table created before the
  UnitTestLock, so no thread-local EngineContext)
- `LocalizationServiceTests/TranslationInvalidationOnlyHappensOnceAtInit`
  (LocalizationServiceTest.cpp:1855, same pattern)

`DFFlag::InstanceNullEngineContextHardCrash` is false by default and
forced true by `--fflags=true`, which is why the plain variant only
soft-fails these two (RBXASSERT) while the flags variant aborts the
process. The rotest daemon runs whole groups in one process, so the abort
cascades CRASHED to every not-yet-run case in the group — that is the 59
CloudLocalizationTableTests and the other LocalizationServiceTests crash
entries. Every one of the cascade tests passes standalone under
`--fflags=true` and through daemon churn once the aborting test is
removed.

Both root tests are on Linebacker's auto-disable exclusion list for every
platform, including `darwin/arm64` —
`Client/BuildScripts/rotest/filter_config/linebacker-exclusions.json` —
i.e. they fail the same way on macOS CI and are excluded there too.
Machgate matches macOS behavior; no machgate fix is possible or needed.
The remaining FAILED/timeout scatter (LocaleIdChangeTest_*, DataModel-
InitializesLocalesProperly, PlayerLocaleChanges_*, TimesoutBefore-
ManifestIsLoaded, translation_SendsEntryToStatistics...) is likewise on
the darwin/arm64 exclusion list (engine-side TLS-swap ordering /
use-after-scope test bugs).

`LocalizationFormatTests/FormatStringDateAndTime` (NOT_STARTED in both
variants) passes standalone and through daemon churn; it was scheduler
fallout from earlier daemon deaths.

## Before / after

Before: MemoryPressure_FlagEnabled_EnoughMemory_FieldIsFalse FAILED
(both variants); MemoryStatsTests/FreeMemory + /UsedMemory FAILED (both
variants); 63 .Flags CRASHED (cascade of two excluded engine bugs).

After (with the memory-stats shim fix, installed to /opt/machgate):
- MemoryPressure_FlagEnabled_EnoughMemory_FieldIsFalse — pass, both
  variants
- MemoryPressure_InstanceWithRootLocalizationTable_FieldDefaultsFalse,
  MemoryPressure_FlagDisabled_FieldIsFalse — pass, both variants
- Base MemoryStatsTests/FreeMemory, /UsedMemory — pass, both variants
- 14-case daemon-churn group covering the previously-crashed
  non-excluded cases (IsLoadingInternalTranslationsSettingChangedLua,
  SetUgcLocaleId_EmptyString, TextModificationSettingsTest_TextElongation,
  ClearAllEntries_ClearsLoadedLocaleAssets, LoadAssetTest_*,
  PendingCallbackLimit_*, NoDataModel_FetchFails, FormatStringDateAndTime,
  GetCountryRegionForPlayerAsync_*) — all pass
- TimesoutBeforeManifestIsLoaded and TranslationInvalidationOnlyHappens-
  OnceAtInit still crash with --fflags=true (engine bugs, excluded on all
  platforms; identical on macOS)

machgate suite 35/35; NamedMutex regression (NamedMutex/*) passes.

## Also fixed in this pass

The prior session's uncommitted free-quarantine work retained every
shim-freed allocation until a 64 MB default budget accumulated, so small
frees were never returned to the host allocator and the suite's
host-allocator-reuse assertion in test_libsystem_shim.sh failed
deterministically (35/35 dropped to 34/35). The quarantine is now
opt-in via MACHGATE_FREE_QUARANTINE_MB=<budget>; with the variable
unset, frees pass straight through to the host allocator as before.
