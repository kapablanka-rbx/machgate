#ifndef MACHGATE_PORTS_GUEST_EXECUTION_H
#define MACHGATE_PORTS_GUEST_EXECUTION_H

#include "domain/guest_image.h"

#include <stddef.h>
#include <stdint.h>

struct machgate_guest_stack {
	void* mapping;
	size_t mapping_size;
	void* top;
	size_t size;
	uintptr_t pointer;
};

int machgate_prepare_guest_image(const char* executable_path,
				 struct machgate_guest_image** image,
				 char* error, size_t error_size);
void machgate_release_guest_image(struct machgate_guest_image* image);
void machgate_complete_guest_image_preparation(struct machgate_guest_image* image);
int machgate_prepare_native_linkage(struct machgate_guest_image* image,
				    char* error, size_t error_size);
int machgate_prepare_guest_unwind(struct machgate_guest_image* image,
				  char* error, size_t error_size);
int machgate_apply_guest_redirections(struct machgate_guest_image* image,
				      char* error, size_t error_size);
int machgate_prepare_darwin_abi(struct machgate_guest_image* image,
			       char* error, size_t error_size);
int machgate_prepare_guest_runtime_image(struct machgate_guest_image* image,
					 char* error, size_t error_size);
int machgate_allocate_guest_main_stack(struct machgate_guest_stack* stack,
				       char* error, size_t error_size);
void machgate_release_guest_main_stack(struct machgate_guest_stack* stack);
char* machgate_resolve_guest_executable_path(const char* executable_path,
					     char* error,
					     size_t error_size);
void machgate_release_guest_executable_path(char* executable_path);
void machgate_connect_guest_allocator(struct machgate_guest_image* image);
int machgate_run_guest_initializers(
	struct machgate_guest_image* image, int argument_count,
	char** arguments, char** environment, char** apple,
	uintptr_t stack_pointer, char* error, size_t error_size);
int machgate_enter_guest_working_directory(const char* executable_path,
					   char* error,
					   size_t error_size);
_Noreturn void machgate_enter_guest_image(
	struct machgate_guest_image* image, uintptr_t stack_pointer,
	int argument_count, char** arguments, char** environment, char** apple);

#endif
