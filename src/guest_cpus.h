#ifndef MACHGATE_GUEST_CPUS_H
#define MACHGATE_GUEST_CPUS_H

/*
 * The CPU count reported to the guest. Guests size their internal thread
 * pools from this value, so a 64-core host makes every guest daemon spawn
 * 64-thread pools; N resident daemons then oversubscribe the host badly.
 * MACHGATE_GUEST_NCPU caps the reported count so guest pools stay small.
 */

int guest_cpu_count(void);

#endif
