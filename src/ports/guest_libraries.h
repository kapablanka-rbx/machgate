#ifndef MACHGATE_PORTS_GUEST_LIBRARIES_H
#define MACHGATE_PORTS_GUEST_LIBRARIES_H

#include "domain/guest_image.h"
#include <stddef.h>
#include <stdint.h>

void machgate_begin_guest_linkage(struct machgate_guest_image* main_image);
int machgate_open_guest_dependency(struct machgate_guest_image* importer,
	const char* install_name, struct machgate_guest_image** library,
	char* error, size_t error_size);
struct machgate_guest_image* machgate_main_guest_image(void);
struct machgate_guest_image* machgate_guest_image_at_index(size_t index);
int machgate_prepare_guest_library_images(char* error, size_t error_size);
int machgate_initialize_guest_libraries(int argument_count, char** arguments,
	char** environment, char** apple, uintptr_t stack_pointer,
	char* error, size_t error_size);
void machgate_release_guest_libraries(void);

#endif
