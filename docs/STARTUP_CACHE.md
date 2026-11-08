# Image Preparation cache

MachGate reuses derived preparation artifacts on later launches of the same
Guest Executable. The cache is enabled by default. It stores instruction patch
sites, native-call references used by import classification, generated unwind
frames, and the information needed to rebuild their lookup index.

The Guest Executable remains unchanged. Each launch maps fresh guest memory,
binds current Native Providers, creates fresh Branch Islands, and resolves
unwind personality pointers for that process. Guest initializer state, runtime
heap data, provider addresses, and complete process memory are not cached.

## Controls

| Variable | Behavior |
| --- | --- |
| `MACHGATE_STARTUP_CACHE=0` | Disable cache reads and writes. `false` and `no` also disable it. |
| `MACHGATE_STARTUP_CACHE_DIR=/path` | Use this directory for cache files. If it is unavailable, continue with ordinary preparation. |
| `MACHGATE_LOG_STARTUP=1` | Report cache misses, hits, publication, and restored unwind metadata. |

Without an explicit directory, MachGate uses `.machgate-cache` beside the
resolved Guest Executable. If that directory is unavailable, it tries
`$XDG_CACHE_HOME/machgate`, or `$HOME/.cache/machgate`, or a per-user temporary
directory when neither variable is set.

## Validity and recovery

Cache identity includes the guest file and running MachGate executable's device,
inode, size, modification time, and change time, plus the selected ARM64 slice.
Changing either binary invalidates the artifact. Explicit trampoline settings
and the trampoline library's file identity also participate because redirection
can replace an instruction that would otherwise be a Syscall Site. If the
trampoline library cannot be identified before preparation, caching is skipped.

The versioned file format checks lengths and a checksum over its header and
payload. Missing, truncated, invalid, oversized, or unwritable caches fall back
to ordinary Image Preparation. Files are published through a unique temporary
file and atomic rename, so simultaneous launches cannot publish partial data.
The format is distinct from the archive's cache; existing archive artifacts are
not consumed.

## Ownership and layers

Executable Imaging owns the cache object for one Guest Image. The application
signals completed Image Preparation through `ports/guest_execution.h`, after
linkage, unwind registration, redirections, syscall patching, and runtime-image
setup succeed. The imaging adapter then publishes and releases the artifacts.
Failed preparation releases the cache with the Guest Image.

The store and serialization live in `adapters/startup_cache.c`. Each producer
retains responsibility for its artifact's meaning: syscall dispatch stays in
Darwin ABI Translation, import decisions stay in Native Linkage, and unwind
relocation stays in the unwind adapter. Domain and application code contain no
cache paths, file identities, serialization, or host-memory representation.

## Validation

`tests/test_startup_cache.sh` executes cold and warm thin/fat guests, raw
syscalls, thread-pointer adaptation, and C++ unwinding. It checks input-file
preservation, unchanged warm-cache reuse, runtime/guest invalidation, changed
redirections, disabled and unavailable caches, corrupt files, and concurrent
writers. Existing strict/lazy/weak import and syscall-range tests still apply
when caching is enabled.
