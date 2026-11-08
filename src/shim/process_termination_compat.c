#include "thread_local_compat.h"

#include <dlfcn.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

extern void machgate_trace_process_exit_code(const char* name, int status,
	                                      void* caller)
	__attribute__((weak));

static void trace_process_termination(const char* name, int status,
	                              void* caller)
{
	if (machgate_trace_process_exit_code)
		machgate_trace_process_exit_code(name, status, caller);
}

__attribute__((noreturn, visibility("hidden")))
void machgate_terminate_after_guest_return(int status)
{
	trace_process_termination("guest_return", status,
		__builtin_extract_return_addr(__builtin_return_address(0)));
	void (*run_callbacks)(void) = (void (*)(void))dlsym(
		RTLD_DEFAULT, "machgate_run_thread_local_termination_callbacks");
	if (run_callbacks)
		run_callbacks();
	fflush(NULL);
	syscall(SYS_exit_group, status);
	__builtin_unreachable();
}

#ifndef MACHGATE_GUEST_RUNNER

__attribute__((noreturn))
void exit(int status)
{
	trace_process_termination(
		"exit", status,
		__builtin_extract_return_addr(__builtin_return_address(0)));
	machgate_run_thread_local_termination_callbacks();
	fflush(NULL);
	syscall(SYS_exit_group, status);
	__builtin_unreachable();
}

__attribute__((noreturn))
void _exit(int status)
{
	trace_process_termination(
		"_exit", status,
		__builtin_extract_return_addr(__builtin_return_address(0)));
	syscall(SYS_exit_group, status);
	__builtin_unreachable();
}
#endif
