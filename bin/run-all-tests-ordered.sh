#!/usr/bin/env bash
set -uo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
machgate_root="${MACHGATE_ROOT:-$(cd "$script_dir/.." && pwd)}"
image="${MACHGATE_IMAGE:-machgate-arm64-toolchain}"

common_tests_dir=""
for candidate in \
    "$machgate_root/../game-engine-machgate/build/buck2/common-tests-macos-arm64-release" \
    "$machgate_root/../../game-engine-machgate/build/buck2/common-tests-macos-arm64-release" \
    "/home/coder/git/roblox/game-engine-machgate/build/buck2/common-tests-macos-arm64-release"; do
    if [ -d "$candidate" ]; then
        common_tests_dir="$candidate"
        break
    fi
done
common_tests_dir="${COMMON_TESTS_DIR:-$common_tests_dir}"

if [ -z "$common_tests_dir" ] || [ ! -d "$common_tests_dir" ]; then
    echo "Common-tests directory not found." >&2
    exit 1
fi

# Already-run binaries (from earlier in the session)
ALREADY_DONE="Luau.CLI Luau.Conformance Luau JoinTicketCrypto fmod Sqlite3 Installer.MacOS.Player Installer.MacOS.Studio RbxStorageBench OpenSSL"

# Order: smallest first, largest last. Skip already-done.
ORDERED="RbxStorage Voice.Stratus AssetProviderCommon LuauApiTraceProbe OpenTelemetryTracing Http PerformanceTelemetry AssetProviderCache SignalRCore AssetProvider AssetProviderHttp AssetProviderWorkflow RobloxTelemetry Core Geometry_Base_Group Physics_Base_Group Voice.Server Base_Group RealtimeProtocol VideoStream Physics ScriptServices App.Script Network App App_Group"

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
RESULTS_FILE="/tmp/machgate-test-results.txt"
echo "binary|status|summary" > "$RESULTS_FILE"

for name in $ORDERED; do
    binary_dir="$common_tests_dir/$name"
    if [ ! -d "$binary_dir" ]; then
        echo "=== $name === SKIP (not found)"
        echo
        continue
    fi
    unit_test=$(ls "$binary_dir"/*.UnitTest 2>/dev/null | head -1)
    if [ -z "$unit_test" ]; then
        echo "=== $name === SKIP (no .UnitTest)"
        echo
        continue
    fi
    binary_name=$(basename "$unit_test")

    echo "=== $name ==="
    echo "  docker run --rm --platform linux/arm64 \\"
    echo "    -v build-arm64:/opt/machgate-local:ro \\"
    echo "    -v build-libcxx/lib:/machgate-libcxx:ro \\"
    echo "    -v $name:/input:ro \\"
    echo "    $image machgate /input/$binary_name"
    echo

    logfile="/tmp/machgate-test-$name.log"
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
            /opt/machgate-local/machgate /input/'"$binary_name"'
            echo "__GUEST_EXIT=$?"
        ' 2>&1 | tee "$logfile"
    status=${PIPESTATUS[0]}

    summary=$(grep -E "test cases:|All tests passed|X_CHILD_STATUS|Status:|No errors|FAILURE|assertions:" "$logfile" 2>/dev/null | tail -3 | tr '\n' ' | ')

    if [ $status -eq 0 ]; then
        echo "RESULT: PASS"
        echo "$name|PASS|$summary" >> "$RESULTS_FILE"
        pass_count=$((pass_count + 1))
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
echo "Summary: $pass_count passed, $fail_count failed, $((pass_count + fail_count)) total"
if [ $fail_count -gt 0 ]; then
    echo "Failed: ${failed_list[*]}"
fi
echo
echo "Results file: $RESULTS_FILE"
cat "$RESULTS_FILE"
