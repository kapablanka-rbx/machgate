#include "string_value.h"

#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>

#define FOUNDATION_FORMAT_ARGUMENTS 128

struct host_argument_list {
	void* stack;
	void* general_register_top;
	void* vector_register_top;
	int general_register_offset;
	int vector_register_offset;
};

typedef int (*host_vsnprintf_function)(char*, size_t, const char*, struct host_argument_list*);

static int copy_argument(uintptr_t* values, const uintptr_t* arguments, size_t* count)
{
	if (*count == FOUNDATION_FORMAT_ARGUMENTS)
		return -1;
	values[*count] = arguments[*count];
	(*count)++;
	return 0;
}

static int translate_format(char* format, const uintptr_t* arguments, uintptr_t* values)
{
	size_t count = 0;
	for (char* cursor = format; *cursor; cursor++) {
		if (*cursor != '%')
			continue;
		cursor++;
		if (*cursor == '%')
			continue;
		cursor += strspn(cursor, "-+ #0'");
		if (*cursor == '*') {
			if (copy_argument(values, arguments, &count) != 0)
				return -1;
			cursor++;
		} else {
			cursor += strspn(cursor, "0123456789");
		}
		if (*cursor == '.') {
			cursor++;
			if (*cursor == '*') {
				if (copy_argument(values, arguments, &count) != 0)
					return -1;
				cursor++;
			} else {
				cursor += strspn(cursor, "0123456789");
			}
		}
		cursor += strspn(cursor, "hlzjt");
		if (!*cursor || !strchr("diouxXfFeEgGaAcsp@", *cursor) ||
		    copy_argument(values, arguments, &count) != 0)
			return -1;
		if (*cursor != '@')
			continue;
		struct foundation_string* string = foundation_string_resolve((void*)values[count - 1]);
		if (values[count - 1] && !string)
			return -1;
		values[count - 1] = (uintptr_t)(string ? string->utf8 : "(null)");
		*cursor = 's';
	}
	return 0;
}

struct foundation_string* foundation_string_format(const void* format_object,
	const uintptr_t* arguments)
{
	struct foundation_string* format_string = foundation_string_resolve(format_object);
	if (!format_string)
		return NULL;
	char* format = strdup(format_string->utf8);
	if (!format)
		return NULL;
	uintptr_t values[FOUNDATION_FORMAT_ARGUMENTS] = {0};
	if (translate_format(format, arguments, values) != 0) {
		free(format);
		return NULL;
	}
	host_vsnprintf_function host_vsnprintf =
		(host_vsnprintf_function)dlvsym(RTLD_DEFAULT, "vsnprintf", "GLIBC_2.17");
	if (!host_vsnprintf) {
		free(format);
		return NULL;
	}
	struct host_argument_list list = {.stack = values};
	int length = host_vsnprintf(NULL, 0, format, &list);
	char* text = length >= 0 && length < 16 * 1024 * 1024 ? malloc((size_t)length + 1) : NULL;
	struct foundation_string* result = NULL;
	if (text) {
		list = (struct host_argument_list){.stack = values};
		if (host_vsnprintf(text, (size_t)length + 1, format, &list) == length)
			result = foundation_string_create(text, (size_t)length, "UTF-8");
	}
	free(text);
	free(format);
	return result;
}
