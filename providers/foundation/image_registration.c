#include "archive/runtime.h"
#include "ports/native_provider_image.h"

#include <string.h>

void machgate_guest_section_loaded_v1(const char segment[16],
	const char section[16], const void* address, size_t size)
{
	(void)segment;
	if (strncmp(section, "__objc_classlist", 16) != 0 ||
	    size % sizeof(void*) != 0)
		return;
	void* const* classes = address;
	for (size_t index = 0; index < size / sizeof(*classes); index++)
		machgate_shim_register_objc_class(classes[index]);
}
