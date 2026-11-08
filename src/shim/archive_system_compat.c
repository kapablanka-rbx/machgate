#define _GNU_SOURCE

#include "darwin_errno.h"

#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <mntent.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/statfs.h>
#include <sys/syscall.h>
#include <unistd.h>

enum unsupported_operation_result {
	DARWIN_INVALID_ARGUMENT = 22,
	DARWIN_NOT_SUPPORTED = 45,
};

struct darwin_filesystem_status {
	uint32_t block_size;
	int32_t io_size;
	uint64_t blocks;
	uint64_t free_blocks;
	uint64_t available_blocks;
	uint64_t files;
	uint64_t free_files;
	int32_t filesystem_id[2];
	uint32_t owner;
	uint32_t filesystem_type;
	uint32_t flags;
	uint32_t subtype;
	char type_name[16];
	char mounted_on[1024];
	char mounted_from[1024];
	uint32_t extended_flags;
	uint32_t reserved[7];
};

_Static_assert(sizeof(struct darwin_filesystem_status) == 2168,
	"Darwin filesystem status must occupy 2168 bytes");
_Static_assert(offsetof(struct darwin_filesystem_status, type_name) == 72,
	"Darwin filesystem type name must begin at byte 72");
_Static_assert(offsetof(struct darwin_filesystem_status, extended_flags) == 2136,
	"Darwin extended mount flags must begin at byte 2136");

int64_t OSAtomicAdd64(int64_t amount, volatile int64_t* address)
{
	if (!address)
		return 0;
	return __atomic_add_fetch(address, amount, __ATOMIC_RELAXED);
}

bool OSAtomicCompareAndSwap64(int64_t expected, int64_t replacement,
	                      volatile int64_t* address)
{
	if (!address)
		return false;
	return __atomic_compare_exchange_n(address, &expected, replacement, false,
	                                   __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

int64_t OSAtomicIncrement64(volatile int64_t* address)
{
	return OSAtomicAdd64(1, address);
}

int64_t OSAtomicDecrement64(volatile int64_t* address)
{
	return OSAtomicAdd64(-1, address);
}

__attribute__((noreturn))
void _Exit(int status)
{
	syscall(SYS_exit_group, status);
	__builtin_unreachable();
}

typedef void (*quick_exit_function)(int);

__attribute__((visibility("hidden")))
quick_exit_function machgate_host_quick_exit(void)
{
	quick_exit_function result = dlsym(RTLD_NEXT, "quick_exit");
	return result ? result : _Exit;
}

static int filesystem_error(void)
{
	switch (errno) {
	case ELOOP:
		errno = 62;
		break;
	case ENAMETOOLONG:
		errno = 63;
		break;
	case EOVERFLOW:
		errno = 84;
		break;
	default:
		errno = machgate_darwin_errno_from_linux(errno);
		break;
	}
	return -1;
}

static uint32_t darwin_mount_flags(unsigned long host_flags)
{
	uint32_t result = 0;
	if (host_flags & MS_RDONLY)
		result |= 0x00000001;
	if (host_flags & MS_SYNCHRONOUS)
		result |= 0x00000002;
	if (host_flags & MS_NOEXEC)
		result |= 0x00000004;
	if (host_flags & MS_NOSUID)
		result |= 0x00000008;
	if (host_flags & MS_NODEV)
		result |= 0x00000010;
	if (host_flags & MS_NOATIME)
		result |= 0x10000000;
	return result;
}

static void copy_mount_name(char* output, size_t capacity, const char* name)
{
	size_t length = strnlen(name, capacity - 1);
	memcpy(output, name, length);
	output[length] = '\0';
}

static bool mount_contains_path(const char* directory, const char* path)
{
	size_t length = strlen(directory);
	if (strncmp(directory, path, length) != 0)
		return false;
	return length == 1 || path[length] == '/' || path[length] == '\0';
}

static void read_mount_names(const char* path,
	                     struct darwin_filesystem_status* output)
{
	char resolved_path[PATH_MAX];
	if (!realpath(path, resolved_path))
		return;
	FILE* mounts = setmntent("/proc/self/mounts", "r");
	if (!mounts)
		return;

	size_t longest_directory = 0;
	struct mntent* mount;
	while ((mount = getmntent(mounts)) != NULL) {
		size_t length = strlen(mount->mnt_dir);
		if (length < longest_directory ||
		    !mount_contains_path(mount->mnt_dir, resolved_path))
			continue;
		longest_directory = length;
		copy_mount_name(output->type_name, sizeof(output->type_name),
		                mount->mnt_type);
		copy_mount_name(output->mounted_on, sizeof(output->mounted_on),
		                mount->mnt_dir);
		copy_mount_name(output->mounted_from, sizeof(output->mounted_from),
		                mount->mnt_fsname);
	}
	endmntent(mounts);
}

static void translate_filesystem_status(const struct statfs* host_status,
	                                const char* path,
	                                struct darwin_filesystem_status* output)
{
	struct darwin_filesystem_status result = {0};
	result.block_size = host_status->f_frsize > 0 ?
	                    host_status->f_frsize : host_status->f_bsize;
	result.io_size = host_status->f_bsize;
	result.blocks = host_status->f_blocks;
	result.free_blocks = host_status->f_bfree;
	result.available_blocks = host_status->f_bavail;
	result.files = host_status->f_files;
	result.free_files = host_status->f_ffree;
	result.filesystem_id[0] = host_status->f_fsid.__val[0];
	result.filesystem_id[1] = host_status->f_fsid.__val[1];
	result.flags = darwin_mount_flags(host_status->f_flags);
	if (path)
		read_mount_names(path, &result);
	*output = result;
}

__attribute__((visibility("hidden")))
int machgate_darwin_statfs_compat(const char* path,
	                         struct darwin_filesystem_status* output)
{
	if (!path || !output) {
		errno = EFAULT;
		return filesystem_error();
	}
	struct statfs host_status;
	if (syscall(SYS_statfs, path, &host_status) < 0)
		return filesystem_error();
	translate_filesystem_status(&host_status, path, output);
	return 0;
}

__attribute__((visibility("hidden")))
int machgate_darwin_fstatfs_compat(int descriptor,
	                          struct darwin_filesystem_status* output)
{
	if (!output) {
		errno = EFAULT;
		return filesystem_error();
	}
	struct statfs host_status;
	if (syscall(SYS_fstatfs, descriptor, &host_status) < 0)
		return filesystem_error();
	char descriptor_path[64];
	int result = snprintf(descriptor_path, sizeof(descriptor_path),
	                      "/proc/self/fd/%d", descriptor);
	translate_filesystem_status(&host_status,
		result > 0 && (size_t)result < sizeof(descriptor_path) ?
			descriptor_path : NULL, output);
	return 0;
}

int pthread_cpu_number_np(size_t* cpu_number)
{
	if (!cpu_number) {
		errno = DARWIN_INVALID_ARGUMENT;
		return DARWIN_INVALID_ARGUMENT;
	}
	unsigned int result;
	if (syscall(SYS_getcpu, &result, NULL, NULL) < 0) {
		errno = machgate_darwin_errno_from_linux(errno);
		return errno;
	}
	*cpu_number = result;
	return 0;
}

int pthread_mutexattr_setpolicy_np(pthread_mutexattr_t* attributes, int policy)
{
	if (!attributes || (policy != 1 && policy != 3))
		return DARWIN_INVALID_ARGUMENT;
	return DARWIN_NOT_SUPPORTED;
}
