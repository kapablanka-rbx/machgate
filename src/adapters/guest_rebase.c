#include "adapters/guest_rebase.h"

#include "macho_defs.h"
#include "ports/guest_image_view.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define MAX_REBASE_SEGMENTS 128u

enum rebase_opcode {
	REBASE_DONE = 0x00,
	REBASE_SET_TYPE = 0x10,
	REBASE_SET_SEGMENT_OFFSET = 0x20,
	REBASE_ADD_ADDRESS = 0x30,
	REBASE_ADD_ADDRESS_SCALED = 0x40,
	REBASE_REPEAT_IMMEDIATE = 0x50,
	REBASE_REPEAT_ULEB = 0x60,
	REBASE_ADD_AFTER_POINTER = 0x70,
	REBASE_REPEAT_SKIPPING = 0x80,
};

struct rebase_context {
	struct machgate_guest_image* image;
	const struct segment_command_64* segments[MAX_REBASE_SEGMENTS];
	size_t segment_count;
	size_t segment_index;
	int pointer_type_set;
	uint64_t offset;
	size_t page_size;
};

static int read_uleb(const uint8_t** cursor, const uint8_t* end, uint64_t* result)
{
	*result = 0;
	for (unsigned shift = 0; shift < 64 && *cursor < end; shift += 7) {
		uint8_t byte = *(*cursor)++;
		if (shift == 63 && (byte & 0x7e))
			return -1;
		*result |= (uint64_t)(byte & 0x7f) << shift;
		if (!(byte & 0x80))
			return 0;
	}
	return -1;
}

static int advance_offset(struct rebase_context* context, uint64_t amount)
{
	if (amount > UINT64_MAX - context->offset)
		return -1;
	context->offset += amount;
	return 0;
}

static int rebase_pointer(struct rebase_context* context)
{
	if (!context->pointer_type_set || context->segment_index >= context->segment_count)
		return -1;
	const struct segment_command_64* segment = context->segments[context->segment_index];
	if (context->offset > segment->vmsize ||
	    sizeof(uintptr_t) > segment->vmsize - context->offset)
		return -1;
	intptr_t slide = machgate_guest_image_slide(context->image);
	uintptr_t address = segment->vmaddr + slide + context->offset;
	if (!machgate_guest_image_contains(context->image, address, sizeof(uintptr_t)))
		return -1;
	uintptr_t result;
	memcpy(&result, (void*)address, sizeof(result));
	if (slide >= 0) {
		if (result > UINTPTR_MAX - (uintptr_t)slide)
			return -1;
		result += (uintptr_t)slide;
	} else {
		uintptr_t magnitude = (uintptr_t)(-(slide + 1)) + 1;
		if (result < magnitude)
			return -1;
		result -= magnitude;
	}
	int protection = 0;
	if (segment->initprot & VM_PROT_READ)
		protection |= PROT_READ;
	if (segment->initprot & VM_PROT_WRITE)
		protection |= PROT_WRITE;
	if (segment->initprot & VM_PROT_EXECUTE)
		protection |= PROT_EXEC;
	uintptr_t mask = context->page_size - 1;
	uintptr_t start = address & ~mask;
	uintptr_t end = (address + sizeof(result) + mask) & ~mask;
	if (!(protection & PROT_WRITE) &&
	    mprotect((void*)start, end - start, protection | PROT_WRITE) != 0)
		return -1;
	memcpy((void*)address, &result, sizeof(result));
	if (protection & PROT_EXEC)
		__builtin___clear_cache((char*)address, (char*)address + sizeof(result));
	if (!(protection & PROT_WRITE) &&
	    mprotect((void*)start, end - start, protection) != 0)
		return -1;
	return advance_offset(context, sizeof(result));
}

static int rebase_repeated(struct rebase_context* context, uint64_t count, uint64_t skip)
{
	if (context->segment_index >= context->segment_count ||
	    count > context->segments[context->segment_index]->vmsize / sizeof(uintptr_t))
		return -1;
	for (uint64_t index = 0; index < count; index++)
		if (rebase_pointer(context) != 0 || advance_offset(context, skip) != 0)
			return -1;
	return 0;
}

static int apply_rebase_stream(struct rebase_context* context,
	const uint8_t* cursor, size_t size)
{
	const uint8_t* end = cursor + size;
	while (cursor < end) {
		uint8_t byte = *cursor++;
		uint8_t immediate = byte & 0x0f;
		uint64_t count;
		uint64_t skip;
		switch (byte & 0xf0) {
		case REBASE_DONE:
			return 0;
		case REBASE_SET_TYPE:
			if (immediate != 1)
				return -1;
			context->pointer_type_set = 1;
			break;
		case REBASE_SET_SEGMENT_OFFSET:
			context->segment_index = immediate;
			if (read_uleb(&cursor, end, &context->offset) != 0)
				return -1;
			break;
		case REBASE_ADD_ADDRESS:
			if (read_uleb(&cursor, end, &skip) != 0 || advance_offset(context, skip) != 0)
				return -1;
			break;
		case REBASE_ADD_ADDRESS_SCALED:
			if (advance_offset(context, (uint64_t)immediate * sizeof(uintptr_t)) != 0)
				return -1;
			break;
		case REBASE_REPEAT_IMMEDIATE:
			if (rebase_repeated(context, immediate, 0) != 0)
				return -1;
			break;
		case REBASE_REPEAT_ULEB:
			if (read_uleb(&cursor, end, &count) != 0 || rebase_repeated(context, count, 0) != 0)
				return -1;
			break;
		case REBASE_ADD_AFTER_POINTER:
			if (read_uleb(&cursor, end, &skip) != 0 || rebase_repeated(context, 1, skip) != 0)
				return -1;
			break;
		case REBASE_REPEAT_SKIPPING:
			if (read_uleb(&cursor, end, &count) != 0 || read_uleb(&cursor, end, &skip) != 0 ||
			    rebase_repeated(context, count, skip) != 0)
				return -1;
			break;
		default:
			return -1;
		}
	}
	return 0;
}

int machgate_rebase_guest_pointers(struct machgate_guest_image* image,
	char* error, size_t error_size)
{
	struct mach_header_64* header = machgate_guest_image_validated_header(image);
	struct rebase_context context = { .image = image, .segment_index = SIZE_MAX };
	const struct dyld_info_command* info = NULL;
	const uint8_t* cursor = (const void*)(header + 1);
	for (uint32_t index = 0; index < header->ncmds; index++) {
		const struct load_command* command = (const void*)cursor;
		if (command->cmd == LC_SEGMENT_64) {
			if (context.segment_count == MAX_REBASE_SEGMENTS)
				goto invalid;
			context.segments[context.segment_count++] = (const void*)command;
		}
		if (command->cmd == LC_DYLD_INFO_ONLY) {
			if (command->cmdsize < sizeof(*info))
				goto invalid;
			info = (const void*)command;
		}
		cursor += command->cmdsize;
	}
	if (!info || !info->rebase_size)
		return 0;
	long page_size = sysconf(_SC_PAGESIZE);
	if (page_size <= 0)
		goto invalid;
	context.page_size = (size_t)page_size;
	for (size_t index = 0; index < context.segment_count; index++) {
		const struct segment_command_64* segment = context.segments[index];
		if (info->rebase_off < segment->fileoff ||
		    info->rebase_off - segment->fileoff > segment->filesize ||
		    info->rebase_size > segment->filesize - (info->rebase_off - segment->fileoff))
			continue;
		uintptr_t address = segment->vmaddr + machgate_guest_image_slide(image) +
		                    info->rebase_off - segment->fileoff;
		if (!machgate_guest_image_contains(image, address, info->rebase_size) ||
		    apply_rebase_stream(&context, (const void*)address, info->rebase_size) != 0)
			goto invalid;
		return 0;
	}
invalid:
	snprintf(error, error_size, "Mach-O contains an invalid or unsupported pointer rebase stream");
	return -1;
}
