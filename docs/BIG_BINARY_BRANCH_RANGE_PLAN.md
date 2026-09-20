# MachGate ±128MB Branch-Range Design Report

Research-only deliverable. No repo files were modified.

## 0. Executive summary

Three independent patchers rewrite single 4-byte guest instructions into a direct ARM64 `b` (imm26, ±128 MiB) to executable "island" pools: the LSE atomics emulator, the Darwin syscall gate, and the function trampoline system. All three depend on an island pool being within 128 MiB of every patch site. The LSE emulator uses **one big pool** guarded by `span + pool_size < 128MB` (src/machgate.c:686) — this guard is mathematically unsatisfiable for __TEXT spans ≥ ~126 MiB (corpus: 117 MiB and 147 MiB binaries), so allocation falls through to the fixed 4 MB adjacent pool and fails ("LSE pool alloc failed", src/machgate.c:907). The syscall gate already solved the same problem with **per-site spread pools + MAP_FIXED_NOREPLACE search** (src/syscall/syscall_gate.c:505-573, worker-B history in docs/external-fix-workers/syscall-islands.md) — it works today only because svc sites are rare (4–83 per corpus binary). The trampoline system has **no fallback at all** (src/trampoline.c:75-83) and shares the same 4 MB adjacent pool, which the LSE pool can exhaust first.

**Recommendation**: Phase 1 generalizes the syscall gate's proven pool-registry strategy to the LSE emulator (and fixes adjacent-pool sharing order). Phase 2 gives the trampoline pool the same fallback. Phase 3 adds a range-unlimited `brk #imm` + SIGTRAP lazy-dispatch fallback for stragglers (both svc and LSE), which removes the hard failure mode entirely. Veneer chains and register-indirect site patching are evaluated and rejected (complexity / impossibility). In-gap placement is a minor opportunistic optimization. The ASan problem is a separate dylib/shadow-memory issue (docs/NEXT_STEPS.md:29-44) and will not be fixed by any of this.

---

## 1. Inventory of the patching machinery

### 1.1 The direct-branch emitters (what patches what)

| System | What it patches | Guest insn | Replacement | Island size | Pool source |
|---|---|---|---|---|---|
| LSE/BCAX emulation (src/isa_emul.c:465-517, driven from src/machgate.c:947-952) | every LSE atomic (LDADD/SWP/CAS/…) and BCAX in executable sections of main exe **and** every Mach-O dylib (src/machgate.c:1027-1089) | 1 insn (4 B) | `b` island (isa_emul.c:488) | ≤24 words LSE / 5 words BCAX (isa_emul.c:222-356, 415-443) | one main pool + per-dylib pools |
| Syscall gate (src/syscall/syscall_gate.c:575-607) | every `svc #0x80` (0xD4001001, syscall_gate.c:51) in executable sections | 1 insn | `b` island (syscall_gate.c:588) | 96 words fixed (syscall_gate.c:397-450) | registry of pools (≤256) |
| Function trampolines (src/trampoline.c:327-354) | symbol entry points in __TEXT (SDL/bgfx/overrides) | ≥1 insn at entry | `b` island (trampoline.c:345-348) | 16 B (ldr x16,#8; br x16; .quad) | one 1 MB pool, no fallback |
| User patch scripts (src/patcher.c:578-598) | config-driven `at` directives | 1 insn | `b`/`bl`, range-validated (patcher.c:591) | n/a | n/a (in-guest targets only) |

The island **contents** are already range-free where it matters: the syscall-gate island reaches the C dispatcher with `ldr x16,[pc]; blr x16` plus a stored 64-bit address (syscall_gate.c:142-146, 419-425, 447-448); trampoline islands use `ldr x16,#8; br x16; .quad addr` (trampoline.c:42-50). **Only the first hop — guest site → island — is a range-limited direct `b`.** This is the crux: any fix must either place islands within 128 MiB of each site, or replace the first-hop encoding.

### 1.2 Pool allocators and placement strategy

**Adjacent tail pool (main exe)** — src/loader.c:117-141, src/loader.h:8-54. The loader reserves `PAGE_ROUNDUP(total segment span) + 4 MB` with one PROT_NONE `mmap()` using the Mach-O vmaddr base as a *hint* (loader.c:123 — the 0x100000000 "4GB" base is a hint, not MAP_FIXED; actual placement gets whatever slide the kernel returns). Segments are then MAP_FIXED-mapped inside the reservation (loader.c:183, 196), and the 4 MB tail after the *last* segment is mprotected RWX as the bump-allocated `machgate_pool_alloc()` pool (loader.h:45-54; MACHGATE_POOL_PADDING = 4 MB, loader.h:11). Consumers, in carve order: LSE main pool first (machgate.c:710, up to the whole 4 MB), trampoline 1 MB (machgate.c:1124-1131), syscall gate 256 KB (syscall_gate.c:616). **The tail sits after the LAST segment** — for a big binary that is ~span-distance from early __TEXT sites, which is exactly the worker-B failure `island at 0x109b12000 too far from 0x1000d7910` (~155 MiB, docs/external-fix-workers/syscall-islands.md:31-37).

**LSE main pool** — src/machgate.c:57-59 (MIN 2 MB, SLACK 64 KB, ARM64_DIRECT_BRANCH_RANGE 128 MB), estimate at machgate.c:640-674 (24 words/site LSE, isa_emul.c:453), placement at machgate.c:676-716: if `span + aligned_size < 128MB` it walks **downward from `text_begin - size`** page-by-page with `MAP_FIXED_NOREPLACE` (up to ~32k mmap attempts worst case), else falls to the adjacent pool. Then `isa_emul_patch()` re-checks range per site and **silently skips** out-of-range sites (isa_emul.c:480-484 — real LSE insns left in guest code → SIGILL on non-LSE hosts). Entire pool is skipped when the host has native atomics (machgate.c:901-911, `host_supports_lse_atomics()`).

**Syscall gate pools** — src/syscall/syscall_gate.c:52-56: 256 KB/pool (682 islands), ≤256 pools, 1 MB search step. Placement: first pool from adjacent tail; then per-site: try any registered pool with space + in range (`find_gate_pool`, 493-503); else search **below** the site then **above** (`allocate_gate_pool_below/above`, 518-573) with MAP_FIXED_NOREPLACE at 1 MB granularity within the 128 MiB window. Failure is a hard abort (patch_instruction 575-584 → machgate.c:1274-1277 aborts the run).

**Dylib pools** — src/dylib_loader.c:140-257: each Mach-O dylib reserves its span + 64 KB tail (DYLIB_POOL_PADDING, dylib_loader.h:12) used for that dylib's LSE islands (machgate.c:1038-1044). Dylib trampolines share the main 1 MB pool.

**Commpage/stack** — src/commpage.c:28-29: guest commpage MAP_FIXED at 0x0FFFFFC000 (just below 4 GiB); guest stack MAP_GROWSDOWN immediately below it (machgate.c:1425-1447). These occupy the top of the low 4 GiB; the rest of low 4 GiB and everything above the guest reservation is free VA for pools on aarch64 Linux (48-bit VA, top-down mmap region).

### 1.3 Exact failure modes

1. **Span ≥ ~126 MiB** (App_Group 147 MB, App 140 MB): guard `span + aligned_size < 128MB` (machgate.c:686) is false for any pool ≥ 2 MB → mmap path skipped → adjacent 4 MB pool → `estimate_main_lse_pool_size` (≥ 2 MB, and ~16.9 MB for 176k sites) fails `machgate_pool_alloc` → "LSE pool alloc failed — LSE atomics will SIGILL" (machgate.c:907). **Unverified in-repo**: the 147/140/117 MB spans and the 176k-site count are from the task brief / external corpus; not present in this checkout.
2. **Span 60–126 MiB with a big LSE estimate** (e.g. 117 MB span + ≥11 MB pool): same guard fails → same warning.
3. **Span ≤ 126 MiB but pool placed after last segment**: adjacent-pool fallback can succeed while early-__TEXT sites are silently skipped by isa_emul.c:481-483 → latent SIGILL.
4. **Adjacent-pool contention**: LSE carve (first, up to 4 MB) can starve the trampoline's 1 MB — trampoline has **no** MAP_FIXED fallback (init_island_pool, trampoline.c:75-83) → all trampoline patching fails (machgate.c:1130 warning).
5. **Trampoline range**: any trampoline site >128 MiB from the 1 MB pool fails per-symbol (trampoline.c:337-341) — same spread-pool fix applies.

---

## 2. Placement math

- `b`/`bl` imm26: ±2^25 words = **±128 MiB** (ARM64_DIRECT_BRANCH_RANGE, machgate.c:59; word-domain check isa_emul.c:481 `±0x02000000` words — consistent).
- Guest reservation = one contiguous PROT_NONE region [B, B+span+4MB) where B ≈ 0x100000000 (hint; loader.c:123). __PAGEZERO (the first 4 GiB of Mach-O vmaddr space) is **skipped** (loader.c:167) — the low 4 GiB is free apart from commpage+stack at the top of it.
- **Single-pool coverage condition** for bump-ordered islands: `span + pool_size ≤ 128 MiB` — exactly the machgate.c:686 guard. For span 147 MiB no single pool can ever work: a pool ending at B+128 MiB covers sites only up to B+128 MiB… sites in the last 19 MiB are unreachable from any pool that also serves B.
- **Two pools always suffice for any span**: a pool *immediately below* `text_begin` reaches every site in [B−128MiB+Z, B+128MiB) (islands at pool top ≈ B), and a pool *immediately above* `text_end` reaches every site in (B+S−128MiB, B+S+128MiB]. Union covers [B, B+S] for arbitrary S. A pool above text_end also sits before the loader's 4 MB tail unless the reservation already extends there — the tail itself is one such pool. So the minimal generalization is: *keep a small pool list, allocate near each site like the syscall gate does*. With 176k LSE sites × 96 B ≈ **16.9 MB** of islands spread over ≤ a handful of 2–8 MB pools, this is modest.
- Host mmap layout: aarch64 Linux containers (48-bit VA, top-down allocation) leave the entire low 4 GiB (minus commpage/stack at 0x0FFFxxxxxx) and the region above the guest reservation free; MAP_FIXED_NOREPLACE probing as in syscall_gate.c:505-516 is the correct collision-avoiding tool. **Edge risk**: `allocate_gate_pool_above` can claim VA the guest later MAP_FIXED-mmaps (guest_vm_track.c tracks guest regions but pools are not registered there). Below-first search order (already the case, syscall_gate.c:569-572) minimizes this; registering pools in the guest-VM tracker would close it.
- Intra-reservation gaps: the reservation is contiguous and segments are MAP_FIXED inside it; inter-segment gaps remain PROT_NONE and could be mprotected RWX as pool space — but for the failing binaries the 147 MiB span is a *single* __TEXT segment, so gaps don't exist where needed.

---

## 3. Approach survey

| Approach | Mechanism | Worst-case per-site cost (176k LSE sites) | Correctness risk | Fit for this codebase |
|---|---|---|---|---|
| **(a) Veneer/thunk chains** (ARM-linker style) | near-site pool holds a 4 B `b` veneer; veneer jumps to the one big far pool with full islands | 4 B near + 96 B far = 100 B/site ≈ 17.6 MB total; extra branch per hit (negligible) | Low, but the veneer pool *still* must be within 128 MiB of each site and within 128 MiB of the main pool — for a 147 MiB span the main pool sits mid-span, fine | Strictly more machinery than (b) for the same total memory; islands are not shareable across sites (each ends with a site-specific `b` back, isa_emul.c:264, 354) so no dedup win. **Rejected as primary**; useful only if near-site space were scarce (it isn't) |
| **(b) Multiple spread pools** (per-site, like syscall gate) | pool registry + per-site `find/allocate near` + range-aware island bump | identical island bytes (~16.9 MB) in several 2–8 MB pools; 0 extra per-site runtime cost | Low — this is the *already shipped, corpus-proven* syscall-gate design (syscall_gate.c:476-573; worker-B: 7 binaries fixed) | **Best fit.** Generalizes an in-repo pattern; estimate_main_lse_pool_size becomes per-window; isa_emul_patch needs a pool-context instead of (cur,end) pair |
| **(c) Register-indirect first hop** (`ldr x16,[pc,#imm]; br x16`) | needs **8 bytes** at the site (ldr+br) plus an 8 B literal within ±1 MiB (imm19) | n/a | **Unsafe**: sites are single guest instructions; expanding to 8 B requires relocating the following instruction, but branch targets inside guest text are unknowable at runtime (no relocation info; jump tables, internal labels). The trampoline `ldr x16,#8` trick (trampoline.c:42) lives *inside islands*, where MachGate controls every byte — it does not solve in-guest expansion | **Rejected** for site patching; already used correctly for island→dispatcher hops (syscall_gate.c:142-146, trampoline.c:42-50) |
| **(d) In-gap placement** | mprotect RWX unused PROT_NONE gaps inside the guest reservation | pool bytes come from already-reserved VA | Low | Only helps multi-segment layouts; the failing binaries are single-big-__TEXT. Keep as opportunistic pre-search optimization (use tail + gaps before MAP_FIXED probing) |
| **(e) Trap-based lazy emulation** | replace site in place with `brk #imm` (4 B, no range limit); SIGTRAP handler reads ucontext, emulates, sets pc+4 | ~1–3 µs per hit (signal round-trip); zero pool bytes; zero range failures | Medium: must preserve/chain debuggers' SIGTRAP; guest code rarely uses `brk` in release CLIs; ucontext read/write of x0–x30/NZCV is mechanical | **Best fallback**: guarantees correctness for any span with no placement math. For the syscall gate (4–83 sites/binary) the µs cost is irrelevant; for LSE, hot atomic loops could slow 10–100× — acceptable as a *straggler-only* fallback, not the primary path |
| (f) qemu-user / kernel-text-patching analogues | qemu relocates *everything* via TCG (no guest-code patching); kernel static-keys patch at boot with stop_machine | n/a | n/a | Not applicable: MachGate executes guest code natively on the CPU; there is no translation layer to absorb range limits |

---

## 4. Ranked recommendation

1. **(b) Spread pools for LSE** — lowest risk, proven pattern in this repo (syscall gate, worker B), zero runtime cost, fixes 117/147 MB binaries on non-LSE hosts. Files: src/isa_emul.c/.h, src/machgate.c, mirror syscall_gate.c:476-573.
2. **(b) Spread pools for trampolines** — same pattern; also fix the adjacent-pool carve order so LSE cannot starve trampoline/gate shares (reserve fixed shares or carve LSE last).
3. **(e) brk/SIGTRAP universal fallback** — converts every remaining "no island pool within B range" from a hard abort/SIGILL into correct slow execution. Needed for: pathological site distributions, MAP_FIXED_NOREPLACE-hostile address spaces, and as the safety net behind (b).
4. **(d) In-gap + search tuning** — opportunistic: try reservation tail/gaps before 1 MB-step probing; consider raising the LSE search step from 4 KB (machgate.c:693) to 1 MB like the gate (syscall_gate.c:56) to bound startup syscalls.
5. **(a) Veneers** — only if a future workload makes near-site pool space scarce; not now.
6. **(c) In-guest ldr/br pairs** — do not pursue; unsafe by construction.

**ASan clarification** (task requirement): the "asan binaries problem" documented at docs/NEXT_STEPS.md:29-44 is a *dylib-mapping and shadow-memory-layout* problem (no mapping for `libclang_rt.asan_osx_dynamic.dylib`; `___asan_shadow_memory_dynamic_address` pointer; Darwin vs Linux shadow layouts). It is **independent** of the ±128 MiB branch-range issue. ASan builds are also bigger (instrumentation), so the two problems co-occur in the same binaries, but fixing branch range will **not** make ASan binaries load, and ASan support needs its own shim/shadow work. The ±128 MiB issue is purely a big-__TEXT-span issue and must be fixed for sanitize=none big binaries regardless.

---

## 5. Phased plan (small scoped commits, failing test first — per AGENTS.md)

**Phase 1 — LSE spread pools (est. 2–4 days)**
1. Test first: extend tests/test_lse_emul.c with a multi-pool case (two mmap'd regions, site range crossing both). Unit-level, no fixture needed.
2. Introduce a pool registry in src/isa_emul.c (struct + find/allocate-near mirroring syscall_gate.c:476-573); change `isa_emul_patch()` signature to take a registry context (src/isa_emul.h); keep the old two-arg behavior for the dylib 64 KB pools initially (machgate.c:1038-1044) by wrapping them as single-pool registries.
3. In src/machgate.c: replace `estimate_main_lse_pool_size` + `allocate_main_lse_pool` with per-window estimates and below-first MAP_FIXED_NOREPLACE allocation (1 MB step). Keep the whole-pool skip on HWCAP_ATOMICS hosts unchanged.
4. Make out-of-range sites a hard startup error (or route to Phase-3 fallback) instead of the silent `continue` at isa_emul.c:483.
5. Corpus gate: rerun the 57-binary external suite + common-tests corpus on a non-LSE host (or forced-emulation env knob) — the knob does not exist today; consider adding `MACHGATE_FORCE_LSE_EMULATION=1` for testability.

**Phase 2 — Trampoline spread pools + pool-share fairness (est. 1–2 days)**
1. Test first: test_trampoline_basic.sh extended with a far-__TEXT trampoline target (small fixture with .space padding is fine at ~130 MB built on demand and deleted — build_fixtures.sh pattern; or a hand-crafted minimal Mach-O with vmsize>filesize so the loader zero-fills (loader.c:178-189) and payload copied in — avoids a 130 MB object).
2. Replace trampoline.c's single 1 MB pool (trampoline.c:53-56, 75-83) with the same registry; add `allocate_gate_pool_near`-style fallback in write_trampoline failure path (trampoline.c:337-341).
3. In src/machgate.c: carve the adjacent pool in fixed shares (e.g. gate 256 KB + trampoline 1 MB reserved *before* LSE), so LSE can never starve them.

**Phase 3 — brk/SIGTRAP universal fallback (est. 3–5 days)**
1. Test first: new fixture `darwin_svc_brk_fallback` — patch failure forced via env knob, assert the syscall still translates through the trap path.
2. Add `brk #imm` encoder (0xD4200000 | imm) to src/arm64_enc.h; a SIGTRAP handler that rebuilds `struct syscall_gate_state` from ucontext_t, calls `syscall_gate_dispatch()` (syscall_gate.c:297), writes back regs + NZCV + pc+4; chain to previous SIGTRAP action otherwise.
3. Same fallback for LSE sites (reuse decode_lse, isa_emul.c:175-220, in C against ucontext).
4. Use only when the spread-pool allocator fails — keeps the fast path fast.
5. Register island pools in the guest-VM tracker (src/syscall/guest_vm_track.c) so guest MAP_FIXED mmap can't collide with above-__TEXT pools.

**Phase 4 — opportunistic polish (est. 1–2 days)**
- In-gap/tail-first placement before MAP_FIXED probing; LSE search step 4 KB → 1 MB; startup logging of pool layout under MACHGATE_VERBOSE.

Total: ~7–15 engineering days.

## 6. Claims not verifiable in this checkout
- __TEXT spans (2.2–147 MB), 117/140/147 MB binaries, and the 176k LSE-site count come from the task brief; the in-repo external corpus (tests/external/arm64_macho_cli_manifest.txt, 27 rows) is small CLI binaries, and the common-tests corpus (docs/COMMON_TESTS_PROGRESS.md) is external with only file sizes listed.
- Host-without-HWCAP_ATOMICS behavior is inferred from code (machgate.c:901-911); no such host is available here.
