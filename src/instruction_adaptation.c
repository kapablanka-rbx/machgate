#include "instruction_adaptation.h"

#include "arm64_enc.h"
#include "loader.h"
#include "macho_defs.h"
#include "ports/guest_image_view.h"
#include "adapters/startup_cache.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

#define ARM64_REGISTER_MASK 0x0000001fu
#define ARM64_MRS_SYSTEM_REGISTER_MASK 0xffffffe0u
#define ARM64_MRS_TPIDRRO_EL0 0xd53bd060u
#define ARM64_LDR_LITERAL_64 0x58000000u
#define ARM64_MOVZ_64_SHIFT_16 0xd2a00000u
#define THREAD_CONTEXT_SIZE (16u * 1024u)
#define THREAD_CONTEXT_ADDRESS_STEP UINT64_C(0x10000)
#define ADAPTATION_ISLAND_WORDS 4u
#define ADAPTATION_POOL_SIZE (64u * 1024u)
#define ADAPTATION_POOL_ALIGNMENT (1024u * 1024u)
#define ADAPTATION_MAX_POOLS 64u

struct adaptation_pool {
	uint32_t* base;
	uint32_t* next;
	uint32_t* end;
};

static _Alignas(THREAD_CONTEXT_SIZE)
    unsigned char fallback_thread_context[THREAD_CONTEXT_SIZE];
static unsigned char* guest_thread_context = fallback_thread_context;
static uint32_t thread_context_immediate;
static int thread_context_initialized;
static struct adaptation_pool adaptation_pools[ADAPTATION_MAX_POOLS];
static size_t adaptation_pool_count;

void* machgate_guest_thread_context(void)
{
	return guest_thread_context;
}

static void initialize_thread_context(void)
{
	if (thread_context_initialized)
		return;
	for (uintptr_t address = THREAD_CONTEXT_ADDRESS_STEP;
	     address <= UINT32_MAX; address += THREAD_CONTEXT_ADDRESS_STEP) {
		void* mapping = mmap((void*)address, THREAD_CONTEXT_SIZE,
		                     PROT_READ | PROT_WRITE,
		                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
		                     -1, 0);
		if (mapping == MAP_FAILED)
			continue;
		if ((uintptr_t)mapping != address) {
			munmap(mapping, THREAD_CONTEXT_SIZE);
			continue;
		}
		guest_thread_context = mapping;
		thread_context_immediate = ARM64_MOVZ_64_SHIFT_16 |
		                           ((uint32_t)(address >> 16) << 5);
		break;
	}
	memset(guest_thread_context, 0, THREAD_CONTEXT_SIZE);
	*(uintptr_t*)guest_thread_context = (uintptr_t)guest_thread_context;
	thread_context_initialized = 1;
}

static int section_is_executable(const struct section_64* section)
{
	return section->flags &
	       (S_ATTR_PURE_INSTRUCTIONS | S_ATTR_SOME_INSTRUCTIONS);
}

static int section_range_is_valid(const struct segment_command_64* segment,
				  const struct section_64* section)
{
	if (section->addr < segment->vmaddr)
		return 0;
	uint64_t section_offset = section->addr - segment->vmaddr;
	return section_offset <= segment->vmsize &&
	       section->size <= segment->vmsize - section_offset;
}

static int island_reachable(const uint32_t* instruction,
			    const uint32_t* island)
{
	return arm64_branch_in_range((int64_t)(uintptr_t)island -
	                             (int64_t)(uintptr_t)instruction);
}

static struct adaptation_pool* register_pool(void* mapping, size_t size)
{
	if (adaptation_pool_count == ADAPTATION_MAX_POOLS)
		return NULL;
	struct adaptation_pool* result =
	    &adaptation_pools[adaptation_pool_count++];
	result->base = mapping;
	result->next = mapping;
	result->end = (uint32_t*)((char*)mapping + size);
	return result;
}

static struct adaptation_pool* allocate_pool_near(uint32_t* instruction)
{
	uintptr_t origin = (uintptr_t)instruction &
	                   ~((uintptr_t)ADAPTATION_POOL_ALIGNMENT - 1);
	for (uintptr_t distance = ADAPTATION_POOL_ALIGNMENT;
	     distance < (UINT64_C(1) << 27);
	     distance += ADAPTATION_POOL_ALIGNMENT) {
		uintptr_t candidates[2] = {0};
		if (origin >= distance)
			candidates[0] = origin - distance;
		if (origin <= UINTPTR_MAX - distance)
			candidates[1] = origin + distance;

		for (size_t index = 0; index < 2; index++) {
			if (!candidates[index] ||
			    !arm64_branch_in_range(
				    (int64_t)candidates[index] -
				    (int64_t)(uintptr_t)instruction))
				continue;
			void* mapping = mmap(
			    (void*)candidates[index], ADAPTATION_POOL_SIZE,
			    PROT_READ | PROT_WRITE,
			    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
			    -1, 0);
			if (mapping == MAP_FAILED)
				continue;
			struct adaptation_pool* result =
			    register_pool(mapping, ADAPTATION_POOL_SIZE);
			if (result)
				return result;
			munmap(mapping, ADAPTATION_POOL_SIZE);
			return NULL;
		}
	}
	return NULL;
}

static struct adaptation_pool* find_pool(uint32_t* instruction)
{
	for (size_t index = 0; index < adaptation_pool_count; index++) {
		struct adaptation_pool* pool = &adaptation_pools[index];
		if ((size_t)(pool->end - pool->next) >=
		        ADAPTATION_ISLAND_WORDS &&
		    island_reachable(instruction, pool->next))
			return pool;
	}
	return allocate_pool_near(instruction);
}

static int adapt_thread_pointer_read(uint32_t* instruction, uint32_t encoding,
				     char* error, size_t error_size)
{
	uint32_t destination_register = encoding & ARM64_REGISTER_MASK;
	if (thread_context_immediate) {
		*instruction = thread_context_immediate | destination_register;
		return 0;
	}
	struct adaptation_pool* pool = find_pool(instruction);
	if (!pool) {
		snprintf(error, error_size,
		         "could not allocate a nearby instruction-adaptation island");
		return -1;
	}

	uint32_t* island = pool->next;
	int64_t return_distance = (int64_t)(uintptr_t)instruction -
	                          (int64_t)(uintptr_t)island;
	int64_t island_distance = -return_distance;
	island[0] = ARM64_LDR_LITERAL_64 | (2u << 5) | destination_register;
	island[1] = enc_b((int32_t)(return_distance / 4));
	uintptr_t context = (uintptr_t)guest_thread_context;
	memcpy(&island[2], &context, sizeof(context));
	*instruction = enc_b((int32_t)(island_distance / 4));
	pool->next += ADAPTATION_ISLAND_WORDS;
	return 0;
}

struct adaptation_error {
	char* message;
	size_t size;
};

static int adapt_cached_instruction(uint32_t* instruction, void* context)
{
	struct adaptation_error* error = context;
	return adapt_thread_pointer_read(instruction, *instruction,
					error->message, error->size);
}

static int adapt_section(const struct machgate_guest_image* image,
			 const struct segment_command_64* segment,
			 const struct section_64* section, char* error,
			 size_t error_size)
{
	if (!section_is_executable(section))
		return 0;
	if (!section_range_is_valid(segment, section)) {
		snprintf(error, error_size,
		         "Mach-O executable section lies outside its segment");
		return -1;
	}

	uint32_t* code = (void*)(uintptr_t)(section->addr + machgate_guest_image_slide(image));
	struct adaptation_error context = {error, error_size};
	return machgate_startup_cache_patch(machgate_guest_image_startup_cache(image), MACHGATE_CACHE_THREAD_POINTERS,
		code, section->size, ARM64_MRS_SYSTEM_REGISTER_MASK,
		ARM64_MRS_TPIDRRO_EL0, adapt_cached_instruction, &context);
}

static int protect_adaptation_pools(char* error, size_t error_size)
{
	for (size_t index = 0; index < adaptation_pool_count; index++) {
		struct adaptation_pool* pool = &adaptation_pools[index];
		__builtin___clear_cache((char*)pool->base, (char*)pool->next);
		if (mprotect(pool->base, ADAPTATION_POOL_SIZE,
		             PROT_READ | PROT_EXEC) != 0) {
			snprintf(error, error_size,
			         "could not protect an instruction-adaptation island: %s",
			         strerror(errno));
			return -1;
		}
	}
	return 0;
}

int machgate_adapt_guest_instructions(struct machgate_guest_image* image,
				      char* error, size_t error_size)
{
	struct mach_header_64* header = machgate_guest_image_validated_header(image);
	uint8_t* cursor = (uint8_t*)(header + 1);
	int result = 0;

	adaptation_pool_count = 0;
	initialize_thread_context();

	for (uint32_t command_index = 0;
	     command_index < header->ncmds;
	     command_index++) {
		struct load_command* command = (void*)cursor;
		if (command->cmd == LC_SEGMENT_64) {
			struct segment_command_64* segment = (void*)command;
			if (segment->initprot & VM_PROT_EXECUTE) {
				struct section_64* sections = (void*)(segment + 1);
				for (uint32_t section_index = 0;
				     section_index < segment->nsects;
				     section_index++) {
					int section_result = adapt_section(
						image, segment, &sections[section_index],
						error, error_size);
					if (section_result < 0)
						return -1;
					result += section_result;
				}
			}
		}
		cursor += command->cmdsize;
	}

	if (protect_adaptation_pools(error, error_size) != 0)
		return -1;
	return result;
}
