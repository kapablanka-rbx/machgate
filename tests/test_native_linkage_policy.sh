#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/.."

BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
FIXTURE_DIR="tests/fixtures"
PROVIDER="$(pwd)/$FIXTURE_DIR/libmachgate_test_provider.so"
MAP_FILE="$(mktemp)"
trap 'rm -f "$MAP_FILE"' EXIT

[ -f "$FIXTURE_DIR/dynamic_versioned_provider" ] || \
    bash "$FIXTURE_DIR/build_fixtures.sh"

run_status()
{
    local fixture="$1"
    local status=0
    MACHGATE_DYLIB_MAP="$MAP_FILE" MACHGATE_UNRESOLVED_IMPORTS=strict \
        "$BUILD_DIR/machgate" "$FIXTURE_DIR/$fixture" 2>/dev/null || status=$?
    printf '%s' "$status"
}

run_compat_status()
{
    local fixture="$1"
    local status=0
    MACHGATE_DYLIB_MAP="$MAP_FILE" MACHGATE_UNRESOLVED_IMPORTS=stub \
        "$BUILD_DIR/machgate" "$FIXTURE_DIR/$fixture" 2>/dev/null || status=$?
    printf '%s' "$status"
}

printf 'libMachGateFixture = %s\n' "$PROVIDER" > "$MAP_FILE"
[ "$(run_status dynamic_versioned_provider)" -eq 73 ]

printf 'libMachGateFixture = %s\nlibMachGateFixture.9 = STUB\n' \
    "$PROVIDER" > "$MAP_FILE"
[ "$(run_status dynamic_versioned_provider)" -eq 127 ]

: > "$MAP_FILE"
[ "$(run_status dynamic_lazy_unused)" -eq 41 ]
[ "$(run_status dynamic_lazy_called)" -eq 127 ]

printf 'libMachGateFixture = SKIP\n' > "$MAP_FILE"
[ "$(run_status dynamic_skipped_data)" -eq 0 ]

printf 'libMachGateFixture = STUB\n' > "$MAP_FILE"
[ "$(run_status dynamic_stubbed_function)" -eq 127 ]

printf 'libMachGateFixture = PARTIAL:%s\n' "$PROVIDER" > "$MAP_FILE"
[ "$(run_status dynamic_partial_provider)" -eq 73 ]
[ "$(run_status dynamic_partial_used_data)" -eq 139 ]

printf 'libMachGateFixture = %s\n' "$PROVIDER" > "$MAP_FILE"
[ "$(run_status dynamic_regular_lazy_unused)" -eq 44 ]
[ "$(run_status dynamic_regular_lazy_called)" -eq 127 ]
[ "$(run_status dynamic_chained_lazy_unused)" -eq 41 ]
[ "$(run_status dynamic_chained_lazy_called)" -eq 127 ]
[ "$(run_status dynamic_chained_data_missing)" -eq 1 ]

printf 'libSystem.B = %s/libsystem_shim.so\n' "$BUILD_DIR" > "$MAP_FILE"
[ "$(run_status dynamic_flat_provider_precedence)" -eq 69 ]

printf 'libMachGateFixture = UNMAPPED\n' > "$MAP_FILE"
[ "$(run_status dynamic_stubbed_function)" -eq 1 ]
[ "$(run_status dynamic_chained_stubbed_function)" -eq 1 ]
[ "$(run_status dynamic_chained_data_missing)" -eq 1 ]
[ "$(run_compat_status dynamic_chained_data_missing)" -eq 0 ]
[ "$(run_compat_status dynamic_partial_used_data)" -eq 0 ]
[ "$(run_compat_status dynamic_stubbed_function)" -eq 93 ]
[ "$(run_compat_status dynamic_chained_stubbed_function)" -eq 93 ]

[ "$(run_status dynamic_guest_definition)" -eq 67 ]

printf 'libMachGateFixture = %s\n' "$PROVIDER" > "$MAP_FILE"
[ "$(run_status dynamic_compiler_runtime)" -eq 66 ]
[ "$(run_status dynamic_compiler_floattidf)" -eq 68 ]
