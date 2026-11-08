#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
python3 - "$BUILD_DIR" <<'PY'
import ctypes
from pathlib import Path
import subprocess
import sys

build = Path(sys.argv[1]).resolve()
expected = set(Path('tests/fixtures/archive_shim_exports.txt').read_text().splitlines())
owned = set()
for library in ('libsystem_shim.so', 'libmachgate_foundation.so', 'libmachgate_icu.so', 'libmachgate_apple_compat.so'):
    output = subprocess.check_output(['nm', '-D', '--defined-only', str(build / library)], text=True)
    owned.update(line.split()[-1].split('@')[0] for line in output.splitlines() if len(line.split()) >= 3)
missing = expected - owned
assert not missing, 'archive exports missing from MachGate providers: ' + ', '.join(sorted(missing))
provider = ctypes.CDLL(str(build / 'libmachgate_apple_compat.so'))
for name in expected:
    provider[name]
print(f'Archive shim parity: {len(expected)}/{len(expected)} exports owned and resolvable')
PY
