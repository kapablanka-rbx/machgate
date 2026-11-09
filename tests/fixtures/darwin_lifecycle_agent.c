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

extern int sysctl(const int*, unsigned int, void*, size_t*, const void*, size_t);
extern int getifaddrs(struct darwin_ifaddrs**);
extern void freeifaddrs(struct darwin_ifaddrs*);
extern int socket(int, int, int);
extern int close(int);

static int read_udp_sysctl(int selector, void* output, size_t* size)
{
	int name[] = {4, 2, 17, selector};
	return sysctl(name, 4, output, size, 0, 0);
}

static int check_udp_pcblist(void)
{
	size_t size = 0;
	if (read_udp_sysctl(5, 0, &size) || size < 8)
		return 10;
	uint32_t header[4] = {0xa5a5a5a5, 0xa5a5a5a5, 0xa5a5a5a5, 0xa5a5a5a5};
	size = sizeof(header);
	if (read_udp_sysctl(5, header, &size) || size != sizeof(header))
		return 11;
	if (!header[0] || header[0] > size)
		return 12;
	size = 1;
	if (read_udp_sysctl(5, header, &size) != -1 || size != sizeof(header))
		return 13;
	size = sizeof(header);
	if (read_udp_sysctl(5, header, &size) || header[1] < 1)
		return 14;
	int name[] = {4, 2, 17, 5};
	if (sysctl(name, 4, header, &size, header, sizeof(header)) != -1)
		return 15;
	return 0;
}

static int check_udp_stats(void)
{
	uint32_t stats[22];
	size_t size = 0;
	if (read_udp_sysctl(2, 0, &size) || size != sizeof(stats))
		return 20;
	for (size_t index = 0; index < 22; index++)
		stats[index] = 0xa5a5a5a5;
	size = sizeof(stats);
	if (read_udp_sysctl(2, stats, &size) || size != sizeof(stats))
		return 21;
	return 0;
}

static int check_interface_data(void)
{
	struct darwin_ifaddrs* addresses = 0;
	if (getifaddrs(&addresses) || !addresses)
		return 30;
	int found = 0;
	for (struct darwin_ifaddrs* item = addresses; item; item = item->next) {
		if (!item->address || item->address->family != 18 || (item->flags & 8))
			continue;
		if (!item->data)
			return 31;
		const uint8_t* bytes = item->data;
		uint32_t received;
		uint32_t sent;
		__builtin_memcpy(&received, bytes + 40, sizeof(received));
		__builtin_memcpy(&sent, bytes + 44, sizeof(sent));
		found = 1;
	}
	freeifaddrs(addresses);
	return found ? 0 : 32;
}

int main(void)
{
	int descriptor = socket(2, 2, 0);
	if (descriptor < 0)
		return 1;
	int result = check_udp_pcblist();
	close(descriptor);
	if (result)
		return result;
	result = check_udp_stats();
	if (result)
		return result;
	return check_interface_data();
}
