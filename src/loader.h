#ifndef MACHGATE_LOADER_H
#define MACHGATE_LOADER_H

#include "domain/guest_image.h"

#include <stddef.h>

struct machgate_startup_cache;
struct machgate_startup_cache* machgate_guest_image_startup_cache(
	const struct machgate_guest_image* image);

int machgate_load_executable(const char* path,
			     struct machgate_guest_image** image,
			     char* error, size_t error_size);
int machgate_load_guest_library(const char* path,
	struct machgate_guest_image** image, char* error, size_t error_size);
void machgate_unload_guest_image(struct machgate_guest_image* image);
void* machgate_pool_alloc(struct machgate_guest_image* image, size_t size);

#endif
