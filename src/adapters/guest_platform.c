#include "ports/guest_platform.h"
#include "ports/guest_image_view.h"

#include <asm/hwcap.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/auxv.h>
#include <sys/mman.h>
#include <sys/sysinfo.h>
#include <unistd.h>

#define DARWIN_COMMPAGE_ADDRESS UINT64_C(0x0000000fffffc000)
#define DARWIN_COMMPAGE_READ_ONLY_ADDRESS UINT64_C(0x0000000fffff4000)
#define DARWIN_COMMPAGE_SIZE 0x4000u
#define DARWIN_PLATFORM_IOS 2
#define DARWIN_PLATFORM_IOS_SIMULATOR 7

static uint8_t* commpage;
static uint8_t* read_only_commpage;

static unsigned guest_cpu_count(int platform)
{
	const char* override = getenv("MACHGATE_GUEST_NCPU");
	if (override && *override) {
		char* end;
		errno = 0;
		long result = strtol(override, &end, 10);
		if (!errno && !*end && result >= 1 && result <= 255)
			return (unsigned)result;
	}
	if (platform == DARWIN_PLATFORM_IOS || platform == DARWIN_PLATFORM_IOS_SIMULATOR)
		return 6;
	long result = sysconf(_SC_NPROCESSORS_ONLN);
	return result < 1 ? 1 : result > 255 ? 255 : (unsigned)result;
}

static uint64_t cpu_capabilities(unsigned cpu_count, unsigned cache_line_size)
{
	static const struct {
		unsigned long host;
		uint64_t guest;
	} features[] = {
		{HWCAP_ASIMD, UINT64_C(0x00000100)},
		{HWCAP_FP, UINT64_C(0x00002400)},
		{HWCAP_FPHP, UINT64_C(0x00000200)},
		{HWCAP_ASIMDFHM, UINT64_C(0x00004000)},
		{HWCAP_ATOMICS, UINT64_C(0x02000000)},
		{HWCAP_CRC32, UINT64_C(0x04000000)},
		{HWCAP_SHA512, UINT64_C(0x80000000)},
		{HWCAP_SHA3, UINT64_C(0x0000000100000000)},
		{HWCAP_FCMA, UINT64_C(0x0000000200000000)},
		{HWCAP_ASIMDDP, UINT64_C(0x0000040000000000)},
		{HWCAP_ASIMDRDM, UINT64_C(0x0000080000000000)},
		{HWCAP_LRCPC, UINT64_C(0x0001000000000000)},
		{HWCAP_ILRCPC, UINT64_C(0x0002000000000000)},
		{HWCAP_JSCVT, UINT64_C(0x0004000000000000)},
		{HWCAP_DCPOP, UINT64_C(0x0010000000000000)},
	};
	unsigned long host = getauxval(AT_HWCAP);
	uint64_t result = (uint64_t)cpu_count << 16;
	if (cpu_count == 1)
		result |= UINT64_C(0x00008000);
	if (cache_line_size == 32 || cache_line_size == 64 || cache_line_size == 128)
		result |= cache_line_size >> 1;
	unsigned long crypto = HWCAP_AES | HWCAP_PMULL | HWCAP_SHA1 | HWCAP_SHA2;
	if ((host & crypto) == crypto)
		result |= UINT64_C(0x01000000);
	if ((host & (HWCAP_FPHP | HWCAP_ASIMDHP)) == (HWCAP_FPHP | HWCAP_ASIMDHP))
		result |= UINT64_C(0x00000008);
	for (size_t index = 0; index < sizeof(features) / sizeof(features[0]); index++)
		if (host & features[index].host)
			result |= features[index].guest;
	return result;
}

static void populate_commpage(unsigned cpu_count)
{
	long page_size = sysconf(_SC_PAGESIZE);
	unsigned page_shift = (unsigned)__builtin_ctzl((unsigned long)page_size);
	long cache_line_size = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
	uint16_t cache_size = cache_line_size > 0 && cache_line_size <= UINT16_MAX
		? (uint16_t)cache_line_size : 0;
	uint64_t capabilities = cpu_capabilities(cpu_count, cache_size);
	uint32_t capabilities32 = (uint32_t)capabilities;
	uint16_t version = 3;
	memcpy(commpage, "commpage arm64", 14);
	memcpy(commpage + 0x010, &capabilities, sizeof(capabilities));
	memcpy(commpage + 0x01e, &version, sizeof(version));
	memcpy(commpage + 0x020, &capabilities32, sizeof(capabilities32));
	memcpy(commpage + 0x026, &cache_size, sizeof(cache_size));
	commpage[0x025] = read_only_commpage[0x025] = (uint8_t)page_shift;
	commpage[0x037] = read_only_commpage[0x037] = (uint8_t)page_shift;
	commpage[0x034] = commpage[0x035] = commpage[0x036] = (uint8_t)cpu_count;
	struct sysinfo information;
	if (sysinfo(&information) == 0) {
		uint64_t memory_size = (uint64_t)information.totalram * information.mem_unit;
		memcpy(commpage + 0x038, &memory_size, sizeof(memory_size));
	}
}

void machgate_release_guest_platform(void)
{
	if (commpage)
		munmap(commpage, DARWIN_COMMPAGE_SIZE);
	if (read_only_commpage)
		munmap(read_only_commpage, DARWIN_COMMPAGE_SIZE);
	commpage = read_only_commpage = NULL;
}

static uint8_t* map_commpage(uintptr_t address)
{
	void* result = mmap((void*)address, DARWIN_COMMPAGE_SIZE,
		PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
		-1, 0);
	if (result == MAP_FAILED)
		return NULL;
	if (result != (void*)address) {
		munmap(result, DARWIN_COMMPAGE_SIZE);
		errno = EEXIST;
		return NULL;
	}
	return result;
}

int machgate_prepare_guest_platform(const struct machgate_guest_image* image,
				   char* error, size_t error_size)
{
	if (commpage)
		return 0;
	long page_size = sysconf(_SC_PAGESIZE);
	if (page_size < 1 || page_size > DARWIN_COMMPAGE_SIZE ||
	    (page_size & (page_size - 1))) {
		snprintf(error, error_size, "host page size cannot map the Darwin commpage");
		return -1;
	}
	unsigned cpu_count = guest_cpu_count(machgate_guest_image_platform(image));
	commpage = map_commpage(DARWIN_COMMPAGE_ADDRESS);
	if (!commpage)
		goto failed;
	read_only_commpage = map_commpage(DARWIN_COMMPAGE_READ_ONLY_ADDRESS);
	if (!read_only_commpage)
		goto failed;
	populate_commpage(cpu_count);
	if (mprotect(commpage, DARWIN_COMMPAGE_SIZE, PROT_READ) != 0 ||
	    mprotect(read_only_commpage, DARWIN_COMMPAGE_SIZE, PROT_READ) != 0)
		goto failed;
	int* provider_cpu_count = dlsym(RTLD_DEFAULT, "machgate_shim_guest_ncpu");
	if (provider_cpu_count)
		*provider_cpu_count = (int)cpu_count;
	return 0;
failed:
	snprintf(error, error_size, "could not prepare Darwin commpage: %s", strerror(errno));
	machgate_release_guest_platform();
	return -1;
}
