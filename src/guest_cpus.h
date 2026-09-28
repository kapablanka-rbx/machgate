#ifndef MACHGATE_GUEST_CPUS_H
#define MACHGATE_GUEST_CPUS_H

/*
 * Guest platform identity and hardware reporting.
 *
 * The guest's LC_BUILD_VERSION load command declares its platform
 * (macOS / iOS / iOS-simulator); the loader records it once, before guest
 * execution, and it never changes for the life of the process — one machgate
 * process, one guest, one mode. This is what lets a single host run iOS and
 * macOS guests side by side: each process derives its mode from its own
 * guest binary. Everything that should answer differently per platform —
 * reported CPU count, hw.machine, osversion — reads this one fact.
 */

void machgate_set_guest_platform(int macho_platform);
int machgate_guest_platform(void);
const char* machgate_guest_platform_name(void);
int guest_cpu_count(void);

#endif
