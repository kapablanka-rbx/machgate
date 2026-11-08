#ifndef MACHGATE_ADAPTERS_DYNAMIC_UNWIND_H
#define MACHGATE_ADAPTERS_DYNAMIC_UNWIND_H

#include <dlfcn.h>

int machgate_find_dynamic_unwind(void* address, struct dl_find_object* result);

#endif
