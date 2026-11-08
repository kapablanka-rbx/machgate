#include "loader.h"

#include "instruction_adaptation.h"
#include "macho_defs.h"
#include "ports/guest_image_view.h"
#include "adapters/startup_cache.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

#define MACHGATE_MAX_LOAD_COMMAND_BYTES (16u * 1024u * 1024u)
#define MACHGATE_MAX_MAPPED_SEGMENTS 128u
#define MACHGATE_POOL_SEARCH_STEP (1024u * 1024u)
#define MACHGATE_PLATFORM_MACOS 1u
#define MACHGATE_PLATFORM_IOS 2u

struct file_slice {
	uint64_t offset;
	uint64_t size;
};

struct mapped_segment {
	uintptr_t address;
	size_t size;
	int protection;
};

struct machgate_guest_image {
	struct machgate_startup_cache* startup_cache;
	char* path;
	void* reservation;
	size_t reservation_size;
	void* header;
	intptr_t slide;
	uintptr_t mapping_start;
	uintptr_t mapping_end;
	uintptr_t entry_point;
	uint32_t platform;
	struct mapped_segment segments[MACHGATE_MAX_MAPPED_SEGMENTS];
	size_t segment_count;
};

struct load_context {
	int file_descriptor;
	uint32_t expected_file_type;
	intptr_t slide;
	void* reservation;
	size_t reservation_size;
	uintptr_t minimum_virtual_address;
	uintptr_t maximum_virtual_address;
	struct file_slice slice;
	struct mach_header_64 header;
	uint8_t* commands;
	struct mapped_segment segments[MACHGATE_MAX_MAPPED_SEGMENTS];
	size_t segment_count;
	uintptr_t mapping_start;
	uintptr_t mapping_end;
};

static uint32_t swap32(uint32_t value)
{
	return __builtin_bswap32(value);
}

static uint64_t swap64(uint64_t value)
{
	return __builtin_bswap64(value);
}

static int range_valid(uint64_t offset, uint64_t size, uint64_t limit)
{
	return offset <= limit && size <= limit - offset;
}

static int read_exact(int file_descriptor, void* buffer, size_t size,
		      uint64_t offset, char* error, size_t error_size)
{
	if (offset > INT64_MAX || size > (uint64_t)INT64_MAX - offset) {
		snprintf(error, error_size, "Mach-O read range is too large");
		return -1;
	}

	size_t complete = 0;
	while (complete < size) {
		ssize_t result = pread(file_descriptor, (char*)buffer + complete,
		                       size - complete, (off_t)(offset + complete));
		if (result == 0) {
			snprintf(error, error_size, "unexpected end of Mach-O file");
			return -1;
		}
		if (result < 0) {
			if (errno == EINTR)
				continue;
			snprintf(error, error_size, "could not read Mach-O file: %s",
			         strerror(errno));
			return -1;
		}
		complete += (size_t)result;
	}
	return 0;
}

static int select_fat32_slice(int file_descriptor, uint64_t file_size,
			      uint32_t architecture_count,
			      struct file_slice* slice, char* error,
			      size_t error_size)
{
	uint64_t table_size =
		(uint64_t)architecture_count * sizeof(struct fat_arch);
	if (architecture_count == 0 ||
	    !range_valid(sizeof(struct fat_header), table_size, file_size)) {
		snprintf(error, error_size, "invalid fat Mach-O architecture table");
		return -1;
	}

	for (uint32_t index = 0; index < architecture_count; index++) {
		struct fat_arch architecture;
		uint64_t offset = sizeof(struct fat_header) +
		                  (uint64_t)index * sizeof(architecture);
		if (read_exact(file_descriptor, &architecture, sizeof(architecture),
		               offset, error, error_size) != 0)
			return -1;
		if (swap32(architecture.cputype) != CPU_TYPE_ARM64)
			continue;

		slice->offset = swap32(architecture.offset);
		slice->size = swap32(architecture.size);
		if (!range_valid(slice->offset, slice->size, file_size)) {
			snprintf(error, error_size, "ARM64 fat Mach-O slice is truncated");
			return -1;
		}
		return 0;
	}

	snprintf(error, error_size, "fat Mach-O contains no ARM64 slice");
	return -1;
}

static int select_fat64_slice(int file_descriptor, uint64_t file_size,
			      uint32_t architecture_count,
			      struct file_slice* slice, char* error,
			      size_t error_size)
{
	uint64_t table_size =
		(uint64_t)architecture_count * sizeof(struct fat_arch_64);
	if (architecture_count == 0 ||
	    !range_valid(sizeof(struct fat_header), table_size, file_size)) {
		snprintf(error, error_size, "invalid fat64 Mach-O architecture table");
		return -1;
	}

	for (uint32_t index = 0; index < architecture_count; index++) {
		struct fat_arch_64 architecture;
		uint64_t offset = sizeof(struct fat_header) +
		                  (uint64_t)index * sizeof(architecture);
		if (read_exact(file_descriptor, &architecture, sizeof(architecture),
		               offset, error, error_size) != 0)
			return -1;
		if (swap32(architecture.cputype) != CPU_TYPE_ARM64)
			continue;

		slice->offset = swap64(architecture.offset);
		slice->size = swap64(architecture.size);
		if (!range_valid(slice->offset, slice->size, file_size)) {
			snprintf(error, error_size,
			         "ARM64 fat64 Mach-O slice is truncated");
			return -1;
		}
		return 0;
	}

	snprintf(error, error_size, "fat64 Mach-O contains no ARM64 slice");
	return -1;
}

static int select_slice(int file_descriptor, uint64_t file_size,
			struct file_slice* slice, char* error,
			size_t error_size)
{
	struct fat_header header;
	if (file_size < sizeof(header) ||
	    read_exact(file_descriptor, &header, sizeof(header), 0, error,
	               error_size) != 0)
		return -1;

	if (header.magic == MH_MAGIC_64) {
		slice->offset = 0;
		slice->size = file_size;
		return 0;
	}

	uint32_t architecture_count = swap32(header.nfat_arch);
	if (header.magic == FAT_CIGAM)
		return select_fat32_slice(file_descriptor, file_size,
		                          architecture_count, slice, error,
		                          error_size);
	if (header.magic == FAT_CIGAM_64)
		return select_fat64_slice(file_descriptor, file_size,
		                          architecture_count, slice, error,
		                          error_size);

	snprintf(error, error_size, "input is not a supported Mach-O file");
	return -1;
}

static int read_load_commands(struct load_context* context, char* error,
			      size_t error_size)
{
	if (context->slice.size < sizeof(context->header) ||
	    read_exact(context->file_descriptor, &context->header,
	               sizeof(context->header), context->slice.offset, error,
	               error_size) != 0)
		return -1;

	if (context->header.magic != MH_MAGIC_64 ||
	    context->header.cputype != CPU_TYPE_ARM64 ||
	    context->header.filetype != context->expected_file_type) {
		snprintf(error, error_size,
		         "Mach-O slice is not an ARM64 %s",
		         context->expected_file_type == MH_EXECUTE ? "executable" : "dylib");
		return -1;
	}
	if (context->header.sizeofcmds > MACHGATE_MAX_LOAD_COMMAND_BYTES ||
	    !range_valid(sizeof(context->header), context->header.sizeofcmds,
	                 context->slice.size)) {
		snprintf(error, error_size, "invalid Mach-O load-command table");
		return -1;
	}

	context->commands = malloc(context->header.sizeofcmds);
	if (!context->commands) {
		snprintf(error, error_size,
		         "could not allocate Mach-O load-command table");
		return -1;
	}
	return read_exact(context->file_descriptor, context->commands,
	                  context->header.sizeofcmds,
	                  context->slice.offset + sizeof(context->header), error,
	                  error_size);
}

static int select_guest_platform_override(struct machgate_guest_image* image,
					  int* overridden, char* error,
					  size_t error_size)
{
	const char* override = getenv("MACHGATE_GUEST_PLATFORM");
	*overridden = 0;
	if (!override || !*override)
		return 0;
	if (strcmp(override, "macos") == 0) {
		image->platform = MACHGATE_PLATFORM_MACOS;
		*overridden = 1;
		return 0;
	}
	if (strcmp(override, "ios") == 0) {
		image->platform = MACHGATE_PLATFORM_IOS;
		*overridden = 1;
		return 0;
	}
	snprintf(error, error_size,
		 "MACHGATE_GUEST_PLATFORM must be macos or ios");
	return -1;
}

static int select_guest_platform(struct load_context* context,
				 struct machgate_guest_image* image,
				 char* error, size_t error_size)
{
	image->platform = MACHGATE_PLATFORM_MACOS;
	int overridden;
	if (select_guest_platform_override(image, &overridden, error,
					   error_size) != 0)
		return -1;
	if (overridden)
		return 0;

	uint8_t* cursor = context->commands;
	for (uint32_t index = 0; index < context->header.ncmds; index++) {
		struct load_command* command = (void*)cursor;
		if (command->cmd == LC_BUILD_VERSION) {
			if (command->cmdsize < sizeof(struct build_version_command)) {
				snprintf(error, error_size,
					 "invalid LC_BUILD_VERSION command");
				return -1;
			}
			struct build_version_command* build_version = (void*)command;
			if (build_version->platform == MACHGATE_PLATFORM_IOS)
				image->platform = MACHGATE_PLATFORM_IOS;
		}
		cursor += command->cmdsize;
	}
	return 0;
}

static int native_protection(int protection)
{
	int result = PROT_NONE;
	if (protection & VM_PROT_READ)
		result |= PROT_READ;
	if (protection & VM_PROT_WRITE)
		result |= PROT_WRITE;
	if (protection & VM_PROT_EXECUTE)
		result |= PROT_EXEC;
	return result;
}

static int validate_segment(const struct load_context* context,
			    const struct segment_command_64* segment,
			    char* error, size_t error_size)
{
	if (segment->cmdsize < sizeof(*segment)) {
		snprintf(error, error_size, "invalid Mach-O segment command");
		return -1;
	}
	uint64_t section_bytes =
		(uint64_t)segment->nsects * sizeof(struct section_64);
	if (section_bytes > segment->cmdsize - sizeof(*segment)) {
		snprintf(error, error_size, "invalid Mach-O segment command");
		return -1;
	}
	if (!range_valid(segment->fileoff, segment->filesize,
	                 context->slice.size) ||
	    (segment->vmsize != 0 && segment->filesize > segment->vmsize)) {
		snprintf(error, error_size, "invalid Mach-O segment file range");
		return -1;
	}
	if (segment->vmaddr > UINTPTR_MAX ||
	    segment->vmsize > UINTPTR_MAX - segment->vmaddr) {
		snprintf(error, error_size, "Mach-O segment address overflows host");
		return -1;
	}
	return 0;
}

static int map_segment(struct load_context* context,
		       const struct segment_command_64* segment,
		       char* error, size_t error_size)
{
	if (validate_segment(context, segment, error, error_size) != 0)
		return -1;
	if (segment->vmsize == 0 || segment->initprot == 0)
		return 0;
	if (context->segment_count == MACHGATE_MAX_MAPPED_SEGMENTS) {
		snprintf(error, error_size, "Mach-O contains too many segments");
		return -1;
	}

	long page_size_result = sysconf(_SC_PAGESIZE);
	if (page_size_result <= 0) {
		snprintf(error, error_size, "could not determine host page size");
		return -1;
	}
	uintptr_t page_size = (uintptr_t)page_size_result;
	uintptr_t virtual_address = (uintptr_t)segment->vmaddr + context->slide;
	uintptr_t segment_end = virtual_address + (uintptr_t)segment->vmsize;
	uintptr_t map_start = virtual_address & ~(page_size - 1);
	uintptr_t map_end = (segment_end + page_size - 1) & ~(page_size - 1);
	if (map_end < segment_end || map_end <= map_start) {
		snprintf(error, error_size, "invalid Mach-O mapped segment size");
		return -1;
	}

	size_t map_size = map_end - map_start;
	if (context->reservation) {
		uintptr_t reserved_start = (uintptr_t)context->reservation;
		if (map_start < reserved_start || map_end - reserved_start > context->reservation_size) {
			snprintf(error, error_size, "Mach-O segment exceeds its reserved image range");
			return -1;
		}
		for (size_t index = 0; index < context->segment_count; index++) {
			const struct mapped_segment* previous = &context->segments[index];
			if (map_start < previous->address + previous->size && map_end > previous->address) {
				snprintf(error, error_size, "Mach-O dylib segments overlap");
				return -1;
			}
		}
	}
	void* mapping = mmap((void*)map_start, map_size,
	                     PROT_READ | PROT_WRITE,
	                     MAP_PRIVATE | MAP_ANONYMOUS |
	                         (context->reservation ? MAP_FIXED : MAP_FIXED_NOREPLACE),
	                     -1, 0);
	if (mapping == MAP_FAILED) {
		snprintf(error, error_size,
		         "could not map Mach-O segment at 0x%" PRIxPTR ": %s",
		         map_start, strerror(errno));
		return -1;
	}

	if (segment->filesize &&
	    read_exact(context->file_descriptor, (void*)virtual_address,
	               (size_t)segment->filesize,
	               context->slice.offset + segment->fileoff, error,
	               error_size) != 0) {
		munmap(mapping, map_size);
		return -1;
	}

	struct mapped_segment* mapped =
		&context->segments[context->segment_count++];
	mapped->address = map_start;
	mapped->size = map_size;
	mapped->protection = native_protection(segment->initprot);
	if (!context->mapping_start || map_start < context->mapping_start)
		context->mapping_start = map_start;
	if (map_end > context->mapping_end)
		context->mapping_end = map_end;
	return 0;
}

static int walk_segments(struct load_context* context,
	int (*visit)(struct load_context*, const struct segment_command_64*, char*, size_t),
	char* error, size_t error_size)
{
	uint8_t* cursor = context->commands;
	uint8_t* end = context->commands + context->header.sizeofcmds;

	for (uint32_t index = 0; index < context->header.ncmds; index++) {
		if ((size_t)(end - cursor) < sizeof(struct load_command)) {
			snprintf(error, error_size, "truncated Mach-O load command");
			return -1;
		}
		struct load_command* command = (void*)cursor;
		if (command->cmdsize < sizeof(*command) ||
		    command->cmdsize > (size_t)(end - cursor)) {
			snprintf(error, error_size, "invalid Mach-O load command size");
			return -1;
		}
		if (command->cmd == LC_SEGMENT_64 &&
		    visit(context, (void*)command, error, error_size) != 0)
			return -1;
		cursor += command->cmdsize;
	}

	if (cursor != end) {
		snprintf(error, error_size, "Mach-O load-command size mismatch");
		return -1;
	}
	return 0;
}

static int measure_library_segment(struct load_context* context,
	const struct segment_command_64* segment, char* error, size_t error_size)
{
	if (validate_segment(context, segment, error, error_size) != 0)
		return -1;
	if (!segment->vmsize || !segment->initprot)
		return 0;
	if (segment->vmaddr < context->minimum_virtual_address)
		context->minimum_virtual_address = segment->vmaddr;
	if (segment->vmaddr + segment->vmsize > context->maximum_virtual_address)
		context->maximum_virtual_address = segment->vmaddr + segment->vmsize;
	return 0;
}

static int reserve_library_image(struct load_context* context,
	char* error, size_t error_size)
{
	if (context->expected_file_type == MH_EXECUTE)
		return 0;
	context->minimum_virtual_address = UINTPTR_MAX;
	if (walk_segments(context, measure_library_segment, error, error_size) != 0)
		return -1;
	long page_size_result = sysconf(_SC_PAGESIZE);
	if (page_size_result <= 0 ||
	    context->minimum_virtual_address >= context->maximum_virtual_address ||
	    context->maximum_virtual_address > INTPTR_MAX - (uintptr_t)page_size_result) {
		snprintf(error, error_size, "Mach-O dylib has an invalid virtual address range");
		return -1;
	}
	uintptr_t page_mask = (uintptr_t)page_size_result - 1;
	uintptr_t minimum = context->minimum_virtual_address & ~page_mask;
	uintptr_t maximum = (context->maximum_virtual_address + page_mask) & ~page_mask;
	context->reservation_size = maximum - minimum;
	void* result = mmap(NULL, context->reservation_size, PROT_NONE,
	                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (result == MAP_FAILED) {
		snprintf(error, error_size, "could not reserve Mach-O dylib address space: %s", strerror(errno));
		return -1;
	}
	context->reservation = result;
	context->slide = (intptr_t)result - (intptr_t)minimum;
	return 0;
}

static int locate_header_and_entry(struct load_context* context,
				   struct machgate_guest_image* image,
				   char* error, size_t error_size)
{
	uint8_t* cursor = context->commands;
	uint8_t* end = context->commands + context->header.sizeofcmds;
	uint64_t entry_offset = 0;
	uintptr_t unix_thread_entry = 0;
	int has_main_entry = 0;

	for (uint32_t index = 0; index < context->header.ncmds; index++) {
		struct load_command* command = (void*)cursor;
		if (command->cmd == LC_MAIN) {
			if (command->cmdsize < sizeof(struct entry_point_command)) {
				snprintf(error, error_size, "invalid LC_MAIN command");
				return -1;
			}
			struct entry_point_command* entry = (void*)command;
			entry_offset = entry->entryoff;
			has_main_entry = 1;
		} else if (command->cmd == LC_UNIXTHREAD &&
		           command->cmdsize >= sizeof(*command) + 8 +
		                               sizeof(struct arm_thread_state64)) {
			struct arm_thread_state64* state =
				(void*)(cursor + sizeof(*command) + 8);
			unix_thread_entry = (uintptr_t)state->pc;
		}
		cursor += command->cmdsize;
	}

	cursor = context->commands;
	while (cursor < end) {
		struct load_command* command = (void*)cursor;
		if (command->cmd == LC_SEGMENT_64) {
			struct segment_command_64* segment = (void*)command;
			if (segment->fileoff == 0 && segment->filesize >= sizeof(context->header))
				image->header = (void*)((uintptr_t)segment->vmaddr + context->slide);
			if (has_main_entry && entry_offset >= segment->fileoff &&
			    entry_offset - segment->fileoff < segment->filesize) {
				image->entry_point = (uintptr_t)segment->vmaddr + context->slide +
				                      (uintptr_t)(entry_offset - segment->fileoff);
			}
		}
		cursor += command->cmdsize;
	}

	if (!image->entry_point && unix_thread_entry)
		image->entry_point = unix_thread_entry + context->slide;
	if (!image->header || !machgate_guest_image_contains(image, (uintptr_t)image->header,
	    sizeof(context->header) + context->header.sizeofcmds) ||
	    (context->expected_file_type == MH_EXECUTE && !image->entry_point)) {
		snprintf(error, error_size,
		         "Mach-O executable has no supported mapped entry point");
		return -1;
	}
	return 0;
}

static int protect_segments(struct load_context* context, char* error,
			    size_t error_size)
{
	for (size_t index = 0; index < context->segment_count; index++) {
		struct mapped_segment* segment = &context->segments[index];
		if (mprotect((void*)segment->address, segment->size,
		             segment->protection) != 0) {
			snprintf(error, error_size,
			         "could not protect Mach-O segment at 0x%" PRIxPTR ": %s",
			         segment->address, strerror(errno));
			return -1;
		}
		if (segment->protection & PROT_EXEC)
			__builtin___clear_cache((char*)segment->address,
			                        (char*)segment->address + segment->size);
	}
	return 0;
}

static void unmap_segments(struct load_context* context)
{
	if (context->reservation) {
		munmap(context->reservation, context->reservation_size);
		return;
	}
	for (size_t index = 0; index < context->segment_count; index++)
		munmap((void*)context->segments[index].address,
		       context->segments[index].size);
}

static int load_guest_image(const char* path, uint32_t file_type,
			     struct machgate_guest_image** image_result,
			     char* error, size_t error_size)
{
	if (!image_result) {
		snprintf(error, error_size, "Guest Image output is required");
		return -1;
	}
	*image_result = NULL;
	struct machgate_guest_image* image = calloc(1, sizeof(*image));
	if (!image) {
		snprintf(error, error_size, "could not allocate Guest Image");
		return -1;
	}

	struct load_context context;
	memset(&context, 0, sizeof(context));
	context.file_descriptor = -1;
	context.expected_file_type = file_type;

	context.file_descriptor = open(path, O_RDONLY | O_CLOEXEC);
	if (context.file_descriptor < 0) {
		snprintf(error, error_size, "could not open %s: %s", path,
		         strerror(errno));
		free(image);
		return -1;
	}

	struct stat status;
	if (fstat(context.file_descriptor, &status) != 0 || status.st_size < 0) {
		snprintf(error, error_size, "could not inspect %s: %s", path,
		         strerror(errno));
		close(context.file_descriptor);
		free(image);
		return -1;
	}

	int load_result = -1;
	if (select_slice(context.file_descriptor, (uint64_t)status.st_size,
	                 &context.slice, error, error_size) != 0 ||
	    read_load_commands(&context, error, error_size) != 0 ||
	    reserve_library_image(&context, error, error_size) != 0 ||
	    walk_segments(&context, map_segment, error, error_size) != 0)
		goto done;

	image->slide = context.slide;
	image->reservation = context.reservation;
	image->reservation_size = context.reservation_size;
	image->mapping_start = context.mapping_start;
	image->mapping_end = context.mapping_end;
	memcpy(image->segments, context.segments,
	       context.segment_count * sizeof(context.segments[0]));
	image->segment_count = context.segment_count;
	if (locate_header_and_entry(&context, image, error, error_size) != 0 ||
	    select_guest_platform(&context, image, error, error_size) != 0)
		goto done;
	image->path = realpath(path, NULL);
	if (!image->path) {
		snprintf(error, error_size, "could not resolve Mach-O image path: %s", strerror(errno));
		goto done;
	}
	image->startup_cache = machgate_startup_cache_begin(path, &status, context.slice.offset,
				   context.slice.size);
	if (machgate_adapt_guest_instructions(image, error, error_size) < 0 ||
	    protect_segments(&context, error, error_size) != 0)
		goto done;
	*image_result = image;
	load_result = 0;

done:
	if (load_result != 0) {
		machgate_startup_cache_close(image->startup_cache);
		unmap_segments(&context);
		free(image->path);
		free(image);
	}
	free(context.commands);
	close(context.file_descriptor);
	return load_result;
}

int machgate_load_executable(const char* path,
	struct machgate_guest_image** image, char* error, size_t error_size)
{
	return load_guest_image(path, MH_EXECUTE, image, error, error_size);
}

int machgate_load_guest_library(const char* path,
	struct machgate_guest_image** image, char* error, size_t error_size)
{
	return load_guest_image(path, MH_DYLIB, image, error, error_size);
}

void machgate_unload_guest_image(struct machgate_guest_image* image)
{
	if (!image)
		return;
	machgate_startup_cache_close(image->startup_cache);
	if (image->reservation)
		munmap(image->reservation, image->reservation_size);
	else
		for (size_t index = 0; index < image->segment_count; index++)
			munmap((void*)image->segments[index].address,
			       image->segments[index].size);
	free(image->path);
	free(image);
}

const char* machgate_guest_image_path(const struct machgate_guest_image* image)
{
	return image ? image->path : NULL;
}

struct machgate_startup_cache* machgate_guest_image_startup_cache(
	const struct machgate_guest_image* image)
{
	return image ? image->startup_cache : NULL;
}

void machgate_complete_guest_image_preparation(struct machgate_guest_image* image)
{
	if (!image)
		return;
	machgate_startup_cache_publish(image->startup_cache);
	machgate_startup_cache_close(image->startup_cache);
	image->startup_cache = NULL;
}

void* machgate_guest_image_validated_header(
	const struct machgate_guest_image* image)
{
	return image ? image->header : NULL;
}

intptr_t machgate_guest_image_slide(const struct machgate_guest_image* image)
{
	return image ? image->slide : 0;
}

uintptr_t machgate_guest_image_mapping_start(
	const struct machgate_guest_image* image)
{
	return image ? image->mapping_start : 0;
}

uintptr_t machgate_guest_image_mapping_end(
	const struct machgate_guest_image* image)
{
	return image ? image->mapping_end : 0;
}

uintptr_t machgate_guest_image_entry_point(
	const struct machgate_guest_image* image)
{
	return image ? image->entry_point : 0;
}

int machgate_guest_image_platform(const struct machgate_guest_image* image)
{
	return image ? (int)image->platform : 0;
}

int machgate_guest_image_contains(const struct machgate_guest_image* image,
				 uintptr_t address, uint64_t size)
{
	if (!image || address < image->mapping_start ||
	    address > image->mapping_end)
		return 0;
	for (size_t index = 0; index < image->segment_count; index++) {
		const struct mapped_segment* segment = &image->segments[index];
		if (address >= segment->address && address - segment->address <= segment->size &&
		    size <= segment->size - (address - segment->address))
			return 1;
	}
	return 0;
}

static uintptr_t align_up(uintptr_t value, size_t alignment)
{
	return (value + alignment - 1) & ~((uintptr_t)alignment - 1);
}

void* machgate_pool_alloc(struct machgate_guest_image* image, size_t size)
{
	if (!image || !image->mapping_end || !size)
		return NULL;

	long page_size_result = sysconf(_SC_PAGESIZE);
	if (page_size_result <= 0)
		return NULL;
	size_t page_size = (size_t)page_size_result;
	size = align_up(size, page_size);
	uintptr_t start = align_up(image->mapping_end, page_size);

	for (uintptr_t distance = 0; distance < (1u << 27);
	     distance += MACHGATE_POOL_SEARCH_STEP) {
		uintptr_t address = start + distance;
		void* mapping = mmap((void*)address, size,
		                     PROT_READ | PROT_WRITE | PROT_EXEC,
		                     MAP_PRIVATE | MAP_ANONYMOUS |
		                         MAP_FIXED_NOREPLACE,
		                     -1, 0);
		if (mapping != MAP_FAILED)
			return mapping;
	}
	return NULL;
}
