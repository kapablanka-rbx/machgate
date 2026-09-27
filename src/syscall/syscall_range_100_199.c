#include "syscall_range_100_199.h"
#include "guest_vm_dispatch.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stddef.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/timex.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define SYSCALL_GATE_CARRY 0x20000000u

#define DARWIN_ENOSYS 78

#define DARWIN_EPERM 1
#define DARWIN_ENOENT 2
#define DARWIN_ESRCH 3
#define DARWIN_EINTR 4
#define DARWIN_EIO 5
#define DARWIN_ENXIO 6
#define DARWIN_E2BIG 7
#define DARWIN_ENOEXEC 8
#define DARWIN_EBADF 9
#define DARWIN_ECHILD 10
#define DARWIN_EDEADLK 11
#define DARWIN_ENOMEM 12
#define DARWIN_EACCES 13
#define DARWIN_EFAULT 14
#define DARWIN_EBUSY 16
#define DARWIN_EEXIST 17
#define DARWIN_EXDEV 18
#define DARWIN_ENODEV 19
#define DARWIN_ENOTDIR 20
#define DARWIN_EISDIR 21
#define DARWIN_EINVAL 22
#define DARWIN_ENFILE 23
#define DARWIN_EMFILE 24
#define DARWIN_ENOTTY 25
#define DARWIN_EFBIG 27
#define DARWIN_ENOSPC 28
#define DARWIN_ESPIPE 29
#define DARWIN_EROFS 30
#define DARWIN_EMLINK 31
#define DARWIN_EPIPE 32
#define DARWIN_EDOM 33
#define DARWIN_ERANGE 34
#define DARWIN_EAGAIN 35
#define DARWIN_EINPROGRESS 36
#define DARWIN_EALREADY 37
#define DARWIN_ENOTSOCK 38
#define DARWIN_EDESTADDRREQ 39
#define DARWIN_EMSGSIZE 40
#define DARWIN_EPROTOTYPE 41
#define DARWIN_ENOPROTOOPT 42
#define DARWIN_EPROTONOSUPPORT 43
#define DARWIN_ENOTSUP 45
#define DARWIN_EAFNOSUPPORT 47
#define DARWIN_EADDRINUSE 48
#define DARWIN_EADDRNOTAVAIL 49
#define DARWIN_ENETDOWN 50
#define DARWIN_ENETUNREACH 51
#define DARWIN_ENETRESET 52
#define DARWIN_ECONNABORTED 53
#define DARWIN_ECONNRESET 54
#define DARWIN_ENOBUFS 55
#define DARWIN_EISCONN 56
#define DARWIN_ENOTCONN 57
#define DARWIN_ESHUTDOWN 58
#define DARWIN_ETIMEDOUT 60
#define DARWIN_ECONNREFUSED 61
#define DARWIN_ELOOP 62
#define DARWIN_ENAMETOOLONG 63
#define DARWIN_EHOSTUNREACH 65
#define DARWIN_ENOTEMPTY 66
#define DARWIN_EDQUOT 69
#define DARWIN_ESTALE 70
#define DARWIN_ENOLCK 77
#define DARWIN_EOVERFLOW 84
#define DARWIN_ECANCELED 89
#define DARWIN_EILSEQ 92

#define DARWIN_MAP_SHARED  0x0001
#define DARWIN_MAP_PRIVATE 0x0002
#define DARWIN_MAP_FIXED   0x0010
#define DARWIN_MAP_ANON    0x1000

#define DARWIN_SYS_settimeofday 122
#define DARWIN_SYS_adjtime 140
#define DARWIN_SYS_sigsuspend 111
#define DARWIN_SYS_nfssvc 155
#define DARWIN_SYS_unmount 159
#define DARWIN_SYS_getfh 161
#define DARWIN_SYS_funmount 164
#define DARWIN_SYS_quotactl 165
#define DARWIN_SYS_mount 167
#define DARWIN_SYS_csops 169
#define DARWIN_SYS_csops_audittoken 170
#define DARWIN_SYS_waitid 173
#define DARWIN_SYS_sigreturn 184
#define DARWIN_SYS_sys_panic_with_data 185
#define DARWIN_SYS_thread_selfcounts 186

#define DARWIN_CS_OPS_STATUS 0

#define DARWIN_RLIMIT_CPU     0
#define DARWIN_RLIMIT_FSIZE   1
#define DARWIN_RLIMIT_DATA    2
#define DARWIN_RLIMIT_STACK   3
#define DARWIN_RLIMIT_CORE    4
#define DARWIN_RLIMIT_AS      5
#define DARWIN_RLIMIT_MEMLOCK 6
#define DARWIN_RLIMIT_NPROC   7
#define DARWIN_RLIMIT_NOFILE  8

#define DARWIN_PC_LINK_MAX            1
#define DARWIN_PC_MAX_CANON           2
#define DARWIN_PC_MAX_INPUT           3
#define DARWIN_PC_NAME_MAX            4
#define DARWIN_PC_PATH_MAX            5
#define DARWIN_PC_PIPE_BUF            6
#define DARWIN_PC_CHOWN_RESTRICTED    7
#define DARWIN_PC_NO_TRUNC            8
#define DARWIN_PC_VDISABLE            9
#define DARWIN_PC_NAME_CHARS_MAX      10
#define DARWIN_PC_CASE_SENSITIVE      11
#define DARWIN_PC_CASE_PRESERVING     12
#define DARWIN_PC_2_SYMLINKS          15
#define DARWIN_PC_ALLOC_SIZE_MIN      16
#define DARWIN_PC_ASYNC_IO            17
#define DARWIN_PC_FILESIZEBITS        18
#define DARWIN_PC_PRIO_IO             19
#define DARWIN_PC_REC_INCR_XFER_SIZE  20
#define DARWIN_PC_REC_MAX_XFER_SIZE   21
#define DARWIN_PC_REC_MIN_XFER_SIZE   22
#define DARWIN_PC_REC_XFER_ALIGN      23
#define DARWIN_PC_SYMLINK_MAX         24
#define DARWIN_PC_SYNC_IO             25

#define DARWIN_AF_UNIX   1
#define DARWIN_AF_INET   2
#define DARWIN_AF_INET6 30

#define DARWIN_THSC_CPI 1
#define DARWIN_THSC_CPI_PER_PERF_LEVEL 2
#define DARWIN_THSC_TIME_CPI 3
#define DARWIN_THSC_TIME_CPI_PER_PERF_LEVEL 4
#define DARWIN_THSC_TIME_ENERGY_CPI 5
#define DARWIN_THSC_TIME_ENERGY_CPI_PER_PERF_LEVEL 6

#define DARWIN_SOL_SOCKET 0xffff
#define DARWIN_IPPROTO_TCP 6
#define DARWIN_TCP_KEEPALIVE 0x10
#define DARWIN_TCP_KEEPINTVL 0x101
#define DARWIN_TCP_KEEPCNT 0x102
#define DARWIN_SO_DEBUG      0x0001
#define DARWIN_SO_ACCEPTCONN 0x0002
#define DARWIN_SO_REUSEADDR  0x0004
#define DARWIN_SO_KEEPALIVE  0x0008
#define DARWIN_SO_DONTROUTE  0x0010
#define DARWIN_SO_BROADCAST  0x0020
#define DARWIN_SO_LINGER     0x0080
#define DARWIN_SO_OOBINLINE  0x0100
#define DARWIN_SO_REUSEPORT  0x0200
#define DARWIN_SO_TYPE       0x1008
#define DARWIN_SO_ERROR      0x1007

#ifndef LINK_MAX
#define LINK_MAX 127
#endif

#ifndef NAME_MAX
#define NAME_MAX 255
#endif

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#ifndef PIPE_BUF
#define PIPE_BUF 512
#endif

struct darwin_stat {
	int32_t         st_dev;
	uint16_t        st_mode;
	uint16_t        st_nlink;
	uint64_t        st_ino;
	uint32_t        st_uid;
	uint32_t        st_gid;
	int32_t         st_rdev;
	int32_t         __pad0;
	struct timespec st_atimespec;
	struct timespec st_mtimespec;
	struct timespec st_ctimespec;
	struct timespec st_birthtimespec;
	int64_t         st_size;
	int64_t         st_blocks;
	int32_t         st_blksize;
	uint32_t        st_flags;
	uint32_t        st_gen;
	int32_t         st_lspare;
	int64_t         st_qspare[2];
};

_Static_assert(sizeof(struct darwin_stat) == 144,
	"darwin_stat must be 144 bytes");

struct darwin_statfs {
	uint32_t f_bsize;
	int32_t  f_iosize;
	uint64_t f_blocks;
	uint64_t f_bfree;
	uint64_t f_bavail;
	uint64_t f_files;
	uint64_t f_ffree;
	int32_t  f_fsid[2];
	uint32_t f_owner;
	uint32_t f_type;
	uint32_t f_flags;
	uint32_t f_fssubtype;
	char     f_fstypename[16];
	char     f_mntonname[1024];
	char     f_mntfromname[1024];
	uint32_t f_flags_ext;
	uint32_t f_reserved[7];
};

_Static_assert(sizeof(struct darwin_statfs) == 2168,
	"darwin_statfs must be 2168 bytes");

struct darwin_timeval {
	int64_t tv_sec;
	int32_t tv_usec;
	int32_t __pad;
};

_Static_assert(sizeof(struct darwin_timeval) == 16,
	"darwin_timeval must be 16 bytes");

struct linux_dirent64_local {
	uint64_t d_ino;
	int64_t  d_off;
	uint16_t d_reclen;
	uint8_t  d_type;
	char     d_name[];
};

struct darwin_dirent {
	uint64_t d_ino;
	uint64_t d_seekoff;
	uint16_t d_reclen;
	uint16_t d_namlen;
	uint8_t  d_type;
	char     d_name[];
};

struct darwin_thsc_cpi {
	uint64_t instructions;
	uint64_t cycles;
};

struct darwin_thsc_time_cpi {
	uint64_t instructions;
	uint64_t cycles;
	uint64_t user_time_mach;
	uint64_t system_time_mach;
};

struct darwin_thsc_time_energy_cpi {
	uint64_t instructions;
	uint64_t cycles;
	uint64_t user_time_mach;
	uint64_t system_time_mach;
	uint64_t energy_nj;
};

static void set_success(struct syscall_gate_state* state, uint64_t result)
{
	state->x[0] = result;
	state->nzcv &= ~SYSCALL_GATE_CARRY;
}

static void set_failure(struct syscall_gate_state* state, int err)
{
	state->x[0] = (uint64_t)err;
	state->nzcv |= SYSCALL_GATE_CARRY;
}

static int darwin_errno_from_linux(int err)
{
	switch (err) {
	case 0: return 0;
	case EPERM: return DARWIN_EPERM;
	case ENOENT: return DARWIN_ENOENT;
	case ESRCH: return DARWIN_ESRCH;
	case EINTR: return DARWIN_EINTR;
	case EIO: return DARWIN_EIO;
	case ENXIO: return DARWIN_ENXIO;
	case E2BIG: return DARWIN_E2BIG;
	case ENOEXEC: return DARWIN_ENOEXEC;
	case EBADF: return DARWIN_EBADF;
	case ECHILD: return DARWIN_ECHILD;
	case EDEADLK: return DARWIN_EDEADLK;
	case ENOMEM: return DARWIN_ENOMEM;
	case EACCES: return DARWIN_EACCES;
	case EFAULT: return DARWIN_EFAULT;
	case EBUSY: return DARWIN_EBUSY;
	case EEXIST: return DARWIN_EEXIST;
	case EXDEV: return DARWIN_EXDEV;
	case ENODEV: return DARWIN_ENODEV;
	case ENOTDIR: return DARWIN_ENOTDIR;
	case EISDIR: return DARWIN_EISDIR;
	case EINVAL: return DARWIN_EINVAL;
	case ENFILE: return DARWIN_ENFILE;
	case EMFILE: return DARWIN_EMFILE;
	case ENOTTY: return DARWIN_ENOTTY;
	case EFBIG: return DARWIN_EFBIG;
	case ENOSPC: return DARWIN_ENOSPC;
	case ESPIPE: return DARWIN_ESPIPE;
	case EROFS: return DARWIN_EROFS;
	case EMLINK: return DARWIN_EMLINK;
	case EPIPE: return DARWIN_EPIPE;
	case EDOM: return DARWIN_EDOM;
	case ERANGE: return DARWIN_ERANGE;
	case EAGAIN: return DARWIN_EAGAIN;
	case EINPROGRESS: return DARWIN_EINPROGRESS;
	case EALREADY: return DARWIN_EALREADY;
	case ENOTSOCK: return DARWIN_ENOTSOCK;
	case EDESTADDRREQ: return DARWIN_EDESTADDRREQ;
	case EMSGSIZE: return DARWIN_EMSGSIZE;
	case EPROTOTYPE: return DARWIN_EPROTOTYPE;
	case ENOPROTOOPT: return DARWIN_ENOPROTOOPT;
	case EPROTONOSUPPORT: return DARWIN_EPROTONOSUPPORT;
	case ENOTSUP: return DARWIN_ENOTSUP;
	case EAFNOSUPPORT: return DARWIN_EAFNOSUPPORT;
	case EADDRINUSE: return DARWIN_EADDRINUSE;
	case EADDRNOTAVAIL: return DARWIN_EADDRNOTAVAIL;
	case ENETDOWN: return DARWIN_ENETDOWN;
	case ENETUNREACH: return DARWIN_ENETUNREACH;
	case ENETRESET: return DARWIN_ENETRESET;
	case ECONNABORTED: return DARWIN_ECONNABORTED;
	case ECONNRESET: return DARWIN_ECONNRESET;
	case ENOBUFS: return DARWIN_ENOBUFS;
	case EISCONN: return DARWIN_EISCONN;
	case ENOTCONN: return DARWIN_ENOTCONN;
	case ESHUTDOWN: return DARWIN_ESHUTDOWN;
	case ETIMEDOUT: return DARWIN_ETIMEDOUT;
	case ECONNREFUSED: return DARWIN_ECONNREFUSED;
	case ELOOP: return DARWIN_ELOOP;
	case ENAMETOOLONG: return DARWIN_ENAMETOOLONG;
	case EHOSTUNREACH: return DARWIN_EHOSTUNREACH;
	case ENOTEMPTY: return DARWIN_ENOTEMPTY;
	case EDQUOT: return DARWIN_EDQUOT;
	case ESTALE: return DARWIN_ESTALE;
	case ENOLCK: return DARWIN_ENOLCK;
	case ENOSYS: return DARWIN_ENOSYS;
	case EOVERFLOW: return DARWIN_EOVERFLOW;
	case ECANCELED: return DARWIN_ECANCELED;
	case EILSEQ: return DARWIN_EILSEQ;
	default: return DARWIN_EINVAL;
	}
}

static void finish_syscall_result(struct syscall_gate_state* state, long result)
{
	if (result < 0)
		set_failure(state, darwin_errno_from_linux(errno ? errno : EIO));
	else
		set_success(state, (uint64_t)result);
}

static void set_enosys(struct syscall_gate_state* state)
{
	set_failure(state, DARWIN_ENOSYS);
}

static uint64_t current_thread_cpu_time_mach(void)
{
	struct timespec thread_time;
	memset(&thread_time, 0, sizeof(thread_time));

	errno = 0;
	long result = syscall(SYS_clock_gettime, CLOCK_THREAD_CPUTIME_ID,
	                      &thread_time);
	if (result < 0)
		return 0;

	return (uint64_t)thread_time.tv_sec * 1000000000ull +
	       (uint64_t)thread_time.tv_nsec;
}

static void copy_thread_selfcounts(struct syscall_gate_state* state,
                                   const void* counts, size_t counts_size)
{
	void* buffer = (void*)state->x[1];
	size_t buffer_size = (size_t)state->x[2];
	size_t copy_size = counts_size;

	if (copy_size > buffer_size)
		copy_size = buffer_size;
	if (copy_size && !buffer) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	if (copy_size)
		memcpy(buffer, counts, copy_size);
	set_success(state, 0);
}

static void handle_thread_selfcounts(struct syscall_gate_state* state)
{
	uint32_t kind = (uint32_t)state->x[0];
	uint64_t thread_time = current_thread_cpu_time_mach();

	switch (kind) {
	case DARWIN_THSC_CPI:
	case DARWIN_THSC_CPI_PER_PERF_LEVEL: {
		struct darwin_thsc_cpi counts = { 0 };
		copy_thread_selfcounts(state, &counts, sizeof(counts));
		return;
	}
	case DARWIN_THSC_TIME_CPI:
	case DARWIN_THSC_TIME_CPI_PER_PERF_LEVEL: {
		struct darwin_thsc_time_cpi counts = { 0 };
		counts.user_time_mach = thread_time;
		copy_thread_selfcounts(state, &counts, sizeof(counts));
		return;
	}
	case DARWIN_THSC_TIME_ENERGY_CPI:
	case DARWIN_THSC_TIME_ENERGY_CPI_PER_PERF_LEVEL: {
		struct darwin_thsc_time_energy_cpi counts = { 0 };
		counts.user_time_mach = thread_time;
		copy_thread_selfcounts(state, &counts, sizeof(counts));
		return;
	}
	default:
		set_failure(state, DARWIN_ENOTSUP);
		return;
	}
}

static void handle_gethostuuid(struct syscall_gate_state* state)
{
	static const uint8_t machgate_host_uuid[16] = {
		0x4d, 0x47, 0x41, 0x54, 0x45, 0x00, 0x40, 0x00,
		0x80, 0x00, 0x6d, 0x61, 0x63, 0x68, 0x67, 0x74
	};
	uint8_t* uuid_buf = (uint8_t*)state->x[0];

	if (!uuid_buf) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}
	memcpy(uuid_buf, machgate_host_uuid, sizeof(machgate_host_uuid));
	set_success(state, 0);
}

static size_t align8(size_t value)
{
	return (value + 7u) & ~7u;
}

static int translate_mmap_flags(int darwin_flags)
{
	int linux_flags = 0;

	if (darwin_flags & DARWIN_MAP_SHARED) linux_flags |= MAP_SHARED;
	if (darwin_flags & DARWIN_MAP_PRIVATE) linux_flags |= MAP_PRIVATE;
	if (darwin_flags & DARWIN_MAP_FIXED) linux_flags |= MAP_FIXED;
	if (darwin_flags & DARWIN_MAP_ANON) linux_flags |= MAP_ANONYMOUS;

	return linux_flags;
}

static int translate_socket_domain(int darwin_domain, int* linux_domain)
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

#define DARWIN_MSG_OOB_100       0x0001
#define DARWIN_MSG_DONTROUTE_100 0x0004
#define DARWIN_MSG_DONTWAIT_100  0x0080
#define DARWIN_MSG_NOSIGNAL_100  0x80000

static int translate_send_flags_100(int darwin_flags)
{
	int linux_flags = 0;

	if (darwin_flags & DARWIN_MSG_OOB_100)
		linux_flags |= MSG_OOB;
	if (darwin_flags & DARWIN_MSG_DONTROUTE_100)
		linux_flags |= MSG_DONTROUTE;
	if (darwin_flags & DARWIN_MSG_DONTWAIT_100)
		linux_flags |= MSG_DONTWAIT;
	if (darwin_flags & DARWIN_MSG_NOSIGNAL_100)
		linux_flags |= MSG_NOSIGNAL;
	return linux_flags;
}

static int translate_socket_option(int darwin_level, int darwin_option,
                                   int* linux_level, int* linux_option)
{
	if (darwin_level == DARWIN_IPPROTO_TCP) {
		*linux_level = IPPROTO_TCP;
		switch (darwin_option) {
		case DARWIN_TCP_KEEPALIVE:
			*linux_option = TCP_KEEPIDLE;
			return 1;
		case DARWIN_TCP_KEEPINTVL:
			*linux_option = TCP_KEEPINTVL;
			return 1;
		case DARWIN_TCP_KEEPCNT:
			*linux_option = TCP_KEEPCNT;
			return 1;
		default:
			*linux_option = darwin_option;
			return 1;
		}
	}

	if (darwin_level != DARWIN_SOL_SOCKET) {
		*linux_level = darwin_level;
		*linux_option = darwin_option;
		return 1;
	}

	*linux_level = SOL_SOCKET;
	switch (darwin_option) {
	case DARWIN_SO_DEBUG:
		*linux_option = SO_DEBUG;
		return 1;
	case DARWIN_SO_ACCEPTCONN:
		*linux_option = SO_ACCEPTCONN;
		return 1;
	case DARWIN_SO_REUSEADDR:
		*linux_option = SO_REUSEADDR;
		return 1;
	case DARWIN_SO_KEEPALIVE:
		*linux_option = SO_KEEPALIVE;
		return 1;
	case DARWIN_SO_DONTROUTE:
		*linux_option = SO_DONTROUTE;
		return 1;
	case DARWIN_SO_BROADCAST:
		*linux_option = SO_BROADCAST;
		return 1;
	case DARWIN_SO_LINGER:
		*linux_option = SO_LINGER;
		return 1;
	case DARWIN_SO_OOBINLINE:
		*linux_option = SO_OOBINLINE;
		return 1;
#ifdef SO_REUSEPORT
	case DARWIN_SO_REUSEPORT:
		*linux_option = SO_REUSEPORT;
		return 1;
#endif
	case DARWIN_SO_TYPE:
		*linux_option = SO_TYPE;
		return 1;
	case DARWIN_SO_ERROR:
		*linux_option = SO_ERROR;
		return 1;
	default:
		return 0;
	}
}

static int translate_sockaddr(const void* darwin_addr, uint64_t darwin_len,
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

	if (!translate_socket_domain(raw[1], &linux_domain)) {
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

static int translate_rlimit_resource(uint64_t darwin_resource, int* linux_resource)
{
	switch (darwin_resource) {
	case DARWIN_RLIMIT_CPU: *linux_resource = RLIMIT_CPU; return 1;
	case DARWIN_RLIMIT_FSIZE: *linux_resource = RLIMIT_FSIZE; return 1;
	case DARWIN_RLIMIT_DATA: *linux_resource = RLIMIT_DATA; return 1;
	case DARWIN_RLIMIT_STACK: *linux_resource = RLIMIT_STACK; return 1;
	case DARWIN_RLIMIT_CORE: *linux_resource = RLIMIT_CORE; return 1;
#ifdef RLIMIT_AS
	case DARWIN_RLIMIT_AS: *linux_resource = RLIMIT_AS; return 1;
#endif
#ifdef RLIMIT_MEMLOCK
	case DARWIN_RLIMIT_MEMLOCK: *linux_resource = RLIMIT_MEMLOCK; return 1;
#endif
#ifdef RLIMIT_NPROC
	case DARWIN_RLIMIT_NPROC: *linux_resource = RLIMIT_NPROC; return 1;
#endif
	case DARWIN_RLIMIT_NOFILE: *linux_resource = RLIMIT_NOFILE; return 1;
	default: return 0;
	}
}

static void linux_to_darwin_stat(const struct stat* linux_stat,
                                 struct darwin_stat* darwin_stat)
{
	memset(darwin_stat, 0, sizeof(*darwin_stat));
	darwin_stat->st_dev = (int32_t)linux_stat->st_dev;
	darwin_stat->st_mode = (uint16_t)linux_stat->st_mode;
	darwin_stat->st_nlink = (uint16_t)linux_stat->st_nlink;
	darwin_stat->st_ino = linux_stat->st_ino;
	darwin_stat->st_uid = linux_stat->st_uid;
	darwin_stat->st_gid = linux_stat->st_gid;
	darwin_stat->st_rdev = (int32_t)linux_stat->st_rdev;
	darwin_stat->st_atimespec = linux_stat->st_atim;
	darwin_stat->st_mtimespec = linux_stat->st_mtim;
	darwin_stat->st_ctimespec = linux_stat->st_ctim;
	darwin_stat->st_birthtimespec = linux_stat->st_ctim;
	darwin_stat->st_size = linux_stat->st_size;
	darwin_stat->st_blocks = linux_stat->st_blocks;
	darwin_stat->st_blksize = linux_stat->st_blksize;
}

static void linux_to_darwin_statfs(const struct statfs* linux_statfs,
                                   struct darwin_statfs* darwin_statfs)
{
	memset(darwin_statfs, 0, sizeof(*darwin_statfs));
	darwin_statfs->f_bsize = (uint32_t)linux_statfs->f_bsize;
	darwin_statfs->f_iosize = (int32_t)linux_statfs->f_frsize;
	darwin_statfs->f_blocks = linux_statfs->f_blocks;
	darwin_statfs->f_bfree = linux_statfs->f_bfree;
	darwin_statfs->f_bavail = linux_statfs->f_bavail;
	darwin_statfs->f_files = linux_statfs->f_files;
	darwin_statfs->f_ffree = linux_statfs->f_ffree;
	darwin_statfs->f_fsid[0] = linux_statfs->f_fsid.__val[0];
	darwin_statfs->f_fsid[1] = linux_statfs->f_fsid.__val[1];
	darwin_statfs->f_type = (uint32_t)linux_statfs->f_type;
	darwin_statfs->f_flags = (uint32_t)linux_statfs->f_flags;
	snprintf(darwin_statfs->f_fstypename, sizeof(darwin_statfs->f_fstypename),
	         "linux");
}

static void finish_stat_result(struct syscall_gate_state* state, long result,
                               const struct stat* linux_stat)
{
	if (result < 0) {
		set_failure(state, darwin_errno_from_linux(errno ? errno : EIO));
		return;
	}

	linux_to_darwin_stat(linux_stat, (struct darwin_stat*)state->x[1]);
	set_success(state, 0);
}

static void finish_statfs_result(struct syscall_gate_state* state, long result,
                                 const struct statfs* linux_statfs,
                                 struct darwin_statfs* darwin_statfs)
{
	if (result < 0) {
		set_failure(state, darwin_errno_from_linux(errno ? errno : EIO));
		return;
	}

	linux_to_darwin_statfs(linux_statfs, darwin_statfs);
	set_success(state, 0);
}

static void handle_getpriority(struct syscall_gate_state* state)
{
	errno = 0;
	long result = syscall(SYS_getpriority, (int)state->x[0], (id_t)state->x[1]);
	if (result < 0) {
		set_failure(state, darwin_errno_from_linux(errno ? errno : EIO));
		return;
	}

	set_success(state, (uint64_t)(20 - result));
}

static void handle_gettimeofday(struct syscall_gate_state* state)
{
	struct timeval* timeval = (struct timeval*)state->x[0];
	struct timezone* timezone = (struct timezone*)state->x[1];
	uint64_t* absolute_time = (uint64_t*)state->x[2];

	errno = 0;
	long result = syscall(SYS_gettimeofday, timeval, timezone);
	if (result < 0) {
		set_failure(state, darwin_errno_from_linux(errno ? errno : EIO));
		return;
	}

	if (absolute_time) {
		struct timespec monotonic;
		if (clock_gettime(CLOCK_MONOTONIC, &monotonic) != 0) {
			set_failure(state, darwin_errno_from_linux(errno ? errno : EIO));
			return;
		}
		*absolute_time = (uint64_t)monotonic.tv_sec * 1000000000ull +
		                 (uint64_t)monotonic.tv_nsec;
	}

	set_success(state, 0);
}

static void handle_settimeofday(struct syscall_gate_state* state)
{
	if (state->x[0] || state->x[1]) {
		set_failure(state, DARWIN_EPERM);
		return;
	}

	set_success(state, 0);
}

static void store_darwin_timeval_from_microseconds(struct darwin_timeval* timeval,
                                                   int64_t microseconds)
{
	timeval->tv_sec = microseconds / 1000000;
	timeval->tv_usec = (int32_t)(microseconds % 1000000);
	timeval->__pad = 0;
}

static void handle_adjtime(struct syscall_gate_state* state)
{
	struct darwin_timeval* olddelta = (struct darwin_timeval*)state->x[1];
	struct timex linux_timex;
	int64_t microseconds;

	if (state->x[0]) {
		set_failure(state, DARWIN_EPERM);
		return;
	}

	if (!olddelta) {
		set_success(state, 0);
		return;
	}

	memset(&linux_timex, 0, sizeof(linux_timex));
	errno = 0;
	long result = syscall(SYS_adjtimex, &linux_timex);
	if (result < 0) {
		set_failure(state, darwin_errno_from_linux(errno ? errno : EIO));
		return;
	}

	microseconds = linux_timex.offset;
#ifdef STA_NANO
	if (linux_timex.status & STA_NANO)
		microseconds /= 1000;
#endif
	store_darwin_timeval_from_microseconds(olddelta, microseconds);
	set_success(state, 0);
}

static int path_exists_for_mount_mutation(struct syscall_gate_state* state,
                                          const char* path)
{
	struct stat linux_stat;

	if (!path) {
		set_failure(state, DARWIN_EFAULT);
		return 0;
	}

	errno = 0;
	long result = syscall(SYS_newfstatat, AT_FDCWD, path, &linux_stat, 0);
	if (result < 0) {
		set_failure(state, darwin_errno_from_linux(errno ? errno : EIO));
		return 0;
	}

	return 1;
}

static int fd_is_valid_for_mount_mutation(int fd)
{
	errno = 0;
	return syscall(SYS_fcntl, fd, F_GETFD) >= 0;
}

static void handle_mount_mutation_denied(struct syscall_gate_state* state)
{
	set_failure(state, DARWIN_EPERM);
}

static void handle_unmount(struct syscall_gate_state* state)
{
	if (!path_exists_for_mount_mutation(state, (const char*)state->x[0]))
		return;

	handle_mount_mutation_denied(state);
}

static void handle_funmount(struct syscall_gate_state* state)
{
	if (!fd_is_valid_for_mount_mutation((int)state->x[0])) {
		set_failure(state, DARWIN_EBADF);
		return;
	}

	handle_mount_mutation_denied(state);
}

static void handle_quotactl(struct syscall_gate_state* state)
{
	if (!path_exists_for_mount_mutation(state, (const char*)state->x[0]))
		return;

	set_failure(state, DARWIN_ENOTSUP);
}

static void handle_mount(struct syscall_gate_state* state)
{
	if (!state->x[0]) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	if (!path_exists_for_mount_mutation(state, (const char*)state->x[1]))
		return;

	handle_mount_mutation_denied(state);
}

static void handle_stat_at(struct syscall_gate_state* state, int dirfd, int flags)
{
	struct stat linux_stat;
	errno = 0;
	long result = syscall(SYS_newfstatat, dirfd, (const char*)state->x[0],
	                      &linux_stat, flags);
	finish_stat_result(state, result, &linux_stat);
}

static int convert_timeval_pair(const struct timeval* timeval,
                                struct timespec times[2])
{
	if (!timeval)
		return 1;

	for (int i = 0; i < 2; i++) {
		if (timeval[i].tv_usec < 0 || timeval[i].tv_usec >= 1000000) {
			errno = EINVAL;
			return 0;
		}
		times[i].tv_sec = timeval[i].tv_sec;
		times[i].tv_nsec = timeval[i].tv_usec * 1000;
	}

	return 1;
}

static void handle_utimes(struct syscall_gate_state* state)
{
	const struct timeval* timeval = (const struct timeval*)state->x[1];
	struct timespec times[2];
	struct timespec* times_arg = NULL;

	if (timeval) {
		if (!convert_timeval_pair(timeval, times)) {
			set_failure(state, darwin_errno_from_linux(errno));
			return;
		}
		times_arg = times;
	}

	errno = 0;
	long result = syscall(SYS_utimensat, AT_FDCWD, (const char*)state->x[0],
	                      times_arg, 0);
	finish_syscall_result(state, result);
}

static void handle_futimes(struct syscall_gate_state* state)
{
	const struct timeval* timeval = (const struct timeval*)state->x[1];
	struct timespec times[2];
	struct timespec* times_arg = NULL;
	long result;

	if (timeval) {
		if (!convert_timeval_pair(timeval, times)) {
			set_failure(state, darwin_errno_from_linux(errno));
			return;
		}
		times_arg = times;
	}

	errno = 0;
#ifdef AT_EMPTY_PATH
	result = syscall(SYS_utimensat, (int)state->x[0], "", times_arg,
	                 AT_EMPTY_PATH);
#else
	char proc_path[64];
	snprintf(proc_path, sizeof(proc_path), "/proc/self/fd/%d", (int)state->x[0]);
	result = syscall(SYS_utimensat, AT_FDCWD, proc_path, times_arg, 0);
#endif
	finish_syscall_result(state, result);
}

static void handle_statfs_path(struct syscall_gate_state* state)
{
	struct statfs linux_statfs;
	errno = 0;
	long result = syscall(SYS_statfs, (const char*)state->x[0], &linux_statfs);
	finish_statfs_result(state, result, &linux_statfs,
	                     (struct darwin_statfs*)state->x[1]);
}

static void handle_fstatfs_fd(struct syscall_gate_state* state)
{
	struct statfs linux_statfs;
	errno = 0;
	long result = syscall(SYS_fstatfs, (int)state->x[0], &linux_statfs);
	finish_statfs_result(state, result, &linux_statfs,
	                     (struct darwin_statfs*)state->x[1]);
}

static void handle_setsockopt(struct syscall_gate_state* state)
{
	int linux_level;
	int linux_option;

	if (!translate_socket_option((int)state->x[1], (int)state->x[2],
	                             &linux_level, &linux_option)) {
		set_failure(state, DARWIN_ENOPROTOOPT);
		return;
	}

	errno = 0;
	long result = syscall(SYS_setsockopt, (int)state->x[0], linux_level,
	                      linux_option, (const void*)state->x[3],
	                      (socklen_t)state->x[4]);
	finish_syscall_result(state, result);
}

static void handle_getsockopt(struct syscall_gate_state* state)
{
	int linux_level;
	int linux_option;

	if (!translate_socket_option((int)state->x[1], (int)state->x[2],
	                             &linux_level, &linux_option)) {
		set_failure(state, DARWIN_ENOPROTOOPT);
		return;
	}

	errno = 0;
	long result = syscall(SYS_getsockopt, (int)state->x[0], linux_level,
	                      linux_option, (void*)state->x[3],
	                      (socklen_t*)state->x[4]);
	if (result == 0 && (int)state->x[1] == DARWIN_SOL_SOCKET &&
	    (int)state->x[2] == DARWIN_SO_ERROR) {
		socklen_t* value_len_ptr = (socklen_t*)state->x[4];
		int* value_ptr = (int*)state->x[3];
		if (value_ptr && value_len_ptr &&
		    *value_len_ptr >= (socklen_t)sizeof(int))
			*value_ptr = darwin_errno_from_linux(*value_ptr);
	}
	finish_syscall_result(state, result);
}

static void handle_bind(struct syscall_gate_state* state)
{
	struct sockaddr_storage linux_addr;
	socklen_t linux_len;

	if (!translate_sockaddr((const void*)state->x[1], state->x[2],
	                        &linux_addr, &linux_len)) {
		set_failure(state, darwin_errno_from_linux(errno));
		return;
	}

	errno = 0;
	long result = syscall(SYS_bind, (int)state->x[0],
	                      (const struct sockaddr*)&linux_addr, linux_len);
	finish_syscall_result(state, result);
}

static void handle_sendto(struct syscall_gate_state* state)
{
	struct sockaddr_storage linux_addr;
	struct sockaddr_storage* linux_addr_ptr = NULL;
	socklen_t linux_len = 0;

	if (state->x[4]) {
		if (!translate_sockaddr((const void*)state->x[4], state->x[5],
		                        &linux_addr, &linux_len)) {
			set_failure(state, darwin_errno_from_linux(errno));
			return;
		}
		linux_addr_ptr = &linux_addr;
	}

	errno = 0;
	long result = syscall(SYS_sendto, (int)state->x[0],
	                      (const void*)state->x[1], (size_t)state->x[2],
	                      translate_send_flags_100((int)state->x[3]),
	                      (const struct sockaddr*)linux_addr_ptr, linux_len);
	finish_syscall_result(state, result);
}

static void handle_socketpair(struct syscall_gate_state* state)
{
	int linux_domain;

	if (!translate_socket_domain((int)state->x[0], &linux_domain)) {
		set_failure(state, DARWIN_EAFNOSUPPORT);
		return;
	}

	errno = 0;
	long result = syscall(SYS_socketpair, linux_domain, (int)state->x[1],
	                      (int)state->x[2], (int*)state->x[3]);
	finish_syscall_result(state, result);
}

static long pathconf_value_from_statfs(int name, const struct statfs* linux_statfs)
{
	switch (name) {
	case DARWIN_PC_LINK_MAX:
		return LINK_MAX;
	case DARWIN_PC_MAX_CANON:
	case DARWIN_PC_MAX_INPUT:
		return 255;
	case DARWIN_PC_NAME_MAX:
		return linux_statfs && linux_statfs->f_namelen > 0 ?
		       linux_statfs->f_namelen : NAME_MAX;
	case DARWIN_PC_PATH_MAX:
		return PATH_MAX;
	case DARWIN_PC_PIPE_BUF:
		return PIPE_BUF;
	case DARWIN_PC_CHOWN_RESTRICTED:
	case DARWIN_PC_NO_TRUNC:
	case DARWIN_PC_CASE_SENSITIVE:
	case DARWIN_PC_CASE_PRESERVING:
	case DARWIN_PC_2_SYMLINKS:
	case DARWIN_PC_SYNC_IO:
		return 1;
	case DARWIN_PC_VDISABLE:
		return 0;
	case DARWIN_PC_NAME_CHARS_MAX:
		return 255;
	case DARWIN_PC_ALLOC_SIZE_MIN:
	case DARWIN_PC_REC_INCR_XFER_SIZE:
	case DARWIN_PC_REC_MIN_XFER_SIZE:
	case DARWIN_PC_REC_XFER_ALIGN:
		return linux_statfs && linux_statfs->f_frsize > 0 ?
		       linux_statfs->f_frsize : 512;
	case DARWIN_PC_FILESIZEBITS:
		return 64;
	case DARWIN_PC_ASYNC_IO:
	case DARWIN_PC_PRIO_IO:
		return -1;
	case DARWIN_PC_REC_MAX_XFER_SIZE:
		return SSIZE_MAX;
	case DARWIN_PC_SYMLINK_MAX:
		return PATH_MAX;
	default:
		errno = EINVAL;
		return -1;
	}
}

static void handle_pathconf(struct syscall_gate_state* state)
{
	struct stat linux_stat;
	struct statfs linux_statfs;
	const char* path = (const char*)state->x[0];
	int name = (int)state->x[1];

	errno = 0;
	if (syscall(SYS_newfstatat, AT_FDCWD, path, &linux_stat, 0) < 0) {
		set_failure(state, darwin_errno_from_linux(errno ? errno : EIO));
		return;
	}

	memset(&linux_statfs, 0, sizeof(linux_statfs));
	(void)syscall(SYS_statfs, path, &linux_statfs);

	errno = 0;
	long result = pathconf_value_from_statfs(name, &linux_statfs);
	if (result < 0 && errno != 0)
		set_failure(state, darwin_errno_from_linux(errno));
	else
		set_success(state, (uint64_t)result);
}

static void handle_fpathconf(struct syscall_gate_state* state)
{
	struct stat linux_stat;
	struct statfs linux_statfs;
	int fd = (int)state->x[0];
	int name = (int)state->x[1];

	errno = 0;
	if (syscall(SYS_fstat, fd, &linux_stat) < 0) {
		set_failure(state, darwin_errno_from_linux(errno ? errno : EIO));
		return;
	}

	memset(&linux_statfs, 0, sizeof(linux_statfs));
	(void)syscall(SYS_fstatfs, fd, &linux_statfs);

	errno = 0;
	long result = pathconf_value_from_statfs(name, &linux_statfs);
	if (result < 0 && errno != 0)
		set_failure(state, darwin_errno_from_linux(errno));
	else
		set_success(state, (uint64_t)result);
}

static void handle_getdirentries(struct syscall_gate_state* state)
{
	int fd = (int)state->x[0];
	char* darwin_buffer = (char*)state->x[1];
	unsigned int count = (unsigned int)state->x[2];
	long* basep = (long*)state->x[3];
	char* linux_buffer;
	size_t output_offset = 0;
	int64_t last_offset = 0;

	if (count < offsetof(struct darwin_dirent, d_name) + 2) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	linux_buffer = malloc(count);
	if (!linux_buffer) {
		set_failure(state, DARWIN_ENOMEM);
		return;
	}

	errno = 0;
	long result = syscall(SYS_getdents64, fd, linux_buffer, count);
	if (result <= 0) {
		free(linux_buffer);
		finish_syscall_result(state, result);
		return;
	}

	for (long offset = 0; offset < result;) {
		struct linux_dirent64_local* linux_entry =
			(struct linux_dirent64_local*)(linux_buffer + offset);
		size_t name_available;
		size_t name_length;
		size_t darwin_reclen;
		struct darwin_dirent* darwin_entry;

		if (linux_entry->d_reclen == 0 ||
		    offset + linux_entry->d_reclen > result) {
			free(linux_buffer);
			set_failure(state, DARWIN_EIO);
			return;
		}

		name_available = linux_entry->d_reclen -
		                 offsetof(struct linux_dirent64_local, d_name);
		name_length = strnlen(linux_entry->d_name, name_available);
		if (name_length >= name_available) {
			free(linux_buffer);
			set_failure(state, DARWIN_EIO);
			return;
		}

		darwin_reclen = align8(offsetof(struct darwin_dirent, d_name) +
		                       name_length + 1);
		if (darwin_reclen > UINT16_MAX || output_offset + darwin_reclen > count)
			break;

		darwin_entry = (struct darwin_dirent*)(darwin_buffer + output_offset);
		memset(darwin_entry, 0, darwin_reclen);
		darwin_entry->d_ino = linux_entry->d_ino;
		darwin_entry->d_seekoff = (uint64_t)linux_entry->d_off;
		darwin_entry->d_reclen = (uint16_t)darwin_reclen;
		darwin_entry->d_namlen = (uint16_t)name_length;
		darwin_entry->d_type = linux_entry->d_type;
		memcpy(darwin_entry->d_name, linux_entry->d_name, name_length + 1);

		output_offset += darwin_reclen;
		last_offset = linux_entry->d_off;
		offset += linux_entry->d_reclen;
	}

	free(linux_buffer);

	if (output_offset == 0) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	if (basep)
		*basep = (long)last_offset;
	set_success(state, output_offset);
}

static void handle_waitid_single_process(struct syscall_gate_state* state)
{
	set_failure(state, DARWIN_ECHILD);
}

static int is_self_pid(pid_t pid)
{
	return pid == 0 || pid == (pid_t)syscall(SYS_getpid);
}

static void handle_csops(struct syscall_gate_state* state)
{
	pid_t pid = (pid_t)state->x[0];
	uint32_t ops = (uint32_t)state->x[1];
	void* useraddr = (void*)state->x[2];
	size_t usersize = (size_t)state->x[3];
	uint32_t status = 0;

	if (!is_self_pid(pid)) {
		set_failure(state, DARWIN_ESRCH);
		return;
	}

	if (ops != DARWIN_CS_OPS_STATUS) {
		set_failure(state, DARWIN_ENOTSUP);
		return;
	}

	if (!useraddr) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	if (usersize < sizeof(status)) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	memcpy(useraddr, &status, sizeof(status));
	set_success(state, 0);
}

int syscall_range_100_199_dispatch(struct syscall_gate_state* state)
{
	switch (state->x[16]) {
	case 100:
		handle_getpriority(state);
		return 1;
	case 101:
	case 102:
	case 103:
	case 104:
		handle_bind(state);
		return 1;
	case 105:
		handle_setsockopt(state);
		return 1;
	case 106:
		errno = 0;
		finish_syscall_result(state, syscall(SYS_listen, (int)state->x[0],
		                                     (int)state->x[1]));
		return 1;
	case 107:
	case 108:
	case 109:
	case 110:
	case 112:
	case 113:
	case 114:
	case 115:
		set_enosys(state);
		return 1;
	case DARWIN_SYS_sigsuspend:
		set_failure(state, DARWIN_EINTR);
		return 1;
	case 116:
		handle_gettimeofday(state);
		return 1;
	case 117: {
		errno = 0;
		long result = syscall(SYS_getrusage, (int)state->x[0],
		                      (struct rusage*)state->x[1]);
		finish_syscall_result(state, result);
		return 1;
	}
	case 118:
		handle_getsockopt(state);
		return 1;
	case 119:
		set_enosys(state);
		return 1;
	case 120: {
		errno = 0;
		long result = syscall(SYS_readv, (int)state->x[0],
		                      (const struct iovec*)state->x[1],
		                      (int)state->x[2]);
		finish_syscall_result(state, result);
		return 1;
	}
	case 121: {
		long result = machgate_syscall_writev_no_sigpipe((int)state->x[0],
		                                                 (const struct iovec*)state->x[1],
		                                                 (int)state->x[2]);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_settimeofday:
		handle_settimeofday(state);
		return 1;
	case 123: {
		errno = 0;
		long result = syscall(SYS_fchown, (int)state->x[0],
		                      (uid_t)state->x[1], (gid_t)state->x[2]);
		finish_syscall_result(state, result);
		return 1;
	}
	case 124: {
		errno = 0;
		long result = syscall(SYS_fchmod, (int)state->x[0],
		                      (mode_t)state->x[1]);
		finish_syscall_result(state, result);
		return 1;
	}
	case 125:
		set_enosys(state);
		return 1;
	case 126: {
		errno = 0;
		long result = syscall(SYS_setreuid, (uid_t)state->x[0],
		                      (uid_t)state->x[1]);
		finish_syscall_result(state, result);
		return 1;
	}
	case 127: {
		errno = 0;
		long result = syscall(SYS_setregid, (gid_t)state->x[0],
		                      (gid_t)state->x[1]);
		finish_syscall_result(state, result);
		return 1;
	}
	case 128: {
		errno = 0;
		long result = syscall(SYS_renameat, AT_FDCWD, (const char*)state->x[0],
		                      AT_FDCWD, (const char*)state->x[1]);
		finish_syscall_result(state, result);
		return 1;
	}
	case 129:
	case 130:
		set_enosys(state);
		return 1;
	case 131: {
		errno = 0;
		long result = syscall(SYS_flock, (int)state->x[0], (int)state->x[1]);
		finish_syscall_result(state, result);
		return 1;
	}
	case 132: {
		errno = 0;
		long result = syscall(SYS_mknodat, AT_FDCWD, (const char*)state->x[0],
		                      S_IFIFO | (mode_t)state->x[1], 0);
		finish_syscall_result(state, result);
		return 1;
	}
	case 133:
		handle_sendto(state);
		return 1;
	case 134:
		errno = 0;
		finish_syscall_result(state, syscall(SYS_shutdown, (int)state->x[0],
		                                     (int)state->x[1]));
		return 1;
	case 135:
		handle_socketpair(state);
		return 1;
	case 136: {
		errno = 0;
		long result = syscall(SYS_mkdirat, AT_FDCWD, (const char*)state->x[0],
		                      (mode_t)state->x[1]);
		finish_syscall_result(state, result);
		return 1;
	}
	case 137: {
		errno = 0;
		long result = syscall(SYS_unlinkat, AT_FDCWD, (const char*)state->x[0],
		                      AT_REMOVEDIR);
		finish_syscall_result(state, result);
		return 1;
	}
	case 138:
		handle_utimes(state);
		return 1;
	case 139:
		handle_futimes(state);
		return 1;
	case DARWIN_SYS_adjtime:
		handle_adjtime(state);
		return 1;
	case 141:
	case 143:
	case 144:
	case 145:
	case 146:
		set_enosys(state);
		return 1;
	case 142:
		handle_gethostuuid(state);
		return 1;
	case 147: {
		errno = 0;
		long result = syscall(SYS_setsid);
		finish_syscall_result(state, result);
		return 1;
	}
	case 148:
	case 149:
	case 150:
		set_enosys(state);
		return 1;
	case 151: {
		errno = 0;
		long result = syscall(SYS_getpgid, (pid_t)state->x[0]);
		finish_syscall_result(state, result);
		return 1;
	}
	case 152:
		set_success(state, 0);
		return 1;
	case 153: {
		errno = 0;
		long result = syscall(SYS_pread64, (int)state->x[0], (void*)state->x[1],
		                      (size_t)state->x[2], (off_t)state->x[3]);
		finish_syscall_result(state, result);
		return 1;
	}
	case 154: {
		errno = 0;
		long result = syscall(SYS_pwrite64, (int)state->x[0],
		                      (const void*)state->x[1], (size_t)state->x[2],
		                      (off_t)state->x[3]);
		finish_syscall_result(state, result);
		return 1;
	}
	case 156:
	case 160:
	case 162:
	case 163:
	case 166:
	case 168:
	case 171:
	case 172:
	case 174:
	case 175:
	case 176:
		set_enosys(state);
		return 1;
	case DARWIN_SYS_nfssvc:
	case DARWIN_SYS_getfh:
		set_failure(state, DARWIN_ENOTSUP);
		return 1;
	case DARWIN_SYS_quotactl:
		handle_quotactl(state);
		return 1;
	case DARWIN_SYS_unmount:
		handle_unmount(state);
		return 1;
	case DARWIN_SYS_funmount:
		handle_funmount(state);
		return 1;
	case DARWIN_SYS_mount:
		handle_mount(state);
		return 1;
	case DARWIN_SYS_csops:
	case DARWIN_SYS_csops_audittoken:
		handle_csops(state);
		return 1;
	case DARWIN_SYS_waitid:
		handle_waitid_single_process(state);
		return 1;
	case 157:
		handle_statfs_path(state);
		return 1;
	case 158:
		handle_fstatfs_fd(state);
		return 1;
	case 177:
	case 178:
	case 179:
	case 180:
		set_success(state, 0);
		return 1;
	case 181: {
		errno = 0;
		long result = syscall(SYS_setgid, (gid_t)state->x[0]);
		finish_syscall_result(state, result);
		return 1;
	}
	case 182: {
		errno = 0;
		long result = syscall(SYS_setresgid, (gid_t)-1, (gid_t)state->x[0],
		                      (gid_t)-1);
		finish_syscall_result(state, result);
		return 1;
	}
	case 183: {
		errno = 0;
		long result = syscall(SYS_setresuid, (uid_t)-1, (uid_t)state->x[0],
		                      (uid_t)-1);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_thread_selfcounts:
		handle_thread_selfcounts(state);
		return 1;
	case DARWIN_SYS_sigreturn:
		set_failure(state, DARWIN_EINVAL);
		return 1;
	case DARWIN_SYS_sys_panic_with_data:
		set_failure(state, DARWIN_EPERM);
		return 1;
	case 187: {
		errno = 0;
		long result = syscall(SYS_fdatasync, (int)state->x[0]);
		finish_syscall_result(state, result);
		return 1;
	}
	case 188:
		handle_stat_at(state, AT_FDCWD, 0);
		return 1;
	case 189: {
		struct stat linux_stat;
		errno = 0;
		long result = syscall(SYS_fstat, (int)state->x[0], &linux_stat);
		finish_stat_result(state, result, &linux_stat);
		return 1;
	}
	case 190:
		handle_stat_at(state, AT_FDCWD, AT_SYMLINK_NOFOLLOW);
		return 1;
	case 191:
		handle_pathconf(state);
		return 1;
	case 192:
		handle_fpathconf(state);
		return 1;
	case 193:
		set_enosys(state);
		return 1;
	case 194: {
		int linux_resource;
		if (!translate_rlimit_resource(state->x[0], &linux_resource)) {
			set_failure(state, DARWIN_EINVAL);
			return 1;
		}
		errno = 0;
		long result = syscall(SYS_getrlimit, linux_resource,
		                      (struct rlimit*)state->x[1]);
		finish_syscall_result(state, result);
		return 1;
	}
	case 195: {
		int linux_resource;
		if (!translate_rlimit_resource(state->x[0], &linux_resource)) {
			set_failure(state, DARWIN_EINVAL);
			return 1;
		}
		errno = 0;
		long result = syscall(SYS_setrlimit, linux_resource,
		                      (const struct rlimit*)state->x[1]);
		finish_syscall_result(state, result);
		return 1;
	}
	case 196:
		handle_getdirentries(state);
		return 1;
	case 197: {
		long result = machgate_dispatch_darwin_mmap((void*)state->x[0],
		                                   (size_t)state->x[1],
		                                   (int)state->x[2],
		                                   translate_mmap_flags((int)state->x[3]),
		                                   (int)state->x[4],
		                                   (off_t)state->x[5]);
		finish_syscall_result(state, result);
		return 1;
	}
	case 198:
		set_enosys(state);
		return 1;
	case 199: {
		errno = 0;
		long result = syscall(SYS_lseek, (int)state->x[0],
		                      (off_t)state->x[1], (int)state->x[2]);
		finish_syscall_result(state, result);
		return 1;
	}
	default:
		return 0;
	}
}
