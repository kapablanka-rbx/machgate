#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/.."

BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
FIXTURE="tests/fixtures/darwin_variadic_abi"

[ -f "$FIXTURE" ] || bash tests/fixtures/build_fixtures.sh

output="$(
    MACHGATE_SYSTEM_SHIM_LIB="$BUILD_DIR/libsystem_shim.so" \
        "$BUILD_DIR/machgate" "$FIXTURE" 2>/dev/null
)"

expected=$'fprintf:11:stream:1.2:12:stack:2.8\nprintf:7:clean:3.5\nvfprintf:9000000001:room:2.25\nsnprintf:8:direct:0.5\nvsnprintf:42:safe:1.5\nsprintf:17:stack:2.5:18:tail:3.5\nsscanf:31:direct:4.25:32:tail:5.75\nfscanf:41:stream:6.25:42:file:7.75\nsprintf_chk:51:checked:8.5:52:stack:9.5\nvsprintf:61:list:1.5:62:stack:2.5\nvprintf:71:list:3.5:72:stack:4.5\nvsscanf:81:list:9.25:82:stack:10.75\nvfscanf:91:list:11.25:92:stack:12.75\nvasprintf:101:allocated:13.5:102:stack:14.5\nvsnprintf_chk:111:checked:15.5:112:stack:16.5\nvsprintf_chk:121:checked:17.5:122:stack:18.5'
[ "$output" = "$expected" ]
