#ifndef _MACHGATE_STARTUP_CACHE_H_
#define _MACHGATE_STARTUP_CACHE_H_

#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>

#define MACHGATE_STARTUP_CACHE_MAGIC   UINT64_C(0x5441474341474348)
#define MACHGATE_STARTUP_CACHE_VERSION 1

struct startup_cache_key {
	uint64_t dev;
	uint64_t ino;
	uint64_t size;
	uint64_t mtime_sec;
	uint64_t mtime_nsec;
	uint64_t has_unwind;
};

struct startup_cache_header {
	uint64_t magic;
	uint32_t version;
	uint32_t header_size;
	struct startup_cache_key key;
	uint64_t eh_frame_size;
	uint64_t hdr_pairs_size;
	uint64_t patch_count;
	uint64_t cie_personality_offset;
	uint32_t fde_count;
	uint32_t reserved;
	uint64_t checksum;
	uint64_t payload_checksum;
};

struct startup_cache_patch {
	uint64_t text_offset;
	uint32_t instruction;
	uint32_t kind;
};

enum startup_cache_patch_kind {
	STARTUP_CACHE_PATCH_RCPC = 1,
	STARTUP_CACHE_PATCH_TPIDRRO = 2,
};

int startup_cache_enabled(void);

int startup_cache_open(struct startup_cache_header* out_header,
                       const void** out_eh_frame,
                       const uint64_t** out_hdr_pairs,
                       const struct startup_cache_patch** out_patches,
                       const char* binary_path,
                       const struct stat* binary_st);

void* startup_cache_map_eh_frame_near(uintptr_t base);
const void* startup_cache_eh_frame_blob(size_t* out_size);
void startup_cache_close(void);

int startup_cache_publish(const void* eh_frame, size_t eh_frame_size,
                          const uint64_t* hdr_pairs, size_t hdr_pairs_size,
                          uint64_t cie_personality_offset,
                          const struct startup_cache_patch* patches,
                          size_t patch_count, uint32_t fde_count,
                          const char* binary_path,
                          const struct stat* binary_st);

#endif
