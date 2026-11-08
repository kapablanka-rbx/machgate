#ifndef MACHGATE_SHIM_IMAGE_SYMBOLS_COMPAT_H
#define MACHGATE_SHIM_IMAGE_SYMBOLS_COMPAT_H

#include <dlfcn.h>

int machgate_darwin_dladdr(const void* address, Dl_info* info);
char** machgate_darwin_backtrace_symbols(void* const* addresses, int count);
void machgate_darwin_backtrace_symbols_fd(void* const* addresses, int count, int descriptor);

#endif
