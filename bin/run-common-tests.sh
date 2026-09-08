#!/usr/bin/env bash
set -uo pipefail

usage()
{
    cat >&2 <<'EOF'
Usage: bin/run-common-tests.sh [test-binary-name] [args...]

Runs game-engine common-tests macOS ARM64 Mach-O binaries through MachGate
inside an ARM64 Docker container.

Prerequisites:
  - Docker with ARM64/QEMU support (binfmt)
  - MachGate built for ARM64: build-arm64/machgate + build-arm64/libsystem_shim.so
  - Apple-ABI libc++ built: build-libcxx/lib/libc++.so.1
  - game-engine common-tests built with sanitize=none at the path below

Environment:
  MACHGATE_ROOT         MachGate repo root (default: script location/../..)
  COMMON_TESTS_DIR      Path to common-tests-macos-arm64-release (default: sibling game-engine-machgate)
  MACHGATE_IMAGE        Docker image (default: machgate-arm64-toolchain)
  MACHGATE_VERBOSE       Set to 1 for verbose MachGate output
  MACHGATE_TRACE_LCMAIN  Set to 1 for LC_MAIN tracing
  MACHGATE_TRACE_SIGNALS Set to 1 for signal tracing
  MACHGATE_TRACE_CXX_INIT Set to 1 for C++ initializer tracing
  TEST_TIMEOUT          Per-binary timeout in seconds (default: 120)
  RUN_FLAGS             Pass --flags=on or --flags=off to test binaries (default: none)

Examples:
  # Run a single binary
  bin/run-common-tests.sh Luau.CLI

  # Run a single binary with test args
  bin/run-common-tests.sh Sqlite3 --flags=on

  # Run all binaries
  bin/run-common-tests.sh

  # Run all binaries with flags
  RUN_FLAGS="--flags=on" bin/run-common-tests.sh
EOF
    exit 2
}

script_dir="$(cd "$(dirname "$0")" && pwd)"
machgate_root="${MACHGATE_ROOT:-$(cd "$script_dir/.." && pwd)}"
default_ct_dir=""
for candidate in \
    "$machgate_root/../game-engine-machgate/build/buck2/common-tests-macos-arm64-release" \
    "$machgate_root/../../game-engine-machgate/build/buck2/common-tests-macos-arm64-release" \
    "/home/coder/git/roblox/game-engine-machgate/build/buck2/common-tests-macos-arm64-release"; do
    if [ -d "$candidate" ]; then
        default_ct_dir="$candidate"
        break
    fi
done
common_tests_dir="${COMMON_TESTS_DIR:-$default_ct_dir}"
image="${MACHGATE_IMAGE:-machgate-arm64-toolchain}"
test_timeout="${TEST_TIMEOUT:-120}"
run_flags="${RUN_FLAGS:-}"

if [ -n "${1:-}" ] && [ "$1" = "--help" ] || [ -n "${1:-}" ] && [ "$1" = "-h" ]; then
    usage
fi

if [ -z "$common_tests_dir" ] || [ ! -d "$common_tests_dir" ]; then
    echo "Common-tests directory not found." >&2
    echo "Set COMMON_TESTS_DIR to the path containing the test binary directories." >&2
    echo "Expected something like: game-engine-machgate/build/buck2/common-tests-macos-arm64-release" >&2
    exit 1
fi

if [ ! -f "$machgate_root/build-arm64/machgate" ]; then
    echo "MachGate not built. Run:" >&2
    echo "  docker run --rm --platform linux/arm64 -v \"\$PWD:/work\" -w /work machgate-arm64-toolchain bash -lc 'cmake -S . -B build-arm64 -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build-arm64 --parallel'" >&2
    exit 1
fi

if [ ! -f "$machgate_root/build-libcxx/lib/libc++.so.1" ]; then
    echo "Apple-ABI libc++ not built. Run:" >&2
    echo "  git submodule update --init extern/llvm-project" >&2
    echo "  docker run --rm --platform linux/arm64 -v \"\$PWD:/work\" -w /work machgate-arm64-toolchain bash scripts/build-libcxx.sh" >&2
    exit 1
fi

docker_args=(
    docker run --rm --platform linux/arm64
    -v "$machgate_root/build-arm64:/opt/machgate-local:ro"
    -v "$machgate_root/build-libcxx/lib:/machgate-libcxx:ro"
)

for env_name in MACHGATE_VERBOSE MACHGATE_TRACE_LCMAIN MACHGATE_TRACE_SIGNALS MACHGATE_TRACE_CXX_INIT MACHGATE_TRACE_SYSCALL MACHGATE_TRACE_ALLOC; do
    if [ -n "${!env_name:-}" ]; then
        docker_args+=(-e "$env_name=${!env_name}")
    fi
done

single_binary="${1:-}"
if [ -n "$single_binary" ] && [ "$single_binary" != "--all" ]; then
    binary_dir="$common_tests_dir/$single_binary"
    if [ ! -d "$binary_dir" ]; then
        echo "Binary directory not found: $binary_dir" >&2
        exit 1
    fi
    unit_test=$(ls "$binary_dir"/*.UnitTest 2>/dev/null | head -1)
    if [ -z "$unit_test" ]; then
        echo "No .UnitTest binary found in $binary_dir" >&2
        exit 1
    fi
    binary_name=$(basename "$unit_test")
    shift || true
    guest_args="$*"

    docker_args+=(-v "$binary_dir:/input:ro")
    docker_args+=("$image")
    docker_args+=(bash -lc)

    config_script='
set +e
export LD_LIBRARY_PATH=/machgate-libcxx:/opt/machgate-local
export MACHGATE_CONFIG=/tmp/machgate.conf
cat > /tmp/machgate.conf <<CONFEOF
[general]
dylib_map = /tmp/dylib_map.conf
CONFEOF
cat > /tmp/dylib_map.conf <<MAPEOF
libSystem.B = /opt/machgate-local/libsystem_shim.so
CoreFoundation = /opt/machgate-local/libsystem_shim.so
CoreServices = /opt/machgate-local/libsystem_shim.so
Security = /opt/machgate-local/libsystem_shim.so
IOKit = /opt/machgate-local/libsystem_shim.so
libresolv = /opt/machgate-local/libsystem_shim.so
libicucore = /opt/machgate-local/libsystem_shim.so
libz.1 = libz.so
libiconv = libc.so.6
libobjc = STUB
Foundation = SKIP
SystemConfiguration = SKIP
AppKit = SKIP
libc++.1 = /machgate-libcxx/libc++.so.1
MAPEOF
machgate_args=()
if [ "${MACHGATE_VERBOSE:-0}" = "1" ]; then
    machgate_args+=(-v)
fi
/opt/machgate-local/machgate "${machgate_args[@]}" /input/'"$binary_name"' '"$guest_args"'
echo "__GUEST_EXIT=$?"
'
    "${docker_args[@]}" "$config_script"
    exit ${PIPESTATUS[0]}
fi

binary_dirs=()
for d in "$common_tests_dir"/*/; do
    unit_test=$(ls "$d"*.UnitTest 2>/dev/null | head -1)
    if [ -n "$unit_test" ]; then
        binary_dirs+=("$(basename "$d")")
    fi
done

if [ ${#binary_dirs[@]} -eq 0 ]; then
    echo "No .UnitTest binaries found in $common_tests_dir" >&2
    exit 1
fi

echo "Found ${#binary_dirs[@]} test binaries"
echo "Timeout per binary: ${test_timeout}s"
if [ -n "$run_flags" ]; then
    echo "Test flags: $run_flags"
fi
echo

pass_count=0
fail_count=0
failed_list=()

for name in "${binary_dirs[@]}"; do
    binary_dir="$common_tests_dir/$name"
    unit_test=$(ls "$binary_dir"/*.UnitTest 2>/dev/null | head -1)
    binary_name=$(basename "$unit_test")
    echo "=== $name ==="

    docker_args_single=(
        docker run --rm --platform linux/arm64
        -v "$machgate_root/build-arm64:/opt/machgate-local:ro"
        -v "$machgate_root/build-libcxx/lib:/machgate-libcxx:ro"
        -v "$binary_dir:/input:ro"
    )

    for env_name in MACHGATE_VERBOSE MACHGATE_TRACE_LCMAIN MACHGATE_TRACE_SIGNALS MACHGATE_TRACE_CXX_INIT MACHGATE_TRACE_SYSCALL MACHGATE_TRACE_ALLOC; do
        if [ -n "${!env_name:-}" ]; then
            docker_args_single+=(-e "$env_name=${!env_name}")
        fi
    done

    docker_args_single+=("$image")
    docker_args_single+=(bash -lc)

    config_script='
set +e
export LD_LIBRARY_PATH=/machgate-libcxx:/opt/machgate-local
export MACHGATE_CONFIG=/tmp/machgate.conf
cat > /tmp/machgate.conf <<CONFEOF
[general]
dylib_map = /tmp/dylib_map.conf
CONFEOF
cat > /tmp/dylib_map.conf <<MAPEOF
libSystem.B = /opt/machgate-local/libsystem_shim.so
CoreFoundation = /opt/machgate-local/libsystem_shim.so
CoreServices = /opt/machgate-local/libsystem_shim.so
Security = /opt/machgate-local/libsystem_shim.so
IOKit = /opt/machgate-local/libsystem_shim.so
libresolv = /opt/machgate-local/libsystem_shim.so
libicucore = /opt/machgate-local/libsystem_shim.so
libz.1 = libz.so
libiconv = libc.so.6
libobjc = STUB
Foundation = SKIP
SystemConfiguration = SKIP
AppKit = SKIP
libc++.1 = /machgate-libcxx/libc++.so.1
MAPEOF
machgate_args=()
if [ "${MACHGATE_VERBOSE:-0}" = "1" ]; then
    machgate_args+=(-v)
fi
/opt/machgate-local/machgate "${machgate_args[@]}" /input/'"$binary_name"' '"$run_flags"'
exit $?
'

    output=$("${docker_args_single[@]}" "$config_script" 2>&1)
    status=$?

    if [ $status -eq 0 ]; then
        summary=$(echo "$output" | grep -E 'test cases:|All tests passed|X_CHILD_STATUS|Status:' | tail -3)
        if [ -n "$summary" ]; then
            echo "$summary"
        else
            echo "(no test summary output)"
        fi
        echo "RESULT: PASS (exit 0)"
        pass_count=$((pass_count + 1))
    else
        echo "$output" | tail -5
        echo "RESULT: FAIL (exit $status)"
        fail_count=$((fail_count + 1))
        failed_list+=("$name")
    fi
    echo
done

echo "=========================================="
echo "Summary: $pass_count passed, $fail_count failed, $((pass_count + fail_count)) total"
if [ $fail_count -gt 0 ]; then
    echo "Failed: ${failed_list[*]}"
fi
