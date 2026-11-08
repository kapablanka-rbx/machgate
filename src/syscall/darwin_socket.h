#ifndef MACHGATE_SYSCALL_DARWIN_SOCKET_H
#define MACHGATE_SYSCALL_DARWIN_SOCKET_H

#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>

#define DARWIN_AF_UNIX   1
#define DARWIN_AF_INET   2
#define DARWIN_AF_INET6 30

#define DARWIN_SOCK_CLOEXEC  0x10000000
#define DARWIN_SOCK_NONBLOCK 0x20000000

struct darwin_msghdr {
	void* msg_name;
	uint32_t msg_namelen;
	struct iovec* msg_iov;
	int32_t msg_iovlen;
	void* msg_control;
	uint32_t msg_controllen;
	int32_t msg_flags;
};

struct darwin_msghdr_x {
	struct darwin_msghdr header;
	uint64_t msg_datalen;
};

struct darwin_cmsghdr {
	uint32_t cmsg_len;
	int32_t cmsg_level;
	int32_t cmsg_type;
};

_Static_assert(sizeof(struct darwin_msghdr) == 48,
	"darwin_msghdr must be 48 bytes");
_Static_assert(sizeof(struct darwin_msghdr_x) == 56,
	"darwin_msghdr_x must be 56 bytes");
_Static_assert(sizeof(struct darwin_cmsghdr) == 12,
	"darwin_cmsghdr must be 12 bytes");

int machgate_darwin_socket_errno_from_linux(int linux_errno);
int machgate_darwin_send_flags_to_linux(int darwin_flags);
int machgate_darwin_recv_flags_to_linux(int darwin_flags);
int machgate_darwin_recv_flags_from_linux(int linux_flags);
int machgate_darwin_socket_domain_to_linux(int darwin_domain,
                                           int* linux_domain);
int machgate_darwin_socket_type_to_linux(int darwin_type);
int machgate_darwin_sockaddr_to_linux(const void* darwin_addr,
                                      uint64_t darwin_len,
                                      struct sockaddr_storage* linux_addr,
                                      socklen_t* linux_len);
int machgate_linux_sockaddr_to_darwin(const struct sockaddr_storage* linux_addr,
                                      socklen_t linux_len, void* darwin_addr,
                                      socklen_t* darwin_len);
int machgate_darwin_socket_option_to_linux(int darwin_level, int darwin_option,
                                           int* linux_level, int* linux_option);
int machgate_darwin_socket_option_value_to_linux(int darwin_level,
                                                 int darwin_option,
                                                 const void* darwin_value,
                                                 uint32_t darwin_value_len,
                                                 void* linux_value,
                                                 socklen_t* linux_value_len);
int machgate_darwin_socket_option_value_from_linux(int darwin_level,
                                                   int darwin_option,
                                                   void* darwin_value,
                                                   socklen_t* darwin_value_len);
int machgate_darwin_socket_option_value_is_noop(int darwin_level,
                                                int darwin_option);

size_t machgate_darwin_cmsgs_to_linux(const void* darwin_control,
                                      size_t darwin_length,
                                      void* linux_control,
                                      size_t linux_capacity,
                                      size_t* required_capacity);
size_t machgate_linux_cmsgs_to_darwin(const void* linux_control,
                                      size_t linux_length,
                                      void* darwin_control,
                                      size_t darwin_capacity,
                                      size_t* required_capacity);

long machgate_darwin_connect(int fd, const void* address, size_t address_length);
long machgate_darwin_connect_translated(int fd,
                         const struct sockaddr_storage* address, socklen_t length);

long machgate_darwin_sendmsg(int fd, const struct darwin_msghdr* darwin_msg,
                             int darwin_flags,
                             const struct sockaddr* linux_name,
                             socklen_t linux_name_len);
long machgate_darwin_recvmsg(int fd, struct darwin_msghdr* darwin_msg,
                             int darwin_flags,
                             struct sockaddr* linux_name,
                             socklen_t* linux_name_len);

#endif
