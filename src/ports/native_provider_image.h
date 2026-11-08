#ifndef MACHGATE_NATIVE_PROVIDER_IMAGE_H
#define MACHGATE_NATIVE_PROVIDER_IMAGE_H

#include <stddef.h>

#define MACHGATE_SECTION_LOADED_SYMBOL "machgate_guest_section_loaded_v1"

typedef void (*machgate_section_loaded_function)(const char segment[16],
	const char section[16], const void* address, size_t size);

#endif
