#define _GNU_SOURCE
#include "registered_memory_compat.h"

#include <stdatomic.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

struct registered_mapping {
	void* address;
	size_t size;
	struct registered_mapping* next;
};

static struct registered_mapping* mappings;
static atomic_size_t mapping_count;
static atomic_flag mappings_lock = ATOMIC_FLAG_INIT;

static void lock_mappings(void)
{
	while (atomic_flag_test_and_set_explicit(&mappings_lock, memory_order_acquire))
		;
}

static void unlock_mappings(void)
{
	atomic_flag_clear_explicit(&mappings_lock, memory_order_release);
}

void mmap_registry_add(void* address, size_t size)
{
	if (!address || address == MAP_FAILED || !size)
		return;
	lock_mappings();
	for (struct registered_mapping* entry = mappings; entry; entry = entry->next) {
		if (entry->address == address) {
			entry->size = size;
			unlock_mappings();
			return;
		}
	}
	struct registered_mapping* result = (void*)syscall(SYS_mmap, NULL,
		sizeof(*result), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (result == MAP_FAILED) {
		unlock_mappings();
		abort();
	}
	*result = (struct registered_mapping){address, size, mappings};
	mappings = result;
	atomic_fetch_add_explicit(&mapping_count, 1, memory_order_release);
	unlock_mappings();
}

int machgate_release_registered_mapping(void* address)
{
	if (atomic_load_explicit(&mapping_count, memory_order_acquire) == 0)
		return 0;
	lock_mappings();
	for (struct registered_mapping** cursor = &mappings; *cursor;
	     cursor = &(*cursor)->next) {
		struct registered_mapping* entry = *cursor;
		if (entry->address != address)
			continue;
		*cursor = entry->next;
		atomic_fetch_sub_explicit(&mapping_count, 1, memory_order_release);
		long result = syscall(SYS_munmap, entry->address, entry->size);
		syscall(SYS_munmap, entry, sizeof(*entry));
		unlock_mappings();
		if (result != 0)
			abort();
		return 1;
	}
	unlock_mappings();
	return 0;
}
