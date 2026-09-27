# Network.UnitTest FlagFlip / ClassDesc crash — root cause record (2026-09-27)

## Symptom

~120 bad outcomes in Network.UnitTest, dominated by:

- `NetworkSchema/*` CRASHED (ClassDescTest et al.)
- `NetworkEventAndStressTests/*` CRASHED (72 cases — cascade)
- `FlagFlipNetworkConnectionTests/*` TIMEOUT / FAILED

Reproduces standalone (not a daemon-env issue):

```
machgate Network.UnitTest --run_test=NetworkSchema/ClassDescTest
# SIGSEGV in RBX::Reflection::DescriptorEntryGetKeyStringView::operator()
```

## Root cause

The crash is a **use-after-free read inside the engine's reflection
rebuild**, masked by macOS libmalloc but exposed by glibc:

1. `ClassDescriptor::finalizeHierarchy()` re-runs
   `buildDescriptorByNameMapV2()` for every class after a
   `ReflectionMutateInScope` (test-only reflection mutation).
2. That rebuild clears and re-grows each class's
   `descriptorByName`/`descriptorByNameV2StringView` SplitHash tables.
   The engine's `MemberDescriptorContainerV2` keeps **stale
   ArrayViews over the old SplitHash items arrays** and re-reads them
   while inserting into the new arrays (self-referential rebuild).
3. On macOS, the default malloc zone rarely hands the just-freed
   items array to an unrelated allocation during that window, so the
   stale reads still see valid `DescriptorEntry` data.
4. Under MachGate (glibc backend), the freed chunk is immediately
   reused (unsorted-bin/tcache) and rewritten — by glibc free-list
   metadata, engine `0xee` uninit poison (`RBX::Memory::
   allocateOrReturnNull`), or unrelated data. The stale read then
   copies garbage into the rebuilt map; e.g. `DescriptorEntry.desc`
   becomes `0x107b7befc` — the address of the `""` literal in
   `__TEXT` used as the `DenseHashMap<StringView,...>` empty-key —
   and `DescriptorEntryGetKeyStringView` dereferences it and
   SIGSEGVs at `ldrsb w9, [x8, #23]`.

Hardware-watchpoint trace of the poisoned slot (0xaaaaab9e5770):

```
W val=(nil)            <- machgate_shim_malloc 0-fill
W val=0xeeeeeeee...   <- engine operator new poison fill
W val=(nil)           <- DenseHashTable ctor fills empty keys
W val=0xfffff7f60ab0  <- _int_free writes unsorted-bin fd (chunk freed!)
W val=0xfffff7f611b0  <- _int_malloc arena writes (reuse begins)
W val=(nil)           <- reused allocation 0-fill
W val=0xeeeeeeee...   <- reused allocation poison fill
W val=0x107b7befc     <- resize_and_rehash memcpy copies poisoned entry
```

The freeing caller is `RBX::Reflection::Stat::createRegistry` /
`ClassDescriptor::finalizeHierarchy` via the guest's
`operator delete(void*, size_t)`.

The ~90 NetworkEventAndStressTests CRASHED scatter is a cascade of
the same crash (the daemon dies mid-run; later groups never start).
FlagFlipNetworkConnectionTests TIMEOUTs hang when the crash corrupts
the child-process handshake instead of killing the parent outright.
DFFlagFlip_Manager standalone reaches a genuine engine assertion
(stale flag name in `ScopedFastFlagSetting` via
`buildStableFlagsScope`, recorded previously).

## What was fixed in MachGate (commit e5ebb93)

`shim_free` now runs a FIFO free-quarantine ring that delays
`real_free` for glibc-heap chunks (default 8 GiB budget,
`MACHGATE_FREE_QUARANTINE_MB` to override, 0 disables):

- stale ledger sizes (e.g. bogus guest-CXX-marked records) no longer
  bypass the quarantine; implausible sizes fall back to
  `malloc_usable_size`
- `shim_realloc` no longer calls glibc realloc (which frees the old
  chunk in place); it mallocs, copies, and routes the free through the
  quarantine
- `dlsym(RTLD_NEXT, "malloc"|"free"|...)` from guest code resolves to
  the shim implementations so romem's `libc_default_*` table captures
  the quarantining free (machgate master commits from the concurrent
  fix loop landed the dlsym interpose and the guest CXX allocator
  bridge)

This moves most engine frees onto the quarantining path and fixes the
immediate-reuse poisoning for those. The machgate suite passes 35/35
with the change.

## Remaining gap

`RBX::Memory::deallocate` keeps a private romem libc-pointer table
(`g_romemRealLibc`) that resolves `free` once via `dlsym` at guest
static init. Some static initializers run before the shim's allocator
interpose is consulted (table slot cached raw glibc `free`), so a
fraction of guest frees still bypass the quarantine, and
NetworkSchema/ClassDescTest still crashes when one of those frees owns
the poisoned chunk (the `Stat::createRegistry` vector assign).

Options if this lane is picked up again:

1. Force every indirect free through the shim: scan the mapped
   `__DATA` for the romem table after guest init and rewrite the
   `free` slot to `machgate_shim_free` (the loader already fixes
   allocator GOT slots this way — see `fixup_darwin_allocator_slot`
   in machgate.c).
2. Or interpose at the glibc arena level (custom malloc via
   `LD_PRELOAD`-style arena hooks is not available; a machgate-owned
   ptmalloc front-end would be the heavy hammer).
3. The engine-side latent bug (stale ArrayView across the map rebuild)
   is worth reporting upstream: `MemberDescriptorContainerV2` holds
   views over `descriptorByName` items that `finalizeHierarchy`
   frees and reallocates while the container still walks them.

## Verified

- machgate fixture suite: 35/35 (`bash tests/run_tests.sh`)
- popen regression: PASS (`test_darwin_popen`)
- NetworkSchema/ClassDescTest standalone: still SIGSEGV (remaining gap
  above); watchpoint trace captured with the quarantine in place shows
  the surviving bypass caller.
