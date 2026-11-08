#include "instruction_adaptation.h"
#include "macho_defs.h"
#include "ports/guest_image_view.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static struct {
	struct mach_header_64 header;
	struct segment_command_64 segment;
	struct section_64 section;
} guest;

void* machgate_guest_image_validated_header(const struct machgate_guest_image* image)
{
	(void)image;
	return &guest.header;
}

intptr_t machgate_guest_image_slide(const struct machgate_guest_image* image)
{
	(void)image;
	return 0;
}

struct machgate_startup_cache* machgate_guest_image_startup_cache(
	const struct machgate_guest_image* image)
{
	(void)image;
	return NULL;
}

struct register_result {
	uintptr_t pointer;
	uint64_t flags;
};

int main(void)
{
	void* occupied = mmap((void*)0x10000, 16384, PROT_READ | PROT_WRITE,
	                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
	if (occupied != MAP_FAILED)
		memset(occupied, 0x5a, 16384);
	size_t reservation_size = 512u * 1024 * 1024;
	void* reservation = mmap(NULL, reservation_size, PROT_NONE,
	                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	assert(reservation != MAP_FAILED);
	uint32_t* code = (void*)((char*)reservation + reservation_size / 2);
	size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
	assert(mprotect(code, page_size, PROT_READ | PROT_WRITE) == 0);
	static const uint32_t function[] = {
		0xa9bf7bfd, 0xa9bf53f3, 0xa9bf5bf5, 0xa9bf63f7, 0xa9bf6bf9, 0xa9bf73fb,
		0xeb01003f, 0xd53bd060, 0xaa0003e0, 0xd53b4201,
		0xa8c173fb, 0xa8c16bf9, 0xa8c163f7, 0xa8c15bf5, 0xa8c153f3, 0xa8c17bfd,
		0xd65f03c0,
	};
	for (unsigned i = 0; i < 32; ++i) {
		uint32_t* current = code + i * (sizeof(function) / sizeof(function[0]));
		memcpy(current, function, sizeof(function));
		current[7] |= i;
		current[8] |= i << 16;
	}
	guest.header.ncmds = 1;
	guest.segment.cmd = LC_SEGMENT_64;
	guest.segment.cmdsize = sizeof(guest.segment) + sizeof(guest.section);
	guest.segment.vmaddr = (uintptr_t)reservation;
	guest.segment.vmsize = reservation_size;
	guest.segment.initprot = VM_PROT_READ | VM_PROT_EXECUTE;
	guest.segment.nsects = 1;
	guest.section.addr = (uintptr_t)code;
	guest.section.size = 32 * sizeof(function);
	guest.section.flags = S_ATTR_PURE_INSTRUCTIONS;
	char error[256] = {0};
	int result = machgate_adapt_guest_instructions(NULL, error, sizeof(error));
	if (result != 32) {
		fprintf(stderr, "large image adaptation failed: %s (%d)\n", error, result);
		return 1;
	}
	__builtin___clear_cache((char*)code, (char*)code + guest.section.size);
	assert(mprotect(code, page_size, PROT_READ | PROT_EXEC) == 0);
	uintptr_t context = (uintptr_t)machgate_guest_thread_context();
	assert(context && *(uintptr_t*)context == context);
	if (occupied != MAP_FAILED) {
		assert((void*)context != occupied);
		for (size_t i = 0; i < 16384; ++i)
			assert(((unsigned char*)occupied)[i] == 0x5a);
	}
	for (unsigned i = 0; i < 32; ++i) {
		struct register_result (*run)(void) =
			(void*)(code + i * (sizeof(function) / sizeof(function[0])));
		struct register_result result = run();
		assert(result.pointer == (i == 31 ? 0 : context));
		assert((result.flags & 0xf0000000) == 0x60000000);
	}
	assert(munmap(reservation, reservation_size) == 0);
	if (occupied != MAP_FAILED)
		assert(munmap(occupied, 16384) == 0);
	return 0;
}
