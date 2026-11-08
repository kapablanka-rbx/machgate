#include "url_path.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const char* url_path_start(const char* url)
{
	const char* scheme = strchr(url, ':');
	if (!scheme || scheme > url + strcspn(url, "/;?#"))
		return url;
	if (scheme[1] != '/' || scheme[2] != '/')
		return NULL;
	const char* authority = scheme + 3;
	const char* result = authority + strcspn(authority, "/;?#");
	return *result == '/' ? result : NULL;
}

static int path_byte_is_literal(unsigned char value)
{
	return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
	       (value >= '0' && value <= '9') ||
	       (value && strchr("-._~!$&'()*+,=:@/", value));
}

char* foundation_url_append_path(const char* url, const char* component, int directory)
{
	if (!url || !component)
		return NULL;
	const char* path = url_path_start(url);
	if (!path)
		return NULL;
	const char* suffix = path + strcspn(path, ";?#");
	size_t prefix_size = (size_t)(suffix - url);
	size_t suffix_size = strlen(suffix);
	size_t component_size = strlen(component);
	if (prefix_size > SIZE_MAX - suffix_size - 3 ||
	    component_size > (SIZE_MAX - prefix_size - suffix_size - 3) / 3)
		return NULL;
	char* result = malloc(prefix_size + component_size * 3 + suffix_size + 3);
	if (!result)
		return NULL;
	memcpy(result, url, prefix_size);
	char* output = result + prefix_size;
	if ((suffix == path || suffix[-1] != '/') && component[0] != '/')
		*output++ = '/';
	static const char digits[] = "0123456789ABCDEF";
	for (size_t index = 0; index < component_size; ++index) {
		unsigned char value = (unsigned char)component[index];
		if (path_byte_is_literal(value))
			*output++ = (char)value;
		else {
			*output++ = '%';
			*output++ = digits[value >> 4];
			*output++ = digits[value & 15];
		}
	}
	if (directory)
		*output++ = '/';
	memcpy(output, suffix, suffix_size + 1);
	return result;
}

static int hexadecimal_value(unsigned char value)
{
	if (value >= '0' && value <= '9')
		return value - '0';
	if (value >= 'A' && value <= 'F')
		return value - 'A' + 10;
	if (value >= 'a' && value <= 'f')
		return value - 'a' + 10;
	return -1;
}

char* foundation_url_copy_path(const char* url)
{
	const char* path = url ? url_path_start(url) : NULL;
	if (!path)
		return NULL;
	size_t size = strcspn(path, ";?#");
	if (size > 1 && path[size - 1] == '/')
		--size;
	char* result = malloc(size + 1);
	if (!result)
		return NULL;
	char* output = result;
	for (size_t index = 0; index < size; ++index) {
		if (path[index] == '%' && index + 2 < size) {
			int high = hexadecimal_value((unsigned char)path[index + 1]);
			int low = hexadecimal_value((unsigned char)path[index + 2]);
			if (high >= 0 && low >= 0) {
				*output++ = (char)((high << 4) | low);
				index += 2;
				continue;
			}
		}
		*output++ = path[index];
	}
	*output = '\0';
	return result;
}
