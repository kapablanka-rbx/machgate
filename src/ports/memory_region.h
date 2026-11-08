#ifndef MACHGATE_MEMORY_REGION_H
#define MACHGATE_MEMORY_REGION_H

#include <stdint.h>

struct machgate_memory_region {
	uint64_t start;
	uint64_t size;
	uint64_t offset;
	int readable;
	int writable;
	int executable;
	int may_read;
	int may_write;
	int may_execute;
	int shared;
	int excluded_from_fork;
	int random_access;
	int sequential_access;
	int locked;
};

__attribute__((visibility("hidden")))
int machgate_query_memory_region(uint64_t address, struct machgate_memory_region* region);

#endif
