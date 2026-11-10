#include "darwin_socket.h"
#include "ports/socket_state.h"

#ifdef MACHGATE_SYSTEM_SHIM
#define DARWIN_SOCKET_PUBLIC __attribute__((visibility("hidden")))
#else
#define DARWIN_SOCKET_PUBLIC
#endif

#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <unistd.h>

#define DARWIN_MSG_OOB       0x0001
#define DARWIN_MSG_PEEK      0x0002
#define DARWIN_MSG_DONTROUTE 0x0004
#define DARWIN_MSG_TRUNC     0x0010
#define DARWIN_MSG_CTRUNC    0x0020
#define DARWIN_MSG_WAITALL   0x0040
#define DARWIN_MSG_DONTWAIT  0x0080
#define DARWIN_MSG_NOSIGNAL  0x80000

#define DARWIN_SOL_SOCKET 0xffff
#define DARWIN_SO_DEBUG      0x0001
#define DARWIN_SO_ACCEPTCONN 0x0002
#define DARWIN_SO_REUSEADDR  0x0004
#define DARWIN_SO_KEEPALIVE  0x0008
#define DARWIN_SO_DONTROUTE  0x0010
#define DARWIN_SO_BROADCAST  0x0020
#define DARWIN_SO_LINGER     0x0080
#define DARWIN_SO_OOBINLINE  0x0100
#define DARWIN_SO_REUSEPORT  0x0200
#define DARWIN_SO_SNDBUF     0x1001
#define DARWIN_SO_RCVBUF     0x1002
#define DARWIN_SO_SNDLOWAT   0x1003
#define DARWIN_SO_RCVLOWAT   0x1004
#define DARWIN_SO_SNDTIMEO   0x1005
#define DARWIN_SO_RCVTIMEO   0x1006
#define DARWIN_SO_ERROR      0x1007
#define DARWIN_SO_TYPE       0x1008
#define DARWIN_SO_NOSIGPIPE  0x1022

#define DARWIN_IPPROTO_IP   0
#define DARWIN_IPPROTO_TCP  6
#define DARWIN_IPPROTO_IPV6 41
#define DARWIN_IP_PKTINFO   26
#define DARWIN_IP_DONTFRAG  28
#define DARWIN_TCP_KEEPALIVE 0x10
#define DARWIN_TCP_KEEPINTVL 0x101
#define DARWIN_TCP_KEEPCNT   0x102
#define DARWIN_IPV6_MULTICAST_IF    9
#define DARWIN_IPV6_MULTICAST_HOPS 10
#define DARWIN_IPV6_MULTICAST_LOOP 11
#define DARWIN_IPV6_JOIN_GROUP     12
#define DARWIN_IPV6_LEAVE_GROUP    13
#define DARWIN_IPV6_UNICAST_HOPS    4
#define DARWIN_IPV6_PKTINFO        46
#define DARWIN_IPV6_CHECKSUM       26
#define DARWIN_IPV6_V6ONLY         27
#define DARWIN_IPV6_RECVPKTINFO    61

static int connect_destination_port_is_zero(const struct sockaddr_storage* address)
{
	if (address->ss_family == AF_INET)
		return ((const struct sockaddr_in*)address)->sin_port == 0;
	if (address->ss_family == AF_INET6)
		return ((const struct sockaddr_in6*)address)->sin6_port == 0;
	return 0;
}

static int connect_ipv6_v4mapped_v6only(int fd,
                                        const struct sockaddr_storage* address)
{
	const struct sockaddr_in6* addr6;
	const uint8_t* bytes;
	int v6only = 0;
	socklen_t optlen = sizeof(v6only);
	static const uint8_t v4mapped_prefix[12] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff
	};

	if (address->ss_family != AF_INET6)
		return 0;
	addr6 = (const struct sockaddr_in6*)address;
	bytes = (const uint8_t*)&addr6->sin6_addr;
	if (memcmp(bytes, v4mapped_prefix, 12) != 0)
		return 0;
	if (syscall(SYS_getsockopt, fd, IPPROTO_IPV6, IPV6_V6ONLY,
	            &v6only, &optlen) < 0)
		return 0;
	return v6only != 0;
}

DARWIN_SOCKET_PUBLIC long machgate_darwin_connect(int fd, const void* address,
                                                 size_t address_length)
{
	struct sockaddr_storage linux_address = {0};
	socklen_t linux_length;
	if (!machgate_darwin_sockaddr_to_linux(address, address_length,
	                                      &linux_address, &linux_length))
		return -1;
	return machgate_darwin_connect_translated(fd, &linux_address, linux_length);
}

DARWIN_SOCKET_PUBLIC long machgate_darwin_connect_translated(int fd,
                         const struct sockaddr_storage* address, socklen_t length)
{
	if (connect_ipv6_v4mapped_v6only(fd, address)) {
		errno = EINVAL;
		return -1;
	}
	if (connect_destination_port_is_zero(address)) {
		int socket_type;
		int socket_domain;
		socklen_t size = sizeof(int);
		if (syscall(SYS_getsockopt, fd, SOL_SOCKET, SO_TYPE, &socket_type, &size) < 0 ||
		    syscall(SYS_getsockopt, fd, SOL_SOCKET, SO_DOMAIN, &socket_domain, &size) < 0)
			return -1;
		if (socket_domain == address->ss_family && socket_type != SOCK_RAW) {
			if (socket_type == SOCK_STREAM) {
				struct sockaddr_storage peer;
				socklen_t peer_size = sizeof(peer);
				if (syscall(SYS_getpeername, fd, &peer, &peer_size) == 0) {
					errno = EISCONN;
					return -1;
				}
			}
			errno = EADDRNOTAVAIL;
			return -1;
		}
	}
	return syscall(SYS_connect, fd, address, length);
}

#define DARWIN_CMSG_ALIGN(length) (((length) + 3u) & ~3u)

DARWIN_SOCKET_PUBLIC int machgate_darwin_socket_errno_from_linux(int linux_errno)
{
	switch (linux_errno) {
	case 0: return 0;
	case EPERM: return 1;
	case ENOENT: return 2;
	case ESRCH: return 3;
	case EINTR: return 4;
	case EIO: return 5;
	case ENXIO: return 6;
	case E2BIG: return 7;
	case ENOEXEC: return 8;
	case EBADF: return 9;
	case ECHILD: return 10;
	case EDEADLK: return 11;
	case ENOMEM: return 12;
	case EACCES: return 13;
	case EFAULT: return 14;
#ifdef ENOTBLK
	case ENOTBLK: return 15;
#endif
	case EBUSY: return 16;
	case EEXIST: return 17;
	case EXDEV: return 18;
	case ENODEV: return 19;
	case ENOTDIR: return 20;
	case EISDIR: return 21;
	case EINVAL: return 22;
	case ENFILE: return 23;
	case EMFILE: return 24;
	case ENOTTY: return 25;
	case ETXTBSY: return 26;
	case EFBIG: return 27;
	case ENOSPC: return 28;
	case ESPIPE: return 29;
	case EROFS: return 30;
	case EMLINK: return 31;
	case EPIPE: return 32;
	case EDOM: return 33;
	case ERANGE: return 34;
#if EWOULDBLOCK != EAGAIN
	case EWOULDBLOCK: return 35;
#endif
	case EAGAIN: return 35;
	case EINPROGRESS: return 36;
	case EALREADY: return 37;
	case ENOTSOCK: return 38;
	case EDESTADDRREQ: return 39;
	case EMSGSIZE: return 40;
	case EPROTOTYPE: return 41;
	case ENOPROTOOPT: return 42;
	case EPROTONOSUPPORT: return 43;
#ifdef ESOCKTNOSUPPORT
	case ESOCKTNOSUPPORT: return 44;
#endif
#if defined(ENOTSUP) && ENOTSUP != EOPNOTSUPP
	case ENOTSUP:
#endif
	case EOPNOTSUPP: return 102;
#ifdef EPFNOSUPPORT
	case EPFNOSUPPORT: return 46;
#endif
	case EAFNOSUPPORT: return 47;
	case EADDRINUSE: return 48;
	case EADDRNOTAVAIL: return 49;
	case ENETDOWN: return 50;
	case ENETUNREACH: return 51;
	case ENETRESET: return 52;
	case ECONNABORTED: return 53;
	case ECONNRESET: return 54;
	case ENOBUFS: return 55;
	case EISCONN: return 56;
	case ENOTCONN: return 57;
#ifdef ESHUTDOWN
	case ESHUTDOWN: return 58;
#endif
#ifdef ETOOMANYREFS
	case ETOOMANYREFS: return 59;
#endif
	case ETIMEDOUT: return 60;
	case ECONNREFUSED: return 61;
	case ELOOP: return 62;
	case ENAMETOOLONG: return 63;
#ifdef EHOSTDOWN
	case EHOSTDOWN: return 64;
#endif
	case EHOSTUNREACH: return 65;
	case ENOTEMPTY: return 66;
#ifdef EPROCLIM
	case EPROCLIM: return 67;
#endif
#ifdef EUSERS
	case EUSERS: return 68;
#endif
	case EDQUOT: return 69;
	case ESTALE: return 70;
#ifdef EREMOTE
	case EREMOTE: return 71;
#endif
#ifdef EBADRPC
	case EBADRPC: return 72;
#endif
#ifdef ERPCMISMATCH
	case ERPCMISMATCH: return 73;
#endif
#ifdef EPROGUNAVAIL
	case EPROGUNAVAIL: return 74;
#endif
#ifdef EPROGMISMATCH
	case EPROGMISMATCH: return 75;
#endif
#ifdef EPROCUNAVAIL
	case EPROCUNAVAIL: return 76;
#endif
	case ENOLCK: return 77;
	case ENOSYS: return 78;
#ifdef EFTYPE
	case EFTYPE: return 79;
#endif
#ifdef EAUTH
	case EAUTH: return 80;
#endif
#ifdef ENEEDAUTH
	case ENEEDAUTH: return 81;
#endif
#ifdef EPWROFF
	case EPWROFF: return 82;
#endif
#ifdef EDEVERR
	case EDEVERR: return 83;
#endif
	case EOVERFLOW: return 84;
#ifdef EBADEXEC
	case EBADEXEC: return 85;
#endif
#ifdef EBADARCH
	case EBADARCH: return 86;
#endif
#ifdef ESHLIBVERS
	case ESHLIBVERS: return 87;
#endif
#ifdef EBADMACHO
	case EBADMACHO: return 88;
#endif
	case ECANCELED: return 89;
#ifdef EIDRM
	case EIDRM: return 90;
#endif
#ifdef ENOMSG
	case ENOMSG: return 91;
#endif
	case EILSEQ: return 92;
#ifdef EBADMSG
	case EBADMSG: return 94;
#endif
#ifdef EMULTIHOP
	case EMULTIHOP: return 95;
#endif
#ifdef ENODATA
	case ENODATA: return 96;
#endif
#ifdef ENOLINK
	case ENOLINK: return 97;
#endif
#ifdef ENOSR
	case ENOSR: return 98;
#endif
#ifdef ENOSTR
	case ENOSTR: return 99;
#endif
#ifdef EPROTO
	case EPROTO: return 100;
#endif
#ifdef ETIME
	case ETIME: return 101;
#endif
	default: return 22;
	}
}

DARWIN_SOCKET_PUBLIC int machgate_darwin_send_flags_to_linux(int darwin_flags)
{
	int result = 0;

	if (darwin_flags & DARWIN_MSG_OOB)
		result |= MSG_OOB;
	if (darwin_flags & DARWIN_MSG_DONTROUTE)
		result |= MSG_DONTROUTE;
	if (darwin_flags & DARWIN_MSG_DONTWAIT)
		result |= MSG_DONTWAIT;
	if (darwin_flags & DARWIN_MSG_NOSIGNAL)
		result |= MSG_NOSIGNAL;
	return result;
}

DARWIN_SOCKET_PUBLIC int machgate_darwin_recv_flags_to_linux(int darwin_flags)
{
	int result = machgate_darwin_send_flags_to_linux(darwin_flags) &
	             ~MSG_NOSIGNAL;

	if (darwin_flags & DARWIN_MSG_PEEK)
		result |= MSG_PEEK;
	if (darwin_flags & DARWIN_MSG_TRUNC)
		result |= MSG_TRUNC;
	if (darwin_flags & DARWIN_MSG_CTRUNC)
		result |= MSG_CTRUNC;
	if (darwin_flags & DARWIN_MSG_WAITALL)
		result |= MSG_WAITALL;
	return result;
}

DARWIN_SOCKET_PUBLIC int machgate_darwin_recv_flags_from_linux(int linux_flags)
{
	int result = 0;

	if (linux_flags & MSG_OOB)
		result |= DARWIN_MSG_OOB;
	if (linux_flags & MSG_PEEK)
		result |= DARWIN_MSG_PEEK;
	if (linux_flags & MSG_DONTROUTE)
		result |= DARWIN_MSG_DONTROUTE;
	if (linux_flags & MSG_TRUNC)
		result |= DARWIN_MSG_TRUNC;
	if (linux_flags & MSG_CTRUNC)
		result |= DARWIN_MSG_CTRUNC;
	if (linux_flags & MSG_WAITALL)
		result |= DARWIN_MSG_WAITALL;
	if (linux_flags & MSG_DONTWAIT)
		result |= DARWIN_MSG_DONTWAIT;
	return result;
}

DARWIN_SOCKET_PUBLIC int machgate_darwin_socket_domain_to_linux(int darwin_domain,
                                           int* linux_domain)
{
	switch (darwin_domain) {
	case DARWIN_AF_UNIX:
		*linux_domain = AF_UNIX;
		return 1;
	case DARWIN_AF_INET:
		*linux_domain = AF_INET;
		return 1;
	case DARWIN_AF_INET6:
		*linux_domain = AF_INET6;
		return 1;
	default:
		return 0;
	}
}

DARWIN_SOCKET_PUBLIC int machgate_darwin_socket_type_to_linux(int darwin_type)
{
	int result = darwin_type & ~(DARWIN_SOCK_CLOEXEC | DARWIN_SOCK_NONBLOCK);

	if (darwin_type & DARWIN_SOCK_CLOEXEC)
		result |= SOCK_CLOEXEC;
	if (darwin_type & DARWIN_SOCK_NONBLOCK)
		result |= SOCK_NONBLOCK;
	return result;
}

DARWIN_SOCKET_PUBLIC int machgate_darwin_sockaddr_to_linux(const void* darwin_addr,
                                      uint64_t darwin_len,
                                      struct sockaddr_storage* linux_addr,
                                      socklen_t* linux_len)
{
	const unsigned char* raw = (const unsigned char*)darwin_addr;
	int linux_domain;

	if (!darwin_addr) {
		*linux_len = 0;
		return 1;
	}
	if (darwin_len < 2 || darwin_len > sizeof(*linux_addr)) {
		errno = EINVAL;
		return 0;
	}
	if (!machgate_darwin_socket_domain_to_linux(raw[1], &linux_domain)) {
		errno = EAFNOSUPPORT;
		return 0;
	}

	memset(linux_addr, 0, sizeof(*linux_addr));
	switch (raw[1]) {
	case DARWIN_AF_UNIX: {
		struct sockaddr_un* unix_addr = (struct sockaddr_un*)linux_addr;
		size_t path_len = darwin_len - 2;

		if (path_len > sizeof(unix_addr->sun_path)) {
			errno = ENAMETOOLONG;
			return 0;
		}
		unix_addr->sun_family = AF_UNIX;
		memcpy(unix_addr->sun_path, raw + 2, path_len);
		*linux_len = (socklen_t)(offsetof(struct sockaddr_un, sun_path) +
		                         path_len);
		return 1;
	}
	case DARWIN_AF_INET: {
		struct sockaddr_in* inet_addr = (struct sockaddr_in*)linux_addr;

		if (darwin_len < 16) {
			errno = EINVAL;
			return 0;
		}
		inet_addr->sin_family = AF_INET;
		memcpy(&inet_addr->sin_port, raw + 2, 2);
		memcpy(&inet_addr->sin_addr, raw + 4, 4);
		*linux_len = sizeof(*inet_addr);
		return 1;
	}
	case DARWIN_AF_INET6: {
		struct sockaddr_in6* inet6_addr = (struct sockaddr_in6*)linux_addr;

		if (darwin_len < 28) {
			errno = EINVAL;
			return 0;
		}
		inet6_addr->sin6_family = AF_INET6;
		memcpy(&inet6_addr->sin6_port, raw + 2, 2);
		memcpy(&inet6_addr->sin6_flowinfo, raw + 4, 4);
		memcpy(&inet6_addr->sin6_addr, raw + 8, 16);
		memcpy(&inet6_addr->sin6_scope_id, raw + 24, 4);
		*linux_len = sizeof(*inet6_addr);
		return 1;
	}
	default:
		errno = EAFNOSUPPORT;
		return 0;
	}
}

DARWIN_SOCKET_PUBLIC int machgate_linux_sockaddr_to_darwin(const struct sockaddr_storage* linux_addr,
                                      socklen_t linux_len, void* darwin_addr,
                                      socklen_t* darwin_len)
{
	unsigned char* raw = (unsigned char*)darwin_addr;
	socklen_t capacity;

	if (!darwin_addr || !darwin_len)
		return 1;

	capacity = *darwin_len;
	memset(raw, 0, capacity);
	switch (linux_addr->ss_family) {
	case AF_UNIX: {
		const struct sockaddr_un* unix_addr =
			(const struct sockaddr_un*)linux_addr;
		socklen_t path_len = 0;

		if (linux_len > offsetof(struct sockaddr_un, sun_path))
			path_len = linux_len - offsetof(struct sockaddr_un, sun_path);
		if (path_len + 2 > capacity || path_len + 2 > UINT8_MAX) {
			errno = EINVAL;
			return 0;
		}
		raw[0] = (unsigned char)(path_len + 2);
		raw[1] = DARWIN_AF_UNIX;
		memcpy(raw + 2, unix_addr->sun_path, path_len);
		*darwin_len = path_len + 2;
		return 1;
	}
	case AF_INET: {
		const struct sockaddr_in* inet_addr =
			(const struct sockaddr_in*)linux_addr;

		if (capacity < 16) {
			errno = EINVAL;
			return 0;
		}
		raw[0] = 16;
		raw[1] = DARWIN_AF_INET;
		memcpy(raw + 2, &inet_addr->sin_port, 2);
		memcpy(raw + 4, &inet_addr->sin_addr, 4);
		*darwin_len = 16;
		return 1;
	}
	case AF_INET6: {
		const struct sockaddr_in6* inet6_addr =
			(const struct sockaddr_in6*)linux_addr;

		if (capacity < 28) {
			errno = EINVAL;
			return 0;
		}
		raw[0] = 28;
		raw[1] = DARWIN_AF_INET6;
		memcpy(raw + 2, &inet6_addr->sin6_port, 2);
		memcpy(raw + 4, &inet6_addr->sin6_flowinfo, 4);
		memcpy(raw + 8, &inet6_addr->sin6_addr, 16);
		memcpy(raw + 24, &inet6_addr->sin6_scope_id, 4);
		*darwin_len = 28;
		return 1;
	}
	default:
		errno = EAFNOSUPPORT;
		return 0;
	}
}

static int translate_sol_socket_option(int darwin_option, int* linux_option)
{
	switch (darwin_option) {
	case DARWIN_SO_DEBUG: *linux_option = SO_DEBUG; return 1;
	case DARWIN_SO_ACCEPTCONN: *linux_option = SO_ACCEPTCONN; return 1;
	case DARWIN_SO_REUSEADDR: *linux_option = SO_REUSEADDR; return 1;
	case DARWIN_SO_KEEPALIVE: *linux_option = SO_KEEPALIVE; return 1;
	case DARWIN_SO_DONTROUTE: *linux_option = SO_DONTROUTE; return 1;
	case DARWIN_SO_BROADCAST: *linux_option = SO_BROADCAST; return 1;
	case DARWIN_SO_LINGER: *linux_option = SO_LINGER; return 1;
	case DARWIN_SO_OOBINLINE: *linux_option = SO_OOBINLINE; return 1;
#ifdef SO_REUSEPORT
	case DARWIN_SO_REUSEPORT: *linux_option = SO_REUSEPORT; return 1;
#endif
	case DARWIN_SO_SNDBUF: *linux_option = SO_SNDBUF; return 1;
	case DARWIN_SO_RCVBUF: *linux_option = SO_RCVBUF; return 1;
#ifdef SO_SNDLOWAT
	case DARWIN_SO_SNDLOWAT: *linux_option = SO_SNDLOWAT; return 1;
#endif
#ifdef SO_RCVLOWAT
	case DARWIN_SO_RCVLOWAT: *linux_option = SO_RCVLOWAT; return 1;
#endif
#ifdef SO_SNDTIMEO
	case DARWIN_SO_SNDTIMEO: *linux_option = SO_SNDTIMEO; return 1;
#endif
#ifdef SO_RCVTIMEO
	case DARWIN_SO_RCVTIMEO: *linux_option = SO_RCVTIMEO; return 1;
#endif
	case DARWIN_SO_ERROR: *linux_option = SO_ERROR; return 1;
	case DARWIN_SO_TYPE: *linux_option = SO_TYPE; return 1;
	default: return 0;
	}
}

struct socket_option_translation {
	int darwin_level;
	int darwin_option;
	int linux_level;
	int linux_option;
};

static const struct socket_option_translation socket_options[] = {
	{0, 1, IPPROTO_IP, IP_OPTIONS},
	{0, 2, IPPROTO_IP, IP_HDRINCL},
	{0, 3, IPPROTO_IP, IP_TOS},
	{0, 4, IPPROTO_IP, IP_TTL},
	{0, 5, IPPROTO_IP, IP_RECVOPTS},
	{0, 9, IPPROTO_IP, IP_MULTICAST_IF},
	{0, 10, IPPROTO_IP, IP_MULTICAST_TTL},
	{0, 11, IPPROTO_IP, IP_MULTICAST_LOOP},
	{0, 12, IPPROTO_IP, IP_ADD_MEMBERSHIP},
	{0, 13, IPPROTO_IP, IP_DROP_MEMBERSHIP},
	{0, 24, IPPROTO_IP, IP_RECVTTL},
	{0, DARWIN_IP_PKTINFO, IPPROTO_IP, IP_PKTINFO},
	{0, 27, IPPROTO_IP, IP_RECVTOS},
	{0, DARWIN_IP_DONTFRAG, IPPROTO_IP, IP_MTU_DISCOVER},
	{6, 1, IPPROTO_TCP, TCP_NODELAY},
	{6, 2, IPPROTO_TCP, TCP_MAXSEG},
	{6, 4, IPPROTO_TCP, TCP_CORK},
	{6, DARWIN_TCP_KEEPALIVE, IPPROTO_TCP, TCP_KEEPIDLE},
	{6, DARWIN_TCP_KEEPINTVL, IPPROTO_TCP, TCP_KEEPINTVL},
	{6, DARWIN_TCP_KEEPCNT, IPPROTO_TCP, TCP_KEEPCNT},
	{6, 0x201, IPPROTO_TCP, TCP_NOTSENT_LOWAT},
	{41, DARWIN_IPV6_UNICAST_HOPS, IPPROTO_IPV6, IPV6_UNICAST_HOPS},
	{41, DARWIN_IPV6_MULTICAST_IF, IPPROTO_IPV6, IPV6_MULTICAST_IF},
	{41, DARWIN_IPV6_MULTICAST_HOPS, IPPROTO_IPV6, IPV6_MULTICAST_HOPS},
	{41, DARWIN_IPV6_MULTICAST_LOOP, IPPROTO_IPV6, IPV6_MULTICAST_LOOP},
	{41, DARWIN_IPV6_JOIN_GROUP, IPPROTO_IPV6, IPV6_JOIN_GROUP},
	{41, DARWIN_IPV6_LEAVE_GROUP, IPPROTO_IPV6, IPV6_LEAVE_GROUP},
	{41, 19, IPPROTO_IPV6, IPV6_2292PKTINFO},
	{41, 20, IPPROTO_IPV6, IPV6_2292HOPLIMIT},
	{41, DARWIN_IPV6_CHECKSUM, IPPROTO_IPV6, IPV6_CHECKSUM},
	{41, DARWIN_IPV6_V6ONLY, IPPROTO_IPV6, IPV6_V6ONLY},
	{41, 35, IPPROTO_IPV6, IPV6_RECVTCLASS},
	{41, 36, IPPROTO_IPV6, IPV6_TCLASS},
	{41, 37, IPPROTO_IPV6, IPV6_RECVHOPLIMIT},
	{41, 44, IPPROTO_IPV6, IPV6_MTU},
	{41, DARWIN_IPV6_PKTINFO, IPPROTO_IPV6, IPV6_PKTINFO},
	{41, 47, IPPROTO_IPV6, IPV6_HOPLIMIT},
	{41, DARWIN_IPV6_RECVPKTINFO, IPPROTO_IPV6, IPV6_RECVPKTINFO},
	{41, 62, IPPROTO_IPV6, IPV6_DONTFRAG},
};

DARWIN_SOCKET_PUBLIC int machgate_darwin_socket_option_to_linux(int darwin_level, int darwin_option,
                                           int* linux_level, int* linux_option)
{
	if (darwin_level == DARWIN_SOL_SOCKET) {
		*linux_level = SOL_SOCKET;
		return translate_sol_socket_option(darwin_option, linux_option);
	}
	for (size_t i = 0; i < sizeof(socket_options) / sizeof(socket_options[0]); ++i) {
		const struct socket_option_translation* option = &socket_options[i];
		if (option->darwin_level != darwin_level || option->darwin_option != darwin_option)
			continue;
		*linux_level = option->linux_level;
		*linux_option = option->linux_option;
		return 1;
	}
	return 0;
}

DARWIN_SOCKET_PUBLIC int machgate_darwin_socket_option_value_to_linux(int darwin_level,
                                                 int darwin_option,
                                                 const void* darwin_value,
                                                 uint32_t darwin_value_len,
                                                 void* linux_value,
                                                 socklen_t* linux_value_len)
{
	if (darwin_level == DARWIN_IPPROTO_IP &&
	    darwin_option == DARWIN_IP_DONTFRAG) {
		if (!darwin_value || darwin_value_len < sizeof(int)) {
			errno = EINVAL;
			return 0;
		}
		*(int*)linux_value = *(const int*)darwin_value > 0 ?
		                     IP_PMTUDISC_DO : IP_PMTUDISC_DONT;
		*linux_value_len = sizeof(int);
		return 1;
	}

	if ((darwin_level == DARWIN_SOL_SOCKET &&
	     (darwin_option == DARWIN_SO_SNDTIMEO ||
	      darwin_option == DARWIN_SO_RCVTIMEO))) {
		const int64_t* darwin_timeval = (const int64_t*)darwin_value;
		struct timeval* linux_timeval = (struct timeval*)linux_value;

		if (!darwin_value || darwin_value_len < 16) {
			errno = EINVAL;
			return 0;
		}
		linux_timeval->tv_sec = (time_t)darwin_timeval[0];
		linux_timeval->tv_usec = (suseconds_t)*(const int32_t*)(darwin_timeval + 1);
		*linux_value_len = sizeof(*linux_timeval);
		return 1;
	}

	if (darwin_value_len > *linux_value_len) {
		errno = EINVAL;
		return 0;
	}
	if (darwin_value_len)
		memcpy(linux_value, darwin_value, darwin_value_len);
	*linux_value_len = darwin_value_len;
	return 1;
}

DARWIN_SOCKET_PUBLIC int machgate_darwin_socket_option_value_from_linux(int darwin_level,
                                                   int darwin_option,
                                                   void* darwin_value,
                                                   socklen_t* darwin_value_len)
{
	if (darwin_level == DARWIN_IPPROTO_IP &&
	    darwin_option == DARWIN_IP_DONTFRAG) {
		if (!darwin_value || !darwin_value_len ||
		    *darwin_value_len < sizeof(int)) {
			errno = EINVAL;
			return 0;
		}
		*(int*)darwin_value = *(const int*)darwin_value == IP_PMTUDISC_DO;
		*darwin_value_len = sizeof(int);
		return 1;
	}

	if (darwin_level == DARWIN_SOL_SOCKET &&
	    (darwin_option == DARWIN_SO_SNDTIMEO ||
	     darwin_option == DARWIN_SO_RCVTIMEO)) {
		struct timeval linux_timeval;
		int64_t* darwin_timeval = (int64_t*)darwin_value;

		if (!darwin_value || !darwin_value_len || *darwin_value_len < 16) {
			errno = EINVAL;
			return 0;
		}
		memcpy(&linux_timeval, darwin_value, sizeof(linux_timeval));
		darwin_timeval[0] = (int64_t)linux_timeval.tv_sec;
		*(int32_t*)(darwin_timeval + 1) = (int32_t)linux_timeval.tv_usec;
		*((int32_t*)(darwin_timeval + 1) + 1) = 0;
		*darwin_value_len = 16;
		return 1;
	}

	if (darwin_level == DARWIN_SOL_SOCKET &&
	    darwin_option == DARWIN_SO_ERROR) {
		if (!darwin_value || !darwin_value_len ||
		    *darwin_value_len < sizeof(int)) {
			errno = EINVAL;
			return 0;
		}
		*(int*)darwin_value = machgate_darwin_socket_errno_from_linux(*(int*)darwin_value);
	}
	return 1;
}

DARWIN_SOCKET_PUBLIC int machgate_darwin_socket_option_value_is_noop(int darwin_level,
                                                int darwin_option)
{
	return darwin_level == DARWIN_SOL_SOCKET &&
	       darwin_option == DARWIN_SO_NOSIGPIPE;
}

static int translate_cmsg_level_to_linux(int level)
{
	return level == DARWIN_SOL_SOCKET ? SOL_SOCKET : level;
}

static int translate_cmsg_level_to_darwin(int level)
{
	return level == SOL_SOCKET ? DARWIN_SOL_SOCKET : level;
}

static int translate_cmsg_type_to_linux(int level, int type)
{
	if (level == DARWIN_IPPROTO_IP && type == DARWIN_IP_PKTINFO)
		return IP_PKTINFO;
	if (level == DARWIN_IPPROTO_IPV6 && type == DARWIN_IPV6_PKTINFO)
		return IPV6_PKTINFO;
	return type;
}

static int translate_cmsg_type_to_darwin(int level, int type)
{
	if (level == IPPROTO_IP && type == IP_PKTINFO)
		return DARWIN_IP_PKTINFO;
	if (level == IPPROTO_IPV6 && type == IPV6_PKTINFO)
		return DARWIN_IPV6_PKTINFO;
	return type;
}

DARWIN_SOCKET_PUBLIC size_t machgate_darwin_cmsgs_to_linux(const void* darwin_control,
                                      size_t darwin_length,
                                      void* linux_control,
                                      size_t linux_capacity,
                                      size_t* required_capacity)
{
	size_t darwin_offset = 0;
	size_t linux_offset = 0;

	if (required_capacity)
		*required_capacity = 0;
	if (!darwin_control && darwin_length)
		return 0;

	while (darwin_offset + sizeof(struct darwin_cmsghdr) <= darwin_length) {
		const unsigned char* darwin_bytes = (const unsigned char*)darwin_control;
		const struct darwin_cmsghdr* darwin_cmsg =
			(const struct darwin_cmsghdr*)(darwin_bytes + darwin_offset);
		size_t data_length;
		size_t linux_stride;

		if (darwin_cmsg->cmsg_len < sizeof(*darwin_cmsg) ||
		    darwin_cmsg->cmsg_len > darwin_length - darwin_offset)
			break;
		data_length = darwin_cmsg->cmsg_len - sizeof(*darwin_cmsg);
		linux_stride = CMSG_SPACE(data_length);
		if (linux_control && linux_offset + linux_stride > linux_capacity)
			break;

		if (linux_control) {
			unsigned char* linux_bytes = (unsigned char*)linux_control;
			struct cmsghdr* linux_cmsg =
				(struct cmsghdr*)(linux_bytes + linux_offset);

			memset(linux_cmsg, 0, linux_stride);
			linux_cmsg->cmsg_len = CMSG_LEN(data_length);
			linux_cmsg->cmsg_level = translate_cmsg_level_to_linux(
				darwin_cmsg->cmsg_level);
			linux_cmsg->cmsg_type = translate_cmsg_type_to_linux(
				darwin_cmsg->cmsg_level, darwin_cmsg->cmsg_type);
			memcpy(CMSG_DATA(linux_cmsg),
			       (const unsigned char*)darwin_cmsg + sizeof(*darwin_cmsg),
			       data_length);
		}
		linux_offset += linux_stride;
		darwin_offset += DARWIN_CMSG_ALIGN(darwin_cmsg->cmsg_len);
	}

	if (required_capacity)
		*required_capacity = linux_offset;
	return linux_control ? linux_offset : 0;
}

DARWIN_SOCKET_PUBLIC size_t machgate_linux_cmsgs_to_darwin(const void* linux_control,
                                      size_t linux_length,
                                      void* darwin_control,
                                      size_t darwin_capacity,
                                      size_t* required_capacity)
{
	size_t linux_offset = 0;
	size_t darwin_offset = 0;

	if (required_capacity)
		*required_capacity = 0;
	if (!linux_control && linux_length)
		return 0;

	while (linux_offset + sizeof(struct cmsghdr) <= linux_length) {
		const unsigned char* linux_bytes = (const unsigned char*)linux_control;
		const struct cmsghdr* linux_cmsg =
			(const struct cmsghdr*)(linux_bytes + linux_offset);
		size_t data_length;
		size_t darwin_stride;

		if (linux_cmsg->cmsg_len < sizeof(*linux_cmsg) ||
		    linux_cmsg->cmsg_len > linux_length - linux_offset)
			break;
		data_length = linux_cmsg->cmsg_len - sizeof(*linux_cmsg);
		darwin_stride = DARWIN_CMSG_ALIGN(sizeof(struct darwin_cmsghdr) +
		                                  data_length);
		if (darwin_control && darwin_offset + darwin_stride > darwin_capacity)
			break;

		if (darwin_control) {
			unsigned char* darwin_bytes = (unsigned char*)darwin_control;
			struct darwin_cmsghdr* darwin_cmsg =
				(struct darwin_cmsghdr*)(darwin_bytes + darwin_offset);

			memset(darwin_cmsg, 0, darwin_stride);
			darwin_cmsg->cmsg_len = sizeof(*darwin_cmsg) + data_length;
			darwin_cmsg->cmsg_level = translate_cmsg_level_to_darwin(
				linux_cmsg->cmsg_level);
			darwin_cmsg->cmsg_type = translate_cmsg_type_to_darwin(
				linux_cmsg->cmsg_level, linux_cmsg->cmsg_type);
			memcpy((unsigned char*)darwin_cmsg + sizeof(*darwin_cmsg),
			       CMSG_DATA(linux_cmsg), data_length);
		}
		darwin_offset += darwin_stride;
		linux_offset += CMSG_ALIGN(linux_cmsg->cmsg_len);
	}

	if (required_capacity)
		*required_capacity = darwin_offset;
	return darwin_control ? darwin_offset : 0;
}

DARWIN_SOCKET_PUBLIC long machgate_darwin_sendmsg(int fd, const struct darwin_msghdr* darwin_msg,
                             int darwin_flags,
                             const struct sockaddr* linux_name,
                             socklen_t linux_name_len)
{
	struct msghdr linux_msg;
	void* linux_control = NULL;
	long result;
	int saved_errno;

	if (!darwin_msg || darwin_msg->msg_iovlen < 0) {
		errno = darwin_msg ? EINVAL : EFAULT;
		return -1;
	}

	memset(&linux_msg, 0, sizeof(linux_msg));
	linux_msg.msg_name = (void*)linux_name;
	linux_msg.msg_namelen = linux_name_len;
	linux_msg.msg_iov = darwin_msg->msg_iov;
	linux_msg.msg_iovlen = (size_t)darwin_msg->msg_iovlen;

	if (darwin_msg->msg_control && darwin_msg->msg_controllen) {
		size_t required_capacity = 0;

		machgate_darwin_cmsgs_to_linux(darwin_msg->msg_control,
		                               darwin_msg->msg_controllen,
		                               NULL, 0, &required_capacity);
		if (!required_capacity) {
			errno = EINVAL;
			return -1;
		}
		linux_control = calloc(1, required_capacity);
		if (!linux_control) {
			errno = ENOMEM;
			return -1;
		}
		linux_msg.msg_controllen = machgate_darwin_cmsgs_to_linux(
			darwin_msg->msg_control, darwin_msg->msg_controllen,
			linux_control, required_capacity, NULL);
		linux_msg.msg_control = linux_control;
	} else if (darwin_msg->msg_controllen || darwin_msg->msg_control) {
		errno = darwin_msg->msg_controllen ? EFAULT : EINVAL;
		return -1;
	}

	errno = 0;
	result = syscall(SYS_sendmsg, fd, &linux_msg,
	                 machgate_socket_send_flags(fd, machgate_darwin_send_flags_to_linux(darwin_flags)));
	saved_errno = errno;
	free(linux_control);
	errno = saved_errno;
	return result;
}

DARWIN_SOCKET_PUBLIC long machgate_darwin_recvmsg(int fd, struct darwin_msghdr* darwin_msg,
                             int darwin_flags,
                             struct sockaddr* linux_name,
                             socklen_t* linux_name_len)
{
	struct msghdr linux_msg;
	void* linux_control = NULL;
	size_t linux_capacity = 0;
	long result;
	int saved_errno;

	if (!darwin_msg || darwin_msg->msg_iovlen < 0) {
		errno = darwin_msg ? EINVAL : EFAULT;
		return -1;
	}

	memset(&linux_msg, 0, sizeof(linux_msg));
	linux_msg.msg_iov = darwin_msg->msg_iov;
	linux_msg.msg_iovlen = (size_t)darwin_msg->msg_iovlen;
	if (darwin_msg->msg_name && linux_name && linux_name_len) {
		linux_msg.msg_name = linux_name;
		linux_msg.msg_namelen = *linux_name_len;
	}

	if (darwin_msg->msg_control && darwin_msg->msg_controllen) {
		linux_capacity = (size_t)darwin_msg->msg_controllen +
		                 CMSG_SPACE(0) * 8 + 64;
		linux_control = calloc(1, linux_capacity);
		if (!linux_control) {
			errno = ENOMEM;
			return -1;
		}
		linux_msg.msg_control = linux_control;
		linux_msg.msg_controllen = linux_capacity;
	} else if (darwin_msg->msg_controllen || darwin_msg->msg_control) {
		errno = darwin_msg->msg_controllen ? EFAULT : EINVAL;
		return -1;
	}

	errno = 0;
	result = syscall(SYS_recvmsg, fd, &linux_msg,
	                 machgate_darwin_recv_flags_to_linux(darwin_flags));
	saved_errno = errno;
	if (result >= 0) {
		darwin_msg->msg_flags = machgate_darwin_recv_flags_from_linux(
			linux_msg.msg_flags);
		if (linux_name_len)
			*linux_name_len = linux_msg.msg_namelen;
		if (linux_control) {
			size_t original_darwin_capacity = darwin_msg->msg_controllen;
			size_t available_darwin_length = 0;
			size_t required_darwin_length = 0;
			size_t repacked_darwin_length;

			machgate_linux_cmsgs_to_darwin(
				linux_control, linux_msg.msg_controllen, NULL, 0,
				&required_darwin_length);
			available_darwin_length = required_darwin_length;
			if (available_darwin_length > original_darwin_capacity)
				available_darwin_length = original_darwin_capacity;
			repacked_darwin_length = machgate_linux_cmsgs_to_darwin(
				linux_control, linux_msg.msg_controllen,
				darwin_msg->msg_control, available_darwin_length, NULL);
			darwin_msg->msg_controllen = (uint32_t)repacked_darwin_length;
			if ((linux_msg.msg_flags & MSG_CTRUNC) ||
			    required_darwin_length > original_darwin_capacity ||
			    repacked_darwin_length < available_darwin_length)
				darwin_msg->msg_flags |= DARWIN_MSG_CTRUNC;
		}
	}
	free(linux_control);
	errno = saved_errno;
	return result;
}
