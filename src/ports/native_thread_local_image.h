#ifndef MACHGATE_PORTS_NATIVE_THREAD_LOCAL_IMAGE_H
#define MACHGATE_PORTS_NATIVE_THREAD_LOCAL_IMAGE_H

#include <stddef.h>
#include <stdint.h>

struct machgate_tlv_descriptor {
	void* (*resolver)(struct machgate_tlv_descriptor*);
	uintptr_t key;
	uintptr_t offset;
};

struct machgate_tlv_descriptor_span {
	struct machgate_tlv_descriptor* descriptors;
	size_t count;
};

struct machgate_native_thread_local_image {
	const void* identity;
	const void* initial_data;
	size_t initialized_size;
	size_t zero_fill_size;
	size_t alignment;
	const struct machgate_tlv_descriptor_span* spans;
	size_t span_count;
};

int machgate_register_thread_local_image(
	const struct machgate_native_thread_local_image* image);
void machgate_unregister_thread_local_image(const void* identity);

#endif
