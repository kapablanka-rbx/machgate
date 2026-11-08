#include <dlfcn.h>
#include <stdio.h>

int main(int argc, char** argv)
{
	if (argc != 3)
		return 2;
	void* shim = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
	if (!shim)
		return 1;
	void* (*host_dlopen)(const char*, int) = dlsym(shim, "dlopen");
	if (!host_dlopen)
		return 3;
	void* handle = host_dlopen(argv[2], RTLD_NOW | RTLD_GLOBAL);
	if (!handle || !dlsym(RTLD_DEFAULT, "machgate_dlopen_marker")) {
		fprintf(stderr, "host dlopen flags were changed by the shim\n");
		return 4;
	}
	dlclose(handle);
	return 0;
}
