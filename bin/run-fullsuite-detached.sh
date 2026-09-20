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

binaries=("$@")
if [ ${#binaries[@]} -eq 0 ]; then
    binaries=(App App_Group App.Script Network Physics RealtimeProtocol ScriptServices VideoStream)
fi

LOG_DIR="${LOG_DIR:-/tmp/machgate-fullsuite-logs}"
mkdir -p "$LOG_DIR"
RESULTS_FILE="$LOG_DIR/results.txt"
STATUS_FILE="$LOG_DIR/status.txt"
echo "binary|status|summary" > "$RESULTS_FILE"
echo "starting $(date)" > "$STATUS_FILE"

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

for name in "${binaries[@]}"; do
    unit_test=$(ls "$common_tests_dir/$name"/*.UnitTest 2>/dev/null | head -1)
    if [ -z "$unit_test" ]; then
        echo "SKIP $name: no .UnitTest binary" | tee -a "$STATUS_FILE"
        echo "$name|SKIP|no binary" >> "$RESULTS_FILE"
        continue
    fi

    echo "=== $name === ($(date +%H:%M:%S))" | tee -a "$STATUS_FILE"
    logfile="$LOG_DIR/$name.log"

    timeout --foreground --kill-after=15s "${TEST_TIMEOUT_SECONDS:-3600}s" \
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
            exec /opt/machgate-local/machgate '"$unit_test"'
        ' > "$logfile" 2>&1 &
    run_pid=$!

    last_note=0
    while kill -0 "$run_pid" 2>/dev/null; do
        sleep 30
        now=$(date +%s)
        if [ $((now - last_note)) -ge 300 ]; then
            last_note=$now
            tail -1 "$logfile" > "$LOG_DIR/$name.last" 2>/dev/null
            echo "$name: running... last: $(head -c 120 "$LOG_DIR/$name.last" 2>/dev/null)" >> "$STATUS_FILE"
        fi
    done
    wait "$run_pid"
    status=$?

    summary=$(grep -E "test cases:|All tests passed|No errors|assertions:|X_CHILD_STATUS" "$logfile" 2>/dev/null | tail -3 | tr '\n' ' ')

    if [ $status -eq 0 ] && echo "$summary" | grep -qE "All tests passed|No errors|SUCCESS"; then
        echo "$name|PASS|$summary" >> "$RESULTS_FILE"
        echo "$name: PASS ($summary)" | tee -a "$STATUS_FILE"
    elif [ $status -eq 124 ] || [ $status -eq 137 ]; then
        echo "$name|TIMEOUT|$summary" >> "$RESULTS_FILE"
        echo "$name: TIMEOUT" | tee -a "$STATUS_FILE"
    elif echo "$summary" | grep -qE "test cases:|assertions:"; then
        echo "$name|PARTIAL|$summary" >> "$RESULTS_FILE"
        echo "$name: PARTIAL ($summary)" | tee -a "$STATUS_FILE"
    else
        echo "$name|FAIL(exit $status)|$summary" >> "$RESULTS_FILE"
        echo "$name: FAIL exit=$status ($summary)" | tee -a "$STATUS_FILE"
    fi
    kill -9 "$(docker ps -q --filter ancestor=$image)" 2>/dev/null || true
done

echo "done $(date)" >> "$STATUS_FILE"
echo "Results:"
cat "$RESULTS_FILE"
