typedef unsigned int uint32_t;
typedef int int32_t;

struct darwin_addrinfo {
	int32_t flags;
	int32_t family;
	int32_t socktype;
	int32_t protocol;
	uint32_t addrlen;
	char* canonname;
	void* addr;
	struct darwin_addrinfo* next;
};

extern int getaddrinfo(const char*, const char*, const struct darwin_addrinfo*,
                       struct darwin_addrinfo**);
extern void freeaddrinfo(struct darwin_addrinfo*);
extern int memcmp(const void*, const void*, unsigned long);

static int matched_family(const struct darwin_addrinfo* info, int family)
{
	while (info) {
		if (info->family == family)
			return 1;
		info = info->next;
	}
	return 0;
}

static const unsigned char* mapped_loopback(const struct darwin_addrinfo* info)
{
	const unsigned char* address;
	while (info) {
		address = (const unsigned char*)info->addr;
		if (info->family == 30 && info->addrlen == 28 &&
		    address[18] == 0xff && address[19] == 0xff &&
		    address[20] == 127 && address[21] == 0 &&
		    address[22] == 0 && address[23] == 1)
			return address;
		info = info->next;
	}
	return 0;
}

int main(void)
{
	struct darwin_addrinfo hints;
	struct darwin_addrinfo* result;
	int status;

	hints.flags = 0;
	hints.family = 30;
	hints.socktype = 1;
	hints.protocol = 6;
	hints.addrlen = 0;
	hints.canonname = 0;
	hints.addr = 0;
	hints.next = 0;

	status = getaddrinfo("127.0.0.1", "80", &hints, &result);
	if (status)
		return 1;
	if (!result || !matched_family(result, 30) || !mapped_loopback(result))
		return 2;
	freeaddrinfo(result);

	status = getaddrinfo("::ffff:127.0.0.1", "80", &hints, &result);
	if (status)
		return 3;
	if (!result || !matched_family(result, 30) || !mapped_loopback(result))
		return 4;
	freeaddrinfo(result);

	hints.family = 2;
	status = getaddrinfo("127.0.0.1", "80", &hints, &result);
	if (status)
		return 5;
	if (!result || !matched_family(result, 2))
		return 6;
	freeaddrinfo(result);

	hints.family = 30;
	hints.flags = 4;
	status = getaddrinfo("127.0.0.1", "80", &hints, &result);
	if (status != 8)
		return 7;
	if (result)
		return 8;
	freeaddrinfo(result);

	status = getaddrinfo("::1", "80", &hints, &result);
	if (status)
		return 9;
	if (!result || !matched_family(result, 30))
		return 10;
	freeaddrinfo(result);

	status = getaddrinfo("nonexistent.invalid.example", "80", &hints, &result);
	if (status != 2 && status != 7 && status != 8)
		return 11;
	if (result)
		return 12;

	return 0;
}
