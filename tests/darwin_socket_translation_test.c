#include "syscall/darwin_socket.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
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
#define DARWIN_SO_SNDBUF 0x1001
#define DARWIN_SO_RCVBUF 0x1002
#define DARWIN_SO_SNDLOWAT 0x1003
#define DARWIN_SO_RCVLOWAT 0x1004
#define DARWIN_SO_ERROR 0x1007
#define DARWIN_SO_NOSIGPIPE 0x1022

#define DARWIN_IP_PKTINFO 26
#define DARWIN_IP_DONTFRAG 28
#define DARWIN_TCP_KEEPALIVE 0x10
#define DARWIN_TCP_KEEPINTVL 0x101
#define DARWIN_TCP_KEEPCNT 0x102
#define DARWIN_IPV6_MULTICAST_IF 9
#define DARWIN_IPV6_MULTICAST_HOPS 10
#define DARWIN_IPV6_MULTICAST_LOOP 11
#define DARWIN_IPV6_JOIN_GROUP 12
#define DARWIN_IPV6_LEAVE_GROUP 13
#define DARWIN_IPV6_UNICAST_HOPS 4
#define DARWIN_IPV6_PKTINFO 46
#define DARWIN_IPV6_CHECKSUM 26
#define DARWIN_IPV6_V6ONLY 27
#define DARWIN_IPV6_RECVPKTINFO 61

static void require_condition(int condition, const char* message)
{
	if (condition)
		return;
	fprintf(stderr, "FAIL: %s (errno=%d)\n", message, errno);
	exit(1);
}

static void require_linux_errno(int linux_errno, int darwin_errno)
{
	require_condition(machgate_darwin_socket_errno_from_linux(linux_errno) == darwin_errno,
	                  "Linux-to-Darwin errno mapping mismatch");
}

static void test_errno_mapping(void)
{
	require_linux_errno(0, 0);
	require_linux_errno(EPERM, 1);
	require_linux_errno(ENOENT, 2);
	require_linux_errno(ESRCH, 3);
	require_linux_errno(EINTR, 4);
	require_linux_errno(EIO, 5);
	require_linux_errno(ENXIO, 6);
	require_linux_errno(E2BIG, 7);
	require_linux_errno(ENOEXEC, 8);
	require_linux_errno(EBADF, 9);
	require_linux_errno(ECHILD, 10);
	require_linux_errno(EDEADLK, 11);
	require_linux_errno(ENOMEM, 12);
	require_linux_errno(EACCES, 13);
	require_linux_errno(EFAULT, 14);
	require_linux_errno(ENOTBLK, 15);
	require_linux_errno(EBUSY, 16);
	require_linux_errno(EEXIST, 17);
	require_linux_errno(EXDEV, 18);
	require_linux_errno(ENODEV, 19);
	require_linux_errno(ENOTDIR, 20);
	require_linux_errno(EISDIR, 21);
	require_linux_errno(EINVAL, 22);
	require_linux_errno(ENFILE, 23);
	require_linux_errno(EMFILE, 24);
	require_linux_errno(ENOTTY, 25);
	require_linux_errno(ETXTBSY, 26);
	require_linux_errno(EFBIG, 27);
	require_linux_errno(ENOSPC, 28);
	require_linux_errno(ESPIPE, 29);
	require_linux_errno(EROFS, 30);
	require_linux_errno(EMLINK, 31);
	require_linux_errno(EPIPE, 32);
	require_linux_errno(EDOM, 33);
	require_linux_errno(ERANGE, 34);
	require_linux_errno(EAGAIN, 35);
	require_linux_errno(EINPROGRESS, 36);
	require_linux_errno(EALREADY, 37);
	require_linux_errno(ENOTSOCK, 38);
	require_linux_errno(EDESTADDRREQ, 39);
	require_linux_errno(EMSGSIZE, 40);
	require_linux_errno(EPROTOTYPE, 41);
	require_linux_errno(ENOPROTOOPT, 42);
	require_linux_errno(EPROTONOSUPPORT, 43);
	require_linux_errno(ESOCKTNOSUPPORT, 44);
	require_linux_errno(EOPNOTSUPP, 102);
	require_linux_errno(EPFNOSUPPORT, 46);
	require_linux_errno(EAFNOSUPPORT, 47);
	require_linux_errno(EADDRINUSE, 48);
	require_linux_errno(EADDRNOTAVAIL, 49);
	require_linux_errno(ENETDOWN, 50);
	require_linux_errno(ENETUNREACH, 51);
	require_linux_errno(ENETRESET, 52);
	require_linux_errno(ECONNABORTED, 53);
	require_linux_errno(ECONNRESET, 54);
	require_linux_errno(ENOBUFS, 55);
	require_linux_errno(EISCONN, 56);
	require_linux_errno(ENOTCONN, 57);
	require_linux_errno(ESHUTDOWN, 58);
	require_linux_errno(ETOOMANYREFS, 59);
	require_linux_errno(ETIMEDOUT, 60);
	require_linux_errno(ECONNREFUSED, 61);
	require_linux_errno(ELOOP, 62);
	require_linux_errno(ENAMETOOLONG, 63);
	require_linux_errno(EHOSTDOWN, 64);
	require_linux_errno(EHOSTUNREACH, 65);
	require_linux_errno(ENOTEMPTY, 66);
	require_linux_errno(EUSERS, 68);
	require_linux_errno(EDQUOT, 69);
	require_linux_errno(ESTALE, 70);
	require_linux_errno(EREMOTE, 71);
	require_linux_errno(ENOLCK, 77);
	require_linux_errno(ENOSYS, 78);
	require_linux_errno(EOVERFLOW, 84);
	require_linux_errno(ECANCELED, 89);
	require_linux_errno(EIDRM, 90);
	require_linux_errno(ENOMSG, 91);
	require_linux_errno(EILSEQ, 92);
	require_linux_errno(EBADMSG, 94);
	require_linux_errno(EMULTIHOP, 95);
	require_linux_errno(ENODATA, 96);
	require_linux_errno(ENOLINK, 97);
	require_linux_errno(ENOSR, 98);
	require_linux_errno(ENOSTR, 99);
	require_linux_errno(EPROTO, 100);
	require_linux_errno(ETIME, 101);
	require_linux_errno(10000, 22);
}

static void test_flag_translation(void)
{
	int send_flags = DARWIN_MSG_OOB | DARWIN_MSG_DONTROUTE |
	                 DARWIN_MSG_DONTWAIT | DARWIN_MSG_NOSIGNAL;
	int recv_flags = DARWIN_MSG_OOB | DARWIN_MSG_PEEK | DARWIN_MSG_DONTROUTE |
	                 DARWIN_MSG_TRUNC | DARWIN_MSG_CTRUNC |
	                 DARWIN_MSG_WAITALL | DARWIN_MSG_DONTWAIT;

	require_condition(machgate_darwin_send_flags_to_linux(send_flags) ==
	                  (MSG_OOB | MSG_DONTROUTE | MSG_DONTWAIT | MSG_NOSIGNAL),
	                  "send flags were not translated");
	require_condition(machgate_darwin_recv_flags_to_linux(recv_flags) ==
	                  (MSG_OOB | MSG_PEEK | MSG_DONTROUTE | MSG_TRUNC |
	                   MSG_CTRUNC | MSG_WAITALL | MSG_DONTWAIT),
	                  "recv flags were not translated");
	require_condition(machgate_darwin_recv_flags_from_linux(
	                  machgate_darwin_recv_flags_to_linux(recv_flags)) == recv_flags,
	                  "recv flags did not round-trip");
}

static void require_socket_option(int darwin_level, int darwin_option,
                                  int expected_linux_level,
                                  int expected_linux_option)
{
	int linux_level = -1;
	int linux_option = -1;

	require_condition(machgate_darwin_socket_option_to_linux(
	                  darwin_level, darwin_option, &linux_level, &linux_option),
	                  "socket option mapping was rejected");
	require_condition(linux_level == expected_linux_level &&
	                  linux_option == expected_linux_option,
	                  "socket option mapping mismatch");
}

static void test_socket_options(void)
{
	require_socket_option(0, 1, IPPROTO_IP, IP_OPTIONS);
	require_socket_option(0, 2, IPPROTO_IP, IP_HDRINCL);
	require_socket_option(0, 3, IPPROTO_IP, IP_TOS);
	require_socket_option(0, 4, IPPROTO_IP, IP_TTL);
	require_socket_option(0, 5, IPPROTO_IP, IP_RECVOPTS);
	require_socket_option(0, 9, IPPROTO_IP, IP_MULTICAST_IF);
	require_socket_option(0, 10, IPPROTO_IP, IP_MULTICAST_TTL);
	require_socket_option(0, 11, IPPROTO_IP, IP_MULTICAST_LOOP);
	require_socket_option(0, 12, IPPROTO_IP, IP_ADD_MEMBERSHIP);
	require_socket_option(0, 13, IPPROTO_IP, IP_DROP_MEMBERSHIP);
	require_socket_option(0, 24, IPPROTO_IP, IP_RECVTTL);
	require_socket_option(0, 27, IPPROTO_IP, IP_RECVTOS);
	require_socket_option(6, 1, IPPROTO_TCP, TCP_NODELAY);
	require_socket_option(6, 2, IPPROTO_TCP, TCP_MAXSEG);
	require_socket_option(6, 4, IPPROTO_TCP, TCP_CORK);
	require_socket_option(6, 0x201, IPPROTO_TCP, TCP_NOTSENT_LOWAT);
	require_socket_option(41, 19, IPPROTO_IPV6, IPV6_2292PKTINFO);
	require_socket_option(41, 20, IPPROTO_IPV6, IPV6_2292HOPLIMIT);
	require_socket_option(41, 35, IPPROTO_IPV6, IPV6_RECVTCLASS);
	require_socket_option(41, 36, IPPROTO_IPV6, IPV6_TCLASS);
	require_socket_option(41, 37, IPPROTO_IPV6, IPV6_RECVHOPLIMIT);
	require_socket_option(41, 44, IPPROTO_IPV6, IPV6_MTU);
	require_socket_option(41, 47, IPPROTO_IPV6, IPV6_HOPLIMIT);
	require_socket_option(41, 62, IPPROTO_IPV6, IPV6_DONTFRAG);
	const int unsupported[][2] = {{0, 25}, {6, 8}, {41, 16}, {0xffff, 0x10000}, {123, 1}};
	for (size_t i = 0; i < sizeof(unsupported) / sizeof(unsupported[0]); ++i) {
		int level;
		int option;
		require_condition(!machgate_darwin_socket_option_to_linux(
			unsupported[i][0], unsupported[i][1], &level, &option),
			"unimplemented Darwin option leaked into a Linux option number");
	}
	require_socket_option(0xffff, 0x1001, SOL_SOCKET, SO_SNDBUF);
	require_socket_option(0xffff, 0x1002, SOL_SOCKET, SO_RCVBUF);
	require_socket_option(0xffff, 0x1003, SOL_SOCKET, SO_SNDLOWAT);
	require_socket_option(0xffff, 0x1004, SOL_SOCKET, SO_RCVLOWAT);
	require_socket_option(6, DARWIN_TCP_KEEPALIVE, IPPROTO_TCP, TCP_KEEPIDLE);
	require_socket_option(6, DARWIN_TCP_KEEPINTVL, IPPROTO_TCP, TCP_KEEPINTVL);
	require_socket_option(6, DARWIN_TCP_KEEPCNT, IPPROTO_TCP, TCP_KEEPCNT);
	require_socket_option(0, DARWIN_IP_PKTINFO, IPPROTO_IP, IP_PKTINFO);
	require_socket_option(0, DARWIN_IP_DONTFRAG, IPPROTO_IP, IP_MTU_DISCOVER);
	require_socket_option(41, DARWIN_IPV6_UNICAST_HOPS, IPPROTO_IPV6,
	                      IPV6_UNICAST_HOPS);
	require_socket_option(41, DARWIN_IPV6_V6ONLY, IPPROTO_IPV6, IPV6_V6ONLY);
	require_socket_option(41, DARWIN_IPV6_MULTICAST_IF, IPPROTO_IPV6,
	                      IPV6_MULTICAST_IF);
	require_socket_option(41, DARWIN_IPV6_MULTICAST_HOPS, IPPROTO_IPV6,
	                      IPV6_MULTICAST_HOPS);
	require_socket_option(41, DARWIN_IPV6_MULTICAST_LOOP, IPPROTO_IPV6,
	                      IPV6_MULTICAST_LOOP);
	require_socket_option(41, DARWIN_IPV6_JOIN_GROUP, IPPROTO_IPV6,
	                      IPV6_JOIN_GROUP);
	require_socket_option(41, DARWIN_IPV6_LEAVE_GROUP, IPPROTO_IPV6,
	                      IPV6_LEAVE_GROUP);
	require_socket_option(41, DARWIN_IPV6_CHECKSUM, IPPROTO_IPV6, IPV6_CHECKSUM);
	require_socket_option(41, DARWIN_IPV6_RECVPKTINFO, IPPROTO_IPV6,
	                      IPV6_RECVPKTINFO);
	require_socket_option(41, DARWIN_IPV6_PKTINFO, IPPROTO_IPV6, IPV6_PKTINFO);
	require_condition(machgate_darwin_socket_option_value_is_noop(
	                  DARWIN_SOL_SOCKET, DARWIN_SO_NOSIGPIPE),
	                  "SO_NOSIGPIPE was not handled as a no-op");

	int enabled = 1;
	int policy = 0;
	socklen_t policy_len = sizeof(policy);
	require_condition(machgate_darwin_socket_option_value_to_linux(
	                  0, DARWIN_IP_DONTFRAG, &enabled, sizeof(enabled),
	                  &policy, &policy_len) && policy == IP_PMTUDISC_DO,
	                  "IP_DONTFRAG did not enable path MTU discovery");
	policy_len = sizeof(policy);
	require_condition(machgate_darwin_socket_option_value_from_linux(
	                  0, DARWIN_IP_DONTFRAG, &policy, &policy_len) &&
	                  policy == 1 && policy_len == sizeof(policy),
	                  "IP_DONTFRAG result did not become a Darwin boolean");

	int socket_error = ECONNRESET;
	socklen_t error_len = sizeof(socket_error);
	require_condition(machgate_darwin_socket_option_value_from_linux(
	                  DARWIN_SOL_SOCKET, DARWIN_SO_ERROR, &socket_error,
	                  &error_len) && socket_error == 54,
	                  "SO_ERROR did not return Darwin errno");
}

static void test_cmsg_repacking(void)
{
	unsigned char darwin_control[128] = {0};
	unsigned char linux_control[256] = {0};
	unsigned char roundtrip[128] = {0};
	unsigned char ipv6_control[128] = {0};
	unsigned char ipv6_linux[256] = {0};
	unsigned char ipv6_roundtrip[128] = {0};
	struct darwin_cmsghdr* darwin_cmsg = (struct darwin_cmsghdr*)darwin_control;
	struct cmsghdr* linux_cmsg;
	struct darwin_cmsghdr* roundtrip_cmsg;
	struct in_pktinfo packet_info;
	struct in6_pktinfo packet6_info;
	size_t required_capacity = 0;
	size_t linux_length;
	size_t darwin_length;
	int rights[2] = {7, 11};

	packet_info.ipi_ifindex = 3;
	inet_pton(AF_INET, "127.0.0.1", &packet_info.ipi_spec_dst);
	inet_pton(AF_INET, "127.0.0.2", &packet_info.ipi_addr);
	darwin_cmsg->cmsg_len = sizeof(*darwin_cmsg) + sizeof(packet_info);
	darwin_cmsg->cmsg_level = IPPROTO_IP;
	darwin_cmsg->cmsg_type = DARWIN_IP_PKTINFO;
	memcpy(darwin_control + sizeof(*darwin_cmsg), &packet_info,
	       sizeof(packet_info));

	darwin_cmsg = (struct darwin_cmsghdr*)(darwin_control + 24);
	darwin_cmsg->cmsg_len = sizeof(*darwin_cmsg) + sizeof(rights);
	darwin_cmsg->cmsg_level = DARWIN_SOL_SOCKET;
	darwin_cmsg->cmsg_type = SCM_RIGHTS;
	memcpy((unsigned char*)darwin_cmsg + sizeof(*darwin_cmsg), rights,
	       sizeof(rights));

	machgate_darwin_cmsgs_to_linux(darwin_control, 48, NULL, 0,
	                               &required_capacity);
	require_condition(required_capacity == CMSG_SPACE(sizeof(packet_info)) +
	                  CMSG_SPACE(sizeof(rights)),
	                  "Linux ancillary size query was incorrect");
	linux_length = machgate_darwin_cmsgs_to_linux(
		darwin_control, 48, linux_control, sizeof(linux_control), NULL);
	require_condition(linux_length == required_capacity,
	                  "Darwin ancillary data was not repacked");

	linux_cmsg = (struct cmsghdr*)linux_control;
	require_condition(linux_cmsg->cmsg_len == CMSG_LEN(sizeof(packet_info)) &&
	                  linux_cmsg->cmsg_level == IPPROTO_IP &&
	                  linux_cmsg->cmsg_type == IP_PKTINFO,
	                  "IP_PKTINFO ancillary header was not translated");
	require_condition(memcmp(CMSG_DATA(linux_cmsg), &packet_info,
	                         sizeof(packet_info)) == 0,
	                  "IPv4 packet info payload changed");

	linux_cmsg = (struct cmsghdr*)(linux_control + CMSG_SPACE(sizeof(packet_info)));
	require_condition(linux_cmsg->cmsg_len == CMSG_LEN(sizeof(rights)) &&
	                  linux_cmsg->cmsg_level == SOL_SOCKET &&
	                  linux_cmsg->cmsg_type == SCM_RIGHTS &&
	                  memcmp(CMSG_DATA(linux_cmsg), rights, sizeof(rights)) == 0,
	                  "SCM_RIGHTS ancillary data was not preserved");

	darwin_length = machgate_linux_cmsgs_to_darwin(
		linux_control, linux_length, roundtrip, sizeof(roundtrip), NULL);
	require_condition(darwin_length == 44, "Darwin ancillary size mismatch");
	roundtrip_cmsg = (struct darwin_cmsghdr*)roundtrip;
	require_condition(roundtrip_cmsg->cmsg_len == 12 + sizeof(packet_info) &&
	                  roundtrip_cmsg->cmsg_level == IPPROTO_IP &&
	                  roundtrip_cmsg->cmsg_type == DARWIN_IP_PKTINFO &&
	                  memcmp(roundtrip + 12, &packet_info, sizeof(packet_info)) == 0,
	                  "IP_PKTINFO did not round-trip");
	roundtrip_cmsg = (struct darwin_cmsghdr*)(roundtrip + 24);
	require_condition(roundtrip_cmsg->cmsg_len == 12 + sizeof(rights) &&
	                  roundtrip_cmsg->cmsg_level == DARWIN_SOL_SOCKET &&
	                  roundtrip_cmsg->cmsg_type == SCM_RIGHTS &&
	                  memcmp(roundtrip + 36, rights, sizeof(rights)) == 0,
	                  "SCM_RIGHTS did not round-trip");

	memset(&packet6_info, 0, sizeof(packet6_info));
	inet_pton(AF_INET6, "::1", &packet6_info.ipi6_addr);
	packet6_info.ipi6_ifindex = 1;
	darwin_cmsg = (struct darwin_cmsghdr*)ipv6_control;
	darwin_cmsg->cmsg_len = sizeof(*darwin_cmsg) + sizeof(packet6_info);
	darwin_cmsg->cmsg_level = IPPROTO_IPV6;
	darwin_cmsg->cmsg_type = DARWIN_IPV6_PKTINFO;
	memcpy(ipv6_control + sizeof(*darwin_cmsg), &packet6_info,
	       sizeof(packet6_info));
	linux_length = machgate_darwin_cmsgs_to_linux(
		ipv6_control, 12 + sizeof(packet6_info), ipv6_linux,
		sizeof(ipv6_linux), NULL);
	linux_cmsg = (struct cmsghdr*)ipv6_linux;
	require_condition(linux_cmsg->cmsg_type == IPV6_PKTINFO &&
	                  memcmp(CMSG_DATA(linux_cmsg), &packet6_info,
	                         sizeof(packet6_info)) == 0,
	                  "IPv6 packet info field order changed");
	darwin_length = machgate_linux_cmsgs_to_darwin(
		ipv6_linux, linux_length, ipv6_roundtrip, sizeof(ipv6_roundtrip), NULL);
	roundtrip_cmsg = (struct darwin_cmsghdr*)ipv6_roundtrip;
	require_condition(darwin_length == 12 + sizeof(packet6_info) &&
	                  roundtrip_cmsg->cmsg_type == DARWIN_IPV6_PKTINFO &&
	                  memcmp(ipv6_roundtrip + 12, &packet6_info,
	                         sizeof(packet6_info)) == 0,
	                  "IPv6 packet info did not round-trip verbatim");

	memset(ipv6_roundtrip, 0, sizeof(ipv6_roundtrip));
	darwin_length = machgate_linux_cmsgs_to_darwin(
		ipv6_linux, linux_length, ipv6_roundtrip, 15, NULL);
	require_condition(darwin_length == 0,
	                  "short Darwin ancillary buffer accepted a partial header");
}

static void test_raw_send_and_recv(void)
{
	int sockets[2];
	char byte = 'x';
	char received = 0;
	struct iovec iov = { .iov_base = &byte, .iov_len = 1 };
	struct iovec recv_iov = { .iov_base = &received, .iov_len = 1 };
	struct darwin_msghdr send_msg = {0};
	struct darwin_msghdr recv_msg = {0};
	unsigned char darwin_control[16] = {0};
	struct darwin_cmsghdr* darwin_cmsg;
	int descriptor;

	require_condition(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0,
	                  "socketpair failed");
	send_msg.msg_iov = &iov;
	send_msg.msg_iovlen = 1;
	darwin_cmsg = (struct darwin_cmsghdr*)darwin_control;
	darwin_cmsg->cmsg_len = sizeof(*darwin_cmsg) + sizeof(int);
	darwin_cmsg->cmsg_level = DARWIN_SOL_SOCKET;
	darwin_cmsg->cmsg_type = SCM_RIGHTS;
	descriptor = dup(sockets[0]);
	require_condition(descriptor >= 0, "dup failed");
	memcpy(darwin_control + sizeof(*darwin_cmsg), &descriptor, sizeof(descriptor));
	send_msg.msg_control = darwin_control;
	send_msg.msg_controllen = sizeof(darwin_control);
	require_condition(machgate_darwin_sendmsg(
	                  sockets[0], &send_msg,
	                  DARWIN_MSG_DONTWAIT | DARWIN_MSG_NOSIGNAL, NULL, 0) == 1,
	                  "raw Darwin sendmsg with SCM_RIGHTS failed");

	recv_msg.msg_iov = &recv_iov;
	recv_msg.msg_iovlen = 1;
	recv_msg.msg_control = darwin_control;
	recv_msg.msg_controllen = sizeof(darwin_control);
	require_condition(machgate_darwin_recvmsg(
	                  sockets[1], &recv_msg, DARWIN_MSG_DONTWAIT,
	                  NULL, NULL) == 1 && received == byte,
	                  "raw Darwin recvmsg failed");
	darwin_cmsg = (struct darwin_cmsghdr*)darwin_control;
	require_condition(darwin_cmsg->cmsg_len == sizeof(*darwin_cmsg) + sizeof(int) &&
	                  darwin_cmsg->cmsg_level == DARWIN_SOL_SOCKET &&
	                  darwin_cmsg->cmsg_type == SCM_RIGHTS &&
	                  recv_msg.msg_controllen == sizeof(darwin_control),
	                  "received SCM_RIGHTS used the wrong Darwin layout");
	memcpy(&descriptor, darwin_control + sizeof(*darwin_cmsg), sizeof(descriptor));
	require_condition(descriptor >= 0, "received descriptor was invalid");
	close(descriptor);

	send_msg.msg_control = NULL;
	send_msg.msg_controllen = 0;
	close(sockets[1]);
	require_condition(machgate_darwin_sendmsg(
	                  sockets[0], &send_msg, DARWIN_MSG_NOSIGNAL,
	                  NULL, 0) == -1 && errno == EPIPE,
	                  "MSG_NOSIGNAL send did not report EPIPE");

	send_msg.msg_control = NULL;
	send_msg.msg_controllen = 0;
	require_condition(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0,
	                  "second socketpair failed");
	require_condition(write(sockets[0], &byte, 1) == 1, "socket write failed");
	recv_msg.msg_control = NULL;
	recv_msg.msg_controllen = 0;
	require_condition(machgate_darwin_recvmsg(
	                  sockets[1], &recv_msg,
	                  DARWIN_MSG_PEEK | DARWIN_MSG_WAITALL | DARWIN_MSG_DONTWAIT,
	                  NULL, NULL) == 1 && received == byte,
	                  "PEEK/WAITALL/DONTWAIT recvmsg failed");
	close(sockets[0]);
	close(sockets[1]);
}

static void test_connect_v4mapped_v6only(void)
{
	int descriptor;
	int v6only;
	struct sockaddr_storage linux_addr;
	struct sockaddr_in6* addr6 = (struct sockaddr_in6*)&linux_addr;
	socklen_t addr_len = sizeof(*addr6);

	descriptor = socket(AF_INET6, SOCK_STREAM, 0);
	require_condition(descriptor >= 0, "IPv6 TCP socket failed");

	v6only = 1;
	require_condition(setsockopt(descriptor, IPPROTO_IPV6, IPV6_V6ONLY,
	                             &v6only, sizeof(v6only)) == 0,
	                  "IPV6_V6ONLY setsockopt failed");

	memset(&linux_addr, 0, sizeof(linux_addr));
	addr6->sin6_family = AF_INET6;
	addr6->sin6_port = htons(1);
	/* ::ffff:127.0.0.1 */
	memset(&addr6->sin6_addr.s6_addr[0], 0, 10);
	addr6->sin6_addr.s6_addr[10] = 0xff;
	addr6->sin6_addr.s6_addr[11] = 0xff;
	addr6->sin6_addr.s6_addr[12] = 127;
	addr6->sin6_addr.s6_addr[13] = 0;
	addr6->sin6_addr.s6_addr[14] = 0;
	addr6->sin6_addr.s6_addr[15] = 1;

	require_condition(machgate_darwin_connect_translated(
	                      descriptor, &linux_addr, addr_len) == -1 &&
	                  errno == EINVAL,
	                  "connect to v4-mapped addr with V6ONLY should return EINVAL");
	close(descriptor);

	/* Without V6ONLY the address must not be rejected by the shim. */
	descriptor = socket(AF_INET6, SOCK_STREAM | SOCK_NONBLOCK, 0);
	require_condition(descriptor >= 0, "second IPv6 TCP socket failed");
	v6only = 0;
	require_condition(setsockopt(descriptor, IPPROTO_IPV6, IPV6_V6ONLY,
	                             &v6only, sizeof(v6only)) == 0,
	                  "clearing IPV6_V6ONLY failed");
	/* connect is expected to fail (no listener), but NOT with EINVAL. */
	require_condition(machgate_darwin_connect_translated(
	                      descriptor, &linux_addr, addr_len) == -1 &&
	                  errno != EINVAL,
	                  "connect to v4-mapped addr without V6ONLY returned EINVAL");
	close(descriptor);

	/* Pure IPv6 address must not be blocked even with V6ONLY. */
	descriptor = socket(AF_INET6, SOCK_STREAM | SOCK_NONBLOCK, 0);
	require_condition(descriptor >= 0, "third IPv6 TCP socket failed");
	v6only = 1;
	require_condition(setsockopt(descriptor, IPPROTO_IPV6, IPV6_V6ONLY,
	                             &v6only, sizeof(v6only)) == 0,
	                  "second IPV6_V6ONLY setsockopt failed");
	memset(&addr6->sin6_addr, 0, 16);
	addr6->sin6_addr.s6_addr[15] = 1; /* ::1 */
	require_condition(machgate_darwin_connect_translated(
	                      descriptor, &linux_addr, addr_len) == -1 &&
	                  errno != EINVAL,
	                  "connect to ::1 with V6ONLY should not return EINVAL");
	close(descriptor);
}

int main(void)
{
	signal(SIGPIPE, SIG_IGN);
	test_errno_mapping();
	test_flag_translation();
	test_socket_options();
	test_cmsg_repacking();
	test_raw_send_and_recv();
	test_connect_v4mapped_v6only();
	puts("darwin-socket-translation-ok");
	return 0;
}
