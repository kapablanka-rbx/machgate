#define _GNU_SOURCE

#include "file_abi.h"

#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

struct machgate_darwin_file_status {
	int32_t device;
	uint16_t mode;
	uint16_t link_count;
	uint64_t inode;
	uint32_t user_id;
	uint32_t group_id;
	int32_t special_device;
	int32_t alignment_padding;
	struct timespec access_time;
	struct timespec modification_time;
	struct timespec status_change_time;
	struct timespec creation_time;
	int64_t byte_size;
	int64_t allocated_blocks;
	int32_t preferred_block_size;
	uint32_t flags;
	uint32_t generation;
	int32_t reserved;
	int64_t extended_reserved[2];
};

_Static_assert(sizeof(struct machgate_darwin_file_status) == 144,
	       "Darwin file status must occupy 144 bytes");
_Static_assert(offsetof(struct machgate_darwin_file_status, mode) == 4,
	       "Darwin file mode offset must remain stable");
_Static_assert(offsetof(struct machgate_darwin_file_status, inode) == 8,
	       "Darwin inode offset must remain stable");
_Static_assert(offsetof(struct machgate_darwin_file_status, access_time) == 32,
	       "Darwin access time offset must remain stable");
_Static_assert(offsetof(struct machgate_darwin_file_status, byte_size) == 96,
	       "Darwin file size offset must remain stable");
_Static_assert(offsetof(struct machgate_darwin_file_status, extended_reserved) == 128,
	       "Darwin reserved-field offset must remain stable");

static void write_guest_file_status(
	const struct stat* host_status,
	struct machgate_darwin_file_status* guest_status)
{
	struct machgate_darwin_file_status result = {0};

	result.device = (int32_t)host_status->st_dev;
	result.mode = (uint16_t)host_status->st_mode;
	result.link_count = (uint16_t)host_status->st_nlink;
	result.inode = (uint64_t)host_status->st_ino;
	result.user_id = host_status->st_uid;
	result.group_id = host_status->st_gid;
	result.special_device = (int32_t)host_status->st_rdev;
	result.access_time = host_status->st_atim;
	result.modification_time = host_status->st_mtim;
	result.status_change_time = host_status->st_ctim;
	result.creation_time = host_status->st_ctim;
	result.byte_size = host_status->st_size;
	result.allocated_blocks = host_status->st_blocks;
	result.preferred_block_size = host_status->st_blksize;
	*guest_status = result;
}

static int read_path_status(int directory, const char* path, void* output,
	                        int linux_flags)
{
	struct stat host_status;

	if (!output) {
		errno = EFAULT;
		return -1;
	}
	if (syscall(SYS_newfstatat, directory, path, &host_status, linux_flags) < 0)
		return -1;
	write_guest_file_status(&host_status, output);
	return 0;
}

static int read_descriptor_status(int descriptor, void* output)
{
	struct stat host_status;

	if (!output) {
		errno = EFAULT;
		return -1;
	}
	if (syscall(SYS_fstat, descriptor, &host_status) < 0)
		return -1;
	write_guest_file_status(&host_status, output);
	return 0;
}

int machgate_stat(const char* path, void* output) __asm__("stat");
int machgate_stat(const char* path, void* output)
{
	return read_path_status(AT_FDCWD, path, output, 0);
}

int machgate_fstat(int descriptor, void* output) __asm__("fstat");
int machgate_fstat(int descriptor, void* output)
{
	return read_descriptor_status(descriptor, output);
}

int machgate_lstat(const char* path, void* output) __asm__("lstat");
int machgate_lstat(const char* path, void* output)
{
	return read_path_status(AT_FDCWD, path, output, AT_SYMLINK_NOFOLLOW);
}

int machgate_fstatat(int directory, const char* path, void* output,
	             int darwin_flags) __asm__("fstatat");
int machgate_fstatat(int directory, const char* path, void* output,
	             int darwin_flags)
{
	return read_path_status(machgate_linux_dirfd(directory), path, output,
	                        machgate_linux_at_flags(darwin_flags));
}

int stat_darwin(const char* path, void* output)
{
	return read_path_status(AT_FDCWD, path, output, 0);
}

int fstat_darwin(int descriptor, void* output)
{
	return read_descriptor_status(descriptor, output);
}

int lstat_darwin(const char* path, void* output)
{
	return read_path_status(AT_FDCWD, path, output, AT_SYMLINK_NOFOLLOW);
}

int fstatat_darwin(int directory, const char* path, void* output, int darwin_flags)
{
	return read_path_status(machgate_linux_dirfd(directory), path, output,
		machgate_linux_at_flags(darwin_flags));
}

struct machgate_darwin_directory_entry {
	uint64_t inode;
	uint64_t seek_offset;
	uint16_t record_length;
	uint16_t name_length;
	uint8_t type;
	char name[1024];
	char alignment_padding[3];
};

_Static_assert(sizeof(struct machgate_darwin_directory_entry) == 1048,
	       "Darwin directory entry must occupy 1048 bytes");
_Static_assert(offsetof(struct machgate_darwin_directory_entry, record_length) == 16,
	       "Darwin directory record length offset must remain stable");
_Static_assert(offsetof(struct machgate_darwin_directory_entry, type) == 20,
	       "Darwin directory type offset must remain stable");
_Static_assert(offsetof(struct machgate_darwin_directory_entry, name) == 21,
	       "Darwin directory name offset must remain stable");

static __thread struct machgate_darwin_directory_entry thread_directory_entry;

static struct dirent* read_host_directory_entry(DIR* directory)
{
	static struct dirent* (*host_readdir)(DIR*);

	if (!host_readdir)
		host_readdir = (struct dirent* (*)(DIR*))dlsym(RTLD_NEXT, "readdir");
	if (!host_readdir) {
		errno = ENOSYS;
		return NULL;
	}
	return host_readdir(directory);
}

static uint16_t darwin_directory_record_length(size_t name_length)
{
	size_t result = sizeof(struct machgate_darwin_directory_entry) +
	                name_length - 1023 + 7;

	return (uint16_t)(result & ~(size_t)7);
}

static void write_guest_directory_entry(
	const struct dirent* host_entry,
	struct machgate_darwin_directory_entry* guest_entry)
{
	size_t name_length = strnlen(host_entry->d_name, sizeof(host_entry->d_name));
	struct machgate_darwin_directory_entry result = {0};

	if (name_length >= sizeof(result.name))
		name_length = sizeof(result.name) - 1;
	result.inode = host_entry->d_ino;
	result.seek_offset = host_entry->d_off;
	result.record_length = darwin_directory_record_length(name_length);
	result.name_length = (uint16_t)name_length;
	result.type = host_entry->d_type;
	memcpy(result.name, host_entry->d_name, name_length);
	*guest_entry = result;
}

struct dirent* machgate_readdir(DIR* directory) __asm__("readdir");
struct dirent* machgate_readdir(DIR* directory)
{
	struct dirent* host_entry = read_host_directory_entry(directory);

	if (!host_entry)
		return NULL;
	write_guest_directory_entry(host_entry, &thread_directory_entry);
	return (struct dirent*)(void*)&thread_directory_entry;
}

int machgate_readdir_r(DIR* directory, void* entry, void** output)
	__asm__("readdir_r");
int machgate_readdir_r(DIR* directory, void* entry, void** output)
{
	if (!entry || !output)
		return EINVAL;

	errno = 0;
	struct dirent* host_entry = read_host_directory_entry(directory);
	if (!host_entry) {
		*output = NULL;
		return errno;
	}

	write_guest_directory_entry(host_entry, entry);
	*output = entry;
	return 0;
}
