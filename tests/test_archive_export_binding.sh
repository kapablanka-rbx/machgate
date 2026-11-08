#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT
CLANG="$(command -v clang || command -v clang-18)"
LINKER="$(command -v ld64.lld || command -v ld64.lld-18)"

python3 - "$WORK_DIR" <<'PY'
from pathlib import Path
import sys

work = Path(sys.argv[1])
names = Path('tests/fixtures/archive_shim_exports.txt').read_text().splitlines()
provider = ['.data', '.p2align 3']
guest = ['.data', '.p2align 3', 'import_addresses:']
for name in names:
    provider += [f'.globl _{name}', f'_{name}:', '.quad 0']
    guest += [f'.quad _{name}']
guest += [
    '.text', '.globl _main', '_main:',
    'adrp x9, import_addresses@PAGE',
    'add x9, x9, import_addresses@PAGEOFF',
    f'mov x8, #{len(names)}',
    '1:', 'ldr x10, [x9], #8', 'cbz x10, 2f',
    'subs x8, x8, #1', 'b.ne 1b', 'mov x0, #0', 'b 3f',
    '2:', 'mov x0, #41', '3:', 'mov x16, #1', 'svc #0x80',
]
(work / 'provider.s').write_text('\n'.join(provider) + '\n')
(work / 'guest.s').write_text('\n'.join(guest) + '\n')
PY

"$CLANG" -target arm64-apple-macos11 -c "$WORK_DIR/provider.s" -o "$WORK_DIR/provider.o"
"$LINKER" -arch arm64 -platform_version macos 11.0 11.0 -dylib \
    -install_name /usr/lib/libSystem.B.dylib -o "$WORK_DIR/libSystem.B.dylib" \
    "$WORK_DIR/provider.o"
"$CLANG" -target arm64-apple-macos11 -c "$WORK_DIR/guest.s" -o "$WORK_DIR/guest.o"
"$LINKER" -arch arm64 -platform_version macos 11.0 11.0 -e _main \
    -o "$WORK_DIR/guest" "$WORK_DIR/guest.o" "$WORK_DIR/libSystem.B.dylib"
printf 'libSystem.B = %s/libmachgate_apple_compat.so\n' "$BUILD_DIR" > "$WORK_DIR/providers.conf"
for CACHE in 0 1 1; do
    MACHGATE_STARTUP_CACHE="$CACHE" MACHGATE_STARTUP_CACHE_DIR="$WORK_DIR/cache" \
    MACHGATE_UNRESOLVED_IMPORTS=strict MACHGATE_DYLIB_MAP="$WORK_DIR/providers.conf" \
    MACHGATE_SYSTEM_SHIM_LIB="$BUILD_DIR/libmachgate_apple_compat.so" \
    timeout -k 2 10 "$BUILD_DIR/machgate" "$WORK_DIR/guest"
done
printf '%s\n' 'Archive exports: all 849 names bound in a strict Mach-O guest, cache disabled/cold/warm'
