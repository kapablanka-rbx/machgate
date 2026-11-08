#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/.."
export BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
[ -f tests/fixtures/darwin_cached_syscall ] || bash tests/fixtures/build_fixtures.sh

python3 - <<'PY'
import concurrent.futures
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

build = Path(os.environ['BUILD_DIR']).resolve()
fixtures = Path('tests/fixtures').resolve()

with tempfile.TemporaryDirectory(prefix='machgate-startup-cache-') as directory:
    work = Path(directory)
    runner = work / 'machgate'
    shutil.copy2(build / 'machgate', runner)
    env = dict(os.environ, MACHGATE_LOG_STARTUP='1',
               MACHGATE_SYSTEM_SHIM_LIB=str(build / 'libsystem_shim.so'))
    env.pop('MACHGATE_STARTUP_CACHE', None)
    env.pop('MACHGATE_STARTUP_CACHE_DIR', None)
    env.pop('MACHGATE_TRAMPOLINE_LIB', None)
    env.pop('MACHGATE_TRAMPOLINE_PREFIX', None)

    def run(guest, expected=0, overrides=None):
        result = subprocess.run([str(runner), str(guest)], env=env | (overrides or {}),
                                capture_output=True, timeout=15)
        assert result.returncode == expected, (guest.name, result.returncode, result.stderr.decode())
        return result

    def cached_pair(name, expected=0):
        guest = work / name
        shutil.copy2(fixtures / name, guest)
        original = hashlib.sha256(guest.read_bytes()).digest()
        cold = run(guest, expected)
        assert b'startup cache: published' in cold.stderr, cold.stderr.decode()
        warm = run(guest, expected)
        assert b'startup cache: hit' in warm.stderr, warm.stderr.decode()
        assert b'startup cache: published' not in warm.stderr, warm.stderr.decode()
        assert cold.stdout == warm.stdout
        assert hashlib.sha256(guest.read_bytes()).digest() == original
        return guest

    guest = cached_pair('darwin_write_stdout')
    cached_pair('darwin_tpidrro')
    cached_pair('fat_binary', 42)
    for name in ('dynamic_guest_unwind', 'dynamic_guest_unwind_registers'):
        unwind = cached_pair(name)
        warm = run(unwind)
        assert b'startup cache: restored unwind' in warm.stderr, warm.stderr.decode()

    redirected = work / 'darwin_cached_syscall'
    shutil.copy2(fixtures / redirected.name, redirected)
    redirection = {'MACHGATE_TRAMPOLINE_LIB': str(fixtures / 'libmachgate_test_provider.so'),
                   'MACHGATE_TRAMPOLINE_PREFIX': '_machgate_'}
    run(redirected, 99, redirection)
    run(redirected, 99, redirection)
    run(redirected, 9)
    run(redirected, 9)
    run(redirected, 99, redirection)

    soname_cache = work / 'soname-cache'
    soname_redirection = redirection | {
        'MACHGATE_TRAMPOLINE_LIB': 'libmachgate_test_provider.so',
        'LD_LIBRARY_PATH': str(fixtures),
        'MACHGATE_STARTUP_CACHE_DIR': str(soname_cache),
    }
    for _ in range(2):
        result = run(redirected, 99, soname_redirection)
        assert b'unavailable redirection identity' in result.stderr, result.stderr.decode()
        assert b'startup cache: hit' not in result.stderr
        assert b'startup cache: published' not in result.stderr
    assert not soname_cache.exists()

    cache_files = list((work / '.machgate-cache').glob('*.cache'))
    before = {p: p.read_bytes() for p in cache_files}
    disabled = run(guest, overrides={'MACHGATE_STARTUP_CACHE': '0'})
    assert b'startup cache: hit' not in disabled.stderr
    assert {p: p.read_bytes() for p in cache_files} == before

    status = guest.stat()
    os.utime(guest, ns=(status.st_atime_ns, status.st_mtime_ns + 1_000_000))
    stale = run(guest)
    assert b'startup cache: hit' not in stale.stderr
    assert b'startup cache: published' in stale.stderr
    assert b'startup cache: hit' in run(guest).stderr

    status = runner.stat()
    os.utime(runner, ns=(status.st_atime_ns, status.st_mtime_ns + 1_000_000))
    changed_runtime = run(guest)
    assert b'startup cache: hit' not in changed_runtime.stderr
    assert b'startup cache: published' in changed_runtime.stderr

    custom = work / 'custom'
    custom_env = {'MACHGATE_STARTUP_CACHE_DIR': str(custom)}
    assert b'startup cache: published' in run(guest, overrides=custom_env).stderr
    cache = next(custom.glob('*.cache'))
    valid = cache.read_bytes()
    for corrupted in (b'broken', valid[:len(valid) // 2], valid[:-1] + bytes([valid[-1] ^ 1])):
        cache.write_bytes(corrupted)
        result = run(guest, overrides=custom_env)
        assert b'startup cache: hit' not in result.stderr
        assert b'startup cache: published' in result.stderr
        assert b'startup cache: hit' in run(guest, overrides=custom_env).stderr

    unavailable = work / 'not-a-directory'
    unavailable.write_text('occupied')
    result = run(guest, overrides={'MACHGATE_STARTUP_CACHE_DIR': str(unavailable)})
    assert result.stdout == b'darwin write\n'
    assert b'startup cache: published' not in result.stderr

    parallel = work / 'parallel'
    with concurrent.futures.ThreadPoolExecutor(max_workers=6) as executor:
        results = list(executor.map(lambda _: run(guest, overrides={
            'MACHGATE_STARTUP_CACHE_DIR': str(parallel)}), range(12)))
    assert all(result.stdout == b'darwin write\n' for result in results)
    assert b'startup cache: hit' in run(guest, overrides={
        'MACHGATE_STARTUP_CACHE_DIR': str(parallel)}).stderr
    assert not list(parallel.glob('*.tmp.*'))

print('startup cache: cold/warm, unwind, syscall, thread-pointer, fat, invalidation, corruption, disabled, and concurrent runs passed')
PY
