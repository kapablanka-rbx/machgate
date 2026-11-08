#ifndef MACHGATE_PORTS_GUEST_SYMBOL_LOOKUP_H
#define MACHGATE_PORTS_GUEST_SYMBOL_LOOKUP_H

#include <stdint.h>

struct machgate_image_description {
	const char* path;
	void* header;
	intptr_t slide;
};

struct machgate_address_description {
	struct machgate_image_description image;
	const char* symbol_name;
	void* symbol_address;
};

int machgate_describe_guest_image_v1(uint32_t index, struct machgate_image_description* description);
int machgate_describe_guest_address_v1(const void* address, struct machgate_address_description* description);
void machgate_release_guest_symbol_lookup(void);

#endif
