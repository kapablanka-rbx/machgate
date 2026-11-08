#include <dlfcn.h>
#include <stdio.h>

int main(void)
{
	void* handle = dlopen(NULL, -1);
	if (!handle) {
		fprintf(stderr, "dlopen NULL -1 failed: %s\n", dlerror());
		return 1;
	}
	dlclose(handle);
	return 0;
}
