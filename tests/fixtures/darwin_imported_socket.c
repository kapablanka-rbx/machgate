typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef signed int int32_t;
typedef unsigned long uint64_t;
typedef signed long int64_t;

struct darwin_sockaddr_in {
	uint8_t sin_len;
	uint8_t sin_family;
	uint16_t sin_port;
	uint32_t sin_addr;
	uint8_t sin_zero[8];
};

struct darwin_iovec {
	void* iov_base;
	uint64_t iov_len;
};

struct darwin_msghdr {
	void* msg_name;
	uint32_t msg_namelen;
	struct darwin_iovec* msg_iov;
	int32_t msg_iovlen;
	void* msg_control;
	uint32_t msg_controllen;
	int32_t msg_flags;
};

struct darwin_cmsghdr {
	uint32_t cmsg_len;
	int32_t cmsg_level;
	int32_t cmsg_type;
};

extern int socket(int domain, int type, int protocol);
extern int bind(int fd, const void* address, uint32_t address_len);
extern int connect(int fd, const void* address, uint32_t address_len);
extern int listen(int fd, int backlog);
extern int accept(int fd, void* address, uint32_t* address_len);
extern int getsockname(int fd, void* address, uint32_t* address_len);
extern int64_t send(int fd, const void* buffer, uint64_t length, int flags);
extern int64_t recv(int fd, void* buffer, uint64_t length, int flags);
extern int64_t sendto(int fd, const void* buffer, uint64_t length, int flags,
                      const void* address, uint32_t address_len);
extern int64_t recvfrom(int fd, void* buffer, uint64_t length, int flags,
                        void* address, uint32_t* address_len);
extern int64_t sendmsg(int fd, const struct darwin_msghdr* message, int flags);
extern int64_t recvmsg(int fd, struct darwin_msghdr* message, int flags);
extern int setsockopt(int fd, int level, int option, const void* value,
                      uint32_t value_len);
extern int getsockopt(int fd, int level, int option, void* value,
                      uint32_t* value_len);
extern int inet_pton(int family, const char* source, void* destination);
extern const char* inet_ntop(int family, const void* source, char* destination,
                             uint32_t destination_len);
extern int socketpair(int domain, int type, int protocol, int descriptors[2]);
extern int64_t sendmsg_x(int fd, void* messages, unsigned int count, int flags);
extern int64_t recvmsg_x(int fd, void* messages, unsigned int count, int flags);
extern int close(int fd);
extern int* __error(void);

static uint16_t host_to_big_endian16(uint16_t value)
{
	return (uint16_t)((value << 8) | (value >> 8));
}

static int address_matches(const struct darwin_sockaddr_in* address,
                           uint16_t expected_port)
{
	char text[16];
	uint32_t expected_address;

	return address->sin_len == 16 && address->sin_family == 2 &&
	       address->sin_port == host_to_big_endian16(expected_port) &&
	       inet_pton(2, "127.0.0.1", &expected_address) == 1 &&
	       address->sin_addr == expected_address &&
	       inet_ntop(2, &address->sin_addr, text, sizeof(text)) == text &&
	       text[0] == '1' && text[1] == '2' && text[2] == '7' &&
	       text[3] == '.' && text[4] == '0' && text[5] == '.' &&
	       text[6] == '0' && text[7] == '.' && text[8] == '1' &&
	       text[9] == '\0';
}

static int test_option_numbers(void)
{
	static const int options[][5] = {
		{2, 2, 0, 4, 33},
		{2, 2, 0, 3, 16},
		{2, 2, 0, 10, 7},
		{2, 2, 0, 11, 0},
		{2, 1, 6, 4, 1},
		{30, 2, 41, 4, 65},
		{30, 2, 41, 36, 32},
	};
	for (unsigned i = 0; i < sizeof(options) / sizeof(options[0]); ++i) {
		int fd = socket(options[i][0], options[i][1], 0);
		int value = options[i][4];
		if (fd < 0 || setsockopt(fd, options[i][2], options[i][3], &value, sizeof(value)) != 0)
			return 80 + i;
		value = 0;
		uint32_t length = sizeof(value);
		if (getsockopt(fd, options[i][2], options[i][3], &value, &length) != 0 ||
		    value != options[i][4])
			return 90 + i;
		close(fd);
	}
	return 0;
}

static int test_stream_sockets(void)
{
	struct darwin_sockaddr_in address = {0};
	struct darwin_sockaddr_in peer = {0};
	uint32_t peer_len = sizeof(peer);
	char byte = 's';
	char received = 0;
	int listener;
	int client;
	int server;
	int option = 1;
	uint32_t option_len = sizeof(option);

	address.sin_len = sizeof(address);
	address.sin_family = 2;
	if (inet_pton(2, "127.0.0.1", &address.sin_addr) != 1)
		return 10;
	listener = socket(2, 1, 0);
	if (listener < 0 || setsockopt(listener, 0xffff, 4, &option,
	                              sizeof(option)) != 0)
		return 11;
	option = 0;
	if (getsockopt(listener, 0xffff, 4, &option, &option_len) != 0 ||
	    option != 1)
		return 12;
	if (setsockopt(listener, 0xffff, 0x1022, &option, sizeof(option)) != 0)
		return 13;
	uint32_t address_len = sizeof(address);
	if (bind(listener, &address, sizeof(address)) != 0 ||
	    listen(listener, 1) != 0 ||
	    getsockname(listener, &address, &address_len) != 0 ||
	    address_len != sizeof(address))
		return 14;
	client = socket(2, 1, 0);
	if (client < 0 || connect(client, &address, sizeof(address)) != 0)
		return 15;
	server = accept(listener, &peer, &peer_len);
	if (server < 0 || peer_len != sizeof(peer) || peer.sin_len != 16 ||
	    peer.sin_family != 2)
		return 16;
	address.sin_port = 0;
	if (connect(client, &address, sizeof(address)) != -1 || *__error() != 56)
		return 18;
	if (send(client, &byte, 1, 0x80000) != 1 ||
	    recv(server, &received, 1, 0x80) != 1 || received != byte)
		return 17;
	close(server);
	close(client);
	close(listener);
	return 0;
}

static int test_datagram_sockets(void)
{
	struct darwin_sockaddr_in receiver_address = {0};
	struct darwin_sockaddr_in sender_address = {0};
	uint32_t sender_len = sizeof(sender_address);
	char byte = 'd';
	char received = 0;
	struct darwin_iovec send_iov = { &byte, 1 };
	struct darwin_iovec recv_iov = { &received, 1 };
	struct darwin_msghdr send_message = {0};
	struct darwin_msghdr recv_message = {0};
	int receiver;
	int sender;

	receiver_address.sin_len = sizeof(receiver_address);
	receiver_address.sin_family = 2;
	if (inet_pton(2, "127.0.0.1", &receiver_address.sin_addr) != 1)
		return 20;
	receiver = socket(2, 2, 0);
	sender = socket(2, 2, 0);
	uint32_t receiver_len = sizeof(receiver_address);
	if (receiver < 0 || sender < 0 ||
	    bind(receiver, &receiver_address, sizeof(receiver_address)) != 0 ||
	    getsockname(receiver, &receiver_address, &receiver_len) != 0 ||
	    receiver_len != sizeof(receiver_address) ||
	    connect(sender, &receiver_address, sizeof(receiver_address)) != 0)
		return 21;
	if (sendto(sender, &byte, 1, 0x80000, &receiver_address,
	           sizeof(receiver_address)) != 1)
		return 22;
	if (recvfrom(receiver, &received, 1, 0, &sender_address, &sender_len) != 1 ||
	    received != byte || sender_len != 16 || sender_address.sin_len != 16 ||
	    sender_address.sin_family != 2 ||
	    sender_address.sin_addr != receiver_address.sin_addr)
		return 23;

	send_message.msg_iov = &send_iov;
	send_message.msg_iovlen = 1;
	if (sendmsg(sender, &send_message, 0x80000) != 1)
		return 24;
	recv_message.msg_iov = &recv_iov;
	recv_message.msg_iovlen = 1;
	if (recvmsg(receiver, &recv_message, 0) != 1 || received != byte)
		return 25;
	close(receiver);
	close(sender);
	return 0;
}

static int test_unix_message_rights(void)
{
	int pair[2];
	char byte = 'u';
	char received = 0;
	struct darwin_iovec send_iov = { &byte, 1 };
	struct darwin_iovec recv_iov = { &received, 1 };
	struct darwin_msghdr send_message = {0};
	struct darwin_msghdr recv_message = {0};
	unsigned char send_control[16] = {0};
	unsigned char recv_control[16] = {0};
	struct darwin_cmsghdr* send_cmsg =
		(struct darwin_cmsghdr*)send_control;
	struct darwin_cmsghdr* recv_cmsg =
		(struct darwin_cmsghdr*)recv_control;

	if (socketpair(1, 1 | 0x20000000, 0, pair) != 0)
		return 27;
	send_cmsg->cmsg_len = sizeof(*send_cmsg) + sizeof(int);
	send_cmsg->cmsg_level = 0xffff;
	send_cmsg->cmsg_type = 1;
	*(int*)(send_control + sizeof(*send_cmsg)) = pair[0];
	send_message.msg_iov = &send_iov;
	send_message.msg_iovlen = 1;
	send_message.msg_control = send_control;
	send_message.msg_controllen = sizeof(send_control);
	if (sendmsg(pair[0], &send_message, 0x80000) != 1)
		return 28;
	recv_message.msg_iov = &recv_iov;
	recv_message.msg_iovlen = 1;
	recv_message.msg_control = recv_control;
	recv_message.msg_controllen = sizeof(recv_control);
	if (recvmsg(pair[1], &recv_message, 0) != 1 || received != byte)
		return 29;
	if (recv_message.msg_controllen != sizeof(recv_control) ||
	    recv_cmsg->cmsg_len != sizeof(*recv_cmsg) + sizeof(int) ||
	    recv_cmsg->cmsg_level != 0xffff || recv_cmsg->cmsg_type != 1)
		return 30;
	close(*(int*)(recv_control + sizeof(*recv_cmsg)));
	close(pair[1]);
	if (send(pair[0], &byte, 1, 0x80000) != -1 || *__error() != 32)
		return 31;
	close(pair[0]);
	return 0;
}

struct darwin_sa_endpoints {
	uint32_t sae_srcif;
	const void* sae_srcaddr;
	uint32_t sae_srcaddrlen;
	const void* sae_dstaddr;
	uint32_t sae_dstaddrlen;
};

_Static_assert(sizeof(struct darwin_sa_endpoints) == 40, "Darwin ARM64 sa_endpoints size");

static int raw_connect_error(int fd, const void* address, uint32_t length,
                             uint64_t number)
{
	struct darwin_sa_endpoints endpoints = {
		.sae_dstaddr = address,
		.sae_dstaddrlen = length,
	};
	register uint64_t argument0 __asm__("x0") = (uint64_t)fd;
	register uint64_t argument1 __asm__("x1") = number == 447 ? (uint64_t)&endpoints : (uint64_t)address;
	register uint64_t argument2 __asm__("x2") = number == 447 ? 0 : length;
	register uint64_t argument3 __asm__("x3") = 0;
	register uint64_t argument4 __asm__("x4") = 0;
	register uint64_t argument5 __asm__("x5") = 0;
	register uint64_t argument6 __asm__("x6") = 0;
	register uint64_t argument7 __asm__("x7") = 0;
	register uint64_t syscall_number __asm__("x16") = number;
	uint64_t failed;
	__asm__ volatile("svc #0x80\n\tcset %w1, cs"
		: "+r"(argument0), "=&r"(failed)
		: "r"(argument1), "r"(argument2), "r"(argument3), "r"(argument4),
		  "r"(argument5), "r"(argument6), "r"(argument7), "r"(syscall_number)
		: "memory", "cc");
	return failed ? (int)argument0 : 0;
}

static int test_connect_zero_port(void)
{
	for (int family_index = 0; family_index < 2; family_index++) {
		uint8_t address[28] = {0};
		uint32_t length = family_index ? 28 : 16;
		int family = family_index ? 30 : 2;
		address[0] = (uint8_t)length;
		address[1] = (uint8_t)family;
		if (inet_pton(family, family_index ? "::1" : "127.0.0.1",
		              address + (family_index ? 8 : 4)) != 1)
			return 40;
		for (int type = 1; type <= 2; type++) {
			int fd = socket(family, type, 0);
			if (fd < 0)
				return 41;
			if (connect(fd, address, length) != -1 || *__error() != 49)
				return 42;
			if (raw_connect_error(fd, address, length, 98) != 49 ||
			    raw_connect_error(fd, address, length, 409) != 49 ||
			    raw_connect_error(fd, address, length, 447) != 49)
				return 43;
			close(fd);
		}
		if (connect(-1, address, length) != -1 || *__error() != 9 ||
		    raw_connect_error(-1, address, length, 98) != 9 ||
		    raw_connect_error(-1, address, length, 409) != 9 ||
		    raw_connect_error(-1, address, length, 447) != 9)
			return 44;
	}
	return 0;
}

int main(void)
{
	int zero_port_result = test_connect_zero_port();
	if (zero_port_result)
		return zero_port_result;
	int option_result = test_option_numbers();
	if (option_result)
		return option_result;
	int result = test_stream_sockets();

	if (result)
		return result;
	result = test_datagram_sockets();
	if (result)
		return result;
	result = test_unix_message_rights();
	if (result)
		return result;
	if (sendmsg_x(-1, 0, 0, 0) != -1 || *__error() != 78)
		return 30;
	if (recvmsg_x(-1, 0, 0, 0) != -1 || *__error() != 78)
		return 31;
	return 0;
}
