#ifndef MACHGATE_SHIM_FILE_ABI_H
#define MACHGATE_SHIM_FILE_ABI_H

#include <fcntl.h>
#include <stdint.h>
#include <sys/types.h>

struct machgate_darwin_flock {
	off_t start;
	off_t length;
	int32_t process;
	int16_t type;
	int16_t whence;
};

struct machgate_darwin_fstore {
	uint32_t flags;
	int32_t position_mode;
	off_t offset;
	off_t length;
	int32_t allocated_bytes;
	int32_t padding;
};

__attribute__((visibility("hidden")))
int machgate_linux_at_flags(int darwin_flags);

__attribute__((visibility("hidden")))
int machgate_linux_dirfd(int darwin_dirfd);

__attribute__((visibility("hidden")))
int machgate_linux_open_flags(int darwin_flags);

__attribute__((visibility("hidden")))
int machgate_darwin_open_flags(int linux_flags);

__attribute__((visibility("hidden")))
int machgate_darwin_open_uses_mode(int darwin_flags);

__attribute__((visibility("hidden")))
int machgate_darwin_flock_to_linux(
	const struct machgate_darwin_flock* darwin_flock,
	struct flock* linux_flock);

__attribute__((visibility("hidden")))
void machgate_linux_flock_to_darwin(
	const struct flock* linux_flock,
	struct machgate_darwin_flock* darwin_flock);

__attribute__((visibility("hidden")))
long machgate_darwin_preallocate(
	int descriptor, const struct machgate_darwin_fstore* store);

#endif
