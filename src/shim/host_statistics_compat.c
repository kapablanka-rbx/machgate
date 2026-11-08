#define _GNU_SOURCE

#include "host_facts.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <sys/times.h>
#include <time.h>
#include <unistd.h>

extern uint32_t mach_task_self_;

enum {
	DARWIN_KERN_SUCCESS = 0,
	DARWIN_KERN_INVALID_ARGUMENT = 4,
	DARWIN_KERN_FAILURE = 5,
	DARWIN_KERN_NOT_SUPPORTED = 46,
	DARWIN_HOST_LOAD_INFO = 1,
	DARWIN_HOST_VM_INFO = 2,
	DARWIN_HOST_CPU_LOAD_INFO = 3,
	DARWIN_HOST_VM_INFO64 = 4,
	DARWIN_HOST_EXTMOD_INFO64 = 5,
	DARWIN_TASK_BASIC_INFO_32 = 4,
	DARWIN_TASK_BASIC_INFO_64 = 5,
	DARWIN_TASK_BASIC2_INFO_32 = 6,
	DARWIN_TASK_BASIC_INFO_64_2 = 18,
	DARWIN_MACH_TASK_BASIC_INFO = 20,
	DARWIN_TASK_VM_INFO = 22,
	DARWIN_LOAD_SCALE = 1000,
	DARWIN_CPU_STATE_MAX = 4,
	DARWIN_CPU_STATE_COUNT = 3,
};

#pragma pack(push, 4)

struct darwin_time_value {
	int32_t seconds;
	int32_t microseconds;
};

struct darwin_task_basic_info_32 {
	int32_t suspend_count;
	uint32_t virtual_size;
	uint32_t resident_size;
	struct darwin_time_value user_time;
	struct darwin_time_value system_time;
	int32_t policy;
};

struct darwin_task_basic_info_64 {
	int32_t suspend_count;
	uint64_t virtual_size;
	uint64_t resident_size;
	struct darwin_time_value user_time;
	struct darwin_time_value system_time;
	int32_t policy;
};

struct darwin_mach_task_basic_info {
	uint64_t virtual_size;
	uint64_t resident_size;
	uint64_t resident_size_max;
	struct darwin_time_value user_time;
	struct darwin_time_value system_time;
	int32_t policy;
	int32_t suspend_count;
};

struct darwin_task_vm_info_rev1 {
	uint64_t virtual_size;
	int32_t region_count;
	int32_t page_size;
	uint64_t resident_size;
	uint64_t resident_size_peak;
	uint64_t device;
	uint64_t device_peak;
	uint64_t internal;
	uint64_t internal_peak;
	uint64_t external;
	uint64_t external_peak;
	uint64_t reusable;
	uint64_t reusable_peak;
	uint64_t purgeable_volatile_pmap;
	uint64_t purgeable_volatile_resident;
	uint64_t purgeable_volatile_virtual;
	uint64_t compressed;
	uint64_t compressed_peak;
	uint64_t compressed_lifetime;
	uint64_t phys_footprint;
};

struct darwin_host_load_info {
	int32_t avenrun[3];
	int32_t mach_factor[3];
};

struct darwin_vm_statistics {
	uint32_t free_count;
	uint32_t active_count;
	uint32_t inactive_count;
	uint32_t wire_count;
	uint32_t zero_fill_count;
	uint32_t reactivations;
	uint32_t pageins;
	uint32_t pageouts;
	uint32_t faults;
	uint32_t cow_faults;
	uint32_t lookups;
	uint32_t hits;
	uint32_t purgeable_count;
	uint32_t purges;
	uint32_t speculative_count;
};

struct darwin_vm_statistics64 {
	uint32_t free_count;
	uint32_t active_count;
	uint32_t inactive_count;
	uint32_t wire_count;
	uint64_t zero_fill_count;
	uint64_t reactivations;
	uint64_t pageins;
	uint64_t pageouts;
	uint64_t faults;
	uint64_t cow_faults;
	uint64_t lookups;
	uint64_t hits;
	uint64_t purges;
	uint32_t purgeable_count;
	uint32_t speculative_count;
	uint64_t decompressions;
	uint64_t compressions;
	uint64_t swapins;
	uint64_t swapouts;
	uint32_t compressor_page_count;
	uint32_t throttled_count;
	uint32_t external_page_count;
	uint32_t internal_page_count;
	uint64_t total_uncompressed_pages_in_compressor;
	uint64_t swapped_count;
};

#pragma pack(pop)

_Static_assert(sizeof(struct darwin_task_basic_info_32) == 32,
	       "Darwin task_basic_info_32 layout changed");
_Static_assert(sizeof(struct darwin_task_basic_info_64) == 40,
	       "Darwin task_basic_info_64 layout changed");
_Static_assert(sizeof(struct darwin_mach_task_basic_info) == 48,
	       "Darwin mach_task_basic_info layout changed");
_Static_assert(sizeof(struct darwin_task_vm_info_rev1) == 152,
	       "Darwin task_vm_info revision 1 layout changed");
_Static_assert(offsetof(struct darwin_task_vm_info_rev1, phys_footprint) == 144,
	       "Darwin task_vm_info revision 0 layout changed");
_Static_assert(sizeof(struct darwin_vm_statistics) == 60,
	       "Darwin vm_statistics layout changed");
_Static_assert(sizeof(struct darwin_vm_statistics64) == 160,
	       "Darwin vm_statistics64 layout changed");

static uint32_t truncate_page_count(uint64_t count)
{
	return count > UINT32_MAX ? UINT32_MAX : (uint32_t)count;
}

static int copy_counted_output(const void* source, size_t source_size,
	                       void* output, uint32_t* count)
{
	uint32_t source_count = (uint32_t)(source_size / sizeof(uint32_t));

	if (!output || !count)
		return DARWIN_KERN_INVALID_ARGUMENT;
	if (*count < source_count)
		return DARWIN_KERN_FAILURE;

	memcpy(output, source, source_size);
	*count = source_count;
	return DARWIN_KERN_SUCCESS;
}

static int copy_limited_counted_output(const void* source, size_t source_size,
	                               size_t minimum_size, void* output,
	                               uint32_t* count)
{
	uint32_t source_count = (uint32_t)(source_size / sizeof(uint32_t));
	uint32_t minimum_count = (uint32_t)(minimum_size / sizeof(uint32_t));
	uint32_t copy_count;

	if (!output || !count)
		return DARWIN_KERN_INVALID_ARGUMENT;
	if (*count < minimum_count)
		return DARWIN_KERN_FAILURE;

	copy_count = *count < source_count ? *count : source_count;
	memcpy(output, source, (size_t)copy_count * sizeof(uint32_t));
	*count = copy_count;
	return DARWIN_KERN_SUCCESS;
}

static void time_value_from_microseconds(uint64_t microseconds,
	                                 struct darwin_time_value* value)
{
	value->seconds = (int32_t)(microseconds / 1000000);
	value->microseconds = (int32_t)(microseconds % 1000000);
}

static int read_process_times(struct darwin_time_value* user_time,
	                      struct darwin_time_value* system_time)
{
	struct tms process_times;
	long ticks_per_second = sysconf(_SC_CLK_TCK);

	if (times(&process_times) == (clock_t)-1 || ticks_per_second < 1)
		return DARWIN_KERN_FAILURE;

	time_value_from_microseconds(
		(uint64_t)process_times.tms_utime * 1000000 / ticks_per_second,
		user_time);
	time_value_from_microseconds(
		(uint64_t)process_times.tms_stime * 1000000 / ticks_per_second,
		system_time);
	return DARWIN_KERN_SUCCESS;
}

static int read_current_process_memory(uint64_t* virtual_size,
	                               uint64_t* resident_size,
	                               uint64_t* resident_size_max)
{
	FILE* status = fopen("/proc/self/status", "r");

	if (!status)
		return 0;

	char line[256];
	while (fgets(line, sizeof(line), status)) {
		unsigned long long kilobytes;
		if (sscanf(line, "VmSize: %llu kB", &kilobytes) == 1)
			*virtual_size = kilobytes * 1024;
		else if (sscanf(line, "VmRSS: %llu kB", &kilobytes) == 1)
			*resident_size = kilobytes * 1024;
		else if (sscanf(line, "VmHWM: %llu kB", &kilobytes) == 1)
			*resident_size_max = kilobytes * 1024;
	}
	fclose(status);
	return *resident_size != 0;
}

static int fill_task_basic_info_32(uint32_t flavor, void* output,
	                           uint32_t* count)
{
	struct darwin_task_basic_info_32 info = { 0 };
	uint64_t virtual_size = 0;
	uint64_t resident_size = 0;
	uint64_t resident_size_max = 0;
	int result = read_process_times(&info.user_time, &info.system_time);

	if (result != DARWIN_KERN_SUCCESS)
		return result;
	read_current_process_memory(&virtual_size, &resident_size,
	                            &resident_size_max);
	if (flavor == DARWIN_TASK_BASIC2_INFO_32)
		resident_size = resident_size_max;
	info.virtual_size = virtual_size > UINT32_MAX ? UINT32_MAX :
		(uint32_t)virtual_size;
	info.resident_size = resident_size > UINT32_MAX ? UINT32_MAX :
		(uint32_t)resident_size;
	info.policy = 1;
	return copy_counted_output(&info, sizeof(info), output, count);
}

static int fill_task_basic_info_64(void* output, uint32_t* count)
{
	struct darwin_task_basic_info_64 info = { 0 };
	int result = read_process_times(&info.user_time, &info.system_time);

	if (result != DARWIN_KERN_SUCCESS)
		return result;
	read_current_process_memory(&info.virtual_size, &info.resident_size,
	                            &info.resident_size);
	info.policy = 1;
	return copy_counted_output(&info, sizeof(info), output, count);
}

static int fill_mach_task_basic_info(void* output, uint32_t* count)
{
	struct darwin_mach_task_basic_info info = { 0 };
	int result = read_process_times(&info.user_time, &info.system_time);

	if (result != DARWIN_KERN_SUCCESS)
		return result;
	read_current_process_memory(&info.virtual_size, &info.resident_size,
	                            &info.resident_size_max);
	info.policy = 1;
	return copy_counted_output(&info, sizeof(info), output, count);
}

static int fill_task_vm_info(void* output, uint32_t* count)
{
	struct darwin_task_vm_info_rev1 info = { 0 };
	size_t revision_zero_size = offsetof(
		struct darwin_task_vm_info_rev1, phys_footprint);

	if (!output || !count || *count < revision_zero_size / sizeof(uint32_t))
		return DARWIN_KERN_INVALID_ARGUMENT;
	if (!read_current_process_memory(&info.virtual_size, &info.resident_size,
	                                 &info.resident_size_peak))
		return DARWIN_KERN_FAILURE;

	info.page_size = (int32_t)machgate_host_page_size();
	info.internal = info.resident_size;
	info.internal_peak = info.resident_size_peak;
	info.phys_footprint = info.resident_size;
	size_t output_size = *count >= sizeof(info) / sizeof(uint32_t) ?
		sizeof(info) : revision_zero_size;
	return copy_counted_output(&info, output_size, output, count);
}

int task_info(uint32_t target_task, uint32_t flavor, void* output,
              uint32_t* count)
{
	if (target_task != mach_task_self_)
		return DARWIN_KERN_INVALID_ARGUMENT;

	switch (flavor) {
	case DARWIN_TASK_BASIC_INFO_32:
	case DARWIN_TASK_BASIC2_INFO_32:
	case DARWIN_TASK_BASIC_INFO_64:
		return fill_task_basic_info_32(flavor, output, count);
	case DARWIN_TASK_BASIC_INFO_64_2:
		return fill_task_basic_info_64(output, count);
	case DARWIN_MACH_TASK_BASIC_INFO:
		return fill_mach_task_basic_info(output, count);
	case DARWIN_TASK_VM_INFO:
		return fill_task_vm_info(output, count);
	default:
		return DARWIN_KERN_NOT_SUPPORTED;
	}
}

static int read_host_vm_statistics64(struct darwin_vm_statistics64* info)
{
	struct sysinfo host_info;
	uint64_t free_pages;
	uint64_t active_pages;
	uint64_t page_size = (uint64_t)machgate_host_page_size();

	if (sysinfo(&host_info) != 0)
		return DARWIN_KERN_FAILURE;

	memset(info, 0, sizeof(*info));
	uint64_t memory_unit = host_info.mem_unit ? host_info.mem_unit : 1;
	free_pages = ((uint64_t)host_info.freeram * memory_unit + page_size - 1) /
		page_size;
	active_pages = (((uint64_t)host_info.totalram - host_info.freeram) *
	                memory_unit + page_size - 1) / page_size;
	info->free_count = truncate_page_count(free_pages);
	info->active_count = truncate_page_count(active_pages);
	return DARWIN_KERN_SUCCESS;
}

static int fill_host_vm_statistics(void* output, uint32_t* count)
{
	struct darwin_vm_statistics64 statistics64;
	struct darwin_vm_statistics statistics = { 0 };
	int result = read_host_vm_statistics64(&statistics64);

	if (result != DARWIN_KERN_SUCCESS)
		return result;
	statistics.free_count = statistics64.free_count;
	statistics.active_count = statistics64.active_count;
	statistics.inactive_count = statistics64.inactive_count;
	statistics.wire_count = statistics64.wire_count;
	return copy_limited_counted_output(&statistics, sizeof(statistics),
	                                   48, output, count);
}

static int fill_host_vm_statistics64(void* output, uint32_t* count)
{
	struct darwin_vm_statistics64 statistics;
	int result = read_host_vm_statistics64(&statistics);

	if (result != DARWIN_KERN_SUCCESS)
		return result;
	return copy_limited_counted_output(&statistics, sizeof(statistics),
	                                   88, output, count);
}

static int fill_host_load_info(void* output, uint32_t* count)
{
	struct darwin_host_load_info info = { 0 };
	double load_averages[3];

	if (getloadavg(load_averages, DARWIN_CPU_STATE_COUNT) !=
	    DARWIN_CPU_STATE_COUNT)
		return DARWIN_KERN_FAILURE;

	for (size_t index = 0; index < DARWIN_CPU_STATE_COUNT; index++)
		info.avenrun[index] = (int32_t)(load_averages[index] *
		                                DARWIN_LOAD_SCALE);
	return copy_counted_output(&info, sizeof(info), output, count);
}

int host_statistics(uint32_t host, int flavor, void* output, uint32_t* count)
{
	(void)host;

	switch (flavor) {
	case DARWIN_HOST_LOAD_INFO:
		return fill_host_load_info(output, count);
	case DARWIN_HOST_VM_INFO:
		return fill_host_vm_statistics(output, count);
	case DARWIN_HOST_CPU_LOAD_INFO: {
		uint32_t ticks[DARWIN_CPU_STATE_MAX] = { 0 };
		return copy_counted_output(ticks, sizeof(ticks), output, count);
	}
	default:
		return DARWIN_KERN_INVALID_ARGUMENT;
	}
}

int host_statistics64(uint32_t host, int flavor, void* output, uint32_t* count)
{
	if (flavor == DARWIN_HOST_VM_INFO64)
		return fill_host_vm_statistics64(output, count);
	if (flavor == DARWIN_HOST_EXTMOD_INFO64) {
		uint64_t info[6] = { 0 };
		return copy_counted_output(info, sizeof(info), output, count);
	}
	return host_statistics(host, flavor, output, count);
}

int host_page_size(uint32_t host, uint64_t* page_size)
{
	(void)host;

	if (!page_size)
		return DARWIN_KERN_INVALID_ARGUMENT;
	*page_size = (uint64_t)machgate_host_page_size();
	return DARWIN_KERN_SUCCESS;
}
