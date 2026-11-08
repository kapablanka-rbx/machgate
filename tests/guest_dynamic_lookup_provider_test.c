#define _GNU_SOURCE
#include "native_provider_guest_import.h"

#include <dlfcn.h>
#include <stdio.h>

int main(int argc, char** argv)
{
	if (argc != 2)
		return 1;
	void* provider = dlopen(argv[1], RTLD_NOW | RTLD_GLOBAL);
	if (!provider)
		return 2;
	machgate_guest_import_resolver_function resolver =
		(machgate_guest_import_resolver_function)dlsym(
			provider, MACHGATE_GUEST_IMPORT_RESOLVER_SYMBOL);
	if (!resolver)
		return 3;
	const char* names[] = {"dlsym", "dlopen"};
	const char* implementations[] = {
		"machgate_darwin_dlsym_impl", "machgate_darwin_dlopen_impl",
	};
	for (unsigned int index = 0; index < 2; index++) {
		void* address = NULL;
		void* expected = dlsym(provider, implementations[index]);
		if (!expected || !resolver(names[index], &address) ||
		    address != expected) {
			fprintf(stderr, "guest resolver returned host %s\n", names[index]);
			return 4;
		}
	}
	return 0;
}
