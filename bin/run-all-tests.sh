#!/usr/bin/env bash
set -uo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
machgate_root="${MACHGATE_ROOT:-$(cd "$script_dir/.." && pwd)}"
image="${MACHGATE_IMAGE:-machgate-arm64-toolchain}"
run_flags="${RUN_FLAGS:-}"

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
libobjc = STUB
Foundation = SKIP
SystemConfiguration = SKIP
AppKit = SKIP
libc++.1 = /machgate-libcxx/libc++.so.1'

pass_count=0
partial_count=0
fail_count=0
failed_list=()
LOG_DIR="/tmp/machgate-test-logs"
mkdir -p "$LOG_DIR"
RESULTS_FILE="/tmp/machgate-test-results.txt"
echo "binary|status|summary" > "$RESULTS_FILE"

for d in "$common_tests_dir"/*/; do
    name=$(basename "$d")
    unit_test=$(ls "$d"*.UnitTest 2>/dev/null | head -1)
    [ -z "$unit_test" ] && continue

    echo "=== $name ==="
    echo "  machgate $unit_test $run_flags" >&2

    logfile="$LOG_DIR/$name.log"
    timeout --foreground --kill-after=10s "${TEST_TIMEOUT_SECONDS:-14400}s" \
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
            exec /opt/machgate-local/machgate '"$unit_test"' '"$run_flags"'
        ' 2>&1 | tee "$logfile"
    timeout_status=${PIPESTATUS[0]}
    if [ $timeout_status -eq 124 ] || [ $timeout_status -eq 137 ]; then
        status=124
    else
        status=$timeout_status
    fi

    summary=$(grep -E "test cases:|All tests passed|X_CHILD_STATUS|Status:|No errors|assertions:" "$logfile" 2>/dev/null | tail -3 | tr '\n' ' ')

    if [ $status -eq 0 ] && echo "$summary" | grep -qE "SUCCESS|All tests passed|No errors"; then
        echo "RESULT: PASS"
        echo "$name|PASS|$summary" >> "$RESULTS_FILE"
        pass_count=$((pass_count + 1))
    elif [ $status -eq 124 ]; then
        echo "RESULT: TIMEOUT (crash hang or long run — killed at ${TEST_TIMEOUT_SECONDS:-14400}s)"
        echo "$name|TIMEOUT|$summary" >> "$RESULTS_FILE"
        fail_count=$((fail_count + 1))
        failed_list+=("$name")
    elif echo "$summary" | grep -qE "test cases:|assertions:"; then
        echo "RESULT: PARTIAL — $summary"
        echo "$name|PARTIAL|$summary" >> "$RESULTS_FILE"
        partial_count=$((partial_count + 1))
    else
        echo "RESULT: FAIL (exit $status)"
        echo "$name|FAIL|$summary" >> "$RESULTS_FILE"
        fail_count=$((fail_count + 1))
        failed_list+=("$name")
    fi
    echo
    echo "---"
    echo
done

echo "=========================================="
echo "Tests: $pass_count passed, $partial_count partial, $fail_count failed, $((pass_count + partial_count + fail_count)) total"
[ $fail_count -gt 0 ] && echo "Failed: ${failed_list[*]}"
echo "Results: $RESULTS_FILE"
echo "Logs: $LOG_DIR/<binary>.log"
exit $fail_count
