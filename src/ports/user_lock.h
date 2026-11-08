#ifndef MACHGATE_USER_LOCK_H
#define MACHGATE_USER_LOCK_H

#include <stdint.h>

int machgate_wait_on_word(void* address, uint64_t expected, unsigned width,
                         int shared, uint64_t timeout_ns, int absolute);
int machgate_wake_word(void* address, int shared, int wake_all);

#endif
