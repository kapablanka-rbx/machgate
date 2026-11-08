#ifndef MACHGATE_ADAPTERS_GUEST_REBASE_H
#define MACHGATE_ADAPTERS_GUEST_REBASE_H

#include "domain/guest_image.h"
#include <stddef.h>

int machgate_rebase_guest_pointers(struct machgate_guest_image* image,
	char* error, size_t error_size);

#endif
