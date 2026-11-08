#ifndef MACHGATE_PORTS_GUEST_IMAGE_VIEW_H
#define MACHGATE_PORTS_GUEST_IMAGE_VIEW_H

#include "domain/guest_image.h"

#include <stdint.h>

void* machgate_guest_image_validated_header(
	const struct machgate_guest_image* image);
const char* machgate_guest_image_path(const struct machgate_guest_image* image);
intptr_t machgate_guest_image_slide(const struct machgate_guest_image* image);
uintptr_t machgate_guest_image_mapping_start(
	const struct machgate_guest_image* image);
uintptr_t machgate_guest_image_mapping_end(
	const struct machgate_guest_image* image);
uintptr_t machgate_guest_image_entry_point(
	const struct machgate_guest_image* image);
int machgate_guest_image_platform(const struct machgate_guest_image* image);
int machgate_guest_image_contains(const struct machgate_guest_image* image,
				 uintptr_t address, uint64_t size);

#endif
