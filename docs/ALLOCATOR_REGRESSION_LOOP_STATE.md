# Allocator Regression Loop State

Last updated: 2026-07-02

## Current Problem

The private C++ test runner previously progressed much further through Catch2 execution, but recent allocator/C++ ownership changes regressed it to an early Catch2 `RunContext::assertionEnded` null active-testcase crash while reporting `Memory.cpp:884`.

The failure must be treated as a generic Darwin allocator/libc++ contract problem. MachGate must not contain workload-specific symbols, names, strings, or special cases for private binaries.

## Validation Rules

The external Mach-O corpus is only meaningful under ARM64 Linux:

```sh
docker run --rm --platform linux/arm64 \
  -v "$PWD:/work" \
  -w /work \
  machgate-arm64-toolchain \
  bash -lc '
    cmake --build build-arm64 --parallel
    MACHGATE_SHIM_LIB=/work/build-arm64/libsystem_shim.so bash scripts/build-libcxx.sh
    MACHGATE_RUN_EXTERNAL=1 \
    MACHGATE_EXTERNAL_VERBOSE=1 \
    MACHGATE_TRACE_SIGNALS=1 \
    MACHGATE_TRACE_LCMAIN=1 \
    MACHGATE_EXTERNAL_MAP_LIBCXX=1 \
    BUILD_DIR=/work/build-arm64 \
    MACHGATE_EXTERNAL_MANIFEST=/work/tests/external/all_public_macho_cli_manifest.txt \
    MACHGATE_EXTERNAL_TIMEOUT=120 \
    bash tests/test_external_macho_cli.sh
  '
```

Do not use local `build/machgate` for this corpus on an x86-64 host. That binary is an x86-64 host build and can produce invalid after-`_main` crashes that do not represent the ARM64 Docker target.

Before any external run with `MACHGATE_EXTERNAL_MAP_LIBCXX=1`, verify the mapped libc++ architecture:

```sh
file build-libcxx/lib/libc++.so.1.0 build-libcxx/lib/libc++abi.so.1.0
```

It must be `ARM aarch64` for ARM64 Docker validation.

## Recent Known Good Context

The project docs record that the external corpus was previously healthy:

- `README.md`: original external ARM64 macOS CLI probes documented as passing.
- `docs/PENDING_EXTERNAL_FIX_LOOP_PLAN.md`: latest integrated functional full run recorded as `57 / 57`.
- `docs/RUST_EXPANSION_LIVE_STATUS.md`: ARM64 Docker regression suite recorded as passing after prior fixes.

Use these as historical baselines, but revalidate with the current ARM64 Docker command before claiming a fix.

Important corpus-history clarification:

- The historical `57 / 57` run in `bc3adae` used `tests/external/arm64_macho_cli_manifest.txt` plus the then-current public CLI set. It did not include the later Bitcoin-family C++ test-runner rows.
- `bitcoin-26.2-test` entered the later C++ static-init focused manifest and has no saved clean-exit evidence in the focused logs inspected so far.
- Saved C++ logs show `bitcoin-26.2-test` clearing static initialization, entering `_main`, printing Boost.Test help, and aborting with status `134`. The current symptom is therefore not new for that row, but it is now part of the unified single-gate public corpus and must stay visible.
- The old `3/4 PASS` wording in `docs/CATCH2_STATIC_INIT_EXTERNAL_LOOP.md` meant the `auto_start_dbg` parser symptom was cleared for three rows; it did not mean those binaries exited `0`. The saved console logs show `0/3` and `0/4` clean exits for the Boost abort buckets.

## Current Local Patch Under Test

The current worktree has allocator-related changes in:

- `scripts/build-libcxx.sh`
- `scripts/libcxx_allocator_overlay.c`
- `src/resolver.c`
- `src/shim/libsystem_shim.c`
- `tests/test_libcxx_allocator_overlay.sh`
- `tests/test_libsystem_shim.sh`

The intended direction is:

- one allocator ownership domain for mapped libc++/libc++abi and shim allocations;
- no guest-operator bridge mode flag as a public behavioral switch;
- Mach-O import provider order is part of the contract: special ordinals keep
  their defined behavior, normal dylib ordinals must try the declared mapped
  provider before MachGate compatibility hooks, and C++ allocator hooks are
  last-resort fallbacks only;
- guest-defined C++ operators may win normal Mach-O import binds only through
  provider-first resolver order;
- bundled Apple-ABI libc++/libc++abi C++ operator wrappers may call the guest
  executable's public `operator new/delete` definitions when present, but the
  lookup must reject local, private-external, undefined, and stub-section
  symbols and must try the Mach-O double-underscore C++ nlist spelling;
- the guest C++ bridge covers throwing, nothrow, aligned, array, sized-delete,
  and sized-aligned-delete operator variants that the bundled libc++ overlay
  wraps. Nothrow overlay wrappers must not collapse to the throwing bridge when
  a guest nothrow operator is present;
- bundled Apple-ABI libc++/libc++abi delete wrappers must treat unknown C++
  delete pointers as possible direct guest C++ allocations when a guest deleter
  is configured. Known shim-owned allocation records remain shim-owned; bridge-
  marked guest records and unknown C++ pointers route to the guest deleter;
- bundled Apple-ABI libc++/libc++abi C malloc/free wrappers stay in shim
  ownership and must not bypass shim allocator tracking;
- when a shim guest-operator wrapper forwards to a guest deleter, the guest
  allocator owns both the free and its own accounting retirement; the shim
  ledger must not call `forget_allocation` on that path. The shim ledger only
  retires pointers when the shim actually frees, reallocates, or unmaps them.

This direction passed local allocator-surface tests and the unified public
ARM64 corpus, with the one public EH failure documented below. It still needs
private C++ test-runner validation before release.

## 2026-06-28 ARM64 Validation Snapshot

The host-local `build/` run was invalid because `build/machgate` was x86-64. Ignore any `0/19` result from that path.

Valid Docker/ARM64 run:

```sh
docker run --rm --platform linux/arm64 -v "$PWD:/work" -w /work machgate-arm64-toolchain ...
```

Single-gate result after rebuilding `build-arm64` and rebuilding mapped libc++ as ARM64:

- `tests/external/all_public_macho_cli_manifest.txt`: `52 / 53` passed.
- The only failure is `bitcoin-26.2-test`, status `134`.
- The failure prints Boost.Test help, then libc++abi aborts on uncaught `boost::unit_test::framework::nothing_to_test`.

Focused diagnostic results from before the unified manifest was added:

- `tests/external/arm64_macho_cli_manifest.txt`: `19 / 19` passed.
- `tests/external/cpp_static_init_manifest.txt`: `21 / 22` passed.
- `tests/external/rust_expansion_pass_only_manifest.txt`: `10 / 10` passed.

Log roots:

- `tests/external/logs/all-public-regression-arm64-20260628-231430`
- `tests/external/logs/corpus-regression-arm64-20260628-203459`
- `tests/external/logs/cpp-static-init-regression-arm64-20260628-204510`
- `tests/external/logs/rust-pass-regression-arm64-20260628-204813`

## Required Next Steps

1. Compare private C++ test-runner behavior against the public validation above.
2. If the private runner still regresses while public corpora pass, isolate the private-only allocator path without adding workload-specific code.
3. If bisecting recent allocator tags, use the single public manifest before attempting the private runner again.
4. Do not ship another release until the ARM64 external corpus signal is understood.

## 2026-06-29 Agent Review

External and internal agents agreed on two separate loops:

- Allocator/private loop: the current worktree changes are the right generic
  candidate for the private `Memory.cpp:884` regression. They restore mapped
  libc++/libc++abi C++ operator wrapping to shim allocation ownership and retire
  guest C++ ownership after the guest deleter returns.
- Public corpus loop: the remaining `bitcoin-26.2-test` failure is not an
  allocator signal. It reaches `_main`, prints Boost.Test help, then aborts with
  uncaught `boost::unit_test::framework::nothing_to_test`. This is an EH/catch
  loop centered on native Mach-O `__eh_frame` / LSDA handling.

Local tests run after the allocator changes:

- `BUILD_DIR=/home/kapablanka/repos/machgate/build bash tests/test_libsystem_shim.sh`
- `MACHGATE_ROOT=/home/kapablanka/repos/machgate BUILD_DIR=/home/kapablanka/repos/machgate/build LIBCXX_DIR=/home/kapablanka/repos/machgate/build-libcxx/lib MACHGATE_REQUIRE_LIBCXX_OVERLAY=1 bash tests/test_libcxx_allocator_overlay.sh`
- `bash tests/test_guest_cxx_allocator_bridge_config.sh`

All three passed.

EH experiments that did not fix `bitcoin-26.2-test` and were backed out:

- Deduplicating synthetic FDEs when native `__eh_frame` had the same initial PC.
  It removed no entries for this binary, and the abort stayed identical. Log:
  `tests/external/logs/bitcoin-26-eh-dedup-attempt1`.
- Env-gated native `__eh_frame` registration through `__register_frame` in
  addition to the `_dl_find_object` hook. The experiment ran, but the abort
  stayed identical. Log:
  `tests/external/logs/bitcoin-26-native-register-attempt1`.

## 2026-06-29 Resolver Provider-Order Fix

The resolver had a generic contract violation: C++ allocation/deallocation
operator imports were being checked by a resolver-local allocator hook before
the normal Mach-O import provider was tried. That could bind `_Znwm`, `_ZdlPv`,
and related operators to MachGate compatibility code instead of the import's
declared mapped dylib provider.

The contract is now:

- special ordinals keep their normal lookup behavior;
- normal mapped dylib ordinals try `dlsym(de->handle, ...)`, mangling variants,
  then allocator compatibility fallback, then generic `RTLD_DEFAULT`;
- deferred binds follow the same provider-first order;
- resolver-local C++ allocator hooks are last-resort fallbacks only;
- the fallback helper must not search the main executable.

This is enforced by `tests/test_guest_cxx_allocator_bridge_config.sh`.

Validation after this resolver change:

```sh
bash tests/test_guest_cxx_allocator_bridge_config.sh
BUILD_DIR=/home/kapablanka/repos/machgate/build bash tests/test_libsystem_shim.sh
MACHGATE_ROOT=/home/kapablanka/repos/machgate BUILD_DIR=/home/kapablanka/repos/machgate/build LIBCXX_DIR=/home/kapablanka/repos/machgate/build-libcxx/lib MACHGATE_REQUIRE_LIBCXX_OVERLAY=1 bash tests/test_libcxx_allocator_overlay.sh
cmake --build build --parallel
```

All passed.

ARM64 Docker unified public corpus was also re-run with
`tests/external/all_public_macho_cli_manifest.txt` and mapped ARM64 libc++:

- result: `52 / 53` passed;
- only failure: `bitcoin-26.2-test`, status `134`;
- adjacent C++ rows including `bitcoin-test`, `bitcoin-util`,
  `bitcoin-30.2-test`, `bitcoin-29.2-test`, `bitcoin-28.2-test`,
  `bitcoin-27.2-test`, `bitcoin-25.2-test`, `knots-test`, `qtum-test`,
  and Rust-heavy rows passed.

## 2026-06-29 EH Header Contract Fix

The `bitcoin-26.2-test --help` abort was a generic unwinder contract bug, not
an allocator bug.

The old `_dl_find_object` hook built a merged `.eh_frame_hdr` with `absptr`
encodings for `eh_frame_ptr`, `fde_count`, and table entries. GCC/libgcc does
not use that binary-search table shape. It expects the GNU header contract:

- `eh_frame_ptr`: `DW_EH_PE_pcrel | DW_EH_PE_sdata4`
- `fde_count`: `DW_EH_PE_udata4`
- table entries: `DW_EH_PE_datarel | DW_EH_PE_sdata4`

Because the table was ignored, the unwinder effectively saw only the synthetic
compact-unwind FDE buffer through linear fallback and did not reliably search
the native Mach-O `__eh_frame` rows. That caused Boost.Test's control exception
to escape as uncaught after printing help.

The fix in `src/eh_frame.c` is:

- map MachGate-generated `.eh_frame` and `.eh_frame_hdr` near the Mach-O text
  range so GNU 32-bit relative encodings can represent Mach-O PCs, native FDEs,
  and synthetic FDEs;
- fill `_dl_find_object` using the real glibc `struct dl_find_object`, with
  optional `dlfo_eh_dbase` / `dlfo_eh_count` fields only when the target glibc
  declares them;
- emit `.eh_frame_hdr` in GNU searchable form instead of `absptr` form;
- preserve the generic ARM64 compact-unwind correction that frameless compact
  rows do not save LR on the stack and must describe saved X/D pairs from the
  compact encoding.

Validation:

```sh
docker run --rm --platform linux/arm64 -v "$PWD:/work" -w /work machgate-arm64-toolchain ...
```

- focused `bitcoin-26.2-test --help`: passed.
- Boost abort bucket `tests/external/cpp_static_init_boost_nothing_to_test_abort_manifest.txt`: `4 / 4` passed.
- unified public corpus `tests/external/all_public_macho_cli_manifest.txt`: `53 / 53` passed.

Relevant log roots:

- `tests/external/logs/bitcoin-eh-loop-attempt6`
- `tests/external/logs/boost-bucket-after-ehhdr`
- `tests/external/logs/all-public-after-ehhdr`

## 2026-07-02 Guest Deleter Ownership Correction

`temp.txt` showed the private C++ runner still reaching `_main`, then emitting
the guest `Memory.cpp:884` allocation accounting assertion. Catch2 then crashed
while reporting it because `RunContext::m_activeTestCase` was null. Agent review
confirmed the Catch2 crash is secondary: the primary failure remains the guest
allocator accounting assertion.

The previous allocator note said guest-delete ownership should be retired after
the guest deleter returns. That was too broad. If a shim C++ operator wrapper
forwards to a guest-defined `operator delete`, the guest allocator has already
performed the deallocation and owns its own accounting transition. Calling
`forget_allocation` in the shim after that can make the shim's ownership shadow
disagree with the guest allocator and can hide later ownership evidence.

The corrected generic contract is:

- shim-allocated pointers are retired only by shim free/realloc/munmap paths;
- guest-C++ pointers marked by shim forwarding remain marked only until the
  guest allocator path handles them;
- forwarding to a guest deleter must not additionally retire the shim record.
- custom malloc-zone records must remain live while custom `free`,
  `batch_free`, and `free_definite_size` callbacks execute; the shim may retire
  those records only after the callback returns.

Agent review also found a resolver ordering hole: for allocator-sensitive C++
operators, `RTLD_DEFAULT` can find an arbitrary host C++ runtime provider before
MachGate's allocator fallback runs. The generic resolver contract is now:

- normal mapped dylib provider first;
- for replaceable C++ allocation/deallocation operators, if the mapped provider
  is absent or resolves to a weak ELF definition and the main Mach-O defines an
  exported non-private `N_SECT` symbol for the same operator, bind the guest main
  definition;
- C++ allocator fallback after the guest-main override check;
- generic `RTLD_DEFAULT` only after allocator fallback fails.

This keeps compiler-runtime globals available while preventing host C++ operator
definitions from stealing guest-visible allocation ownership.

This rule is intentionally generic. It uses symbol metadata only:

- Mach-O main definition: `N_SECT`, `N_EXT`, not `N_PEXT`; `N_WEAK_DEF` is
  still eligible because these operators are coalesced replaceable definitions;
- mapped native provider: weak ELF binding detected with `dladdr1(...,
  RTLD_DL_SYMENT)`;
- affected imports: replaceable C++ operator new/delete families only.

## 2026-07-02 Guest C++ Bridge Completion

The allocator bridge must follow the Darwin/libc++ ownership contract without
any private-workload switches:

- mapped Apple-ABI libc++ and libc++abi C allocator wrappers stay shim-owned;
- mapped Apple-ABI libc++ and libc++abi C++ operator wrappers route through the
  guest C++ bridge when the main Mach-O exports real public operator
  definitions;
- guest operator lookup must reject local, private-external, undefined, debug,
  and stub-section symbols;
- guest operator lookup must try both the ordinary mangled name and Mach-O
  double-underscore C++ nlist spelling;
- bridge coverage includes throwing, nothrow, aligned, array, sized-delete, and
  sized-aligned-delete variants;
- unknown C++ delete pointers route to the guest deleter when a guest deleter is
  configured, because direct guest `operator new` calls are not necessarily
  visible to the shim ledger;
- known shim-owned pointers remain shim-owned and are not forwarded to guest
  delete;
- array delete may fall back to scalar guest delete when the binary only exports
  the scalar replacement.

Build and release packaging also has an explicit artifact contract now:

- `scripts/build-libcxx.sh` removes existing libc++ and libc++abi shared-library
  outputs before invoking Ninja so a changed overlay object cannot leave stale
  linked libraries behind;
- the GitHub release workflow extracts the final tarball and runs the libc++
  overlay validation against `machgate/lib` from that tarball before publishing
  the checksum.

Validation on 2026-07-02:

```sh
cmake --build build --parallel
BUILD_DIR=/home/kapablanka/repos/machgate/build bash tests/test_libsystem_shim.sh
MACHGATE_ROOT=/home/kapablanka/repos/machgate \
  BUILD_DIR=/home/kapablanka/repos/machgate/build \
  LIBCXX_DIR=/home/kapablanka/repos/machgate/build-libcxx/lib \
  MACHGATE_REQUIRE_LIBCXX_OVERLAY=1 \
  bash tests/test_libcxx_allocator_overlay.sh
bash tests/test_allocator_export_surface.sh
bash tests/test_guest_cxx_allocator_bridge_config.sh
```

All focused tests passed.

ARM64 Docker unified public corpus:

```sh
docker run --rm --platform linux/arm64 -v "$PWD:/work" -w /work machgate-arm64-toolchain ...
```

- `tests/external/all_public_macho_cli_manifest.txt`: `53 / 53` passed.
- A second pass rebuilt the final resolver object after the stub-rejection
  guard and also completed `53 / 53` with `0` failures.
