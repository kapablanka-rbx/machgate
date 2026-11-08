#include "syscall/darwin_socket.h"
#include "ports/socket_state.h"

#include <arpa/inet.h>
#include <dlfcn.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>

#define DARWIN_EFAULT 14
#define DARWIN_EINVAL 22
#define DARWIN_ENOPROTOOPT 42
#define DARWIN_ENOSYS 78
#define DARWIN_EAFNOSUPPORT 47
#define DARWIN_SOL_SOCKET 0xffff
#define DARWIN_SO_NOSIGPIPE 0x1022

static long socket_result(long result)
{
	if (result < 0)
		errno = machgate_darwin_socket_errno_from_linux(errno ? errno : EIO);
	return result;
}

static long socket_call(long syscall_number, unsigned long argument1,
                        unsigned long argument2, unsigned long argument3,
                        unsigned long argument4, unsigned long argument5,
                        unsigned long argument6)
{
	errno = 0;
	long result = syscall(syscall_number, argument1, argument2, argument3,
	                      argument4, argument5, argument6);
	return socket_result(result);
}

int socket(int domain, int type, int protocol)
{
	int linux_domain;

	if (!machgate_darwin_socket_domain_to_linux(domain, &linux_domain)) {
		errno = DARWIN_EAFNOSUPPORT;
		return -1;
	}
	return (int)socket_call(SYS_socket, linux_domain,
	                        machgate_darwin_socket_type_to_linux(type),
	                        protocol, 0, 0, 0);
}

int bind(int fd, const struct sockaddr* address, socklen_t address_len)
{
	struct sockaddr_storage linux_address;
	socklen_t linux_len;

	if (!machgate_darwin_sockaddr_to_linux(address, address_len,
	                                      &linux_address, &linux_len))
		return (int)socket_result(-1);
	return (int)socket_call(SYS_bind, fd, (unsigned long)&linux_address,
	                        linux_len, 0, 0, 0);
}

int connect(int fd, const struct sockaddr* address, socklen_t address_len)
{
	return (int)socket_result(machgate_darwin_connect(fd, address, address_len));
}

int listen(int fd, int backlog)
{
	return (int)socket_call(SYS_listen, fd, backlog, 0, 0, 0, 0);
}

int socketpair(int domain, int type, int protocol, int descriptors[2])
{
	int linux_domain;

	if (!machgate_darwin_socket_domain_to_linux(domain, &linux_domain)) {
		errno = DARWIN_EAFNOSUPPORT;
		return -1;
	}
	return (int)socket_call(SYS_socketpair, linux_domain,
	                        machgate_darwin_socket_type_to_linux(type),
	                        protocol, (unsigned long)descriptors, 0, 0);
}

int accept(int fd, struct sockaddr* address, socklen_t* address_len)
{
	struct sockaddr_storage linux_address;
	socklen_t linux_len = sizeof(linux_address);
	long result = socket_call(SYS_accept, fd,
	                          address ? (unsigned long)&linux_address : 0,
	                          address ? (unsigned long)&linux_len : 0,
	                          0, 0, 0);

	if (result >= 0 && machgate_socket_state_accept(fd, (int)result) < 0) {
		int saved_errno = errno;
		syscall(SYS_close, result);
		errno = saved_errno;
		return (int)socket_result(-1);
	}
	if (result < 0 || !address || !address_len)
		return (int)result;
	if (!machgate_linux_sockaddr_to_darwin(&linux_address, linux_len,
	                                      address, address_len))
		return (int)socket_result(-1);
	return (int)result;
}

static int socket_name(int fd, struct sockaddr* address,
                       socklen_t* address_len, int peer)
{
	struct sockaddr_storage linux_address;
	socklen_t linux_len = sizeof(linux_address);
	long result = socket_call(peer ? SYS_getpeername : SYS_getsockname, fd,
	                          (unsigned long)&linux_address,
	                          (unsigned long)&linux_len, 0, 0, 0);

	if (result < 0 || !address || !address_len)
		return (int)result;
	if (!machgate_linux_sockaddr_to_darwin(&linux_address, linux_len,
	                                      address, address_len))
		return (int)socket_result(-1);
	return 0;
}

int getsockname(int fd, struct sockaddr* address, socklen_t* address_len)
{
	return socket_name(fd, address, address_len, 0);
}

int getpeername(int fd, struct sockaddr* address, socklen_t* address_len)
{
	return socket_name(fd, address, address_len, 1);
}

ssize_t send(int fd, const void* buffer, size_t length, int flags)
{
	return (ssize_t)socket_call(SYS_sendto, fd, (unsigned long)buffer, length,
	                            machgate_socket_send_flags(fd, machgate_darwin_send_flags_to_linux(flags)),
	                            0, 0);
}

ssize_t recv(int fd, void* buffer, size_t length, int flags)
{
	return (ssize_t)socket_call(SYS_recvfrom, fd, (unsigned long)buffer, length,
	                            machgate_darwin_recv_flags_to_linux(flags),
	                            0, 0);
}

ssize_t sendto(int fd, const void* buffer, size_t length, int flags,
               const struct sockaddr* address, socklen_t address_len)
{
	struct sockaddr_storage linux_address;
	socklen_t linux_len = 0;
	const struct sockaddr* linux_address_ptr = NULL;

	if (address) {
		if (!machgate_darwin_sockaddr_to_linux(address, address_len,
		                                      &linux_address, &linux_len))
			return (ssize_t)socket_result(-1);
		linux_address_ptr = (const struct sockaddr*)&linux_address;
	}
	return (ssize_t)socket_call(SYS_sendto, fd, (unsigned long)buffer, length,
	                            machgate_socket_send_flags(fd, machgate_darwin_send_flags_to_linux(flags)),
	                            (unsigned long)linux_address_ptr, linux_len);
}

ssize_t recvfrom(int fd, void* buffer, size_t length, int flags,
                 struct sockaddr* address, socklen_t* address_len)
{
	struct sockaddr_storage linux_address;
	socklen_t linux_len = sizeof(linux_address);
	long result = socket_call(SYS_recvfrom, fd, (unsigned long)buffer, length,
	                          machgate_darwin_recv_flags_to_linux(flags),
	                          address ? (unsigned long)&linux_address : 0,
	                          address ? (unsigned long)&linux_len : 0);

	if (result < 0 || !address || !address_len)
		return (ssize_t)result;
	if (!machgate_linux_sockaddr_to_darwin(&linux_address, linux_len,
	                                      address, address_len))
		return (ssize_t)socket_result(-1);
	return (ssize_t)result;
}

ssize_t sendmsg(int fd, const struct msghdr* message, int flags)
{
	struct sockaddr_storage linux_name;
	socklen_t linux_name_len = 0;
	const struct darwin_msghdr* darwin_message =
		(const struct darwin_msghdr*)message;
	const struct sockaddr* linux_name_ptr = NULL;

	if (!message) {
		errno = DARWIN_EFAULT;
		return -1;
	}
	if (darwin_message->msg_name) {
		if (!machgate_darwin_sockaddr_to_linux(
			    darwin_message->msg_name, darwin_message->msg_namelen,
			    &linux_name, &linux_name_len))
			return (ssize_t)socket_result(-1);
		linux_name_ptr = (const struct sockaddr*)&linux_name;
	}
	return (ssize_t)socket_result(machgate_darwin_sendmsg(
		fd, darwin_message, flags, linux_name_ptr, linux_name_len));
}

ssize_t recvmsg(int fd, struct msghdr* message, int flags)
{
	struct darwin_msghdr* darwin_message = (struct darwin_msghdr*)message;
	struct sockaddr_storage linux_name;
	socklen_t linux_name_len = sizeof(linux_name);
	long result;

	if (!message) {
		errno = DARWIN_EFAULT;
		return -1;
	}
	result = machgate_darwin_recvmsg(fd, darwin_message, flags,
	                                (struct sockaddr*)&linux_name,
	                                &linux_name_len);
	if (result < 0)
		return (ssize_t)socket_result(result);
	if (darwin_message->msg_name &&
	    !machgate_linux_sockaddr_to_darwin(
		    &linux_name, linux_name_len, darwin_message->msg_name,
		    &darwin_message->msg_namelen))
		return (ssize_t)socket_result(-1);
	return (ssize_t)result;
}

int setsockopt(int fd, int level, int option, const void* value,
               socklen_t value_len)
{
	int linux_level;
	int linux_option;
	unsigned char value_buffer[256];
	socklen_t linux_value_len = sizeof(value_buffer);

	if (level == DARWIN_SOL_SOCKET && option == DARWIN_SO_NOSIGPIPE) {
		if (!value || value_len < sizeof(int)) {
			errno = value ? DARWIN_EINVAL : DARWIN_EFAULT;
			return -1;
		}
		int enabled;
		memcpy(&enabled, value, sizeof(enabled));
		return (int)socket_result(machgate_socket_nosigpipe_set(fd, enabled));
	}
	if (!machgate_darwin_socket_option_to_linux(level, option,
	                                           &linux_level, &linux_option)) {
		errno = DARWIN_ENOPROTOOPT;
		return -1;
	}
	if (value_len > sizeof(value_buffer)) {
		errno = DARWIN_EINVAL;
		return -1;
	}
	if (!machgate_darwin_socket_option_value_to_linux(
		    level, option, value, value_len, value_buffer, &linux_value_len))
		return (int)socket_result(-1);
	return (int)socket_call(SYS_setsockopt, fd, linux_level, linux_option,
	                        (unsigned long)value_buffer, linux_value_len, 0);
}

int getsockopt(int fd, int level, int option, void* value,
               socklen_t* value_len)
{
	if (level == DARWIN_SOL_SOCKET && option == DARWIN_SO_NOSIGPIPE) {
		if (!value_len || (!value && *value_len)) {
			errno = DARWIN_EFAULT;
			return -1;
		}
		int result = machgate_socket_nosigpipe_get(fd);
		if (result < 0)
			return (int)socket_result(result);
		socklen_t size = *value_len < sizeof(result) ? *value_len : sizeof(result);
		if (size)
			memcpy(value, &result, size);
		*value_len = size;
		return 0;
	}

	int linux_level;
	int linux_option;
	unsigned char value_buffer[256];
	socklen_t linux_value_len = sizeof(value_buffer);
	long result;

	if (!machgate_darwin_socket_option_to_linux(level, option,
	                                           &linux_level, &linux_option)) {
		errno = DARWIN_ENOPROTOOPT;
		return -1;
	}
	if (value && value_len && *value_len < linux_value_len)
		linux_value_len = *value_len;
	result = socket_call(SYS_getsockopt, fd, linux_level, linux_option,
	                     (unsigned long)value_buffer,
	                     (unsigned long)&linux_value_len, 0);
	if (result < 0)
		return (int)result;
	if (value && linux_value_len > sizeof(value_buffer)) {
		errno = DARWIN_EINVAL;
		return -1;
	}
	if (value && linux_value_len)
		memcpy(value, value_buffer, linux_value_len);
	if (value_len)
		*value_len = linux_value_len;
	if (value && !machgate_darwin_socket_option_value_from_linux(
			level, option, value, value_len))
		return (int)socket_result(-1);
	return 0;
}

int inet_pton(int family, const char* source, void* destination)
{
	int linux_family;
	static int (*host_inet_pton)(int, const char*, void*);
	int result;

	if (!machgate_darwin_socket_domain_to_linux(family, &linux_family)) {
		errno = DARWIN_EAFNOSUPPORT;
		return -1;
	}
	if (!host_inet_pton)
		host_inet_pton = (int (*)(int, const char*, void*))dlsym(
			RTLD_NEXT, "inet_pton");
	if (!host_inet_pton) {
		errno = DARWIN_ENOSYS;
		return -1;
	}
	result = host_inet_pton(linux_family, source, destination);
	if (result < 0)
		errno = machgate_darwin_socket_errno_from_linux(errno ? errno : EIO);
	return result;
}

const char* inet_ntop(int family, const void* source, char* destination,
                      socklen_t destination_len)
{
	int linux_family;
	static const char* (*host_inet_ntop)(int, const void*, char*, socklen_t);
	const char* result;

	if (!machgate_darwin_socket_domain_to_linux(family, &linux_family)) {
		errno = DARWIN_EAFNOSUPPORT;
		return NULL;
	}
	if (!host_inet_ntop)
		host_inet_ntop = (const char* (*)(int, const void*, char*, socklen_t))
			dlsym(RTLD_NEXT, "inet_ntop");
	if (!host_inet_ntop) {
		errno = DARWIN_ENOSYS;
		return NULL;
	}
	result = host_inet_ntop(linux_family, source, destination, destination_len);
	if (!result)
		errno = machgate_darwin_socket_errno_from_linux(errno ? errno : EIO);
	return result;
}

ssize_t recvmsg_x(int fd, void* messages, unsigned int count, int flags)
{
	(void)fd;
	(void)messages;
	(void)count;
	(void)flags;
	errno = DARWIN_ENOSYS;
	return -1;
}

ssize_t sendmsg_x(int fd, void* messages, unsigned int count, int flags)
{
	(void)fd;
	(void)messages;
	(void)count;
	(void)flags;
	errno = DARWIN_ENOSYS;
	return -1;
}
