#ifndef MACHGATE_DARWIN_ULOCK_H
#define MACHGATE_DARWIN_ULOCK_H

#include <stdint.h>

#define MACHGATE_ULF_NO_ERRNO 0x01000000u

int machgate_darwin_ulock_wait(uint32_t operation, void* address, uint64_t value,
                              uint64_t timeout_ns);
int machgate_darwin_ulock_wake(uint32_t operation, void* address, uint64_t wake_value);

#endif
