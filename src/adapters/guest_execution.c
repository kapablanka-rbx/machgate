#include "ports/guest_execution.h"
#include "ports/guest_image_view.h"

#include "adapters/guest_unwind.h"
#include "guest_allocator_bridge.h"
#include "loader.h"
#include "macho_defs.h"
#include "native_imports.h"
#include "syscall/syscall_gate.h"
#include "trampoline.h"

#include <dlfcn.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define MACHGATE_GUEST_STACK_SIZE (8u * 1024u * 1024u)

_Noreturn void machgate_enter_guest(uintptr_t entry_point,
				    uintptr_t stack_pointer,
				    int argument_count,
				    char** arguments,
				    char** environment,
				    char** apple);
void machgate_call_guest_initializer(void* function, int argument_count,
				     char** arguments, char** environment,
				     char** apple, void* stack_pointer);

static void call_guest_initializer(void* function, int argument_count,
				   char** arguments, char** environment,
				   char** apple, uintptr_t stack_pointer,
				   const char* kind, size_t index, size_t total)
{
	void (*note_context)(const char*, int, int, uintptr_t) =
		dlsym(RTLD_DEFAULT, "machgate_shim_note_init_context");
	void (*clear_context)(void) =
		dlsym(RTLD_DEFAULT, "machgate_shim_clear_init_context");
	if (note_context)
		note_context(kind, (int)index, (int)total, (uintptr_t)function);
	machgate_call_guest_initializer(function, argument_count, arguments,
					 environment, apple,
					 (void*)stack_pointer);
	if (clear_context)
		clear_context();
}

static int run_pointer_initializers(struct machgate_guest_image* image,
				    const struct section_64* section,
				    int argument_count, char** arguments,
				    char** environment, char** apple,
				    uintptr_t stack_pointer, char* error,
				    size_t error_size)
{
	uintptr_t section_address =
		section->addr + machgate_guest_image_slide(image);
	if (section->size % sizeof(uintptr_t) != 0 ||
	    !machgate_guest_image_contains(image, section_address,
					   section->size)) {
		snprintf(error, error_size,
			 "Mach-O has an invalid initializer-pointer section");
		return -1;
	}

	uintptr_t* functions = (void*)section_address;
	size_t count = section->size / sizeof(*functions);
	for (size_t function_index = 0; function_index < count;
	     function_index++) {
		if (!functions[function_index])
			continue;
		uintptr_t function = functions[function_index];
		if (!machgate_guest_image_contains(image, function, 1)) {
			snprintf(error, error_size,
				 "Mach-O initializer address is outside the image");
			return -1;
		}
		call_guest_initializer((void*)function, argument_count, arguments,
				       environment, apple, stack_pointer,
				       "pointer", function_index, count);
	}
	return 0;
}

static int run_offset_initializers(struct machgate_guest_image* image,
				   const struct section_64* section,
				   int argument_count, char** arguments,
				   char** environment, char** apple,
				   uintptr_t stack_pointer, char* error,
				   size_t error_size)
{
	uintptr_t section_address =
		section->addr + machgate_guest_image_slide(image);
	if (section->size % sizeof(uint32_t) != 0 ||
	    !machgate_guest_image_contains(image, section_address,
					   section->size)) {
		snprintf(error, error_size,
			 "Mach-O has an invalid initializer-offset section");
		return -1;
	}

	uint32_t* offsets = (void*)section_address;
	size_t count = section->size / sizeof(*offsets);
	for (size_t offset_index = 0; offset_index < count; offset_index++) {
		uintptr_t function =
			(uintptr_t)machgate_guest_image_validated_header(image) +
			offsets[offset_index];
		if (!machgate_guest_image_contains(image, function, 1)) {
			snprintf(error, error_size,
				 "Mach-O initializer offset is outside the image");
			return -1;
		}
		call_guest_initializer((void*)function, argument_count, arguments,
				       environment, apple, stack_pointer,
				       "offset", offset_index, count);
	}
	return 0;
}

int machgate_run_guest_initializers(
	struct machgate_guest_image* image, int argument_count,
	char** arguments, char** environment, char** apple,
	uintptr_t stack_pointer, char* error, size_t error_size)
{
	struct mach_header_64* header =
		machgate_guest_image_validated_header(image);
	uint8_t* cursor = (uint8_t*)(header + 1);

	for (uint32_t command_index = 0;
	     command_index < header->ncmds;
	     command_index++) {
		struct load_command* command = (void*)cursor;
		if (command->cmd == LC_SEGMENT_64) {
			struct segment_command_64* segment = (void*)command;
			struct section_64* sections = (void*)(segment + 1);
			for (uint32_t section_index = 0;
			     section_index < segment->nsects;
			     section_index++) {
				struct section_64* section = &sections[section_index];
				uint32_t section_type = section->flags & SECTION_TYPE;
				if (section_type == S_MOD_INIT_FUNC_POINTERS &&
				    run_pointer_initializers(
					    image, section, argument_count, arguments,
					    environment, apple, stack_pointer, error,
					    error_size) != 0)
					return -1;
				if (section_type == S_INIT_FUNC_OFFSETS &&
				    run_offset_initializers(
					    image, section, argument_count, arguments,
					    environment, apple, stack_pointer, error,
					    error_size) != 0)
					return -1;
			}
		}
		cursor += command->cmdsize;
	}
	return 0;
}

int machgate_prepare_guest_image(const char* executable_path,
				 struct machgate_guest_image** image,
				 char* error, size_t error_size)
{
	return machgate_load_executable(executable_path, image, error, error_size);
}

void machgate_release_guest_image(struct machgate_guest_image* image)
{
	void (*unregister_tlv_image)(const void*) =
		dlsym(RTLD_DEFAULT, "machgate_unregister_thread_local_image");
	if (image && unregister_tlv_image)
		unregister_tlv_image(image);
	machgate_unload_guest_image(image);
}

int machgate_prepare_native_linkage(struct machgate_guest_image* image,
				    char* error, size_t error_size)
{
	int result = machgate_bind_native_imports(image, error, error_size);
	return result < 0 ? -1 : 0;
}

int machgate_prepare_guest_unwind(struct machgate_guest_image* image,
				  char* error, size_t error_size)
{
	return machgate_register_guest_unwind(image, error, error_size);
}

int machgate_apply_guest_redirections(struct machgate_guest_image* image,
				      char* error, size_t error_size)
{
	int result = machgate_apply_trampolines(image, error, error_size);
	return result < 0 ? -1 : 0;
}

int machgate_prepare_darwin_abi(struct machgate_guest_image* image,
			       char* error, size_t error_size)
{
	if (syscall_gate_patch(image) >= 0)
		return 0;
	snprintf(error, error_size, "could not patch Darwin syscall sites");
	return -1;
}

int machgate_allocate_guest_main_stack(struct machgate_guest_stack* stack,
				       char* error, size_t error_size)
{
	long page_size_result = sysconf(_SC_PAGESIZE);
	if (page_size_result <= 0) {
		snprintf(error, error_size, "could not determine host page size");
		return -1;
	}

	size_t page_size = (size_t)page_size_result;
	size_t mapping_size = MACHGATE_GUEST_STACK_SIZE + page_size;
	void* mapping = mmap(NULL, mapping_size, PROT_NONE,
			     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (mapping == MAP_FAILED) {
		snprintf(error, error_size, "could not allocate guest stack: %s",
			 strerror(errno));
		return -1;
	}

	void* stack_base = (char*)mapping + page_size;
	if (mprotect(stack_base, MACHGATE_GUEST_STACK_SIZE,
		     PROT_READ | PROT_WRITE) != 0) {
		snprintf(error, error_size, "could not enable guest stack: %s",
			 strerror(errno));
		munmap(mapping, mapping_size);
		return -1;
	}

	stack->mapping = mapping;
	stack->mapping_size = mapping_size;
	stack->top = (char*)stack_base + MACHGATE_GUEST_STACK_SIZE;
	stack->size = MACHGATE_GUEST_STACK_SIZE;
	stack->pointer = (uintptr_t)stack->top & ~(uintptr_t)15;
	return 0;
}

void machgate_release_guest_main_stack(struct machgate_guest_stack* stack)
{
	if (!stack || !stack->mapping)
		return;
	munmap(stack->mapping, stack->mapping_size);
	memset(stack, 0, sizeof(*stack));
}

char* machgate_resolve_guest_executable_path(const char* executable_path,
					     char* error,
					     size_t error_size)
{
	char* result = realpath(executable_path, NULL);
	if (!result)
		result = strdup(executable_path);
	if (!result)
		snprintf(error, error_size,
			 "could not save guest executable path");
	return result;
}

void machgate_release_guest_executable_path(char* executable_path)
{
	free(executable_path);
}

void machgate_connect_guest_allocator(struct machgate_guest_image* image)
{
	machgate_configure_guest_allocator_bridge(image);
}

int machgate_enter_guest_working_directory(const char* executable_path,
					   char* error,
					   size_t error_size)
{
	char* directory = strdup(executable_path);
	if (!directory) {
		snprintf(error, error_size,
			 "could not copy guest executable directory");
		return -1;
	}

	int result = 0;
	char* slash = strrchr(directory, '/');
	if (slash) {
		*slash = '\0';
		if (*directory)
			result = chdir(directory);
	}
	free(directory);
	if (result != 0)
		snprintf(error, error_size,
			 "could not enter guest directory: %s", strerror(errno));
	return result;
}

_Noreturn void machgate_enter_guest_image(
	struct machgate_guest_image* image, uintptr_t stack_pointer,
	int argument_count, char** arguments, char** environment, char** apple)
{
	machgate_enter_guest(machgate_guest_image_entry_point(image),
			     stack_pointer, argument_count, arguments,
			     environment, apple);
}
