#include "ports/memory_region.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern uint32_t mach_task_self_;

#pragma pack(push, 4)
struct darwin_vm_region_basic_info_64 {
	int32_t protection;
	int32_t max_protection;
	int32_t inheritance;
	int32_t shared;
	int32_t reserved;
	uint64_t offset;
	int32_t behavior;
	uint16_t user_wired_count;
};
#pragma pack(pop)

_Static_assert(sizeof(struct darwin_vm_region_basic_info_64) == 36,
	"XNU vm_region_basic_info_64 size");
_Static_assert(offsetof(struct darwin_vm_region_basic_info_64, offset) == 20,
	"XNU vm_region_basic_info_64 offset");

int mach_vm_region(uint32_t target_task, uint64_t* address, uint64_t* size,
	int flavor, void* info, uint32_t* info_count, uint32_t* object_name)
{
	if (target_task != mach_task_self_ || !address || !size || !info ||
	    !info_count || !object_name || (flavor != 9 && flavor != 10) || *info_count < 9)
		return 4;
	struct machgate_memory_region region;
	int result = machgate_query_memory_region(*address, &region);
	if (result <= 0)
		return result == 0 ? 1 : 5;
	struct darwin_vm_region_basic_info_64 basic = {
		.protection = (region.readable ? 1 : 0) | (region.writable ? 2 : 0) |
			(region.executable ? 4 : 0),
		.max_protection = (region.may_read ? 1 : 0) | (region.may_write ? 2 : 0) |
			(region.may_execute ? 4 : 0),
		.inheritance = region.excluded_from_fork ? 2 : region.shared ? 0 : 1,
		.shared = region.shared,
		.offset = region.offset,
		.behavior = region.sequential_access ? 2 : region.random_access ? 1 : 0,
		.user_wired_count = region.locked ? 1 : 0,
	};
	memcpy(info, &basic, sizeof(basic));
	*address = region.start;
	*size = region.size;
	*info_count = 9;
	*object_name = 0;
	return 0;
}

int vm_region_64(uint32_t target_task, uint64_t* address, uint64_t* size,
	int flavor, void* info, uint32_t* info_count, uint32_t* object_name)
{
	return mach_vm_region(target_task, address, size, flavor, info, info_count, object_name);
}
