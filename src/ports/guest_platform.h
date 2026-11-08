#ifndef MACHGATE_PORTS_GUEST_PLATFORM_H
#define MACHGATE_PORTS_GUEST_PLATFORM_H

#include "domain/guest_image.h"

#include <stddef.h>

int machgate_prepare_guest_platform(const struct machgate_guest_image* image,
				   char* error, size_t error_size);
void machgate_release_guest_platform(void);

#endif
