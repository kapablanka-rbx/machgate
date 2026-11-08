#include "adapters/guest_unwind.h"
#include "adapters/dynamic_unwind.h"

#include "ports/guest_image_view.h"

#include "loader.h"
#include "log.h"
#include "macho_defs.h"
#include "adapters/startup_cache.h"

#include <dlfcn.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define UNWIND_SECTION_VERSION 1u
#define UNWIND_SECOND_LEVEL_REGULAR 2u
#define UNWIND_SECOND_LEVEL_COMPRESSED 3u

#define UNWIND_HAS_LSDA 0x40000000u
#define UNWIND_PERSONALITY_MASK 0x30000000u
#define UNWIND_ARM64_MODE_MASK 0x0f000000u
#define UNWIND_ARM64_MODE_FRAMELESS 0x02000000u
#define UNWIND_ARM64_MODE_DWARF 0x03000000u
#define UNWIND_ARM64_MODE_FRAME 0x04000000u
#define UNWIND_ARM64_FRAMELESS_STACK_SIZE_MASK 0x00fff000u
#define UNWIND_ARM64_DWARF_SECTION_OFFSET 0x00ffffffu
#define UNWIND_ARM64_X_REGISTER_PAIRS_MASK 0x0000001fu
#define UNWIND_ARM64_D_REGISTER_PAIRS_MASK 0x00000f00u
#define UNWIND_ARM64_REGISTER_PAIRS_MASK \
	(UNWIND_ARM64_X_REGISTER_PAIRS_MASK | \
	 UNWIND_ARM64_D_REGISTER_PAIRS_MASK)
#define UNWIND_ARCHITECTURE_INDEPENDENT_MASK 0xf0000000u

#define DW_EH_PE_ABSPTR 0x00u
#define DW_EH_PE_UDATA4 0x03u
#define DW_EH_PE_SDATA4 0x0bu
#define DW_EH_PE_DATAREL 0x30u

#define DW_CFA_NOP 0x00u
#define DW_CFA_OFFSET_EXTENDED 0x05u
#define DW_CFA_SAME_VALUE 0x08u
#define DW_CFA_DEF_CFA 0x0cu
#define DW_CFA_DEF_CFA_OFFSET 0x0eu

struct compact_unwind_header {
	uint32_t version;
	uint32_t common_encodings_offset;
	uint32_t common_encodings_count;
	uint32_t personalities_offset;
	uint32_t personalities_count;
	uint32_t index_offset;
	uint32_t index_count;
};

struct compact_unwind_index {
	uint32_t function_offset;
	uint32_t second_level_offset;
	uint32_t lsda_offset;
};

struct compact_unwind_lsda {
	uint32_t function_offset;
	uint32_t lsda_offset;
};

struct compact_regular_header {
	uint32_t kind;
	uint16_t entries_offset;
	uint16_t entries_count;
};

struct compact_regular_entry {
	uint32_t function_offset;
	uint32_t encoding;
};

struct compact_compressed_header {
	uint32_t kind;
	uint16_t entries_offset;
	uint16_t entries_count;
	uint16_t encodings_offset;
	uint16_t encodings_count;
};

struct mapped_section {
	uint8_t* address;
	size_t size;
};

struct unwind_row {
	uint32_t function_offset;
	uint32_t encoding;
	uintptr_t lsda;
	uintptr_t fde;
};

struct cached_unwind_header {
	uint64_t image_base;
	uint64_t metadata_size;
	uint64_t frame_offset;
	uint64_t frame_size;
	uint64_t index_count;
	uint64_t personality_offsets[4][2];
};

struct cached_unwind_index {
	uint32_t function_offset;
	uint32_t guest_fde;
	uint64_t fde_offset;
};

struct byte_writer {
	uint8_t* bytes;
	size_t capacity;
	size_t offset;
};

struct guest_unwind_registration {
	uintptr_t map_start;
	uintptr_t map_end;
	void* eh_frame_header;
};

typedef int (*find_object_function)(void*, struct dl_find_object*);

static struct guest_unwind_registration guest_unwind_registration;
static find_object_function next_find_object;

static int section_name_is(const char name[16], const char* expected)
{
	return strncmp(name, expected, 16) == 0;
}

static int mapped_range_valid(const struct machgate_guest_image* image,
			      uintptr_t address, size_t size)
{
	return address >= machgate_guest_image_mapping_start(image) && address <= machgate_guest_image_mapping_end(image) &&
	       size <= machgate_guest_image_mapping_end(image) - address;
}

static int find_mapped_section(struct machgate_guest_image* image,
			       const char* segment_name,
			       const char* section_name,
			       struct mapped_section* result)
{
	struct mach_header_64* header = machgate_guest_image_validated_header(image);
	uint8_t* cursor = (uint8_t*)(header + 1);
	for (uint32_t command_index = 0; command_index < header->ncmds;
	     command_index++) {
		struct load_command* command = (void*)cursor;
		if (command->cmd == LC_SEGMENT_64) {
			struct segment_command_64* segment = (void*)command;
			struct section_64* sections = (void*)(segment + 1);
			for (uint32_t section_index = 0;
			     section_index < segment->nsects; section_index++) {
				struct section_64* section = &sections[section_index];
				if (!section_name_is(section->segname, segment_name) ||
				    !section_name_is(section->sectname, section_name))
					continue;
				if (section->size > SIZE_MAX ||
				    section->addr > UINTPTR_MAX - machgate_guest_image_slide(image))
					return -1;
				uintptr_t address = section->addr + machgate_guest_image_slide(image);
				if (!mapped_range_valid(image, address, section->size))
					return -1;
				result->address = (void*)address;
				result->size = section->size;
				return 1;
			}
		}
		cursor += command->cmdsize;
	}
	return 0;
}

static void* section_bytes(const struct mapped_section* section,
			   size_t offset, size_t size)
{
	if (offset > section->size || size > section->size - offset)
		return NULL;
	return section->address + offset;
}

static uintptr_t find_lsda(const struct mapped_section* compact,
			   const struct compact_unwind_index* index,
			   uint32_t function_offset,
			   const struct machgate_guest_image* image)
{
	if (index[1].lsda_offset < index[0].lsda_offset)
		return UINTPTR_MAX;
	size_t bytes = index[1].lsda_offset - index[0].lsda_offset;
	if (bytes % sizeof(struct compact_unwind_lsda) != 0)
		return UINTPTR_MAX;
	struct compact_unwind_lsda* entries = section_bytes(
		compact, index[0].lsda_offset, bytes);
	if (!entries && bytes)
		return UINTPTR_MAX;
	size_t count = bytes / sizeof(*entries);
	for (size_t entry_index = 0; entry_index < count; entry_index++) {
		if (entries[entry_index].function_offset != function_offset)
			continue;
		uintptr_t image_base = (uintptr_t)machgate_guest_image_validated_header(image);
		if (entries[entry_index].lsda_offset > UINTPTR_MAX - image_base)
			return UINTPTR_MAX;
		uintptr_t address = image_base + entries[entry_index].lsda_offset;
		if (!mapped_range_valid(image, address, 1))
			return UINTPTR_MAX;
		return address;
	}
	return 0;
}

static int append_unwind_row(struct unwind_row* rows, size_t* row_count,
			     size_t row_capacity,
			     uint32_t function_offset, uint32_t encoding,
			     uintptr_t lsda, char* error, size_t error_size)
{
	if (*row_count && rows[*row_count - 1].function_offset > function_offset) {
		snprintf(error, error_size,
		         "Mach-O compact unwind rows are not ordered");
		return -1;
	}
	if (*row_count && rows[*row_count - 1].function_offset == function_offset) {
		rows[*row_count - 1] = (struct unwind_row){
			.function_offset = function_offset,
			.encoding = encoding,
			.lsda = lsda,
		};
		return 0;
	}
	if (*row_count == row_capacity) {
		snprintf(error, error_size,
		         "Mach-O compact unwind table has too many rows");
		return -1;
	}
	rows[*row_count] = (struct unwind_row){
		.function_offset = function_offset,
		.encoding = encoding,
		.lsda = lsda,
	};
	(*row_count)++;
	return 0;
}

static int collect_regular_rows(const struct mapped_section* compact,
				const struct compact_unwind_index* index,
				const struct machgate_guest_image* image,
				struct unwind_row* rows,
				size_t* row_count, size_t row_capacity,
				char* error,
				size_t error_size)
{
	struct compact_regular_header* header = section_bytes(
		compact, index->second_level_offset, sizeof(*header));
	if (!header)
		return -1;
	size_t entries_offset = index->second_level_offset + header->entries_offset;
	size_t entries_size =
		(size_t)header->entries_count * sizeof(struct compact_regular_entry);
	struct compact_regular_entry* entries =
		section_bytes(compact, entries_offset, entries_size);
	if (!entries && entries_size)
		return -1;
	for (size_t entry_index = 0; entry_index < header->entries_count;
	     entry_index++) {
		uintptr_t lsda = find_lsda(compact, index,
		                           entries[entry_index].function_offset,
		                           image);
		if (lsda == UINTPTR_MAX ||
		    append_unwind_row(rows, row_count, row_capacity,
		                      entries[entry_index].function_offset,
		                      entries[entry_index].encoding, lsda, error,
		                      error_size) != 0)
			return -1;
	}
	return 0;
}

static int collect_compressed_rows(
	const struct mapped_section* compact,
	const struct compact_unwind_header* compact_header,
	const struct compact_unwind_index* index,
	const struct machgate_guest_image* image,
	struct unwind_row* rows, size_t* row_count, size_t row_capacity,
	char* error,
	size_t error_size)
{
	struct compact_compressed_header* header = section_bytes(
		compact, index->second_level_offset, sizeof(*header));
	if (!header)
		return -1;
	uint32_t* common_encodings = section_bytes(
		compact, compact_header->common_encodings_offset,
		(size_t)compact_header->common_encodings_count * sizeof(uint32_t));
	uint32_t* local_encodings = section_bytes(
		compact, index->second_level_offset + header->encodings_offset,
		(size_t)header->encodings_count * sizeof(uint32_t));
	uint32_t* entries = section_bytes(
		compact, index->second_level_offset + header->entries_offset,
		(size_t)header->entries_count * sizeof(uint32_t));
	if ((!common_encodings && compact_header->common_encodings_count) ||
	    (!local_encodings && header->encodings_count) ||
	    (!entries && header->entries_count))
		return -1;
	for (size_t entry_index = 0; entry_index < header->entries_count;
	     entry_index++) {
		uint32_t packed = entries[entry_index];
		uint32_t encoding_index = packed >> 24;
		uint32_t encoding;
		if (encoding_index < compact_header->common_encodings_count) {
			encoding = common_encodings[encoding_index];
		} else {
			uint32_t local_index = encoding_index -
			                       compact_header->common_encodings_count;
			if (local_index >= header->encodings_count)
				return -1;
			encoding = local_encodings[local_index];
		}
		uint32_t relative_offset = packed & 0x00ffffffu;
		if (relative_offset > UINT32_MAX - index->function_offset)
			return -1;
		uint32_t function_offset = index->function_offset + relative_offset;
		uintptr_t lsda = find_lsda(compact, index, function_offset, image);
		if (lsda == UINTPTR_MAX ||
		    append_unwind_row(rows, row_count, row_capacity,
		                      function_offset, encoding,
		                      lsda, error, error_size) != 0)
			return -1;
	}
	return 0;
}

static int collect_unwind_rows(const struct mapped_section* compact,
			       const struct machgate_guest_image* image,
			       struct unwind_row* rows,
			       size_t* row_count, size_t row_capacity,
			       uint32_t* final_offset,
			       char* error, size_t error_size)
{
	struct compact_unwind_header* header =
		section_bytes(compact, 0, sizeof(*header));
	if (!header || header->version != UNWIND_SECTION_VERSION ||
	    header->index_count < 2) {
		snprintf(error, error_size, "Mach-O has an invalid compact unwind header");
		return -1;
	}
	struct compact_unwind_index* indices = section_bytes(
		compact, header->index_offset,
		(size_t)header->index_count * sizeof(*indices));
	if (!indices) {
		snprintf(error, error_size, "Mach-O compact unwind index is truncated");
		return -1;
	}
	for (size_t index_number = 0; index_number + 1 < header->index_count;
	     index_number++) {
		struct compact_unwind_index* index = &indices[index_number];
		uint32_t* kind = section_bytes(compact, index->second_level_offset,
		                               sizeof(*kind));
		int result = -1;
		if (kind && *kind == UNWIND_SECOND_LEVEL_REGULAR)
			result = collect_regular_rows(compact, index, image, rows,
			                              row_count, row_capacity, error,
			                              error_size);
		if (kind && *kind == UNWIND_SECOND_LEVEL_COMPRESSED)
			result = collect_compressed_rows(
				compact, header, index, image, rows, row_count,
				row_capacity, error, error_size);
		if (result != 0) {
			snprintf(error, error_size,
			         "Mach-O has an invalid compact unwind second-level page");
			return -1;
		}
	}
	*final_offset = indices[header->index_count - 1].function_offset;
	return 0;
}

static int writer_bytes(struct byte_writer* writer, const void* bytes,
			size_t size)
{
	if (writer->offset > writer->capacity ||
	    size > writer->capacity - writer->offset)
		return -1;
	memcpy(writer->bytes + writer->offset, bytes, size);
	writer->offset += size;
	return 0;
}

static int writer_u8(struct byte_writer* writer, uint8_t value)
{
	return writer_bytes(writer, &value, sizeof(value));
}

static int writer_u32(struct byte_writer* writer, uint32_t value)
{
	return writer_bytes(writer, &value, sizeof(value));
}

static int writer_u64(struct byte_writer* writer, uint64_t value)
{
	return writer_bytes(writer, &value, sizeof(value));
}

static int writer_uleb(struct byte_writer* writer, uint64_t value)
{
	do {
		uint8_t byte = value & 0x7fu;
		value >>= 7;
		if (value)
			byte |= 0x80u;
		if (writer_u8(writer, byte) != 0)
			return -1;
	} while (value);
	return 0;
}

static int writer_sleb(struct byte_writer* writer, int64_t value)
{
	int more;
	do {
		uint8_t byte = value & 0x7f;
		int sign = byte & 0x40;
		value >>= 7;
		more = !((value == 0 && !sign) || (value == -1 && sign));
		if (more)
			byte |= 0x80;
		if (writer_u8(writer, byte) != 0)
			return -1;
	} while (more);
	return 0;
}

static int finish_record(struct byte_writer* writer, size_t record_start)
{
	while ((writer->offset - record_start) % sizeof(uintptr_t) != 0) {
		if (writer_u8(writer, DW_CFA_NOP) != 0)
			return -1;
	}
	if (writer->offset - record_start - sizeof(uint32_t) > UINT32_MAX)
		return -1;
	uint32_t length = writer->offset - record_start - sizeof(uint32_t);
	memcpy(writer->bytes + record_start, &length, sizeof(length));
	return 0;
}

static int emit_cie(struct byte_writer* writer, uintptr_t personality,
		    int has_lsda, size_t* result, uint64_t* personality_offset)
{
	size_t start = writer->offset;
	if (writer_u32(writer, 0) != 0 || writer_u32(writer, 0) != 0 ||
	    writer_u8(writer, 1) != 0)
		return -1;
	const char* augmentation;
	if (personality)
		augmentation = has_lsda ? "zPLR" : "zPR";
	else
		augmentation = has_lsda ? "zLR" : "zR";
	if (writer_bytes(writer, augmentation, strlen(augmentation) + 1) != 0 ||
	    writer_uleb(writer, 1) != 0 || writer_sleb(writer, -8) != 0 ||
	    writer_uleb(writer, 30) != 0)
		return -1;
	size_t augmentation_size = (personality ? 9 : 0) + (has_lsda ? 1 : 0) + 1;
	if (writer_uleb(writer, augmentation_size) != 0)
		return -1;
	if (personality) {
		if (writer_u8(writer, DW_EH_PE_ABSPTR) != 0)
			return -1;
		*personality_offset = writer->offset;
		if (writer_u64(writer, personality) != 0)
			return -1;
	}
	if (has_lsda && writer_u8(writer, DW_EH_PE_ABSPTR) != 0)
		return -1;
	if (writer_u8(writer, DW_EH_PE_ABSPTR) != 0 ||
	    writer_u8(writer, DW_CFA_DEF_CFA) != 0 ||
	    writer_uleb(writer, 31) != 0 || writer_uleb(writer, 0) != 0 ||
	    writer_u8(writer, DW_CFA_SAME_VALUE) != 0 ||
	    writer_uleb(writer, 30) != 0 || finish_record(writer, start) != 0)
		return -1;
	*result = start;
	return 0;
}

static int emit_offset_rule(struct byte_writer* writer, unsigned reg,
			    unsigned factor)
{
	if (reg < 64)
		return writer_u8(writer, 0x80u | reg) ||
		       writer_uleb(writer, factor);
	return writer_u8(writer, DW_CFA_OFFSET_EXTENDED) ||
	       writer_uleb(writer, reg) || writer_uleb(writer, factor);
}

static int emit_saved_register_pairs(struct byte_writer* writer,
				     uint32_t encoding,
				     unsigned first_factor)
{
	static const struct {
		uint32_t flag;
		unsigned first;
		unsigned second;
	} pairs[] = {
		{0x00000001u, 19, 20}, {0x00000002u, 21, 22},
		{0x00000004u, 23, 24}, {0x00000008u, 25, 26},
		{0x00000010u, 27, 28}, {0x00000100u, 72, 73},
		{0x00000200u, 74, 75}, {0x00000400u, 76, 77},
		{0x00000800u, 78, 79},
	};
	size_t register_index = 0;
	for (size_t index = 0; index < sizeof(pairs) / sizeof(pairs[0]); index++) {
		if (!(encoding & pairs[index].flag))
			continue;
		for (size_t in_pair = 0; in_pair < 2; in_pair++) {
			unsigned reg = in_pair ? pairs[index].second : pairs[index].first;
			unsigned factor = first_factor + register_index;
			if (emit_offset_rule(writer, reg, factor) != 0)
				return -1;
			register_index++;
		}
	}
	return 0;
}

static int emit_fde(struct byte_writer* writer, size_t cie_offset,
		    uintptr_t function_start, uintptr_t function_end,
		    uint32_t encoding, uintptr_t lsda, int has_lsda,
		    uintptr_t* result)
{
	size_t start = writer->offset;
	if (function_end <= function_start ||
	    start + sizeof(uint32_t) < cie_offset)
		return -1;
	uint32_t cie_distance = start + sizeof(uint32_t) - cie_offset;
	if (writer_u32(writer, 0) != 0 ||
	    writer_u32(writer, cie_distance) != 0 ||
	    writer_u64(writer, function_start) != 0 ||
	    writer_u64(writer, function_end - function_start) != 0 ||
	    writer_uleb(writer, has_lsda ? sizeof(uintptr_t) : 0) != 0 ||
	    (has_lsda && writer_u64(writer, lsda) != 0))
		return -1;
	uint32_t mode = encoding & UNWIND_ARM64_MODE_MASK;
	if (mode == UNWIND_ARM64_MODE_FRAME) {
		uint32_t allowed_bits = UNWIND_ARCHITECTURE_INDEPENDENT_MASK |
		                        UNWIND_ARM64_MODE_MASK |
		                        UNWIND_ARM64_REGISTER_PAIRS_MASK;
		if (encoding & ~allowed_bits)
			return -1;
		if (writer_u8(writer, DW_CFA_DEF_CFA) != 0 ||
		    writer_uleb(writer, 29) != 0 || writer_uleb(writer, 16) != 0 ||
		    emit_offset_rule(writer, 29, 2) != 0 ||
		    emit_offset_rule(writer, 30, 1) != 0)
			return -1;
		if (emit_saved_register_pairs(writer, encoding, 3) != 0)
			return -1;
	} else if (mode == UNWIND_ARM64_MODE_FRAMELESS) {
		uint32_t allowed_bits = UNWIND_ARCHITECTURE_INDEPENDENT_MASK |
		                        UNWIND_ARM64_MODE_MASK |
		                        UNWIND_ARM64_FRAMELESS_STACK_SIZE_MASK |
		                        UNWIND_ARM64_REGISTER_PAIRS_MASK;
		if (encoding & ~allowed_bits)
			return -1;
		size_t stack_size =
			((encoding & UNWIND_ARM64_FRAMELESS_STACK_SIZE_MASK) >> 12) * 16;
		if (writer_u8(writer, DW_CFA_DEF_CFA) != 0 ||
		    writer_uleb(writer, 31) != 0 ||
		    writer_uleb(writer, stack_size) != 0 ||
		    emit_saved_register_pairs(writer, encoding, 1) != 0)
			return -1;
	} else {
		return -1;
	}
	if (finish_record(writer, start) != 0)
		return -1;
	*result = (uintptr_t)(writer->bytes + start);
	return 0;
}

static uintptr_t resolve_personality(
	const struct mapped_section* compact,
	const struct compact_unwind_header* header,
	const struct machgate_guest_image* image, uint32_t encoding)
{
	uint32_t personality_index = (encoding & UNWIND_PERSONALITY_MASK) >> 28;
	if (!personality_index)
		return 0;
	if (personality_index > header->personalities_count)
		return UINTPTR_MAX;
	uint32_t* personalities = section_bytes(
		compact, header->personalities_offset,
		(size_t)header->personalities_count * sizeof(uint32_t));
	if (!personalities)
		return UINTPTR_MAX;
	uintptr_t image_base = (uintptr_t)machgate_guest_image_validated_header(image);
	uint32_t slot_offset = personalities[personality_index - 1];
	if (slot_offset > UINTPTR_MAX - image_base)
		return UINTPTR_MAX;
	uintptr_t slot = image_base + slot_offset;
	if (!mapped_range_valid(image, slot, sizeof(uintptr_t)))
		return UINTPTR_MAX;
	return *(uintptr_t*)slot;
}

static int dwarf_fde_valid(const struct mapped_section* eh_frame,
			   size_t fde_offset)
{
	uint32_t* fde_length = section_bytes(
		eh_frame, fde_offset, sizeof(*fde_length));
	if (!fde_length || !*fde_length || *fde_length == UINT32_MAX ||
	    *fde_length < sizeof(uint32_t) ||
	    *fde_length > eh_frame->size - fde_offset - sizeof(uint32_t))
		return 0;
	size_t cie_pointer_offset = fde_offset + sizeof(uint32_t);
	uint32_t* cie_distance = section_bytes(
		eh_frame, cie_pointer_offset, sizeof(*cie_distance));
	if (!cie_distance || !*cie_distance ||
	    *cie_distance > cie_pointer_offset)
		return 0;
	size_t cie_offset = cie_pointer_offset - *cie_distance;
	uint32_t* cie_length = section_bytes(
		eh_frame, cie_offset, sizeof(*cie_length));
	if (!cie_length || !*cie_length || *cie_length == UINT32_MAX ||
	    *cie_length < sizeof(uint32_t) ||
	    *cie_length > eh_frame->size - cie_offset - sizeof(uint32_t))
		return 0;
	uint32_t* cie_identifier = section_bytes(
		eh_frame, cie_offset + sizeof(uint32_t), sizeof(*cie_identifier));
	return cie_identifier && *cie_identifier == 0;
}

static size_t align_up(size_t value, size_t alignment)
{
	return (value + alignment - 1) & ~(alignment - 1);
}

typedef void (*register_frame_function)(void*);

static register_frame_function resolve_register_frame(void)
{
	void* library = dlopen("libgcc_s.so.1", RTLD_NOW | RTLD_LOCAL);
	if (!library)
		return NULL;
	return (register_frame_function)dlsym(library, "__register_frame");
}

static int resolve_next_find_object(char* error, size_t error_size)
{
	if (!next_find_object)
		next_find_object =
			(find_object_function)dlsym(RTLD_NEXT, "_dl_find_object");
	if (next_find_object)
		return 0;
	snprintf(error, error_size,
	         "host dynamic linker does not support unwind lookup");
	return -1;
}

static int signed_offset(uintptr_t target, uintptr_t base, int32_t* result)
{
	if (target >= base) {
		uintptr_t distance = target - base;
		if (distance > INT32_MAX)
			return -1;
		*result = distance;
		return 0;
	}
	uintptr_t distance = base - target;
	if (distance > (uintptr_t)INT32_MAX + 1)
		return -1;
	*result = -(int64_t)distance;
	return 0;
}

static int build_eh_frame_header(uint8_t* header, size_t capacity,
				 struct unwind_row* rows, size_t row_count,
				 uintptr_t image_base, uint32_t final_offset,
				 uintptr_t eh_frame)
{
	size_t indexed_count = 0;
	for (size_t index = 0; index < row_count; index++)
		if (rows[index].fde)
			indexed_count++;
	if (capacity < 12 + indexed_count * 8 || indexed_count > UINT32_MAX)
		return -1;
	header[0] = 1;
	header[1] = DW_EH_PE_DATAREL | DW_EH_PE_SDATA4;
	header[2] = DW_EH_PE_UDATA4;
	header[3] = DW_EH_PE_DATAREL | DW_EH_PE_SDATA4;
	int32_t eh_offset;
	if (signed_offset(eh_frame, (uintptr_t)header, &eh_offset) != 0)
		return -1;
	memcpy(header + 4, &eh_offset, sizeof(eh_offset));
	uint32_t count = indexed_count;
	memcpy(header + 8, &count, sizeof(count));
	size_t output_index = 0;
	for (size_t index = 0; index < row_count; index++) {
		if (!rows[index].fde)
			continue;
		uintptr_t function = image_base + rows[index].function_offset;
		uintptr_t function_end = image_base +
			(index + 1 < row_count ? rows[index + 1].function_offset :
			                         final_offset);
		if (function_end <= function)
			return -1;
		int32_t function_value;
		int32_t fde_value;
		if (signed_offset(function, (uintptr_t)header, &function_value) != 0 ||
		    signed_offset(rows[index].fde, (uintptr_t)header, &fde_value) != 0)
			return -1;
		memcpy(header + 12 + output_index * 8, &function_value,
		       sizeof(function_value));
		memcpy(header + 16 + output_index * 8, &fde_value,
		       sizeof(fde_value));
		output_index++;
	}
	return 0;
}

static int register_unwind_metadata(struct machgate_guest_image* image,
	uint8_t* metadata, size_t metadata_size, size_t frame_offset,
	register_frame_function register_frame, char* error, size_t error_size)
{
	long page_size_result = sysconf(_SC_PAGESIZE);
	if (page_size_result <= 0 ||
	    mprotect(metadata, align_up(metadata_size, page_size_result), PROT_READ) != 0) {
		snprintf(error, error_size, "could not protect guest unwind metadata");
		return -1;
	}
	register_frame(metadata + frame_offset);
	guest_unwind_registration = (struct guest_unwind_registration){
		.map_start = machgate_guest_image_mapping_start(image),
		.map_end = machgate_guest_image_mapping_end(image),
		.eh_frame_header = metadata,
	};
	return 0;
}

static void cache_unwind_metadata(struct machgate_guest_image* image,
	const struct byte_writer* writer, size_t frame_offset,
	const struct unwind_row* rows, size_t row_count,
	uint64_t personality_offsets[4][2])
{
	struct machgate_startup_cache* cache = machgate_guest_image_startup_cache(image);
	if (!cache)
		return;
	uintptr_t image_base = (uintptr_t)machgate_guest_image_validated_header(image);
	struct cached_unwind_header header = {
		.image_base = image_base, .metadata_size = writer->capacity,
		.frame_offset = frame_offset, .frame_size = writer->offset - frame_offset,
	};
	for (size_t index = 0; index < row_count; index++)
		if (rows[index].fde)
			header.index_count++;
	size_t index_bytes = header.index_count * sizeof(struct cached_unwind_index);
	size_t size = sizeof(header) + index_bytes + header.frame_size;
	unsigned char* data = malloc(size);
	if (!data)
		return;
	struct cached_unwind_index* indices = (void*)(data + sizeof(header));
	unsigned char* frames = data + sizeof(header) + index_bytes;
	memcpy(frames, writer->bytes + frame_offset, header.frame_size);
	for (size_t personality = 0; personality < 4; personality++) {
		for (size_t lsda = 0; lsda < 2; lsda++) {
			uint64_t offset = personality_offsets[personality][lsda];
			header.personality_offsets[personality][lsda] = UINT64_MAX;
			if (offset == UINT64_MAX)
				continue;
			header.personality_offsets[personality][lsda] = offset - frame_offset;
			memset(frames + offset - frame_offset, 0, sizeof(uintptr_t));
		}
	}
	size_t output_index = 0;
	for (size_t index = 0; index < row_count; index++) {
		if (!rows[index].fde)
			continue;
		int guest_fde = (rows[index].encoding & UNWIND_ARM64_MODE_MASK) == UNWIND_ARM64_MODE_DWARF;
		uintptr_t base = guest_fde ? image_base : (uintptr_t)(writer->bytes + frame_offset);
		indices[output_index++] = (struct cached_unwind_index){
			.function_offset = rows[index].function_offset,
			.guest_fde = guest_fde, .fde_offset = rows[index].fde - base,
		};
	}
	memcpy(data, &header, sizeof(header));
	machgate_startup_cache_put(cache, MACHGATE_CACHE_UNWIND, data, size);
	free(data);
}

static int cached_unwind_is_valid(const struct cached_unwind_header* header,
	size_t size, const struct machgate_guest_image* image)
{
	uintptr_t image_base = (uintptr_t)machgate_guest_image_validated_header(image);
	if (size < sizeof(*header) || header->image_base != image_base ||
	    header->index_count > (size - sizeof(*header)) / sizeof(struct cached_unwind_index) ||
	    header->metadata_size > 128u * 1024u * 1024u ||
	    header->frame_offset > header->metadata_size || header->frame_size < 4 ||
	    header->frame_size > header->metadata_size - header->frame_offset ||
	    header->frame_offset < 12 + header->index_count * 8)
		return 0;
	size_t index_bytes = header->index_count * sizeof(struct cached_unwind_index);
	if (header->frame_size != size - sizeof(*header) - index_bytes)
		return 0;
	const struct cached_unwind_index* indices = (const void*)(header + 1);
	for (size_t index = 0; index < header->index_count; index++) {
		if (indices[index].function_offset > UINTPTR_MAX - image_base ||
		    !mapped_range_valid(image, image_base + indices[index].function_offset, 1) ||
		    (index && indices[index].function_offset <= indices[index - 1].function_offset))
			return 0;
		if (indices[index].guest_fde) {
			if (indices[index].guest_fde != 1 || indices[index].fde_offset > UINTPTR_MAX - image_base ||
			    !mapped_range_valid(image, image_base + indices[index].fde_offset, 8))
				return 0;
		} else if (header->frame_size < 8 || indices[index].fde_offset > header->frame_size - 8) {
			return 0;
		}
	}
	for (size_t personality = 0; personality < 4; personality++)
		for (size_t lsda = 0; lsda < 2; lsda++) {
			uint64_t offset = header->personality_offsets[personality][lsda];
			if (offset != UINT64_MAX && (!personality || header->frame_size < 8 ||
			    offset > header->frame_size - 8))
				return 0;
		}
	return 1;
}

static int restore_cached_unwind(struct machgate_guest_image* image,
	const struct mapped_section* compact, register_frame_function register_frame,
	char* error, size_t error_size)
{
	size_t size;
	const struct cached_unwind_header* header = machgate_startup_cache_get(
		machgate_guest_image_startup_cache(image), MACHGATE_CACHE_UNWIND, &size);
	if (!header || !cached_unwind_is_valid(header, size, image))
		return 0;
	const struct compact_unwind_header* compact_header = section_bytes(compact, 0, sizeof(*compact_header));
	if (!compact_header)
		return 0;
	uintptr_t personalities[4] = {0};
	for (size_t personality = 1; personality < 4; personality++) {
		if (header->personality_offsets[personality][0] == UINT64_MAX &&
		    header->personality_offsets[personality][1] == UINT64_MAX)
			continue;
		personalities[personality] = resolve_personality(compact, compact_header, image, personality << 28);
		if (!personalities[personality] || personalities[personality] == UINTPTR_MAX)
			return 0;
	}
	uint8_t* metadata = machgate_pool_alloc(image, header->metadata_size);
	if (!metadata)
		return 0;
	const struct cached_unwind_index* indices = (const void*)(header + 1);
	const unsigned char* frames = (const void*)(indices + header->index_count);
	memcpy(metadata + header->frame_offset, frames, header->frame_size);
	for (size_t personality = 1; personality < 4; personality++)
		for (size_t lsda = 0; lsda < 2; lsda++) {
			uint64_t offset = header->personality_offsets[personality][lsda];
			if (offset != UINT64_MAX)
				memcpy(metadata + header->frame_offset + offset, &personalities[personality], sizeof(uintptr_t));
		}
	metadata[0] = 1;
	metadata[1] = DW_EH_PE_DATAREL | DW_EH_PE_SDATA4;
	metadata[2] = DW_EH_PE_UDATA4;
	metadata[3] = DW_EH_PE_DATAREL | DW_EH_PE_SDATA4;
	int32_t frame_offset = header->frame_offset;
	uint32_t count = header->index_count;
	memcpy(metadata + 4, &frame_offset, sizeof(frame_offset));
	memcpy(metadata + 8, &count, sizeof(count));
	for (size_t index = 0; index < header->index_count; index++) {
		uintptr_t fde_base = indices[index].guest_fde ? header->image_base : (uintptr_t)(metadata + header->frame_offset);
		int32_t function_value;
		int32_t fde_value;
		if (signed_offset(header->image_base + indices[index].function_offset, (uintptr_t)metadata, &function_value) != 0 ||
		    signed_offset(fde_base + indices[index].fde_offset, (uintptr_t)metadata, &fde_value) != 0) {
			munmap(metadata, header->metadata_size);
			return 0;
		}
		memcpy(metadata + 12 + index * 8, &function_value, sizeof(function_value));
		memcpy(metadata + 16 + index * 8, &fde_value, sizeof(fde_value));
	}
	if (register_unwind_metadata(image, metadata, header->metadata_size, header->frame_offset,
				     register_frame, error, error_size) != 0)
		return -1;
	if (machgate_env_truthy(getenv("MACHGATE_LOG_STARTUP")) ||
	    machgate_env_truthy(getenv("MACHGATE_VERBOSE")))
		fprintf(stderr, "machgate: startup cache: restored unwind\n");
	return 1;
}

int machgate_register_guest_unwind(struct machgate_guest_image* image,
				   char* error, size_t error_size)
{
	if (resolve_next_find_object(error, error_size) != 0)
		return -1;

	struct mapped_section compact = {0};
	struct mapped_section eh_frame = {0};
	int compact_result = find_mapped_section(
		image, "__TEXT", "__unwind_info", &compact);
	int eh_result =
		find_mapped_section(image, "__TEXT", "__eh_frame", &eh_frame);
	if (compact_result < 0 || eh_result < 0) {
		snprintf(error, error_size, "Mach-O has invalid unwind sections");
		return -1;
	}
	if (!compact_result && !eh_result)
		return 0;
	register_frame_function register_frame = resolve_register_frame();
	if (!register_frame) {
		snprintf(error, error_size,
		         "host unwinder does not support frame registration");
		return -1;
	}
	if (!compact_result) {
		register_frame(eh_frame.address);
		return 0;
	}
	int cached_result = restore_cached_unwind(image, &compact, register_frame, error, error_size);
	if (cached_result != 0)
		return cached_result < 0 ? -1 : 0;

	size_t row_capacity = compact.size / sizeof(uint32_t);
	struct unwind_row* rows = calloc(row_capacity, sizeof(*rows));
	if (!rows) {
		snprintf(error, error_size, "could not allocate guest unwind rows");
		return -1;
	}
	size_t row_count = 0;
	uint32_t final_offset = 0;
	uintptr_t image_base = (uintptr_t)machgate_guest_image_validated_header(image);
	if (compact_result &&
	    collect_unwind_rows(&compact, image, rows, &row_count,
	                        row_capacity, &final_offset, error,
	                        error_size) != 0) {
		free(rows);
		return -1;
	}
	if (!row_count) {
		free(rows);
		if (eh_result)
			register_frame(eh_frame.address);
		return 0;
	}

	if (row_count > (SIZE_MAX - 4096) / 192) {
		free(rows);
		snprintf(error, error_size, "guest unwind metadata size overflows");
		return -1;
	}
	size_t header_capacity = align_up(12 + row_count * 8, 8);
	size_t metadata_size = header_capacity + row_count * 192 + 4096;
	uint8_t* metadata = machgate_pool_alloc(image, metadata_size);
	if (!metadata) {
		free(rows);
		snprintf(error, error_size, "could not allocate nearby guest unwind metadata");
		return -1;
	}
	struct byte_writer writer = {
		.bytes = metadata,
		.capacity = metadata_size,
		.offset = header_capacity,
	};
	uintptr_t cie_offsets[4][2];
	uint64_t personality_offsets[4][2];
	memset(personality_offsets, 0xff, sizeof(personality_offsets));
	for (size_t personality = 0; personality < 4; personality++)
		for (size_t lsda = 0; lsda < 2; lsda++)
			cie_offsets[personality][lsda] = UINTPTR_MAX;

	struct compact_unwind_header* compact_header =
		compact_result ? section_bytes(&compact, 0, sizeof(*compact_header)) :
		                 NULL;
	for (size_t index = 0; index < row_count; index++) {
		uint32_t encoding = rows[index].encoding;
		uint32_t mode = encoding & UNWIND_ARM64_MODE_MASK;
		uintptr_t function_start = image_base + rows[index].function_offset;
		uintptr_t function_end = image_base +
			(index + 1 < row_count ? rows[index + 1].function_offset :
			                         final_offset);
		if (mode == UNWIND_ARM64_MODE_DWARF) {
			size_t offset = encoding & UNWIND_ARM64_DWARF_SECTION_OFFSET;
			if (!offset)
				continue;
			if (!eh_result || !dwarf_fde_valid(&eh_frame, offset)) {
				snprintf(error, error_size,
				         "Mach-O compact unwind row has an invalid DWARF FDE");
				free(rows);
				return -1;
			}
			rows[index].fde = (uintptr_t)eh_frame.address + offset;
			continue;
		}
		if (encoding == 0)
			continue;
		if (mode != UNWIND_ARM64_MODE_FRAME &&
		    mode != UNWIND_ARM64_MODE_FRAMELESS) {
			snprintf(error, error_size,
			         "Mach-O compact unwind row has unsupported encoding 0x%08x",
			         encoding);
			free(rows);
			return -1;
		}
		uint32_t personality_index =
			(encoding & UNWIND_PERSONALITY_MASK) >> 28;
		int has_lsda = (encoding & UNWIND_HAS_LSDA) != 0;
		uintptr_t personality = resolve_personality(
			&compact, compact_header, image, encoding);
		if (personality == UINTPTR_MAX || (personality_index && !personality)) {
			snprintf(error, error_size,
			         "Mach-O compact unwind row has an invalid personality");
			free(rows);
			return -1;
		}
		if (has_lsda && !rows[index].lsda) {
			snprintf(error, error_size,
			         "Mach-O compact unwind row has no matching LSDA");
			free(rows);
			return -1;
		}
		if (cie_offsets[personality_index][has_lsda] == UINTPTR_MAX &&
		    emit_cie(&writer, personality, has_lsda,
		             (size_t*)&cie_offsets[personality_index][has_lsda],
		             &personality_offsets[personality_index][has_lsda]) != 0) {
			snprintf(error, error_size, "could not encode guest unwind CIE");
			free(rows);
			return -1;
		}
		if (emit_fde(&writer, cie_offsets[personality_index][has_lsda],
		             function_start, function_end, encoding, rows[index].lsda,
		             has_lsda, &rows[index].fde) != 0) {
			snprintf(error, error_size, "could not encode guest unwind FDE");
			free(rows);
			return -1;
		}
	}
	if (writer_u32(&writer, 0) != 0 ||
	    build_eh_frame_header(metadata, header_capacity, rows, row_count,
	                          image_base, final_offset,
	                          (uintptr_t)(metadata + header_capacity)) != 0) {
		snprintf(error, error_size, "could not encode guest unwind index");
		free(rows);
		return -1;
	}

	if (register_unwind_metadata(image, metadata, metadata_size, header_capacity,
				     register_frame, error, error_size) != 0) {
		free(rows);
		return -1;
	}
	cache_unwind_metadata(image, &writer, header_capacity, rows, row_count, personality_offsets);
	free(rows);
	return 0;
}

int _dl_find_object(void* address, struct dl_find_object* result)
{
	if (machgate_find_dynamic_unwind(address, result) == 0)
		return 0;
	uintptr_t value = (uintptr_t)address;
	if (guest_unwind_registration.eh_frame_header &&
	    value >= guest_unwind_registration.map_start &&
	    value < guest_unwind_registration.map_end) {
		memset(result, 0, sizeof(*result));
		result->dlfo_map_start = (void*)guest_unwind_registration.map_start;
		result->dlfo_map_end = (void*)guest_unwind_registration.map_end;
		result->dlfo_eh_frame = guest_unwind_registration.eh_frame_header;
#if DLFO_STRUCT_HAS_EH_DBASE
		result->dlfo_eh_dbase = NULL;
#endif
		return 0;
	}
	return next_find_object ? next_find_object(address, result) : -1;
}
