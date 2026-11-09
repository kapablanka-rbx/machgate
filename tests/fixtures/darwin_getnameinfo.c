typedef unsigned int uint32_t;

struct socket_address {
	unsigned char length;
	unsigned char family;
	unsigned short port;
	uint32_t flow;
	unsigned char address[16];
	uint32_t scope;
};

extern int getnameinfo(const void*, uint32_t, char*, uint32_t, char*, uint32_t, int);
extern int strcmp(const char*, const char*);

int main(void)
{
	unsigned char ipv4[16] = {16, 2, 0xff, 0xff, 192, 0, 2, 17};
	struct socket_address ipv6 = {28, 30, 0xbb01, 0,
		{0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, 0};
	char host[80];
	char service[16];
	int result = getnameinfo(ipv4, sizeof(ipv4), host, sizeof(host),
	                         service, sizeof(service), 2 | 8);
	if (result || strcmp(host, "192.0.2.17") || strcmp(service, "65535"))
		return 1;
	result = getnameinfo(&ipv6, sizeof(ipv6), host, sizeof(host),
	                     service, sizeof(service), 2 | 8);
	if (result || strcmp(host, "2001:db8::1") || strcmp(service, "443"))
		return 2;
	if (getnameinfo(ipv4, sizeof(ipv4), host, 2, 0, 0, 2) != 14)
		return 3;
	if (getnameinfo(ipv4, sizeof(ipv4), 0, 0, service, 2, 8) != 14)
		return 4;
	if (getnameinfo(ipv4, sizeof(ipv4), host, sizeof(host), 0, 0, 0x4000) != 3)
		return 5;
	ipv4[1] = 99;
	if (getnameinfo(ipv4, sizeof(ipv4), host, sizeof(host), 0, 0, 2) != 5)
		return 6;
	return 0;
}
