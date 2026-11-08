#ifndef MACHGATE_INTERFACE_ADDRESSES_COMPAT_H
#define MACHGATE_INTERFACE_ADDRESSES_COMPAT_H

#include <stdint.h>

struct machgate_darwin_ifaddrs {
	struct machgate_darwin_ifaddrs* next;
	char* name;
	uint32_t flags;
	void* address;
	void* netmask;
	void* destination;
	void* data;
};

__attribute__((visibility("hidden")))
int machgate_darwin_getifaddrs(struct machgate_darwin_ifaddrs** addresses);
__attribute__((visibility("hidden")))
void machgate_darwin_freeifaddrs(struct machgate_darwin_ifaddrs* addresses);

#endif
