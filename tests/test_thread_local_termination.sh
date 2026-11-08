#!/bin/bash
# Test: guest return and imported exit run TLV callbacks before Darwin TSD destructors.
set -euo pipefail

cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
probe_directory="$(mktemp -d)"
trap 'rm -rf "$probe_directory"' EXIT

cc -Wall -Wextra -Werror -o "$probe_directory/thread_local_exit_probe" \
    tests/fixtures/thread_local_exit_probe.c -ldl -lpthread

run_imported_exit()
{
    local output_file="$probe_directory/exit.log"
    local status=0

    LD_LIBRARY_PATH="$BUILD_DIR" "$probe_directory/thread_local_exit_probe" \
        "$output_file" exit || status=$?
    [ "$status" -eq 37 ]
    diff -u - "$output_file" <<'EOF'
tlv second
tlv first
tsd
EOF
}

run_guest_return()
{
    local fixture="tests/fixtures/darwin_thread_local_termination"
    local map_file="$probe_directory/dylib_map.conf"
    local status=0

    [ -f "$fixture" ] || bash tests/fixtures/build_fixtures.sh
    printf 'libSystem.B = %s/libsystem_shim.so\n' "$BUILD_DIR" > "$map_file"
    guest_output="$(MACHGATE_SYSTEM_SHIM_LIB="$BUILD_DIR/libsystem_shim.so" \
        MACHGATE_DYLIB_MAP="$map_file" \
        "$BUILD_DIR/machgate" "$fixture" 2>/dev/null)" || status=$?
    [ "$status" -eq 29 ]
    diff -u - <(printf '%s\n' "$guest_output") <<'EOF'
tlv second
tlv first
tsd
EOF
}

run_imported_exit
run_guest_return
