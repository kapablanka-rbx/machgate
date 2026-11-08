#define _GNU_SOURCE

#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>

static FILE* exit_output;

static void record_tlv_exit(void* argument)
{
	fprintf(exit_output, "%s\n", (const char*)argument);
	fflush(exit_output);
}

static void record_tsd_exit(void* value)
{
	fprintf(exit_output, "%s\n", (const char*)value);
	fflush(exit_output);
}

int main(int argument_count, char** arguments)
{
	if (argument_count != 3)
		return 2;

	const char* output_path = arguments[1];
	const char* termination_symbol = arguments[2];
	FILE* output = fopen(output_path, "w");
	if (!output)
		return 3;
	fclose(output);

	output = fopen(output_path, "a");
	if (!output)
		return 4;
	exit_output = output;

	void* shim = dlopen("libsystem_shim.so", RTLD_NOW | RTLD_LOCAL);
	if (!shim)
		return 5;

	void (*register_tlv_exit)(void (*)(void*), void*) =
		(void (*)(void (*)(void*), void*))dlsym(shim, "_tlv_atexit");
	void (*terminate)(int) = (void (*)(int))dlsym(shim, termination_symbol);
	if (!register_tlv_exit || !terminate)
		return 6;

	int (*key_create)(pthread_key_t*, void (*)(void*)) =
		(int (*)(pthread_key_t*, void (*)(void*)))dlsym(shim, "pthread_key_create");
	int (*set_specific)(pthread_key_t, const void*) =
		(int (*)(pthread_key_t, const void*))dlsym(shim, "pthread_setspecific");
	if (!key_create || !set_specific)
		return 7;

	pthread_key_t key;
	if (key_create(&key, record_tsd_exit) != 0)
		return 8;

	register_tlv_exit(record_tlv_exit, "tlv first");
	if (set_specific(key, "tsd") != 0)
		return 9;
	register_tlv_exit(record_tlv_exit, "tlv second");

	terminate(37);
}
