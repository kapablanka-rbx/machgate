#ifndef MACHGATE_SHIM_EXTENDED_ATTRIBUTES_COMPAT_H
#define MACHGATE_SHIM_EXTENDED_ATTRIBUTES_COMPAT_H

#include <stddef.h>
#include <stdint.h>

long machgate_darwin_getxattr(const char*, const char*, void*, size_t, uint32_t, int);
long machgate_darwin_fgetxattr(int, const char*, void*, size_t, uint32_t, int);
int machgate_darwin_setxattr(const char*, const char*, const void*, size_t, uint32_t, int);
int machgate_darwin_fsetxattr(int, const char*, const void*, size_t, uint32_t, int);
int machgate_darwin_removexattr(const char*, const char*, int);
int machgate_darwin_fremovexattr(int, const char*, int);
long machgate_darwin_listxattr(const char*, char*, size_t, int);
long machgate_darwin_flistxattr(int, char*, size_t, int);

#endif
