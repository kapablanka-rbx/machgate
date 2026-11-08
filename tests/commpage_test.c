#include "ports/guest_platform.h"
#include "ports/guest_image_view.h"

#include <asm/hwcap.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/auxv.h>
#include <sys/mman.h>

int machgate_shim_guest_ncpu;

int machgate_guest_image_platform(const struct machgate_guest_image* image)
{
	return (int)(uintptr_t)image;
}

int main(void)
{
	uintptr_t addresses[] = {UINT64_C(0x0000000fffffc000), UINT64_C(0x0000000fffff4000)};
	char error[256];
	for (size_t index = 0; index < 2; index++) {
		uint8_t* existing = mmap((void*)addresses[index], 0x4000,
			PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
			-1, 0);
		if (existing == MAP_FAILED)
			return 1;
		existing[0] = 0xa5;
		if (machgate_prepare_guest_platform((void*)1, error, sizeof(error)) == 0 ||
		    existing[0] != 0xa5)
			return 2;
		munmap(existing, 0x4000);
	}
	setenv("MACHGATE_GUEST_NCPU", "3", 1);
	if (machgate_prepare_guest_platform((void*)1, error, sizeof(error)) != 0 ||
	    machgate_prepare_guest_platform((void*)1, error, sizeof(error)) != 0)
		return 3;
	uint64_t capabilities = *(uint64_t*)(addresses[0] + 0x010);
	unsigned long host = getauxval(AT_HWCAP);
	if (!!(capabilities & UINT64_C(0x02000000)) != !!(host & HWCAP_ATOMICS) ||
	    !!(capabilities & UINT64_C(0x0000000100000000)) != !!(host & HWCAP_SHA3) ||
	    machgate_shim_guest_ncpu != 3)
		return 4;
	machgate_release_guest_platform();
	setenv("MACHGATE_GUEST_NCPU", "invalid", 1);
	if (machgate_prepare_guest_platform((void*)2, error, sizeof(error)) != 0 ||
	    *(uint8_t*)(addresses[0] + 0x022) != 6 || machgate_shim_guest_ncpu != 6)
		return 5;
	machgate_release_guest_platform();
	puts("commpage mapping collision, host capability translation and iOS CPU policy passed");
	return 0;
}
