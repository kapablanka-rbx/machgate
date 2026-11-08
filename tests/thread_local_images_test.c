#include "ports/native_thread_local_image.h"

#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>

static int (*register_image)(const struct machgate_native_thread_local_image*);
static void (*unregister_image)(const void*);
static void* (*resolve_descriptor)(struct machgate_tlv_descriptor*);
static struct machgate_tlv_descriptor descriptors[4];

static void* check_thread(void* argument)
{
	(void)argument;
	unsigned char* first = resolve_descriptor(&descriptors[0]);
	unsigned char* second = resolve_descriptor(&descriptors[3]);
	assert(first && second && first != second);
	assert(first == resolve_descriptor(&descriptors[1]));
	assert(first + 4 == resolve_descriptor(&descriptors[2]));
	assert(((uintptr_t)first & 63) == 3);
	assert(((uintptr_t)second & 63) == 5);
	assert(first[0] == 19 && first[4] == 23 && first[8] == 0);
	assert(second[0] == 73 && second[8] == 0);
	first[0] = 47;
	second[0] = 59;
	return NULL;
}

int main(int argument_count, char** arguments)
{
	assert(argument_count == 2);
	void* provider = dlopen(arguments[1], RTLD_NOW | RTLD_LOCAL);
	assert(provider);
	register_image = dlsym(provider, "machgate_register_thread_local_image");
	unregister_image = dlsym(provider, "machgate_unregister_thread_local_image");
	resolve_descriptor = dlsym(provider, "_tlv_bootstrap");
	assert(register_image && unregister_image && resolve_descriptor);
	_Alignas(64) unsigned char first_template[64] = {0};
	_Alignas(64) unsigned char second_template[64] = {0};
	first_template[3] = 19;
	first_template[7] = 23;
	second_template[5] = 73;
	descriptors[2].offset = 4;
	struct machgate_tlv_descriptor_span first_spans[] = {
		{descriptors, 2}, {descriptors + 2, 1},
	};
	struct machgate_tlv_descriptor_span second_span = {descriptors + 3, 1};
	struct machgate_native_thread_local_image first = {
		.identity = first_template, .initial_data = first_template + 3,
		.initialized_size = 8, .zero_fill_size = 8, .alignment = 64,
		.spans = first_spans, .span_count = 2,
	};
	struct machgate_native_thread_local_image second = {
		.identity = second_template, .initial_data = second_template + 5,
		.initialized_size = 8, .zero_fill_size = 8, .alignment = 64,
		.spans = &second_span, .span_count = 1,
	};
	for (size_t iteration = 0; iteration < 2048; iteration++) {
		assert(register_image(&first) == 0 && register_image(&second) == 0);
		assert(register_image(&first) == -1 && errno == EEXIST);
		assert(descriptors[0].key == descriptors[1].key);
		assert(descriptors[0].key == descriptors[2].key);
		assert(descriptors[0].key != descriptors[3].key);
		check_thread(NULL);
		if (iteration == 0) {
			pthread_t thread;
			assert(pthread_create(&thread, NULL, check_thread, NULL) == 0);
			assert(pthread_join(thread, NULL) == 0);
		}
		assert(*(unsigned char*)resolve_descriptor(&descriptors[0]) == 47);
		assert(*(unsigned char*)resolve_descriptor(&descriptors[3]) == 59);
		unregister_image(first.identity);
		unregister_image(second.identity);
	}
	descriptors[0].offset = 17;
	assert(register_image(&first) == -1 && errno == EINVAL);
	dlclose(provider);
	return 0;
}
