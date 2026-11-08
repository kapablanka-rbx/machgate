#ifndef MACHGATE_PORTS_PROVIDER_LIFETIME_H
#define MACHGATE_PORTS_PROVIDER_LIFETIME_H

typedef void (*machgate_release_function)(void* object);

int machgate_defer_release_v1(void* object, machgate_release_function release);

#endif
