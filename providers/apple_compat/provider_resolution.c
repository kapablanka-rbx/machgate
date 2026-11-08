#define _GNU_SOURCE
#include "native_provider_guest_import.h"
#include "ports/native_provider_image.h"

#include <dlfcn.h>
#include <link.h>
#include <stdint.h>

static void* providers[3];
static machgate_guest_import_resolver_function shim_resolver;
static machgate_section_loaded_function foundation_section_loaded;

static int owns_address(void* handle, void* address)
{
	struct link_map* provider = NULL;
	Dl_info info;
	return address && dlinfo(handle, RTLD_DI_LINKMAP, &provider) == 0 &&
		provider && dladdr(address, &info) != 0 &&
		info.dli_fbase == (void*)(uintptr_t)provider->l_addr;
}

__attribute__((constructor))
static void connect_providers(void)
{
	const char* names[] = {"libmachgate_foundation.so", "libmachgate_icu.so", "libsystem_shim.so"};
	for (size_t index = 0; index < 3; index++)
		providers[index] = dlopen(names[index], RTLD_NOW | RTLD_NOLOAD);
	if (providers[2])
		shim_resolver = (machgate_guest_import_resolver_function)dlsym(providers[2],
			MACHGATE_GUEST_IMPORT_RESOLVER_SYMBOL);
	if (providers[0])
		foundation_section_loaded = (machgate_section_loaded_function)dlsym(providers[0],
			MACHGATE_SECTION_LOADED_SYMBOL);
}

int machgate_resolve_guest_import_v1(const char* name, void** address)
{
	if (!name || !address)
		return 0;
	*address = NULL;
	if (shim_resolver && shim_resolver(name, address))
		return 1;
	for (size_t index = 0; index < 3; index++) {
		if (!providers[index])
			continue;
		void* result = dlsym(providers[index], name);
		if (owns_address(providers[index], result)) {
			*address = result;
			return 1;
		}
	}
	return 0;
}

void machgate_guest_section_loaded_v1(const char segment[16],
	const char section[16], const void* address, size_t size)
{
	if (foundation_section_loaded)
		foundation_section_loaded(segment, section, address, size);
}
