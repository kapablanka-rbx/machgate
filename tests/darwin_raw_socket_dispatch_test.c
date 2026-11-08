#include "syscall/darwin_socket.h"
#include "syscall/syscall_range_000_099.h"
#include "syscall/syscall_range_100_199.h"
#include "syscall/syscall_range_400_plus.h"
#include "syscall/syscall_gate.h"

#include <errno.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>

static void require_condition(int condition, const char* message)
{
	if (condition)
		return;
	fprintf(stderr, "FAIL: %s (errno=%d)\n", message, errno);
	exit(1);
}

static void require_success(const struct machgate_syscall_frame* state,
                            const char* message)
{
	require_condition(!(state->nzcv & (1ull << 29)), message);
}

static void require_errno(const struct machgate_syscall_frame* state,
                          int expected_errno, const char* message)
{
	require_condition((state->nzcv & (1ull << 29)) &&
	                  state->x[0] == (uint64_t)expected_errno, message);
}

static void close_socket_pair(int sockets[2])
{
	close(sockets[0]);
	close(sockets[1]);
}

static void test_nonblocking_recv(uint64_t syscall_number, int range)
{
	int sockets[2];
	struct machgate_syscall_frame state = {0};
	char byte;

	require_condition(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0,
	                  "socketpair failed");
	state.x[0] = sockets[1];
	state.x[1] = (uint64_t)&byte;
	state.x[2] = 1;
	state.x[3] = 0x80;
	state.x[16] = syscall_number;
	if (range == 0)
		require_condition(syscall_range_000_099_dispatch(&state) == 1,
		                  "range 000 did not handle recvfrom");
	else
		require_condition(syscall_range_400_plus_dispatch(&state) == 1,
		                  "range 400 did not handle recvfrom");
	require_errno(&state, 35, "DONTWAIT recv did not return Darwin EAGAIN");
	close_socket_pair(sockets);
}

static int inject_non_socket_eOPNOTSUPP;
long __real_syscall(long number, ...);

long __wrap_syscall(long number, ...)
{
	va_list arguments;
	unsigned long values[6];
	long result;

	va_start(arguments, number);
	for (size_t index = 0; index < 6; index++)
		values[index] = va_arg(arguments, unsigned long);
	va_end(arguments);
	if (number == SYS_madvise && inject_non_socket_eOPNOTSUPP) {
		errno = EOPNOTSUPP;
		return -1;
	}
	result = __real_syscall(number, values[0], values[1], values[2], values[3],
	                        values[4], values[5]);
	return result;
}

static void test_non_socket_not_supported(void)
{
	struct machgate_syscall_frame state = {0};

	inject_non_socket_eOPNOTSUPP = 1;
	state.x[16] = 75;
	require_condition(syscall_range_000_099_dispatch(&state) == 1,
	                  "range 000 did not handle madvise");
	inject_non_socket_eOPNOTSUPP = 0;
	require_errno(&state, 102,
	              "raw non-socket EOPNOTSUPP did not map to Darwin 102");
}

static void test_raw_darwin_enotsup_is_not_retranslated(void)
{
	struct machgate_syscall_frame range_state = {0};
	struct machgate_syscall_frame gate_state = {0};

	range_state.x[16] = 56;
	require_condition(syscall_range_000_099_dispatch(&range_state) == 1,
	                  "range 000 did not handle revoke");
	require_errno(&range_state, 45,
	              "raw range Darwin ENOTSUP was translated a second time");

	gate_state.x[16] = 56;
	syscall_gate_dispatch(&gate_state);
	require_errno(&gate_state, 45,
	              "raw gate Darwin ENOTSUP was translated a second time");
}

static void test_socket_operation_not_supported(void)
{
	int sockets[2];
	struct machgate_syscall_frame state = {0};
	char byte;

	require_condition(socketpair(AF_UNIX, SOCK_DGRAM, 0, sockets) == 0,
	                  "datagram socketpair failed");
	state.x[0] = sockets[0];
	state.x[1] = (uint64_t)&byte;
	state.x[2] = 1;
	state.x[3] = 1;
	state.x[16] = 29;
	require_condition(syscall_range_000_099_dispatch(&state) == 1,
	                  "range 000 did not handle MSG_OOB recvfrom");
	require_errno(&state, 102,
	              "raw socket EOPNOTSUPP did not map to Darwin 102");
	close_socket_pair(sockets);
}

static void test_send_nosignal(uint64_t syscall_number, int range)
{
	int sockets[2];
	struct machgate_syscall_frame state = {0};
	char byte = 'x';
	struct iovec iov = { .iov_base = &byte, .iov_len = 1 };
	struct darwin_msghdr message = {0};

	require_condition(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0,
	                  "socketpair failed");
	close(sockets[1]);
	state.x[0] = sockets[0];
	state.x[16] = syscall_number;
	if (syscall_number == 133 || syscall_number == 413) {
		state.x[1] = (uint64_t)&byte;
		state.x[2] = 1;
		state.x[3] = 0x80000;
	} else {
		message.msg_iov = &iov;
		message.msg_iovlen = 1;
		state.x[1] = (uint64_t)&message;
		state.x[2] = 0x80000;
	}
	if (range == 0)
		require_condition(syscall_range_000_099_dispatch(&state) == 1,
		                  "range 000 did not handle sendmsg");
	else if (range == 100)
		require_condition(syscall_range_100_199_dispatch(&state) == 1,
		                  "range 100 did not handle sendto");
	else
		require_condition(syscall_range_400_plus_dispatch(&state) == 1,
		                  "range 400 did not handle send");
	require_errno(&state, 32, "MSG_NOSIGNAL send did not return Darwin EPIPE");
	close(sockets[0]);
}

static void test_scm_rights(uint64_t syscall_number, int range)
{
	int sockets[2];
	int received_sockets[2];
	int duplicate;
	struct machgate_syscall_frame send_state = {0};
	struct machgate_syscall_frame recv_state = {0};
	struct darwin_msghdr send_message = {0};
	struct darwin_msghdr recv_message = {0};
	unsigned char send_control[16] = {0};
	unsigned char recv_control[16] = {0};
	struct darwin_cmsghdr* send_cmsg = (struct darwin_cmsghdr*)send_control;
	struct darwin_cmsghdr* recv_cmsg = (struct darwin_cmsghdr*)recv_control;
	char byte = 'r';
	char received = 0;
	struct iovec send_iov = { .iov_base = &byte, .iov_len = 1 };
	struct iovec recv_iov = { .iov_base = &received, .iov_len = 1 };

	require_condition(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0,
	                  "socketpair failed");
	duplicate = dup(sockets[0]);
	require_condition(duplicate >= 0, "dup failed");
	send_cmsg->cmsg_len = 16;
	send_cmsg->cmsg_level = 0xffff;
	send_cmsg->cmsg_type = SCM_RIGHTS;
	memcpy(send_control + 12, &duplicate, sizeof(duplicate));
	send_message.msg_iov = &send_iov;
	send_message.msg_iovlen = 1;
	send_message.msg_control = send_control;
	send_message.msg_controllen = sizeof(send_control);
	send_state.x[0] = sockets[0];
	send_state.x[1] = (uint64_t)&send_message;
	send_state.x[2] = 0x80000;
	send_state.x[16] = syscall_number;
	if (range == 0)
		require_condition(syscall_range_000_099_dispatch(&send_state) == 1,
		                  "range 000 did not handle ancillary sendmsg");
	else
		require_condition(syscall_range_400_plus_dispatch(&send_state) == 1,
		                  "range 400 did not handle ancillary sendmsg");
	require_success(&send_state, "raw SCM_RIGHTS sendmsg failed");
	require_condition(send_state.x[0] == 1, "raw SCM_RIGHTS send length mismatch");

	recv_message.msg_iov = &recv_iov;
	recv_message.msg_iovlen = 1;
	recv_message.msg_control = recv_control;
	recv_message.msg_controllen = sizeof(recv_control);
	recv_state.x[0] = sockets[1];
	recv_state.x[1] = (uint64_t)&recv_message;
	recv_state.x[2] = 0x80;
	recv_state.x[16] = syscall_number - 1;
	if (range == 0)
		require_condition(syscall_range_000_099_dispatch(&recv_state) == 1,
		                  "range 000 did not handle ancillary recvmsg");
	else
		require_condition(syscall_range_400_plus_dispatch(&recv_state) == 1,
		                  "range 400 did not handle ancillary recvmsg");
	require_success(&recv_state, "raw SCM_RIGHTS recvmsg failed");
	require_condition(recv_state.x[0] == 1 && received == byte &&
	                  recv_message.msg_controllen == 16 &&
	                  recv_cmsg->cmsg_len == 16 &&
	                  recv_cmsg->cmsg_level == 0xffff &&
	                  recv_cmsg->cmsg_type == SCM_RIGHTS,
	                  "raw SCM_RIGHTS recvmsg layout mismatch");
	memcpy(&received_sockets[0], recv_control + 12, sizeof(received_sockets[0]));
	require_condition(fcntl(received_sockets[0], F_GETFD) >= 0,
	                  "raw SCM_RIGHTS did not transfer a descriptor");
	close(received_sockets[0]);
	close(duplicate);
	close_socket_pair(sockets);
}

static void test_socket_option(uint64_t setsockopt_number,
                               uint64_t getsockopt_number)
{
	struct machgate_syscall_frame state = {0};
	int descriptor;
	int value = 30;
	socklen_t value_len = sizeof(value);

	descriptor = socket(AF_INET, SOCK_STREAM, 0);
	require_condition(descriptor >= 0, "TCP socket failed");
	state.x[0] = descriptor;
	state.x[1] = 6;
	state.x[2] = 0x10;
	state.x[3] = (uint64_t)&value;
	state.x[4] = sizeof(value);
	state.x[16] = setsockopt_number;
	require_condition(syscall_range_100_199_dispatch(&state) == 1,
	                  "range 100 did not handle TCP_KEEPALIVE");
	require_success(&state, "Darwin TCP_KEEPALIVE setsockopt failed");
	value_len = sizeof(value);
	memset(&state, 0, sizeof(state));
	state.x[0] = descriptor;
	state.x[1] = 6;
	state.x[2] = 0x10;
	state.x[3] = (uint64_t)&value;
	state.x[4] = (uint64_t)&value_len;
	state.x[16] = getsockopt_number;
	require_condition(syscall_range_100_199_dispatch(&state) == 1,
	                  "range 100 did not handle TCP_KEEPALIVE getsockopt");
	require_success(&state, "Darwin TCP_KEEPALIVE getsockopt failed");
	require_condition(value == 30, "TCP_KEEPALIVE value did not round-trip");
	value = 7;
	memset(&state, 0, sizeof(state));
	state.x[0] = descriptor;
	state.x[1] = 6;
	state.x[2] = 0x101;
	state.x[3] = (uint64_t)&value;
	state.x[4] = sizeof(value);
	state.x[16] = setsockopt_number;
	require_condition(syscall_range_100_199_dispatch(&state) == 1,
	                  "range 100 did not handle TCP_KEEPINTVL");
	require_success(&state, "Darwin TCP_KEEPINTVL setsockopt failed");
	value = 4;
	memset(&state, 0, sizeof(state));
	state.x[0] = descriptor;
	state.x[1] = 6;
	state.x[2] = 0x102;
	state.x[3] = (uint64_t)&value;
	state.x[4] = sizeof(value);
	state.x[16] = setsockopt_number;
	require_condition(syscall_range_100_199_dispatch(&state) == 1,
	                  "range 100 did not handle TCP_KEEPCNT");
	require_success(&state, "Darwin TCP_KEEPCNT setsockopt failed");
	value_len = sizeof(value);
	require_condition(getsockopt(descriptor, IPPROTO_TCP, TCP_KEEPCNT,
	                            &value, &value_len) == 0 && value == 4,
	                  "Darwin TCP_KEEPCNT did not reach Linux");
	close(descriptor);

	descriptor = socket(AF_INET, SOCK_DGRAM, 0);
	require_condition(descriptor >= 0, "UDP socket failed");
	value = 1;
	memset(&state, 0, sizeof(state));
	state.x[0] = descriptor;
	state.x[1] = 0;
	state.x[2] = 26;
	state.x[3] = (uint64_t)&value;
	state.x[4] = sizeof(value);
	state.x[16] = setsockopt_number;
	require_condition(syscall_range_100_199_dispatch(&state) == 1,
	                  "range 100 did not handle IP_PKTINFO");
	require_success(&state, "Darwin IP_PKTINFO setsockopt failed");
	close(descriptor);

	descriptor = socket(AF_INET6, SOCK_DGRAM, 0);
	require_condition(descriptor >= 0, "IPv6 UDP socket failed");
	value = 1;
	memset(&state, 0, sizeof(state));
	state.x[0] = descriptor;
	state.x[1] = 41;
	state.x[2] = 27;
	state.x[3] = (uint64_t)&value;
	state.x[4] = sizeof(value);
	state.x[16] = setsockopt_number;
	require_condition(syscall_range_100_199_dispatch(&state) == 1,
	                  "range 100 did not handle IPV6_V6ONLY");
	require_success(&state, "Darwin IPV6_V6ONLY setsockopt failed");
	value = 1;
	memset(&state, 0, sizeof(state));
	state.x[0] = descriptor;
	state.x[1] = 41;
	state.x[2] = 61;
	state.x[3] = (uint64_t)&value;
	state.x[4] = sizeof(value);
	state.x[16] = setsockopt_number;
	require_condition(syscall_range_100_199_dispatch(&state) == 1,
	                  "range 100 did not handle IPV6_RECVPKTINFO");
	require_success(&state, "Darwin IPV6_RECVPKTINFO setsockopt failed");
	value = 3;
	memset(&state, 0, sizeof(state));
	state.x[0] = descriptor;
	state.x[1] = 41;
	state.x[2] = 4;
	state.x[3] = (uint64_t)&value;
	state.x[4] = sizeof(value);
	state.x[16] = setsockopt_number;
	require_condition(syscall_range_100_199_dispatch(&state) == 1,
	                  "range 100 did not handle IPV6_UNICAST_HOPS");
	require_success(&state, "Darwin IPV6_UNICAST_HOPS setsockopt failed");
	close(descriptor);
}

static void test_socket_buffer_options(void)
{
	struct machgate_syscall_frame state = {0};
	int sockets[2];
	int option_numbers[] = {0x1001, 0x1002, 0x1004};
	int linux_options[] = {SO_SNDBUF, SO_RCVBUF, SO_RCVLOWAT};

	require_condition(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0,
	                  "socketpair failed");
	for (unsigned int index = 0;
	     index < sizeof(option_numbers) / sizeof(option_numbers[0]); index++) {
		int value = 4096;
		int expected_value = 0;
		int linux_value = 0;
		socklen_t value_len = sizeof(value);
		socklen_t linux_len = sizeof(linux_value);

		memset(&state, 0, sizeof(state));
		state.x[0] = sockets[0];
		state.x[1] = 0xffff;
		state.x[2] = option_numbers[index];
		state.x[3] = (uint64_t)&value;
		state.x[4] = sizeof(value);
		state.x[16] = 105;
		require_condition(syscall_range_100_199_dispatch(&state) == 1,
		                  "range 100 did not handle buffer setsockopt");
		require_success(&state, "Darwin buffer setsockopt failed");

		memset(&state, 0, sizeof(state));
		state.x[0] = sockets[0];
		state.x[1] = 0xffff;
		state.x[2] = option_numbers[index];
		state.x[3] = (uint64_t)&expected_value;
		state.x[4] = (uint64_t)&value_len;
		state.x[16] = 118;
		require_condition(syscall_range_100_199_dispatch(&state) == 1,
		                  "range 100 did not handle buffer getsockopt");
		require_success(&state, "Darwin buffer getsockopt failed");
		require_condition(getsockopt(sockets[0], SOL_SOCKET, linux_options[index],
		                            &linux_value, &linux_len) == 0,
		                  "host buffer getsockopt failed");
		require_condition(expected_value == linux_value,
		                  "Darwin buffer value did not match host value");
	}
	close_socket_pair(sockets);
}

static void test_so_error_translation(void)
{
	struct machgate_syscall_frame state = {0};
	struct sockaddr_in address;
	int descriptor;
	int value = -1;
	socklen_t value_len = sizeof(value);

	descriptor = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	require_condition(descriptor >= 0, "nonblocking TCP socket failed");
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_port = htons(9);
	address.sin_addr.s_addr = htonl(0x7f000001);
	require_condition(connect(descriptor, (struct sockaddr*)&address,
	                         sizeof(address)) < 0,
	                  "nonblocking connection unexpectedly succeeded");
	state.x[0] = descriptor;
	state.x[1] = 0xffff;
	state.x[2] = 0x1007;
	state.x[3] = (uint64_t)&value;
	state.x[4] = (uint64_t)&value_len;
	state.x[16] = 118;
	require_condition(syscall_range_100_199_dispatch(&state) == 1,
	                  "range 100 did not handle SO_ERROR");
	require_success(&state, "SO_ERROR getsockopt failed");
	require_condition(value == 36 || value == 61 || value == 49,
	                  "SO_ERROR did not return a translated Darwin socket errno");
	close(descriptor);
}

int main(void)
{
	signal(SIGPIPE, SIG_IGN);
	test_nonblocking_recv(29, 0);
	test_nonblocking_recv(403, 400);
	test_non_socket_not_supported();
	test_raw_darwin_enotsup_is_not_retranslated();
	test_socket_operation_not_supported();
	test_send_nosignal(28, 0);
	test_send_nosignal(133, 100);
	test_send_nosignal(402, 400);
	test_send_nosignal(413, 400);
	test_scm_rights(28, 0);
	test_scm_rights(402, 400);
	test_socket_option(105, 118);
	test_socket_buffer_options();
	test_so_error_translation();
	puts("darwin-raw-socket-dispatch-ok");
	return 0;
}
