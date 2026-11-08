#define _GNU_SOURCE

#include "thread_local_compat.h"
#include "ports/native_thread_local_image.h"

#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void* __tlv_image_base;
size_t __tlv_image_size;
size_t __tlv_bss_size;

struct machgate_tlv_exit_callback {
	void (*function)(void*);
	void* argument;
	struct machgate_tlv_exit_callback* next;
};

typedef int (*native_key_create_function)(pthread_key_t*, void (*)(void*));
typedef int (*native_key_delete_function)(pthread_key_t);
typedef int (*native_setspecific_function)(pthread_key_t, const void*);
typedef void* (*native_getspecific_function)(pthread_key_t);
typedef void* (*native_malloc_function)(size_t);
typedef void* (*native_calloc_function)(size_t, size_t);
typedef int (*native_posix_memalign_function)(void**, size_t, size_t);
typedef void (*native_free_function)(void*);

static native_key_create_function native_key_create;
static native_key_delete_function native_key_delete;
static native_setspecific_function native_setspecific;
static native_getspecific_function native_getspecific;
static native_malloc_function native_malloc;
static native_calloc_function native_calloc;
static native_posix_memalign_function native_posix_memalign;
static native_free_function native_free;
static atomic_flag key_creation_lock = ATOMIC_FLAG_INIT;
static _Atomic uintptr_t encoded_exit_callback_key;
static int exit_callback_key_ready;

#define MACHGATE_MAX_THREAD_LOCAL_IMAGES 128u

struct thread_local_image {
	_Atomic uintptr_t encoded_key;
	const void* identity;
	const void* initial_data;
	size_t initialized_size;
	size_t storage_size;
	size_t alignment;
	size_t alignment_offset;
};

static struct thread_local_image thread_local_images[MACHGATE_MAX_THREAD_LOCAL_IMAGES];

void* _tlv_bootstrap(struct machgate_tlv_descriptor* descriptor);

int machgate_configure_tlv_image(void* image_base, size_t image_size,
				 size_t bss_size)
{
	if ((image_size && !image_base) || image_size > SIZE_MAX - bss_size) {
		errno = EINVAL;
		return -1;
	}

	__tlv_image_base = image_base;
	__tlv_image_size = image_size;
	__tlv_bss_size = bss_size;
	return 0;
}

static void lock_key_creation(void)
{
	while (atomic_flag_test_and_set_explicit(&key_creation_lock,
	                                         memory_order_acquire))
		sched_yield();
}

static void unlock_key_creation(void)
{
	atomic_flag_clear_explicit(&key_creation_lock, memory_order_release);
}

static int resolve_native_thread_storage(void)
{
	if (!native_key_create)
		native_key_create = (native_key_create_function)dlsym(
			RTLD_NEXT, "pthread_key_create");
	if (!native_key_delete)
		native_key_delete = (native_key_delete_function)dlsym(
			RTLD_NEXT, "pthread_key_delete");
	if (!native_setspecific)
		native_setspecific = (native_setspecific_function)dlsym(
			RTLD_NEXT, "pthread_setspecific");
	if (!native_getspecific)
		native_getspecific = (native_getspecific_function)dlsym(
			RTLD_NEXT, "pthread_getspecific");
	if (!native_malloc)
		native_malloc = (native_malloc_function)dlsym(RTLD_NEXT, "malloc");
	if (!native_calloc)
		native_calloc = (native_calloc_function)dlsym(RTLD_NEXT, "calloc");
	if (!native_posix_memalign)
		native_posix_memalign = (native_posix_memalign_function)dlsym(
			RTLD_NEXT, "posix_memalign");
	if (!native_free)
		native_free = (native_free_function)dlsym(RTLD_NEXT, "free");
	if (!native_key_create || !native_key_delete || !native_setspecific ||
	    !native_getspecific || !native_malloc || !native_calloc ||
	    !native_posix_memalign || !native_free) {
		errno = ENOSYS;
		return -1;
	}
	return 0;
}

static int valid_thread_local_image(const struct machgate_native_thread_local_image* image)
{
	if (!image || !image->identity || !image->alignment ||
	    (image->alignment & (image->alignment - 1)) ||
	    (image->initialized_size && !image->initial_data) ||
	    image->initialized_size > SIZE_MAX - image->zero_fill_size ||
	    (image->span_count && !image->spans))
		return 0;
	size_t size = image->initialized_size + image->zero_fill_size;
	if (!size || size > SIZE_MAX - image->alignment)
		return 0;
	for (size_t index = 0; index < image->span_count; index++) {
		const struct machgate_tlv_descriptor_span* span = &image->spans[index];
		if (span->count && !span->descriptors)
			return 0;
		for (size_t descriptor = 0; descriptor < span->count; descriptor++)
			if (span->descriptors[descriptor].offset > size)
				return 0;
	}
	return 1;
}

int machgate_register_thread_local_image(const struct machgate_native_thread_local_image* image)
{
	if (!valid_thread_local_image(image)) {
		errno = EINVAL;
		return -1;
	}
	lock_key_creation();
	struct thread_local_image* available = NULL;
	for (size_t index = 0; index < MACHGATE_MAX_THREAD_LOCAL_IMAGES; index++) {
		struct thread_local_image* candidate = &thread_local_images[index];
		if (!atomic_load_explicit(&candidate->encoded_key, memory_order_relaxed)) {
			if (!available)
				available = candidate;
		} else if (candidate->identity == image->identity) {
			unlock_key_creation();
			errno = EEXIST;
			return -1;
		}
	}
	if (!available || resolve_native_thread_storage() != 0) {
		unlock_key_creation();
		if (!available)
			errno = ENOMEM;
		return -1;
	}
	pthread_key_t key;
	int result = native_key_create(&key, native_free);
	if (result != 0) {
		unlock_key_creation();
		errno = result;
		return -1;
	}
	available->identity = image->identity;
	available->initial_data = image->initial_data;
	available->initialized_size = image->initialized_size;
	available->storage_size = image->initialized_size + image->zero_fill_size;
	available->alignment = image->alignment < sizeof(void*) ? sizeof(void*) : image->alignment;
	available->alignment_offset = (uintptr_t)image->initial_data & (image->alignment - 1);
	uintptr_t encoded = (uintptr_t)key + 1;
	atomic_store_explicit(&available->encoded_key, encoded, memory_order_release);
	for (size_t index = 0; index < image->span_count; index++) {
		const struct machgate_tlv_descriptor_span* span = &image->spans[index];
		for (size_t descriptor = 0; descriptor < span->count; descriptor++) {
			span->descriptors[descriptor].resolver = _tlv_bootstrap;
			__atomic_store_n(&span->descriptors[descriptor].key, encoded, __ATOMIC_RELEASE);
		}
	}
	unlock_key_creation();
	return 0;
}

void machgate_unregister_thread_local_image(const void* identity)
{
	lock_key_creation();
	for (size_t index = 0; index < MACHGATE_MAX_THREAD_LOCAL_IMAGES; index++) {
		struct thread_local_image* image = &thread_local_images[index];
		uintptr_t encoded = atomic_load_explicit(&image->encoded_key, memory_order_relaxed);
		if (!encoded || image->identity != identity)
			continue;
		pthread_key_t key = (pthread_key_t)(encoded - 1);
		void* storage = native_getspecific(key);
		native_setspecific(key, NULL);
		native_key_delete(key);
		native_free(storage);
		atomic_store_explicit(&image->encoded_key, 0, memory_order_release);
		break;
	}
	unlock_key_creation();
}

static const struct thread_local_image* registered_thread_local_image(pthread_key_t key)
{
	uintptr_t encoded = (uintptr_t)key + 1;
	for (size_t index = 0; index < MACHGATE_MAX_THREAD_LOCAL_IMAGES; index++)
		if (atomic_load_explicit(&thread_local_images[index].encoded_key,
		                         memory_order_acquire) == encoded)
			return &thread_local_images[index];
	return NULL;
}

static void* allocate_registered_storage(const struct thread_local_image* image)
{
	void* result = NULL;
	int error = native_posix_memalign(&result, image->alignment,
	                                 image->storage_size + image->alignment_offset);
	if (error != 0) {
		errno = error;
		return NULL;
	}
	unsigned char* base = (unsigned char*)result + image->alignment_offset;
	memset(base, 0, image->storage_size);
	if (image->initialized_size)
		memcpy(base, image->initial_data, image->initialized_size);
	return result;
}

static int descriptor_storage_key(struct machgate_tlv_descriptor* descriptor,
	                              pthread_key_t* output)
{
	uintptr_t encoded = __atomic_load_n(&descriptor->key, __ATOMIC_ACQUIRE);

	if (encoded) {
		*output = (pthread_key_t)(encoded - 1);
		return 0;
	}

	lock_key_creation();
	encoded = __atomic_load_n(&descriptor->key, __ATOMIC_RELAXED);
	if (!encoded) {
		pthread_key_t new_key;
		int result = resolve_native_thread_storage();

		if (result == 0)
			result = native_key_create(&new_key, native_free);
		if (result != 0) {
			if (result > 0)
				errno = result;
			unlock_key_creation();
			return -1;
		}
		encoded = (uintptr_t)new_key + 1;
		__atomic_store_n(&descriptor->key, encoded, __ATOMIC_RELEASE);
	}
	unlock_key_creation();
	*output = (pthread_key_t)(encoded - 1);
	return 0;
}

static size_t required_storage_size(const struct machgate_tlv_descriptor* descriptor)
{
	if (__tlv_image_size > SIZE_MAX - __tlv_bss_size)
		return 0;

	size_t result = __tlv_image_size + __tlv_bss_size;
	if (descriptor->offset >= result) {
		if (descriptor->offset == UINTPTR_MAX)
			return 0;
		result = (size_t)descriptor->offset + 1;
	}
	if (result < 4096)
		result = 4096;
	return result;
}

void* _tlv_bootstrap_impl(struct machgate_tlv_descriptor* descriptor)
{
	if (!descriptor) {
		errno = EINVAL;
		return NULL;
	}

	pthread_key_t storage_key;
	if (descriptor_storage_key(descriptor, &storage_key) != 0)
		return NULL;
	const struct thread_local_image* image = registered_thread_local_image(storage_key);
	if (image && descriptor->offset > image->storage_size) {
		errno = EINVAL;
		return NULL;
	}

	void* storage = native_getspecific(storage_key);
	if (!storage) {
		size_t storage_size = image ? image->storage_size : required_storage_size(descriptor);

		if (!storage_size) {
			errno = EOVERFLOW;
			return NULL;
		}
		storage = image ? allocate_registered_storage(image) : native_calloc(1, storage_size);
		if (!storage)
			return NULL;
		if (!image && __tlv_image_base && __tlv_image_size)
			memcpy(storage, __tlv_image_base, __tlv_image_size);
		int result = native_setspecific(storage_key, storage);
		if (result != 0) {
			native_free(storage);
			errno = result;
			return NULL;
		}
	}
	return (unsigned char*)storage + (image ? image->alignment_offset : 0) + descriptor->offset;
}

void machgate_run_tlv_exit_callbacks(void)
{
	struct machgate_tlv_exit_callback* callback = NULL;

	if (resolve_native_thread_storage() != 0)
		return;
	uintptr_t encoded = atomic_load_explicit(&encoded_exit_callback_key,
	                                         memory_order_acquire);
	if (encoded) {
		pthread_key_t key = (pthread_key_t)(encoded - 1);
		callback = native_getspecific(key);
		native_setspecific(key, NULL);
	}
	while (callback) {
		struct machgate_tlv_exit_callback* next = callback->next;
		callback->function(callback->argument);
		native_free(callback);
		callback = next;
	}
}

static void run_exit_callbacks(void* list_pointer)
{
	struct machgate_tlv_exit_callback* callback = list_pointer;

	while (callback) {
		struct machgate_tlv_exit_callback* next = callback->next;
		callback->function(callback->argument);
		native_free(callback);
		callback = next;
	}
}

static int ensure_exit_callback_key(void)
{
	if (__atomic_load_n(&exit_callback_key_ready, __ATOMIC_ACQUIRE))
		return 0;

	lock_key_creation();
	if (!exit_callback_key_ready) {
		int result = resolve_native_thread_storage();

		pthread_key_t new_key;
		if (result == 0)
			result = native_key_create(&new_key, run_exit_callbacks);
		if (result == 0)
			atomic_store_explicit(&encoded_exit_callback_key,
			                      (uintptr_t)new_key + 1, memory_order_release);
		if (result != 0) {
			if (result > 0)
				errno = result;
			unlock_key_creation();
			return -1;
		}
		__atomic_store_n(&exit_callback_key_ready, 1, __ATOMIC_RELEASE);
	}
	unlock_key_creation();
	return 0;
}

void _tlv_atexit(void (*function)(void*), void* argument)
{
	if (!function || ensure_exit_callback_key() != 0)
		return;

	struct machgate_tlv_exit_callback* callback = native_malloc(sizeof(*callback));
	if (!callback)
		return;
	callback->function = function;
	callback->argument = argument;
	pthread_key_t key = (pthread_key_t)(
		atomic_load_explicit(&encoded_exit_callback_key, memory_order_acquire) - 1);
	callback->next = native_getspecific(key);
	if (native_setspecific(key, callback) != 0)
		native_free(callback);
}
