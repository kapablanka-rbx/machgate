#define _GNU_SOURCE

#include "file_abi.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

_Static_assert(sizeof(struct machgate_darwin_flock) == 24,
	       "Darwin flock must occupy 24 bytes");
_Static_assert(offsetof(struct machgate_darwin_flock, type) == 20,
	       "Darwin flock lock type must be at byte 20");
_Static_assert(sizeof(struct machgate_darwin_fstore) == 32,
	       "Darwin fstore must occupy 32 bytes");

enum darwin_at_value {
	DARWIN_AT_CURRENT_WORKING_DIRECTORY = -2,
	DARWIN_AT_CHECK_EFFECTIVE_ACCESS = 0x0010,
	DARWIN_AT_DO_NOT_FOLLOW_SYMLINK = 0x0020,
	DARWIN_AT_FOLLOW_SYMLINK = 0x0040,
	DARWIN_AT_REMOVE_DIRECTORY = 0x0080,
};

enum darwin_lock_value {
	DARWIN_LOCK_READ = 1,
	DARWIN_LOCK_UNLOCK = 2,
	DARWIN_LOCK_WRITE = 3,
};

enum darwin_preallocate_value {
	DARWIN_PREALLOCATE_ALL = 0x04,
	DARWIN_POSITION_END_OF_FILE = 3,
};

enum darwin_open_value {
	DARWIN_OPEN_NONBLOCKING = 0x00000004,
	DARWIN_OPEN_APPEND = 0x00000008,
	DARWIN_OPEN_ASYNCHRONOUS = 0x00000040,
	DARWIN_OPEN_NO_FOLLOW = 0x00000100,
	DARWIN_OPEN_CREATE = 0x00000200,
	DARWIN_OPEN_TRUNCATE = 0x00000400,
	DARWIN_OPEN_EXCLUSIVE = 0x00000800,
	DARWIN_OPEN_NO_CONTROLLING_TERMINAL = 0x00020000,
	DARWIN_OPEN_DIRECTORY = 0x00100000,
	DARWIN_OPEN_DATA_SYNCHRONIZED = 0x00400000,
	DARWIN_OPEN_CLOSE_ON_EXEC = 0x01000000,
};

struct open_flag_translation {
	int guest;
	int host;
};

static const struct open_flag_translation open_flag_translations[] = {
	{ DARWIN_OPEN_NONBLOCKING, O_NONBLOCK },
	{ DARWIN_OPEN_APPEND, O_APPEND },
	{ DARWIN_OPEN_ASYNCHRONOUS, FASYNC },
	{ DARWIN_OPEN_NO_FOLLOW, O_NOFOLLOW },
	{ DARWIN_OPEN_CREATE, O_CREAT },
	{ DARWIN_OPEN_TRUNCATE, O_TRUNC },
	{ DARWIN_OPEN_EXCLUSIVE, O_EXCL },
	{ DARWIN_OPEN_NO_CONTROLLING_TERMINAL, O_NOCTTY },
	{ DARWIN_OPEN_DIRECTORY, O_DIRECTORY },
	{ DARWIN_OPEN_DATA_SYNCHRONIZED, O_DSYNC },
	{ DARWIN_OPEN_CLOSE_ON_EXEC, O_CLOEXEC },
};

int machgate_linux_open_flags(int darwin_flags)
{
	int result = darwin_flags & O_ACCMODE;

	for (size_t index = 0;
	     index < sizeof(open_flag_translations) /
	             sizeof(open_flag_translations[0]);
	     index++) {
		if (darwin_flags & open_flag_translations[index].guest)
			result |= open_flag_translations[index].host;
	}
	return result;
}

int machgate_darwin_open_flags(int linux_flags)
{
	int result = linux_flags & O_ACCMODE;

	for (size_t index = 0;
	     index < sizeof(open_flag_translations) /
	             sizeof(open_flag_translations[0]);
	     index++) {
		if (linux_flags & open_flag_translations[index].host)
			result |= open_flag_translations[index].guest;
	}
	return result;
}

int machgate_darwin_open_uses_mode(int darwin_flags)
{
	return (darwin_flags & DARWIN_OPEN_CREATE) != 0;
}

int machgate_linux_dirfd(int darwin_dirfd)
{
	if (darwin_dirfd == DARWIN_AT_CURRENT_WORKING_DIRECTORY)
		return AT_FDCWD;
	return darwin_dirfd;
}

int machgate_linux_at_flags(int darwin_flags)
{
	int result = 0;

	if (darwin_flags & DARWIN_AT_CHECK_EFFECTIVE_ACCESS)
		result |= AT_EACCESS;
	if (darwin_flags & DARWIN_AT_DO_NOT_FOLLOW_SYMLINK)
		result |= AT_SYMLINK_NOFOLLOW;
	if (darwin_flags & DARWIN_AT_FOLLOW_SYMLINK)
		result |= AT_SYMLINK_FOLLOW;
	if (darwin_flags & DARWIN_AT_REMOVE_DIRECTORY)
		result |= AT_REMOVEDIR;
	return result;
}

int machgate_darwin_flock_to_linux(
	const struct machgate_darwin_flock* darwin_flock,
	struct flock* linux_flock)
{
	if (!darwin_flock || !linux_flock) {
		errno = EFAULT;
		return -1;
	}

	memset(linux_flock, 0, sizeof(*linux_flock));
	switch (darwin_flock->type) {
	case DARWIN_LOCK_READ:
		linux_flock->l_type = F_RDLCK;
		break;
	case DARWIN_LOCK_WRITE:
		linux_flock->l_type = F_WRLCK;
		break;
	case DARWIN_LOCK_UNLOCK:
		linux_flock->l_type = F_UNLCK;
		break;
	default:
		errno = EINVAL;
		return -1;
	}
	linux_flock->l_whence = darwin_flock->whence;
	linux_flock->l_start = darwin_flock->start;
	linux_flock->l_len = darwin_flock->length;
	linux_flock->l_pid = darwin_flock->process;
	return 0;
}

void machgate_linux_flock_to_darwin(
	const struct flock* linux_flock,
	struct machgate_darwin_flock* darwin_flock)
{
	switch (linux_flock->l_type) {
	case F_RDLCK:
		darwin_flock->type = DARWIN_LOCK_READ;
		break;
	case F_WRLCK:
		darwin_flock->type = DARWIN_LOCK_WRITE;
		break;
	default:
		darwin_flock->type = DARWIN_LOCK_UNLOCK;
		break;
	}
	darwin_flock->whence = linux_flock->l_whence;
	darwin_flock->start = linux_flock->l_start;
	darwin_flock->length = linux_flock->l_len;
	darwin_flock->process = linux_flock->l_pid;
}

long machgate_darwin_preallocate(
	int descriptor, const struct machgate_darwin_fstore* store)
{
	if (!store) {
		errno = EFAULT;
		return -1;
	}
	if ((store->position_mode != 0 &&
	     store->position_mode != DARWIN_POSITION_END_OF_FILE) ||
	    store->offset < 0 || store->length < 0) {
		errno = EINVAL;
		return -1;
	}

	struct stat file_status;
	if (syscall(SYS_fstat, descriptor, &file_status) != 0)
		return -1;

	off_t allocate_from = store->position_mode == DARWIN_POSITION_END_OF_FILE ?
	                      file_status.st_size : store->offset;
	if (store->length > INT64_MAX - allocate_from) {
		errno = EOVERFLOW;
		return -1;
	}
	off_t allocate_end = allocate_from + store->length;

	if (store->flags & DARWIN_PREALLOCATE_ALL) {
		if (allocate_end <= file_status.st_size)
			return 0;
		return syscall(SYS_ftruncate, descriptor, allocate_end);
	}
	return syscall(SYS_fallocate, descriptor, 0, allocate_from, store->length);
}
