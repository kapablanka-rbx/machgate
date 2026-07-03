# Catch2 / C++ Static Init External Loop

## Goal

Build a focused external Mach-O corpus for C++ test-runner-like binaries that stress `__mod_init_func` static constructors before `LC_MAIN`, matching the user-observed Catch2 binary that hangs/crashes at `mod_init[3/707]` with `pc=NULL`.

## Why This Class Matters

Catch2 test binaries usually register tests through global/static C++ objects. The constructors run synchronously in the main thread from Mach-O `__mod_init_func` before guest `main`. A large Catch2 binary can legitimately have hundreds of entries. Failures here expose C++ runtime, libc++, TLV, missing callback/vtable, and constructor ABI issues.

## Search Findings

- Direct public releases of prebuilt macOS ARM64 Catch2 test runners are rare; most projects build them only as CI artifacts.
- Catch2 upstream publishes source releases and package-manager library artifacts, but no release-asset test runner binary.
- Public C++ macOS ARM64 release binaries with similar constructor pressure are available and are useful analogs.
- Current closest analog from existing public corpus: Kitware CMake, with `__mod_init_func` size `0xa30`, i.e. `326` constructors.
- Other C++ analogs: `protoc` and `duckdb`, each with `5` constructors and TLV sections; `ninja` has only `1` constructor and is a low-pressure control.
- Bitcoin Core 31.0 publishes `libexec/test_bitcoin`, a real macOS ARM64 C++ test runner. It uses `__TEXT,__init_offsets` size `0x3f8`, i.e. `254` initializer offsets, plus `libc++` and `libSystem`.
- Bitcoin Core 25.2 through 30.2 publish additional ARM64 macOS `test_bitcoin` runners. They provide version diversity in the same Boost.Test-heavy family and raise initializer pressure up to `425` entries.
- Bitcoin Knots 29.3 publishes `bin/test_bitcoin`, another Bitcoin-Core-family macOS ARM64 C++ test runner.
- Elements 23.3.3 publishes `bin/test_bitcoin`, another Bitcoin-Core-family macOS ARM64 C++ test runner.
- Dash Core 23.1.4 publishes `bin/test_dash`, a related Boost.Test-style macOS ARM64 C++ test runner.
- Qtum 29.1 publishes `bin/test_qtum`, a large Bitcoin-Core-family test runner with `851` initializer offsets. It is the closest public match so far to the user's 707-constructor Catch2-style binary.
- Groestlcoin 31.0 publishes `libexec/test_groestlcoin`, another Bitcoin-Core-family test runner with `254` initializer offsets.
- Syscoin 5.0.5 publishes ARM64 macOS C++ CLI binaries but no test executable in the tarball, so it is a control row rather than a test-runner row.
- CMake also ships `ctest` in the same upstream macOS universal release archive. It is a real C++ test-driving CLI and passes.
- A generated local fixture `cpp_many_ctors` now covers the exact `707` C++ constructor-count pressure from the user-observed Catch2 binary.

## Candidate Table

| Name | Source | Static constructors | Why included | Current loop status |
|---|---:|---:|---|---|
| `cmake` | Kitware CMake public release | 326 | Closest heavy C++ constructor analog to 707-init Catch2 binary | PASS |
| `ctest` | Kitware CMake public release | 367 | Real C++ test-driving CLI from the same archive | PASS |
| `protoc` | Protocol Buffers public release | 5 | Known constructor-phase crash class; C++/libc++/TLV | PASS |
| `duckdb` | DuckDB public CLI release | 5 | Large C++ binary with huge static-init-related string surface and TLV | PASS |
| `ninja` | Ninja public release | 1 | C++ control binary with minimal constructor pressure | PASS |
| `bitcoin-test` | Bitcoin Core 31.0 public release | 254 init offsets | Real C++ test runner shipped in a macOS ARM64 release tarball | TIMEOUT |
| `bitcoin-util` | Bitcoin Core 31.0 public release | 18 init offsets | Smaller C++ CLI from same release/runtime family | PASS |
| `bitcoin-30.2-test` | Bitcoin Core 30.2 public release | 425 init offsets | Real C++ Boost.Test runner; larger initializer table than 31.0 | TIMEOUT |
| `bitcoin-29.2-test` | Bitcoin Core 29.2 public release | 305 init offsets | Real C++ Boost.Test runner; version-diverse Bitcoin Core row | BOOST-NOTHING-TO-TEST-ABORT |
| `bitcoin-28.2-test` | Bitcoin Core 28.2 public release | 296 init offsets | Real C++ Boost.Test runner; version-diverse Bitcoin Core row | TIMEOUT |
| `bitcoin-27.2-test` | Bitcoin Core 27.2 public release | 276 init offsets | Real C++ Boost.Test runner; version-diverse Bitcoin Core row | TIMEOUT |
| `bitcoin-26.2-test` | Bitcoin Core 26.2 public release | 270 init offsets | Real C++ Boost.Test runner; version-diverse Bitcoin Core row | BOOST-NOTHING-TO-TEST-ABORT |
| `bitcoin-25.2-test` | Bitcoin Core 25.2 public release | 263 constructors | Real C++ Boost.Test runner using `__mod_init_func` rather than `__init_offsets` | TIMEOUT |
| `knots-test` | Bitcoin Knots 29.3 public release | 319 init offsets | Bitcoin-Core-derived C++ test runner | BOOST-NOTHING-TO-TEST-ABORT |
| `knots-util` | Bitcoin Knots 29.3 public release | 11 init offsets | Smaller C++ CLI from same release/runtime family | PASS |
| `elements-test` | Elements 23.3.3 public release | 248 | Bitcoin-Core-derived C++ test runner | TIMEOUT |
| `elements-util` | Elements 23.3.3 public release | 19 | Smaller C++ CLI from same release/runtime family | PASS |
| `dash-test` | Dash Core 23.1.4 public release | 335 init offsets | Bitcoin-Core-derived C++ test runner | TIMEOUT |
| `dash-util` | Dash Core 23.1.4 public release | 9 init offsets | Smaller C++ CLI from same release/runtime family | PASS |
| `syscoin-cli` | Syscoin 5.0.5 public release | 11 init offsets | C++ control row; no test executable in release tarball | PASS |
| `qtum-test` | Qtum 29.1 public release | 851 init offsets | Largest public C++ unit-test runner found so far | BOOST-NOTHING-TO-TEST-ABORT |
| `groestlcoin-test` | Groestlcoin 31.0 public release | 254 init offsets | Bitcoin-Core-derived C++ test runner | TIMEOUT |
| `cpp_many_ctors` | Generated local fixture | 707 | Exact constructor-count pressure fixture; no libc++ dependency | PASS |

## Loop Procedure

1. Run focused manifest under ARM64 Docker with current `build-arm64`.
2. Enable `MACHGATE_EXTERNAL_VERBOSE=1` to print `mod_init[i/N]` progress.
3. Enable `MACHGATE_TRACE_SIGNALS=1` and `MACHGATE_TRACE_LCMAIN=1`.
4. For failures, inspect last `mod_init` line plus `signal.lr` and `signal.lr-4` Mach-O offset diagnostics.
5. Classify failures by constructor index, file offset, signal, missing symbol surface, and whether the failure matches the Catch2 `pc=NULL` pattern.
6. Fix one failure class at a time; rerun focused manifest after each fix.

## Exit Criteria

- Focused public analog corpus passes, or each remaining failure has a concrete constructor index/address/fileoff and a documented owner.
- If the same binary remains failing after 5 fix attempts, mark it `harder-later` with the exact evidence.
- Continue searching for actual public Catch2 test-runner binaries; add any found candidates to this document and to a focused manifest.

## Loop Log

- [x] Created focused plan.
- [x] Added external harness verbose mode via `MACHGATE_EXTERNAL_VERBOSE=1`.
- [x] Run focused C++ static-init manifest attempt 1.
- [x] Attempt 1 `cpp-static-init-attempt1`: ran `cmake`, `protoc`, `duckdb`, and `ninja` under ARM64 Docker with `MACHGATE_EXTERNAL_VERBOSE=1`, `MACHGATE_TRACE_SIGNALS=1`, `MACHGATE_TRACE_LCMAIN=1`, `MACHGATE_EXTERNAL_MAP_LIBCXX=1`, timeout 60s. Result: `4/4` passed. This means generic C++ constructor execution is currently healthy for public analogs up to CMake's 326 constructors. The user Catch2 failure at `mod_init[3/707]` is probably a narrower constructor body/runtime dependency issue, not just constructor-count pressure. Logs: `tests/external/logs/cpp-static-init-attempt1/`.
- [x] Built and ran generated `cpp_many_ctors`: `707/707` constructors completed and the fixture exited `0`.
- [x] Run focused C++ static-init manifest attempt 2 with Bitcoin Core `test_bitcoin` and `bitcoin-util`.
- [x] Added Bitcoin Knots, Elements, Dash, and Syscoin public ARM64 macOS rows after release-asset inspection.
- [x] Run focused C++ static-init manifest attempt 3 with the expanded 14-row corpus.
- [x] Added Bitcoin Core 25.2 through 30.2 public ARM64 macOS `test_bitcoin` rows after checksum/path inspection.
- [x] Run focused C++ static-init manifest attempt 4 with the expanded 22-row corpus.
- [x] Loop K attempt 1 fixed `BOOST_AUTO_START_DBG` by correcting Darwin ctype masks in `__DefaultRuneLocale` / `___maskrune` to match Apple's `<ctype.h>`.
- [x] Loop K fixed the old invalid-name parser symptom for `bitcoin-29.2-test`, `knots-test`, and `qtum-test`; after the Loop L unwind fix these three now print Boost help and abort with `boost::unit_test::framework::nothing_to_test`.
- [x] Loop K split `bitcoin-26.2-test` into a new post-ctype abort symptom: it now prints Boost help and aborts with `boost::unit_test::framework::nothing_to_test`, no longer `invalid_cla_id`.
- [x] Loop L read-only classification for `TIMEOUT_AFTER_MAIN`: the eight focused rows all completed static initialization, entered `_main`, and then consumed CPU until the harness timeout. The shared qemu-strace tail was two futex wakes, sometimes heap `brk` plus `mmap(NULL, 1048576, ...)`, and then no further guest syscalls before the timeout kill. stdout stayed empty. stderr was either empty or stopped before Boost.Test completed its help path. Host `strace` was not useful in this Docker/qemu setup because ptrace was unavailable.
- [x] Loop L experiment 1 added a narrow libc++ mangling fallback from Darwin `_ZNSt3__120__libcpp_atomic_waitEPVKvx` to the host libc++ `_ZNSt3__120__libcpp_atomic_waitEPVKvi`. This changed `bitcoin-test` from missing that bind and timing out silently to resolving all binds and printing the full Boost.Test help, but it still timed out. Logs: `tests/external/logs/cpp-timeout-main-exp1-atomic-wait-20260624/` and `tests/external/logs/cpp-timeout-main-exp1-atomic-wait-30s-20260624/`.
- [x] Loop L experiment 2 replaced the atomic-wait bind with a no-op diagnostic shim. It produced the same timeout shape as experiment 1, so the no-op was rejected and the host-symbol fallback was kept. Logs: `tests/external/logs/cpp-timeout-main-exp2-atomic-wait-noop-20260624/`.
- [x] Loop L experiment 3 used a temporary timeout PC diagnostic. Interrupting `bitcoin-test` after the Boost.Test help path showed native `pc` and guest `lr=0x10000cef4`. Disassembly mapped that return site to the compiler's `_Unwind_Resume`-returned path, immediately before the call that enters `std::terminate`. This narrowed the loop to the libSystem `_Unwind_Resume` forwarding shim rather than syscall blocking or argv. Logs: `tests/external/logs/cpp-timeout-main-exp3-timeout-pc-20260624/`.
- [x] Loop L experiment 4 fixed `_Unwind_Resume` to resolve Linux `libgcc_s.so.1` explicitly and abort if the real unwinder cannot be found or returns. The one-row `bitcoin-test` probe passed. The final eight-row `TIMEOUT_AFTER_MAIN` bucket then passed `8 / 8` without the temporary timeout diagnostic: `bitcoin-test`, `bitcoin-30.2-test`, `bitcoin-28.2-test`, `bitcoin-27.2-test`, `bitcoin-25.2-test`, `elements-test`, `dash-test`, and `groestlcoin-test`. Logs: `tests/external/logs/cpp-timeout-main-exp4-unwind-resume-20260624/`, `tests/external/logs/cpp-timeout-main-exp4-unwind-resume-bucket-20260624/`, and `tests/external/logs/cpp-timeout-main-final-bucket-20260624/`.
- [x] Loop L did not change manifest rows; all runs used temporary manifests derived from the existing focused manifest.
- [x] Current focused C++ state after local rerun: `18 / 22` pass. Remaining bucket is `BOOST_NOTHING_TO_TEST_ABORT` for `bitcoin-29.2-test`, `bitcoin-26.2-test`, `knots-test`, and `qtum-test`. Current logs: `tests/external/logs/cpp-static-init-boost-auto-start-dbg-attempt1/` and `tests/external/logs/cpp-static-init-boost-auto-start-dbg-abort-attempt1/`.
- [x] Added focused static-initializer crash instrumentation for the private
  `mod_init[3/707] pc=NULL` class. With `MACHGATE_TRACE_SIGNALS=1`, MachGate
  now records the current initializer kind/index/total/address in both the
  loader signal diagnostics and the libSystem guest signal dispatcher. It also
  decodes ARM64 `br`/`blr`/`ret` at `lr-4` and prints the target register value,
  which is the key evidence for null function-pointer crashes. Public smoke:
  `qtum-test` SIGILL probe logged `current initializer kind=__init_offsets
  index=850 total=851 address=...`.

## Attempt 1 Results

| Name | Result | Notes |
|---|---|---|
| `cmake` | PASS | 326 constructor analog; no repro |
| `protoc` | PASS | 5 constructor C++/TLV analog; no repro |
| `duckdb` | PASS | 5 constructor large C++ analog; no repro |
| `ninja` | PASS | 1 constructor control; no repro |

## Attempt 2 Results

Command:

```sh
docker run --rm --platform linux/arm64 \
  -v /home/kapablanka/repos/machgate:/work \
  -w /work machgate-arm64-toolchain bash -lc '
    cmake --build build-arm64 --parallel
    MACHGATE_RUN_EXTERNAL=1 \
    MACHGATE_EXTERNAL_VERBOSE=1 \
    MACHGATE_TRACE_SIGNALS=1 \
    MACHGATE_TRACE_LCMAIN=1 \
    MACHGATE_EXTERNAL_MAP_LIBCXX=1 \
    BUILD_DIR=/work/build-arm64 \
    MACHGATE_EXTERNAL_MANIFEST=/work/tests/external/cpp_static_init_manifest.txt \
    MACHGATE_EXTERNAL_LOGS=/work/tests/external/logs/cpp-static-init-attempt2 \
    MACHGATE_EXTERNAL_WORK=/work/tests/external/work/cpp-static-init-attempt2 \
    MACHGATE_EXTERNAL_TIMEOUT=90 \
    bash tests/test_external_macho_cli.sh'
```

Result: `5 / 6` focused public C++ rows passed.

| Name | Result | Notes |
|---|---|---|
| `cmake` | PASS | Reconfirmed; `326` `__mod_init_func` constructors complete and `_main` returns version output. |
| `protoc` | PASS | Reconfirmed C++/TLV analog. |
| `duckdb` | PASS | Reconfirmed large C++ CLI analog. |
| `ninja` | PASS | Reconfirmed low-pressure C++ control. |
| `bitcoin-test` | TIMEOUT | Real Boost.Test C++ test runner. It completes all `254` `__TEXT,__init_offsets` initializers, enters `_main`, then loops until the 90s timeout. `qemu-strace` shows process/system discovery, then repeated `clock_gettime(CLOCK_MONOTONIC)` calls and no stdout before timeout. This is a post-initializer Boost/runtime loop, not a constructor-table crash. Logs: `tests/external/logs/cpp-static-init-attempt2/bitcoin-test.*`. |
| `bitcoin-util` | PASS | Same Bitcoin Core release/runtime family; smaller CLI passes. |

## Attempt 3 Results

Command:

```sh
docker run --rm --platform linux/arm64 \
  -v /home/kapablanka/repos/machgate:/work \
  -w /work machgate-arm64-toolchain bash -lc '
    cmake --build build-arm64 --parallel
    MACHGATE_RUN_EXTERNAL=1 \
    MACHGATE_EXTERNAL_VERBOSE=1 \
    MACHGATE_TRACE_SIGNALS=1 \
    MACHGATE_TRACE_LCMAIN=1 \
    MACHGATE_EXTERNAL_MAP_LIBCXX=1 \
    BUILD_DIR=/work/build-arm64 \
    MACHGATE_EXTERNAL_MANIFEST=/work/tests/external/cpp_static_init_manifest.txt \
    MACHGATE_EXTERNAL_LOGS=/work/tests/external/logs/cpp-static-init-attempt3 \
    MACHGATE_EXTERNAL_WORK=/work/tests/external/work/cpp-static-init-attempt3 \
    MACHGATE_EXTERNAL_TIMEOUT=90 \
    bash tests/test_external_macho_cli.sh'
```

Result: `10 / 14` focused public C++ rows passed. All four real external
Core-family unit-test runners failed after completing static initialization and
entering `_main`; their same-release control binaries passed.

| Name | Result | Notes |
|---|---|---|
| `cmake` | PASS | Reconfirmed. |
| `ctest` | PASS | Real C++ test-driving CLI; `367` `__mod_init_func` constructors complete, then version output. |
| `protoc` | PASS | Reconfirmed. |
| `duckdb` | PASS | Reconfirmed. |
| `ninja` | PASS | Reconfirmed. |
| `bitcoin-test` | TIMEOUT | Completes `254` `__init_offsets`, enters `_main`, then no output before timeout. QEMU trace shows futex wake, heap growth, one `mmap`, then timeout kill. |
| `bitcoin-util` | PASS | Same release control. |
| `knots-test` | BOOST-SETUP-ERROR | Completes `319` `__init_offsets`, enters `_main`, then exits `200` with `Test setup error: Parameter auto_start_dbg has invalid characters in name.` |
| `knots-util` | PASS | Same release control. |
| `elements-test` | TIMEOUT | Completes `248` `__mod_init_func` constructors, enters `_main`, then no output before timeout. |
| `elements-util` | PASS | Same release control. |
| `dash-test` | TIMEOUT | Completes `335` `__init_offsets`, enters `_main`, then no output before timeout. |
| `dash-util` | PASS | Same release control. |
| `syscoin-cli` | PASS | Control row; Syscoin release tarball has no `test_*` executable. |

## Attempt 4 Results

Command:

```sh
docker run --rm --platform linux/arm64 \
  -v /home/kapablanka/repos/machgate:/work \
  -w /work machgate-arm64-toolchain bash -lc '
    cmake --build build-arm64 --parallel
    MACHGATE_RUN_EXTERNAL=1 \
    MACHGATE_EXTERNAL_VERBOSE=1 \
    MACHGATE_TRACE_SIGNALS=1 \
    MACHGATE_TRACE_LCMAIN=1 \
    MACHGATE_EXTERNAL_MAP_LIBCXX=1 \
    BUILD_DIR=/work/build-arm64 \
    MACHGATE_EXTERNAL_MANIFEST=/work/tests/external/cpp_static_init_manifest.txt \
    MACHGATE_EXTERNAL_LOGS=/work/tests/external/logs/cpp-static-init-attempt4 \
    MACHGATE_EXTERNAL_WORK=/work/tests/external/work/cpp-static-init-attempt4 \
    MACHGATE_EXTERNAL_TIMEOUT=45 \
    bash tests/test_external_macho_cli.sh'
```

Result: `10 / 22` focused public C++ rows passed. The 12 failures are all real
C++ test-runner rows; all same-release utility/control rows still pass.

| Name | Result | Notes |
|---|---|---|
| `cmake` | PASS | Reconfirmed heavy C++ CLI control. |
| `ctest` | PASS | Reconfirmed real C++ test-driving CLI. |
| `protoc` | PASS | Reconfirmed. |
| `duckdb` | PASS | Reconfirmed. |
| `ninja` | PASS | Reconfirmed. |
| `bitcoin-test` | TIMEOUT | Completes `254` init offsets, enters `_main`, then times out. |
| `bitcoin-util` | PASS | Same Bitcoin Core 31.0 release control. |
| `bitcoin-30.2-test` | TIMEOUT | Completes `425` init offsets, enters `_main`, then times out. |
| `bitcoin-29.2-test` | BOOST-SETUP-ERROR | Completes `305` init offsets, enters `_main`, exits `200`: `Parameter auto_start_dbg has invalid characters in name.` |
| `bitcoin-28.2-test` | TIMEOUT | Completes `296` init offsets, enters `_main`, then times out. |
| `bitcoin-27.2-test` | TIMEOUT | Completes `276` init offsets, enters `_main`, then times out. |
| `bitcoin-26.2-test` | BOOST-SETUP-ABORT | Completes `270` init offsets, enters `_main`, then aborts through libc++ after uncaught `boost::runtime::invalid_cla_id` with the same `auto_start_dbg` message. |
| `bitcoin-25.2-test` | TIMEOUT | Completes `263` `__mod_init_func` constructors, enters `_main`, then times out. |
| `knots-test` | BOOST-SETUP-ERROR | Completes `319` init offsets, enters `_main`, exits `200` with the same `auto_start_dbg` message. |
| `knots-util` | PASS | Same release control. |
| `elements-test` | TIMEOUT | Completes `248` constructors, enters `_main`, then times out. |
| `elements-util` | PASS | Same release control. |
| `dash-test` | TIMEOUT | Completes `335` init offsets, enters `_main`, then times out. |
| `dash-util` | PASS | Same release control. |
| `syscoin-cli` | PASS | Control row. |
| `qtum-test` | BOOST-SETUP-ERROR | Completes `851` init offsets. During initialization it hits a handled `SIGILL` probe at fileoff `0x12df9e8`, instruction `0xd53be020`; after `_main` it exits `200` with the same `auto_start_dbg` message. |
| `groestlcoin-test` | TIMEOUT | Completes `254` init offsets, enters `_main`, then times out. |

Failure buckets:

| Bucket | Count | Binaries | Evidence |
|---|---:|---|---|
| `TIMEOUT_AFTER_MAIN` | 0 active, 8 fixed | `bitcoin-test`, `bitcoin-30.2-test`, `bitcoin-28.2-test`, `bitcoin-27.2-test`, `bitcoin-25.2-test`, `elements-test`, `dash-test`, `groestlcoin-test` | Fixed by libc++ atomic-wait mangling fallback plus `_Unwind_Resume` forwarding to `libgcc_s.so.1`; current bucket rerun passes `8 / 8`. |
| `BOOST_AUTO_START_DBG` | 0 active, 3 superseded | `bitcoin-29.2-test`, `knots-test`, `qtum-test` | Old invalid `auto_start_dbg` parser error fixed by Darwin ctype masks; these rows now belong to `BOOST_NOTHING_TO_TEST_ABORT`. |
| `BOOST_AUTO_START_DBG_ABORT` | 0 active, 1 superseded | `bitcoin-26.2-test` | Old uncaught `boost::runtime::invalid_cla_id` parser failure fixed by Darwin ctype masks; this row now belongs to `BOOST_NOTHING_TO_TEST_ABORT`. |
| `BOOST_NOTHING_TO_TEST_ABORT` | 4 active | `bitcoin-29.2-test`, `bitcoin-26.2-test`, `knots-test`, `qtum-test` | Prints Boost help/usage for `--help`, then aborts with uncaught `boost::unit_test::framework::nothing_to_test`. Current rerun: `0 / 3` for old Boost bucket and `0 / 1` for abort bucket. |

Argv probe:

- Tried `-h`, `--help`, `--version`, `--list_content=DOT`, `--list_content=HRF`, `--list_test_units=DOT`, `--list_labels`, and `--log_level=nothing --report_level=no --run_test=no_such_test` against the original four test-runner failures.
- Result: no argv-only change made them pass. `bitcoin-test`, `elements-test`, and `dash-test` still timed out; `knots-test` still exited `200` with the `auto_start_dbg` message.
- Logs: `tests/external/logs/cpp-static-init-argv-matrix/`.

## BOOST_AUTO_START_DBG Fix Loop

Primary-source check:

- Boost.Test registers `auto_start_dbg` as a built-in runtime option and
  validates command-line parameter names with `std::isalnum(c) || c == '+' ||
  c == '_' || c == '?'`.
- Apple's current `_ctype.h` defines `_CTYPE_A`/`_CTYPE_D` at `0x100` and
  `0x400`, not the low-byte values that were in `libsystem_shim.c`.
- The failing binaries import `___maskrune` and `__DefaultRuneLocale`, so bad
  Darwin ctype flags make Boost reject letters in its own option name.

Sources:

- <https://raw.githubusercontent.com/boostorg/test/boost-1.84.0/include/boost/test/utils/runtime/parameter.hpp>
- <https://raw.githubusercontent.com/apple-oss-distributions/Libc/main/include/_ctype.h>
- <https://raw.githubusercontent.com/simonbyrne/apple-libm/master/Source/ARM/math.h>

Attempt 1, Darwin ctype flag constants:

Hypothesis: Boost's runtime parameter-name validation is seeing all alphabetic
characters as non-alnum because MachGate's `_DefaultRuneLocale.__runetype`
uses obsolete low-byte `_CTYPE_*` masks.

Change:

- Updated `src/shim/libsystem_shim.c` `_CTYPE_*` constants to Apple's current
  masks and marked printable graph characters with `_CTYPE_R`.

Smallest targeted test:

```sh
docker run --rm --platform linux/arm64 \
  -v /home/kapablanka/repos/machgate:/work \
  -w /work machgate-arm64-toolchain bash -lc '
    set -e
    cmake --build build-arm64 --parallel
    grep "^bitcoin-29.2-test|" tests/external/cpp_static_init_manifest.txt > /tmp/boost-auto-start-one.txt
    MACHGATE_RUN_EXTERNAL=1 \
    MACHGATE_EXTERNAL_VERBOSE=1 \
    MACHGATE_TRACE_SIGNALS=1 \
    MACHGATE_TRACE_LCMAIN=1 \
    MACHGATE_EXTERNAL_MAP_LIBCXX=1 \
    BUILD_DIR=/work/build-arm64 \
    MACHGATE_EXTERNAL_MANIFEST=/tmp/boost-auto-start-one.txt \
    MACHGATE_EXTERNAL_LOGS=/work/tests/external/logs/boost-auto-start-dbg-attempt1-bitcoin29 \
    MACHGATE_EXTERNAL_WORK=/work/tests/external/work/boost-auto-start-dbg-attempt1-bitcoin29 \
    MACHGATE_EXTERNAL_TIMEOUT=45 \
    bash tests/test_external_macho_cli.sh'
```

Historical note: this single-row smoke was recorded as `bitcoin-29.2-test`
PASS, `1/1`, but it is superseded by the saved bucket logs below. Do not use it
as current clean-exit evidence.

Bucket test after attempt 1:

```sh
docker run --rm --platform linux/arm64 \
  -v /home/kapablanka/repos/machgate:/work \
  -w /work machgate-arm64-toolchain bash -lc '
    set -e
    cmake --build build-arm64 --parallel
    grep -E "^(bitcoin-29.2-test|bitcoin-26.2-test|knots-test|qtum-test)\\|" tests/external/cpp_static_init_manifest.txt > /tmp/boost-auto-start-bucket.txt
    MACHGATE_RUN_EXTERNAL=1 \
    MACHGATE_EXTERNAL_VERBOSE=1 \
    MACHGATE_TRACE_SIGNALS=1 \
    MACHGATE_TRACE_LCMAIN=1 \
    MACHGATE_EXTERNAL_MAP_LIBCXX=1 \
    BUILD_DIR=/work/build-arm64 \
    MACHGATE_EXTERNAL_MANIFEST=/tmp/boost-auto-start-bucket.txt \
    MACHGATE_EXTERNAL_LOGS=/work/tests/external/logs/boost-auto-start-dbg-attempt1-bucket \
    MACHGATE_EXTERNAL_WORK=/work/tests/external/work/boost-auto-start-dbg-attempt1-bucket \
    MACHGATE_EXTERNAL_TIMEOUT=45 \
    bash tests/test_external_macho_cli.sh'
```

Result: the old `auto_start_dbg` invalid-name exception was cleared for
`bitcoin-29.2-test`, `knots-test`, and `qtum-test`, but the bucket did not pass
cleanly. The saved logs under
`tests/external/logs/cpp-static-init-boost-auto-start-dbg-attempt1/` show
`0/3` clean exits, all status `134`. `bitcoin-26.2-test` also still aborts with
status `134`; after the ctype fix it prints Boost help and aborts after uncaught
`boost::unit_test::framework::nothing_to_test`.

Attempt 2, missing `___fpclassifyd` bind in `bitcoin-26.2-test`:

Hypothesis: the remaining 26.2 abort might be influenced by its one unresolved
libSystem bind, `___fpclassifyd`.

Change:

- Added `__fpclassifyd` to `src/shim/libsystem_shim.c` and exported it from
  `src/shim/libsystem_shim.ver`, returning Apple/OpenLibm ARM `FP_*`
  constants explicitly.

Smallest targeted test:

```sh
docker run --rm --platform linux/arm64 \
  -v /home/kapablanka/repos/machgate:/work \
  -w /work machgate-arm64-toolchain bash -lc '
    set -e
    cmake --build build-arm64 --parallel
    grep "^bitcoin-26.2-test|" tests/external/cpp_static_init_manifest.txt > /tmp/boost-auto-start-26.txt
    MACHGATE_RUN_EXTERNAL=1 \
    MACHGATE_EXTERNAL_VERBOSE=1 \
    MACHGATE_TRACE_SIGNALS=1 \
    MACHGATE_TRACE_LCMAIN=1 \
    MACHGATE_EXTERNAL_MAP_LIBCXX=1 \
    BUILD_DIR=/work/build-arm64 \
    MACHGATE_EXTERNAL_MANIFEST=/tmp/boost-auto-start-26.txt \
    MACHGATE_EXTERNAL_LOGS=/work/tests/external/logs/boost-auto-start-dbg-attempt2-bitcoin26-fpclassify \
    MACHGATE_EXTERNAL_WORK=/work/tests/external/work/boost-auto-start-dbg-attempt2-bitcoin26-fpclassify \
    MACHGATE_EXTERNAL_TIMEOUT=45 \
    bash tests/test_external_macho_cli.sh'
```

Result: still FAIL, status `134`. Resolver improved to `0 failed` binds, but
stderr still ends with `libc++abi: terminating due to uncaught exception of
type boost::unit_test::framework::nothing_to_test`.

Final bucket check:

```sh
docker run --rm --platform linux/arm64 \
  -v /home/kapablanka/repos/machgate:/work \
  -w /work machgate-arm64-toolchain bash -lc '
    set -e
    cmake --build build-arm64 --parallel
    grep -E "^(bitcoin-29.2-test|bitcoin-26.2-test|knots-test|qtum-test)\\|" tests/external/cpp_static_init_manifest.txt > /tmp/boost-auto-start-bucket.txt
    MACHGATE_RUN_EXTERNAL=1 \
    MACHGATE_EXTERNAL_VERBOSE=1 \
    MACHGATE_TRACE_SIGNALS=1 \
    MACHGATE_TRACE_LCMAIN=1 \
    MACHGATE_EXTERNAL_MAP_LIBCXX=1 \
    BUILD_DIR=/work/build-arm64 \
    MACHGATE_EXTERNAL_MANIFEST=/tmp/boost-auto-start-bucket.txt \
    MACHGATE_EXTERNAL_LOGS=/work/tests/external/logs/boost-auto-start-dbg-final-bucket \
    MACHGATE_EXTERNAL_WORK=/work/tests/external/work/boost-auto-start-dbg-final-bucket \
    MACHGATE_EXTERNAL_TIMEOUT=45 \
    bash tests/test_external_macho_cli.sh'
```

Result: `0/4` clean exits. Current status:

| Name | Status | Evidence |
|---|---|---|
| `bitcoin-29.2-test` | BOOST-NOTHING-TO-TEST-ABORT | The old `auto_start_dbg` invalid-name error is gone; current rerun prints Boost help, then aborts with `SIGABRT`. |
| `knots-test` | BOOST-NOTHING-TO-TEST-ABORT | The old `auto_start_dbg` invalid-name error is gone; current rerun prints Boost help, then aborts with `SIGABRT`. |
| `qtum-test` | BOOST-NOTHING-TO-TEST-ABORT | Completes `851` init offsets and no longer reports invalid `auto_start_dbg`; current rerun prints Boost help, then aborts with `SIGABRT`. |
| `bitcoin-26.2-test` | BOOST-NOTHING-TO-TEST-ABORT | Completes `270` init offsets, prints Boost help, then aborts with uncaught `boost::unit_test::framework::nothing_to_test`; resolver has `0 failed` binds. |

Remaining hypothesis:

- The original `BOOST_AUTO_START_DBG` parser bucket is fixed by the Darwin
  ctype correction.
- All four remaining Boost rows have moved into a narrower Boost help/control-flow
  exception handling bucket. It has `21614` native Mach-O `__eh_frame` FDEs and
  only `958` synthetic FDEs; the thrown Boost `nothing_to_test` control
  exception is not caught. This likely needs a focused C++ exception/native
  `__eh_frame` investigation, not another argv or ctype fix.
- Do not make broad native `__eh_frame` registration changes without a
  regression bucket. Prior EH-frame loop notes show this area can regress
  previously passing binaries.

## Current Interpretation

- Exact 707-entry constructor pressure is covered and passes with `cpp_many_ctors`.
- A source-derived local fixture now covers 707 Catch2/gtest-style registration constructors with nontrivial constructor bodies. It is generated from pinned public source archives for CLI11, Catch2, spdlog, fmt, and Microsoft GSL, but the emitted Mach-O remains freestanding: no Apple SDK, no libSystem import, no libc++ import, and no public headers are compiled into the target.
- Public heavy C++ CLI constructor pressure passes up to CMake's 367 `__mod_init_func` constructors through `ctest`; public test-runner constructor pressure now reaches Qtum's `851` initializer offsets and still completes static initialization.
- The private Catch2 crash at `mod_init[3/707]` is therefore more likely a specific constructor body dependency, callback/vtable target, TLV/libc++ object state, or missing shim path reached by that constructor, not the loader's ability to iterate large initializer tables.
- `BOOST_AUTO_START_DBG` was a Darwin ctype emulation bug: the shim's `_CTYPE_*` masks were too low by eight bits, so Boost.Test's `std::isalnum` path through `___maskrune` rejected valid parameter names such as `auto_start_dbg`.
- The focused public test-runner failure set is now only the Boost.Test `nothing_to_test` abort-after-help bucket, not constructor-table iteration or timeout-after-main.

## Static-Initializer Crash Instrumentation

For private or local binaries that fail during `__mod_init_func` or
`__init_offsets`, run with:

```sh
MACHGATE_EXTERNAL_VERBOSE=1
MACHGATE_TRACE_SIGNALS=1
MACHGATE_TRACE_LCMAIN=1
```

The expected evidence for a `pc=NULL` indirect-call failure is:

```text
machgate: mod_init[3/707] at 0x...
libsystem_shim: guest signal signum=11 ... pc=(nil) lr=0x...
libsystem_shim: current initializer kind=__mod_init_func index=3 total=707 address=0x...
libsystem_shim: signal callsite 0x... insn=0xd63f.... blr xN target=(nil)
machgate: guest context signal.lr-4=0x... segment=__TEXT section=__text fileoff=0x... insn=0xd63f....
```

That tells the next loop which register was the null function pointer. Then
trace where that register was loaded from: chained fixup slot, vtable,
singleton/global registry, shim return value, or TLV/TLS data.

## Local Public-Source Fixture

Directly compiling the candidate libraries into a no-SDK/no-runtime Mach-O is not a good narrow next step:

- CLI11 and Catch2 exercise the desired registration style, but their usable test runners require a large C++ runtime surface: strings, vectors, exceptions, iostream/reporting, allocation, and platform headers.
- spdlog and fmt are useful body-shape candidates, but realistic use quickly reaches formatting, chrono, sink, mutex, or allocation paths.
- Microsoft GSL is header-only, but even `gsl/span` pulls standard-library and C-header assumptions before target code links.

The implemented compromise is `cpp_public_test_registrars`, built only when requested:

```sh
MACHGATE_BUILD_CPP_PUBLIC_REGISTRARS=1 bash tests/fixtures/build_fixtures.sh
```

It downloads checksum-pinned archives:

| Project | Version/tag | Use in fixture |
|---|---:|---|
| CLI11 | `v2.6.2` | Catch2-style `TEST_CASE` names from upstream tests |
| Catch2 | `v3.9.1` | Native Catch2 examples and self-test declarations |
| spdlog | `v1.17.0` | Catch2-style logging test declarations |
| fmt | `12.2.0` | gtest-style formatting test declarations |
| Microsoft GSL | `v4.2.2` | gtest-style safety/span test declarations |

The generator parses public test declarations, round-robins across projects, and emits 707 static constructors. Each constructor registers source id, line, path, and test name through a shared hashing body, stores a registry entry, mutates a volatile sink, and `main` verifies the final count, sink, and aggregate before issuing Darwin `exit(0)`.

Run it through the existing test hook with:

```sh
MACHGATE_TEST_CPP_PUBLIC_REGISTRARS=1 BUILD_DIR=/work/build-arm64 bash tests/test_cpp_many_ctors.sh
```

The fixture is optional because it intentionally needs network access the first time unless the archives already exist under `tests/external/cache/public-cpp-static-init`.

## Agent / External Research Status

- Codex agents launched and completed:
  - Core-family release assets: added Bitcoin Knots, Elements, and Dash test-runner candidates; rejected x86_64-only Litecoin/Ravencoin test runners.
  - Direct Catch2 release artifacts: no stable public macOS ARM64 Catch2 test-runner executable found; suggested Catch2-using CLIs as controls only.
  - Package ecosystems: added CMake `ctest`; confirmed Catch2/gtest/doctest packages usually ship headers/libs but no executable runners.
  - Source-build candidates: suggested CLI11, spdlog, fmt, and GSL for generated Catch2/gtest registration-pressure fixtures if prebuilt artifacts are insufficient.
- Second Codex wave completed:
  - Core-family release search added `qtum-test` and `groestlcoin-test`; rejected several no-ARM64 or no-test-runner projects.
  - Package-manager search documented Homebrew/conda/LLVM/Qt candidates that need harness extraction support before automatic execution.
  - Source-derived fixture worker implemented opt-in `cpp_public_test_registrars`.
  - Argv triage proved command-line changes alone do not make the original four test-runner failures pass.
- Grok was launched with `--best-of-n` corpus and argv-triage prompts, but the headless runs produced no usable stdout in this shell.
- Claude was launched headless, but produced no usable stdout in this shell during this pass.

## Next Search/Loop Step

The next target is the deterministic Boost.Test startup failure bucket,
especially `BOOST_AUTO_START_DBG`, because it affects multiple real test
runners and exits quickly with stable stderr.

## Qtum Handled SIGILL Probe

Question: Qtum logs a handled `SIGILL` during static initialization at fileoff
`0x12df9e8`, instruction `0xd53be020`, then later reaches `_main` and fails
with `BOOST_AUTO_START_DBG`. Determine whether this is a MachGate emulation
gap or an expected guest probe.

Conclusion:

- `0xd53be020` decodes to `mrs x0, CNTPCT_EL0`, a read of the AArch64 physical
  count register.
- The instruction is in an OpenSSL-style ARM capability probe table, not in
  Qtum business logic. The surrounding stubs are `__armv7_neon_probe`,
  `__armv7_tick`, `__armv8_aes_probe`, `__armv8_sha1_probe`,
  `__armv8_sha256_probe`, `__armv8_pmull_probe`, `__armv8_sha512_probe`, and
  related one-instruction probe functions.
- MachGate should not emulate this as part of the current C++ static-init
  loop. The guest installed a `SIGILL` handler, the signal was delivered, the
  handler resumed execution, all `851` initializers completed, and `_main` was
  entered. The active Qtum failure remains the later Boost.Test
  `auto_start_dbg` setup error.
- If a future binary executes `mrs CNTPCT_EL0` as a required timer read without
  a guest `SIGILL` probe handler, that should be handled as a separate,
  narrower timer-register compatibility feature. This loop does not prove such
  a failure.

Evidence experiments:

| # | Experiment | Evidence | Result |
|---:|---|---|---|
| 1 | Decode the raw instruction fields. | `0xd53be020` has `Rt=0`, `op0=3`, `op1=3`, `CRn=14`, `CRm=0`, `op2=1`. | This is `MRS Xt, S3_3_C14_C0_1`, decoded by the toolchain as `mrs x0, CNTPCT_EL0`. |
| 2 | Disassemble Qtum around `0x1012df9e8`. | `llvm-objdump --disassemble --arch-name=arm64 --start-address=0x1012df980 --stop-address=0x1012dfa80 tests/external/work/cpp-static-init-attempt4/qtum-test` shows `1012df9e8: d53be020 mrs x0, CNTPCT_EL0; 1012df9ec: ret`. Nearby entries are `aese`, `sha1h`, `sha256su0`, `pmull`, and `sha512su0`, each followed by `ret`. | The PC is a one-instruction CPU probe function. |
| 3 | Re-read Qtum trace chronology. | `tests/external/logs/cpp-static-init-attempt4/qtum-test.err` shows `init[850/851]`, then `guest signal signum=4 ... pc=0x1012df9e8`, then `static initializers complete`, then `calling _main`, then `Test setup error: Parameter auto_start_dbg has invalid characters in name.` | The SIGILL is handled and is not the terminal failure. |
| 4 | Scan the focused C++ static-init corpus for the same probe table. | Alignment-aware byte scan of `tests/external/work/cpp-static-init-attempt4` found this exact timer/probe table only in `qtum-test`: one `cntpct_el0` plus one each of AES/SHA/PMULL probe opcodes. | No other focused C++ test-runner row currently shows this probe as an observed issue. |
| 5 | Scan already-extracted wider external binaries. | Same scan under `tests/external/work/*/*` found duplicate probe tables in `qtum-test`, `nu`, and `node`. `nu` and `node` disassemble with named OpenSSL symbols `__armv7_tick`, `__armv8_aes_probe`, `__armv8_sha1_probe`, `__armv8_sha256_probe`, `__armv8_pmull_probe`, `__armv8_sm4_probe`, `__armv8_sha512_probe`, and `__armv8_eor3_probe`. Their current logs do not show a `guest signal signum=4` for the tested paths. | Similar probes exist in other external binaries, but current evidence does not show them blocking C++ static initialization. |

Upstream/source interpretation:

- OpenSSL's `crypto/armcap.c` documents fallback ARM feature detection by
  calling probe functions that may `SIGILL`; if a probe faults, it returns zero
  for that capability.
- The same file declares `_armv7_tick` and uses it from `OPENSSL_rdtsc()` only
  when `OPENSSL_armcap_P` contains `ARMV7_TICK`.
- On Apple AArch64, OpenSSL's source path normally uses known Apple Silicon
  capability defaults plus `sysctlbyname` for newer extensions; the probe table
  can still be present in linked code even when not all entries are expected to
  execute on a given path.

Primary source: <https://github.com/openssl/openssl/blob/master/crypto/armcap.c>

No code change was made for this item.

## Focused Bucket Loop Harness

The full 22-row manifest is useful for corpus snapshots, but it is too broad
for fast fix iteration. Use the bucket manifests and wrapper below while
working on one failure class:

| Bucket | Manifest | Runner alias | Default log prefix |
|---|---|---|---|
| `BOOST_NOTHING_TO_TEST_ABORT` | `tests/external/cpp_static_init_boost_nothing_to_test_abort_manifest.txt` | `BOOST_NOTHING_TO_TEST_ABORT` | `cpp-static-init-boost-nothing-to-test-abort-attemptN` |
| `BOOST_AUTO_START_DBG` | `tests/external/cpp_static_init_boost_auto_start_dbg_manifest.txt` | `BOOST_AUTO_START_DBG` | `cpp-static-init-boost-auto-start-dbg-attemptN` |
| `BOOST_AUTO_START_DBG_ABORT` | `tests/external/cpp_static_init_boost_auto_start_dbg_abort_manifest.txt` | `BOOST_AUTO_START_DBG_ABORT` | `cpp-static-init-boost-auto-start-dbg-abort-attemptN` |
| `TIMEOUT_AFTER_MAIN` | `tests/external/cpp_static_init_timeout_after_main_manifest.txt` | `TIMEOUT_AFTER_MAIN` | `cpp-static-init-timeout-after-main-attemptN` |

Quick single-attempt smoke after a small fix:

```sh
MACHGATE_CPP_LOOP_ATTEMPTS=1 \
  bash tests/external/run_cpp_static_init_bucket_loop.sh BOOST_NOTHING_TO_TEST_ABORT
```

Five-attempt stop run for the active Boost abort bucket:

```sh
bash tests/external/run_cpp_static_init_bucket_loop.sh BOOST_NOTHING_TO_TEST_ABORT
```

Five-attempt stop run for the post-main timeout bucket:

```sh
bash tests/external/run_cpp_static_init_bucket_loop.sh TIMEOUT_AFTER_MAIN
```

Optional companion run for the aborting Boost setup-error row:

```sh
bash tests/external/run_cpp_static_init_bucket_loop.sh BOOST_AUTO_START_DBG_ABORT
```

For a final stability claim after a bucket starts passing, require all five
attempts instead of stopping after the first pass:

```sh
MACHGATE_CPP_LOOP_REQUIRE_ALL_ATTEMPTS=1 \
  bash tests/external/run_cpp_static_init_bucket_loop.sh BOOST_NOTHING_TO_TEST_ABORT
```

The wrapper builds `build-arm64` once, then invokes the unchanged external
Mach-O harness with:

```text
MACHGATE_RUN_EXTERNAL=1
MACHGATE_EXTERNAL_VERBOSE=1
MACHGATE_TRACE_SIGNALS=1
MACHGATE_TRACE_LCMAIN=1
MACHGATE_EXTERNAL_MAP_LIBCXX=1
MACHGATE_EXTERNAL_TIMEOUT=45
```

Each attempt writes:

- per-binary harness logs under `tests/external/logs/cpp-static-init-<bucket>-attemptN/`
- extracted binaries and config under `tests/external/work/cpp-static-init-<bucket>-attemptN/`
- console output in `tests/external/logs/cpp-static-init-<bucket>-attemptN/console.log`
- a TSV summary at `tests/external/logs/cpp-static-init-<bucket>-loop-summary.tsv`

Loop controls:

| Variable | Default | Use |
|---|---:|---|
| `MACHGATE_CPP_LOOP_ATTEMPTS` | `5` | Number of attempts, capped at `5`. |
| `MACHGATE_CPP_LOOP_TIMEOUT` | `45` | Per harness run timeout passed through unchanged. |
| `MACHGATE_CPP_LOOP_REQUIRE_ALL_ATTEMPTS` | `0` | Set to `1` for a final `5 / 5` validation run. |
| `MACHGATE_CPP_LOOP_SKIP_BUILD` | `0` | Set to `1` only when `build-arm64` is already current. |
| `MACHGATE_CPP_LOOP_BUILD_EACH_ATTEMPT` | `0` | Set to `1` if rebuilding between attempts is intentional. |

Stop and promotion criteria:

- During implementation, run the smallest matching bucket first. Start with
  `BOOST_NOTHING_TO_TEST_ABORT`; it is the only active focused public C++
  test-runner blocker.
- If a bucket passes during a normal loop, the runner stops immediately and
  returns success. Then run the same bucket with
  `MACHGATE_CPP_LOOP_REQUIRE_ALL_ATTEMPTS=1` before promoting the fix.
- If the same bucket remains failing after five attempts, stop that loop and
  mark the bucket `harder-later` with the exact summary path and the most useful
  stderr, signal, and qemu-strace evidence.
- Do not mix `BOOST_NOTHING_TO_TEST_ABORT` and `TIMEOUT_AFTER_MAIN` in one fix
  loop. The timeout bucket is fixed and should be treated as a regression gate.

## Additional Public Core-Family Search

Added verified public macOS ARM64 unit-test runners:

| Name | Source | Static constructors | Verification |
|---|---:|---:|---|
| `qtum-test` | Qtum 29.1 public release | `851` `__init_offsets` | SHA-256 verified; `qtum-29.1/bin/test_qtum` is Mach-O 64-bit arm64. |
| `groestlcoin-test` | Groestlcoin 31.0 public release | `254` `__init_offsets` | SHA-256 verified; `groestlcoin-31.0/libexec/test_groestlcoin` is Mach-O 64-bit arm64. |

Rejected candidates from this search pass:

| Candidate | Reason |
|---|---|
| Bitcoin Cash Node 29.0.0 | Latest macOS tarball is `osx64`, not ARM64; no ARM64 Apple Darwin tarball found in release assets. |
| Bitcoin ABC 0.33.6 | Publishes `osx-unsigned.zip`, but no ARM64 Apple Darwin CLI tarball in release assets. |
| Litecoin 0.21.5.5 | Publishes Linux aarch64 and `osx64`/DMG assets, but no ARM64 Apple Darwin CLI tarball. |
| Dogecoin 1.14.9 | Publishes Linux aarch64 and macOS DMG assets, but no ARM64 Apple Darwin CLI tarball. |
| Peercoin 0.15.1 | ARM64 Apple Darwin tarball verified, but it contains only CLI/daemon/Qt binaries and no `test_*` executable. |
| Vertcoin 23.2 | ARM64 Apple Darwin tarball verified, but it contains only CLI/daemon/Qt binaries and no `test_*` executable. |
| Potcoin 2.0.1 | ARM64 Apple Darwin tarball verified, but it contains only `potcoind` and `potcoin-qt`, no `test_*` executable. |
| BitcoinUnlimited BUcash 1.2.0.0 | `arm64` tarball contains Linux ELF aarch64 binaries; macOS tarball is app-packaging-only and has no `test_bitcoin`. |

## Package-Manager / LLVM-Adjacent Search

Verified candidates from Homebrew bottle, conda-forge, and GitHub release
metadata search. Rows below are exact manifest candidates:

## Private 707-Constructor Crash Evidence

Latest private C++/Catch2-style runner signal trace:

```text
machgate: mod_init[3/707] at 0x10000b41c
libsystem_shim: guest signal signum=11 darwin=11 code=1 addr=(nil) pc=(nil) lr=0x1008a26a0
libsystem_shim: current initializer kind=__mod_init_func index=3 total=707 address=0x10000b41c
libsystem_shim: signal callsite 0x1008a269c insn=0xd63f0100 blr x8 target=(nil)
machgate: guest context signal.lr-4=0x1008a269c vmaddr=0x1008a269c segment=__TEXT section=__text fileoff=0x8a269c insn=0xd63f0100
```

What this proves:

- The constructor runner is not stuck in the 707-entry loop; it reaches
  `__mod_init_func` entry `3` and then faults inside guest `__TEXT`.
- The actual fault is an ARM64 indirect branch with link, `blr x8`, where the
  signal frame reports `x8 == NULL`.
- This is not directly a patched Darwin syscall instruction. The callsite is
  a guest `blr`, not `svc #0x80`.
- The next distinction to make is whether guest code loaded NULL from an
  unresolved/badly initialized function slot, or whether MachGate clobbered
  `x8` immediately before the indirect call.

Diagnostic improvement added after this trace:

- `machgate_trace_guest_address()` now prints the nearest retained Mach-O
  symbol for guest addresses when the symbol table is available.
- For `signal.lr-4`, MachGate prints a small instruction window around the
  indirect branch. This should show the load/move/call sequence that produced
  the NULL `x8` target.
- The decoded sequence for the private trace is:

  ```text
  0x1008a268c: adrp x8, 0x100d92000
  0x1008a2694: ldr  x8, [x8, #0x1a8]
  0x1008a2698: mov  w0, #0x80000
  0x1008a269c: blr  x8
  ```

  The indirect target is loaded from slot `0x100d921a8`; at the crash, that
  slot contains NULL. This is stronger evidence for an unfilled or wrongly
  filled bind/lazy pointer slot than for the branch patcher randomly clobbering
  `x8`.
- A resolver bind-slot registry now records which Mach-O import/bind populated
  each slot. Signal diagnostics decode the `adrp`/`ldr`/`blr` pattern and print
  a new `indirect-slot` line with bind symbol, lookup name, dylib, resolver
  context, resolved address, and source.
- If the resolver registry reports `bind=(unrecorded)`, signal diagnostics now
  also inspect the slot's Mach-O section and LC_DYSYMTAB indirect symbol table.
  The next run should print `indirect-slot-section` and, when available,
  `indirect-symbol` with the symbol-table name for the pointer slot.
- `v0.3.6` showed the private NULL slot lives in `__DATA,__common`,
  `type=0x1`, not in a lazy/non-lazy symbol pointer section. That means the
  call target is a zero-fill global/common variable or a field inside one, not
  an imported function pointer. The current leading causes are a MachGate
  initialization-order difference, a prior constructor/runtime call returning
  different state than macOS, or a Darwin runtime setup step missing before
  app constructors.
- The next diagnostic prints `data-symbol`, the nearest Mach-O nlist symbol in
  that data section, to identify the global object/function-pointer cell whose
  value is still zero.
- `MACHGATE_TRACE_BINDINGS=all` prints every resolver bind when a full import
  dump is needed.

`v0.3.7` private trace resolved the slot identity:

```text
machgate: guest context signal.lr-4 data-symbol slot=0x100d921a8 section-ordinal=30 symbol='_libc_default_malloc'+0x0 symbol-vmaddr=0x100d921a8 next='_libc_default_calloc' next-delta=0x8
```

Conclusion:

- The NULL `blr x8` target is `_libc_default_malloc`, a statically linked
  Darwin libc/libmalloc default allocator function-pointer slot in
  `__DATA,__common`.
- The crashing constructor path is inside
  `_ZN11RomemThread6createERS_PFPvS1_ES1_`, which loads
  `_libc_default_malloc`, moves `0x80000` into `w0`, then calls the function
  pointer.
- This is not an instruction-boundary patch bug, not x8 clobbering by the
  syscall gateway, and not an imported lazy-bind slot. It is guest runtime data
  that should have been initialized before constructors.
- On macOS, dyld/libSystem/libmalloc startup initializes this allocator default
  surface before user `__mod_init_func` entries run. MachGate previously
  recreated the loader/fixup path but did not initialize the statically linked
  Darwin libc allocator defaults.

Fix staged after this trace:

- Export explicit shim allocator entry points:
  `machgate_shim_malloc`, `machgate_shim_calloc`,
  `machgate_shim_realloc`, and `machgate_shim_free`.
- Before guest static constructors, look up guest symbols
  `_libc_default_malloc`, `_libc_default_calloc`,
  `_libc_default_realloc`, and `_libc_default_free`.
- If any slot exists and is still NULL, patch it to the corresponding shim
  allocator function and log the patch.
- This is intentionally narrower than full libmalloc initialization. It covers
  the proven Darwin libc default allocator slots while leaving binaries that do
  not contain those symbols untouched.

`v0.3.8` moved the private binary past the NULL allocator call. The next trace:

```text
machgate: mod_init[3/707] at 0x10000b41c
libsystem_shim: guest signal signum=11 darwin=11 code=1 addr=(nil) pc=0x1001ba5a4 lr=0x10050a690
machgate: guest context signal.pc=0x1001ba5a4 vmaddr=0x1001ba5a4 symbol=_ZNSt3__16__treeINS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEENS_4lessIS6_EENS4_IS6_EEE16__insert_node_atEPNS_15__tree_end_nodeIPNS_16__tree_node_baseIPvEEEERSF_SF_+0x20 segment=__TEXT section=__text fileoff=0x1ba5a4 insn=0xf9400108
machgate: guest regs x0=0x100da3320 x1=0x100da3328 x2=0x100da3328 x3=0x75463f4b2e50 ... x8=(nil)
```

Interpretation:

- Constructor `3/707` now reaches libc++ `std::__tree::__insert_node_at`.
- The call shape matches an empty-tree insert: `x0` is the tree object,
  `x1/x2` point at the embedded end/root slot, and `x3` is the new node.
- In a constructed empty libc++ tree, `__begin_node_` should point at the
  embedded end node. The faulting load with `x8 == NULL` strongly suggests
  `__begin_node_` is still zero, so a C++ tree object is being used before its
  constructor has run.
- Apple dyld's `MachOAnalyzer::forEachInitializer()` runs initializer phases
  in this order: `LC_ROUTINES`, then `S_MOD_INIT_FUNC_POINTERS`, then
  `S_INIT_FUNC_OFFSETS`. MachGate previously scanned sections once and ran
  whichever initializer section it encountered first, which can interleave
  those phases differently from macOS.

Fix staged after this trace:

- Main-executable initializers now run in dyld phase order:
  `LC_ROUTINES`, `LC_ROUTINES_64`, all `__mod_init_func`, then all
  `__init_offsets`.
- Initializers are called with dyld-style `(argc, argv, envp, apple)` registers
  instead of through a no-argument function pointer. C++ constructor thunks
  ignore these registers, but Darwin initializers that expect them now see the
  right ABI.
- The verbose log preserves the existing `mod_init[i/N]` / `init[i/N]` labels
  so private traces remain comparable.

`v0.3.9` did not move the private trace; it still faults in the same libc++
tree insert with `__begin_node_ == NULL`. The remaining high-confidence
runtime gap for this shape is C++ guarded function-local static initialization:

- A registry implemented as a function-local static `std::set<std::string>` is
  compiled around `__cxa_guard_acquire`, `__cxa_guard_release`, and
  `__cxa_guard_abort`.
- If the guard path incorrectly reports "already initialized" before the
  constructor runs, the compiler will skip constructing the tree and then use
  zero-filled storage, matching the observed `__begin_node_ == NULL` crash.
- MachGate's shim previously did not export this C++ ABI guard surface.

Fix staged after this trace:

- Export single-thread-target `__cxa_guard_acquire`, `__cxa_guard_release`,
  and `__cxa_guard_abort` from `libsystem_shim`.
- Export no-op success `__cxa_atexit` and no-op `__cxa_finalize`, because the
  same static initialization path often registers destructors after successful
  construction.
- The guard implementation follows the Itanium/Darwin byte layout: byte `0`
  is set only after successful release; byte `1` tracks pending/completed
  initialization for the current single guest thread.

`v0.3.10` still did not move the private trace. That means the C++ guard
surface was either not used on this path, statically linked inside the binary,
or not the root cause. The next release is diagnostic-only:

- For signal traces, MachGate now prints section/symbol context for register
  values `x0` through `x8` when they point into the guest Mach-O image.
- For the exact libc++ tree-insert fault instruction `0xf9400108`, the shim
  dumps four pointer-sized words at `x0` (tree object), `x1`/`x2` (child slot),
  and `x3` (new node).
- Expected evidence: identify the Mach-O data symbol owning `x0=0x100da3320`
  and determine whether the tree object is fully zero, partially initialized,
  or has a bad `__begin_node_` only.

`v0.3.11` confirmed the tree object is in guest zero-fill storage:

```text
machgate: guest context signal.x0=0x100da3320 vmaddr=0x100da3320 segment=__DATA section=__bss fileoff=0x10fa0 insn=0x00000000
machgate: guest context signal.x1=0x100da3328 vmaddr=0x100da3328 segment=__DATA section=__bss fileoff=0x10fa8 insn=0x6227ae50
machgate: guest context signal.x2=0x100da3328 vmaddr=0x100da3328 segment=__DATA section=__bss fileoff=0x10fa8 insn=0x6227ae50
```

Interpretation:

- `x0` is the libc++ tree object in `__DATA,__bss`.
- `x1` and `x2` both point at `tree + 8`, the embedded child/root slot.
- The low word printed at `tree + 8` matches the low bits of `x3`, so the
  insertion already stored the new node into the child/root slot.
- The fault remains `ldr x8, [x8]` with `x8 == NULL`, meaning `tree[0]`
  (`__begin_node_`) is NULL. A constructed empty libc++ tree should have
  `__begin_node_ == tree + 8`.
- The first `v0.3.11` register walker was too broad and appears to have crashed
  inside the signal handler before printing the targeted memory-word probe.

Fix staged after this trace:

- Restrict the signal register address context to guest-image candidates needed
  for this failure (`x0`, `x1`, `x2`, and non-NULL `x8`).
- Do not dereference the host-heap `x3` new-node pointer in the signal handler.
- Print nearest data symbols for normal `__DATA` addresses, not only for
  indirect-call slots, so `signal.x0` / `signal.tree` should identify the
  owning global.
- Keep the focused memory-word dump for `x0.tree`, `x1.child-slot`, and
  `x2.child-slot`.

`v0.3.12` confirmed the exact object and field state:

```text
machgate: guest context signal.x0=0x100da3320 vmaddr=0x100da3320 segment=__DATA section=__bss fileoff=0x10fa0 insn=0x00000000
machgate: guest context signal.x0 data-symbol slot=0x100da3320 section-ordinal=31 symbol='__MergedGlobals'+0x10 symbol-vmaddr=0x100da3310 next='_optblocker_u8' next-delta=0x88
machgate: guest context signal.x1=0x100da3328 vmaddr=0x100da3328 segment=__DATA section=__bss fileoff=0x10fa8 insn=0xdf7f2e50
machgate: guest context signal.x1 data-symbol slot=0x100da3328 section-ordinal=31 symbol='__MergedGlobals'+0x18 symbol-vmaddr=0x100da3310 next='_optblocker_u8' next-delta=0x80
libsystem_shim: signal libcxx-tree-insert probe pc=0x1001ba5a4
libsystem_shim: signal memory x0.tree=0x100da3320 [0]=(nil) [8]=0x7011df7f2e50 [16]=(nil) [24]=(nil)
libsystem_shim: signal memory x1.child-slot=0x100da3328 [0]=0x7011df7f2e50 [8]=(nil) [16]=(nil) [24]=(nil)
libsystem_shim: signal memory x3.new-node=0x7011df7f2e50 (not dereferenced)
machgate: guest context signal.tree data-symbol slot=0x100da3320 section-ordinal=31 symbol='__MergedGlobals'+0x10 symbol-vmaddr=0x100da3310 next='_optblocker_u8' next-delta=0x88
```

Interpretation:

- The crashing object is `__MergedGlobals + 0x10`.
- The object has the libc++ `__tree` shape, but field zero is still NULL.
- The new node has already been stored through `tree + 8`.
- The faulting instruction is the second load in libc++'s
  `__tree::__insert_node_at`: register `x8` already contains `tree[0]`, so the
  retry needs both memory repair and saved-register repair.
- A valid empty libc++ tree has `tree[0] == tree + 8`; after the current insert,
  libc++ will naturally set `tree[0]` to the inserted node.

Rejected local experiment after this trace:

- `v0.3.13` added a narrow signal-dispatch repair for this exact libc++ tree
  insertion state. It repaired `tree[0]` and the saved `x8`, then retried the
  original instruction.
- The private binary moved past the tree insert, but immediately faulted later
  in the same Catch2 `TestCaseInfo` constructor while copying another
  C++/Catch2 object. That proves the tree repair was only masking the first
  visible broken invariant.
- The local branch has backed out this repair. The next accepted fix must
  target the systemic runtime issue: C++ static/local-static initialization,
  Darwin libc++ ABI setup, or initializer ordering for the Catch2 registration
  path.

Accepted fix direction after the rejection:

- The current fix targets the Darwin pthread/C++ runtime surface instead of the
  crashed tree object.
- `libsystem_shim` implements Darwin-compatible `pthread_once` for the Darwin
  once signature state `0x30B1BCBA`, so C++/libc++ one-time initialization does
  not fall through to glibc's incompatible `pthread_once` state machine.
- `libsystem_shim.ver` exports the broader pthread surface used by Darwin C++
  runtimes, including `pthread_once`, `pthread_mach_thread_np`, and
  `pthread_threadid_np`.
- The resolver routes `pthread_*` imports through the Darwin runtime shim before
  host libraries, avoiding return-zero stubs or host glibc semantics for Darwin
  pthread ABI calls.
- `tests/test_libsystem_shim.sh` now directly verifies that a Darwin
  `pthread_once_t` signature state runs the initializer exactly once and reaches
  done state.

Remaining interpretation rule for future similar traces:

- If the instruction window shows `ldr x8, [...]` from a Mach-O bind/lazy-bind
  slot that is zero, investigate resolver binding or missing shim export.
- If it shows `mov x8, x0` or similar immediately after a shim/native call,
  inspect that callee's return convention and whether the interposed function
  should return a callback/table pointer.
- If it shows `x8` expected to survive across a MachGate/native call, inspect
  the exact patched call path, but remember `x8` is caller-saved under AAPCS64.

```text
homebrew-cmake-ctest|https://ghcr.io/v2/homebrew/core/cmake/blobs/sha256:5648b07aa9fefcae2347c2293b87230f89977a8175c9aa25e4ef453199474852|5648b07aa9fefcae2347c2293b87230f89977a8175c9aa25e4ef453199474852|cmake/4.3.4/bin/ctest|--version
conda-cmake-ctest|https://conda.anaconda.org/conda-forge/osx-arm64/cmake-4.3.4-h8cb302d_0.conda|8c82c756a3833debe2039ab05ff4214eb0a3316a5e8b5723ac8349379ee7e30b|bin/ctest|--version
conda-clangd|https://conda.anaconda.org/conda-forge/osx-arm64/clang-tools-22.1.8-default_h8e162e0_2.conda|78c0e4a37fa1c9dea0dcf313a653205b3c26ab5aa3e93ed649de7a9a4e559e0f|bin/clangd|--version
conda-clang-tidy|https://conda.anaconda.org/conda-forge/osx-arm64/clang-tools-22.1.8-default_h8e162e0_2.conda|78c0e4a37fa1c9dea0dcf313a653205b3c26ab5aa3e93ed649de7a9a4e559e0f|bin/clang-tidy-22|--version
conda-llvm-exegesis|https://conda.anaconda.org/conda-forge/osx-arm64/llvm-tools-22-22.1.8-hb545844_1.conda|a5ac8bd2fd67c6e71c991e3172c0ec8430de4ec91506e93c0f1afb56a88ed8e6|bin/llvm-exegesis-22|--version
conda-llvm-tblgen|https://conda.anaconda.org/conda-forge/osx-arm64/llvm-tools-22-22.1.8-hb545844_1.conda|a5ac8bd2fd67c6e71c991e3172c0ec8430de4ec91506e93c0f1afb56a88ed8e6|bin/llvm-tblgen-22|--version
conda-qt-qmlls|https://conda.anaconda.org/conda-forge/osx-arm64/qt6-main-6.11.1-pl5321h7775a44_1.conda|84e8269f5bff6e167729cfe419f13551dc9a66af0e7af1038965a360c6965663|lib/qt6/bin/qmlls|--help
conda-qt-qmltestrunner|https://conda.anaconda.org/conda-forge/osx-arm64/qt6-main-6.11.1-pl5321h7775a44_1.conda|84e8269f5bff6e167729cfe419f13551dc9a66af0e7af1038965a360c6965663|lib/qt6/bin/qmltestrunner|--help
```

Archive inspection notes:

| Candidate | Verification |
|---|---|
| `homebrew-cmake-ctest` | Homebrew bottle downloaded with OCI bearer token; SHA-256 matched formula metadata. `cmake/4.3.4/bin/ctest` is Mach-O 64-bit arm64 with `__TEXT,__init_offsets` size `0x5d8`, i.e. `374` initializer offsets, plus TLV sections. |
| `conda-cmake-ctest` | conda-forge package SHA-256 matched repodata. `bin/ctest` is Mach-O 64-bit arm64 with `__DATA,__mod_init_func` size `0xb70`, i.e. `366` constructors, plus TLV sections. |
| `conda-clangd` | conda-forge `clang-tools` package SHA-256 matched repodata. `bin/clangd` is Mach-O 64-bit arm64 with `__mod_init_func` size `0x1b8`, i.e. `55` constructors, plus TLV sections. |
| `conda-clang-tidy` | Same `clang-tools` package. `bin/clang-tidy-22` is Mach-O 64-bit arm64 with `__mod_init_func` size `0xf8`, i.e. `31` constructors. |
| `conda-llvm-exegesis` | conda-forge `llvm-tools-22` package SHA-256 matched repodata. `bin/llvm-exegesis-22` is Mach-O 64-bit arm64, about `66 MiB`, with `__mod_init_func` size `0xab8`, i.e. `343` constructors, plus TLV sections. |
| `conda-llvm-tblgen` | Same `llvm-tools-22` package. `bin/llvm-tblgen-22` is Mach-O 64-bit arm64 with `__mod_init_func` size `0x178`, i.e. `47` constructors. |
| `conda-qt-qmlls` | conda-forge `qt6-main` package SHA-256 matched repodata. `lib/qt6/bin/qmlls` is Mach-O 64-bit arm64, about `8.4 MiB`, with `__mod_init_func` size `0x138`, i.e. `39` constructors. |
| `conda-qt-qmltestrunner` | Same `qt6-main` package. `lib/qt6/bin/qmltestrunner` is a real Qt test runner and Mach-O 64-bit arm64, but has no visible `__mod_init_func`; include as a low-pressure true runner row. |

Rejected or not appended:

| Candidate | Reason |
|---|---|
| conda-forge `catch2-3.15.1-h3feff0a_0.conda` | Archive inspection found headers, CMake helpers, and `libCatch2*.dylib`, but no executable test runner. |
| conda-forge `gtest-1.17.0-ha393de7_1.conda` | Archive inspection found headers and `libgtest`/`libgmock` dylibs, but no executable sample runner. |
| conda-forge `doctest-2.5.2-h4ddebb9_0.conda` | Archive inspection found headers and CMake/pkg-config metadata only; no executable. |
| conda-forge `boost-cpp-1.84.0-hf7f88b1_7.conda` | Current package is compatibility metadata/license only; no Boost.Test binary. |
| conda-forge `llvm-tools-22` `FileCheck`, `split-file`, `llvm-lit` | `llvm-tools-22` contains many versioned LLVM tool binaries, but archive inspection found no `FileCheck`, `split-file`, or `llvm-lit` payload. |
| GitHub `LLVM-22.1.8-macOS-ARM64.tar.xz` | Release asset exists with attested SHA-256 `f260f4f7c0d430828a81ae8a3826a1d63fc0963ec2459489308cc23b1f7eab4f`, but the archive is about `1.48 GiB`; bounded stream inspection timed out before confirming `FileCheck`/`split-file` paths, so no row was appended. |

## v0.3.17 C++ Init Diagnostic

`v0.3.16` fixed a real loader-contract issue by running Mach-O initializers on
the guest stack, but the private Catch2 binary still reports the same
`__MergedGlobals + 0x10` libc++ tree insert fault. That proves guest-stack
discipline was not the failing invariant for this specific object.

`v0.3.17` adds opt-in diagnostics behind `MACHGATE_TRACE_CXX_INIT=1`:

- loader-side `__MergedGlobals` word dumps before and after each initializer
- shim-side `__cxa_guard_acquire`, `__cxa_guard_release`, and
  `__cxa_guard_abort` logs, including guard value and guest caller context

The next private run should answer whether the storage is never constructed,
constructed and later zeroed, or guarded as already initialized before its
constructor runs.

## v0.3.17 Private Run Follow-Up

The private `Core.UnitTest` run with `MACHGATE_TRACE_CXX_INIT=1` still crashed
at `mod_init[3/707]` in libc++ `__tree::__insert_node_at`, with
`__MergedGlobals + 0x10` / `+0x18` still showing the uninitialized empty-tree
shape. However, the log contained no loader-side `cxx-init` lines and no
shim-side `__cxa_guard_*` lines.

That means the Docker helper did not pass `MACHGATE_TRACE_CXX_INIT` through to
the container, so this run did not actually exercise the new v0.3.17 C++
diagnostics.

The same run also showed:

- `libc++.1.dylib` had no mapping and was skipped.
- The resolver ended with `3380 failed` binds.

For this C++ workload, the next diagnostic run must use an updated
`bin/run-macho-docker.sh` plus an Apple-ABI libc++ mapping:

```bash
MACHGATE_TRACE_CXX_INIT=1 \
MACHGATE_TRACE_SIGNALS=1 \
MACHGATE_TRACE_LCMAIN=1 \
MACHGATE_VERBOSE=1 \
MACHGATE_TIMEOUT=120 \
MACHGATE_LIBCXX=/home/kapablanka/repos/machgate/build-libcxx/lib/libc++.so.1 \
MACHGATE_TARBALL=/path/to/machgate-0.3.17-linux-arm64.tar.gz \
bin/run-macho-docker.sh /path/to/Core.UnitTest
```

The runner now passes every `MACHGATE_TRACE_*` variable through dynamically and
maps `libc++.1.dylib` when `MACHGATE_LIBCXX` is set.

## v0.3.21 Private Run Follow-Up

The private `Core.UnitTest` run with `v0.3.21` reached a new milestone:

- all `707 / 707` `__mod_init_func` static constructors completed
- MachGate changed directory to `/input`
- MachGate called `_main`
- the LC_MAIN ABI stack/argv/envp/apple vectors were visible and coherent

The remaining failure moved out of loader initialization and into guest runtime
allocator accounting:

```text
Memory.cpp(884) ASSERTION FAILED: rest >= size && rest <= kMaxHeapSize
Function: trackDeallocate
Message: rest=0, size=72, kMaxHeapSize=9223372036854775807
```

Interpretation:

- This is no longer the `mod_init[3/707]` libc++ tree initialization crash.
- The binary is now executing its own runtime code in `_main`.
- The allocator surface is still incomplete: Darwin code expects
  `malloc_size(ptr)` and the malloc-zone callbacks to report a valid allocation
  size for pointers allocated through the shim.

Accepted fix direction:

- Keep treating this as a loader/runtime contract issue, not a
  private-binary-specific repair.
- Track MachGate-owned allocation sizes inside `libsystem_shim`.
- Apply the tracking uniformly across `malloc`, `calloc`, `realloc`,
  `posix_memalign`, `memalign`, and `valloc`.
- Make `malloc_size(ptr)` return the recorded size for live shim allocations and
  `0` after those pointers are freed.

The direct regression test is `tests/test_libsystem_shim.sh`, which now checks
that `malloc_size` reports nonzero sizes for normal, aligned, and reallocated
shim allocations and returns `0` after free.

## v0.3.23 Private Run Follow-Up

The `v0.3.23` private run confirmed the initializer progress logging fix:

```text
machgate: mod_init[707/707] index=706 at 0x100947a30
machgate: __mod_init_func constructors complete
machgate: calling _main at 0x10094754c
```

The remaining failure is still the post-`_main` allocator accounting assert:

```text
Memory.cpp(884) ASSERTION FAILED: rest >= size && rest <= kMaxHeapSize
Message: rest=0, size=72
```

Next accepted fix:

- Correct the Darwin allocator default callback table so
  `_libc_default_memalign` uses the `memalign(alignment, size) -> void*`
  signature, not the incompatible
  `posix_memalign(&ptr, alignment, size) -> int` signature.
- Keep `_libc_default_posix_memalign` on the real `posix_memalign` callback.
- Add explicit shim coverage for both ABI shapes.
- Add opt-in `MACHGATE_TRACE_ALLOC=1` diagnostics that report whether a
  zero-size `malloc_size(ptr)` result is for a tracked-freed pointer or a
  foreign/untracked pointer.

## v0.3.25 Logging Follow-Up

The private runs proved that the constructor loop is healthy enough to reach
`_main`, so per-initializer logs are now too noisy for normal diagnosis.

Accepted logging behavior:

- `-v` prints sparse initializer progress only: first initializer, every 100th
  initializer, last initializer, and completion.
- `MACHGATE_TRACE_CXX_INIT=1` stays compact and does not print
  per-initializer `__MergedGlobals` words or every `__cxa_guard_*` call.
- `MACHGATE_TRACE_CXX_INIT=full` restores the old deep dump for cases where an
  initializer crashes before completion.
- `MACHGATE_TRACE_INIT_EACH=1` can also force per-initializer address logs
  without enabling the full C++ object-state dump.

## v0.3.26 Allocator Interposition Follow-Up

The `v0.3.25` private run still reaches `_main` and then aborts in guest memory
accounting:

```text
Memory.cpp(884) ASSERTION FAILED: rest >= size && rest <= kMaxHeapSize
Function: trackDeallocate
Message: rest=0, size=72
```

New evidence from the ARM64 regression test showed that internal shim calls to
`posix_memalign` could go through the ELF PLT and resolve to glibc instead of
the shim implementation. That returns a valid host allocation, but it bypasses
MachGate's allocation-size ledger. The direct symptom was a 72-byte aligned
allocation whose later `malloc_size` lookup returned too little.

Accepted fix:

- split `posix_memalign` into a private `shim_posix_memalign_impl` helper plus
  the exported Darwin-compatible symbol
- make `machgate_shim_memalign`, `machgate_shim_posix_memalign`,
  `malloc_zone_memalign`, and `malloc_zone_valloc` call the private helper
  directly
- route sized, nothrow, and aligned C++ allocation/deallocation imports through
  the same MachGate allocation hooks as ordinary `operator new/delete`
- add `MACHGATE_TRACE_ALLOC_SIZE=<n>` so private runs can trace only the
  allocation size involved in a guest failure, for example `72`

Validation:

- `BUILD_DIR=/home/kapablanka/repos/machgate/build bash tests/test_libsystem_shim.sh`
  passes
- ARM64 Docker `BUILD_DIR=/work/build-arm64 bash tests/run_tests.sh` passes
  `29 / 29`

## v0.3.27 Host libc++ Operator Allocation Follow-Up

The `v0.3.26` private run still aborts after `_main` with the same
`trackDeallocate` assertion and no `MACHGATE_TRACE_ALLOC_SIZE=72` events. That
negative trace means the failing 72-byte ownership path was not visible through
MachGate's exported malloc/default-zone ledger.

New evidence:

- packaged `libc++.so.1` has undefined `operator new/delete` symbols
- packaged `libc++abi.so.1` provides weak `operator new/delete` symbols backed
  by the host allocator
- `libsystem_shim.so` did not export those C++ operator symbols, so host
  libc++/libc++abi code servicing Mach-O calls could allocate outside the
  MachGate ownership path
- native testing also showed that calling an exported shim symbol from inside
  the shim can be interposed through the ELF PLT; `_ZdlPvm` logged the sized
  delete but initially did not reach the shim `free` tombstone path

Accepted fix:

- export C++ allocation/deallocation operators from `libsystem_shim.so`:
  ordinary, array, nothrow, aligned, sized, and sized-aligned forms
- route those operators through the same private shim allocation helpers used
  by Darwin malloc/default-zone callbacks
- split `malloc`, `free`, `calloc`, and `realloc` into private implementation
  helpers plus exported symbols, so internal shim calls cannot be hijacked by
  ELF symbol interposition
- extend `tests/test_libsystem_shim.sh` to directly call `_Znwm`,
  `_ZnwmSt11align_val_t`, `_ZdlPvm`, and `_ZdlPvmSt11align_val_t`

Validation:

- native `MACHGATE_TRACE_ALLOC=2 BUILD_DIR=... tests/test_libsystem_shim.sh`
  shows `_ZdlPvm` followed by the real shim `free` event and a tracked-freed
  tombstone
- ARM64 Docker `BUILD_DIR=/work/build-arm64 bash tests/run_tests.sh` passes
  `29 / 29`
- ARM64 `nm -D build-arm64/libsystem_shim.so` shows exported `_Znwm`,
  `_ZdlPvm`, and aligned operator forms

## v0.3.28 Allocator Contract Follow-Up

The `v0.3.27` private run still completes all static constructors and reaches
`_main`, but aborts in the guest memory tracker:

```text
machgate: mod_init progress 707/707
machgate: __mod_init_func constructors complete
machgate: calling _main at 0x10094754c (argc=1)
Memory.cpp(884) ASSERTION FAILED: rest >= size && rest <= kMaxHeapSize
Function: trackDeallocate
Message: rest=0, size=72
```

This moves the problem out of constructor ordering. The remaining failure shape
is an allocator ownership mismatch after guest code starts executing.

Independent audits from internal agents plus Grok and Agy agreed on the same
generic gaps:

- mapped Apple-ABI `libc++.so.1` and `libc++abi.so.1` can allocate through
  native ELF symbols before Mach-O guest bindings reach the shim
- flat Mach-O lookup preferred the host `RTLD_DEFAULT` path for allocator-like
  symbols unless they were already resolved by ordinal
- deferred binds did not use the C++ operator allocation hook path
- several Darwin libmalloc and malloc-zone symbols existed only as internal
  shim callbacks, so guest imports could fail or fall back incorrectly
- direct `memalign`, `aligned_alloc`, and `valloc` symbols were not exported as
  first-class Darwin allocator entry points

Accepted fix:

- normalize C++ operator symbol names so both Mach-O spellings (`__Znwm`) and
  ELF/dlsym spellings (`_Znwm`) route through the same MachGate allocation hooks
- use the operator hook path when completing deferred binds
- make flat lookup prefer the mapped libSystem shim for Darwin allocator and
  malloc-zone symbols before trying `RTLD_DEFAULT`
- export direct `memalign`, `aligned_alloc`, and `valloc` and route them through
  the private shim allocation helpers
- export the minimal Darwin malloc-zone query/control surface:
  `malloc_size`, `malloc_default_zone`, `malloc_create_zone`,
  `malloc_destroy_zone`, `malloc_set_zone_name`, `malloc_get_zone_name`,
  `malloc_zone_from_ptr`, `malloc_get_all_zones`, `malloc_num_zones`,
  `malloc_zones`, and the default zone callback functions
- add `tests/test_allocator_export_surface.sh` so release builds fail if the
  allocator ABI surface is not exported as versioned `GLIBC_2.17` symbols
- extend `tests/test_libsystem_shim.sh` to exercise direct alignment
  allocation and malloc-zone allocation, size, from-ptr, claimed-address,
  realloc, calloc, and valloc paths

Validation:

- native `BUILD_DIR=/home/kapablanka/repos/machgate/build bash tests/test_libsystem_shim.sh`
  passes
- native `BUILD_DIR=/home/kapablanka/repos/machgate/build bash tests/test_allocator_export_surface.sh`
  passes
- ARM64 Docker
  `BUILD_DIR=/work/build-arm64 bash tests/run_tests.sh` passes `30 / 30`

Remaining private validation:

- rerun the private `Core.UnitTest` with the next release tarball
- if it still aborts with `rest=0, size=72`, run with
  `MACHGATE_TRACE_ALLOC=1 MACHGATE_TRACE_ALLOC_SIZE=72 MACHGATE_TRACE_SIGNALS=1`
  and compare whether the failing 72-byte path is now visible in shim traces

## v0.3.29 Guest Operator Allocation Follow-Up

The `v0.3.28` private run still completes all static constructors and reaches
`_main`, then aborts in `Memory.cpp(884) trackDeallocate` with
`rest=0, size=72`.

The new hypothesis is stricter than the `v0.3.28` allocator export fix:

- some C++ binaries define their own global `operator new/delete` in the main
  Mach-O executable
- those operators can be backed by the program's own memory tracker
- MachGate's resolver was intercepting imported C++ operator symbols before it
  checked whether the main executable defined the same symbol
- that can allocate through the shim while deallocating through the guest
  tracker, which matches `trackDeallocate(rest=0)`

Accepted fix:

- normalize C++ operator symbols as before, but resolve the main executable's
  own symbol table first
- use MachGate's zeroing/shim allocator hook only when the guest Mach-O does not
  define the requested C++ operator
- apply the same rule to chained fixups, LC_DYLD_INFO binds, and deferred binds
- preserve trace binding source labels so `MACHGATE_TRACE_BINDINGS=1` can show
  whether a C++ operator bind used `main executable` or
  `machgate c++ allocator hook`

Validation:

- native allocator export and shim tests pass
- ARM64 Docker `BUILD_DIR=/work/build-arm64 bash tests/run_tests.sh` passes
  `30 / 30`

Expected private validation:

- rerun `Core.UnitTest` with `v0.3.29`
- if it still fails, rerun with
  `MACHGATE_TRACE_BINDINGS=1 MACHGATE_TRACE_ALLOC=1 MACHGATE_TRACE_ALLOC_SIZE=72`
  and inspect `*_Znwm`, `*_Znam`, `*_Zdl*` binding source lines

## v0.3.30 Scoped libc++ Allocator Overlay Follow-Up

The `v0.3.29` private run still completes all `707 / 707` static constructors
and reaches `_main`, then aborts in the guest allocator tracker:

```text
Memory.cpp(884) ASSERTION FAILED: rest >= size && rest <= kMaxHeapSize
Function: trackDeallocate
Message: rest=0, size=72
```

That proves the old constructor-table and libc++ tree initialization failures are
closed for this binary. The remaining failure is post-`_main` allocator
ownership: some allocation still reaches a runtime path outside the same
ownership model used by guest deallocation/accounting.

Accepted fix:

- Darwin malloc-zone APIs now preserve custom-zone ownership in the shim
  allocation ledger.
- `malloc_zone_malloc`, `calloc`, `realloc`, `memalign`, `valloc`,
  `free_definite_size`, `batch_malloc`, `batch_free`, `size`, and
  `malloc_zone_from_ptr` dispatch through custom zone callbacks when a guest or
  mapped runtime supplies a non-default Darwin zone.
- `malloc_zone_register` and `malloc_zone_unregister` maintain the exported
  Darwin `malloc_zones` / `malloc_num_zones` surface.
- The packaged Apple-ABI `libc++.so.1` and `libc++abi.so.1` are now linked with
  a scoped allocator overlay. Their internal ELF `malloc`, `free`, `calloc`,
  `realloc`, alignment allocation, `valloc`, and C++ operator new/delete calls
  route through `libsystem_shim.so` instead of directly reaching host glibc.
- The release workflow passes the exact ARM64 `libsystem_shim.so` to the libc++
  build so the packaged overlay cannot bind against a stale local shim.

Validation:

- native `tests/test_libsystem_shim.sh` passes with custom-zone callback
  coverage
- native `tests/test_allocator_export_surface.sh` passes
- native `tests/test_libcxx_allocator_overlay.sh` passes
- ARM64 Docker validation passes `31 / 31`

Expected private validation:

- rerun `Core.UnitTest` with the next release tarball
- if it still fails with `trackDeallocate(rest=0, size=72)`, rerun with the
  generic provenance trace:

```bash
MACHGATE_TRACE_ALLOC_MISMATCH=1 \
MACHGATE_TRACE_ALLOC=1 \
MACHGATE_TRACE_ALLOC_SIZE=72 \
MACHGATE_TRACE_SIGNALS=1 \
MACHGATE_VERBOSE=1 \
MACHGATE_TARBALL=/path/to/machgate-next-linux-arm64.tar.gz \
bin/run-macho-docker.sh /path/to/Core.UnitTest
```

`MACHGATE_TRACE_ALLOC_MISMATCH=1` is not size-specific. It dumps the recent
allocator ring when an unknown-pointer free/realloc or guest `abort` happens.
The size filter only keeps ordinary trace output small; the mismatch dump is
intended to catch the same ownership bug for any allocation size.
