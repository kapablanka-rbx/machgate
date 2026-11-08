#ifndef MACHGATE_PTHREAD_ATTR_COMPAT_H
#define MACHGATE_PTHREAD_ATTR_COMPAT_H

#include <pthread.h>

int machgate_native_pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                                   void* (*start_routine)(void*), void* arg);
__attribute__((visibility("hidden")))
int machgate_darwin_pthread_attr_setstacksize(pthread_attr_t* attr, size_t stack_size);
__attribute__((visibility("hidden")))
int machgate_darwin_pthread_attr_getstacksize(const pthread_attr_t* attr, size_t* stack_size);
__attribute__((visibility("hidden")))
int machgate_darwin_pthread_attr_getschedpolicy(const pthread_attr_t* attr, int* policy);

#endif
