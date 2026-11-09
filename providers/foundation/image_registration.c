#include "archive/runtime.h"
#include "ports/native_provider_image.h"

#include <string.h>

void machgate_guest_section_loaded_v1(const char segment[16],
	const char section[16], const void* address, size_t size)
{
	(void)segment;
	if (size % sizeof(void*) != 0)
		return;
	void* const* entries = address;
	size_t count = size / sizeof(*entries);
	if (strncmp(section, "__objc_classlist", 16) == 0) {
		for (size_t index = 0; index < count; index++)
			machgate_shim_register_objc_class(entries[index]);
		return;
	}
	if (strncmp(section, "__objc_catlist", 16) == 0) {
		for (size_t index = 0; index < count; index++)
			machgate_shim_register_objc_category(entries[index]);
		return;
	}
}
