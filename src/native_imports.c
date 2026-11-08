#include "native_imports.h"
#include "adapters/startup_cache.h"
#include "adapters/guest_rebase.h"

#include "guest_symbols.h"
#include "loader.h"
#include "macho_defs.h"
#include "native_provider_guest_import.h"
#include "ports/native_unwind.h"
#include "native_provider_map.h"
#include "native_structor_adapter.h"
#include "ports/guest_image_view.h"
#include "ports/guest_libraries.h"
#include "ports/native_provider_image.h"

#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <link.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define MACHGATE_MAX_IMPORT_SEGMENTS 128u
#define MACHGATE_MAX_NATIVE_PROVIDERS 128u

#define BIND_OPCODE_MASK 0xf0u
#define BIND_IMMEDIATE_MASK 0x0fu
#define BIND_OPCODE_DONE 0x00u
#define BIND_OPCODE_SET_DYLIB_ORDINAL_IMM 0x10u
#define BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB 0x20u
#define BIND_OPCODE_SET_DYLIB_SPECIAL_IMM 0x30u
#define BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM 0x40u
#define BIND_OPCODE_SET_TYPE_IMM 0x50u
#define BIND_OPCODE_SET_ADDEND_SLEB 0x60u
#define BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB 0x70u
#define BIND_OPCODE_ADD_ADDR_ULEB 0x80u
#define BIND_OPCODE_DO_BIND 0x90u
#define BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB 0xa0u
#define BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED 0xb0u
#define BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB 0xc0u

#define BIND_SYMBOL_FLAGS_WEAK_IMPORT 0x1u
#define BIND_TYPE_POINTER 1u

#define BIND_SPECIAL_DYLIB_SELF 0
#define BIND_SPECIAL_DYLIB_MAIN_EXECUTABLE -1
#define BIND_SPECIAL_DYLIB_FLAT_LOOKUP -2
#define BIND_SPECIAL_DYLIB_WEAK_LOOKUP -3

#define DYLD_CHAINED_IMPORT 1u
#define DYLD_CHAINED_IMPORT_ADDEND 2u
#define DYLD_CHAINED_IMPORT_ADDEND64 3u
#define DYLD_CHAINED_PTR_64 2u
#define DYLD_CHAINED_PTR_64_OFFSET 6u
#define DYLD_CHAINED_PTR_START_NONE 0xffffu
#define DYLD_CHAINED_PTR_START_MULTI 0x8000u
#define DYLD_CHAINED_PTR_START_LAST 0x8000u

struct chained_fixups_header {
	uint32_t fixups_version;
	uint32_t starts_offset;
	uint32_t imports_offset;
	uint32_t symbols_offset;
	uint32_t imports_count;
	uint32_t imports_format;
	uint32_t symbols_format;
};

struct chained_starts_in_segment {
	uint32_t size;
	uint16_t page_size;
	uint16_t pointer_format;
	uint64_t segment_offset;
	uint32_t max_valid_pointer;
	uint16_t page_count;
	uint16_t page_start[];
};

struct chained_import {
	int provider_ordinal;
	int weak_import;
	int64_t addend;
	const char* symbol;
};

struct import_segment {
	const struct segment_command_64* command;
	uintptr_t address;
	uint64_t size;
	int protection;
};

struct native_provider {
	const char* install_name;
	enum machgate_provider_decision_kind decision;
	void* handle;
	struct machgate_guest_image* guest_image;
	machgate_guest_import_resolver_function guest_import_resolver;
};

static struct native_provider dynamic_providers[MACHGATE_MAX_NATIVE_PROVIDERS];
static size_t dynamic_provider_count;

struct unresolved_data_pool {
	uint8_t* bytes;
	size_t used;
	size_t capacity;
};

struct import_context {
	struct machgate_guest_image* image;
	struct dyld_info_command* dyld_info;
	struct linkedit_data_command* chained_fixups;
	struct symtab_command* symbol_table;
	struct dysymtab_command* dynamic_symbol_table;
	struct import_segment segments[MACHGATE_MAX_IMPORT_SEGMENTS];
	size_t segment_count;
	struct native_provider providers[MACHGATE_MAX_NATIVE_PROVIDERS];
	size_t provider_count;
	struct machgate_structor_adapters* structor_adapters;
	struct unresolved_data_pool unresolved_data;
	uintptr_t* called_pointers;
	size_t called_pointer_count;
	size_t called_pointer_capacity;
	int allow_unresolved_stubs;
	long page_size;
	char* error;
	size_t error_size;
};

struct bind_state {
	int provider_ordinal;
	uint8_t segment_index;
	uint8_t type;
	uint8_t symbol_flags;
	uint64_t segment_offset;
	int64_t addend;
	const char* symbol;
};

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

static void* address_for_file_offset(struct import_context* context,
				     uint64_t file_offset,
				     uint64_t required_size)
{
	struct mach_header_64* header = machgate_guest_image_validated_header(context->image);
	uint8_t* cursor = (uint8_t*)(header + 1);
	for (uint32_t index = 0; index < header->ncmds; index++) {
		struct load_command* command = (void*)cursor;
		if (command->cmd == LC_SEGMENT_64) {
			struct segment_command_64* segment = (void*)command;
			if (file_offset >= segment->fileoff &&
			    file_offset - segment->fileoff <= segment->filesize &&
			    required_size <= segment->filesize -
			                     (file_offset - segment->fileoff)) {
				return (void*)(uintptr_t)(segment->vmaddr +
				                         machgate_guest_image_slide(context->image) +
				                         file_offset - segment->fileoff);
			}
		}
		cursor += command->cmdsize;
	}
	return NULL;
}

static int read_uleb128(const uint8_t** cursor, const uint8_t* end,
			uint64_t* value)
{
	uint64_t result = 0;
	unsigned shift = 0;
	while (*cursor < end && shift < 64) {
		uint8_t byte = *(*cursor)++;
		if (shift == 63 && (byte & 0x7eu))
			return -1;
		result |= (uint64_t)(byte & 0x7fu) << shift;
		if (!(byte & 0x80u)) {
			*value = result;
			return 0;
		}
		shift += 7;
	}
	return -1;
}

static int read_sleb128(const uint8_t** cursor, const uint8_t* end,
			int64_t* value)
{
	uint64_t result = 0;
	unsigned shift = 0;
	uint8_t byte = 0;
	while (*cursor < end && shift < 64) {
		byte = *(*cursor)++;
		result |= (uint64_t)(byte & 0x7fu) << shift;
		shift += 7;
		if (!(byte & 0x80u)) {
			if (shift < 64 && (byte & 0x40u))
				result |= UINT64_MAX << shift;
			*value = (int64_t)result;
			return 0;
		}
	}
	return -1;
}

static int range_valid(uint64_t offset, uint64_t size, uint64_t limit)
{
	return offset <= limit && size <= limit - offset;
}

static int provider_owns_address(void* handle, void* address)
{
	struct link_map* provider_map = NULL;
	Dl_info address_info;
	if (!address ||
	    dlinfo(handle, RTLD_DI_LINKMAP, &provider_map) != 0 ||
	    !provider_map || dladdr(address, &address_info) == 0)
		return 0;
	return address_info.dli_fbase ==
	       (void*)(uintptr_t)provider_map->l_addr;
}

int machgate_resolve_native_symbol_v1(void* handle, const char* name,
				     void** address)
{
	if (!name || !address)
		return 0;
	*address = NULL;
	for (size_t index = 0; index < dynamic_provider_count; index++) {
		struct native_provider* provider = &dynamic_providers[index];
		if (!provider->handle || (handle && handle != provider->handle))
			continue;
		if (provider->guest_import_resolver &&
		    provider->guest_import_resolver(name, address))
			return 1;
		void* result = dlsym(provider->handle, name);
		if (provider_owns_address(provider->handle, result)) {
			*address = result;
			return 1;
		}
	}
	return 0;
}

static int add_provider(struct import_context* context,
			const struct dylib_command* command)
{
	if (context->provider_count == MACHGATE_MAX_NATIVE_PROVIDERS) {
		snprintf(context->error, context->error_size,
		         "Mach-O has too many native dependencies");
		return -1;
	}
	if (command->dylib.name_offset >= command->cmdsize) {
		snprintf(context->error, context->error_size,
		         "Mach-O contains an invalid dylib name");
		return -1;
	}

	const char* name = (const char*)command + command->dylib.name_offset;
	size_t remaining = command->cmdsize - command->dylib.name_offset;
	if (!memchr(name, '\0', remaining)) {
		snprintf(context->error, context->error_size,
		         "Mach-O contains an unterminated dylib name");
		return -1;
	}

	struct native_provider* provider =
		&context->providers[context->provider_count++];
	provider->install_name = name;
	char path[PATH_MAX];
	if (machgate_native_provider_decision(
		    name, &provider->decision, path, sizeof(path), context->error,
		    context->error_size) != 0)
		return -1;
	if (provider->decision == MACHGATE_PROVIDER_UNMAPPED) {
		int result = machgate_open_guest_dependency(context->image, name,
		    &provider->guest_image, context->error, context->error_size);
		if (result < 0)
			return -1;
	}
	if (provider->decision != MACHGATE_PROVIDER_MAP &&
	    provider->decision != MACHGATE_PROVIDER_PARTIAL)
		return 0;
	provider->handle = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
	if (!provider->handle) {
		snprintf(context->error, context->error_size,
		         "could not load native provider for %s: %s", name,
		         dlerror());
		return -1;
	}
	union {
		void* object;
		machgate_guest_import_resolver_function function;
	} resolver = {
		.object = dlsym(provider->handle,
		                MACHGATE_GUEST_IMPORT_RESOLVER_SYMBOL),
	};
	if (provider_owns_address(provider->handle, resolver.object))
		provider->guest_import_resolver = resolver.function;
	return 0;
}

static int collect_load_commands(struct import_context* context)
{
	struct mach_header_64* header = machgate_guest_image_validated_header(context->image);
	uint8_t* cursor = (uint8_t*)(header + 1);
	for (uint32_t index = 0; index < header->ncmds; index++) {
		struct load_command* command = (void*)cursor;
		if (command->cmd == LC_SEGMENT_64) {
			if (context->segment_count == MACHGATE_MAX_IMPORT_SEGMENTS) {
				snprintf(context->error, context->error_size,
				         "Mach-O has too many import segments");
				return -1;
			}
			struct segment_command_64* segment = (void*)command;
			if (command->cmdsize < sizeof(*segment)) {
				snprintf(context->error, context->error_size,
				         "Mach-O has invalid import segment sections");
				return -1;
			}
			uint64_t section_bytes =
				(uint64_t)segment->nsects * sizeof(struct section_64);
			if (section_bytes > command->cmdsize - sizeof(*segment)) {
				snprintf(context->error, context->error_size,
				         "Mach-O has invalid import segment sections");
				return -1;
			}
			struct import_segment* result =
				&context->segments[context->segment_count++];
			result->command = segment;
			result->address = segment->vmaddr + machgate_guest_image_slide(context->image);
			result->size = segment->vmsize;
			result->protection = native_protection(segment->initprot);
		} else if (command->cmd == LC_DYLD_INFO_ONLY) {
			if (command->cmdsize < sizeof(struct dyld_info_command)) {
				snprintf(context->error, context->error_size,
				         "Mach-O has an invalid dyld info command");
				return -1;
			}
			context->dyld_info = (void*)command;
		} else if (command->cmd == LC_SYMTAB) {
			if (command->cmdsize < sizeof(struct symtab_command)) {
				snprintf(context->error, context->error_size,
				         "Mach-O has an invalid symbol-table command");
				return -1;
			}
			context->symbol_table = (void*)command;
		} else if (command->cmd == LC_DYSYMTAB) {
			if (command->cmdsize < sizeof(struct dysymtab_command)) {
				snprintf(context->error, context->error_size,
				         "Mach-O has an invalid dynamic-symbol-table command");
				return -1;
			}
			context->dynamic_symbol_table = (void*)command;
		} else if (command->cmd == LC_DYLD_CHAINED_FIXUPS) {
			if (command->cmdsize < sizeof(struct linkedit_data_command)) {
				snprintf(context->error, context->error_size,
				         "Mach-O has an invalid chained-fixups command");
				return -1;
			}
			context->chained_fixups = (void*)command;
		} else if (command->cmd == LC_LOAD_DYLIB ||
		           command->cmd == LC_LOAD_WEAK_DYLIB ||
		           command->cmd == LC_REEXPORT_DYLIB ||
		           command->cmd == LC_LOAD_UPWARD_DYLIB) {
			if (command->cmdsize < sizeof(struct dylib_command) ||
			    add_provider(context, (void*)command) != 0)
				return -1;
		}
		cursor += command->cmdsize;
	}
	return 0;
}

static void unresolved_stub_binder(void)
{
	_exit(127);
}

static uintptr_t unresolved_return_zero(void)
{
	return 0;
}

static void compiler_clear_cache(void* start, void* end)
{
	__builtin___clear_cache(start, end);
}

static long double compiler_floattidf(__int128 value)
{
	return (long double)value;
}

extern __int128 machgate_host_divti3(__int128 dividend, __int128 divisor)
	__asm__("__divti3");

static const char* normalized_native_name(const char* guest_name,
	                                  char* storage,
	                                  size_t storage_size)
{
	static const char* suffixes[] = {
		"$DARWIN_EXTSN",
		"$INODE64",
		"$NOCANCEL",
	};
	const char* result = guest_name[0] == '_' ? guest_name + 1 : guest_name;
	size_t result_length = strlen(result);

	for (size_t index = 0; index < sizeof(suffixes) / sizeof(suffixes[0]);
	     index++) {
		size_t suffix_length = strlen(suffixes[index]);
		if (result_length < suffix_length ||
		    strcmp(result + result_length - suffix_length,
		           suffixes[index]) != 0)
			continue;
		size_t base_length = result_length - suffix_length;
		if (base_length + 1 > storage_size)
			return result;
		memcpy(storage, result, base_length);
		storage[base_length] = '\0';
		return storage;
	}
	return result;
}

static void* find_compiler_runtime_symbol(const char* native_name)
{
	if (strcmp(native_name, "__register_frame") == 0 ||
	    strcmp(native_name, "__unw_add_dynamic_fde") == 0)
		return machgate_guest_register_frame;
	if (strcmp(native_name, "__deregister_frame") == 0 ||
	    strcmp(native_name, "__unw_remove_dynamic_fde") == 0)
		return machgate_guest_deregister_frame;
	if (strcmp(native_name, "__clear_cache") == 0)
		return compiler_clear_cache;
	if (strcmp(native_name, "__floattidf") == 0)
		return compiler_floattidf;
	if (strcmp(native_name, "__divti3") == 0)
		return machgate_host_divti3;
	return NULL;
}

static void* find_provider_symbol(struct native_provider* provider,
				  const char* native_name)
{
	void* symbol = NULL;
	if (provider->guest_import_resolver &&
	    provider->guest_import_resolver(native_name, &symbol))
		return symbol;
	return dlsym(provider->handle, native_name);
}

static void* find_dependency_symbol(struct import_context* context,
				    const char* native_name, const char* guest_name,
				    int* mapped_native_provider)
{
	for (size_t index = 0; index < context->provider_count; index++) {
		struct native_provider* provider = &context->providers[index];
		if (provider->guest_image) {
			void* result = machgate_find_guest_definition(provider->guest_image, guest_name);
			if (result)
				return result;
		}
		if (!provider->handle)
			continue;
		void* symbol = find_provider_symbol(provider, native_name);
		if (symbol) {
			*mapped_native_provider = 1;
			return symbol;
		}
	}
	return NULL;
}

static void* find_symbol(struct import_context* context,
			 struct bind_state* state,
			 int* mapped_native_provider)
{
	*mapped_native_provider = 0;
	if (!state->symbol)
		return NULL;
	if (strcmp(state->symbol, "dyld_stub_binder") == 0)
		return unresolved_stub_binder;

	char normalized_name[1024];
	const char* native_name = normalized_native_name(
		state->symbol, normalized_name, sizeof(normalized_name));
	if (state->provider_ordinal > 0) {
		size_t index = (size_t)state->provider_ordinal - 1;
		if (index >= context->provider_count)
			return NULL;
		struct native_provider* provider = &context->providers[index];
		if (provider->guest_image)
			return machgate_find_guest_definition(provider->guest_image, state->symbol);
		if (provider->handle) {
			void* symbol = find_provider_symbol(provider, native_name);
			if (symbol) {
				*mapped_native_provider = 1;
				return symbol;
			}
		}
		if (provider->decision == MACHGATE_PROVIDER_MAP ||
		    provider->decision == MACHGATE_PROVIDER_PARTIAL)
			return find_compiler_runtime_symbol(native_name);
		return NULL;
	}

	if (state->provider_ordinal == BIND_SPECIAL_DYLIB_SELF ||
	    state->provider_ordinal == BIND_SPECIAL_DYLIB_MAIN_EXECUTABLE ||
	    state->provider_ordinal == BIND_SPECIAL_DYLIB_FLAT_LOOKUP ||
	    state->provider_ordinal == BIND_SPECIAL_DYLIB_WEAK_LOOKUP) {
		struct machgate_guest_image* search_image =
			state->provider_ordinal == BIND_SPECIAL_DYLIB_MAIN_EXECUTABLE
			? machgate_main_guest_image() : context->image;
		if (!search_image)
			search_image = context->image;
		void* symbol = machgate_find_guest_definition(search_image,
		                                                state->symbol);
		if (symbol)
			return symbol;
	}
	if (state->provider_ordinal != BIND_SPECIAL_DYLIB_FLAT_LOOKUP &&
	    state->provider_ordinal != BIND_SPECIAL_DYLIB_WEAK_LOOKUP)
		return NULL;

	void* symbol = find_dependency_symbol(context, native_name, state->symbol,
	                                      mapped_native_provider);
	if (symbol)
		return symbol;
	symbol = dlsym(RTLD_DEFAULT, native_name);
	if (symbol) {
		*mapped_native_provider = 1;
		return symbol;
	}
	return find_compiler_runtime_symbol(native_name);
}

static int add_native_symbol_addend(struct import_context* context,
				    uintptr_t address, int64_t addend,
				    uintptr_t* result)
{
	if (addend >= 0) {
		if ((uint64_t)addend <= UINTPTR_MAX - address) {
			*result = address + (uintptr_t)addend;
			return 0;
		}
	} else {
		uint64_t magnitude = (uint64_t)(-(addend + 1)) + 1u;
		if (magnitude <= address) {
			*result = address - (uintptr_t)magnitude;
			return 0;
		}
	}
	snprintf(context->error, context->error_size,
	         "native symbol addend overflows host address space");
	return -1;
}

static int resolved_symbol_value(struct import_context* context,
				 struct bind_state* state, void* symbol,
				 int mapped_native_provider,
				 uintptr_t* result)
{
	uintptr_t target;
	if (add_native_symbol_addend(context, (uintptr_t)symbol, state->addend,
	                             &target) != 0)
		return -1;
	if (!mapped_native_provider) {
		*result = target;
		return 0;
	}
	return machgate_adapt_native_structor(
		context->image, &context->structor_adapters, state->symbol, target,
		result, context->error, context->error_size);
}

enum unresolved_import_action {
	UNRESOLVED_IMPORT_ERROR,
	UNRESOLVED_IMPORT_ZERO,
	UNRESOLVED_IMPORT_TRAP,
	UNRESOLVED_IMPORT_RETURN_ZERO,
	UNRESOLVED_IMPORT_DATA,
};

static int import_context_allows_unresolved_stubs(void)
{
	const char* policy = getenv("MACHGATE_UNRESOLVED_IMPORTS");
	if (!policy || !*policy || strcmp(policy, "strict") == 0)
		return 0;
	if (strcmp(policy, "stub") == 0)
		return 1;
	fprintf(stderr,
	        "machgate: MACHGATE_UNRESOLVED_IMPORTS must be strict or stub; using strict\n");
	return 0;
}

static void* allocate_unresolved_data(struct import_context* context)
{
	const size_t alignment = 64;
	if (!context->unresolved_data.bytes) {
		context->unresolved_data.capacity = 64 * 1024;
		context->unresolved_data.bytes = mmap(
			NULL, context->unresolved_data.capacity,
			PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (context->unresolved_data.bytes == MAP_FAILED) {
			context->unresolved_data.bytes = NULL;
			context->unresolved_data.capacity = 0;
			return NULL;
		}
	}
	size_t offset = (context->unresolved_data.used + alignment - 1) &
	                ~(alignment - 1);
	if (offset > context->unresolved_data.capacity ||
	    alignment > context->unresolved_data.capacity - offset)
		return NULL;
	context->unresolved_data.used = offset + alignment;
	return context->unresolved_data.bytes + offset;
}

static int bind_targets_lazy_symbol_pointer(
	struct import_context* context, const struct bind_state* state)
{
	if (state->segment_index >= context->segment_count)
		return 0;

	const struct segment_command_64* segment =
		context->segments[state->segment_index].command;
	if (!segment || state->segment_offset > UINT64_MAX - segment->vmaddr)
		return 0;
	uint64_t target = segment->vmaddr + state->segment_offset;
	const struct section_64* sections = (const void*)(segment + 1);

	for (uint32_t index = 0; index < segment->nsects; index++) {
		const struct section_64* section = &sections[index];
		if ((section->flags & SECTION_TYPE) != S_LAZY_SYMBOL_POINTERS ||
		    target < section->addr)
			continue;
		uint64_t section_offset = target - section->addr;
		if (section_offset <= section->size &&
		    sizeof(uintptr_t) <= section->size - section_offset)
			return 1;
	}
	return 0;
}

static int symbol_has_stub_entry(struct import_context* context,
				 const char* symbol_name)
{
	const struct symtab_command* symbol_table = context->symbol_table;
	const struct dysymtab_command* dynamic_symbol_table =
		context->dynamic_symbol_table;
	if (!symbol_name || !symbol_table || !dynamic_symbol_table)
		return 0;

	uint64_t symbol_bytes =
		(uint64_t)symbol_table->nsyms * sizeof(struct nlist_64);
	const struct nlist_64* symbols = address_for_file_offset(
		context, symbol_table->symoff, symbol_bytes);
	const char* strings = address_for_file_offset(
		context, symbol_table->stroff, symbol_table->strsize);
	uint64_t indirect_bytes =
		(uint64_t)dynamic_symbol_table->nindirectsyms * sizeof(uint32_t);
	const uint32_t* indirect_symbols = address_for_file_offset(
		context, dynamic_symbol_table->indirectsymoff, indirect_bytes);
	if (!symbols || !strings || !indirect_symbols)
		return 0;

	for (size_t segment_index = 0; segment_index < context->segment_count;
	     segment_index++) {
		const struct segment_command_64* segment =
			context->segments[segment_index].command;
		const struct section_64* sections = (const void*)(segment + 1);
		for (uint32_t section_index = 0;
		     section_index < segment->nsects; section_index++) {
			const struct section_64* section = &sections[section_index];
			if ((section->flags & SECTION_TYPE) != S_SYMBOL_STUBS ||
			    !section->reserved2 ||
			    section->size % section->reserved2 != 0)
				continue;

			uint64_t stub_count = section->size / section->reserved2;
			if (section->reserved1 >
				    dynamic_symbol_table->nindirectsyms ||
			    stub_count > dynamic_symbol_table->nindirectsyms -
			                         section->reserved1)
				continue;

			for (uint64_t stub_index = 0; stub_index < stub_count;
			     stub_index++) {
				uint32_t symbol_index = indirect_symbols[
					section->reserved1 + stub_index];
				if (symbol_index &
				    (INDIRECT_SYMBOL_LOCAL | INDIRECT_SYMBOL_ABS))
					continue;
				if (symbol_index >= symbol_table->nsyms)
					continue;
				uint32_t string_index = symbols[symbol_index].n_strx;
				if (string_index >= symbol_table->strsize)
					continue;
				const char* candidate = strings + string_index;
				if (!memchr(candidate, '\0',
				            symbol_table->strsize - string_index))
					continue;
				if (strcmp(candidate, symbol_name) == 0)
					return 1;
			}
		}
	}
	return 0;
}

static int record_called_pointer(struct import_context* context,
				 uintptr_t address)
{
	if (context->called_pointer_count == context->called_pointer_capacity) {
		size_t capacity = context->called_pointer_capacity
			                  ? context->called_pointer_capacity * 2 : 32;
		uintptr_t* result = realloc(context->called_pointers,
		                            capacity * sizeof(*result));
		if (!result) {
			snprintf(context->error, context->error_size,
			         "could not allocate native call reference table");
			return -1;
		}
		context->called_pointers = result;
		context->called_pointer_capacity = capacity;
	}
	context->called_pointers[context->called_pointer_count++] = address;
	return 0;
}

enum reference_kind { REFERENCE_UNKNOWN, REFERENCE_ADDRESS, REFERENCE_POINTER };

static int collect_section_call_pointers(struct import_context* context,
					uintptr_t address, uint64_t size)
{
	uintptr_t references[32] = {0};
	enum reference_kind kinds[32] = {REFERENCE_UNKNOWN};
	for (uint64_t offset = 0; offset + sizeof(uint32_t) <= size; offset += 4) {
		uint32_t instruction;
		memcpy(&instruction, (void*)(address + offset), sizeof(instruction));
		unsigned int destination = instruction & 31u;
		unsigned int source = (instruction >> 5) & 31u;
		if ((instruction & 0x9f000000u) == 0x90000000u) {
			uint32_t immediate = ((instruction >> 29) & 3u) |
			                     (((instruction >> 5) & 0x7ffffu) << 2);
			int64_t pages = (int64_t)(immediate ^ 0x100000u) - 0x100000;
			references[destination] = ((address + offset) & ~(uintptr_t)4095) +
			                          pages * 4096;
			kinds[destination] = REFERENCE_ADDRESS;
		} else if ((instruction & 0xff800000u) == 0x91000000u) {
			uintptr_t immediate = (instruction >> 10) & 0xfffu;
			if (instruction & (1u << 22))
				immediate <<= 12;
			references[destination] = references[source] + immediate;
			kinds[destination] = kinds[source] == REFERENCE_ADDRESS
				                     ? REFERENCE_ADDRESS : REFERENCE_UNKNOWN;
		} else if ((instruction & 0xffc00000u) == 0xf9400000u) {
			references[destination] = references[source] +
			                          (((instruction >> 10) & 0xfffu) * 8);
			kinds[destination] = kinds[source] == REFERENCE_ADDRESS
				                     ? REFERENCE_POINTER : REFERENCE_UNKNOWN;
		} else if ((instruction & 0xffe0ffe0u) == 0xaa0003e0u) {
			source = (instruction >> 16) & 31u;
			references[destination] = references[source];
			kinds[destination] = kinds[source];
		} else if ((instruction & 0x1f800000u) == 0x12800000u) {
			kinds[destination] = REFERENCE_UNKNOWN;
		} else if (instruction != 0xd503201fu) {
			uint32_t branch = instruction & 0xfffffc1fu;
			if ((branch == 0xd63f0000u || branch == 0xd61f0000u) &&
			    kinds[source] == REFERENCE_POINTER &&
			    record_called_pointer(context, references[source]) != 0)
				return -1;
			memset(kinds, 0, sizeof(kinds));
		}
		kinds[31] = REFERENCE_UNKNOWN;
	}
	return 0;
}

static int collect_called_pointers(struct import_context* context)
{
	size_t cached_size;
	const void* cached = machgate_startup_cache_get(
		machgate_guest_image_startup_cache(context->image), MACHGATE_CACHE_CALL_REFERENCES, &cached_size);
	if (cached && cached_size % sizeof(uintptr_t) == 0) {
		uintptr_t* result = malloc(cached_size ? cached_size : 1);
		if (result) {
			memcpy(result, cached, cached_size);
			context->called_pointers = result;
			context->called_pointer_count = cached_size / sizeof(uintptr_t);
			context->called_pointer_capacity = context->called_pointer_count;
			return 0;
		}
	}
	for (size_t index = 0; index < context->segment_count; index++) {
		const struct import_segment* segment = &context->segments[index];
		if (!(segment->protection & PROT_EXEC))
			continue;
		const struct section_64* sections = (const void*)(segment->command + 1);
		for (uint32_t section_index = 0; section_index < segment->command->nsects;
		     section_index++) {
			const struct section_64* section = &sections[section_index];
			if (!(section->flags & (S_ATTR_PURE_INSTRUCTIONS | S_ATTR_SOME_INSTRUCTIONS)))
				continue;
			uintptr_t address = section->addr + machgate_guest_image_slide(context->image);
			if (!machgate_guest_image_contains(context->image, address, section->size))
				continue;
			if (collect_section_call_pointers(context, address, section->size) != 0)
				return -1;
		}
	}
	machgate_startup_cache_put(machgate_guest_image_startup_cache(context->image), MACHGATE_CACHE_CALL_REFERENCES,
		context->called_pointers, context->called_pointer_count * sizeof(uintptr_t));
	return 0;
}

static int bind_pointer_is_called(struct import_context* context,
				  const struct bind_state* state)
{
	if (state->segment_index >= context->segment_count)
		return 0;
	uintptr_t address = context->segments[state->segment_index].address +
	                    state->segment_offset;
	for (size_t index = 0; index < context->called_pointer_count; index++) {
		if (context->called_pointers[index] == address)
			return 1;
	}
	return 0;
}

static enum unresolved_import_action unresolved_import_action(
	struct import_context* context, const struct bind_state* state, int lazy)
{
	int direct_function_target =
		lazy || bind_targets_lazy_symbol_pointer(context, state);
	int stubbed_function = symbol_has_stub_entry(context, state->symbol);
	if (state->symbol_flags & BIND_SYMBOL_FLAGS_WEAK_IMPORT)
		return UNRESOLVED_IMPORT_ZERO;
	if (state->provider_ordinal == BIND_SPECIAL_DYLIB_WEAK_LOOKUP)
		return UNRESOLVED_IMPORT_ZERO;
	if (context->allow_unresolved_stubs &&
	    (direct_function_target || stubbed_function ||
	     bind_pointer_is_called(context, state)))
		return UNRESOLVED_IMPORT_RETURN_ZERO;
	if (state->provider_ordinal > 0 &&
	    (size_t)state->provider_ordinal <= context->provider_count) {
		enum machgate_provider_decision_kind decision =
			context->providers[state->provider_ordinal - 1].decision;
		if (decision == MACHGATE_PROVIDER_SKIP)
			return UNRESOLVED_IMPORT_ZERO;
		if (decision == MACHGATE_PROVIDER_PARTIAL)
			return direct_function_target || stubbed_function
				       ? UNRESOLVED_IMPORT_TRAP
				       : UNRESOLVED_IMPORT_ZERO;
		if (decision == MACHGATE_PROVIDER_STUB)
			return context->allow_unresolved_stubs
				       ? UNRESOLVED_IMPORT_RETURN_ZERO
				       : UNRESOLVED_IMPORT_TRAP;

		if (decision == MACHGATE_PROVIDER_MAP && stubbed_function)
			return context->allow_unresolved_stubs
				       ? UNRESOLVED_IMPORT_RETURN_ZERO
				       : UNRESOLVED_IMPORT_TRAP;
	}
	if (direct_function_target || stubbed_function)
		return context->allow_unresolved_stubs
		       ? UNRESOLVED_IMPORT_RETURN_ZERO
		       : UNRESOLVED_IMPORT_TRAP;
	if (state->provider_ordinal > 0 &&
	    (size_t)state->provider_ordinal <= context->provider_count &&
	    context->providers[state->provider_ordinal - 1].decision ==
	    MACHGATE_PROVIDER_UNMAPPED && !lazy &&
	    !context->allow_unresolved_stubs)
		return UNRESOLVED_IMPORT_ERROR;
	if (context->allow_unresolved_stubs)
		return UNRESOLVED_IMPORT_DATA;
	return UNRESOLVED_IMPORT_ERROR;
}

static const char* provider_description(struct import_context* context,
					const struct bind_state* state)
{
	if (state->provider_ordinal > 0 &&
	    (size_t)state->provider_ordinal <= context->provider_count)
		return context->providers[state->provider_ordinal - 1].install_name;
	if (state->provider_ordinal == BIND_SPECIAL_DYLIB_SELF)
		return "guest image";
	if (state->provider_ordinal == BIND_SPECIAL_DYLIB_MAIN_EXECUTABLE)
		return "main executable";
	if (state->provider_ordinal == BIND_SPECIAL_DYLIB_WEAK_LOOKUP)
		return "weak lookup";
	return "flat namespace";
}

static int write_pointer(struct import_context* context,
			 struct bind_state* state, int lazy)
{
	if (state->type != BIND_TYPE_POINTER ||
	    state->segment_index >= context->segment_count) {
		snprintf(context->error, context->error_size,
		         "Mach-O uses an unsupported native bind type");
		return -1;
	}
	struct import_segment* segment = &context->segments[state->segment_index];
	if (state->segment_offset > segment->size ||
	    sizeof(uintptr_t) > segment->size - state->segment_offset) {
		snprintf(context->error, context->error_size,
		         "Mach-O native bind address is outside its segment");
		return -1;
	}

	int mapped_native_provider;
	void* symbol = find_symbol(context, state, &mapped_native_provider);
	int bind_zero = 0;
	if (!symbol) {
		enum unresolved_import_action action =
			unresolved_import_action(context, state, lazy);
		if (action == UNRESOLVED_IMPORT_TRAP)
			symbol = unresolved_stub_binder;
		else if (action == UNRESOLVED_IMPORT_RETURN_ZERO)
			symbol = unresolved_return_zero;
		else if (action == UNRESOLVED_IMPORT_ZERO)
			bind_zero = 1;
		else if (action == UNRESOLVED_IMPORT_DATA) {
			symbol = allocate_unresolved_data(context);
			if (!symbol) {
				snprintf(context->error, context->error_size,
				         "could not allocate unresolved data for %s",
				         state->symbol);
				return -1;
			}
			fprintf(stderr,
			        "machgate: unresolved import %s from %s uses compatibility storage\n",
			        state->symbol, provider_description(context, state));
		} else if (action == UNRESOLVED_IMPORT_ERROR) {
			snprintf(context->error, context->error_size,
			         "native symbol %s was not found for %s", state->symbol,
			         provider_description(context, state));
			return -1;
		}
	}

	uintptr_t value = 0;
	if (!bind_zero &&
	    resolved_symbol_value(context, state, symbol, mapped_native_provider,
	                          &value) != 0)
		return -1;

	uintptr_t address = segment->address + state->segment_offset;
	uintptr_t page = address & ~((uintptr_t)context->page_size - 1);
	int restore_protection = segment->protection;
	if (!(restore_protection & PROT_WRITE) &&
	    mprotect((void*)page, (size_t)context->page_size,
	             restore_protection | PROT_WRITE) != 0) {
		snprintf(context->error, context->error_size,
		         "could not make native bind address writable: %s",
		         strerror(errno));
		return -1;
	}

	memcpy((void*)address, &value, sizeof(value));
	if (!(restore_protection & PROT_WRITE) &&
	    mprotect((void*)page, (size_t)context->page_size,
	             restore_protection) != 0) {
		snprintf(context->error, context->error_size,
		         "could not restore native bind protection: %s",
		         strerror(errno));
		return -1;
	}
	return 0;
}

static int write_chained_pointer(struct import_context* context,
				 struct import_segment* segment,
				 uint64_t segment_offset, uintptr_t value)
{
	if (segment_offset > segment->size ||
	    sizeof(value) > segment->size - segment_offset) {
		snprintf(context->error, context->error_size,
		         "Mach-O chained pointer is outside its segment");
		return -1;
	}

	uintptr_t address = segment->address + segment_offset;
	uintptr_t page = address & ~((uintptr_t)context->page_size - 1);
	if (!(segment->protection & PROT_WRITE) &&
	    mprotect((void*)page, (size_t)context->page_size,
	             segment->protection | PROT_WRITE) != 0) {
		snprintf(context->error, context->error_size,
		         "could not make chained pointer writable: %s",
		         strerror(errno));
		return -1;
	}

	memcpy((void*)address, &value, sizeof(value));
	if (!(segment->protection & PROT_WRITE) &&
	    mprotect((void*)page, (size_t)context->page_size,
	             segment->protection) != 0) {
		snprintf(context->error, context->error_size,
		         "could not restore chained-pointer protection: %s",
		         strerror(errno));
		return -1;
	}
	return 0;
}

static int add_offset(uint64_t* offset, uint64_t amount)
{
	if (amount > UINT64_MAX - *offset)
		return -1;
	*offset += amount;
	return 0;
}

static void add_modular_bind_offset(uint64_t* offset, uint64_t amount)
{
	*offset += amount;
}

static int apply_bind_stream(struct import_context* context,
			     uint32_t file_offset, uint32_t size,
			     int lazy, int weak_lookup)
{
	if (!size)
		return 0;
	const uint8_t* cursor = address_for_file_offset(context, file_offset, size);
	if (!cursor) {
		snprintf(context->error, context->error_size,
		         "Mach-O native bind stream is not mapped");
		return -1;
	}
	const uint8_t* end = cursor + size;
	struct bind_state state = {
		.provider_ordinal = weak_lookup ? BIND_SPECIAL_DYLIB_WEAK_LOOKUP : 0,
		.type = BIND_TYPE_POINTER,
	};
	int bind_count = 0;

	while (cursor < end) {
		uint8_t byte = *cursor++;
		uint8_t opcode = byte & BIND_OPCODE_MASK;
		uint8_t immediate = byte & BIND_IMMEDIATE_MASK;
		uint64_t value;

		switch (opcode) {
		case BIND_OPCODE_DONE:
			if (!lazy)
				return bind_count;
			memset(&state, 0, sizeof(state));
			state.provider_ordinal =
				weak_lookup ? BIND_SPECIAL_DYLIB_WEAK_LOOKUP : 0;
			state.type = BIND_TYPE_POINTER;
			break;
		case BIND_OPCODE_SET_DYLIB_ORDINAL_IMM:
			state.provider_ordinal = immediate;
			break;
		case BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB:
			if (read_uleb128(&cursor, end, &value) != 0 ||
			    value > INT32_MAX)
				goto invalid_stream;
			state.provider_ordinal = (int)value;
			break;
		case BIND_OPCODE_SET_DYLIB_SPECIAL_IMM:
			state.provider_ordinal = immediate == 0
				                         ? 0
				                         : (int8_t)(0xf0u | immediate);
			break;
		case BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM: {
			const uint8_t* terminator = memchr(cursor, '\0', end - cursor);
			if (!terminator)
				goto invalid_stream;
			state.symbol_flags = immediate;
			state.symbol = (const char*)cursor;
			cursor = terminator + 1;
			break;
		}
		case BIND_OPCODE_SET_TYPE_IMM:
			state.type = immediate;
			break;
		case BIND_OPCODE_SET_ADDEND_SLEB:
			if (read_sleb128(&cursor, end, &state.addend) != 0)
				goto invalid_stream;
			break;
		case BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
			if (read_uleb128(&cursor, end, &state.segment_offset) != 0)
				goto invalid_stream;
			state.segment_index = immediate;
			break;
		case BIND_OPCODE_ADD_ADDR_ULEB:
			if (read_uleb128(&cursor, end, &value) != 0)
				goto invalid_stream;
			add_modular_bind_offset(&state.segment_offset, value);
			break;
		case BIND_OPCODE_DO_BIND:
			if (write_pointer(context, &state, lazy) != 0 ||
			    add_offset(&state.segment_offset, sizeof(uintptr_t)) != 0)
				return -1;
			bind_count++;
			break;
		case BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB:
			if (write_pointer(context, &state, lazy) != 0 ||
			    read_uleb128(&cursor, end, &value) != 0 ||
			    add_offset(&state.segment_offset,
			               sizeof(uintptr_t) + value) != 0)
				return -1;
			bind_count++;
			break;
		case BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED:
			if (write_pointer(context, &state, lazy) != 0 ||
			    add_offset(&state.segment_offset,
			               sizeof(uintptr_t) * (immediate + 1u)) != 0)
				return -1;
			bind_count++;
			break;
		case BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB: {
			uint64_t count;
			uint64_t skip;
			if (read_uleb128(&cursor, end, &count) != 0 ||
			    read_uleb128(&cursor, end, &skip) != 0)
				goto invalid_stream;
			for (uint64_t index = 0; index < count; index++) {
				if (write_pointer(context, &state, lazy) != 0 ||
				    add_offset(&state.segment_offset,
				               sizeof(uintptr_t) + skip) != 0)
					return -1;
				bind_count++;
			}
			break;
		}
		default:
			goto invalid_stream;
		}
	}
	return bind_count;

invalid_stream:
	snprintf(context->error, context->error_size,
	         "Mach-O contains an invalid native bind stream");
	return -1;
}

static size_t chained_import_size(uint32_t format)
{
	if (format == DYLD_CHAINED_IMPORT)
		return sizeof(uint32_t);
	if (format == DYLD_CHAINED_IMPORT_ADDEND)
		return sizeof(uint32_t) + sizeof(int32_t);
	if (format == DYLD_CHAINED_IMPORT_ADDEND64)
		return sizeof(uint64_t) + sizeof(int64_t);
	return 0;
}

static int read_chained_import(const uint8_t* fixups, size_t fixups_size,
			       const struct chained_fixups_header* header,
			       uint32_t import_index,
			       struct chained_import* result)
{
	size_t entry_size = chained_import_size(header->imports_format);
	uint64_t entry_offset = header->imports_offset +
	                        (uint64_t)import_index * entry_size;
	if (!entry_size || import_index >= header->imports_count ||
	    !range_valid(entry_offset, entry_size, fixups_size) ||
	    header->symbols_offset > fixups_size)
		return -1;

	memset(result, 0, sizeof(*result));
	uint64_t name_offset;
	if (header->imports_format == DYLD_CHAINED_IMPORT ||
	    header->imports_format == DYLD_CHAINED_IMPORT_ADDEND) {
		uint32_t word;
		memcpy(&word, fixups + entry_offset, sizeof(word));
		result->provider_ordinal = (int8_t)(word & 0xffu);
		result->weak_import = (word >> 8) & 1u;
		name_offset = word >> 9;
		if (header->imports_format == DYLD_CHAINED_IMPORT_ADDEND) {
			int32_t addend;
			memcpy(&addend, fixups + entry_offset + sizeof(word),
			       sizeof(addend));
			result->addend = addend;
		}
	} else {
		uint64_t word;
		memcpy(&word, fixups + entry_offset, sizeof(word));
		result->provider_ordinal = (int16_t)(word & 0xffffu);
		result->weak_import = (word >> 16) & 1u;
		name_offset = word >> 32;
		memcpy(&result->addend,
		       fixups + entry_offset + sizeof(word),
		       sizeof(result->addend));
	}

	uint64_t symbol_offset = header->symbols_offset + name_offset;
	if (symbol_offset >= fixups_size ||
	    !memchr(fixups + symbol_offset, '\0', fixups_size - symbol_offset))
		return -1;
	result->symbol = (const char*)fixups + symbol_offset;
	return 0;
}

static int resolve_chained_bind(struct import_context* context,
				const struct chained_import* import,
				uint8_t segment_index, uint64_t segment_offset,
				int64_t pointer_addend,
				uintptr_t* value)
{
	int64_t total_addend;
	if (__builtin_add_overflow(import->addend, pointer_addend,
	                           &total_addend)) {
		snprintf(context->error, context->error_size,
		         "Mach-O chained import addend overflows");
		return -1;
	}
	struct bind_state state = {
		.provider_ordinal = import->provider_ordinal,
		.segment_index = segment_index,
		.type = BIND_TYPE_POINTER,
		.symbol_flags = import->weak_import
			                ? BIND_SYMBOL_FLAGS_WEAK_IMPORT
			                : 0,
		.addend = total_addend,
		.segment_offset = segment_offset,
		.symbol = import->symbol,
	};
	int mapped_native_provider;
	void* symbol = find_symbol(context, &state, &mapped_native_provider);
	int bind_zero = 0;
	if (!symbol) {
		enum unresolved_import_action action =
			unresolved_import_action(context, &state, 0);
		if (action == UNRESOLVED_IMPORT_TRAP)
			symbol = unresolved_stub_binder;
		else if (action == UNRESOLVED_IMPORT_RETURN_ZERO)
			symbol = unresolved_return_zero;
		else if (action == UNRESOLVED_IMPORT_ZERO)
			bind_zero = 1;
		else if (action == UNRESOLVED_IMPORT_DATA) {
			symbol = allocate_unresolved_data(context);
			if (!symbol) {
				snprintf(context->error, context->error_size,
				         "could not allocate unresolved data for %s",
				         import->symbol);
				return -1;
			}
			fprintf(stderr,
			        "machgate: unresolved import %s from %s uses compatibility storage\n",
			        import->symbol, provider_description(context, &state));
		} else if (action == UNRESOLVED_IMPORT_ERROR) {
			snprintf(context->error, context->error_size,
			         "native symbol %s was not found for %s", import->symbol,
			         provider_description(context, &state));
			return -1;
		}
	}
	if (bind_zero) {
		*value = 0;
		return 0;
	}
	if (resolved_symbol_value(context, &state, symbol,
	                          mapped_native_provider, value) != 0)
		return -1;
	return 0;
}

static int apply_chained_page(struct import_context* context,
			      const uint8_t* fixups, size_t fixups_size,
			      const struct chained_fixups_header* header,
			      struct import_segment* segment,
			      uint16_t pointer_format, uint16_t page_size,
			      uint16_t page_index, uint16_t start)
{
	uint64_t page_offset = (uint64_t)page_index * page_size;
	uint64_t pointer_offset = page_offset + start;
	for (;;) {
		if (pointer_offset > segment->size ||
		    sizeof(uint64_t) > segment->size - pointer_offset ||
		    pointer_offset - page_offset >= page_size) {
			snprintf(context->error, context->error_size,
			         "Mach-O chained pointer leaves its page");
			return -1;
		}

		uint64_t raw;
		memcpy(&raw, (void*)(segment->address + pointer_offset),
		       sizeof(raw));
		uint16_t next = (raw >> 51) & 0xfffu;
		uintptr_t value;
		if (raw >> 63) {
			uint32_t import_index = raw & 0xffffffu;
			int64_t pointer_addend = (int8_t)((raw >> 24) & 0xffu);
			struct chained_import import;
			if (read_chained_import(fixups, fixups_size, header,
			                        import_index, &import) != 0) {
				snprintf(context->error, context->error_size,
				         "Mach-O contains an invalid chained import");
				return -1;
			}
			uint8_t segment_index =
				(uint8_t)(segment - context->segments);
			if (resolve_chained_bind(context, &import, segment_index,
			                         pointer_offset, pointer_addend,
			                         &value) != 0)
				return -1;
		} else {
			uint64_t target = raw & ((UINT64_C(1) << 36) - 1);
			if (pointer_format == DYLD_CHAINED_PTR_64) {
				uint64_t high = (raw >> 36) & 0xffu;
				value = (uintptr_t)(target | (high << 56)) +
				        machgate_guest_image_slide(context->image);
			} else {
				value = (uintptr_t)machgate_guest_image_validated_header(context->image) + target;
			}
		}
		if (write_chained_pointer(context, segment, pointer_offset,
		                          value) != 0)
			return -1;
		if (!next)
			return 0;
		if (add_offset(&pointer_offset, (uint64_t)next * 4u) != 0) {
			snprintf(context->error, context->error_size,
			         "Mach-O chained pointer offset overflows");
			return -1;
		}
	}
}

static int apply_chained_segment(
	struct import_context* context, const uint8_t* fixups, size_t fixups_size,
	const struct chained_fixups_header* header, size_t segment_index,
	const struct chained_starts_in_segment* starts, size_t starts_size)
{
	size_t starts_header_size =
		offsetof(struct chained_starts_in_segment, page_start);
	if (segment_index >= context->segment_count ||
	    starts_size < starts_header_size ||
	    starts->size > starts_size ||
	    starts->page_count >
		(starts->size - starts_header_size) / sizeof(uint16_t) ||
	    (starts->pointer_format != DYLD_CHAINED_PTR_64 &&
	     starts->pointer_format != DYLD_CHAINED_PTR_64_OFFSET) ||
	    !starts->page_size) {
		snprintf(context->error, context->error_size,
		         "Mach-O uses unsupported chained-fixup starts");
		return -1;
	}

	struct import_segment* segment = &context->segments[segment_index];
	for (uint16_t page_index = 0; page_index < starts->page_count;
	     page_index++) {
		uint16_t start = starts->page_start[page_index];
		if (start == DYLD_CHAINED_PTR_START_NONE)
			continue;
		if (!(start & DYLD_CHAINED_PTR_START_MULTI)) {
			if (apply_chained_page(context, fixups, fixups_size, header,
			                       segment, starts->pointer_format,
			                       starts->page_size, page_index, start) != 0)
				return -1;
			continue;
		}

		size_t overflow_index = start & ~DYLD_CHAINED_PTR_START_MULTI;
		size_t total_starts =
			(starts->size - starts_header_size) / sizeof(uint16_t);
		for (;;) {
			if (overflow_index >= total_starts) {
				snprintf(context->error, context->error_size,
				         "Mach-O chained-fixup overflow index is invalid");
				return -1;
			}
			uint16_t overflow_start = starts->page_start[overflow_index++];
			if (apply_chained_page(
				    context, fixups, fixups_size, header, segment,
				    starts->pointer_format, starts->page_size, page_index,
				    overflow_start & ~DYLD_CHAINED_PTR_START_LAST) != 0)
				return -1;
			if (overflow_start & DYLD_CHAINED_PTR_START_LAST)
				break;
		}
	}
	return 0;
}

static int apply_chained_fixups(struct import_context* context)
{
	if (!context->chained_fixups || !context->chained_fixups->datasize)
		return 0;
	size_t fixups_size = context->chained_fixups->datasize;
	const uint8_t* fixups = address_for_file_offset(
		context, context->chained_fixups->dataoff, fixups_size);
	if (!fixups || fixups_size < sizeof(struct chained_fixups_header)) {
		snprintf(context->error, context->error_size,
		         "Mach-O chained-fixups data is not mapped");
		return -1;
	}

	const struct chained_fixups_header* header = (const void*)fixups;
	if (header->fixups_version != 0 || header->symbols_format != 0 ||
	    !chained_import_size(header->imports_format) ||
	    !range_valid(header->starts_offset, sizeof(uint32_t), fixups_size)) {
		snprintf(context->error, context->error_size,
		         "Mach-O uses unsupported chained-fixups metadata");
		return -1;
	}

	const uint8_t* starts_data = fixups + header->starts_offset;
	uint32_t segment_count;
	memcpy(&segment_count, starts_data, sizeof(segment_count));
	uint64_t offsets_size = (uint64_t)segment_count * sizeof(uint32_t);
	if (segment_count > context->segment_count ||
	    !range_valid(header->starts_offset + sizeof(uint32_t), offsets_size,
	                 fixups_size)) {
		snprintf(context->error, context->error_size,
		         "Mach-O chained-fixups segment table is invalid");
		return -1;
	}
	const uint32_t* segment_offsets =
		(const void*)(starts_data + sizeof(uint32_t));
	for (uint32_t index = 0; index < segment_count; index++) {
		if (!segment_offsets[index])
			continue;
		uint64_t starts_offset =
			(uint64_t)header->starts_offset + segment_offsets[index];
		if (!range_valid(starts_offset,
		                 offsetof(struct chained_starts_in_segment,
		                          page_start),
		                 fixups_size)) {
			snprintf(context->error, context->error_size,
			         "Mach-O chained segment starts are invalid");
			return -1;
		}
		const struct chained_starts_in_segment* starts =
			(const void*)(fixups + starts_offset);
		if (apply_chained_segment(context, fixups, fixups_size, header,
		                          index, starts,
		                          fixups_size - starts_offset) != 0)
			return -1;
	}
	return 0;
}

static void notify_provider_sections(struct import_context* context)
{
	for (size_t index = 0; index < context->provider_count; index++) {
		void* handle = context->providers[index].handle;
		if (!handle)
			continue;
		int already_notified = 0;
		for (size_t earlier = 0; earlier < index; earlier++)
			already_notified |= context->providers[earlier].handle == handle;
		if (already_notified)
			continue;
		void* address = dlsym(handle, MACHGATE_SECTION_LOADED_SYMBOL);
		if (!provider_owns_address(handle, address))
			continue;
		machgate_section_loaded_function notify = (machgate_section_loaded_function)address;
		for (size_t segment_index = 0; segment_index < context->segment_count;
		     segment_index++) {
			const struct segment_command_64* segment =
				context->segments[segment_index].command;
			const struct section_64* sections = (const void*)(segment + 1);
			for (size_t section_index = 0; section_index < segment->nsects;
			     section_index++) {
				const struct section_64* section = &sections[section_index];
				uintptr_t section_address = section->addr +
					machgate_guest_image_slide(context->image);
				if (section->size && machgate_guest_image_contains(context->image,
					section_address, section->size))
					notify(section->segname, section->sectname,
						(void*)section_address, section->size);
			}
		}
	}
}

int machgate_bind_native_imports(struct machgate_guest_image* image, char* error,
				 size_t error_size)
{
	if (((struct mach_header_64*)machgate_guest_image_validated_header(image))->filetype == MH_EXECUTE)
		dynamic_provider_count = 0;
	struct import_context context;
	memset(&context, 0, sizeof(context));
	context.image = image;
	context.error = error;
	context.error_size = error_size;
	context.allow_unresolved_stubs = import_context_allows_unresolved_stubs();
	context.page_size = sysconf(_SC_PAGESIZE);
	if (context.page_size <= 0) {
		snprintf(error, error_size, "could not determine host page size");
		return -1;
	}
	if (collect_load_commands(&context) != 0)
		return -1;
	if (machgate_rebase_guest_pointers(image, error, error_size) != 0)
		return -1;
	if (context.allow_unresolved_stubs && collect_called_pointers(&context) != 0)
		goto failed;
	int bind_count = 0;
	if (context.dyld_info) {
		int result = apply_bind_stream(
			&context, context.dyld_info->bind_off,
			context.dyld_info->bind_size, 0, 0);
		if (result < 0)
			goto failed;
		bind_count += result;
		result = apply_bind_stream(
			&context, context.dyld_info->weak_bind_off,
			context.dyld_info->weak_bind_size, 0, 1);
		if (result < 0)
			goto failed;
		bind_count += result;
		result = apply_bind_stream(
			&context, context.dyld_info->lazy_bind_off,
			context.dyld_info->lazy_bind_size, 1, 0);
		if (result < 0)
			goto failed;
		bind_count += result;
	}
	if (apply_chained_fixups(&context) != 0)
		goto failed;
	if (machgate_finish_native_structor_adapters(
		    context.structor_adapters, error, error_size) != 0)
		goto failed;
	for (size_t index = 0; index < context.provider_count; index++) {
		struct native_provider* provider = &context.providers[index];
		if (!provider->handle)
			continue;
		int registered = 0;
		for (size_t earlier = 0; earlier < dynamic_provider_count; earlier++)
			registered |= dynamic_providers[earlier].handle == provider->handle;
		if (registered)
			continue;
		if (dynamic_provider_count == MACHGATE_MAX_NATIVE_PROVIDERS) {
			snprintf(error, error_size, "too many selected native provider handles");
			goto failed;
		}
		dynamic_providers[dynamic_provider_count++] = *provider;
	}
	notify_provider_sections(&context);
	free(context.called_pointers);
	return bind_count;

failed:
	free(context.called_pointers);
	machgate_discard_native_structor_adapters(context.structor_adapters);
	return -1;
}
