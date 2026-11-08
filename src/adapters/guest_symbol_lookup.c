#include "ports/guest_symbol_lookup.h"

#include "macho_defs.h"
#include "ports/guest_image_view.h"
#include "ports/guest_libraries.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

struct address_symbol {
	uintptr_t address;
	const char* name;
	uint8_t section;
	uint8_t external;
};

struct image_symbol_index {
	struct machgate_guest_image* image;
	struct address_symbol* symbols;
	size_t count;
	struct image_symbol_index* next;
};

static pthread_mutex_t symbol_lock = PTHREAD_MUTEX_INITIALIZER;
static struct image_symbol_index* symbol_indexes;

void machgate_release_guest_symbol_lookup(void)
{
	pthread_mutex_lock(&symbol_lock);
	while (symbol_indexes) {
		struct image_symbol_index* next = symbol_indexes->next;
		free(symbol_indexes->symbols);
		free(symbol_indexes);
		symbol_indexes = next;
	}
	pthread_mutex_unlock(&symbol_lock);
}

int machgate_describe_guest_image_v1(uint32_t index, struct machgate_image_description* description)
{
	struct machgate_guest_image* image = machgate_guest_image_at_index(index);
	if (!image || !description)
		return 0;
	description->path = machgate_guest_image_path(image);
	description->header = machgate_guest_image_validated_header(image);
	description->slide = machgate_guest_image_slide(image);
	return 1;
}

static void* mapped_file_range(struct machgate_guest_image* image, uint64_t offset, uint64_t size)
{
	struct mach_header_64* header = machgate_guest_image_validated_header(image);
	uint8_t* cursor = (uint8_t*)(header + 1);
	for (uint32_t index = 0; index < header->ncmds; index++) {
		struct load_command* command = (void*)cursor;
		if (command->cmd == LC_SEGMENT_64) {
			struct segment_command_64* segment = (void*)command;
			if (offset >= segment->fileoff && offset - segment->fileoff <= segment->filesize &&
			    size <= segment->filesize - (offset - segment->fileoff)) {
				uintptr_t address = segment->vmaddr + machgate_guest_image_slide(image) +
				                    offset - segment->fileoff;
				return machgate_guest_image_contains(image, address, size) ? (void*)address : NULL;
			}
		}
		cursor += command->cmdsize;
	}
	return NULL;
}

static int compare_symbols(const void* left, const void* right)
{
	const struct address_symbol* first = left;
	const struct address_symbol* second = right;
	if (first->section != second->section)
		return first->section < second->section ? -1 : 1;
	if (first->address != second->address)
		return first->address < second->address ? -1 : 1;
	return (int)first->external - (int)second->external;
}

static struct image_symbol_index* index_symbols(struct machgate_guest_image* image)
{
	for (struct image_symbol_index* index = symbol_indexes; index; index = index->next)
		if (index->image == image)
			return index;
	struct mach_header_64* header = machgate_guest_image_validated_header(image);
	struct symtab_command* table = NULL;
	uint8_t* cursor = (uint8_t*)(header + 1);
	for (uint32_t index = 0; index < header->ncmds; index++) {
		struct load_command* command = (void*)cursor;
		if (command->cmd == LC_SYMTAB)
			table = (void*)command;
		cursor += command->cmdsize;
	}
	if (!table)
		return NULL;
	const struct nlist_64* symbols = mapped_file_range(image, table->symoff,
		(uint64_t)table->nsyms * sizeof(struct nlist_64));
	const char* strings = mapped_file_range(image, table->stroff, table->strsize);
	if (!symbols || !strings)
		return NULL;
	struct image_symbol_index* result = calloc(1, sizeof(*result));
	if (!result)
		return NULL;
	result->symbols = calloc(table->nsyms, sizeof(*result->symbols));
	if (!result->symbols) {
		free(result);
		return NULL;
	}
	result->image = image;
	for (uint32_t index = 0; index < table->nsyms; index++) {
		const struct nlist_64* symbol = &symbols[index];
		if ((symbol->n_type & N_STAB) || (symbol->n_type & N_TYPE) != N_SECT ||
		    !symbol->n_sect || symbol->n_strx >= table->strsize)
			continue;
		const char* name = strings + symbol->n_strx;
		if (!*name || !memchr(name, 0, table->strsize - symbol->n_strx))
			continue;
		result->symbols[result->count++] = (struct address_symbol){
			.address = symbol->n_value + machgate_guest_image_slide(image),
			.name = name[0] == '_' ? name + 1 : name,
			.section = symbol->n_sect,
			.external = (symbol->n_type & N_EXT) != 0,
		};
	}
	qsort(result->symbols, result->count, sizeof(*result->symbols), compare_symbols);
	result->next = symbol_indexes;
	symbol_indexes = result;
	return result;
}

static unsigned section_for_address(struct machgate_guest_image* image, uintptr_t address)
{
	struct mach_header_64* header = machgate_guest_image_validated_header(image);
	uint8_t* cursor = (uint8_t*)(header + 1);
	unsigned section_number = 1;
	for (uint32_t index = 0; index < header->ncmds; index++) {
		struct load_command* command = (void*)cursor;
		if (command->cmd == LC_SEGMENT_64) {
			struct segment_command_64* segment = (void*)command;
			const struct section_64* sections = (void*)(segment + 1);
			for (uint32_t section = 0; section < segment->nsects; section++, section_number++) {
				uintptr_t begin = sections[section].addr + machgate_guest_image_slide(image);
				if (address >= begin && address - begin < sections[section].size)
					return section_number;
			}
		}
		cursor += command->cmdsize;
	}
	return 0;
}

static void describe_symbol(struct machgate_guest_image* image, uintptr_t address,
	struct machgate_address_description* description)
{
	unsigned section = section_for_address(image, address);
	if (!section)
		return;
	pthread_mutex_lock(&symbol_lock);
	struct image_symbol_index* index = index_symbols(image);
	if (index) {
		size_t first = 0;
		size_t last = index->count;
		while (first < last) {
			size_t middle = first + (last - first) / 2;
			const struct address_symbol* symbol = &index->symbols[middle];
			if (symbol->section < section || (symbol->section == section && symbol->address <= address))
				first = middle + 1;
			else
				last = middle;
		}
		if (first && index->symbols[first - 1].section == section) {
			const struct address_symbol* symbol = &index->symbols[first - 1];
			if ((void*)symbol->address != description->image.header) {
				description->symbol_name = symbol->name;
				description->symbol_address = (void*)symbol->address;
			}
		}
	}
	pthread_mutex_unlock(&symbol_lock);
}

int machgate_describe_guest_address_v1(const void* address, struct machgate_address_description* description)
{
	if (!address || !description)
		return 0;
	for (uint32_t index = 0;; index++) {
		struct machgate_guest_image* image = machgate_guest_image_at_index(index);
		if (!image)
			return 0;
		if (!machgate_guest_image_contains(image, (uintptr_t)address, 1))
			continue;
		memset(description, 0, sizeof(*description));
		machgate_describe_guest_image_v1(index, &description->image);
		if (address == description->image.header) {
			description->symbol_name = "__dso_handle";
			description->symbol_address = description->image.header;
		} else {
			describe_symbol(image, (uintptr_t)address, description);
		}
		return 1;
	}
}
