#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
CLANG="${CLANG:-$(command -v clang || command -v clang-18)}"
LD64_LLD="${LD64_LLD:-$(command -v ld64.lld || command -v ld64.lld-18)}"
fixture_directory="$BUILD_DIR/commpage-fixtures"
mkdir -p "$fixture_directory"
"$CLANG" -target arm64-apple-macos11 -O2 -fno-builtin -fno-stack-protector \
    -nostdlib -c tests/fixtures/darwin_commpage.c -o "$fixture_directory/guest.o"
"$LD64_LLD" -arch arm64 -platform_version macos 11.0.0 11.0.0 \
    -e _main "$fixture_directory/guest.o" -o "$fixture_directory/guest"
MACHGATE_GUEST_NCPU=3 "$BUILD_DIR/machgate" "$fixture_directory/guest"

"${CC:-cc}" -D_GNU_SOURCE -std=gnu11 -Wall -Wextra -Werror -Isrc -rdynamic \
    tests/commpage_test.c src/adapters/guest_platform.c -ldl \
    -o "$fixture_directory/native-test"
"$fixture_directory/native-test"
python3 - "$BUILD_DIR/machgate" "$fixture_directory/guest" <<'PY'
import os
import signal
import subprocess
import sys

result = subprocess.run([*sys.argv[1:], 'write'],
                        env=dict(os.environ, MACHGATE_GUEST_NCPU='3'),
                        stdout=subprocess.PIPE, stderr=subprocess.PIPE)
assert result.returncode == -signal.SIGSEGV, result.returncode
print('commpage guest fields, CPU override and read-only protection passed')
PY
