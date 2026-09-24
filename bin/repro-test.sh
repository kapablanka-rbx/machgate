#!/usr/bin/env bash
set -uo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
machgate_root="${MACHGATE_ROOT:-$(cd "$script_dir/.." && pwd)}"
image="${MACHGATE_IMAGE:-machgate-arm64-toolchain}"

default_ct_dir=""
for candidate in \
    "$machgate_root/../game-engine-machgate/build/buck2/common-tests-macos-arm64-optimized" \
    "$machgate_root/../../game-engine-machgate/build/buck2/common-tests-macos-arm64-optimized" \
    "/home/coder/git/roblox/game-engine-machgate/build/buck2/common-tests-macos-arm64-optimized" \
    "$machgate_root/../game-engine-machgate/build/buck2/common-tests-macos-arm64-release" \
    "$machgate_root/../../game-engine-machgate/build/buck2/common-tests-macos-arm64-release" \
    "/home/coder/git/roblox/game-engine-machgate/build/buck2/common-tests-macos-arm64-release"; do
    if [ -d "$candidate" ]; then
        default_ct_dir="$(cd "$candidate" && pwd)"
        break
    fi
done
common_tests_dir="${COMMON_TESTS_DIR:-$default_ct_dir}"
engine_root="$(cd "$common_tests_dir/../../.." && pwd)"

binary="$1"
shift
test_args="$*"

unit_test=$(ls "$common_tests_dir/$binary"/*.UnitTest 2>/dev/null | head -1)
if [ -z "$unit_test" ]; then
    echo "No .UnitTest in $common_tests_dir/$binary" >&2
    exit 2
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
libobjc = /opt/machgate-local/libsystem_shim.so
Foundation = /opt/machgate-local/libsystem_shim.so
AppKit = SKIP
SystemConfiguration = SKIP
libc++.1 = /machgate-libcxx/libc++.so.1'

set -f
docker_env=()
for env_name in MACHGATE_TRACE_SHIM MACHGATE_TRACE_SIGNALS MACHGATE_TRACE_BINDINGS MACHGATE_TRACE_SYSCALL MACHGATE_VERBOSE MACHGATE_TRACE_CXX_INIT MACHGATE_FD_TRACE_FILE MACHGATE_KEEP_CRASH_HANDLER; do
    if [ -n "${!env_name:-}" ]; then
        docker_env+=(-e "$env_name=${!env_name}")
    fi
done

timeout --foreground --kill-after=15s "${REPRO_TIMEOUT_SECONDS:-600}s" \
  docker run --rm --platform linux/arm64 \
    --ulimit core=0 \
    "${docker_env[@]}" \
    -v "$machgate_root/build-arm64:/opt/machgate-local:ro" \
    -v "$machgate_root/build-libcxx/lib:/machgate-libcxx:ro" \
    -v "$engine_root:$engine_root" \
    "$image" \
    bash -c '
        export LD_LIBRARY_PATH=/machgate-libcxx:/opt/machgate-local
        export MACHGATE_CONFIG=/tmp/machgate.conf
        printf "[general]\ndylib_map = /tmp/dylib_map.conf\n" > /tmp/machgate.conf
        printf "'"$DYLIB_MAP"'\n" > /tmp/dylib_map.conf
        exec /opt/machgate-local/machgate "$@"
    ' _ "$unit_test" $test_args 2>&1
status=$?
docker kill $(docker ps -q --filter ancestor="$image" --filter status=running) >/dev/null 2>&1 || true
exit $status
