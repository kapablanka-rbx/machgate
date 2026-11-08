# Apple ABI references used for the archive port

The November 8 targeted TC/local failure round checked these contracts:

- XNU `bsd/sys/xattr.h`, `bsd/sys/errno.h`,
  `bsd/vfs/vfs_syscalls.c` and `bsd/vfs/vfs_xattr.c`: Darwin xattr names
  occupy an unqualified namespace, missing attributes report `ENOATTR` (93),
  and CREATE/REPLACE options differ from Linux. Direct syscall and guest
  import paths share one backend that maps names into Linux `user.` storage,
  removes that prefix when listing, and translates errors contextually.
  Nonzero resource-fork positions and extended options remain unsupported.
- Libc `stdlib/FreeBSD/setenv.c`, `getenv.c` and `putenv.c`: environment
  mutation and lookup use the array returned by `_NSGetEnviron`; `putenv`
  retains the supplied storage. Guest mutation adapters now update that
  same launch environment after host libc changes its array. The regression
  verifies replacement, overwrite control, removal and borrowed storage.
- dyld `dyld/DyldAPIs.cpp::dladdr` and
  `common/MachOLoaded.cpp::findClosestSymbol`: report the containing Mach-O
  image and the closest real section symbol, strip the linker underscore,
  and use `__dso_handle` for an image-header address. The image adapter caches
  sorted symbol indexes; its port exposes descriptions without leaking Mach-O
  parsing into the shim. Tests include an executable and an adjacent dylib.
- Libc `gen/backtrace.c`: Darwin symbol strings use image, hexadecimal PC,
  symbol and decimal offset fields. The descriptor writer uses a bounded
  stack buffer. Shared-cache redaction/UUID lookup is not implemented.
- LLVM `llvm/include/llvm/BinaryFormat/MachO.h`: `nlist_64`, N_STAB/N_SECT
  and section numbering. The full source reviewed is the sibling
  `machgate-archive/extern/llvm-project`; the current `extern/llvm-project`
  directory is not a complete source checkout.

Archive revision `8848b2ebad98b5cccade03df02477393b342d2f0` retains the xattr
namespace/error defect and dyld image stubs. Its environment lookup also
prefers the launch snapshot, with a special HOME override, and contains no
matching guest mutation adapters. Copying those implementations would not
provide these corrections. A fresh compiled-shim comparison still finds all
849 archive exports in the current providers; export presence is independent
of behavioral correctness. No GUI or workload-specific implementation is
part of this round.

The November 8 iOS follow-up corrected mutex attribute type translation:

- libpthread `include/pthread/pthread.h:166-169` defines NORMAL0,
  ERRORCHECK1 and RECURSIVE2. Linux reverses ERRORCHECK and RECURSIVE.
  `src/pthread_mutex.c:341-349,427-441` defines get/set behavior and rejects
  values outside those three types. Guest imports use explicit adapters;
  native ELF providers retain Linux enum semantics.
- libpthread `include/sys/_pthread/_pthread_types.h:37-47,75-83` defines
  the LP64 opaque storage used by the Mach-O regression: 16-byte attributes
  and a 64-byte mutex. The implementation uses the host attribute API for
  its opaque backing; it does not interpret Apple private bitfields.
- LLVM `libcxx/include/__threading_support:289-307` in the archive checkout
  initializes recursive mutexes using the native pthread headers when built
  as an ELF provider. The native-provider regression checks that this path
  remains recursive. Archive `src/shim/libsystem_shim.c` has no guest
  mutexattr type adapters; its static-mutex recursion workaround does not
  repair explicitly initialized guest attributes.

`tests/test_pthread_mutex_attributes.sh` verifies normal and recursive
relocking, error-checking EDEADLK, invalid type rejection, native-provider
isolation, and disabled/cold/warm startup caches.

The third filtered repair batch checked these contracts against the same
pinned sources below:

- XNU `bsd/sys/event.h:304` and `bsd/kern/kern_event.c:1468`: timer units,
  accumulated expirations, non-repeating absolute deadlines, and immediate
  non-repeating delivery for nonpositive intervals. Calendar deadlines are
  converted once to a monotonic delay. Mach time and continuous-time timer
  modes remain unsupported. A Linux timer backend sits behind
  `ports/event_timer.h`; syscall interception tests verify clock selection
  without changing the host clock.
- XNU `libsyscall/mach/mach_init.c:72`, `mach/mach/vm_page_size.h:42` and
  `osfmk/mach/arm/vm_types.h:105`: ARM64 `vm_size_t`, the page-size globals,
  and the `host_page_size` output occupy eight bytes. The existing advertised
  page geometry is unchanged.
- XNU `bsd/sys/socket.h:174`, `bsd/kern/uipc_socket.c:4959,5976` and
  `bsd/kern/uipc_socket2.c:393`: `SO_NOSIGPIPE` is socket state, can be read
  and cleared, and is inherited by accepted sockets. Imported and direct
  syscall paths share `ports/socket_state.h` and the Linux backend. Duplicated
  descriptors share the setting; send/write/writev suppress SIGPIPE only
  when requested, without changing the process signal handler.
- libpthread `src/pthread_tsd.c`: key deletion permits slot reuse and does
  not run destructors. Generation tracking prevents values from a previous
  slot lifetime from reappearing. The Darwin entry preserves the eight-byte
  key ABI and calls the internal implementation without ELF interposition.

Archive comparison found shared defects in timer support, page-size global
width, and SO_NOSIGPIPE state. These repairs are contract corrections, not
claims that copying archive code alone provides equivalent behavior. Queue
descriptor lifetime and alias tests also guard against stale registrations
after close/reopen. Export parity remains separate from API correctness.

The second filtered repair batch also verified these contracts:

- `libpthread/include/sys/_pthread/_pthread_types.h:112`: ARM64 Darwin
  `pthread_key_t` is eight bytes; guest import adapters preserve that width.
- `Libc/include/secure/_stdio.h:38` and LLVM
  `clang/lib/CodeGen/TargetInfo.cpp:6238`: fortified snprintf signature and
  Darwin variadic argument access. The adapter applies only to guest imports.
- `Libc/gen/errlst.c` and `Libc/string/FreeBSD/strerror.c`: error messages,
  truncation and unknown-error behavior. The copied message table retains
  the original notices in `src/shim/darwin_error_messages_compat.c`.
- `xnu/osfmk/mach/vm_region.h:54,83`: four-byte packing, 36-byte basic
  region information, nine integer words, and the 64-bit offset at byte20.
  Current-process basic queries use a separate Linux mapping adapter.
- `CF/CFString.c:4294,4413`: mutable string append and formatting. The
  optional Foundation provider tests Unicode, independent copies and
  encoding-cache invalidation; this is not full Foundation coverage.
- `Libc/include/limits.h:116`, `libpthread/src/pthread.c:470` and
  `libpthread/include/pthread/pthread_impl.h:54-56`: Darwin stack minimum
  and scheduling values OTHER1/FIFO4/RR2. Guest attributes preserve their
  requested stack size while host-owned backing meets the Linux minimum.

The November 2026 port was checked against the official Apple OSS repositories
listed below and the existing LLVM checkout in `machgate-archive/extern`.
These are source references for specific contracts, not a claim that every
Apple subsystem has been audited or implemented. XNU alone does not define
Objective-C, Foundation, libdispatch, or ICU behavior.

The local Apple checkouts are in
`../kapa-prompts/machgate/references/{repository}`. The reviewed LLVM files have
no local modifications at the recorded revision. Full macOS Foundation is not
contained in these Apple OSS repositories; the limited Foundation provider is
an archive port with explicit ABI and behavior tests.

| Official repository | Pinned revision | Files reviewed and resulting checks |
| --- | --- | --- |
| [xnu](https://github.com/apple-oss-distributions/xnu) | `f6217f891ac0bb64f3d375211650a4c1ff8ca1ea` | `bsd/sys/mount.h`, `bsd/sys/stat.h`, `osfmk/mach/kern_return.h`, `osfmk/mach/thread_info.h`: 2168-byte `statfs`, extended flags at byte 2136, Darwin metadata layout, `KERN_NOT_SUPPORTED = 46`. |
| [Libc](https://github.com/apple-oss-distributions/Libc) | `71bbe350ab79eef58113991d817ccc6165061a64` | `stdlib/FreeBSD/_Exit_.c`, `stdlib/FreeBSD/quick_exit.c`, `tests/quick_exit.c`: immediate exit versus reverse-order quick-exit callbacks. |
| [libpthread](https://github.com/apple-oss-distributions/libpthread) | `42d026df5b07825070f60134b980a1ec2552dfee` | `include/pthread/pthread.h`, `src/pthread.c`, `src/pthread_mutex.c`: `pthread_cpu_number_np(size_t*)` writes eight bytes on ARM64; mutex fairness policy values 1 and 3 are recognized but unsupported here. |
| [libplatform](https://github.com/apple-oss-distributions/libplatform) | `2512ffd8bb5c6caff3c8ea83331ab8a0adc820c3` | `src/os/atomic.c`, `include/libkern/OSAtomicDeprecated.h`: non-barrier atomic operations, new-value returns for `OSAtomicAdd64`, boolean CAS. |
| [libdispatch](https://github.com/apple-oss-distributions/libdispatch) | `2361ffb78a76f7ee488cd052eb0bc5c767118bf9` | `src/object.c`, `src/semaphore.c`, `src/time.c`: object context/finalizer, group notifications, negative wall-clock encoding, deadline arithmetic and overflow. Tests caught wall-clock deadlines being treated as monotonic time. |
| [objc4](https://github.com/apple-oss-distributions/objc4) | `fb265098298302243cd7eeaa1f63f0ba7786dd9a` | `runtime/objc-accessors.mm`, `runtime/Messengers.subproj/objc-msg-arm64.s`, `runtime/objc-runtime-new.mm`: property lifetime, superclass lookup, message argument preservation, image/class registration. |
| [dyld](https://github.com/apple-oss-distributions/dyld) | `fd8d0c4d52320ebf64db34f3cb280310d905c5ae` | `dyld/DyldRuntimeState.cpp`: notify the Objective-C runtime about mapped images before initialization. A regression checks name lookup before any direct guest class use. |
| [CF](https://github.com/apple-oss-distributions/CF) | `dc54c6bb1c1e5e0b9486c1d26dd5bef110b20bf3` | `CFString.c`, `CFICULogging.h`, `CFDateFormatter.c`, `CFLocale.c`, `CFICUConverters.c`: UTF-16 character counts, explicit byte conversion, and Apple's use of the public ICU C interfaces. |
| Local LLVM | `248d1a44e31805bcffdd38234bcddcb211a25a8b` | `clang/lib/CodeGen/TargetInfo.cpp` (`AArch64ABIInfo`), `llvm/lib/Target/AArch64/AArch64CallingConvention.td`: aggregate results over 16 bytes use indirect storage in `x8`; Darwin anonymous arguments use stack slots; scalar/pointer ICU register assignments. |

The installed ICU 74.2 public headers and library exports are an additional
reference. The provider checks all 174 prototypes at compile time and reports
the installed ICU version. It does not pretend to use the archive's 76.1 data.

CPU-type discovery was also checked against XNU `bsd/kern/kern_mib.c`
(`hw.cputype` is a read-only integer), `bsd/sys/sysctl.h`, and
`osfmk/mach/machine.h`. The latter and LLVM's
`llvm/include/llvm/BinaryFormat/MachO.h` agree that `CPU_TYPE_ARM64` is
`12 | 0x01000000`. The shim exposes the named query without inventing a fixed
numeric MIB for XNU's dynamically assigned OID.

The named ARM feature queries also follow `bsd/kern/kern_mib.c` and
`osfmk/arm/arm_features.inc`: results are read-only four-byte integers, including
when the caller supplies an eight-byte buffer. Eighteen features and eight
documented legacy aliases are translated from Linux `AT_HWCAP`, including
FP16 (both scalar and SIMD half arithmetic), DotProd and FHM. LLVM's
`llvm/include/llvm/Support/AArch64TargetParser.def` distinguishes full FP16
from FP16 multiply-add. No CPU family or unsupported feature is fabricated.
The host ABI regression verifies capability values, aliases, buffer widths,
short buffers, unknown names and writes. This implements named queries, not
XNU's complete feature/MIB namespace.

Port-zero connection validation follows XNU `bsd/netinet/in_pcb.c`
(`in_pcbladdr`, non-raw sockets), `bsd/netinet6/in6_pcb.c` (`in6_pcbladdr`),
and connected-stream error precedence in `bsd/kern/uipc_socket.c`.
Imported `connect`, direct `connect`, `connect_nocancel` and the existing
`connectx` adapter share the backend. The Darwin fixture checks IPv4/IPv6
TCP/UDP, invalid descriptors, and an established stream. Its `sa_endpoints`
layout comes from `bsd/sys/socket.h`. The syscall 0–99 dispatcher uses the
existing complete errno translator rather than passing unfamiliar Linux
errno values through. This does not complete the socket state-machine audit.

The C++ exception regression was checked against the same LLVM checkout's
`libcxxabi/src/private_typeinfo.h` and `private_typeinfo.cpp`, and the installed
GNU `cxxabi.h`. LLVM's `__shim_type_info::can_catch` and GNU's RTTI virtual
methods do not share a vtable contract. A debugger trace showed an LLVM
`can_catch` call entering GNU `__si_class_type_info::__do_dyncast`. ICU now loads
its GNU dependencies in a separate ELF namespace, and a Mach-O exception test
exercises the boundary with both dependency orders and cached launches.

The ABI fixtures are compiled for ARM64 Darwin and executed by MachGate on
ARM64 Linux. They check floating arguments/results, 24-byte structure returns,
spilled arguments, Unicode conversion, and reserved `x18` preservation at the
new ICU, Objective-C message, filesystem-status, and quick-exit boundaries.
Cache-disabled, cold, and warm provider runs cover the restored class registry
and process-local binding work.

See [archive parity and limits](ARCHIVE_SHIM_PARITY.md) for the resulting
implementation boundaries. Source review does not establish equivalence to the
complete macOS runtime.

The dependency-loading work also reviewed dyld `dyld/Loader.cpp` path expansion,
`mach_o/RebaseOpcodes.cpp` validation, and LLVM's `MachO.h` rebase constants.
The loader keeps `@loader_path` relative to the importing image and
`@executable_path` relative to the main image. Small Mach-O fixtures cover both
classic and chained fixups, dependency initializers, and guest syscalls.

For TLS, dyld `libdyld/ThreadLocalVariables.cpp::initializeThunksFromDisk`
assigns one pthread key to all descriptors in an image, and
`findInitialContent`/`forEachThunkSpan` handle template and descriptor sections.
The new provider port preserves per-image identity, shared descriptor storage,
initialized bytes, zero fill, section alignment, and per-thread isolation.
This does not establish the complete dyld shared-cache TLV format or runtime
unload/initializer contract.

External discovery subsequently exposed an EOF contract missed by the initial
fixture: dyld `mach_o/RebaseOpcodes.cpp::forEachRebase` accepts a complete
opcode stream ending at its declared length without `REBASE_OPCODE_DONE`.
The parser regression now tests this form and still rejects truncated ULEB128
operands. Four real executable discovery probes reproduce the failure before
and succeed after the correction in the isolated build.

The subsequent protocol crash review checked objc4
`runtime/Messengers.subproj/objc-msg-arm64.s:683` and
`runtime/NSObject.mm::_objc_rootInit`: super lookup starts at the superclass,
and the root initializer returns the receiver. Restarting receiver lookup
caused recursive initializers and guest stack exhaustion.

Blocks ownership was compared with the archive implementation and the pinned
LLVM `compiler-rt/lib/BlocksRuntime/{runtime.c,Block_private.h}`. The restored
shim copies stack blocks by descriptor size, invokes compiler lifetime
helpers, forwards byref storage, and uses saturating reference counts. Nested
block capture tests also catch the archive's incorrect field-mask comparison.
Provider object ownership is supplied through a port; the core has no
Foundation dependency. These tests do not establish garbage collection,
complete Objective-C weak-block semantics, or all newer block layout variants.

The networking review additionally checked XNU `bsd/net/if.h`,
`bsd/net/if_dl.h`, `bsd/net/if_types.h`, `bsd/sys/socket.h`,
`bsd/netinet/in.h`, `bsd/netinet/tcp.h`, and `bsd/netinet6/in6.h`.
Interface enumeration now produces Darwin IPv4/IPv6/link-layer addresses,
translates interface flags, and owns independent guest lists. The local source
set does not contain Apple's `getifaddrs` implementation; this is ABI evidence,
not an assertion that its complete implementation was compared.

Socket-option numbers now use an explicit supported mapping. In particular,
XNU defines `IPV6_UNICAST_HOPS` as 4, RFC 3542 `IPV6_PKTINFO` as 46,
and legacy RFC 2292 packet info as 19. The old tests incorrectly repeated
the implementation's hop-limit value 16. Native option translation, raw Darwin
dispatch, and actual Darwin option round trips now check the corrected values.
Ancillary payloads, linger units, resolver flags, and extended options still
require the further work recorded in the networking audit.

The large-image thread-pointer rewrite uses AArch64 `MOVZ` with a 16-bit
immediate shifted by 16, checked against the local LLVM
`llvm/lib/Target/AArch64/AArch64InstrInfo.td`. An executed ARM64 regression
checks all destination registers, NZCV, return-address preservation, and
existing-mapping preservation with no free branch-island space. This concerns
the supported single guest thread and does not add general threading support.

The pinned XNU checkout also includes `iokit/DriverKit` interfaces and
`iokit/Kernel/IOUserServer.cpp`. These can inform a future device-contract
investigation; they are not substitutes for BSD sockets or Mach VM contracts,
and their presence does not establish a complete DriverKit framework port.

Dynamic unwind registration was checked against the pinned LLVM checkout's
`libunwind/src/UnwindLevel1-gcc-ext.c` and `libunwind/src/libunwind.cpp`.
Darwin `__register_frame` / `__deregister_frame` delegate to single-FDE
registration/removal. GCC's same-named registration function consumes a
terminated section, so forwarding the Darwin pointer directly is insufficient.
The adapter instead indexes each decoded FDE independently for the host
unwinder. Tests exercise native stack unwinding through generated ARM64 code,
independent removal, absolute and PC-relative FDEs, read-only metadata, and an
FDE immediately before an inaccessible page with no following terminator.
As with native registration, callers must provide valid readable CIE/FDE
storage and keep it alive while registered and in use by an unwinder.
This implements the per-FDE APIs, not the entire libunwind SPI or authenticated
ARM64e return-address contract.


The iOS crash/timeout repair batch also checked these contracts:

- objc4 `runtime/NSObject.mm` (`objc_alloc_init`) and LLVM
  `clang/lib/CodeGen/CGObjC.cpp`: combined allocation dispatches `init` and
  returns its result. The separate Foundation provider implements this entry.
- objc4 `runtime/objc-exception.mm`: `__objc_personality_v0` delegates to
  the C++ personality; catch/end/rethrow/terminate delegate to the same C++
  runtime. The Objective-C typeinfo has three eight-byte fields and the
  vtable slots match LLVM `libcxxabi/src/private_typeinfo.h`. The regression
  sends a C++ exception through an Objective-C typed catch and `@finally`,
  checks cleanup exactly once, and catches it through a C++ base class. Both
  provider load orders and disabled/cold/warm caches pass. This does not add
  Objective-C exception throwing, alternative exception handlers, or a full
  NSException class; its unavailable native class has no catch match.
- XNU `bsd/kern/sys_ulock.c` (`ulock_wait_cleanup`) returns the remaining
  waiter count. libplatform `src/os/lock.c` uses a positive return to keep
  the next unfair-lock wake enabled. The Linux adapter now tracks private
  waiters by address, removes them on every return, and discards inherited
  records in a forked child. The tests cover the positive return, subsequent
  zero return, mismatch while another waiter sleeps, and fork isolation.
  Shared waits still lack a count spanning multiple host processes; their
  existing futex wake behavior is unchanged. This is not a claim of complete
  ulock priority inheritance or shared-wait parity.

The whitespace and task-memory repair checked:

- Apple Libc `locale/FreeBSD/table.c:63-67`: tab has CONTROL, SPACE and BLANK;
  newline, vertical tab, form feed and carriage return have CONTROL and SPACE.
  Classification accumulates these flags instead of returning at CONTROL.
  The local LLVM `libcxx/src/locale.cpp::ctype<char>::classic_table` uses
  `_DefaultRuneLocale.__runetype` under the Darwin ABI compatibility build.
  Regression coverage checks all 128 ASCII entries through both the exported
  table and `__maskrune`.
- XNU `osfmk/mach/task_info.h::task_vm_info` and
  `osfmk/kern/task.c::task_info`: TASK_VM_INFO is flavor 22. Revision 0 is
  144 bytes; revision 1 adds the eight-byte `phys_footprint`. Counts are in
  32-bit words, a short revision-0 buffer returns KERN_INVALID_ARGUMENT, and
  output counts advance only at supported revision boundaries. MachGate
  implements revisions 0/1 and returns 36/38 words, leaving later fields
  untouched. Its Linux RSS-based footprint and internal-memory estimates
  follow archive behavior; they are not XNU ledger accounting. Device,
  purgeable, compressed, external-memory and region counters remain zero.
  Later revisions and detailed ledger statistics remain unsupported. Tests
  check sizes, boundaries, sentinel preservation, invalid requests and a
  footprint increase after faulting in an allocation.
