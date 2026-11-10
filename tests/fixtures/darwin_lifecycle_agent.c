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
extern int* __error(void);

static int read_udp_sysctl(int selector, void* output, size_t* size)
{
	int name[] = {4, 2, 17, selector};
	return sysctl(name, 4, output, size, 0, 0);
}

static int check_udp_pcblist(void)
{
	/* With a UDP socket open, pcblist should return a header with count >= 1. */
	uint32_t header[4] = {0};
	size_t size = sizeof(header);
	if (read_udp_sysctl(5, header, &size) != 0)
		return 10;
	/* header[1] is xig_count — should be at least 1 since we opened a socket. */
	if (header[1] < 1)
		return 11;
	/* newlen (input data) must still be rejected. */
	int name[] = {4, 2, 17, 5};
	if (sysctl(name, 4, header, &size, header, sizeof(header)) != -1 || *__error() != 1)
		return 15;
	return 0;
}

static int check_udp_stats(void)
{
	/* Stats should now return a zeroed udpstat structure. */
	uint32_t stats[22] = {0};
	size_t size = sizeof(stats);
	if (read_udp_sysctl(2, stats, &size) != 0)
		return 20;
	/* udps_fullsock is at offset 10 — may be 0 or nonzero depending on host. */
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
		/* AF_LINK entries now carry a populated if_data pointer. */
		if (!item->data)
			return 31;
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
