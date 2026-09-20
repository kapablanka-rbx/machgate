#!/usr/bin/env bash
# Parallel corpus sweep: run every common-tests binary through MachGate,
# N at a time, each with its own stall watchdog. A wedged or hung binary
# costs only its own time — it never blocks other binaries.
#
# Usage:
#   bin/run-all-parallel.sh                  # all binaries, 4 at a time
#   bin/run-all-parallel.sh App Network Http # specific binaries only
#
# Configuration is in the block right below — edit the script to tune:
#   PARALLEL, STALL_SECONDS, TEST_TIMEOUT_SECONDS, SKIP_BINARIES, RUN_FLAGS
#
# Output: live status lines as each binary finishes, per-binary logs in
# /tmp/machgate-parallel-logs/, summary table in
# /tmp/machgate-parallel-results.txt
set -uo pipefail

# ================= Configuration =================
PARALLEL=16                # binaries running at the same time (48-core box)
STALL_SECONDS=120          # no-output watchdog per binary (0 = disabled)
TEST_TIMEOUT_SECONDS=1800  # hard ceiling per binary (room for slow passers)
SKIP_BINARIES=""           # space-separated binary names to skip, e.g. "App App_Group"
RUN_FLAGS=""               # extra args passed to every test binary
# ==================================================

script_dir="$(cd "$(dirname "$0")" && pwd)"
machgate_root="$(cd "$script_dir/.." && pwd)"
image="machgate-arm64-toolchain"
run_flags="$RUN_FLAGS"
parallel_jobs="$PARALLEL"

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
common_tests_dir="$default_ct_dir"

if [ -z "$common_tests_dir" ] || [ ! -d "$common_tests_dir" ]; then
    echo "Common-tests directory not found — edit common_tests_dir in this script." >&2
    exit 1
fi

engine_root="$(cd "$common_tests_dir/../../.." && pwd)"

LOG_DIR="/tmp/machgate-parallel-logs"
mkdir -p "$LOG_DIR"
RESULTS_FILE="/tmp/machgate-parallel-results.txt"
LOCK_FILE="/tmp/machgate-parallel-results.lock"
echo "binary|status|summary" > "$RESULTS_FILE"

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

stall_default="$STALL_SECONDS"
timeout_default="$TEST_TIMEOUT_SECONDS"

run_one() {
    local name="$1"
    local unit_test logfile cname exit_file
    local stall_seconds=$stall_default
    local hard_seconds=$timeout_default

    if ! [ "$stall_seconds" -gt 0 ] 2>/dev/null; then stall_seconds=99999999; fi

    unit_test=$(ls "$common_tests_dir/$name"/*.UnitTest 2>/dev/null | head -1)
    if [ -z "$unit_test" ]; then
        ( flock 9; echo "$name|SKIP|no .UnitTest binary" >> "$RESULTS_FILE" ) 9>"$LOCK_FILE"
        echo "[$name] SKIP — no .UnitTest binary"
        return
    fi

    logfile="$LOG_DIR/$name.log"
    exit_file="$LOG_DIR/$name.exit"
    cname="mg-parallel-$$-${name//./-}"
    rm -f "$logfile" "$exit_file"

    (
        timeout --foreground --kill-after=10s "${hard_seconds}s" \
          docker run --rm --platform linux/arm64 \
            --name "$cname" \
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
        echo "${PIPESTATUS[0]}" > "$exit_file"
    ) &
    local pipeline_pid=$!

    local last_size=0
    local last_growth=$(date +%s)
    local now
    while kill -0 "$pipeline_pid" 2>/dev/null; do
        sleep 15
        local cur_size
        cur_size=$(stat -c %s "$logfile" 2>/dev/null || echo 0)
        now=$(date +%s)
        if [ "$cur_size" -gt "$last_size" ]; then
            last_size=$cur_size
            last_growth=$now
        elif [ $((now - last_growth)) -ge "$stall_seconds" ]; then
            docker kill "$cname" >/dev/null 2>&1 || true
            last_growth=$now
        fi
    done
    wait "$pipeline_pid" 2>/dev/null
    docker rm -f "$cname" >/dev/null 2>&1 || true
    local status
    status=$(cat "$exit_file" 2>/dev/null || echo 1)

    local summary
    summary=$(grep -E "test cases:|All tests passed|X_CHILD_STATUS|Status:|No errors|assertions:" "$logfile" 2>/dev/null | tail -3 | tr '\n' ' ')

    local verdict
    if [ "$status" = "0" ] && echo "$summary" | grep -qE "SUCCESS|All tests passed|No errors"; then
        verdict=PASS
    elif [ "$status" = "124" ] || [ "$status" = "137" ]; then
        verdict=STALL
    elif echo "$summary" | grep -qE "test cases:|assertions:"; then
        verdict=PARTIAL
    else
        verdict="FAIL($status)"
    fi

    ( flock 9; echo "$name|$verdict|$summary" >> "$RESULTS_FILE" ) 9>"$LOCK_FILE"
    echo "[$name] $verdict — $(echo "$summary" | cut -c1-90)"
}

binaries=()
if [ $# -gt 0 ]; then
    binaries=("$@")
else
    for d in "$common_tests_dir"/*/; do
        name=$(basename "$d")
        case " $SKIP_BINARIES " in *" $name "*) continue;; esac
        ls "$d"*.UnitTest >/dev/null 2>&1 && binaries+=("$name")
    done
fi

total=${#binaries[@]}
echo "Parallel sweep: $total binaries, $parallel_jobs at a time, stall=${stall_default}s, ceiling=${timeout_default}s"
echo "Logs: $LOG_DIR/<binary>.log — results: $RESULTS_FILE"
echo

running=0
for name in "${binaries[@]}"; do
    run_one "$name" &
    running=$((running + 1))
    while [ "$running" -ge "$parallel_jobs" ]; do
        wait -n
        running=$((running - 1))
    done
done
wait

echo
echo "=========================================="
sort -t'|' -k1 "$RESULTS_FILE" | column -t -s'|' 2>/dev/null || cat "$RESULTS_FILE"
echo "=========================================="
pass=$(grep -c "|PASS|" "$RESULTS_FILE" || true)
partial=$(grep -c "|PARTIAL|" "$RESULTS_FILE" || true)
stall=$(grep -c "|STALL|" "$RESULTS_FILE" || true)
fail=$(grep -c "|FAIL" "$RESULTS_FILE" || true)
echo "Totals: $pass PASS, $partial PARTIAL, $stall STALL/HANG, $fail FAIL — $total binaries"
