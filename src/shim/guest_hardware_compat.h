#ifndef MACHGATE_GUEST_HARDWARE_COMPAT_H
#define MACHGATE_GUEST_HARDWARE_COMPAT_H

__attribute__((visibility("hidden")))
int machgate_guest_cpu_count(void);

__attribute__((visibility("hidden")))
int machgate_guest_arm_feature(const char* name, int* result);

#endif
