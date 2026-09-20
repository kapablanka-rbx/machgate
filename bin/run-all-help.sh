#!/usr/bin/env bash
set -uo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
machgate_root="${MACHGATE_ROOT:-$(cd "$script_dir/.." && pwd)}"
image="${MACHGATE_IMAGE:-machgate-arm64-toolchain}"
args="${MACHGATE_GUEST_ARGS:---help}"

default_ct_dir=""
for candidate in \
    "$machgate_root/../game-engine-machgate/build/buck2/common-tests-macos-arm64-release" \
    "$machgate_root/../../game-engine-machgate/build/buck2/common-tests-macos-arm64-release" \
    "/home/coder/git/roblox/game-engine-machgate/build/buck2/common-tests-macos-arm64-release"; do
    if [ -d "$candidate" ]; then
        default_ct_dir="$(cd "$candidate" && pwd)"
        break
    fi
done
common_tests_dir="${COMMON_TESTS_DIR:-$default_ct_dir}"

if [ -z "$common_tests_dir" ] || [ ! -d "$common_tests_dir" ]; then
    echo "Common-tests directory not found. Set COMMON_TESTS_DIR." >&2
    exit 1
fi

engine_root="$(cd "$common_tests_dir/../../.." && pwd)"

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
SystemConfiguration = SKIP
AppKit = SKIP
libc++.1 = /machgate-libcxx/libc++.so.1'

pass_count=0
fail_count=0
failed_list=()
RESULTS_FILE="/tmp/machgate-help-results.txt"
echo "binary|status" > "$RESULTS_FILE"

for d in "$common_tests_dir"/*/; do
    name=$(basename "$d")
    unit_test=$(ls "$d"*.UnitTest 2>/dev/null | head -1)
    [ -z "$unit_test" ] && continue
    binary_path="$unit_test"

    echo "=== $name ==="
    docker run --rm --platform linux/arm64 \
        --ulimit core=0 \
        -v "$machgate_root/build-arm64:/opt/machgate-local:ro" \
        -v "$machgate_root/build-libcxx/lib:/machgate-libcxx:ro" \
        -v "$engine_root:$engine_root" \
        "$image" \
        bash -c '
            export LD_LIBRARY_PATH=/machgate-libcxx:/opt/machgate-local
            export MACHGATE_CONFIG=/tmp/machgate.conf
            printf "[general]\ndylib_map = /tmp/dylib_map.conf\n" > /tmp/machgate.conf
            printf "'"$DYLIB_MAP"'\n" > /tmp/dylib_map.conf
            exec /opt/machgate-local/machgate '"$binary_path"' '"$args"'
        '
    status=$?
    if [ $status -eq 0 ]; then
        echo "RESULT: PASS"
        echo "$name|PASS" >> "$RESULTS_FILE"
        pass_count=$((pass_count + 1))
    else
        echo "RESULT: FAIL (exit $status)"
        echo "$name|FAIL" >> "$RESULTS_FILE"
        fail_count=$((fail_count + 1))
        failed_list+=("$name")
    fi
    echo
done

echo "=========================================="
echo "--help: $pass_count passed, $fail_count failed"
[ $fail_count -gt 0 ] && echo "Failed: ${failed_list[*]}"
echo "Results: $RESULTS_FILE"
exit $fail_count
