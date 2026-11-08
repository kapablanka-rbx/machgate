#define _GNU_SOURCE

#include "fd_lifecycle.h"
#include "file_abi.h"

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef RENAME_NOREPLACE
#define RENAME_NOREPLACE (1U << 0)
#endif

#ifndef RENAME_EXCHANGE
#define RENAME_EXCHANGE (1U << 1)
#endif

enum darwin_fcntl_command {
	DARWIN_FCNTL_GET_OWNER = 5,
	DARWIN_FCNTL_SET_OWNER = 6,
	DARWIN_FCNTL_GET_LOCK = 7,
	DARWIN_FCNTL_SET_LOCK = 8,
	DARWIN_FCNTL_SET_LOCK_WAIT = 9,
	DARWIN_FCNTL_PREALLOCATE = 42,
	DARWIN_FCNTL_READ_ADVICE = 44,
	DARWIN_FCNTL_READ_AHEAD = 45,
	DARWIN_FCNTL_NO_CACHE = 48,
	DARWIN_FCNTL_GET_PATH = 50,
	DARWIN_FCNTL_FULL_SYNC = 51,
	DARWIN_FCNTL_DUPLICATE_CLOSE_ON_EXEC = 67,
};

enum darwin_ioctl_request {
	DARWIN_IOCTL_CLOSE_ON_EXEC = 0x20006601u,
	DARWIN_IOCTL_CLEAR_CLOSE_ON_EXEC = 0x20006602u,
	DARWIN_IOCTL_BYTES_READABLE = 0x4004667fu,
	DARWIN_IOCTL_NONBLOCKING = 0x8004667eu,
	DARWIN_IOCTL_OUTPUT_QUEUE = 0x40047473u,
	DARWIN_IOCTL_SET_WINDOW_SIZE = 0x80087467u,
	DARWIN_IOCTL_GET_WINDOW_SIZE = 0x40087468u,
};

enum darwin_rename_flag {
	DARWIN_RENAME_SWAP = 0x00000002u,
	DARWIN_RENAME_EXCLUSIVE = 0x00000004u,
};

int machgate_darwin_open_fixed(const char* path, int darwin_flags, mode_t mode)
{
	if (!machgate_darwin_open_uses_mode(darwin_flags))
		mode = 0;
	return syscall(SYS_openat, AT_FDCWD, path,
	               machgate_linux_open_flags(darwin_flags), mode);
}

int machgate_darwin_openat_fixed(int directory, const char* path,
	                         int darwin_flags, mode_t mode)
{
	if (!machgate_darwin_open_uses_mode(darwin_flags))
		mode = 0;
	return syscall(SYS_openat, machgate_linux_dirfd(directory), path,
	               machgate_linux_open_flags(darwin_flags), mode);
}

int __renameatx_np(int source_directory, const char* source,
	           int destination_directory, const char* destination,
	           unsigned int darwin_flags)
{
	unsigned int linux_flags = 0;

	if (darwin_flags & DARWIN_RENAME_SWAP)
		linux_flags |= RENAME_EXCHANGE;
	if (darwin_flags & DARWIN_RENAME_EXCLUSIVE)
		linux_flags |= RENAME_NOREPLACE;
	if (darwin_flags & ~(DARWIN_RENAME_SWAP | DARWIN_RENAME_EXCLUSIVE)) {
		errno = ENOTSUP;
		return -1;
	}
	if (linux_flags) {
		return syscall(SYS_renameat2,
		               machgate_linux_dirfd(source_directory), source,
		               machgate_linux_dirfd(destination_directory), destination,
		               linux_flags);
	}
	return syscall(SYS_renameat,
	               machgate_linux_dirfd(source_directory), source,
	               machgate_linux_dirfd(destination_directory), destination);
}

int renameatx_np(int source_directory, const char* source,
	         int destination_directory, const char* destination,
	         unsigned int darwin_flags)
{
	return __renameatx_np(source_directory, source, destination_directory,
	                      destination, darwin_flags);
}

static int copy_descriptor_path(int descriptor, unsigned long output_address)
{
	if (!output_address) {
		errno = EFAULT;
		return -1;
	}

	char descriptor_path[32];
	int length = snprintf(descriptor_path, sizeof(descriptor_path),
	                      "/proc/self/fd/%d", descriptor);
	if (length < 0 || (size_t)length >= sizeof(descriptor_path)) {
		errno = EINVAL;
		return -1;
	}
	ssize_t result = readlink(descriptor_path, (char*)output_address, 1023);
	if (result < 0)
		return -1;
	((char*)output_address)[result] = '\0';
	return 0;
}

static int perform_fcntl(int descriptor, int darwin_command,
	                     unsigned long argument)
{
	int linux_command;

	switch (darwin_command) {
	case F_DUPFD:
	case F_GETFD:
	case F_SETFD:
		linux_command = darwin_command;
		break;
	case F_GETFL:
		linux_command = F_GETFL;
		break;
	case F_SETFL:
		linux_command = F_SETFL;
		argument = (unsigned long)machgate_linux_open_flags((int)argument);
		break;
	case DARWIN_FCNTL_GET_OWNER:
		linux_command = F_GETOWN;
		break;
	case DARWIN_FCNTL_SET_OWNER:
		linux_command = F_SETOWN;
		break;
	case DARWIN_FCNTL_GET_LOCK:
		linux_command = F_GETLK;
		break;
	case DARWIN_FCNTL_SET_LOCK:
		linux_command = F_SETLK;
		break;
	case DARWIN_FCNTL_SET_LOCK_WAIT:
		linux_command = F_SETLKW;
		break;
	case DARWIN_FCNTL_READ_ADVICE:
	case DARWIN_FCNTL_READ_AHEAD:
	case DARWIN_FCNTL_NO_CACHE:
		return 0;
	case DARWIN_FCNTL_GET_PATH:
		return copy_descriptor_path(descriptor, argument);
	case DARWIN_FCNTL_FULL_SYNC:
		return fsync(descriptor);
	case DARWIN_FCNTL_DUPLICATE_CLOSE_ON_EXEC:
		linux_command = F_DUPFD_CLOEXEC;
		break;
	case DARWIN_FCNTL_PREALLOCATE:
		return machgate_darwin_preallocate(
			descriptor, (const struct machgate_darwin_fstore*)argument);
	default:
		errno = ENOTSUP;
		return -1;
	}

	struct flock linux_flock;
	struct machgate_darwin_flock* darwin_flock = NULL;
	if (darwin_command == DARWIN_FCNTL_GET_LOCK ||
	    darwin_command == DARWIN_FCNTL_SET_LOCK ||
	    darwin_command == DARWIN_FCNTL_SET_LOCK_WAIT) {
		darwin_flock = (struct machgate_darwin_flock*)argument;
		if (machgate_darwin_flock_to_linux(darwin_flock, &linux_flock) != 0)
			return -1;
		argument = (unsigned long)&linux_flock;
	}

	int result = syscall(SYS_fcntl, descriptor, linux_command, argument);
	if (result >= 0 && darwin_command == DARWIN_FCNTL_GET_LOCK)
		machgate_linux_flock_to_darwin(&linux_flock, darwin_flock);
	if (result >= 0 && (darwin_command == F_DUPFD ||
	                    darwin_command == DARWIN_FCNTL_DUPLICATE_CLOSE_ON_EXEC))
		machgate_fd_did_duplicate(descriptor, result);
	if (result >= 0 && darwin_command == F_GETFL)
		result = machgate_darwin_open_flags(result);
	return result;
}

int machgate_darwin_fcntl_fixed(int descriptor, int command,
	                        unsigned long argument)
{
	return perform_fcntl(descriptor, command, argument);
}

static int linux_ioctl_request(unsigned long darwin_request,
	                       unsigned long* result)
{
	switch (darwin_request) {
	case DARWIN_IOCTL_BYTES_READABLE:
		*result = FIONREAD;
		return 0;
	case DARWIN_IOCTL_NONBLOCKING:
		*result = FIONBIO;
		return 0;
#ifdef TIOCOUTQ
	case DARWIN_IOCTL_OUTPUT_QUEUE:
		*result = TIOCOUTQ;
		return 0;
#endif
	case DARWIN_IOCTL_SET_WINDOW_SIZE:
		*result = TIOCSWINSZ;
		return 0;
	case DARWIN_IOCTL_GET_WINDOW_SIZE:
		*result = TIOCGWINSZ;
		return 0;
	default:
		errno = ENOSYS;
		return -1;
	}
}

int machgate_darwin_ioctl_fixed(int descriptor, unsigned long request,
	                        unsigned long argument)
{
	if (request == DARWIN_IOCTL_CLOSE_ON_EXEC)
		return syscall(SYS_fcntl, descriptor, F_SETFD, FD_CLOEXEC);
	if (request == DARWIN_IOCTL_CLEAR_CLOSE_ON_EXEC)
		return syscall(SYS_fcntl, descriptor, F_SETFD, 0);

	unsigned long host_request;
	if (linux_ioctl_request(request, &host_request) != 0)
		return -1;
	return syscall(SYS_ioctl, descriptor, host_request, argument);
}

int machgate_shm_open(const char* name, int darwin_flags, mode_t mode)
	__asm__("shm_open");
int machgate_shm_open(const char* name, int darwin_flags, mode_t mode)
{
	static int (*host_shm_open)(const char*, int, mode_t);

	if (!host_shm_open)
		host_shm_open = (int (*)(const char*, int, mode_t))dlsym(
			RTLD_NEXT, "shm_open");
	if (!host_shm_open) {
		errno = ENOSYS;
		return -1;
	}
	return host_shm_open(name, machgate_linux_open_flags(darwin_flags), mode);
}

int machgate_dup(int descriptor) __asm__("dup");
int machgate_dup(int descriptor)
{
	int result = syscall(SYS_dup, descriptor);

	if (result >= 0)
		machgate_fd_did_duplicate(descriptor, result);
	return result;
}

int machgate_dup2(int source, int destination) __asm__("dup2");
int machgate_dup2(int source, int destination)
{
	int result;

	if (source == destination) {
		result = syscall(SYS_fcntl, source, F_GETFD, 0);
		if (result >= 0)
			result = destination;
	} else {
		result = syscall(SYS_dup3, source, destination, 0);
		if (result >= 0)
			machgate_fd_did_close(destination);
	}
	if (result >= 0)
		machgate_fd_did_duplicate(source, result);
	return result;
}

int machgate_dup3(int source, int destination, int darwin_flags)
	__asm__("dup3");
int machgate_dup3(int source, int destination, int darwin_flags)
{
	int result = syscall(SYS_dup3, source, destination,
	                     machgate_linux_open_flags(darwin_flags));

	if (result >= 0) {
		machgate_fd_did_close(destination);
		machgate_fd_did_duplicate(source, result);
	}
	return result;
}

int machgate_close(int descriptor) __asm__("close");
int machgate_close(int descriptor)
{
	int result = syscall(SYS_close, descriptor);

	if (result == 0)
		machgate_fd_did_close(descriptor);
	return result;
}

int machgate_pipe(int descriptors[2]) __asm__("pipe");
int machgate_pipe(int descriptors[2])
{
	return syscall(SYS_pipe2, descriptors, 0);
}

int machgate_pipe2(int descriptors[2], int darwin_flags) __asm__("pipe2");
int machgate_pipe2(int descriptors[2], int darwin_flags)
{
	return syscall(SYS_pipe2, descriptors,
	               machgate_linux_open_flags(darwin_flags));
}
