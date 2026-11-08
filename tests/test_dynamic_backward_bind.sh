#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/.."

BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
MAP_FILE="$(mktemp)"
trap 'rm -f "$MAP_FILE"' EXIT
OBJ2YAML="${OBJ2YAML:-$(command -v obj2yaml || command -v obj2yaml-18 || true)}"

[ -f tests/fixtures/dynamic_backward_bind_1 ] || \
    bash tests/fixtures/build_fixtures.sh
[ -n "$OBJ2YAML" ] || { echo "obj2yaml not found" >&2; exit 1; }

printf 'libMachGateFixture = %s\n' \
    "$(pwd)/tests/fixtures/libmachgate_test_provider.so" > "$MAP_FILE"

expected_operands=(
    "0xffffffffffff8820"
    "0xffffffffffff7d80"
    "0xffffffffffffd148"
)

for case_index in 1 2 3; do
    fixture="tests/fixtures/dynamic_backward_bind_$case_index"
    yaml="$("$OBJ2YAML" "$fixture")"
    expected_operand="${expected_operands[$((case_index - 1))]}"
    [[ "$yaml" == *BIND_OPCODE_ADD_ADDR_ULEB* ]]
    [[ "${yaml,,}" == *"$expected_operand"* ]]

    MACHGATE_DYLIB_MAP="$MAP_FILE" \
        "$BUILD_DIR/machgate" "$fixture"
done
