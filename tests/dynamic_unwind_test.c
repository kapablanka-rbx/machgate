#include "adapters/dynamic_unwind.h"
#include "domain/dwarf_unwind.h"
#include "ports/native_unwind.h"

#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <unwind.h>

struct dwarf_bases {
	void* text;
	void* data;
	void* function;
};

static int (*host_find_object)(void*, struct dl_find_object*);
static const void* (*find_frame)(const void*, struct dwarf_bases*);
static uintptr_t expected_code;
static int observed_dynamic_frame;

int _dl_find_object(void* address, struct dl_find_object* result)
{
	if (machgate_find_dynamic_unwind(address, result) == 0)
		return 0;
	return host_find_object ? host_find_object(address, result) : -1;
}

static _Unwind_Reason_Code observe_frame(struct _Unwind_Context* context,
				       void* argument)
{
	(void)argument;
	uintptr_t address = _Unwind_GetIP(context);
	if (address >= expected_code && address < expected_code + 32)
		observed_dynamic_frame++;
	return _URC_NO_REASON;
}

static void unwind_callback(void)
{
	_Unwind_Backtrace(observe_frame, NULL);
}

static void write_u32(uint8_t* address, uint32_t value)
{
	memcpy(address, &value, sizeof(value));
}

static void write_u64(uint8_t* address, uint64_t value)
{
	memcpy(address, &value, sizeof(value));
}

static void make_code(uint8_t* code)
{
	static const uint32_t instructions[] = {
		0xa9bf7bfdu, 0x910003fdu, 0x58000090u,
		0xd63f0200u, 0xa8c17bfdu, 0xd65f03c0u,
	};
	memcpy(code, instructions, sizeof(instructions));
	write_u64(code + 24, (uintptr_t)unwind_callback);
	__builtin___clear_cache((char*)code, (char*)code + 32);
}

static void make_cie(uint8_t* cie, uint8_t encoding)
{
	static const uint8_t bytes[24] = {
		20, 0, 0, 0, 0, 0, 0, 0, 1, 'z', 'R', 0,
		1, 0x78, 30, 1, 0, 0x0c, 31, 0, 0, 0, 0, 0,
	};
	memcpy(cie, bytes, sizeof(bytes));
	cie[16] = encoding;
}

static void make_frame(uint8_t* frame, const uint8_t* cie,
		       const uint8_t* code, int relative)
{
	memset(frame, 0, 32);
	write_u32(frame, 28);
	write_u32(frame + 4, (uint32_t)(frame + 4 - cie));
	size_t augmentation;
	if (relative) {
		write_u32(frame + 8, (uint32_t)((uintptr_t)code - (uintptr_t)(frame + 8)));
		write_u32(frame + 12, 32);
		augmentation = 16;
	} else {
		write_u64(frame + 8, (uintptr_t)code);
		write_u64(frame + 16, 32);
		augmentation = 24;
	}
	static const uint8_t cfi[] = {0, 0x0c, 31, 16, 0x9d, 2, 0x9e, 1};
	memcpy(frame + augmentation, cfi, sizeof(cfi));
}

static int check_lookup(const uint8_t* code, const void* expected_frame)
{
	struct dwarf_bases bases = {0};
	const void* result = find_frame(code + 16, &bases);
	if (result != expected_frame) {
		fprintf(stderr, "FDE lookup returned %p, expected %p\n", result, expected_frame);
		return -1;
	}
	if (result && bases.function != code) {
		fprintf(stderr, "FDE function base differs from generated code\n");
		return -1;
	}
	return 0;
}

static int check_unwind(uint8_t* code)
{
	expected_code = (uintptr_t)code;
	observed_dynamic_frame = 0;
	((void (*)(void))code)();
	if (observed_dynamic_frame != 1) {
		fprintf(stderr, "expected one generated frame, observed %d\n", observed_dynamic_frame);
		return -1;
	}
	return 0;
}

static int check_invalid_records(void)
{
	uint8_t records[80] = {0};
	uint8_t* frame = records + 48;
	struct machgate_dwarf_frame result;
	make_cie(records, 0);
	make_frame(frame, records, records, 0);
	if (machgate_decode_dwarf_frame(frame, &result) != 0)
		return -1;
	write_u64(frame + 16, UINT64_MAX);
	if (machgate_decode_dwarf_frame(frame, &result) == 0)
		return -1;
	make_frame(frame, records, records, 0);
	records[16] = 0xff;
	if (machgate_decode_dwarf_frame(frame, &result) == 0)
		return -1;
	make_cie(records, 0);
	write_u32(records, 32);
	memmove(records + 23, records + 14, 10);
	memset(records + 13, 0x80, 9);
	records[22] = 1;
	if (machgate_decode_dwarf_frame(frame, &result) == 0)
		return -1;
	records[22] = 0;
	if (machgate_decode_dwarf_frame(frame, &result) != 0)
		return -1;
	return 0;
}

int main(void)
{
	if (check_invalid_records() != 0)
		return 11;
	host_find_object = dlsym(RTLD_NEXT, "_dl_find_object");
	void* unwinder = dlopen("libgcc_s.so.1", RTLD_NOW | RTLD_LOCAL);
	if (!host_find_object || !unwinder)
		return 1;
	find_frame = dlsym(unwinder, "_Unwind_Find_FDE");
	if (!find_frame)
		return 2;
	size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
	uint8_t* mapping = mmap(NULL, page_size * 3, PROT_READ | PROT_WRITE,
	                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (mapping == MAP_FAILED)
		return 3;
	uint8_t* metadata = mapping + page_size;
	uint8_t* first = metadata + 24;
	uint8_t* second = first + 32;
	uint8_t* third = metadata + page_size - 32;
	make_cie(metadata, 0);
	make_frame(first, metadata, mapping, 0);
	make_frame(second, metadata, mapping + 64, 0);
	make_cie(third - 24, 0x1b);
	make_frame(third, third - 24, mapping + 128, 1);
	for (size_t index = 0; index < 3; index++)
		make_code(mapping + index * 64);
	if (mprotect(mapping, page_size, PROT_READ | PROT_EXEC) != 0 ||
	    mprotect(metadata, page_size, PROT_READ) != 0 ||
	    mprotect(metadata + page_size, page_size, PROT_NONE) != 0)
		return 4;
	if (check_lookup(mapping, NULL) != 0 || check_lookup(mapping + 64, NULL) != 0)
		return 5;
	machgate_guest_register_frame(first);
	if (check_lookup(mapping, first) != 0 || check_lookup(mapping + 64, NULL) != 0 ||
	    check_unwind(mapping) != 0)
		return 6;
	machgate_guest_register_frame(second);
	machgate_guest_register_frame(third);
	if (check_lookup(mapping + 64, second) != 0 || check_lookup(mapping + 128, third) != 0 ||
	    check_unwind(mapping + 64) != 0 || check_unwind(mapping + 128) != 0)
		return 7;
	machgate_guest_deregister_frame(first);
	if (check_lookup(mapping, NULL) != 0 || check_lookup(mapping + 64, second) != 0 ||
	    check_lookup(mapping + 128, third) != 0)
		return 8;
	machgate_guest_register_frame(second);
	machgate_guest_deregister_frame(second);
	if (check_lookup(mapping + 64, NULL) != 0 || check_unwind(mapping + 128) != 0)
		return 9;
	machgate_guest_deregister_frame(third);
	machgate_guest_deregister_frame(third);
	machgate_guest_deregister_frame(NULL);
	machgate_guest_register_frame(metadata);
	if (check_lookup(mapping + 128, NULL) != 0)
		return 10;
	munmap(mapping, page_size * 3);
	puts("dynamic FDE registration, independent removal and native unwinding passed");
	return 0;
}
