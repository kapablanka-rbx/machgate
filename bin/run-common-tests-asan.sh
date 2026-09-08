#!/usr/bin/env bash
set -uo pipefail

usage()
{
    cat >&2 <<'EOF'
Usage: bin/run-common-tests-asan.sh [test-binary-name] [args...]

Runs game-engine common-tests macOS ARM64 Mach-O binaries (built with ASan)
through MachGate inside an ARM64 Docker container.

Prerequisites:
  - Docker with ARM64/QEMU support (binfmt)
  - MachGate built for ARM64: build-arm64/machgate + build-arm64/libsystem_shim.so
  - Apple-ABI libc++ built: build-libcxx/lib/libc++.so.1
  - game-engine common-tests built WITH sanitize=address at the path below
    (Tools/Util/gobot run buck2/build common-tests/UnitTests macos/arm64
     --preset ci xcode-toolchain=cross-compilation build-mode=remote
     materializations=all)

Environment:
  MACHGATE_ROOT         MachGate repo root (default: script location/../..)
  COMMON_TESTS_DIR      Path to common-tests-macos-arm64-release (auto-detected)
  MACHGATE_VERBOSE       Set to 1 for verbose MachGate output
  TEST_TIMEOUT          Per-binary timeout in seconds (default: 300, ASan is slow)
  RUN_FLAGS             Pass --flags=on or --flags=off to test binaries (default: none)

Examples:
  # Run a single binary
  bin/run-common-tests-asan.sh Luau.CLI

  # Run a single binary with test args
  bin/run-common-tests-asan.sh Luau.CLI --flags=on

  # Run all binaries
  bin/run-common-tests-asan.sh

  # Run all binaries with flags
  RUN_FLAGS="--flags=on" bin/run-common-tests-asan.sh
EOF
    exit 2
}

script_dir="$(cd "$(dirname "$0")" && pwd)"
machgate_root="${MACHGATE_ROOT:-$(cd "$script_dir/.." && pwd)}"
image="${MACHGATE_IMAGE:-machgate-arm64-toolchain}"
test_timeout="${TEST_TIMEOUT:-300}"
run_flags="${RUN_FLAGS:-}"

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

if [ -n "${1:-}" ] && { [ "$1" = "--help" ] || [ "$1" = "-h" ]; }; then
    usage
fi

if [ -z "$common_tests_dir" ] || [ ! -d "$common_tests_dir" ]; then
    echo "Common-tests directory not found." >&2
    echo "Set COMMON_TESTS_DIR to the path containing the test binary directories." >&2
    exit 1
fi

if [ ! -f "$machgate_root/build-arm64/machgate" ]; then
    echo "MachGate not built. Run inside Docker:" >&2
    echo "  docker run --rm --platform linux/arm64 -v \"\$PWD:/work\" -w /work machgate-arm64-toolchain bash -lc 'cmake -S . -B build-arm64 -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build-arm64 --parallel'" >&2
    exit 1
fi

if [ ! -f "$machgate_root/build-libcxx/lib/libc++.so.1" ]; then
    echo "Apple-ABI libc++ not built. Run:" >&2
    echo "  git submodule update --init extern/llvm-project" >&2
    echo "  docker run --rm --platform linux/arm64 -v \"\$PWD:/work\" -w /work machgate-arm64-toolchain bash scripts/build-libcxx.sh" >&2
    exit 1
fi

DYLIB_MAP='libSystem.B = /opt/machgate-local/libsystem_shim.so
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
libclang_rt.asan_osx_dynamic = /opt/machgate-local/libsystem_shim.so'

run_one()
{
    local name="$1"
    shift
    local guest_args="$*"
    local binary_dir="$common_tests_dir/$name"
    local unit_test
    unit_test=$(ls "$binary_dir"/*.UnitTest 2>/dev/null | head -1)
    if [ -z "$unit_test" ]; then
        echo "No .UnitTest binary found in $binary_dir" >&2
        return 1
    fi
    local binary_name
    binary_name=$(basename "$unit_test")

    local verbose_flag=""
    if [ -n "${MACHGATE_VERBOSE:-}" ]; then
        verbose_flag="-v"
    fi

    echo "  docker run --rm --platform linux/arm64 \\" >&2
    echo "    -v build-arm64:/opt/machgate-local:ro \\" >&2
    echo "    -v build-libcxx/lib:/machgate-libcxx:ro \\" >&2
    echo "    -v $name:/input:ro \\" >&2
    echo "    $image machgate $verbose_flag /input/$binary_name $guest_args" >&2

    docker run --rm --platform linux/arm64 \
        -v "$machgate_root/build-arm64:/opt/machgate-local:ro" \
        -v "$machgate_root/build-libcxx/lib:/machgate-libcxx:ro" \
        -v "$binary_dir:/input:ro" \
        "$image" \
        bash -lc '
            export LD_LIBRARY_PATH=/machgate-libcxx:/opt/machgate-local
            export MACHGATE_CONFIG=/tmp/machgate.conf
            printf "[general]\ndylib_map = /tmp/dylib_map.conf\n" > /tmp/machgate.conf
            printf "'"$DYLIB_MAP"'\n" > /tmp/dylib_map.conf
            set +e
            /opt/machgate-local/machgate '"$verbose_flag"' /input/'"$binary_name"' '"$guest_args"'
            echo "__GUEST_EXIT=$?"
        '
}

single_binary="${1:-}"
if [ -n "$single_binary" ] && [ "$single_binary" != "--all" ]; then
    binary_dir="$common_tests_dir/$single_binary"
    if [ ! -d "$binary_dir" ]; then
        echo "Binary directory not found: $binary_dir" >&2
        exit 1
    fi
    shift || true
    run_one "$single_binary" "$@"
    exit $?
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

echo "Found ${#binary_dirs[@]} ASan-enabled test binaries"
echo "Timeout per binary: ${test_timeout}s"
if [ -n "$run_flags" ]; then
    echo "Test flags: $run_flags"
fi
echo

pass_count=0
fail_count=0
failed_list=()

for name in "${binary_dirs[@]}"; do
    echo "=== $name ==="
    run_one "$name" $run_flags
    status=$?

    if [ $status -eq 0 ]; then
        echo "RESULT: PASS"
        pass_count=$((pass_count + 1))
    else
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
