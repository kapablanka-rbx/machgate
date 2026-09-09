# MachGate Rotest Integration Plan

## Goal

Run macOS and iOS ARM64 Mach-O common-test binaries on ARM64 Linux agents
through MachGate, integrated into rotest as a first-class platform mode, with
a TeamCity build targeting the ARM64 agent pool.

No Mac agents required. Build and test execution both happen on ARM64 Linux.

## Repositories and branches

- `game-engine-machgate` branch `research/armando/matchgate` — rotest
  integration, gobot config, TC build config
- `machgate` branch `master` — MachGate runtime (no changes needed, already
  works)

## Architecture

```
TeamCity ARM64 Linux agent
  |
  ├── gobot buck2/build common-tests/UnitTests macos/arm64
  |   (cross-compile, produces Mach-O binaries)
  |
  ├── cmake --build machgate (build MachGate for ARM64 Linux)
  |
  └── gobot rotest/rotest common-tests/UnitTests linux/arm64 --machgate
      |
      ├── rotest discovers test cases (machgate /path/test --list-test-cases)
      ├── rotest executes tests (machgate /path/test --run-test TestCaseName)
      └── each test binary runs through MachGate, results reported to TC
```

## Phase 1: Rotest integration

### 1.1 Add `--machgate` flag to rotest CLI

File: `Client/BuildScripts/rotest/librotest/roconfig.py`

- Add `--machgate` as a `store_true` flag, parallel to `--android` and `--ios`
- Add mutual exclusion at the existing `--android` / `--ios` check point
- Add `--machgate-binary` option to specify the MachGate binary path
  (default: `machgate` on PATH)

### 1.2 Add machgate platform to rotest

File: `Client/BuildScripts/rotest/librotest/platform.py`

- Add `machgate` to `initialize_target_platform()` — sets platform string
- Add machgate branch to `make_executable_launch_command()`:
  ```python
  elif platform == "machgate":
      result_command = [machgate_binary] + command
  ```
- The machgate binary path is resolved from `--machgate-binary` or PATH
- The dylib_map.conf and machgate.conf are generated once at init time

### 1.3 Add machgate initialization to rotest

File: `Client/BuildScripts/rotest/rotest.py`

- Add `machgate_initialize()` function, parallel to
  `android_initialize()` / `ios_initialize()`
- Generates `/tmp/machgate.conf` and `/tmp/dylib_map.conf` with:
  - `libSystem.B`, `CoreFoundation`, `CoreServices`, `Security`, `IOKit`,
    `libresolv`, `libicucore` -> `libsystem_shim.so`
  - `libz.1` -> `libz.so`
  - `libiconv` -> `libc.so.6`
  - `libobjc` -> `STUB`
  - `Foundation`, `SystemConfiguration`, `AppKit` -> `SKIP`
  - `libc++.1` -> Apple-ABI libc++ path
  - `libclang_rt.asan_osx_dynamic` -> `libsystem_shim.so` (when ASan shim
    is available)
- Sets `LD_LIBRARY_PATH` to include MachGate and libc++ directories
- Sets `MACHGATE_CONFIG` environment variable
- Calls `platform.initialize_platform_cmd_prefix([machgate_binary])`

### 1.4 Wire `--machgate` into rotest main

File: `Client/BuildScripts/rotest/rotest.py`

- In the platform initialization section (near android/ios init calls):
  ```python
  if args.machgate:
      machgate_initialize(args)
  ```
- Ensure both test execution and test discovery go through the machgate
  prefix (they already do — both use `make_executable_launch_command()`)

## Phase 2: Gobot configuration

### 2.1 Add machgate platform to common-tests

File: `gobot_configs/projects/common-tests.yaml`

- Add `machgate` to the `platform/arch` list for `*.UnitTest` and
  `UnitTests` targets
- Add machgate-specific options:
  - `sanitize: none` (ASan not yet supported in CI)
  - `machgate-binary`: path to MachGate executable
  - `machgate-libcxx`: path to Apple-ABI libc++

### 2.2 Add machgate OS definition

File: `gobot_configs/os/machgate.yaml` (new)

```yaml
machgate:
  options:
    - root-dir: 'build.{{build-tool}}'
    - build-dir: '{{root-dir}}/{{project}}/{{platform}}/{{arch}}/{{build-type}}'
    - sanitize: none
    - machgate-binary: '/opt/machgate/bin/machgate'
    - machgate-libcxx: '/opt/machgate/lib/libc++.so.1'
    - machgate-shim: '/opt/machgate/lib/libsystem_shim.so'
  arch:
    - arm64
  default: arm64
```

### 2.3 Add machgate to os config include

File: `gobot_configs/os/linux.yaml`

- Add `machgate: !include machgate.yaml` under the existing platform
  definitions

## Phase 3: TeamCity build

### 3.1 Build configuration

Create a new TeamCity build configuration: `MachGate Common Tests`

**Build steps:**

1. **Cross-compile common-tests** (existing gobot command):
   ```
   Tools/Util/gobot run buck2/build common-tests/UnitTests macos/arm64
     --preset ci xcode-toolchain=cross-compilation
     build-mode=remote materializations=all sanitize=none
   ```

2. **Build MachGate** (if not pre-installed on agent):
   ```
   cmake -S /opt/machgate-src -B /tmp/machgate-build -G Ninja
   cmake --build /tmp/machgate-build --parallel
   ```

3. **Build Apple-ABI libc++** (if not pre-installed):
   ```
   cd /opt/machgate-src
   git submodule update --init extern/llvm-project
   bash scripts/build-libcxx.sh
   ```

4. **Install MachGate to /opt/machgate**:
   ```
   cmake --install /tmp/machgate-build
   cp -a build-libcxx/lib/libc++.so* /opt/machgate/lib/
   cp -a build-libcxx/lib/libc++abi.so* /opt/machgate/lib/
   ```

5. **Run tests through MachGate**:
   ```
   Tools/Util/gobot run rotest/rotest common-tests/UnitTests
     machgate/arm64 --preset ci sanitize=none
   ```

**Agent requirements:**
- ARM64 Linux (aarch64)
- 16+ CPU cores
- 32+ GB RAM
- Docker with ARM64 support (for cross-compilation toolchain)
- MachGate pre-installed at `/opt/machgate` (or built in step 2)

### 3.2 Trigger rules

- Trigger on `research/armando/matchgate` branch pushes
- Nightly run against `master`
- Manual trigger via TeamCity UI

### 3.3 Artifact rules

- Publish test results XML to TeamCity
- Publish MachGate verbose logs on failure
- Publish `docs/COMMON_TESTS_PROGRESS.md` as build artifact

## Phase 4: Test binary environment

### 4.1 MachGate container image

Build a Docker image with MachGate pre-installed:

File: `.github/docker/machgate-runner.Dockerfile` (new)

```dockerfile
FROM ubuntu:24.04
RUN apt-get update && apt-get install -y \
    build-essential cmake ninja-build clang lld llvm \
    python3 git curl zlib1g-dev libatomic1
COPY machgate /opt/machgate/bin/machgate
COPY libsystem_shim.so /opt/machgate/lib/
COPY libc++.so.1 libc++abi.so.1 /opt/machgate/lib/
COPY machgate.conf /opt/machgate/etc/
COPY dylib_map.conf /opt/machgate/etc/
ENV PATH=/opt/machgate/bin:$PATH
ENV LD_LIBRARY_PATH=/opt/machgate/lib
ENV MACHGATE_CONFIG=/opt/machgate/etc/machgate.conf
```

### 4.2 Default config files

File: `/opt/machgate/etc/machgate.conf`
```ini
[general]
dylib_map = /opt/machgate/etc/dylib_map.conf
```

File: `/opt/machgate/etc/dylib_map.conf`
```
libSystem.B = /opt/machgate/lib/libsystem_shim.so
CoreFoundation = /opt/machgate/lib/libsystem_shim.so
CoreServices = /opt/machgate/lib/libsystem_shim.so
Security = /opt/machgate/lib/libsystem_shim.so
IOKit = /opt/machgate/lib/libsystem_shim.so
libresolv = /opt/machgate/lib/libsystem_shim.so
libicucore = /opt/machgate/lib/libsystem_shim.so
libz.1 = libz.so
libiconv = libc.so.6
libobjc = STUB
Foundation = SKIP
SystemConfiguration = SKIP
AppKit = SKIP
libc++.1 = /opt/machgate/lib/libc++.so.1
libclang_rt.asan_osx_dynamic = /opt/machgate/lib/libsystem_shim.so
```

## Phase 5: Unification (future)

The machgate scheduler mode will eventually unify with:
- Android container scheduler (ADB-based execution)
- Daemon scheduler (in-process asyncio, current desktop path)
- iOS simulator scheduler (simctl-based execution)

All share the same `make_executable_launch_command()` chokepoint. The
unification would create a single `PlatformRunner` interface with
implementations for each platform:
- `DesktopRunner` — direct exec (current Linux/Windows/macOS)
- `AndroidRunner` — ADB prefix (current Android)
- `IOSRunner` — simctl prefix (current iOS)
- `MachGateRunner` — machgate prefix (new)
- `PlayStationRunner` — prospero/orbis-run prefix (current PlayStation)

Not needed for the initial implementation — each platform mode stays
separate, just like Android and iOS are today.

## Implementation order

1. Phase 1.1-1.4: rotest `--machgate` flag (~30 lines, 3 files)
2. Phase 2.1-2.3: gobot config (~50 lines, 3 files)
3. Phase 4.1-4.2: MachGate runner image and config
4. Phase 3.1-3.3: TeamCity build configuration
5. Test on ARM64 agent

## Effort estimate

- Phase 1: hours
- Phase 2: hours
- Phase 3: hours
- Phase 4: hours
- Total: 1-2 days

## What this does NOT change

- MachGate source code — no changes needed
- Existing Android/iOS/PlayStation rotest paths — untouched
- Existing TC builds — untouched
- The `--machgate` flag is additive, parallel to `--android`/`--ios`
