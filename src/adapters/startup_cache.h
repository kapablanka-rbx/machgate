#ifndef MACHGATE_STARTUP_CACHE_H
#define MACHGATE_STARTUP_CACHE_H

#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>

enum machgate_startup_cache_part {
	MACHGATE_CACHE_THREAD_POINTERS,
	MACHGATE_CACHE_SYSCALLS,
	MACHGATE_CACHE_UNWIND,
	MACHGATE_CACHE_CALL_REFERENCES,
	MACHGATE_CACHE_PART_COUNT
};

struct machgate_startup_cache;

struct machgate_startup_cache* machgate_startup_cache_begin(const char* path, const struct stat* status,
				 uint64_t slice_offset, uint64_t slice_size);
const void* machgate_startup_cache_get(struct machgate_startup_cache* cache, enum machgate_startup_cache_part part,
				     size_t* size);
void machgate_startup_cache_put(struct machgate_startup_cache* cache, enum machgate_startup_cache_part part,
			       const void* data, size_t size);
int machgate_startup_cache_patch(struct machgate_startup_cache* cache, enum machgate_startup_cache_part part,
				uint32_t* code, size_t size, uint32_t mask,
				uint32_t encoding,
				int (*patch)(uint32_t*, void*), void* context);
void machgate_startup_cache_publish(struct machgate_startup_cache* cache);
void machgate_startup_cache_close(struct machgate_startup_cache* cache);

#endif
