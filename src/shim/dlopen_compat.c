#define _GNU_SOURCE
#include <dlfcn.h>
#include <stddef.h>
#include <stdio.h>

void* machgate_host_dlopen(const char* path, int flags)
{
	static void* (*host_dlopen)(const char*, int);
	return dlopen(path, flags);
}
