# Archive compatibility provenance

The polling FSEvents implementation in `fsevents.c` was extracted and adapted
from `machgate-archive/src/shim/libsystem_shim.c`, source SHA-256
`c369570e2efe4bdadc10319a9e743a92b316d5bf7239717b30e50c4fda343495`.
The archive's GNU General Public License version 3 text is preserved in
`COPYING`. Existing project legal notices are unchanged.

This provider exposes the archive surface through explicit library composition.
It does not implement a graphical environment, Mach IPC, or full Apple service
behavior. Supported behavior and unavailable services are documented in
`../../docs/ARCHIVE_SHIM_PARITY.md`.
