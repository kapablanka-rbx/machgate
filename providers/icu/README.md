# ICU native provider

`libmachgate_icu.so` exposes 174 unsuffixed ICU C function names used by the
archive shim and implements them through installed ICU 74 libraries. It is a
separate Native Provider: MachGate's domain, application, loader, and system
shim do not depend on it. No archive ICU implementation is copied.

Select it through the dependency map:

```text
libicucore.A = /absolute/path/to/libmachgate_icu.so
libicucore = /absolute/path/to/libmachgate_icu.so
```

Both entries are explicit because alphabetic dependency versions are significant
in Native Linkage. A provider can also be selected for another exact dependency
name through the existing mapping mechanism.

## Interface and ABI

`exports.def` is the reviewed interface manifest. It records each function's
return type, parameter types, and return adaptation. The assembly entries and
ELF export map are generated from that manifest. `abi_checks.c` verifies every
prototype against the installed ICU headers at compile time, along with public
scalar and record sizes used across the boundary.

The reviewed source is ICU 74.2's public C headers (`unicode/*.h`) and its
`libicuuc.so.74` / `libicui18n.so.74` exports. The provider requires ICU major
version 74 and loads those versioned libraries. Its Unicode, locale, timezone,
collation, and formatting data are the host ICU data. `u_getVersion` reports the
actual library version; it does not report the archive shim's hard-coded 76.1.

ICU and its GNU C++ dependencies live in a separate glibc linker namespace,
created with `dlmopen`. The provider has no ELF dependency on ICU: its
constructor resolves every versioned C entry through the isolated runtime and
the Darwin entries call those addresses. ICU's GNU RTTI must not interpose the
guest's LLVM libc++abi RTTI. Those implementations export the same type-info
vtable names but have incompatible virtual methods; mixing them corrupts C++
exception matching. This boundary remains inside the ICU provider and needs no
guest library ordering rules. The runtime stays loaded for the provider's
process lifetime; missing libraries or entries stop initialization with a
diagnostic.

All 174 interfaces are non-variadic. They pass at most eight total scalar or
pointer parameters, including floating-point parameters, and return scalar or
pointer values. These parameter assignments agree between Darwin ARM64 and
Linux AAPCS64; there are no stack arguments, aggregates by value, or variadic
argument lists to translate in this manifest. The reference for register and
stack assignment is LLVM's `AArch64CallingConvention.td`, including
`CC_AArch64_DarwinPCS` and `CC_AArch64_AAPCS`.

Every entry preserves Darwin's reserved `x18` around the host call. This is
required: the installed Linux ICU code uses `x18`, and a guest regression fails
without preservation. Six signed-byte return values (`UBool` and `int8_t`) are
explicitly sign-extended for the Darwin return convention. Host ICU retains its
usual callee-saved general and floating-point register obligations.

Opaque objects must be created and consumed by this provider's ICU release.
Passing objects or data structures from another ICU release is unsupported.
Guest-defined `UText` callback implementations have not been validated. This
provider supplies the listed C API; it does not claim Apple-private ICU APIs,
ICU C++ ABI compatibility, or equivalence to another release's locale data.

## Build and verification

The parent build may include this directory with `add_subdirectory`. For an
isolated provider build:

```sh
cmake -S providers/icu -B build-icu-port -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-icu-port --parallel
MACHGATE_TEST_ICU_PROVIDER="$PWD/build-icu-port/libmachgate_icu.so" \
    bash tests/test_icu_provider.sh
MACHGATE_TEST_ICU_PROVIDER="$PWD/build-icu-port/libmachgate_icu.so" \
    bash tests/test_icu_runtime_isolation.sh
```

The test uses the normal `build/machgate` and `build/libsystem_shim.so`, unless
`BUILD_DIR` selects another runtime. It checks all 174 symbol lookups, Unicode
case expansion and locale-sensitive casing, canonical normalization, IDNA,
grapheme boundaries, floating-point number formatting, eight-argument date
formatter creation, version reporting, and `x18` preservation. The same behavior
runs through a real ARM64 Mach-O guest with the startup cache disabled, cold,
and warm, then through a native ARM64 executable.

The isolation test checks that loading the provider globally leaves the
process's C++ RTTI lookup unchanged. With Apple-compatible libc++ installed at
`/opt/machgate/lib/libc++.so.1` (or selected by `MACHGATE_TEST_LIBCXX`), a Mach-O
guest also catches an exception derived from the host library's `std::exception`.
It calls ICU before and after the throw, tests both dependency orders, and runs
with the cache disabled, cold and warm. The small fixture declares the public
virtual interface of `std::exception` so cross-compilation needs no macOS SDK;
its destructor, `what`, and type information come from the real libc++ library.

## Apple source cross-check

Apple's CF source at revision
`dc54c6bb1c1e5e0b9486c1d26dd5bef110b20bf3` was also consulted:
`CFICULogging.h` directly maps calendar/date/number operations to public ICU
functions; `CFDateFormatter.c` constructs UTF-16 timezone input and calls
`udat_open` with the eight parameters exercised here; `CFLocale.c` consumes
ICU locale and UTF-16 enumeration results; and `CFICUConverters.c` pairs ICU
converter objects with their ICU lifecycle functions. These sources confirm
Apple's use of the public C interfaces. They do not establish equivalence to
Apple's complete ICU distribution, which includes APIs beyond this manifest.
