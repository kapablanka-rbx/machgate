#include "syscall_range_000_099.h"
#include "execve_reexec.h"
#include "guest_vm_dispatch.h"

#include <errno.h>
#include <fcntl.h>
#include <mntent.h>
#include <netinet/in.h>
#include <stddef.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>

#define DARWIN_CARRY 0x20000000u

#define DARWIN_ENOSYS 78
#define DARWIN_EPERM 1
#define DARWIN_ENOENT 2
#define DARWIN_EIO 5
#define DARWIN_ECHILD 10
#define DARWIN_EFAULT 14
#define DARWIN_EINVAL 22
#define DARWIN_EAGAIN 35
#define DARWIN_EDEADLK 11
#define DARWIN_EAFNOSUPPORT 47
#define DARWIN_ENAMETOOLONG 63
#define DARWIN_ENOTSUP 45
#define DARWIN_EOPNOTSUPP 102

#define MACHGATE_LOGIN_NAME_MAX 256

#define DARWIN_SYS_fork 2
#define DARWIN_SYS_getfsstat 18
#define DARWIN_SYS_ptrace 26
#define DARWIN_SYS_chflags 34
#define DARWIN_SYS_fchflags 35
#define DARWIN_SYS_kill 37
#define DARWIN_SYS_sys_crossarch_trap 38
#define DARWIN_SYS_sigaction 46
#define DARWIN_SYS_sigprocmask 48
#define DARWIN_SYS_sigpending 52
#define DARWIN_SYS_sigaltstack 53
#define DARWIN_SYS_ioctl 54
#define DARWIN_SYS_reboot 55
#define DARWIN_SYS_revoke 56
#define DARWIN_SYS_execve 59
#define DARWIN_SYS_chroot 61
#define DARWIN_SYS_vfork 66
#define DARWIN_SYS_oslog_coproc_reg 67
#define DARWIN_SYS_oslog_coproc 68
#define DARWIN_SYS_swapon 85
#define DARWIN_SYS_select 93

#define DARWIN_SIG_BLOCK   1
#define DARWIN_SIG_UNBLOCK 2
#define DARWIN_SIG_SETMASK 3

#define DARWIN_SIG_DFL 0
#define DARWIN_SIG_IGN 1

#define DARWIN_SA_ONSTACK   0x0001
#define DARWIN_SA_RESTART   0x0002
#define DARWIN_SA_RESETHAND 0x0004
#define DARWIN_SA_NOCLDSTOP 0x0008
#define DARWIN_SA_NODEFER   0x0010
#define DARWIN_SA_NOCLDWAIT 0x0020
#define DARWIN_SA_SIGINFO   0x0040
#define DARWIN_SA_USERSPACE_MASK \
	(DARWIN_SA_ONSTACK | DARWIN_SA_RESTART | DARWIN_SA_RESETHAND | \
	 DARWIN_SA_NOCLDSTOP | DARWIN_SA_NODEFER | DARWIN_SA_NOCLDWAIT | \
	 DARWIN_SA_SIGINFO)

#ifdef SA_RESTORER
#define LINUX_SA_RESTORER SA_RESTORER
#else
#define LINUX_SA_RESTORER 0x04000000
#endif

#define DARWIN_SS_ONSTACK 1
#define DARWIN_SS_DISABLE 4

#define DARWIN_O_NONBLOCK  0x00000004
#define DARWIN_O_APPEND    0x00000008
#define DARWIN_O_ASYNC     0x00000040
#define DARWIN_O_NOFOLLOW  0x00000100
#define DARWIN_O_CREAT     0x00000200
#define DARWIN_O_TRUNC     0x00000400
#define DARWIN_O_EXCL      0x00000800
#define DARWIN_O_NOCTTY    0x00020000
#define DARWIN_O_DIRECTORY 0x00100000
#define DARWIN_O_DSYNC     0x00400000
#define DARWIN_O_CLOEXEC   0x01000000

#define DARWIN_F_GETOWN    5
#define DARWIN_F_SETOWN    6
#define DARWIN_F_GETLK     7
#define DARWIN_F_SETLK     8
#define DARWIN_F_SETLKW    9

/* Darwin lock type values (XNU bsd/sys/fcntl.h) */
#define DARWIN_F_RDLCK 1
#define DARWIN_F_UNLCK 2
#define DARWIN_F_WRLCK 3
#define DARWIN_F_FULLFSYNC 51
#define DARWIN_F_DUPFD_CLOEXEC 67
#define DARWIN_F_PREALLOCATE 42
#define DARWIN_F_ALLOCATEALL 0x04
#define DARWIN_F_PEOFPOSMODE 3

#define DARWIN_MS_ASYNC      0x0001
#define DARWIN_MS_INVALIDATE 0x0002
#define DARWIN_MS_SYNC       0x0010

#define DARWIN_MADV_NORMAL     0
#define DARWIN_MADV_RANDOM     1
#define DARWIN_MADV_SEQUENTIAL 2
#define DARWIN_MADV_WILLNEED   3
#define DARWIN_MADV_DONTNEED   4
#define DARWIN_MADV_FREE       5

#define DARWIN_MFSTYPENAMELEN 16
#define DARWIN_MAXPATHLEN 1024

#define DARWIN_AF_UNIX   1
#define DARWIN_AF_INET   2
#define DARWIN_AF_INET6 30

#define DARWIN_SOCK_CLOEXEC  0x10000000
#define DARWIN_SOCK_NONBLOCK 0x20000000

#define DARWIN_FIOCLEX  0x20006601u
#define DARWIN_FIONCLEX 0x20006602u
#define DARWIN_FIONREAD 0x4004667fu
#define DARWIN_FIONBIO  0x8004667eu
#define DARWIN_TIOCOUTQ 0x40047473u
#define DARWIN_TIOCSWINSZ 0x80087467u
#define DARWIN_TIOCGWINSZ 0x40087468u

#define DARWIN_FD_SETSIZE 1024
#define DARWIN_NFDBITS 32

struct flag_pair {
	int darwin;
	int linux_value;
};

struct darwin_sigaction {
	uint64_t handler;
	uint32_t mask;
	uint32_t flags;
};

struct darwin_sigaltstack {
	uint64_t sp;
	uint64_t size;
	uint32_t flags;
	uint32_t pad;
};

struct darwin_timeval_range_000_099 {
	int64_t tv_sec;
	int32_t tv_usec;
	int32_t pad;
};

_Static_assert(sizeof(struct darwin_timeval_range_000_099) == 16,
	"darwin_timeval_range_000_099 must be 16 bytes");

struct darwin_rusage_range_000_099 {
	struct darwin_timeval_range_000_099 ru_utime;
	struct darwin_timeval_range_000_099 ru_stime;
	int64_t ru_maxrss;
	int64_t ru_ixrss;
	int64_t ru_idrss;
	int64_t ru_isrss;
	int64_t ru_minflt;
	int64_t ru_majflt;
	int64_t ru_nswap;
	int64_t ru_inblock;
	int64_t ru_oublock;
	int64_t ru_msgsnd;
	int64_t ru_msgrcv;
	int64_t ru_nsignals;
	int64_t ru_nvcsw;
	int64_t ru_nivcsw;
};

struct darwin_msghdr {
	void* msg_name;
	uint32_t msg_namelen;
	struct iovec* msg_iov;
	int32_t msg_iovlen;
	void* msg_control;
	uint32_t msg_controllen;
	int32_t msg_flags;
};

_Static_assert(sizeof(struct darwin_msghdr) == 48,
	"darwin_msghdr must be 48 bytes");

struct darwin_fsid {
	int32_t val[2];
};

struct darwin_statfs {
	uint32_t f_bsize;
	int32_t f_iosize;
	uint64_t f_blocks;
	uint64_t f_bfree;
	uint64_t f_bavail;
	uint64_t f_files;
	uint64_t f_ffree;
	struct darwin_fsid f_fsid;
	uint32_t f_owner;
	uint32_t f_type;
	uint32_t f_flags;
	uint32_t f_fssubtype;
	char f_fstypename[DARWIN_MFSTYPENAMELEN];
	char f_mntonname[DARWIN_MAXPATHLEN];
	char f_mntfromname[DARWIN_MAXPATHLEN];
	uint32_t f_flags_ext;
	uint32_t f_reserved[7];
};

_Static_assert(sizeof(struct darwin_statfs) == 2168,
	"darwin_statfs must be 2168 bytes");

static char login_name[MACHGATE_LOGIN_NAME_MAX] = "machgate";
static pid_t range_000_process_pid;

__attribute__((constructor))
static void init_range_000_process_pid(void)
{
	range_000_process_pid = (pid_t)syscall(SYS_getpid);
}

static int range_000_in_fork_child(void)
{
	return range_000_process_pid &&
	       (pid_t)syscall(SYS_getpid) != range_000_process_pid;
}

static pid_t range_000_trace_tid(void)
{
	return (pid_t)syscall(SYS_gettid);
}

static FILE* range_000_open_trace_file(void)
{
	const char* trace_file = getenv("MACHGATE_EXECVE_TRACE_FILE");

	if (!trace_file || !*trace_file)
		return NULL;
	return fopen(trace_file, "a");
}

static const struct flag_pair open_flag_pairs[] = {
	{ DARWIN_O_NONBLOCK, O_NONBLOCK },
	{ DARWIN_O_APPEND, O_APPEND },
	{ DARWIN_O_ASYNC, O_ASYNC },
	{ DARWIN_O_NOFOLLOW, O_NOFOLLOW },
	{ DARWIN_O_CREAT, O_CREAT },
	{ DARWIN_O_TRUNC, O_TRUNC },
	{ DARWIN_O_EXCL, O_EXCL },
	{ DARWIN_O_NOCTTY, O_NOCTTY },
	{ DARWIN_O_DIRECTORY, O_DIRECTORY },
	{ DARWIN_O_DSYNC, O_DSYNC },
	{ DARWIN_O_CLOEXEC, O_CLOEXEC },
	{ 0, 0 },
};

static void set_success(struct syscall_gate_state* state, uint64_t result)
{
	state->x[0] = result;
	state->nzcv &= ~DARWIN_CARRY;
}

static void set_failure(struct syscall_gate_state* state, int err)
{
	state->x[0] = (uint64_t)err;
	state->nzcv |= DARWIN_CARRY;
}

static int darwin_errno_from_linux(int linux_errno)
{
	switch (linux_errno) {
	case 0:
		return DARWIN_EIO;
	case EAGAIN:
		return DARWIN_EAGAIN;
	case EDEADLK:
		return DARWIN_EDEADLK;
	case ENOSYS:
		return DARWIN_ENOSYS;
	case ENOTSUP:
		return DARWIN_ENOTSUP;
	case EAFNOSUPPORT:
		return DARWIN_EAFNOSUPPORT;
	case ENAMETOOLONG:
		return DARWIN_ENAMETOOLONG;
	#if EOPNOTSUPP != ENOTSUP
	case EOPNOTSUPP:
		return DARWIN_EOPNOTSUPP;
#endif
	default:
		return linux_errno;
	}
}

static void finish_syscall_result(struct syscall_gate_state* state, long result)
{
	if (result < 0)
		set_failure(state, darwin_errno_from_linux(errno));
	else
		set_success(state, (uint64_t)result);
}

static int trace_wait_enabled(void)
{
	const char* value = getenv("MACHGATE_TRACE_WAIT");
	return value && value[0] && strcmp(value, "0") != 0;
}

static void trace_raw_exit(struct syscall_gate_state* state,
                           const char* name,
                           int status)
{
	FILE* trace_file = range_000_open_trace_file();

	if (!trace_file)
		return;
	fprintf(trace_file,
	        "syscall_gate: %s self=%d tid=%d ppid=%d fork_child=%d status=%d pc=%p lr=%p x16=%llu\n",
	        name, (int)syscall(SYS_getpid), (int)range_000_trace_tid(),
	        (int)syscall(SYS_getppid), range_000_in_fork_child(), status,
	        (void*)state->pc, (void*)state->x30,
	        (unsigned long long)state->x[16]);
	fclose(trace_file);
}

static int should_hide_proc_exe_symlink(const char* path)
{
	const char* cursor;

	if (!path)
		return 0;
	if (strcmp(path, "/proc/self/exe") == 0)
		return 0;
	if (strncmp(path, "/proc/", 6) != 0)
		return 0;

	cursor = path + 6;
	if (*cursor < '0' || *cursor > '9')
		return 0;
	while (*cursor >= '0' && *cursor <= '9')
		cursor++;
	return strcmp(cursor, "/exe") == 0;
}

static int translate_open_flags_to_linux(int darwin_flags)
{
	int result = darwin_flags & O_ACCMODE;

	for (const struct flag_pair* pair = open_flag_pairs; pair->darwin; pair++) {
		if (darwin_flags & pair->darwin)
			result |= pair->linux_value;
	}

	return result;
}

static int translate_open_flags_to_darwin(int linux_flags)
{
	int result = linux_flags & O_ACCMODE;

	for (const struct flag_pair* pair = open_flag_pairs; pair->darwin; pair++) {
		if (linux_flags & pair->linux_value)
			result |= pair->darwin;
	}

	return result;
}

static int translate_msync_flags_to_linux(int darwin_flags)
{
	int result = 0;

	if (darwin_flags & DARWIN_MS_ASYNC)
		result |= MS_ASYNC;
	if (darwin_flags & DARWIN_MS_INVALIDATE)
		result |= MS_INVALIDATE;
	if (darwin_flags & DARWIN_MS_SYNC)
		result |= MS_SYNC;

	return result;
}

static int translate_madvise_to_linux(int darwin_behavior, int* linux_behavior)
{
	switch (darwin_behavior) {
	case DARWIN_MADV_NORMAL:
		*linux_behavior = MADV_NORMAL;
		return 1;
	case DARWIN_MADV_RANDOM:
		*linux_behavior = MADV_RANDOM;
		return 1;
	case DARWIN_MADV_SEQUENTIAL:
		*linux_behavior = MADV_SEQUENTIAL;
		return 1;
	case DARWIN_MADV_WILLNEED:
		*linux_behavior = MADV_WILLNEED;
		return 1;
	case DARWIN_MADV_DONTNEED:
		*linux_behavior = MADV_DONTNEED;
		return 1;
#ifdef MADV_FREE
	case DARWIN_MADV_FREE:
		*linux_behavior = MADV_FREE;
		return 1;
#endif
	default:
		return 0;
	}
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

static int translate_socket_type(int darwin_type)
{
	int linux_type = darwin_type & ~(DARWIN_SOCK_CLOEXEC | DARWIN_SOCK_NONBLOCK);

	if (darwin_type & DARWIN_SOCK_CLOEXEC)
		linux_type |= SOCK_CLOEXEC;
	if (darwin_type & DARWIN_SOCK_NONBLOCK)
		linux_type |= SOCK_NONBLOCK;

	return linux_type;
}

#define DARWIN_MSG_OOB_FLAG       0x0001
#define DARWIN_MSG_PEEK_FLAG      0x0002
#define DARWIN_MSG_DONTROUTE_FLAG 0x0004
#define DARWIN_MSG_TRUNC_FLAG     0x0010
#define DARWIN_MSG_CTRUNC_FLAG    0x0020
#define DARWIN_MSG_WAITALL_FLAG   0x0040
#define DARWIN_MSG_DONTWAIT_FLAG  0x0080
#define DARWIN_MSG_NOSIGNAL_FLAG  0x80000

static int translate_send_flags(int darwin_flags)
{
	int linux_flags = 0;

	if (darwin_flags & DARWIN_MSG_OOB_FLAG)
		linux_flags |= MSG_OOB;
	if (darwin_flags & DARWIN_MSG_DONTROUTE_FLAG)
		linux_flags |= MSG_DONTROUTE;
	if (darwin_flags & DARWIN_MSG_DONTWAIT_FLAG)
		linux_flags |= MSG_DONTWAIT;
	if (darwin_flags & DARWIN_MSG_NOSIGNAL_FLAG)
		linux_flags |= MSG_NOSIGNAL;
	return linux_flags;
}

static int translate_recv_flags(int darwin_flags)
{
	int linux_flags = 0;

	if (darwin_flags & DARWIN_MSG_OOB_FLAG)
		linux_flags |= MSG_OOB;
	if (darwin_flags & DARWIN_MSG_PEEK_FLAG)
		linux_flags |= MSG_PEEK;
	if (darwin_flags & DARWIN_MSG_DONTROUTE_FLAG)
		linux_flags |= MSG_DONTROUTE;
	if (darwin_flags & DARWIN_MSG_TRUNC_FLAG)
		linux_flags |= MSG_TRUNC;
	if (darwin_flags & DARWIN_MSG_CTRUNC_FLAG)
		linux_flags |= MSG_CTRUNC;
	if (darwin_flags & DARWIN_MSG_WAITALL_FLAG)
		linux_flags |= MSG_WAITALL;
	if (darwin_flags & DARWIN_MSG_DONTWAIT_FLAG)
		linux_flags |= MSG_DONTWAIT;
	return linux_flags;
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

static int linux_sockaddr_to_darwin(const struct sockaddr_storage* linux_addr,
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

static int prepare_linux_msghdr(const struct darwin_msghdr* darwin_msg,
                                struct msghdr* linux_msg,
                                struct sockaddr_storage* linux_name,
                                socklen_t* linux_name_len)
{
	if (!darwin_msg) {
		errno = EFAULT;
		return 0;
	}

	if (darwin_msg->msg_control || darwin_msg->msg_controllen) {
		errno = ENOSYS;
		return 0;
	}

	memset(linux_msg, 0, sizeof(*linux_msg));
	linux_msg->msg_iov = darwin_msg->msg_iov;
	linux_msg->msg_iovlen = (size_t)darwin_msg->msg_iovlen;
	linux_msg->msg_flags = darwin_msg->msg_flags;

	if (darwin_msg->msg_name) {
		if (!translate_sockaddr(darwin_msg->msg_name, darwin_msg->msg_namelen,
		                        linux_name, linux_name_len))
			return 0;
		linux_msg->msg_name = linux_name;
		linux_msg->msg_namelen = *linux_name_len;
	}

	return 1;
}

static void handle_sendmsg(struct syscall_gate_state* state)
{
	const struct darwin_msghdr* darwin_msg =
		(const struct darwin_msghdr*)state->x[1];
	struct msghdr linux_msg;
	struct sockaddr_storage linux_name;
	socklen_t linux_name_len = 0;

	if (!prepare_linux_msghdr(darwin_msg, &linux_msg, &linux_name,
	                          &linux_name_len)) {
		set_failure(state, darwin_errno_from_linux(errno));
		return;
	}

	errno = 0;
	finish_syscall_result(state, syscall(SYS_sendmsg, (int)state->x[0],
	                                     &linux_msg,
	                                     translate_send_flags((int)state->x[2])));
}

static void handle_recvmsg(struct syscall_gate_state* state)
{
	struct darwin_msghdr* darwin_msg = (struct darwin_msghdr*)state->x[1];
	struct msghdr linux_msg;
	struct sockaddr_storage linux_name;
	socklen_t linux_name_len = sizeof(linux_name);

	if (!darwin_msg) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	if (darwin_msg->msg_control || darwin_msg->msg_controllen) {
		set_failure(state, DARWIN_ENOSYS);
		return;
	}

	memset(&linux_msg, 0, sizeof(linux_msg));
	linux_msg.msg_iov = darwin_msg->msg_iov;
	linux_msg.msg_iovlen = (size_t)darwin_msg->msg_iovlen;
	if (darwin_msg->msg_name) {
		linux_msg.msg_name = &linux_name;
		linux_msg.msg_namelen = linux_name_len;
	}

	errno = 0;
	long result = syscall(SYS_recvmsg, (int)state->x[0], &linux_msg,
	                      translate_recv_flags((int)state->x[2]));
	if (result < 0) {
		finish_syscall_result(state, result);
		return;
	}

	darwin_msg->msg_flags = linux_msg.msg_flags;
	if (darwin_msg->msg_name) {
		socklen_t darwin_len = darwin_msg->msg_namelen;
		if (!linux_sockaddr_to_darwin(&linux_name, linux_msg.msg_namelen,
		                              darwin_msg->msg_name, &darwin_len)) {
			set_failure(state, darwin_errno_from_linux(errno));
			return;
		}
		darwin_msg->msg_namelen = darwin_len;
	}

	set_success(state, (uint64_t)result);
}

static void handle_recvfrom(struct syscall_gate_state* state)
{
	struct sockaddr_storage linux_name;
	socklen_t linux_name_len = sizeof(linux_name);
	socklen_t* darwin_len_ptr = (socklen_t*)state->x[5];

	errno = 0;
	long result = syscall(SYS_recvfrom, (int)state->x[0], (void*)state->x[1],
	                      (size_t)state->x[2],
	                      translate_recv_flags((int)state->x[3]),
	                      state->x[4] ? (struct sockaddr*)&linux_name : NULL,
	                      state->x[4] ? &linux_name_len : NULL);
	if (result < 0) {
		finish_syscall_result(state, result);
		return;
	}

	if (state->x[4] && darwin_len_ptr) {
		socklen_t darwin_len = *darwin_len_ptr;
		if (!linux_sockaddr_to_darwin(&linux_name, linux_name_len,
		                              (void*)state->x[4], &darwin_len)) {
			set_failure(state, darwin_errno_from_linux(errno));
			return;
		}
		*darwin_len_ptr = darwin_len;
	}

	set_success(state, (uint64_t)result);
}

static void handle_socket_name_result(struct syscall_gate_state* state,
                                      long result,
                                      const struct sockaddr_storage* linux_name,
                                      socklen_t linux_name_len)
{
	socklen_t* darwin_len_ptr = (socklen_t*)state->x[2];

	if (result < 0) {
		finish_syscall_result(state, result);
		return;
	}

	if (state->x[1] && darwin_len_ptr) {
		socklen_t darwin_len = *darwin_len_ptr;
		if (!linux_sockaddr_to_darwin(linux_name, linux_name_len,
		                              (void*)state->x[1], &darwin_len)) {
			set_failure(state, darwin_errno_from_linux(errno));
			return;
		}
		*darwin_len_ptr = darwin_len;
	}

	set_success(state, (uint64_t)result);
}

static void handle_accept(struct syscall_gate_state* state)
{
	struct sockaddr_storage linux_name;
	socklen_t linux_name_len = sizeof(linux_name);

	errno = 0;
	long result = syscall(SYS_accept, (int)state->x[0],
	                      state->x[1] ? (struct sockaddr*)&linux_name : NULL,
	                      state->x[1] ? &linux_name_len : NULL);
	handle_socket_name_result(state, result, &linux_name, linux_name_len);
}

static void handle_getpeername(struct syscall_gate_state* state)
{
	struct sockaddr_storage linux_name;
	socklen_t linux_name_len = sizeof(linux_name);

	errno = 0;
	long result = syscall(SYS_getpeername, (int)state->x[0],
	                      (struct sockaddr*)&linux_name, &linux_name_len);
	handle_socket_name_result(state, result, &linux_name, linux_name_len);
}

static void handle_getsockname(struct syscall_gate_state* state)
{
	struct sockaddr_storage linux_name;
	socklen_t linux_name_len = sizeof(linux_name);

	errno = 0;
	long result = syscall(SYS_getsockname, (int)state->x[0],
	                      (struct sockaddr*)&linux_name, &linux_name_len);
	handle_socket_name_result(state, result, &linux_name, linux_name_len);
}

static void handle_socket(struct syscall_gate_state* state)
{
	int linux_domain;

	if (!translate_socket_domain((int)state->x[0], &linux_domain)) {
		set_failure(state, DARWIN_EAFNOSUPPORT);
		return;
	}

	errno = 0;
	finish_syscall_result(state, syscall(SYS_socket, linux_domain,
	                                     translate_socket_type((int)state->x[1]),
	                                     (int)state->x[2]));
}

static void handle_connect(struct syscall_gate_state* state)
{
	struct sockaddr_storage linux_name;
	socklen_t linux_name_len;

	if (!translate_sockaddr((const void*)state->x[1], state->x[2],
	                        &linux_name, &linux_name_len)) {
		set_failure(state, darwin_errno_from_linux(errno));
		return;
	}

	errno = 0;
	finish_syscall_result(state, syscall(SYS_connect, (int)state->x[0],
	                                     (const struct sockaddr*)&linux_name,
	                                     linux_name_len));
}

static int darwin_signal_to_linux(int darwin_signal)
{
	switch (darwin_signal) {
	case 0: return 0;
	case 1: return SIGHUP;
	case 2: return SIGINT;
	case 3: return SIGQUIT;
	case 4: return SIGILL;
	case 5: return SIGTRAP;
	case 6: return SIGABRT;
	case 8: return SIGFPE;
	case 9: return SIGKILL;
	case 10: return SIGBUS;
	case 11: return SIGSEGV;
	case 12: return SIGSYS;
	case 13: return SIGPIPE;
	case 14: return SIGALRM;
	case 15: return SIGTERM;
	case 16: return SIGURG;
	case 17: return SIGSTOP;
	case 18: return SIGTSTP;
	case 19: return SIGCONT;
	case 20: return SIGCHLD;
	case 21: return SIGTTIN;
	case 22: return SIGTTOU;
	case 23: return SIGIO;
	case 24: return SIGXCPU;
	case 25: return SIGXFSZ;
	case 26: return SIGVTALRM;
	case 27: return SIGPROF;
	case 28: return SIGWINCH;
	case 30: return SIGUSR1;
	case 31: return SIGUSR2;
	default: return -1;
	}
}

static int linux_signal_to_darwin(int linux_signal)
{
	switch (linux_signal) {
	case SIGHUP: return 1;
	case SIGINT: return 2;
	case SIGQUIT: return 3;
	case SIGILL: return 4;
	case SIGTRAP: return 5;
	case SIGABRT: return 6;
	case SIGFPE: return 8;
	case SIGKILL: return 9;
	case SIGBUS: return 10;
	case SIGSEGV: return 11;
	case SIGSYS: return 12;
	case SIGPIPE: return 13;
	case SIGALRM: return 14;
	case SIGTERM: return 15;
	case SIGURG: return 16;
	case SIGSTOP: return 17;
	case SIGTSTP: return 18;
	case SIGCONT: return 19;
	case SIGCHLD: return 20;
	case SIGTTIN: return 21;
	case SIGTTOU: return 22;
	case SIGIO: return 23;
	case SIGXCPU: return 24;
	case SIGXFSZ: return 25;
	case SIGVTALRM: return 26;
	case SIGPROF: return 27;
	case SIGWINCH: return 28;
	case SIGUSR1: return 30;
	case SIGUSR2: return 31;
	default: return 0;
	}
}

static int darwin_wait_options_to_linux(int darwin_options, int* linux_options)
{
	int result = 0;
	int supported = WNOHANG | WUNTRACED;

#ifdef WCONTINUED
	supported |= 0x10;
#endif

	if (darwin_options & ~supported)
		return 0;

	if (darwin_options & WNOHANG)
		result |= WNOHANG;
	if (darwin_options & WUNTRACED)
		result |= WUNTRACED;
#ifdef WCONTINUED
	if (darwin_options & 0x10)
		result |= WCONTINUED;
#endif

	*linux_options = result;
	return 1;
}

static int linux_wait_status_to_darwin(int linux_status)
{
	if (WIFSIGNALED(linux_status)) {
		int darwin_signal = linux_signal_to_darwin(WTERMSIG(linux_status));
		return (linux_status & ~0x7f) | (darwin_signal & 0x7f);
	}
	if (WIFSTOPPED(linux_status)) {
		int darwin_signal = linux_signal_to_darwin(WSTOPSIG(linux_status));
		return (linux_status & ~0xff00) | ((darwin_signal & 0xff) << 8);
	}
	return linux_status;
}

static void linux_rusage_to_darwin(const struct rusage* linux_usage,
                                   struct darwin_rusage_range_000_099* darwin_usage)
{
	darwin_usage->ru_utime.tv_sec = linux_usage->ru_utime.tv_sec;
	darwin_usage->ru_utime.tv_usec = (int32_t)linux_usage->ru_utime.tv_usec;
	darwin_usage->ru_utime.pad = 0;
	darwin_usage->ru_stime.tv_sec = linux_usage->ru_stime.tv_sec;
	darwin_usage->ru_stime.tv_usec = (int32_t)linux_usage->ru_stime.tv_usec;
	darwin_usage->ru_stime.pad = 0;
	darwin_usage->ru_maxrss = linux_usage->ru_maxrss;
	darwin_usage->ru_ixrss = linux_usage->ru_ixrss;
	darwin_usage->ru_idrss = linux_usage->ru_idrss;
	darwin_usage->ru_isrss = linux_usage->ru_isrss;
	darwin_usage->ru_minflt = linux_usage->ru_minflt;
	darwin_usage->ru_majflt = linux_usage->ru_majflt;
	darwin_usage->ru_nswap = linux_usage->ru_nswap;
	darwin_usage->ru_inblock = linux_usage->ru_inblock;
	darwin_usage->ru_oublock = linux_usage->ru_oublock;
	darwin_usage->ru_msgsnd = linux_usage->ru_msgsnd;
	darwin_usage->ru_msgrcv = linux_usage->ru_msgrcv;
	darwin_usage->ru_nsignals = linux_usage->ru_nsignals;
	darwin_usage->ru_nvcsw = linux_usage->ru_nvcsw;
	darwin_usage->ru_nivcsw = linux_usage->ru_nivcsw;
}

static int darwin_sigset_to_linux(uint32_t darwin_set, sigset_t* linux_set)
{
	if (sigemptyset(linux_set) < 0)
		return 0;

	for (int darwin_signal = 1; darwin_signal <= 31; darwin_signal++) {
		if (!(darwin_set & (1u << (darwin_signal - 1))))
			continue;
		int linux_signal = darwin_signal_to_linux(darwin_signal);
		if (linux_signal <= 0)
			return 0;
		if (sigaddset(linux_set, linux_signal) < 0)
			return 0;
	}

	return 1;
}

static uint32_t linux_sigset_to_darwin(const sigset_t* linux_set)
{
	uint32_t result = 0;

	for (int linux_signal = 1; linux_signal < NSIG; linux_signal++) {
		if (sigismember(linux_set, linux_signal) != 1)
			continue;
		int darwin_signal = linux_signal_to_darwin(linux_signal);
		if (darwin_signal > 0)
			result |= 1u << (darwin_signal - 1);
	}

	return result;
}

static int darwin_sigaction_flags_to_linux(uint32_t darwin_flags,
                                           int* linux_flags)
{
	int result = 0;

	if (darwin_flags & ~DARWIN_SA_USERSPACE_MASK)
		return 0;

	if (darwin_flags & DARWIN_SA_ONSTACK)
		result |= SA_ONSTACK;
	if (darwin_flags & DARWIN_SA_RESTART)
		result |= SA_RESTART;
	if (darwin_flags & DARWIN_SA_RESETHAND)
		result |= SA_RESETHAND;
	if (darwin_flags & DARWIN_SA_NOCLDSTOP)
		result |= SA_NOCLDSTOP;
	if (darwin_flags & DARWIN_SA_NODEFER)
		result |= SA_NODEFER;
	if (darwin_flags & DARWIN_SA_NOCLDWAIT)
		result |= SA_NOCLDWAIT;
	if (darwin_flags & DARWIN_SA_SIGINFO)
		result |= SA_SIGINFO;

	*linux_flags = result;
	return 1;
}

static int linux_sigaction_flags_to_darwin(int linux_flags,
                                           uint32_t* darwin_flags)
{
	uint32_t result = 0;
	int supported_flags = SA_ONSTACK | SA_RESTART | SA_RESETHAND |
	                      SA_NOCLDSTOP | SA_NODEFER | SA_NOCLDWAIT |
	                      SA_SIGINFO | LINUX_SA_RESTORER;

	if (linux_flags & ~supported_flags)
		return 0;

	if (linux_flags & SA_ONSTACK)
		result |= DARWIN_SA_ONSTACK;
	if (linux_flags & SA_RESTART)
		result |= DARWIN_SA_RESTART;
	if (linux_flags & SA_RESETHAND)
		result |= DARWIN_SA_RESETHAND;
	if (linux_flags & SA_NOCLDSTOP)
		result |= DARWIN_SA_NOCLDSTOP;
	if (linux_flags & SA_NODEFER)
		result |= DARWIN_SA_NODEFER;
	if (linux_flags & SA_NOCLDWAIT)
		result |= DARWIN_SA_NOCLDWAIT;
	if (linux_flags & SA_SIGINFO)
		result |= DARWIN_SA_SIGINFO;

	*darwin_flags = result;
	return 1;
}

static int darwin_sigaction_to_linux(const struct darwin_sigaction* darwin_action,
                                     struct sigaction* linux_action)
{
	int linux_flags;

	if (darwin_action->handler > DARWIN_SIG_IGN)
		return 0;

	if (!darwin_sigaction_flags_to_linux(darwin_action->flags, &linux_flags))
		return 0;

	memset(linux_action, 0, sizeof(*linux_action));
	if (!darwin_sigset_to_linux(darwin_action->mask, &linux_action->sa_mask))
		return 0;

	linux_action->sa_handler =
		darwin_action->handler == DARWIN_SIG_IGN ? SIG_IGN : SIG_DFL;
	linux_action->sa_flags = linux_flags;
	return 1;
}

static int linux_sigaction_to_darwin(const struct sigaction* linux_action,
                                     struct darwin_sigaction* darwin_action)
{
	uint32_t darwin_flags;

	if (linux_action->sa_handler == SIG_IGN)
		darwin_action->handler = DARWIN_SIG_IGN;
	else if (linux_action->sa_handler == SIG_DFL)
		darwin_action->handler = DARWIN_SIG_DFL;
	else
		return 0;

	if (!linux_sigaction_flags_to_darwin(linux_action->sa_flags,
	                                     &darwin_flags))
		return 0;

	darwin_action->mask = linux_sigset_to_darwin(&linux_action->sa_mask);
	darwin_action->flags = darwin_flags;
	return 1;
}

static int darwin_sigprocmask_how_to_linux(int darwin_how)
{
	switch (darwin_how) {
	case DARWIN_SIG_BLOCK: return SIG_BLOCK;
	case DARWIN_SIG_UNBLOCK: return SIG_UNBLOCK;
	case DARWIN_SIG_SETMASK: return SIG_SETMASK;
	default: return -1;
	}
}

static int darwin_sigaltstack_flags_to_linux(uint32_t darwin_flags,
                                             int* linux_flags)
{
	*linux_flags = 0;
	if (darwin_flags & DARWIN_SS_ONSTACK)
		*linux_flags |= SS_ONSTACK;
	if (darwin_flags & DARWIN_SS_DISABLE)
		*linux_flags |= SS_DISABLE;
	if (darwin_flags & ~(DARWIN_SS_ONSTACK | DARWIN_SS_DISABLE))
		return 0;
	return 1;
}

static uint32_t linux_sigaltstack_flags_to_darwin(int linux_flags)
{
	uint32_t result = 0;
	if (linux_flags & SS_ONSTACK)
		result |= DARWIN_SS_ONSTACK;
	if (linux_flags & SS_DISABLE)
		result |= DARWIN_SS_DISABLE;
	return result;
}

static void handle_kill(struct syscall_gate_state* state)
{
	int linux_signal = darwin_signal_to_linux((int)state->x[1]);
	if (linux_signal < 0) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	errno = 0;
	finish_syscall_result(state, syscall(SYS_kill, (pid_t)state->x[0],
	                                     linux_signal));
}

static void handle_sigaction(struct syscall_gate_state* state)
{
	int linux_signal = darwin_signal_to_linux((int)state->x[0]);
	const struct darwin_sigaction* darwin_new_action =
		(const struct darwin_sigaction*)state->x[1];
	struct darwin_sigaction* darwin_old_action =
		(struct darwin_sigaction*)state->x[2];
	struct sigaction linux_new_action;
	struct sigaction linux_old_action;
	struct darwin_sigaction old_action;

	if (linux_signal <= 0) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	if (darwin_new_action) {
		if (!darwin_sigaction_to_linux(darwin_new_action,
		                               &linux_new_action)) {
			set_failure(state, DARWIN_ENOSYS);
			return;
		}
		if (range_000_in_fork_child() && linux_signal == SIGPIPE) {
			linux_new_action.sa_handler = SIG_IGN;
			linux_new_action.sa_flags = 0;
			sigemptyset(&linux_new_action.sa_mask);
		}
		if (trace_wait_enabled() && linux_signal == SIGPIPE) {
			fprintf(stderr,
			        "syscall_gate: sigaction SIGPIPE fork_child=%d darwin_handler=%llu effective=%p flags=%#x\n",
			        range_000_in_fork_child(),
			        (unsigned long long)darwin_new_action->handler,
			        (void*)linux_new_action.sa_handler,
			        linux_new_action.sa_flags);
		}
	}

	if (darwin_old_action) {
		errno = 0;
		if (sigaction(linux_signal, NULL, &linux_old_action) < 0) {
			set_failure(state, darwin_errno_from_linux(errno));
			return;
		}
		if (!linux_sigaction_to_darwin(&linux_old_action, &old_action)) {
			set_failure(state, DARWIN_ENOSYS);
			return;
		}
	}

	if (darwin_new_action) {
		errno = 0;
		if (sigaction(linux_signal, &linux_new_action, NULL) < 0) {
			set_failure(state, darwin_errno_from_linux(errno));
			return;
		}
	}

	if (darwin_old_action)
		*darwin_old_action = old_action;

	set_success(state, 0);
}

static void handle_sigprocmask(struct syscall_gate_state* state)
{
	int linux_how = darwin_sigprocmask_how_to_linux((int)state->x[0]);
	const uint32_t* darwin_set = (const uint32_t*)state->x[1];
	uint32_t* darwin_old_set = (uint32_t*)state->x[2];
	sigset_t linux_set;
	sigset_t linux_old_set;
	sigset_t* linux_set_ptr = NULL;
	sigset_t* linux_old_set_ptr = darwin_old_set ? &linux_old_set : NULL;

	if (linux_how < 0 && darwin_set) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	if (darwin_set) {
		if (!darwin_sigset_to_linux(*darwin_set, &linux_set)) {
			set_failure(state, DARWIN_EINVAL);
			return;
		}
		linux_set_ptr = &linux_set;
	}

	errno = 0;
	if (sigprocmask(linux_how < 0 ? SIG_SETMASK : linux_how, linux_set_ptr,
	                linux_old_set_ptr) < 0) {
		set_failure(state, darwin_errno_from_linux(errno));
		return;
	}

	if (darwin_old_set)
		*darwin_old_set = linux_sigset_to_darwin(&linux_old_set);

	set_success(state, 0);
}

static void handle_sigpending(struct syscall_gate_state* state)
{
	uint32_t* darwin_set = (uint32_t*)state->x[0];
	sigset_t linux_set;

	if (!darwin_set) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	errno = 0;
	if (sigpending(&linux_set) < 0) {
		set_failure(state, darwin_errno_from_linux(errno));
		return;
	}

	*darwin_set = linux_sigset_to_darwin(&linux_set);
	set_success(state, 0);
}

static void handle_sigaltstack(struct syscall_gate_state* state)
{
	const struct darwin_sigaltstack* darwin_new_stack =
		(const struct darwin_sigaltstack*)state->x[0];
	struct darwin_sigaltstack* darwin_old_stack =
		(struct darwin_sigaltstack*)state->x[1];
	stack_t linux_new_stack;
	stack_t linux_old_stack;
	stack_t* linux_new_stack_ptr = NULL;
	stack_t* linux_old_stack_ptr = darwin_old_stack ? &linux_old_stack : NULL;

	if (darwin_new_stack) {
		int linux_flags;
		if (!darwin_sigaltstack_flags_to_linux(darwin_new_stack->flags,
		                                       &linux_flags)) {
			set_failure(state, DARWIN_EINVAL);
			return;
		}
		linux_new_stack.ss_sp = (void*)darwin_new_stack->sp;
		linux_new_stack.ss_size = darwin_new_stack->size;
		linux_new_stack.ss_flags = linux_flags;
		linux_new_stack_ptr = &linux_new_stack;
	}

	errno = 0;
	if (sigaltstack(linux_new_stack_ptr, linux_old_stack_ptr) < 0) {
		set_failure(state, darwin_errno_from_linux(errno));
		return;
	}

	if (darwin_old_stack) {
		darwin_old_stack->sp = (uint64_t)linux_old_stack.ss_sp;
		darwin_old_stack->size = linux_old_stack.ss_size;
		darwin_old_stack->flags =
			linux_sigaltstack_flags_to_darwin(linux_old_stack.ss_flags);
		darwin_old_stack->pad = 0;
	}

	set_success(state, 0);
}

static long raw_getdtablesize(void)
{
	struct rlimit limit;
#ifdef SYS_prlimit64
	errno = 0;
	long result = syscall(SYS_prlimit64, 0, RLIMIT_NOFILE, NULL, &limit);
	if (result < 0)
		return result;
	return (long)limit.rlim_cur;
#else
	if (getrlimit(RLIMIT_NOFILE, &limit) < 0)
		return -1;
	return (long)limit.rlim_cur;
#endif
}

static void finish_pipe_result(struct syscall_gate_state* state, long result,
                               int pipe_fds[2])
{
	if (result < 0) {
		set_failure(state, darwin_errno_from_linux(errno));
		return;
	}

	state->x[0] = (uint64_t)pipe_fds[0];
	state->x[1] = (uint64_t)pipe_fds[1];
	state->nzcv &= ~DARWIN_CARRY;
}

/* Convert a Darwin-layout struct flock (XNU LP64: l_start+0, l_len+8,
 * l_pid+16, l_type+20, l_whence+22) to the Linux layout. Returns 0 on
 * success, -1 with errno=EINVAL for an unknown lock type. */
static int darwin_flock_to_linux(const void* darwin_flock, struct flock* out)
{
	const unsigned char* d = (const unsigned char*)darwin_flock;
	off_t l_start;
	off_t l_len;
	int32_t l_pid;
	short l_type;
	short l_whence;
	memcpy(&l_start, d + 0, 8);
	memcpy(&l_len, d + 8, 8);
	memcpy(&l_pid, d + 16, 4);
	memcpy(&l_type, d + 20, 2);
	memcpy(&l_whence, d + 22, 2);

	short linux_type;
	if (l_type == DARWIN_F_RDLCK)
		linux_type = F_RDLCK;
	else if (l_type == DARWIN_F_WRLCK)
		linux_type = F_WRLCK;
	else if (l_type == DARWIN_F_UNLCK)
		linux_type = F_UNLCK;
	else {
		errno = EINVAL;
		return -1;
	}

	memset(out, 0, sizeof(*out));
	out->l_type = linux_type;
	out->l_whence = l_whence;
	out->l_start = l_start;
	out->l_len = l_len;
	out->l_pid = l_pid;
	return 0;
}

/* Convert a Linux-layout struct flock back into a Darwin-layout struct. */
static void linux_flock_to_darwin(const struct flock* in, void* darwin_flock)
{
	unsigned char* d = (unsigned char*)darwin_flock;
	short darwin_type;
	if (in->l_type == F_RDLCK)
		darwin_type = DARWIN_F_RDLCK;
	else if (in->l_type == F_WRLCK)
		darwin_type = DARWIN_F_WRLCK;
	else
		darwin_type = DARWIN_F_UNLCK;
	off_t l_start = in->l_start;
	off_t l_len = in->l_len;
	int32_t l_pid = (int32_t)in->l_pid;
	short l_whence = in->l_whence;
	memcpy(d + 0, &l_start, 8);
	memcpy(d + 8, &l_len, 8);
	memcpy(d + 16, &l_pid, 4);
	memcpy(d + 20, &darwin_type, 2);
	memcpy(d + 22, &l_whence, 2);
}

static long raw_fcntl_preallocate(int fd, uint64_t arg)
{
	struct darwin_fstore {
		uint32_t fst_flags;
		int32_t fst_posmode;
		int64_t fst_offset;
		int64_t fst_length;
		int32_t fst_bytesalloc;
		int32_t fst_pad;
	};
	const struct darwin_fstore* store = (const struct darwin_fstore*)arg;

	if (!store)
		return -1;

	errno = 0;
	struct stat file_stat;
	if (syscall(SYS_fstat, fd, &file_stat) != 0)
		return -1;

	if (store->fst_posmode != DARWIN_F_PEOFPOSMODE &&
	    store->fst_posmode != 0)
		return -1;

	off_t allocate_from = store->fst_posmode == DARWIN_F_PEOFPOSMODE
	                          ? file_stat.st_size
	                          : store->fst_offset;
	off_t allocate_end = allocate_from + store->fst_length;

	if (store->fst_flags & DARWIN_F_ALLOCATEALL) {
		if (allocate_end > file_stat.st_size) {
			if (syscall(SYS_ftruncate, fd, allocate_end) != 0)
				return -1;
		}
		return 0;
	}

	errno = 0;
	return syscall(SYS_fallocate, fd, 0, allocate_from,
	               (off_t)store->fst_length);
}

static long raw_fcntl(int fd, int darwin_cmd, uint64_t arg)
{
	int linux_cmd;

	switch (darwin_cmd) {
	case F_DUPFD:
	case F_GETFD:
	case F_SETFD:
	case F_GETFL:
	case F_SETFL:
		linux_cmd = darwin_cmd;
		break;
	case DARWIN_F_GETOWN:
		linux_cmd = F_GETOWN;
		break;
	case DARWIN_F_SETOWN:
		linux_cmd = F_SETOWN;
		break;
	case DARWIN_F_GETLK:
		linux_cmd = F_GETLK;
		break;
	case DARWIN_F_SETLK:
		linux_cmd = F_SETLK;
		break;
	case DARWIN_F_SETLKW:
		linux_cmd = F_SETLKW;
		break;
	case DARWIN_F_DUPFD_CLOEXEC:
		linux_cmd = F_DUPFD_CLOEXEC;
		break;
	case DARWIN_F_PREALLOCATE:
		return raw_fcntl_preallocate(fd, arg);
	case DARWIN_F_FULLFSYNC:
		return syscall(SYS_fsync, fd);
	default:
		errno = ENOSYS;
		return -1;
	}

	if (darwin_cmd == F_SETFL)
		arg = (uint64_t)translate_open_flags_to_linux((int)arg);

	struct flock converted_flock;
	void* darwin_flock_ptr = NULL;
	if (darwin_cmd == DARWIN_F_GETLK || darwin_cmd == DARWIN_F_SETLK ||
	    darwin_cmd == DARWIN_F_SETLKW) {
		darwin_flock_ptr = (void*)arg;
		if (darwin_flock_to_linux(darwin_flock_ptr, &converted_flock) != 0)
			return -1;
		arg = (uint64_t)&converted_flock;
	}

	long result = syscall(SYS_fcntl, fd, linux_cmd, arg);
	if (result >= 0 && darwin_cmd == DARWIN_F_GETLK && darwin_flock_ptr)
		linux_flock_to_darwin(&converted_flock, darwin_flock_ptr);
	if (darwin_cmd == F_GETFL && result >= 0)
		result = translate_open_flags_to_darwin((int)result);
	return result;
}

static long raw_dup2(int from_fd, int to_fd)
{
	if (from_fd == to_fd) {
		long result = syscall(SYS_fcntl, from_fd, F_GETFD);
		if (result < 0)
			return -1;
		return to_fd;
	}

	return syscall(SYS_dup3, from_fd, to_fd, 0);
}

static long raw_fork(void)
{
#ifdef SYS_fork
	return syscall(SYS_fork);
#else
	return syscall(SYS_clone, SIGCHLD, 0, NULL, NULL, 0);
#endif
}

static int translate_ioctl_request(uint64_t darwin_request,
                                   unsigned long* linux_request)
{
	switch (darwin_request) {
	case DARWIN_FIONREAD:
		*linux_request = FIONREAD;
		return 1;
	case DARWIN_FIONBIO:
		*linux_request = FIONBIO;
		return 1;
#ifdef TIOCOUTQ
	case DARWIN_TIOCOUTQ:
		*linux_request = TIOCOUTQ;
		return 1;
#endif
	case DARWIN_TIOCSWINSZ:
		*linux_request = TIOCSWINSZ;
		return 1;
	case DARWIN_TIOCGWINSZ:
		*linux_request = TIOCGWINSZ;
		return 1;
	default:
		return 0;
	}
}

static void handle_ioctl(struct syscall_gate_state* state)
{
	int fd = (int)state->x[0];
	uint64_t darwin_request = state->x[1];
	unsigned long linux_request;

	if (darwin_request == DARWIN_FIOCLEX) {
		errno = 0;
		finish_syscall_result(state, syscall(SYS_fcntl, fd, F_SETFD,
		                                     FD_CLOEXEC));
		return;
	}

	if (darwin_request == DARWIN_FIONCLEX) {
		errno = 0;
		finish_syscall_result(state, syscall(SYS_fcntl, fd, F_SETFD, 0));
		return;
	}

	if (!translate_ioctl_request(darwin_request, &linux_request)) {
		set_failure(state, DARWIN_ENOSYS);
		return;
	}

	errno = 0;
	finish_syscall_result(state, syscall(SYS_ioctl, fd, linux_request,
	                                     state->x[2]));
}

static void darwin_fdset_to_linux(int nfds, const uint32_t* darwin_set,
                                  fd_set* linux_set)
{
	FD_ZERO(linux_set);

	if (!darwin_set)
		return;

	for (int fd = 0; fd < nfds; fd++) {
		uint32_t mask = (uint32_t)1u << (fd % DARWIN_NFDBITS);
		if (darwin_set[fd / DARWIN_NFDBITS] & mask)
			FD_SET(fd, linux_set);
	}
}

static void linux_fdset_to_darwin(int nfds, const fd_set* linux_set,
                                  uint32_t* darwin_set)
{
	if (!darwin_set)
		return;

	size_t words = ((size_t)nfds + DARWIN_NFDBITS - 1) / DARWIN_NFDBITS;
	memset(darwin_set, 0, words * sizeof(*darwin_set));

	for (int fd = 0; fd < nfds; fd++) {
		if (FD_ISSET(fd, linux_set))
			darwin_set[fd / DARWIN_NFDBITS] |=
				(uint32_t)1u << (fd % DARWIN_NFDBITS);
	}
}

static long raw_select(int nfds, fd_set* readfds, fd_set* writefds,
                       fd_set* exceptfds,
                       struct darwin_timeval_range_000_099* darwin_timeout)
{
#ifdef SYS_select
	struct timeval timeout;
	struct timeval* timeout_ptr = NULL;

	if (darwin_timeout) {
		timeout.tv_sec = (time_t)darwin_timeout->tv_sec;
		timeout.tv_usec = (suseconds_t)darwin_timeout->tv_usec;
		timeout_ptr = &timeout;
	}

	long result = syscall(SYS_select, nfds, readfds, writefds, exceptfds,
	                      timeout_ptr);
	if (result >= 0 && darwin_timeout) {
		darwin_timeout->tv_sec = (int64_t)timeout.tv_sec;
		darwin_timeout->tv_usec = (int32_t)timeout.tv_usec;
		darwin_timeout->pad = 0;
	}
	return result;
#else
	struct timespec timeout;
	struct timespec* timeout_ptr = NULL;

	if (darwin_timeout) {
		timeout.tv_sec = (time_t)darwin_timeout->tv_sec;
		timeout.tv_nsec = (long)darwin_timeout->tv_usec * 1000L;
		timeout_ptr = &timeout;
	}

	return syscall(SYS_pselect6, nfds, readfds, writefds, exceptfds,
	               timeout_ptr, NULL);
#endif
}

static void handle_select(struct syscall_gate_state* state)
{
	int nfds = (int)state->x[0];
	uint32_t* darwin_readfds = (uint32_t*)state->x[1];
	uint32_t* darwin_writefds = (uint32_t*)state->x[2];
	uint32_t* darwin_exceptfds = (uint32_t*)state->x[3];
	fd_set readfds;
	fd_set writefds;
	fd_set exceptfds;

	if (nfds < 0 || nfds > FD_SETSIZE || nfds > DARWIN_FD_SETSIZE) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	darwin_fdset_to_linux(nfds, darwin_readfds, &readfds);
	darwin_fdset_to_linux(nfds, darwin_writefds, &writefds);
	darwin_fdset_to_linux(nfds, darwin_exceptfds, &exceptfds);

	errno = 0;
	long result = raw_select(nfds,
	                         darwin_readfds ? &readfds : NULL,
	                         darwin_writefds ? &writefds : NULL,
	                         darwin_exceptfds ? &exceptfds : NULL,
	                         (struct darwin_timeval_range_000_099*)state->x[4]);
	if (result < 0) {
		set_failure(state, darwin_errno_from_linux(errno));
		return;
	}

	linux_fdset_to_darwin(nfds, &readfds, darwin_readfds);
	linux_fdset_to_darwin(nfds, &writefds, darwin_writefds);
	linux_fdset_to_darwin(nfds, &exceptfds, darwin_exceptfds);
	set_success(state, (uint64_t)result);
}

static void linux_to_darwin_statfs(const struct statfs* linux_statfs,
                                   struct darwin_statfs* darwin_statfs,
                                   const char* type_name,
                                   const char* mount_path,
                                   const char* mounted_from)
{
	memset(darwin_statfs, 0, sizeof(*darwin_statfs));
	darwin_statfs->f_bsize = (uint32_t)linux_statfs->f_bsize;
	darwin_statfs->f_iosize = (int32_t)linux_statfs->f_frsize;
	darwin_statfs->f_blocks = linux_statfs->f_blocks;
	darwin_statfs->f_bfree = linux_statfs->f_bfree;
	darwin_statfs->f_bavail = linux_statfs->f_bavail;
	darwin_statfs->f_files = linux_statfs->f_files;
	darwin_statfs->f_ffree = linux_statfs->f_ffree;
	darwin_statfs->f_fsid.val[0] = linux_statfs->f_fsid.__val[0];
	darwin_statfs->f_fsid.val[1] = linux_statfs->f_fsid.__val[1];
	darwin_statfs->f_type = (uint32_t)linux_statfs->f_type;
	darwin_statfs->f_flags = (uint32_t)linux_statfs->f_flags;
	snprintf(darwin_statfs->f_fstypename, sizeof(darwin_statfs->f_fstypename),
	         "%s", type_name ? type_name : "linux");
	snprintf(darwin_statfs->f_mntonname, sizeof(darwin_statfs->f_mntonname),
	         "%s", mount_path ? mount_path : "");
	snprintf(darwin_statfs->f_mntfromname, sizeof(darwin_statfs->f_mntfromname),
	         "%s", mounted_from ? mounted_from : "");
}

static void handle_getfsstat(struct syscall_gate_state* state)
{
	struct darwin_statfs* darwin_statfs = (struct darwin_statfs*)state->x[0];
	int bufsize = (int)state->x[1];
	FILE* mounts;
	struct mntent* entry;
	int count = 0;
	int max_entries = 0;

	if (bufsize < 0) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	mounts = setmntent("/proc/self/mounts", "r");
	if (!mounts) {
		set_failure(state, darwin_errno_from_linux(errno ? errno : EIO));
		return;
	}

	if (darwin_statfs && bufsize > 0)
		max_entries = bufsize / (int)sizeof(*darwin_statfs);

	while ((entry = getmntent(mounts)) != NULL) {
		if (darwin_statfs && count < max_entries) {
			struct statfs linux_statfs;
			errno = 0;
			long result = syscall(SYS_statfs, entry->mnt_dir, &linux_statfs);
			if (result == 0) {
				linux_to_darwin_statfs(&linux_statfs, &darwin_statfs[count],
				                       entry->mnt_type, entry->mnt_dir,
				                       entry->mnt_fsname);
				count++;
			}
			continue;
		}

		count++;
	}

	endmntent(mounts);
	set_success(state, (uint64_t)count);
}

static void handle_chflags(struct syscall_gate_state* state)
{
	struct stat linux_stat;

	if (state->x[1] != 0) {
		set_failure(state, DARWIN_ENOTSUP);
		return;
	}

	errno = 0;
	long result = syscall(SYS_newfstatat, AT_FDCWD,
	                      (const char*)state->x[0], &linux_stat, 0);
	if (result < 0) {
		finish_syscall_result(state, result);
		return;
	}

	set_success(state, 0);
}

static void handle_fchflags(struct syscall_gate_state* state)
{
	struct stat linux_stat;

	if (state->x[1] != 0) {
		set_failure(state, DARWIN_ENOTSUP);
		return;
	}

	errno = 0;
	long result = syscall(SYS_fstat, (int)state->x[0], &linux_stat);
	if (result < 0) {
		finish_syscall_result(state, result);
		return;
	}

	set_success(state, 0);
}

static int handle_enosys(struct syscall_gate_state* state)
{
	set_failure(state, DARWIN_ENOSYS);
	return 1;
}

static void handle_wait4_single_process(struct syscall_gate_state* state)
{
	int linux_options;
	int linux_status = 0;
	struct rusage linux_usage;
	struct rusage* linux_usage_ptr = state->x[3] ? &linux_usage : NULL;
	long result;

	if (!darwin_wait_options_to_linux((int)state->x[2], &linux_options)) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	errno = 0;
	result = syscall(SYS_wait4, (pid_t)state->x[0],
	                 state->x[1] ? &linux_status : NULL,
	                 linux_options, linux_usage_ptr);
	if (result > 0) {
		int darwin_status = linux_status;
		if (state->x[1]) {
			darwin_status = linux_wait_status_to_darwin(linux_status);
			*(int*)state->x[1] = darwin_status;
		}
		if (state->x[3])
			linux_rusage_to_darwin(&linux_usage,
			                       (struct darwin_rusage_range_000_099*)state->x[3]);
		if ((pid_t)state->x[0] > 0)
			result = (pid_t)state->x[0];
		if (trace_wait_enabled() && state->x[1]) {
			fprintf(stderr,
			        "syscall_gate: wait4(pid=%d options=%#x) -> %ld linux_status=%#x darwin_status=%#x exited=%d exit=%d signaled=%d signal=%d core=%d\n",
			        (int)(pid_t)state->x[0], (int)state->x[2], result,
			        linux_status, darwin_status, WIFEXITED(linux_status),
			        WIFEXITED(linux_status) ? WEXITSTATUS(linux_status) : -1,
			        WIFSIGNALED(linux_status),
			        WIFSIGNALED(linux_status) ? WTERMSIG(linux_status) : -1,
#ifdef WCOREDUMP
			        WIFSIGNALED(linux_status) ? WCOREDUMP(linux_status) : 0
#else
			        0
#endif
			);
		}
		FILE* trace_file = range_000_open_trace_file();
		if (trace_file) {
			fprintf(trace_file,
			        "syscall_gate: wait4 self=%d tid=%d ppid=%d fork_child=%d pid=%d result=%ld options=%#llx pc=%p lr=%p linux_status=%#x darwin_status=%#x exited=%d exit=%d signaled=%d signal=%d\n",
			        (int)syscall(SYS_getpid), (int)range_000_trace_tid(),
			        (int)syscall(SYS_getppid), range_000_in_fork_child(),
			        (int)(pid_t)state->x[0], result,
			        (unsigned long long)state->x[2], (void*)state->pc,
			        (void*)state->x30, linux_status, darwin_status,
			        WIFEXITED(linux_status),
			        WIFEXITED(linux_status) ? WEXITSTATUS(linux_status) : -1,
			        WIFSIGNALED(linux_status),
			        WIFSIGNALED(linux_status) ? WTERMSIG(linux_status) : -1);
			fclose(trace_file);
		}
	}
	finish_syscall_result(state, result);
}

static void handle_getlogin(struct syscall_gate_state* state)
{
	char* buffer = (char*)state->x[0];
	size_t length = (size_t)state->x[1];
	size_t login_length = strlen(login_name) + 1;

	if (!buffer) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}
	if (length < login_length) {
		set_failure(state, DARWIN_ENAMETOOLONG);
		return;
	}

	memcpy(buffer, login_name, login_length);
	set_success(state, 0);
}

static void handle_execve(struct syscall_gate_state* state)
{
	int result = machgate_execve_macho_guest((const char*)state->x[0],
	                                        (char* const*)state->x[1],
	                                        (char* const*)state->x[2]);
	set_failure(state, result);
}

static void handle_setlogin(struct syscall_gate_state* state)
{
	const char* name = (const char*)state->x[0];
	size_t length;

	if (!name) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	length = strnlen(name, MACHGATE_LOGIN_NAME_MAX);
	if (length == MACHGATE_LOGIN_NAME_MAX) {
		set_failure(state, DARWIN_ENAMETOOLONG);
		return;
	}

	memcpy(login_name, name, length + 1);
	set_success(state, 0);
}

int syscall_range_000_099_dispatch(struct syscall_gate_state* state)
{
	uint64_t syscall_number = state->x[16];

	if (syscall_number > 99)
		return 0;

	switch (syscall_number) {
	case 0:
	case 8:
	case 11:
	case 17:
	case 19:
	case 21:
	case 22:
	case 40:
	case 44:
	case 45:
	case 62:
	case 63:
	case 64:
	case 69:
	case 70:
	case 71:
	case 72:
	case 76:
	case 77:
	case 84:
	case 87:
	case 88:
	case 91:
	case 94:
	case 99:
		return handle_enosys(state);
		case 1:
			trace_raw_exit(state, "exit_range_000", (int)state->x[0]);
			syscall(SYS_exit_group, (int)state->x[0]);
			_exit((int)state->x[0]);
		case DARWIN_SYS_getfsstat:
			handle_getfsstat(state);
			return 1;
	case DARWIN_SYS_fork:
		errno = 0;
		finish_syscall_result(state, raw_fork());
		return 1;
	case DARWIN_SYS_vfork:
		set_failure(state, DARWIN_EAGAIN);
		return 1;
	case 3:
		errno = 0;
		finish_syscall_result(state, syscall(SYS_read, (int)state->x[0],
		                                     (void*)state->x[1],
		                                     (size_t)state->x[2]));
		return 1;
	case 7:
		handle_wait4_single_process(state);
		return 1;
	case 4:
		finish_syscall_result(state,
		                      machgate_syscall_write_no_sigpipe((int)state->x[0],
		                                                        (const void*)state->x[1],
		                                                        (size_t)state->x[2]));
		return 1;
	case 5:
		errno = 0;
		finish_syscall_result(state, syscall(SYS_openat, AT_FDCWD,
		                                     (const char*)state->x[0],
		                                     translate_open_flags_to_linux((int)state->x[1]),
		                                     (mode_t)state->x[2]));
		return 1;
	case 6:
		errno = 0;
		finish_syscall_result(state, syscall(SYS_close, (int)state->x[0]));
		return 1;
	case 9:
		errno = 0;
		finish_syscall_result(state, syscall(SYS_linkat, AT_FDCWD,
		                                     (const char*)state->x[0],
		                                     AT_FDCWD,
		                                     (const char*)state->x[1], 0));
		return 1;
	case 10:
		errno = 0;
		finish_syscall_result(state, syscall(SYS_unlinkat, AT_FDCWD,
		                                     (const char*)state->x[0], 0));
		return 1;
	case 12:
		errno = 0;
		finish_syscall_result(state, syscall(SYS_chdir, (const char*)state->x[0]));
		return 1;
	case 13:
		errno = 0;
		finish_syscall_result(state, syscall(SYS_fchdir, (int)state->x[0]));
		return 1;
	case 14:
		errno = 0;
		finish_syscall_result(state, syscall(SYS_mknodat, AT_FDCWD,
		                                     (const char*)state->x[0],
		                                     (mode_t)state->x[1],
		                                     (dev_t)state->x[2]));
		return 1;
	case 15:
		errno = 0;
		finish_syscall_result(state, syscall(SYS_fchmodat, AT_FDCWD,
		                                     (const char*)state->x[0],
		                                     (mode_t)state->x[1], 0));
		return 1;
	case 16:
		errno = 0;
		finish_syscall_result(state, syscall(SYS_fchownat, AT_FDCWD,
		                                     (const char*)state->x[0],
		                                     (uid_t)state->x[1],
		                                     (gid_t)state->x[2], 0));
		return 1;
	case 20:
		errno = 0;
		finish_syscall_result(state, syscall(SYS_getpid));
		return 1;
	case 23:
		errno = 0;
		finish_syscall_result(state, syscall(SYS_setuid, (uid_t)state->x[0]));
		return 1;
	case 24:
		errno = 0;
		finish_syscall_result(state, syscall(SYS_getuid));
		return 1;
	case 25:
		errno = 0;
		finish_syscall_result(state, syscall(SYS_geteuid));
		return 1;
	case DARWIN_SYS_ptrace:
		set_failure(state, DARWIN_EPERM);
		return 1;
	case 27:
		handle_recvmsg(state);
		return 1;
	case 28:
		handle_sendmsg(state);
		return 1;
	case 29:
		handle_recvfrom(state);
		return 1;
	case 30:
		handle_accept(state);
		return 1;
	case 31:
		handle_getpeername(state);
		return 1;
	case 32:
		handle_getsockname(state);
		return 1;
	case 33:
		errno = 0;
		finish_syscall_result(state, syscall(SYS_faccessat, AT_FDCWD,
		                                     (const char*)state->x[0],
			                                     (int)state->x[1], 0));
			return 1;
		case DARWIN_SYS_chflags:
			handle_chflags(state);
			return 1;
		case DARWIN_SYS_fchflags:
			handle_fchflags(state);
			return 1;
		case 36:
			syscall(SYS_sync);
			set_success(state, 0);
			return 1;
	case DARWIN_SYS_kill:
		handle_kill(state);
		return 1;
	case DARWIN_SYS_sys_crossarch_trap:
		set_failure(state, DARWIN_EINVAL);
		return 1;
	case 39:
		errno = 0;
		finish_syscall_result(state, syscall(SYS_getppid));
		return 1;
	case 41:
		errno = 0;
		finish_syscall_result(state, syscall(SYS_dup, (int)state->x[0]));
		return 1;
	case 42: {
		int pipe_fds[2];
		errno = 0;
		finish_pipe_result(state, syscall(SYS_pipe2, pipe_fds, 0), pipe_fds);
		return 1;
	}
	case 43:
		errno = 0;
		finish_syscall_result(state, syscall(SYS_getegid));
		return 1;
	case DARWIN_SYS_sigaction:
		handle_sigaction(state);
		return 1;
	case DARWIN_SYS_sigprocmask:
		handle_sigprocmask(state);
		return 1;
	case 49:
		handle_getlogin(state);
		return 1;
	case 50:
		handle_setlogin(state);
		return 1;
	case 51:
		if (state->x[0]) {
			errno = 0;
			finish_syscall_result(state, syscall(SYS_acct, (const char*)state->x[0]));
			return 1;
		}
		set_success(state, 0);
		return 1;
	case DARWIN_SYS_sigpending:
		handle_sigpending(state);
		return 1;
	case DARWIN_SYS_sigaltstack:
		handle_sigaltstack(state);
		return 1;
	case DARWIN_SYS_ioctl:
		handle_ioctl(state);
		return 1;
		case DARWIN_SYS_reboot:
			set_failure(state, DARWIN_EPERM);
			return 1;
		case 47:
			errno = 0;
			finish_syscall_result(state, syscall(SYS_getgid));
			return 1;
		case DARWIN_SYS_revoke:
			set_failure(state, DARWIN_ENOTSUP);
			return 1;
		case 57:
			errno = 0;
			finish_syscall_result(state, syscall(SYS_symlinkat,
		                                        (const char*)state->x[0],
		                                        AT_FDCWD,
		                                        (const char*)state->x[1]));
		return 1;
		case 58:
			if (should_hide_proc_exe_symlink((const char*)state->x[0])) {
				set_failure(state, DARWIN_ENOENT);
				return 1;
			}
			errno = 0;
			finish_syscall_result(state, syscall(SYS_readlinkat, AT_FDCWD,
			                                        (const char*)state->x[0],
		                                        (char*)state->x[1],
		                                        (size_t)state->x[2]));
		return 1;
	case 60:
		errno = 0;
		finish_syscall_result(state, syscall(SYS_umask, (mode_t)state->x[0]));
		return 1;
	case DARWIN_SYS_execve:
		handle_execve(state);
		return 1;
	case DARWIN_SYS_chroot:
		set_failure(state, DARWIN_EPERM);
		return 1;
	case 65:
		errno = 0;
		finish_syscall_result(state, syscall(SYS_msync, (void*)state->x[0],
		                                     (size_t)state->x[1],
		                                     translate_msync_flags_to_linux((int)state->x[2])));
		return 1;
	case DARWIN_SYS_oslog_coproc_reg:
	case DARWIN_SYS_oslog_coproc:
		set_success(state, 0);
		return 1;
	case 73:
		finish_syscall_result(state,
		                    machgate_dispatch_darwin_munmap((void*)state->x[0],
		                                                    (size_t)state->x[1]));
		return 1;
	case 74:
		errno = 0;
		finish_syscall_result(state, syscall(SYS_mprotect, (void*)state->x[0],
		                                     (size_t)state->x[1],
		                                     (int)state->x[2]));
		return 1;
	case 75: {
		int linux_behavior;
		if (!translate_madvise_to_linux((int)state->x[2], &linux_behavior)) {
			errno = ENOSYS;
			finish_syscall_result(state, -1);
			return 1;
		}

		errno = 0;
		finish_syscall_result(state, syscall(SYS_madvise, (void*)state->x[0],
		                                     (size_t)state->x[1],
		                                     linux_behavior));
		return 1;
	}
	case 78:
		errno = 0;
		finish_syscall_result(state, syscall(SYS_mincore, (void*)state->x[0],
		                                     (size_t)state->x[1],
		                                     (unsigned char*)state->x[2]));
		return 1;
	case 79:
		errno = 0;
		finish_syscall_result(state, syscall(SYS_getgroups, (int)state->x[0],
		                                     (gid_t*)state->x[1]));
		return 1;
	case 80:
		errno = 0;
		finish_syscall_result(state, syscall(SYS_setgroups, (size_t)state->x[0],
		                                     (const gid_t*)state->x[1]));
		return 1;
	case 81:
		errno = 0;
		finish_syscall_result(state, syscall(SYS_getpgid, 0));
		return 1;
	case 82:
		errno = 0;
		finish_syscall_result(state, syscall(SYS_setpgid, (pid_t)state->x[0],
		                                     (pid_t)state->x[1]));
		return 1;
	case 83:
		errno = 0;
		finish_syscall_result(state, syscall(SYS_setitimer, (int)state->x[0],
		                                     (const struct itimerval*)state->x[1],
		                                     (struct itimerval*)state->x[2]));
		return 1;
	case DARWIN_SYS_swapon:
		set_failure(state, DARWIN_EPERM);
		return 1;
	case 86:
		errno = 0;
		finish_syscall_result(state, syscall(SYS_getitimer, (int)state->x[0],
		                                     (struct itimerval*)state->x[1]));
		return 1;
	case 89:
		errno = 0;
		finish_syscall_result(state, raw_getdtablesize());
		return 1;
	case 90:
		errno = 0;
		finish_syscall_result(state, raw_dup2((int)state->x[0],
		                                      (int)state->x[1]));
		return 1;
	case 92:
		errno = 0;
		finish_syscall_result(state, raw_fcntl((int)state->x[0],
		                                       (int)state->x[1],
		                                       state->x[2]));
		return 1;
	case DARWIN_SYS_select:
		handle_select(state);
		return 1;
	case 95:
		errno = 0;
		finish_syscall_result(state, syscall(SYS_fsync, (int)state->x[0]));
		return 1;
	case 96:
		errno = 0;
		finish_syscall_result(state, syscall(SYS_setpriority, (int)state->x[0],
		                                     (id_t)state->x[1],
		                                     (int)state->x[2]));
		return 1;
	case 97:
		handle_socket(state);
		return 1;
	case 98:
		handle_connect(state);
		return 1;
	default:
		fprintf(stderr,
		        "syscall_range_000_099: deferred Darwin syscall %llu pc=%p "
		        "x0=%#llx x1=%#llx x2=%#llx x3=%#llx x4=%#llx x5=%#llx\n",
		        (unsigned long long)syscall_number, (void*)state->pc,
		        (unsigned long long)state->x[0],
		        (unsigned long long)state->x[1],
		        (unsigned long long)state->x[2],
		        (unsigned long long)state->x[3],
		        (unsigned long long)state->x[4],
		        (unsigned long long)state->x[5]);
		set_failure(state, DARWIN_ENOSYS);
		return 1;
	}
}
