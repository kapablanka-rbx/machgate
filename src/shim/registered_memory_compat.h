#ifndef MACHGATE_REGISTERED_MEMORY_COMPAT_H
#define MACHGATE_REGISTERED_MEMORY_COMPAT_H

__attribute__((visibility("hidden")))
int machgate_release_registered_mapping(void* address);

#endif
