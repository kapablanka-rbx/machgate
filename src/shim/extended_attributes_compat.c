#include "extended_attributes_compat.h"
#include "syscall/darwin_xattr.h"

#include <errno.h>

static int require_path(const char* path)
{
	if (path)
		return 0;
	errno = EFAULT;
	return -1;
}

static long guest_result(long result)
{
	if (result < 0)
		errno = machgate_xattr_errno_from_linux(errno);
	return result;
}

long machgate_darwin_getxattr(const char* path, const char* name, void* value,
	size_t size, uint32_t position, int options)
{
	if (require_path(path))
		return -1;
	return guest_result(machgate_xattr_get(path, -1, name, value, size, position, options));
}

long machgate_darwin_fgetxattr(int descriptor, const char* name, void* value,
	size_t size, uint32_t position, int options)
{
	return guest_result(machgate_xattr_get(NULL, descriptor, name, value, size, position, options));
}

int machgate_darwin_setxattr(const char* path, const char* name, const void* value,
	size_t size, uint32_t position, int options)
{
	if (require_path(path))
		return -1;
	return guest_result(machgate_xattr_set(path, -1, name, value, size, position, options));
}

int machgate_darwin_fsetxattr(int descriptor, const char* name, const void* value,
	size_t size, uint32_t position, int options)
{
	return guest_result(machgate_xattr_set(NULL, descriptor, name, value, size, position, options));
}

int machgate_darwin_removexattr(const char* path, const char* name, int options)
{
	if (require_path(path))
		return -1;
	return guest_result(machgate_xattr_remove(path, -1, name, options));
}

int machgate_darwin_fremovexattr(int descriptor, const char* name, int options)
{
	return guest_result(machgate_xattr_remove(NULL, descriptor, name, options));
}

long machgate_darwin_listxattr(const char* path, char* names, size_t size, int options)
{
	if (require_path(path))
		return -1;
	return guest_result(machgate_xattr_list(path, -1, names, size, options));
}

long machgate_darwin_flistxattr(int descriptor, char* names, size_t size, int options)
{
	return guest_result(machgate_xattr_list(NULL, descriptor, names, size, options));
}
