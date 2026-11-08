#include "darwin_xattr.h"

#include "shim/darwin_errno.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/xattr.h>
#include <unistd.h>

#define DARWIN_XATTR_NOFOLLOW 1
#define DARWIN_XATTR_CREATE 2
#define DARWIN_XATTR_REPLACE 4
#define DARWIN_XATTR_MAXNAMELEN 127
#define NATIVE_PREFIX "user."
#define NATIVE_PREFIX_LENGTH 5

int machgate_xattr_errno_from_linux(int error)
{
	switch (error) {
	case ENODATA: return 93;
	case ENAMETOOLONG: return 63;
	case EDQUOT: return 69;
	case ESTALE: return 70;
	case EOVERFLOW: return 84;
	default: return machgate_darwin_errno_from_linux(error);
	}
}

static int translate_name(const char* name, char* native_name)
{
	if (!name) {
		errno = EFAULT;
		return -1;
	}
	size_t length = strnlen(name, DARWIN_XATTR_MAXNAMELEN + 1);
	if (!length || length > DARWIN_XATTR_MAXNAMELEN) {
		errno = length ? ENAMETOOLONG : EINVAL;
		return -1;
	}
	memcpy(native_name, NATIVE_PREFIX, NATIVE_PREFIX_LENGTH);
	memcpy(native_name + NATIVE_PREFIX_LENGTH, name, length + 1);
	return 0;
}

static int validate_options(const char* path, uint32_t position, int options, int writing)
{
	int supported = path ? DARWIN_XATTR_NOFOLLOW : 0;
	if (writing)
		supported |= DARWIN_XATTR_CREATE | DARWIN_XATTR_REPLACE;
	if (position || (options & ~supported) ||
	    (writing && (options & 6) == 6)) {
		errno = EINVAL;
		return -1;
	}
	return 0;
}

long machgate_xattr_get(const char* path, int descriptor, const char* name,
	void* value, size_t size, uint32_t position, int options)
{
	char native_name[NATIVE_PREFIX_LENGTH + DARWIN_XATTR_MAXNAMELEN + 1];
	if (validate_options(path, position, options, 0) || translate_name(name, native_name))
		return -1;
	if (!value)
		size = 0;
	if (!path)
		return syscall(SYS_fgetxattr, descriptor, native_name, value, size);
	return syscall(options & DARWIN_XATTR_NOFOLLOW ? SYS_lgetxattr : SYS_getxattr,
	               path, native_name, value, size);
}

long machgate_xattr_set(const char* path, int descriptor, const char* name,
	const void* value, size_t size, uint32_t position, int options)
{
	char native_name[NATIVE_PREFIX_LENGTH + DARWIN_XATTR_MAXNAMELEN + 1];
	if (validate_options(path, position, options, 1) || translate_name(name, native_name))
		return -1;
	if ((!value && size) || size > INT_MAX) {
		errno = size > INT_MAX ? E2BIG : EINVAL;
		return -1;
	}
	int flags = 0;
	if (options & DARWIN_XATTR_CREATE)
		flags |= XATTR_CREATE;
	if (options & DARWIN_XATTR_REPLACE)
		flags |= XATTR_REPLACE;
	if (!path)
		return syscall(SYS_fsetxattr, descriptor, native_name, value, size, flags);
	return syscall(options & DARWIN_XATTR_NOFOLLOW ? SYS_lsetxattr : SYS_setxattr,
	               path, native_name, value, size, flags);
}

long machgate_xattr_remove(const char* path, int descriptor, const char* name, int options)
{
	char native_name[NATIVE_PREFIX_LENGTH + DARWIN_XATTR_MAXNAMELEN + 1];
	if (validate_options(path, 0, options, 0) || translate_name(name, native_name))
		return -1;
	if (!path)
		return syscall(SYS_fremovexattr, descriptor, native_name);
	return syscall(options & DARWIN_XATTR_NOFOLLOW ? SYS_lremovexattr : SYS_removexattr,
	               path, native_name);
}

static long list_native_attributes(const char* path, int descriptor, char* names,
	size_t size, int options)
{
	if (!path)
		return syscall(SYS_flistxattr, descriptor, names, size);
	return syscall(options & DARWIN_XATTR_NOFOLLOW ? SYS_llistxattr : SYS_listxattr,
	               path, names, size);
}

long machgate_xattr_list(const char* path, int descriptor, char* names, size_t size, int options)
{
	if (validate_options(path, 0, options, 0))
		return -1;
	long result = list_native_attributes(path, descriptor, NULL, 0, options);
	if (result <= 0)
		return result;
	char* native_names = malloc((size_t)result);
	if (!native_names)
		return -1;
	result = list_native_attributes(path, descriptor, native_names, result, options);
	if (result < 0) {
		int error = errno;
		free(native_names);
		errno = error;
		return -1;
	}
	size_t count = 0;
	for (size_t offset = 0; offset < (size_t)result;) {
		const char* native_name = native_names + offset;
		size_t length = strlen(native_name) + 1;
		offset += length;
		if (length <= NATIVE_PREFIX_LENGTH + 1 ||
		    length > NATIVE_PREFIX_LENGTH + DARWIN_XATTR_MAXNAMELEN + 1 ||
		    strncmp(native_name, NATIVE_PREFIX, NATIVE_PREFIX_LENGTH) != 0)
			continue;
		size_t guest_length = length - NATIVE_PREFIX_LENGTH;
		if (names && size && count + guest_length <= size)
			memcpy(names + count, native_name + NATIVE_PREFIX_LENGTH, guest_length);
		count += guest_length;
	}
	free(native_names);
	if (names && size && count > size) {
		errno = ERANGE;
		return -1;
	}
	return (long)count;
}
