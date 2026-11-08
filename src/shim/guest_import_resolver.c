#define _GNU_SOURCE

#include "native_provider_guest_import.h"
#include "interface_addresses_compat.h"
#include "resource_limit_compat.h"
#include "thread_local_compat.h"
#include "variadic_abi_compat.h"
#include "darwin_error_messages_compat.h"
#include "pthread_attr_compat.h"
#include "extended_attributes_compat.h"
#include "guest_environment_compat.h"
#include "image_symbols_compat.h"

#include <string.h>

__attribute__((visibility("hidden")))
void* machgate_guest_dlsym_address(void);
__attribute__((visibility("hidden")))
void* machgate_guest_dlopen_address(void);

int machgate_resolve_guest_import_v1(const char* name, void** address)
{
	if (!name || !address)
		return 0;
	if (strcmp(name, "dladdr") == 0) {
		*address = machgate_darwin_dladdr;
		return 1;
	}
	if (strcmp(name, "backtrace_symbols") == 0) {
		*address = machgate_darwin_backtrace_symbols;
		return 1;
	}
	if (strcmp(name, "backtrace_symbols_fd") == 0) {
		*address = machgate_darwin_backtrace_symbols_fd;
		return 1;
	}
	if (strcmp(name, "setenv") == 0) {
		*address = machgate_darwin_setenv;
		return 1;
	}
	if (strcmp(name, "unsetenv") == 0) {
		*address = machgate_darwin_unsetenv;
		return 1;
	}
	if (strcmp(name, "putenv") == 0) {
		*address = machgate_darwin_putenv;
		return 1;
	}

	static const struct {
		const char* name;
		void* address;
	} extended_attributes[] = {
		{"getxattr", machgate_darwin_getxattr},
		{"fgetxattr", machgate_darwin_fgetxattr},
		{"setxattr", machgate_darwin_setxattr},
		{"fsetxattr", machgate_darwin_fsetxattr},
		{"removexattr", machgate_darwin_removexattr},
		{"fremovexattr", machgate_darwin_fremovexattr},
		{"listxattr", machgate_darwin_listxattr},
		{"flistxattr", machgate_darwin_flistxattr},
	};
	for (size_t index = 0; index < sizeof(extended_attributes) / sizeof(extended_attributes[0]); index++) {
		if (strcmp(name, extended_attributes[index].name) == 0) {
			*address = extended_attributes[index].address;
			return 1;
		}
	}

	if (strcmp(name, "pthread_mutexattr_settype") == 0) {
		*address = machgate_darwin_pthread_mutexattr_settype;
		return 1;
	}
	if (strcmp(name, "pthread_mutexattr_gettype") == 0) {
		*address = machgate_darwin_pthread_mutexattr_gettype;
		return 1;
	}
	if (strcmp(name, "pthread_attr_getschedpolicy") == 0) {
		*address = machgate_darwin_pthread_attr_getschedpolicy;
		return 1;
	}
	if (strcmp(name, "pthread_attr_setstacksize") == 0) {
		*address = machgate_darwin_pthread_attr_setstacksize;
		return 1;
	}
	if (strcmp(name, "pthread_attr_getstacksize") == 0) {
		*address = machgate_darwin_pthread_attr_getstacksize;
		return 1;
	}
	if (strcmp(name, "strerror") == 0) {
		*address = machgate_darwin_strerror;
		return 1;
	}
	if (strcmp(name, "strerror_r") == 0) {
		*address = machgate_darwin_strerror_r;
		return 1;
	}
	if (strcmp(name, "__snprintf_chk") == 0) {
		*address = machgate_darwin_snprintf_chk;
		return 1;
	}
	if (strcmp(name, "pthread_key_delete") == 0) {
		*address = machgate_darwin_pthread_key_delete;
		return 1;
	}
	if (strcmp(name, "pthread_key_create") == 0) {
		*address = machgate_darwin_pthread_key_create;
		return 1;
	}
	if (strcmp(name, "pthread_setspecific") == 0) {
		*address = machgate_darwin_pthread_setspecific;
		return 1;
	}
	if (strcmp(name, "pthread_getspecific") == 0) {
		*address = machgate_darwin_pthread_getspecific;
		return 1;
	}
	if (strcmp(name, "dlopen") == 0) {
		*address = machgate_guest_dlopen_address();
		return 1;
	}
	if (strcmp(name, "dlsym") == 0) {
		*address = machgate_guest_dlsym_address();
		return 1;
	}
	if (strcmp(name, "getrlimit") == 0) {
		*address = machgate_darwin_getrlimit;
		return 1;
	}
	if (strcmp(name, "setrlimit") == 0) {
		*address = machgate_darwin_setrlimit;
		return 1;
	}
	if (strcmp(name, "getifaddrs") == 0) {
		*address = machgate_darwin_getifaddrs;
		return 1;
	}
	if (strcmp(name, "freeifaddrs") == 0) {
		*address = machgate_darwin_freeifaddrs;
		return 1;
	}
	return 0;
}
