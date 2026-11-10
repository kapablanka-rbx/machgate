#include "interface_addresses_compat.h"
#include "syscall/darwin_socket.h"

#include <errno.h>
#include <ifaddrs.h>
#include <linux/if_link.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <netpacket/packet.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define DARWIN_AF_LINK 18
#define DARWIN_IFF_MULTICAST 0x8000

/*
 * Darwin struct if_data (LP64 layout): 8 u_char fields, then u_int32_t
 * counters, then a timeval32 (8 bytes), then 3 trailing u_int32_t.
 * Total 96 bytes.  We only need to populate ifi_ibytes (offset 40) and
 * ifi_obytes (offset 44) for the guest network-stats scraper.
 */
enum {
	DARWIN_IF_DATA_SIZE = 96,
	DARWIN_IF_DATA_IPACKETS_OFFSET = 20,
	DARWIN_IF_DATA_IERRORS_OFFSET  = 24,
	DARWIN_IF_DATA_OPACKETS_OFFSET = 28,
	DARWIN_IF_DATA_OERRORS_OFFSET  = 32,
	DARWIN_IF_DATA_IBYTES_OFFSET   = 40,
	DARWIN_IF_DATA_OBYTES_OFFSET   = 44,
};

struct interface_addresses {
	struct machgate_darwin_ifaddrs interface;
	unsigned char address[256];
	unsigned char netmask[256];
	unsigned char destination[256];
	unsigned char if_data[DARWIN_IF_DATA_SIZE];
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

static void populate_if_data(unsigned char* if_data, const void* host_data)
{
	memset(if_data, 0, DARWIN_IF_DATA_SIZE);
	if (!host_data)
		return;
	const struct rtnl_link_stats* stats = host_data;
	uint32_t val;
	val = (uint32_t)stats->rx_packets;
	memcpy(if_data + DARWIN_IF_DATA_IPACKETS_OFFSET, &val, sizeof(val));
	val = (uint32_t)stats->rx_errors;
	memcpy(if_data + DARWIN_IF_DATA_IERRORS_OFFSET, &val, sizeof(val));
	val = (uint32_t)stats->tx_packets;
	memcpy(if_data + DARWIN_IF_DATA_OPACKETS_OFFSET, &val, sizeof(val));
	val = (uint32_t)stats->tx_errors;
	memcpy(if_data + DARWIN_IF_DATA_OERRORS_OFFSET, &val, sizeof(val));
	val = (uint32_t)stats->rx_bytes;
	memcpy(if_data + DARWIN_IF_DATA_IBYTES_OFFSET, &val, sizeof(val));
	val = (uint32_t)stats->tx_bytes;
	memcpy(if_data + DARWIN_IF_DATA_OBYTES_OFFSET, &val, sizeof(val));
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
	/*
	 * For AF_PACKET (link-layer) entries, populate a Darwin if_data
	 * structure with interface statistics from the Linux
	 * rtnl_link_stats provided in ifa_data.
	 */
	if (host->ifa_addr && host->ifa_addr->sa_family == AF_PACKET) {
		populate_if_data(result->if_data, host->ifa_data);
		result->interface.data = result->if_data;
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
