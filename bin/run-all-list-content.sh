#!/usr/bin/env bash
set -uo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
machgate_root="${MACHGATE_ROOT:-$(cd "$script_dir/.." && pwd)}"
image="${MACHGATE_IMAGE:-machgate-arm64-toolchain}"

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

if [ -z "$common_tests_dir" ] || [ ! -d "$common_tests_dir" ]; then
    echo "Common-tests directory not found. Set COMMON_TESTS_DIR." >&2
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
libc++.1 = /machgate-libcxx/libc++.so.1'

pass_count=0
fail_count=0
failed_list=()
RESULTS_FILE="/tmp/machgate-list-results.txt"
echo "binary|status|test_count" > "$RESULTS_FILE"

for d in "$common_tests_dir"/*/; do
    name=$(basename "$d")
    unit_test=$(ls "$d"*.UnitTest 2>/dev/null | head -1)
    [ -z "$unit_test" ] && continue
    binary_name=$(basename "$unit_test")

    echo "=== $name ==="
    output=$(docker run --rm --platform linux/arm64 \
        -v "$machgate_root/build-arm64:/opt/machgate-local:ro" \
        -v "$machgate_root/build-libcxx/lib:/machgate-libcxx:ro" \
        -v "$d:/input:ro" \
        "$image" \
        bash -lc '
            export LD_LIBRARY_PATH=/machgate-libcxx:/opt/machgate-local
            export MACHGATE_CONFIG=/tmp/machgate.conf
            printf "[general]\ndylib_map = /tmp/dylib_map.conf\n" > /tmp/machgate.conf
            printf "'"$DYLIB_MAP"'\n" > /tmp/dylib_map.conf
            set +e
            /opt/machgate-local/machgate /input/'"$binary_name"' --list-content 2>&1
            echo "__GUEST_EXIT=$?"
        ')
    status=$?
    test_count=$(echo "$output" | grep -c "TEST CASE:")
    guest_exit=$(echo "$output" | grep "__GUEST_EXIT" | cut -d= -f2)

    echo "$output" | grep -v "__GUEST_EXIT" | tail -5

    if [ "$guest_exit" = "0" ]; then
        echo "RESULT: PASS ($test_count test cases)"
        echo "$name|PASS|$test_count" >> "$RESULTS_FILE"
        pass_count=$((pass_count + 1))
    else
        echo "RESULT: FAIL (exit $guest_exit)"
        echo "$name|FAIL|$test_count" >> "$RESULTS_FILE"
        fail_count=$((fail_count + 1))
        failed_list+=("$name")
    fi
    echo
done

echo "=========================================="
echo "--list-content: $pass_count passed, $fail_count failed"
[ $fail_count -gt 0 ] && echo "Failed: ${failed_list[*]}"
echo "Results: $RESULTS_FILE"
exit $fail_count
