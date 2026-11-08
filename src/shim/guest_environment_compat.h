#ifndef MACHGATE_SHIM_GUEST_ENVIRONMENT_COMPAT_H
#define MACHGATE_SHIM_GUEST_ENVIRONMENT_COMPAT_H

int machgate_darwin_setenv(const char* name, const char* value, int overwrite);
int machgate_darwin_unsetenv(const char* name);
int machgate_darwin_putenv(char* assignment);

#endif
