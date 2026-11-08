#include "ports/guest_execution.h"
#include "ports/guest_image_view.h"
#include "ports/native_thread_local_image.h"

#include "macho_defs.h"

#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>

#define MACHGATE_MAX_TLV_SPANS 64u

struct thread_local_layout {
	struct machgate_tlv_descriptor_span spans[MACHGATE_MAX_TLV_SPANS];
	size_t span_count;
	uintptr_t start;
	uintptr_t end;
	uintptr_t initialized_end;
	size_t alignment;
};

static int section_address(const struct machgate_guest_image* image,
	const struct section_64* section, uintptr_t* result)
{
	intptr_t slide = machgate_guest_image_slide(image);
	if (slide >= 0) {
		if (section->addr > UINTPTR_MAX - (uintptr_t)slide)
			return -1;
		*result = section->addr + (uintptr_t)slide;
	} else {
		uintptr_t magnitude = (uintptr_t)(-(slide + 1)) + 1;
		if (section->addr < magnitude)
			return -1;
		*result = section->addr - magnitude;
	}
	return machgate_guest_image_contains(image, *result, section->size) ? 0 : -1;
}

static int collect_thread_local_section(struct thread_local_layout* layout,
	const struct machgate_guest_image* image, const struct section_64* section,
	int protection)
{
	uint32_t type = section->flags & SECTION_TYPE;
	if (type != S_THREAD_LOCAL_REGULAR && type != S_THREAD_LOCAL_ZEROFILL &&
	    type != S_THREAD_LOCAL_VARIABLES)
		return 0;
	if (!section->size)
		return 0;
	uintptr_t address;
	if (section_address(image, section, &address) != 0)
		return -1;
	if (type == S_THREAD_LOCAL_VARIABLES) {
		if (section->size % sizeof(struct machgate_tlv_descriptor) ||
		    address % _Alignof(struct machgate_tlv_descriptor) ||
		    layout->span_count == MACHGATE_MAX_TLV_SPANS ||
		    !(protection & VM_PROT_WRITE))
			return -1;
		layout->spans[layout->span_count++] = (struct machgate_tlv_descriptor_span){
			.descriptors = (void*)address,
			.count = section->size / sizeof(struct machgate_tlv_descriptor),
		};
		return 0;
	}
	if (section->align >= sizeof(size_t) * 8 || section->size > UINTPTR_MAX - address)
		return -1;
	size_t alignment = (size_t)1 << section->align;
	if (address < layout->start)
		layout->start = address;
	if (address + section->size > layout->end)
		layout->end = address + section->size;
	if (type == S_THREAD_LOCAL_REGULAR && address + section->size > layout->initialized_end)
		layout->initialized_end = address + section->size;
	if (alignment > layout->alignment)
		layout->alignment = alignment;
	return 0;
}

int machgate_prepare_guest_runtime_image(struct machgate_guest_image* image,
	char* error, size_t error_size)
{
	struct thread_local_layout layout = { .start = UINTPTR_MAX, .alignment = 1 };
	const struct mach_header_64* header = machgate_guest_image_validated_header(image);
	const uint8_t* cursor = (const void*)(header + 1);
	for (uint32_t index = 0; index < header->ncmds; index++) {
		const struct load_command* command = (const void*)cursor;
		if (command->cmd == LC_SEGMENT_64) {
			const struct segment_command_64* segment = (const void*)command;
			const struct section_64* sections = (const void*)(segment + 1);
			for (uint32_t section = 0; section < segment->nsects; section++)
				if (collect_thread_local_section(&layout, image, &sections[section],
				                                 segment->initprot) != 0)
					goto invalid;
		}
		cursor += command->cmdsize;
	}
	if (!layout.span_count && layout.start == UINTPTR_MAX)
		return 0;
	if (layout.start == UINTPTR_MAX || layout.end <= layout.start ||
	    !machgate_guest_image_contains(image, layout.start, layout.end - layout.start))
		goto invalid;
	size_t initialized_size = layout.initialized_end ? layout.initialized_end - layout.start : 0;
	struct machgate_native_thread_local_image native_image = {
		.identity = image,
		.initial_data = (const void*)layout.start,
		.initialized_size = initialized_size,
		.zero_fill_size = layout.end - layout.start - initialized_size,
		.alignment = layout.alignment,
		.spans = layout.spans,
		.span_count = layout.span_count,
	};
	int (*register_image)(const struct machgate_native_thread_local_image*) =
		dlsym(RTLD_DEFAULT, "machgate_register_thread_local_image");
	if (!register_image) {
		snprintf(error, error_size, "native provider does not support per-image Mach-O TLV storage");
		return -1;
	}
	if (register_image(&native_image) != 0) {
		snprintf(error, error_size, "native provider rejected the Mach-O TLV image");
		return -1;
	}
	return 0;
invalid:
	snprintf(error, error_size, "Mach-O has an invalid or unsupported thread-local layout");
	return -1;
}
