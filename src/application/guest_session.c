#include "application/guest_session.h"

#include "domain/guest_process.h"
#include "domain/process_launch_context.h"
#include "ports/guest_execution.h"
#include "ports/guest_libraries.h"
#include "ports/guest_platform.h"

#include <stdio.h>

static struct machgate_guest_process guest_process;

const struct machgate_guest_process_runtime_state*
machgate_guest_process_runtime_state_v1(void)
{
	return &guest_process.runtime;
}

static int fail_guest_run(struct machgate_guest_stack* stack,
		  char* executable_path)
{
	machgate_release_guest_platform();
	machgate_release_guest_executable_path(executable_path);
	machgate_release_guest_main_stack(stack);
	machgate_release_guest_libraries();
	machgate_release_guest_image(guest_process.image);
	machgate_guest_process_reset(&guest_process);
	return 1;
}

int machgate_run_guest(const char* executable_path, int argument_count,
		       char** arguments, char** environment,
		       char* error, size_t error_size)
{
	machgate_guest_process_reset(&guest_process);
	struct machgate_guest_stack stack = {0};
	char* resolved_path = NULL;
	struct machgate_guest_image* image = NULL;

	if (machgate_prepare_guest_image(executable_path, &image,
					 error, error_size) != 0)
		return 1;
	if (machgate_guest_process_accept_image(&guest_process, image) != 0) {
		snprintf(error, error_size, "could not create Guest Process");
		return fail_guest_run(&stack, resolved_path);
	}
	machgate_begin_guest_linkage(image);
	if (machgate_prepare_native_linkage(image, error, error_size) != 0 ||
	    machgate_prepare_guest_library_images(error, error_size) != 0 ||
	    machgate_prepare_guest_unwind(image, error, error_size) != 0 ||
	    machgate_apply_guest_redirections(image, error, error_size) != 0 ||
	    machgate_prepare_darwin_abi(image, error, error_size) != 0 ||
	    machgate_prepare_guest_runtime_image(image, error, error_size) != 0)
		return fail_guest_run(&stack, resolved_path);
	if (machgate_prepare_guest_platform(image, error, error_size) != 0)
		return fail_guest_run(&stack, resolved_path);
	machgate_complete_guest_image_preparation(image);

	if (machgate_allocate_guest_main_stack(&stack, error, error_size) != 0)
		return fail_guest_run(&stack, resolved_path);
	resolved_path = machgate_resolve_guest_executable_path(
		executable_path, error, error_size);
	if (!resolved_path)
		return fail_guest_run(&stack, resolved_path);

	struct machgate_process_launch_context launch_context;
	if (machgate_prepare_process_launch_context(
		    stack.pointer, stack.size, argument_count, arguments,
		    environment, resolved_path, &launch_context,
		    error, error_size) != 0)
		return fail_guest_run(&stack, resolved_path);

	if (machgate_guest_process_prepare_runtime(
		    &guest_process, argument_count, launch_context.arguments,
		    launch_context.environment, launch_context.apple,
		    resolved_path, launch_context.stack_pointer,
		    stack.top, stack.size) != 0) {
		snprintf(error, error_size,
			 "could not prepare Guest Process runtime state");
		return fail_guest_run(&stack, resolved_path);
	}

	if (machgate_enter_guest_working_directory(
		    resolved_path, error, error_size) != 0)
		return fail_guest_run(&stack, resolved_path);

	machgate_connect_guest_allocator(image);
	if (machgate_initialize_guest_libraries(
	        argument_count, launch_context.arguments, launch_context.environment,
	        launch_context.apple, launch_context.stack_pointer, error, error_size) != 0 ||
	    machgate_run_guest_initializers(
		    image, argument_count, launch_context.arguments,
		    launch_context.environment, launch_context.apple,
		    launch_context.stack_pointer, error, error_size) != 0)
		return fail_guest_run(&stack, resolved_path);
	if (machgate_guest_process_mark_initialized(&guest_process) != 0) {
		snprintf(error, error_size,
			 "could not initialize Guest Process lifecycle");
		return fail_guest_run(&stack, resolved_path);
	}
	if (machgate_guest_process_mark_entering(&guest_process) != 0) {
		snprintf(error, error_size,
			 "could not enter Guest Process lifecycle");
		return fail_guest_run(&stack, resolved_path);
	}

	machgate_enter_guest_image(
		image, guest_process.stack_pointer,
		guest_process.runtime.argument_count,
		guest_process.runtime.arguments,
		guest_process.runtime.environment, guest_process.apple);
}
