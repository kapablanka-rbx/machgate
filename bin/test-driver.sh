#!/usr/bin/env bash
set -euo pipefail

MACHGATE_ROOT="${MACHGATE_ROOT:-$(cd "$(dirname "$0")/.." && pwd)}"
ENGINE_ROOT="${ENGINE_ROOT:-/Users/armandomartinez/git/roblox/game-engine}"
CORPUS="${CORPUS:-$ENGINE_ROOT/build/buck2/common-tests-macos-arm64-release}"
IMAGE="${MACHGATE_IMAGE:-machgate-arm64-toolchain}"

binary="$1"
shift
test_args="$*"

unit_test=$(ls "$CORPUS/$binary"/*.UnitTest 2>/dev/null | head -1)
if [ -z "$unit_test" ]; then
    echo "No .UnitTest in $CORPUS/$binary" >&2
    exit 2
fi

binary_dir=$(cd "$(dirname "$unit_test")" && pwd)
binary_name=$(basename "$unit_test")

timeout --foreground --kill-after=15s "${TEST_TIMEOUT:-60}s" \
  docker run --rm --platform linux/arm64 \
    --ulimit core=0 \
    -v "$MACHGATE_ROOT/build-arm64:/opt/machgate-local:ro" \
    -v "$MACHGATE_ROOT/build-libcxx/lib:/machgate-libcxx:ro" \
    -v "$ENGINE_ROOT:$ENGINE_ROOT" \
    -v "$binary_dir:/input:ro" \
    -w "$ENGINE_ROOT" \
    "$IMAGE" \
    bash -c '
        export LD_LIBRARY_PATH=/machgate-libcxx:/opt/machgate-local
        export MACHGATE_CONFIG=/tmp/machgate.conf
        printf "[general]\ndylib_map = /tmp/dylib_map.conf\n" > /tmp/machgate.conf
        printf "libSystem.B = /opt/machgate-local/libsystem_shim.so\nCoreFoundation = /opt/machgate-local/libsystem_shim.so\nCoreServices = /opt/machgate-local/libsystem_shim.so\nSecurity = /opt/machgate-local/libsystem_shim.so\nIOKit = /opt/machgate-local/libsystem_shim.so\nlibresolv = /opt/machgate-local/libsystem_shim.so\nlibicucore = /opt/machgate-local/libsystem_shim.so\nlibz.1 = libz.so\nlibiconv = libc.so.6\nlibobjc = /opt/machgate-local/libsystem_shim.so\nFoundation = /opt/machgate-local/libsystem_shim.so\nSystemConfiguration = SKIP\nAppKit = SKIP\nlibc++.1 = /machgate-libcxx/libc++.so.1\n" > /tmp/dylib_map.conf
        exec /opt/machgate-local/machgate '"$unit_test"' '"$test_args"'
    ' 2>&1
status=$?
exit $status
