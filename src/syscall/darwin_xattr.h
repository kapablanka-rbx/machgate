#ifndef MACHGATE_SYSCALL_DARWIN_XATTR_H
#define MACHGATE_SYSCALL_DARWIN_XATTR_H

#include <stddef.h>
#include <stdint.h>

int machgate_xattr_errno_from_linux(int error);
long machgate_xattr_get(const char* path, int descriptor, const char* name,
	void* value, size_t size, uint32_t position, int options);
long machgate_xattr_set(const char* path, int descriptor, const char* name,
	const void* value, size_t size, uint32_t position, int options);
long machgate_xattr_remove(const char* path, int descriptor, const char* name, int options);
long machgate_xattr_list(const char* path, int descriptor, char* names, size_t size, int options);

#endif
