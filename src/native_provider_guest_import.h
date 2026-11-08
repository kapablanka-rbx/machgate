#ifndef MACHGATE_NATIVE_PROVIDER_GUEST_IMPORT_H
#define MACHGATE_NATIVE_PROVIDER_GUEST_IMPORT_H

#define MACHGATE_GUEST_IMPORT_RESOLVER_SYMBOL \
	"machgate_resolve_guest_import_v1"
#define MACHGATE_NATIVE_SYMBOL_RESOLVER_SYMBOL \
	"machgate_resolve_native_symbol_v1"

typedef int (*machgate_guest_import_resolver_function)(const char* name,
						       void** address);
typedef int (*machgate_native_symbol_resolver_function)(void* handle,
						const char* name, void** address);

int machgate_resolve_guest_import_v1(const char* name, void** address);
int machgate_resolve_native_symbol_v1(void* handle, const char* name,
				     void** address);

#endif
