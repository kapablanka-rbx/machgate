#define _GNU_SOURCE

#include "variadic_abi_compat.h"

#include <dlfcn.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

void machgate_shim_record_external_allocation(void* ptr, size_t size);

struct aapcs64_argument_list {
	void* stack;
	void* general_register_top;
	void* vector_register_top;
	int general_register_offset;
	int vector_register_offset;
};

static struct aapcs64_argument_list make_host_arguments(void* darwin_arguments)
{
	struct aapcs64_argument_list result = {
		.stack = darwin_arguments,
		.general_register_top = NULL,
		.vector_register_top = NULL,
		.general_register_offset = 0,
		.vector_register_offset = 0,
	};
	return result;
}

typedef int (*host_vfprintf_function)(
	FILE* stream, const char* format,
	struct aapcs64_argument_list* arguments);
typedef int (*host_vsnprintf_function)(
	char* buffer, size_t buffer_size, const char* format,
	struct aapcs64_argument_list* arguments);
typedef int (*host_vsprintf_function)(
	char* buffer, const char* format,
	struct aapcs64_argument_list* arguments);
typedef int (*host_vsscanf_function)(
	const char* input, const char* format,
	struct aapcs64_argument_list* arguments);
typedef int (*host_vfscanf_function)(
	FILE* stream, const char* format,
	struct aapcs64_argument_list* arguments);
typedef int (*host_vasprintf_function)(
	char** output, const char* format,
	struct aapcs64_argument_list* arguments);
typedef int (*host_vsnprintf_chk_function)(
	char* buffer, size_t buffer_size, int flag, size_t destination_size,
	const char* format, struct aapcs64_argument_list* arguments);
typedef int (*host_vsprintf_chk_function)(
	char* buffer, int flag, size_t destination_size, const char* format,
	struct aapcs64_argument_list* arguments);

static host_vfprintf_function find_host_vfprintf(void)
{
	union {
		void* object;
		host_vfprintf_function function;
	} symbol;

	symbol.object = dlvsym(RTLD_NEXT, "vfprintf", "GLIBC_2.17");
	if (!symbol.object)
		symbol.object = dlsym(RTLD_NEXT, "vfprintf");
	return symbol.function;
}

static host_vsnprintf_function find_host_vsnprintf(void)
{
	union {
		void* object;
		host_vsnprintf_function function;
	} symbol;

	symbol.object = dlvsym(RTLD_NEXT, "vsnprintf", "GLIBC_2.17");
	if (!symbol.object)
		symbol.object = dlsym(RTLD_NEXT, "vsnprintf");
	return symbol.function;
}

static void* find_host_symbol(const char* name)
{
	void* result = dlvsym(RTLD_NEXT, name, "GLIBC_2.17");
	if (!result)
		result = dlsym(RTLD_NEXT, name);
	return result;
}

static int print_darwin_arguments(FILE* stream, const char* format,
	                          void* darwin_arguments)
{
	host_vfprintf_function host_vfprintf = find_host_vfprintf();
	if (!host_vfprintf) {
		errno = ENOSYS;
		return -1;
	}

	struct aapcs64_argument_list host_arguments =
		make_host_arguments(darwin_arguments);
	return host_vfprintf(stream, format, &host_arguments);
}

static int format_darwin_arguments(char* buffer, size_t buffer_size,
	                           const char* format,
	                           void* darwin_arguments)
{
	host_vsnprintf_function host_vsnprintf = find_host_vsnprintf();
	if (!host_vsnprintf) {
		errno = ENOSYS;
		return -1;
	}

	struct aapcs64_argument_list host_arguments =
		make_host_arguments(darwin_arguments);
	return host_vsnprintf(buffer, buffer_size, format, &host_arguments);
}

__attribute__((visibility("hidden")))
int machgate_darwin_printf_from_stack(const char* format,
	                              void* darwin_arguments)
{
	return print_darwin_arguments(stdout, format, darwin_arguments);
}

__attribute__((visibility("hidden")))
int machgate_darwin_vfprintf_from_stack(FILE* stream, const char* format,
	                               void* darwin_arguments)
{
	return print_darwin_arguments(stream, format, darwin_arguments);
}

__attribute__((visibility("hidden")))
int machgate_darwin_vsnprintf_from_stack(char* buffer, size_t buffer_size,
	                                const char* format,
	                                void* darwin_arguments)
{
	return format_darwin_arguments(
		buffer, buffer_size, format, darwin_arguments);
}

__attribute__((visibility("hidden")))
int machgate_darwin_snprintf_from_stack(char* buffer, size_t buffer_size,
	                               const char* format,
	                               void* darwin_arguments)
{
	return format_darwin_arguments(
		buffer, buffer_size, format, darwin_arguments);
}

__attribute__((visibility("hidden")))
int machgate_darwin_vsprintf(char* buffer, const char* format,
	                    void* darwin_arguments);

__attribute__((visibility("hidden")))
int machgate_darwin_fprintf_from_stack(FILE* stream, const char* format,
	                              void* darwin_arguments)
{
	return print_darwin_arguments(stream, format, darwin_arguments);
}

__attribute__((visibility("hidden")))
int machgate_darwin_sprintf_from_stack(char* buffer, const char* format,
	                              void* darwin_arguments)
{
	return machgate_darwin_vsprintf(buffer, format, darwin_arguments);
}

__attribute__((visibility("hidden")))
int machgate_darwin_sscanf_from_stack(const char* input, const char* format,
	                             void* darwin_arguments)
{
	host_vsscanf_function host_vsscanf = find_host_symbol("vsscanf");
	if (!host_vsscanf) {
		errno = ENOSYS;
		return -1;
	}

	struct aapcs64_argument_list host_arguments =
		make_host_arguments(darwin_arguments);
	return host_vsscanf(input, format, &host_arguments);
}

__attribute__((visibility("hidden")))
int machgate_darwin_fscanf_from_stack(FILE* stream, const char* format,
	                             void* darwin_arguments)
{
	host_vfscanf_function host_vfscanf = find_host_symbol("vfscanf");
	if (!host_vfscanf) {
		errno = ENOSYS;
		return -1;
	}

	struct aapcs64_argument_list host_arguments =
		make_host_arguments(darwin_arguments);
	return host_vfscanf(stream, format, &host_arguments);
}

__attribute__((visibility("hidden")))
int machgate_darwin_sprintf_chk_from_stack(
	char* buffer, int flag, size_t destination_size, const char* format,
	void* darwin_arguments)
{
	host_vsprintf_chk_function host_vsprintf_chk =
		find_host_symbol("__vsprintf_chk");
	if (!host_vsprintf_chk) {
		errno = ENOSYS;
		return -1;
	}

	struct aapcs64_argument_list host_arguments =
		make_host_arguments(darwin_arguments);
	return host_vsprintf_chk(buffer, flag, destination_size, format,
	                         &host_arguments);
}

__attribute__((visibility("hidden")))
int machgate_darwin_vsprintf(char* buffer, const char* format,
	                    void* darwin_arguments)
{
	host_vsprintf_function host_vsprintf = find_host_symbol("vsprintf");
	if (!host_vsprintf) {
		errno = ENOSYS;
		return -1;
	}

	struct aapcs64_argument_list host_arguments =
		make_host_arguments(darwin_arguments);
	return host_vsprintf(buffer, format, &host_arguments);
}

__attribute__((visibility("hidden")))
int machgate_darwin_vprintf(const char* format, void* darwin_arguments)
{
	return print_darwin_arguments(stdout, format, darwin_arguments);
}

__attribute__((visibility("hidden")))
int machgate_darwin_vsscanf(const char* input, const char* format,
	                   void* darwin_arguments)
{
	return machgate_darwin_sscanf_from_stack(input, format, darwin_arguments);
}

__attribute__((visibility("hidden")))
int machgate_darwin_vfscanf(FILE* stream, const char* format,
	                   void* darwin_arguments)
{
	return machgate_darwin_fscanf_from_stack(stream, format, darwin_arguments);
}

__attribute__((visibility("hidden")))
int machgate_darwin_vasprintf(char** output, const char* format,
	                     void* darwin_arguments)
{
	host_vasprintf_function host_vasprintf = find_host_symbol("vasprintf");
	if (!host_vasprintf) {
		errno = ENOSYS;
		return -1;
	}

	struct aapcs64_argument_list host_arguments =
		make_host_arguments(darwin_arguments);
	int result = host_vasprintf(output, format, &host_arguments);
	if (result >= 0)
		machgate_shim_record_external_allocation(*output, (size_t)result + 1);
	return result;
}

__attribute__((visibility("hidden")))
int machgate_darwin_vsnprintf_chk(
	char* buffer, size_t buffer_size, int flag, size_t destination_size,
	const char* format, void* darwin_arguments)
{
	host_vsnprintf_chk_function host_vsnprintf_chk =
		find_host_symbol("__vsnprintf_chk");
	if (!host_vsnprintf_chk) {
		errno = ENOSYS;
		return -1;
	}

	struct aapcs64_argument_list host_arguments =
		make_host_arguments(darwin_arguments);
	return host_vsnprintf_chk(buffer, buffer_size, flag, destination_size,
	                          format, &host_arguments);
}

__attribute__((visibility("hidden")))
int machgate_darwin_vsprintf_chk(
	char* buffer, int flag, size_t destination_size, const char* format,
	void* darwin_arguments)
{
	host_vsprintf_chk_function host_vsprintf_chk =
		find_host_symbol("__vsprintf_chk");
	if (!host_vsprintf_chk) {
		errno = ENOSYS;
		return -1;
	}

	struct aapcs64_argument_list host_arguments =
		make_host_arguments(darwin_arguments);
	return host_vsprintf_chk(buffer, flag, destination_size, format,
	                         &host_arguments);
}

__attribute__((visibility("hidden")))
int machgate_host_vsnprintf(char* buffer, size_t buffer_size,
	                    const char* format, va_list arguments)
{
	host_vsnprintf_function host_vsnprintf = find_host_vsnprintf();
	if (!host_vsnprintf) {
		errno = ENOSYS;
		return -1;
	}

	return host_vsnprintf(
		buffer, buffer_size, format,
		(struct aapcs64_argument_list*)&arguments);
}

__attribute__((visibility("hidden")))
int __wrap_snprintf(char* buffer, size_t buffer_size, const char* format, ...)
{
	va_list arguments;
	va_start(arguments, format);
	int result = machgate_host_vsnprintf(
		buffer, buffer_size, format, arguments);
	va_end(arguments);
	return result;
}
