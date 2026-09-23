#!/usr/bin/env bash
# Materialize the full CI common-tests corpus (optimized, ungrouped) and
# reproduce the RbxTransportProtocol stack-smashing crash locally.
#
# Run from ANY directory:
#   /home/coder/git/roblox/machgate/bin/materialize-and-repro.sh
set -euo pipefail

MACHGATE_HOME=/home/coder/git/roblox/machgate
ENGINE=/home/coder/git/roblox/game-engine-machgate
CORPUS="$ENGINE/build/buck2/common-tests-macos-arm64-optimized"
TARGET_DIR="$CORPUS/RbxTransportProtocol"

echo "== step 1: materialize corpus (skipped if already present) =="
if [ -f "$TARGET_DIR/RbxTransportProtocol.UnitTest" ]; then
    echo "already materialized: $TARGET_DIR"
else
    cd "$ENGINE"
    Tools/gobot/gobot-linux-amd64 run buck2/build common-tests/UnitTests macos/arm64 \
        build-mode=remote materializations=all sanitize=none \
        grouped-tests=off build-type=optimized xcode-toolchain=cross-compilation
fi

echo "== step 2: verify binary =="
ls -la "$TARGET_DIR/RbxTransportProtocol.UnitTest" || {
    echo "materialization did not produce the expected binary" >&2
    exit 1
}

echo "== step 3: repro stack smashing =="
cd "$MACHGATE_HOME"
COMMON_TESTS_DIR="$CORPUS" bin/repro-test.sh RbxTransportProtocol \
    --run_test='QuicSocket/write_stream_fin_closes_write_side_gracefully' 2>&1 | tee /tmp/rbxtransport-repro.log

echo "== done — full log at /tmp/rbxtransport-repro.log =="
echo "expected evidence: 'Could not set send buffer size' warning then"
echo "'*** stack smashing detected ***' from the guest"
