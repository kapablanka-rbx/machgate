typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef unsigned long size_t;

struct darwin_sockaddr {
	uint8_t length;
	uint8_t family;
	uint8_t data[126];
};

struct darwin_ifaddrs {
	struct darwin_ifaddrs* next;
	char* name;
	uint32_t flags;
	struct darwin_sockaddr* address;
	struct darwin_sockaddr* netmask;
	struct darwin_sockaddr* destination;
	void* data;
};

extern int getifaddrs(struct darwin_ifaddrs** result);
extern void freeifaddrs(struct darwin_ifaddrs* addresses);

static int valid_address(const struct darwin_sockaddr* address)
{
	if (!address)
		return 1;
	if (address->family == 2)
		return address->length == 16;
	if (address->family == 30)
		return address->length == 28;
	if (address->family == 18)
		return address->length >= 8;
	return 0;
}

int main(void)
{
	struct darwin_ifaddrs* addresses = 0;
	struct darwin_ifaddrs* independent = 0;
	if (getifaddrs(&addresses) != 0 || !addresses)
		return 10;
	int loopback_ipv4 = 0;
	int links = 0;
	for (struct darwin_ifaddrs* item = addresses; item; item = item->next) {
		if (!item->name || !item->name[0] || !valid_address(item->address) ||
		    !valid_address(item->netmask) || !valid_address(item->destination))
			return 11;
		if (!item->address)
			continue;
		if ((item->flags & 8) && item->address->family == 2) {
			const uint8_t* bytes = (const uint8_t*)item->address;
			if (bytes[4] != 127 || !(item->flags & 1))
				return 12;
			loopback_ipv4++;
		}
		if (item->address->family == 18) {
			const uint8_t* bytes = (const uint8_t*)item->address;
			if ((unsigned)bytes[5] + bytes[6] + 8 > bytes[0])
				return 13;
			if (item->data)
				return 17;
			links++;
		}
	}
	if (!loopback_ipv4 || !links)
		return 14;
	if (getifaddrs(&independent) != 0 || !independent || independent == addresses)
		return 15;
	freeifaddrs(addresses);
	if (!independent->name[0] || !valid_address(independent->address))
		return 16;
	freeifaddrs(independent);
	freeifaddrs(0);
	return 0;
}
