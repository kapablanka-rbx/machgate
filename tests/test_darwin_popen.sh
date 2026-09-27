#!/bin/bash
# Test: guest popen() of a Mach-O guest re-execs through the MachGate loader.
# popen must not fall through to the host shell, which cannot exec Mach-O.
set -e
cd "$(dirname "$0")/.."

MACHGATE_ROOT="${MACHGATE_ROOT:-$(pwd)}"
BUILD_DIR="${BUILD_DIR:-$MACHGATE_ROOT/build}"

[ -f tests/fixtures/darwin_popen_exit42 ] || bash tests/fixtures/build_fixtures.sh

TEST_TMP="$(mktemp -d)"
trap 'rm -rf "$TEST_TMP"' EXIT

cat > "$TEST_TMP/dylib_map.conf" <<EOF
libSystem = $BUILD_DIR/libsystem_shim.so
EOF
cat > "$TEST_TMP/machgate.conf" <<EOF
[general]
dylib_map = $TEST_TMP/dylib_map.conf
EOF

status=0
(cd tests/fixtures && MACHGATE_CONFIG="$TEST_TMP/machgate.conf" "$BUILD_DIR/machgate" darwin_popen_exit42 2>/dev/null) || status=$?
[ "$status" -eq 42 ]
