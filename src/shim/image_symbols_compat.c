#define _GNU_SOURCE

#include "image_symbols_compat.h"
#include "ports/guest_symbol_lookup.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern int machgate_describe_guest_image_v1(uint32_t, struct machgate_image_description*) __attribute__((weak));
extern int machgate_describe_guest_address_v1(const void*, struct machgate_address_description*) __attribute__((weak));

int machgate_darwin_dladdr(const void* address, Dl_info* info)
{
	if (!info)
		return 0;
	struct machgate_address_description description;
	if (machgate_describe_guest_address_v1 && machgate_describe_guest_address_v1(address, &description)) {
		info->dli_fname = description.image.path;
		info->dli_fbase = description.image.header;
		info->dli_sname = description.symbol_name;
		info->dli_saddr = description.symbol_address;
		return 1;
	}
	return dladdr(address, info);
}

uint32_t _dyld_image_count(void)
{
	uint32_t count = 0;
	struct machgate_image_description description;
	while (machgate_describe_guest_image_v1 && machgate_describe_guest_image_v1(count, &description))
		count++;
	return count;
}

const void* _dyld_get_image_header(uint32_t index)
{
	struct machgate_image_description description;
	return machgate_describe_guest_image_v1 && machgate_describe_guest_image_v1(index, &description) ?
		description.header : NULL;
}

const char* _dyld_get_image_name(uint32_t index)
{
	struct machgate_image_description description;
	return machgate_describe_guest_image_v1 && machgate_describe_guest_image_v1(index, &description) ?
		description.path : NULL;
}

intptr_t _dyld_get_image_vmaddr_slide(uint32_t index)
{
	struct machgate_image_description description;
	return machgate_describe_guest_image_v1 && machgate_describe_guest_image_v1(index, &description) ?
		description.slide : 0;
}

static int format_frame(char* buffer, size_t size, int index, const void* address, const Dl_info* info)
{
	const char* image = info->dli_fname ? strrchr(info->dli_fname, '/') : NULL;
	image = image ? image + 1 : info->dli_fname ? info->dli_fname : "???";
	char image_name[36];
	snprintf(image_name, sizeof(image_name), "%.35s", image);
	image = image_name;
	const char* symbol = info->dli_sname ? info->dli_sname : info->dli_fname ? image : "0x0";
	uintptr_t base = (uintptr_t)(info->dli_sname ? info->dli_saddr : info->dli_fbase);
	return snprintf(buffer, size, "%-4d%-35s 0x%016lx %s + %lu", index, image,
	                (unsigned long)(uintptr_t)address, symbol, (unsigned long)((uintptr_t)address - base));
}

char** machgate_darwin_backtrace_symbols(void* const* addresses, int count)
{
	if (count <= 0)
		return NULL;
	Dl_info* information = calloc((size_t)count, sizeof(*information));
	if (!information)
		return NULL;
	size_t size = (size_t)count * sizeof(char*);
	for (int index = 0; index < count; index++) {
		machgate_darwin_dladdr(addresses[index], &information[index]);
		int length = format_frame(NULL, 0, index, addresses[index], &information[index]);
		if (length < 0 || SIZE_MAX - size <= (size_t)length) {
			free(information);
			return NULL;
		}
		size += (size_t)length + 1;
	}
	char** result = malloc(size);
	if (result) {
		char* cursor = (char*)(result + count);
		char* end = (char*)result + size;
		for (int index = 0; index < count; index++) {
			result[index] = cursor;
			cursor += format_frame(cursor, (size_t)(end - cursor), index, addresses[index], &information[index]) + 1;
		}
	}
	free(information);
	return result;
}

void machgate_darwin_backtrace_symbols_fd(void* const* addresses, int count, int descriptor)
{
	for (int index = 0; index < count; index++) {
		Dl_info information = {0};
		machgate_darwin_dladdr(addresses[index], &information);
		char frame[BUFSIZ];
		int length = format_frame(frame, sizeof(frame), index, addresses[index], &information);
		if (length < 0)
			return;
		size_t remaining = (size_t)length < sizeof(frame) ? (size_t)length : sizeof(frame) - 1;
		const char* cursor = frame;
		while (remaining) {
			ssize_t result = write(descriptor, cursor, remaining);
			if (result < 0 && errno == EINTR)
				continue;
			if (result <= 0)
				break;
			cursor += result;
			remaining -= result;
		}
		if (!remaining) {
			ssize_t result;
			do {
				result = write(descriptor, "\n", 1);
			} while (result < 0 && errno == EINTR);
		}
	}
}
