#ifndef MACHGATE_THREAD_LOCAL_COMPAT_H
#define MACHGATE_THREAD_LOCAL_COMPAT_H

#include <stdint.h>

__attribute__((visibility("hidden")))
int machgate_darwin_pthread_key_delete(uint64_t key);
int machgate_darwin_pthread_key_create(uint64_t* key, void (*destructor)(void*));
__attribute__((visibility("hidden")))
int machgate_darwin_pthread_setspecific(uint64_t key, const void* value);
__attribute__((visibility("hidden")))
void* machgate_darwin_pthread_getspecific(uint64_t key);

void machgate_run_tlv_exit_callbacks(void);
void machgate_run_darwin_tsd_destructors(void);
void machgate_run_thread_local_termination_callbacks(void);

#endif
