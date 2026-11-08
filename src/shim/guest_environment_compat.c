#include "guest_environment_compat.h"

#include <stdlib.h>

extern char** environ;
extern char*** _NSGetEnviron(void);

static char*** mutable_environment(void)
{
	char*** environment = _NSGetEnviron();
	environ = *environment;
	return environment;
}

int machgate_darwin_setenv(const char* name, const char* value, int overwrite)
{
	char*** environment = mutable_environment();
	int result = setenv(name, value, overwrite);
	*environment = environ;
	return result;
}

int machgate_darwin_unsetenv(const char* name)
{
	char*** environment = mutable_environment();
	int result = unsetenv(name);
	*environment = environ;
	return result;
}

int machgate_darwin_putenv(char* assignment)
{
	char*** environment = mutable_environment();
	int result = putenv(assignment);
	*environment = environ;
	return result;
}
