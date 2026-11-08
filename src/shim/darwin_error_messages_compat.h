#ifndef MACHGATE_DARWIN_ERROR_MESSAGES_COMPAT_H
#define MACHGATE_DARWIN_ERROR_MESSAGES_COMPAT_H

#include <stddef.h>

__attribute__((visibility("hidden")))
char* machgate_darwin_strerror(int number);
__attribute__((visibility("hidden")))
int machgate_darwin_strerror_r(int number, char* output, size_t capacity);

#endif
