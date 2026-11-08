#define _GNU_SOURCE

#include <dlfcn.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int (*attr_init_function)(pthread_attr_t*);
typedef int (*attr_destroy_function)(pthread_attr_t*);
typedef int (*attr_setstacksize_function)(pthread_attr_t*, size_t);
typedef int (*attr_getstacksize_function)(const pthread_attr_t*, size_t*);
typedef int (*attr_setdetachstate_function)(pthread_attr_t*, int);
typedef int (*attr_getdetachstate_function)(const pthread_attr_t*, int*);
typedef int (*attr_setschedparam_function)(pthread_attr_t*,
                                           const struct sched_param*);
typedef int (*attr_getschedparam_function)(const pthread_attr_t*,
                                           struct sched_param*);
typedef int (*attr_setschedpolicy_function)(pthread_attr_t*, int);
typedef int (*attr_setstack_function)(pthread_attr_t*, void*, size_t);
typedef int (*create_function)(pthread_t*, const pthread_attr_t*,
                               void* (*)(void*), void*);
typedef int (*join_function)(pthread_t, void**);

static void* worker(void* argument)
{
	return argument;
}

int main(int argument_count, char** arguments)
{
	if (argument_count != 2)
		return 2;

	void* shim = dlopen(arguments[1], RTLD_NOW | RTLD_LOCAL);
	if (!shim) {
		fprintf(stderr, "%s\n", dlerror());
		return 3;
	}

	attr_init_function attr_init = (attr_init_function)dlsym(shim, "pthread_attr_init");
	attr_destroy_function attr_destroy = (attr_destroy_function)dlsym(shim, "pthread_attr_destroy");
	attr_setstacksize_function attr_setstacksize = (attr_setstacksize_function)dlsym(shim, "pthread_attr_setstacksize");
	attr_getstacksize_function attr_getstacksize = (attr_getstacksize_function)dlsym(shim, "pthread_attr_getstacksize");
	attr_setdetachstate_function attr_setdetachstate = (attr_setdetachstate_function)dlsym(shim, "pthread_attr_setdetachstate");
	attr_getdetachstate_function attr_getdetachstate = (attr_getdetachstate_function)dlsym(shim, "pthread_attr_getdetachstate");
	attr_setschedparam_function attr_setschedparam = (attr_setschedparam_function)dlsym(shim, "pthread_attr_setschedparam");
	attr_getschedparam_function attr_getschedparam = (attr_getschedparam_function)dlsym(shim, "pthread_attr_getschedparam");
	attr_setschedpolicy_function attr_setschedpolicy = (attr_setschedpolicy_function)dlsym(shim, "pthread_attr_setschedpolicy");
	attr_setstack_function attr_setstack = (attr_setstack_function)dlsym(shim, "pthread_attr_setstack");
	create_function create = (create_function)dlsym(shim, "pthread_create");
	join_function join = (join_function)dlsym(shim, "pthread_join");
	if (!attr_init || !attr_destroy || !attr_setstacksize ||
	    !attr_getstacksize || !attr_setdetachstate || !attr_getdetachstate ||
	    !attr_setschedparam || !attr_getschedparam || !attr_setschedpolicy ||
	    !attr_setstack || !create || !join)
		return 4;

	for (int iteration = 0; iteration < 128; iteration++) {
		pthread_attr_t attr;
		memset(&attr, 0xa5, sizeof(attr));
		if (attr_init(&attr) != 0)
			return 10;
		if (attr_setstacksize(&attr, 1024 * 1024) != 0)
			return 11;
		if (attr_setdetachstate(&attr, 1) != 0)
			return 12;
		int detach_state = 0;
		if (attr_getdetachstate(&attr, &detach_state) != 0 || detach_state != 1)
			return 13;
		struct sched_param param = {.sched_priority = 0};
		if (attr_setschedparam(&attr, &param) != 0 ||
		    attr_getschedparam(&attr, &param) != 0)
			return 14;
		if (attr_setschedpolicy(&attr, 1) != 0)
			return 15;

		pthread_t thread;
		if (create(&thread, &attr, worker, (void*)(uintptr_t)iteration) != 0)
			return 16;
		void* result = NULL;
		if (join(thread, &result) != 0 || result != (void*)(uintptr_t)iteration)
			return 17;
	}

	pthread_attr_t raw_attr;
	memset(&raw_attr, 0x5a, sizeof(raw_attr));
	if (attr_setstacksize(&raw_attr, 1024 * 1024) != 22)
		return 20;
	pthread_t raw_thread;
	if (create(&raw_thread, &raw_attr, worker, (void*)0x1234) != 0)
		return 21;
	void* raw_result = NULL;
	if (join(raw_thread, &raw_result) != 0 || raw_result != (void*)0x1234)
		return 22;

	unsigned char* stack = malloc(1024 * 1024);
	if (!stack)
		return 23;
	pthread_attr_t stack_attr;
	if (attr_init(&stack_attr) != 0 ||
	    attr_setstack(&stack_attr, stack, 1024 * 1024) != 0)
		return 24;
	pthread_t stack_thread;
	if (create(&stack_thread, &stack_attr, worker, (void*)0x5678) != 0)
		return 25;
	void* stack_result = NULL;
	if (join(stack_thread, &stack_result) != 0 ||
	    stack_result != (void*)0x5678)
		return 26;
	if (attr_destroy(&stack_attr) != 0)
		return 27;
	free(stack);
	return 0;
}
