#include "shim/interface_addresses_compat.h"

#include <assert.h>
#include <errno.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <netpacket/packet.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static struct ifaddrs host_interfaces[5];
static int host_error;
static int host_frees;
static int fail_allocation;
static int allocations;

int __wrap_getifaddrs(struct ifaddrs** result)
{
	if (host_error) {
		errno = host_error;
		return -1;
	}
	*result = host_interfaces;
	return 0;
}

void __wrap_freeifaddrs(struct ifaddrs* addresses)
{
	assert(addresses == host_interfaces);
	host_frees++;
}

void* __real_calloc(size_t count, size_t size);

void* __wrap_calloc(size_t count, size_t size)
{
	if (++allocations == fail_allocation) {
		errno = ENOMEM;
		return NULL;
	}
	return __real_calloc(count, size);
}

int main(void)
{
	struct sockaddr_ll link = {
		.sll_family = AF_PACKET, .sll_ifindex = 7, .sll_hatype = ARPHRD_ETHER,
		.sll_halen = 6, .sll_addr = {0x02, 1, 2, 3, 4, 5},
	};
	struct sockaddr_in ipv4 = {.sin_family = AF_INET};
	struct sockaddr_in mask4 = {.sin_family = AF_INET};
	struct sockaddr_in broadcast = {.sin_family = AF_INET};
	const unsigned char ip4_bytes[] = {192, 0, 2, 7};
	const unsigned char mask4_bytes[] = {255, 255, 255, 0};
	const unsigned char broadcast_bytes[] = {192, 0, 2, 255};
	memcpy(&ipv4.sin_addr, ip4_bytes, 4);
	memcpy(&mask4.sin_addr, mask4_bytes, 4);
	memcpy(&broadcast.sin_addr, broadcast_bytes, 4);
	struct sockaddr_in6 ipv6 = {.sin6_family = AF_INET6, .sin6_scope_id = 7};
	ipv6.sin6_addr.s6_addr[0] = 0xfe;
	ipv6.sin6_addr.s6_addr[1] = 0x80;
	ipv6.sin6_addr.s6_addr[15] = 1;
	struct sockaddr_in6 mask6 = {.sin6_family = AF_INET6};
	memset(mask6.sin6_addr.s6_addr, 0xff, 8);
	struct sockaddr unsupported = {.sa_family = AF_UNIX};
	for (size_t i = 0; i < 5; ++i) {
		host_interfaces[i].ifa_name = "test0";
		host_interfaces[i].ifa_flags = IFF_UP | IFF_RUNNING | IFF_MULTICAST;
		if (i < 4)
			host_interfaces[i].ifa_next = &host_interfaces[i + 1];
	}
	host_interfaces[0].ifa_addr = (struct sockaddr*)&link;
	host_interfaces[0].ifa_flags |= IFF_MASTER | IFF_SLAVE;
	host_interfaces[0].ifa_data = &link;
	host_interfaces[1].ifa_addr = (struct sockaddr*)&ipv4;
	host_interfaces[1].ifa_netmask = (struct sockaddr*)&mask4;
	host_interfaces[1].ifa_broadaddr = (struct sockaddr*)&broadcast;
	host_interfaces[1].ifa_flags |= IFF_BROADCAST;
	host_interfaces[2].ifa_addr = (struct sockaddr*)&ipv6;
	host_interfaces[2].ifa_netmask = (struct sockaddr*)&mask6;
	host_interfaces[2].ifa_dstaddr = (struct sockaddr*)&ipv6;
	host_interfaces[2].ifa_flags |= IFF_POINTOPOINT;
	host_interfaces[3].ifa_addr = &unsupported;

	struct machgate_darwin_ifaddrs* addresses;
	assert(machgate_darwin_getifaddrs(&addresses) == 0);
	assert(host_frees == 1);
	assert(strcmp(addresses->name, "test0") == 0);
	assert(addresses->name != host_interfaces[0].ifa_name);
	assert(addresses->flags == 0x8041);
	assert(addresses->data == NULL);
	const unsigned char* bytes = addresses->address;
	assert(bytes[0] == 20 && bytes[1] == 18 && bytes[2] == 7 && bytes[3] == 0);
	assert(bytes[4] == 6 && bytes[5] == 5 && bytes[6] == 6 && bytes[7] == 0);
	assert(memcmp(bytes + 8, "test0", 5) == 0);
	assert(memcmp(bytes + 13, link.sll_addr, 6) == 0);

	struct machgate_darwin_ifaddrs* current = addresses->next;
	assert(current->flags == 0x8043);
	bytes = current->address;
	assert(bytes[0] == 16 && bytes[1] == 2);
	assert(memcmp(bytes + 4, ip4_bytes, 4) == 0);
	assert(memcmp((unsigned char*)current->netmask + 4, mask4_bytes, 4) == 0);
	assert(memcmp((unsigned char*)current->destination + 4, broadcast_bytes, 4) == 0);
	current = current->next;
	assert(current->flags == 0x8051);
	bytes = current->address;
	assert(bytes[0] == 28 && bytes[1] == 30);
	assert(memcmp(bytes + 8, ipv6.sin6_addr.s6_addr, 16) == 0);
	uint32_t scope;
	memcpy(&scope, bytes + 24, sizeof(scope));
	assert(scope == 7);
	assert(memcmp((unsigned char*)current->netmask + 8, mask6.sin6_addr.s6_addr, 16) == 0);
	assert(memcmp(current->destination, current->address, 28) == 0);
	current = current->next;
	assert(current && !current->address && !current->next);

	struct machgate_darwin_ifaddrs* independent;
	assert(machgate_darwin_getifaddrs(&independent) == 0);
	machgate_darwin_freeifaddrs(addresses);
	assert(strcmp(independent->name, "test0") == 0);
	machgate_darwin_freeifaddrs(independent);
	machgate_darwin_freeifaddrs(NULL);

	for (int i = 1; i <= 4; ++i) {
		fail_allocation = i;
		allocations = 0;
		addresses = (void*)1;
		assert(machgate_darwin_getifaddrs(&addresses) == -1);
		assert(errno == 12 && !addresses);
	}
	assert(host_frees == 6);
	fail_allocation = 0;
	host_error = EAFNOSUPPORT;
	addresses = (void*)1;
	assert(machgate_darwin_getifaddrs(&addresses) == -1);
	assert(errno == 47 && !addresses && host_frees == 6);
	assert(machgate_darwin_getifaddrs(NULL) == -1 && errno == 14);
	return 0;
}
