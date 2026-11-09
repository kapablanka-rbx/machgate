#include "interface_addresses_compat.h"
#include "syscall/darwin_socket.h"

#include <errno.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <netpacket/packet.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DARWIN_AF_LINK 18
#define DARWIN_IFF_MULTICAST 0x8000

struct darwin_if_data {
	unsigned char ifi_type;
	unsigned char ifi_typelen;
	unsigned char ifi_physical;
	unsigned char ifi_addrlen;
	unsigned char ifi_hdrlen;
	unsigned char ifi_recvquota;
	unsigned char ifi_xmitquota;
	unsigned char ifi_unused1;
	uint32_t mtu;
	uint32_t metric;
	uint32_t baudrate;
	uint32_t input_packets;
	uint32_t input_errors;
	uint32_t output_packets;
	uint32_t output_errors;
	uint32_t collisions;
	uint32_t input_bytes;
	uint32_t output_bytes;
	uint32_t input_multicast_packets;
	uint32_t output_multicast_packets;
	uint32_t input_drops;
	uint32_t unsupported_protocol;
	uint32_t receive_timing;
	uint32_t transmit_timing;
	uint8_t last_change[16];
	uint32_t unused;
	uint32_t hardware_assist;
	uint32_t reserved[2];
};

_Static_assert(sizeof(struct darwin_if_data) == 104,
	"Darwin LP64 if_data must be 104 bytes");

struct interface_addresses {
	struct machgate_darwin_ifaddrs interface;
	unsigned char address[256];
	unsigned char netmask[256];
	unsigned char destination[256];
	struct darwin_if_data data;
	char name[];
};

_Static_assert(sizeof(struct machgate_darwin_ifaddrs) == 56,
	"Darwin LP64 ifaddrs must be 56 bytes");
_Static_assert(offsetof(struct machgate_darwin_ifaddrs, address) == 24,
	"Darwin LP64 ifaddrs address must be at offset 24");

static unsigned char interface_type(unsigned short hardware_type)
{
	switch (hardware_type) {
	case ARPHRD_ETHER: return 6;
	case ARPHRD_PPP: return 23;
	case ARPHRD_LOOPBACK: return 24;
	default: return 1;
	}
}

static uint32_t interface_flags(unsigned int host_flags)
{
	uint32_t result = 0;
	const unsigned int host_bits[] = {
		IFF_UP, IFF_BROADCAST, IFF_DEBUG, IFF_LOOPBACK, IFF_POINTOPOINT,
		IFF_NOTRAILERS, IFF_RUNNING, IFF_NOARP, IFF_PROMISC, IFF_ALLMULTI,
	};
	for (size_t i = 0; i < sizeof(host_bits) / sizeof(host_bits[0]); ++i) {
		if (host_flags & host_bits[i])
			result |= 1u << i;
	}
	if (host_flags & IFF_MULTICAST)
		result |= DARWIN_IFF_MULTICAST;
	return result;
}

static int translate_link_address(const struct sockaddr_ll* host_address,
				 const char* name, unsigned char* result)
{
	size_t name_length = strlen(name);
	size_t address_length = host_address->sll_halen;
	if (address_length > sizeof(host_address->sll_addr) ||
	    name_length > UINT8_MAX - 8 - address_length ||
	    host_address->sll_ifindex < 0 || host_address->sll_ifindex > UINT16_MAX) {
		errno = EOVERFLOW;
		return 0;
	}
	uint16_t index = (uint16_t)host_address->sll_ifindex;
	size_t length = 8 + name_length + address_length;
	result[0] = length < 20 ? 20 : length;
	result[1] = DARWIN_AF_LINK;
	memcpy(result + 2, &index, sizeof(index));
	result[4] = interface_type(host_address->sll_hatype);
	result[5] = name_length;
	result[6] = address_length;
	memcpy(result + 8, name, name_length);
	memcpy(result + 8 + name_length, host_address->sll_addr, address_length);
	return 1;
}

static int translate_address(const struct sockaddr* host_address,
			     const char* name, unsigned char* storage, void** result)
{
	*result = NULL;
	if (!host_address)
		return 1;
	if (host_address->sa_family == AF_PACKET) {
		if (!translate_link_address((const struct sockaddr_ll*)host_address,
		                            name, storage))
			return 0;
		*result = storage;
		return 1;
	}
	socklen_t host_length;
	switch (host_address->sa_family) {
	case AF_INET: host_length = sizeof(struct sockaddr_in); break;
	case AF_INET6: host_length = sizeof(struct sockaddr_in6); break;
	default: return 1;
	}
	struct sockaddr_storage host_storage = {0};
	memcpy(&host_storage, host_address, host_length);
	socklen_t result_length = 256;
	if (!machgate_linux_sockaddr_to_darwin(&host_storage, host_length,
	                                      storage, &result_length))
		return 0;
	*result = storage;
	return 1;
}

static uint32_t read_interface_counter(const char* name, const char* counter)
{
	char path[256];
	unsigned long long result = 0;

	int path_length = snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/%s",
	                           name, counter);
	if (path_length < 0 || (size_t)path_length >= sizeof(path))
		return 0;
	FILE* file = fopen(path, "r");
	if (!file)
		return 0;
	if (fscanf(file, "%llu", &result) != 1)
		result = 0;
	fclose(file);
	return (uint32_t)result;
}

static void read_interface_statistics(const char* name, struct darwin_if_data* result)
{
	result->input_packets = read_interface_counter(name, "rx_packets");
	result->input_errors = read_interface_counter(name, "rx_errors");
	result->output_packets = read_interface_counter(name, "tx_packets");
	result->output_errors = read_interface_counter(name, "tx_errors");
	result->input_bytes = read_interface_counter(name, "rx_bytes");
	result->output_bytes = read_interface_counter(name, "tx_bytes");
	result->input_drops = read_interface_counter(name, "rx_dropped");
}

static struct machgate_darwin_ifaddrs* copy_interface(const struct ifaddrs* host)
{
	const char* name = host->ifa_name ? host->ifa_name : "";
	struct interface_addresses* result = calloc(1, sizeof(*result) + strlen(name) + 1);
	if (!result)
		return NULL;
	strcpy(result->name, name);
	result->interface.name = result->name;
	result->interface.flags = interface_flags(host->ifa_flags);
	if (host->ifa_addr && host->ifa_addr->sa_family == AF_PACKET) {
		const struct sockaddr_ll* link = (const struct sockaddr_ll*)host->ifa_addr;
		result->data.ifi_type = interface_type(link->sll_hatype);
		result->data.ifi_addrlen = link->sll_halen;
		result->interface.data = &result->data;
		read_interface_statistics(name, &result->data);
	}
	const struct sockaddr* destination = NULL;
	if (host->ifa_flags & (IFF_POINTOPOINT | IFF_BROADCAST))
		destination = host->ifa_dstaddr;
	if (!translate_address(host->ifa_addr, name, result->address,
	                       &result->interface.address) ||
	    !translate_address(host->ifa_netmask, name, result->netmask,
	                       &result->interface.netmask) ||
	    !translate_address(destination, name, result->destination,
	                       &result->interface.destination)) {
		int saved_errno = errno;
		free(result);
		errno = saved_errno;
		return NULL;
	}
	return &result->interface;
}

void machgate_darwin_freeifaddrs(struct machgate_darwin_ifaddrs* addresses)
{
	while (addresses) {
		struct machgate_darwin_ifaddrs* next = addresses->next;
		free(addresses);
		addresses = next;
	}
}

int machgate_darwin_getifaddrs(struct machgate_darwin_ifaddrs** addresses)
{
	if (!addresses) {
		errno = 14;
		return -1;
	}
	*addresses = NULL;
	struct ifaddrs* host_addresses = NULL;
	if (getifaddrs(&host_addresses) != 0) {
		errno = machgate_darwin_socket_errno_from_linux(errno);
		return -1;
	}
	struct machgate_darwin_ifaddrs** next = addresses;
	int saved_errno = 0;
	for (const struct ifaddrs* host = host_addresses; host; host = host->ifa_next) {
		if (host->ifa_addr && host->ifa_addr->sa_family != AF_INET &&
		    host->ifa_addr->sa_family != AF_INET6 && host->ifa_addr->sa_family != AF_PACKET)
			continue;
		*next = copy_interface(host);
		if (!*next) {
			saved_errno = errno ? errno : ENOMEM;
			break;
		}
		next = &(*next)->next;
	}
	freeifaddrs(host_addresses);
	if (!saved_errno)
		return 0;
	machgate_darwin_freeifaddrs(*addresses);
	*addresses = NULL;
	errno = machgate_darwin_socket_errno_from_linux(saved_errno);
	return -1;
}
