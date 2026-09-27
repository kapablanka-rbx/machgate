# Storage suite fixes (RbxStorage + KeyValueStorage)

Date: 2026-09-27

## Symptoms

- `RbxStorage.UnitTest` 53 CRASHED + `.Flags` 52 CRASHED across the
  StorageInterfaceTests and RbxStorageTests families. The daemon logs
  showed most of these as TIMEOUT/CRASHED at the first case of each
  daemon batch, with per-case wall time of 77-170 seconds for the
  SqliteCache variants.
- `KeyValueStorage.UnitTest` 14+14 CRASHED in
  CookieSecureStorageCRUDTest / CookieSecureStorageInitialization.

## Root causes

### RbxStorage: sqlite VFS fell back to the nolock method table

The statically linked Darwin sqlite in the guest calls `statfs()` (the
libSystem symbol) from `autolockIoFinderImpl` to classify the
filesystem and choose a locking strategy. The shim did not export
`statfs`, so the bind resolved to glibc's statfs, which fills a
Linux-layout struct. sqlite then read Darwin-layout fields at
incorrect offsets:

- Darwin `f_flags` (buf+0x40) overlapped Linux `f_namelen` = 255,
  whose bit 0 made the check think the volume was not MNT_LOCAL.
- The fstypename comparison at buf+0x48 compared against Linux
  `f_frsize` bytes, never matching.

The result was the `nolockIoMethods` table, whose `xShmMap` slot is
NULL. WAL mode therefore never engaged (`PRAGMA journal_mode = WAL`
returned success but the DB header stayed 0x0101 rollback-journal),
and every commit in the multithreaded cache tests did a full
rollback-journal cycle: 36006 fsyncs at ~2 ms each on ext4 = 77+ s
per test case. Daemon per-case timeouts then killed the batch and
every later case in the group was reported CRASHED.

Fix: the shim now exports `statfs`/`fstatfs` with full
Darwin `struct statfs` translation (mirroring the syscall gate's
`linux_to_darwin_statfs`), reporting `MNT_LOCAL` set and
`f_fstypename` "hfs", so sqlite selects posix locking and WAL mode.
DB header byte 18 now reads 0x02 after the tests run.

### KeyValueStorage: Foundation gaps in the cookie-jar emulation

`CookieKeyValueStorage::create()` builds its storage key URL through
`NSURLComponents` and validates the root key with
`URLPathAllowedCharacterSet` / `stringByTrimmingCharactersInSet:`.
None of those selectors existed, `create()` returned nullptr, and the
fixture dereferenced a null storage pointer (SIGSEGV).

Fixes (landed across 49b27cc/071b0f3/2260647 and friends):

- `objc_msgSend` trampoline now saves the guest stack-arg area, so
  variadic selectors (`dictionaryWithObjectsAndKeys:`,
  `stringWithFormat:` with `%@`) see their full argument lists.
- NSURLComponents objects with `setScheme:`/`setHost:`/`setPath:`,
  `URL`, `componentsWithURL:resolvingAgainstBaseURL:`, `path`
  getter, `absoluteString`.
- NSCharacterSet class + `URLPathAllowedCharacterSet` and
  `stringByTrimmingCharactersInSet:` (trims URL-path-disallowed
  characters from both ends).
- `sharedCookieStorageForGroupContainerIdentifier:`,
  `cookieAcceptPolicy`, `setCookie:`,
  `cookieWithProperties:` (parses the properties dictionary built
  from stack args; NSHTTPCookieName/Value/Path/Domain/OriginURL key
  constants are exported as string objects and bound through the
  resolver), domain defaulted from OriginURL host.
- `struct shim_cookie.value` grown to 4096 bytes: the CRUD tests
  store 4096-byte values and every read came back truncated.
- Early duplicate `path` handler removed (it shadowed the full
  handler and returned the whole URL text as the cookie name); the
  NSURL `path` getter now skips scheme://host before finding '/'.

## Results

- RbxStorage full suite: pass in 20 s (was: first sqlite case 77 s+,
  batch timeouts, ~105 cases CRASHED/TIMEOUT).
- KeyValueStorage full suite: pass (was: 28 cases CRASHED, SIGSEGV in
  fixture ctor).
- machgate regression suite: 35/35.
- Base NamedMutex regression: pass.
- Http/ACRLocalizationCodec spot checks: no new failures (the
  remaining HttpCookieProtocolTest failures match the baseline).

## Commits

- 49b27cc shim: Foundation file/URL/string objects (also carries the
  statfs translation, msgSend stack-arg capture, and the bulk of the
  cookie-jar/URLComponents work from this investigation)
- 071b0f3 shim: objc_msgSend dispatch preserves args and returns
  doubles in v0
- 2260647 shim: CookieSecureStorage cookie round-trip fixes
  (value buffer size, path getters, OriginURL domain defaulting)
