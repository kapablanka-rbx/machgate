# Archive Foundation provenance

Object and method behavior in objects.c was extracted and adapted from
machgate-archive/src/shim/libsystem_shim.c (source SHA-256: c369570e2efe4bdadc10319a9e743a92b316d5bf7239717b30e50c4fda343495).
The sibling archive's GNU General Public License version 3 text is preserved in
COPYING. Existing project legal notices remain unchanged.

The extracted implementation is kept within the optional Foundation provider.
The current provider retains ownership of NSString Unicode conversion, the
NSProcessInfo ABI, and the ARM64 message entry. Guest class lookup is adapted
into guest_classes.c with direct IMP lookup; original call frames are restored
by the owning provider instead of the archive's incomplete forwarding stub.

Archive behavior is not a claim of complete Foundation or Objective-C support.
No NSApplication, GUI, or game-specific integration is included.
