/* Extracted and adapted from machgate-archive/src/shim/libsystem_shim.c.
 * See NOTICE.md and COPYING for source provenance and licensing. */
#include "runtime.h"
#include "../string_value.h"
#include "../url_path.h"

#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

extern struct foundation_class OBJC_CLASS_$_NSProcessInfo;

static int shim_trace_enabled(void)
{
    const char* result = getenv("MACHGATE_TRACE_OBJC_MSGSEND");
    return result && *result && strcmp(result, "0") != 0;
}

static _Thread_local double shim_objc_msgsend_floating0;

static void* archive_copy_block(void* block)
{
    void* (*copy)(void*) = foundation_shim_symbol("_Block_copy");
    return copy ? copy(block) : block;
}

static void archive_invoke_block(void* block, void* argument)
{
    struct block_layout { void* isa; int flags, reserved; void (*invoke)(void*, void*); };
    const struct block_layout* value = block;
    if (value && value->invoke)
        value->invoke(block, argument);
}

static int archive_host_open(const char* path, int flags, unsigned mode)
{
    return (int)syscall(SYS_openat, AT_FDCWD, path, flags, mode);
}

#define SHIM_OBJC_MAGIC 0x4F424A43

enum shim_objc_kind {
	SHIM_OBJC_CLASS_NSPROCESSINFO,
	SHIM_OBJC_INSTANCE_NSPROCESSINFO,
	SHIM_OBJC_CLASS_NSSTRING,
	SHIM_OBJC_STRING,
	SHIM_OBJC_ARRAY,
	SHIM_OBJC_CLASS_NSHTTPCOOKIE,
	SHIM_OBJC_INSTANCE_NSHTTPCOOKIE,
	SHIM_OBJC_CLASS_NSHTTPCOOKIESTORAGE,
	SHIM_OBJC_INSTANCE_NSHTTPCOOKIESTORAGE,
	SHIM_OBJC_CLASS_NSURL,
	SHIM_OBJC_INSTANCE_NSURL,
	SHIM_OBJC_CLASS_NSMUTABLEDICTIONARY,
	SHIM_OBJC_INSTANCE_NSMUTABLEDICTIONARY,
	SHIM_OBJC_CLASS_NSCHARACTERSET,
	SHIM_OBJC_INSTANCE_NSCHARACTERSET,
	SHIM_OBJC_CLASS_NSURLCOMPONENTS,
	SHIM_OBJC_INSTANCE_NSURLCOMPONENTS,
	SHIM_OBJC_CLASS_NSFILEMANAGER,
	SHIM_OBJC_INSTANCE_NSFILEMANAGER,
	SHIM_OBJC_CLASS_NSFILEHANDLE,
	SHIM_OBJC_INSTANCE_NSFILEHANDLE,
	SHIM_OBJC_CLASS_NSDATA,
	SHIM_OBJC_INSTANCE_NSDATA,
	SHIM_OBJC_CLASS_NSDATE,
	SHIM_OBJC_INSTANCE_NSERROR,
	SHIM_OBJC_CLASS_NSUSERDEFAULTS,
	SHIM_OBJC_INSTANCE_NSUSERDEFAULTS,
	SHIM_OBJC_CLASS_NSNUMBER,
	SHIM_OBJC_INSTANCE_NSNUMBER,
	SHIM_OBJC_CLASS_NSARRAY,
	SHIM_OBJC_CLASS_NSMUTABLEARRAY,
	SHIM_OBJC_INSTANCE_NSMUTABLEARRAY,
	SHIM_OBJC_CLASS_NSDICTIONARY,
	SHIM_OBJC_CLASS_NSJSONSERIALIZATION,
	SHIM_OBJC_RESERVED_KIND_34,
	SHIM_OBJC_CLASS_NSMUTABLESTRING,
	SHIM_OBJC_CLASS_NSCONDITION,
	SHIM_OBJC_INSTANCE_NSCONDITION,
	SHIM_OBJC_CLASS_NSERROR,
	SHIM_OBJC_INSTANCE_NSDATE,
	SHIM_OBJC_INSTANCE_GENERIC,
	SHIM_OBJC_CLASS_GENERIC,
	SHIM_OBJC_CLASS_NSBUNDLE,
	SHIM_OBJC_INSTANCE_NSBUNDLE,
	SHIM_OBJC_NOTIFICATION_CENTER,
	SHIM_OBJC_NOTIFICATION_OBSERVER,
	SHIM_OBJC_CLASS_NSKEYEDARCHIVER,
	SHIM_OBJC_CLASS_NSKEYEDUNARCHIVER,
};

struct shim_objc_header {
	uint32_t magic;
	uint32_t kind;
};

struct shim_objc_string {
	struct shim_objc_header header;
	char utf8[];
};

struct shim_objc_array {
	struct shim_objc_header header;
	uint32_t count;
	uint32_t capacity;
	struct shim_objc_string** elements;
};

struct shim_cookie {
	char name[256];
	char value[4096];
	char domain[256];
	char path[1024];
	int secure;
	int http_only;
	time_t expires_at;
	int has_expiry;
};

struct shim_objc_cookie {
	struct shim_objc_header header;
	struct shim_cookie cookie;
};

struct shim_objc_dictionary {
	struct shim_objc_header header;
	uint32_t pair_count;
	uint32_t pair_capacity;
	struct shim_objc_string** keys;
};

struct shim_objc_url_components {
	struct shim_objc_header header;
	struct shim_objc_string* scheme;
	struct shim_objc_string* host;
	struct shim_objc_string* path;
};

struct shim_objc_condition {
	struct shim_objc_header header;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int initialized;
};

struct shim_objc_error {
	struct shim_objc_header header;
	long long code;
	char domain[256];
	char description[4096];
	void* user_info;
};

struct shim_objc_date {
	struct shim_objc_header header;
	struct timespec deadline;
};

struct shim_objc_property {
	struct shim_objc_property* next;
	char name[128];
	void* value;
};

struct shim_objc_notification {
	struct shim_objc_header header;
	void* name;
	void* object;
	void* block;
	struct shim_objc_notification* next;
};

struct shim_objc_notification_center {
	struct shim_objc_header header;
	pthread_mutex_t mutex;
	struct shim_objc_notification* observers;
	int observer_count;
	int initialized;
};

struct shim_objc_generic_object {
	struct shim_objc_header header;
	struct shim_objc_property* properties;
};



struct shim_objc_header OBJC_CLASS_$_NSBundle = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSBUNDLE,
};

static struct shim_objc_header shim_main_bundle_singleton = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_INSTANCE_NSBUNDLE,
};

static const char* shim_guest_executable_path(void)
{
	static _Thread_local char path[4096];
	int (*get_path)(char*, uint32_t*) = foundation_shim_symbol("_NSGetExecutablePath");
	uint32_t size = sizeof(path);
	return get_path && get_path(path, &size) == 0 ? path : NULL;
}

static const char* shim_guest_executable_directory(void)
{
	const char* path = shim_guest_executable_path();
	if (!path)
		return NULL;
	const char* last_slash = strrchr(path, '/');
	if (!last_slash)
		return NULL;
	static _Thread_local char directory[4096];
	size_t size = last_slash == path ? 1 : (size_t)(last_slash - path);
	memcpy(directory, path, size);
	directory[size] = '\0';
	return directory;
}

#define SHIM_COOKIE_PROPERTY(variable, text) \
	static const struct shim_objc_string variable = { \
		{ SHIM_OBJC_MAGIC, SHIM_OBJC_STRING }, text }

SHIM_COOKIE_PROPERTY(shim_cookie_domain_string, "Domain");
SHIM_COOKIE_PROPERTY(shim_cookie_expires_string, "Expires");
SHIM_COOKIE_PROPERTY(shim_cookie_name_string, "Name");
SHIM_COOKIE_PROPERTY(shim_cookie_originurl_string, "OriginURL");
SHIM_COOKIE_PROPERTY(shim_cookie_path_string, "Path");
SHIM_COOKIE_PROPERTY(shim_cookie_value_string, "Value");

static const struct shim_objc_string shim_nslocalized_description_key = {
	{ SHIM_OBJC_MAGIC, SHIM_OBJC_STRING }, "NSLocalizedDescription",
};

const void* const NSHTTPCookieDomain = &shim_cookie_domain_string;
const void* const NSHTTPCookieExpires = &shim_cookie_expires_string;
const void* const NSHTTPCookieName = &shim_cookie_name_string;
const void* const NSHTTPCookieOriginURL = &shim_cookie_originurl_string;
const void* const NSHTTPCookiePath = &shim_cookie_path_string;
const void* const NSHTTPCookieValue = &shim_cookie_value_string;
const void* const NSLocalizedDescriptionKey = &shim_nslocalized_description_key;

struct shim_objc_header OBJC_CLASS_$_NSFileManager = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSFILEMANAGER,
};

struct shim_objc_header OBJC_CLASS_$_NSFileHandle = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSFILEHANDLE,
};

struct shim_objc_header OBJC_CLASS_$_NSData = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSDATA,
};

struct shim_objc_data {
	struct shim_objc_header header;
	size_t length;
	char bytes[];
};

struct shim_objc_header OBJC_CLASS_$_NSKeyedArchiver = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSKEYEDARCHIVER,
};

struct shim_objc_header OBJC_CLASS_$_NSKeyedUnarchiver = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSKEYEDUNARCHIVER,
};

struct shim_objc_filehandle {
	struct shim_objc_header header;
	int fd;
};

static struct shim_objc_header shim_processinfo_singleton = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_INSTANCE_NSPROCESSINFO,
};

static struct shim_objc_header shim_cookie_storage_singleton = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_INSTANCE_NSHTTPCOOKIESTORAGE,
};


static struct shim_objc_header shim_filemanager_singleton = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_INSTANCE_NSFILEMANAGER,
};

static struct shim_objc_header shim_characterset_singleton = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_INSTANCE_NSCHARACTERSET,
};

static pthread_mutex_t shim_cookie_jar_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct shim_cookie shim_cookie_jar[256];
static uint32_t shim_cookie_jar_count;

static const uint64_t shim_objc_os_version[3] = {15, 0, 0};

static int shim_objc_is_object(const void* receiver, uint32_t kind)
{
	const struct shim_objc_header* header = receiver;

	if (!header || (uintptr_t)header < 4096)
        return 0;
    if (receiver == &OBJC_CLASS_$_NSString)
        return kind == SHIM_OBJC_CLASS_NSSTRING;
    if (receiver == &OBJC_CLASS_$_NSProcessInfo) {
        return kind == SHIM_OBJC_CLASS_NSPROCESSINFO;
    }
	return header->magic == SHIM_OBJC_MAGIC && header->kind == kind;
}

static void* shim_objc_make_string(const char* text)
{
	size_t length = strlen(text);
	struct shim_objc_string* result = malloc(sizeof(*result) + length + 1);
	if (!result)
		return NULL;
	result->header.magic = SHIM_OBJC_MAGIC;
	result->header.kind = SHIM_OBJC_STRING;
	memcpy(result->utf8, text, length + 1);
	return result;
}

static void* shim_objc_make_array(struct shim_objc_string* const* elements, uint32_t count)
{
	uint32_t capacity = count < 64 ? 64 : count;
	struct shim_objc_array* result =
		calloc(1, sizeof(*result));

	if (!result)
		return NULL;
	result->header.magic = SHIM_OBJC_MAGIC;
	result->header.kind = SHIM_OBJC_ARRAY;
	result->count = count;
	result->capacity = capacity;
    result->elements = calloc(capacity, sizeof(*result->elements));
    if (!result->elements) { free(result); return NULL; }
	for (uint32_t element_index = 0; element_index < count; element_index++)
		result->elements[element_index] = elements ? foundation_retain(elements[element_index]) : NULL;
	return result;
}

static void* shim_objc_make_single_element_array(const char* text)
{
	struct shim_objc_string* element = shim_objc_make_string(text);
	if (!element)
		return NULL;
	return shim_objc_make_array(&element, 1);
}

static struct shim_objc_condition* shim_objc_condition_init(
	struct shim_objc_condition* condition)
{
	pthread_mutexattr_t mutex_attributes;

	if (condition->initialized)
		return condition;
	pthread_mutexattr_init(&mutex_attributes);
	pthread_mutexattr_settype(&mutex_attributes, PTHREAD_MUTEX_RECURSIVE);
	if (pthread_mutex_init(&condition->mutex, &mutex_attributes) != 0) {
		pthread_mutexattr_destroy(&mutex_attributes);
		return NULL;
	}
	pthread_mutexattr_destroy(&mutex_attributes);
	if (pthread_cond_init(&condition->cond, NULL) != 0)
		return NULL;
	condition->initialized = 1;
	return condition;
}

static void shim_copy_cstring(char* destination, size_t destination_size,
                              const char* source);

static struct shim_objc_error* shim_objc_make_error(const char* domain,
                                                    long long code,
                                                    const char* description,
                                                    void* user_info)
{
	struct shim_objc_error* result = malloc(sizeof(*result));

	if (!result)
		return NULL;
	result->header.magic = SHIM_OBJC_MAGIC;
	result->header.kind = SHIM_OBJC_INSTANCE_NSERROR;
	result->code = code;
	shim_copy_cstring(result->domain, sizeof(result->domain),
	                  domain ? domain : "");
	shim_copy_cstring(result->description, sizeof(result->description),
	                  description ? description : "");
	result->user_info = user_info;
	return result;
}

static struct shim_objc_property* shim_objc_generic_find_property(
	const struct shim_objc_generic_object* object, const char* name)
{
	struct shim_objc_property* property = object->properties;

	while (property) {
		if (strcmp(property->name, name) == 0)
			return property;
		property = property->next;
	}
	return NULL;
}

static void shim_objc_generic_set_property(
	struct shim_objc_generic_object* object, const char* name, void* value)
{
	struct shim_objc_property* property =
		shim_objc_generic_find_property(object, name);

	if (property) {
		property->value = value;
		return;
	}
	property = malloc(sizeof(*property));
	if (!property)
		return;
	shim_copy_cstring(property->name, sizeof(property->name), name);
	property->value = value;
	property->next = object->properties;
	object->properties = property;
}

static struct shim_objc_generic_object* shim_objc_make_generic(void)
{
	struct shim_objc_generic_object* object = calloc(1, sizeof(*object));

	if (!object)
		return NULL;
	object->header.magic = SHIM_OBJC_MAGIC;
	object->header.kind = SHIM_OBJC_INSTANCE_GENERIC;
	return object;
}

struct shim_objc_generic_object OBJC_CLASS_$_MachGateGenericObject = {
	{ SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_GENERIC },
	NULL,
};

static const char* shim_objc_string_utf8(void* object);

static char* shim_objc_format_values(const char* format,
                                     const uintptr_t* values,
                                     int value_count)
{
	size_t capacity = strlen(format) * 4 + 256;
	char* result = malloc(capacity);
	size_t position = 0;
	int value_index = 0;

	if (!result)
		return NULL;

	for (const char* cursor = format; *cursor && position + 64 < capacity; cursor++) {
		if (*cursor != '%') {
			result[position++] = *cursor;
			continue;
		}
		const char* spec = cursor + 1;
		if (*spec == '%') {
			result[position++] = '%';
			cursor = spec;
			continue;
		}
		while (*spec == 'l' || *spec == 'z')
			spec++;
		if (*spec == '@' && value_index < value_count) {
			const char* text = shim_objc_string_utf8((void*)values[value_index++]);
			if (!text)
				text = "(null)";
			size_t length = strlen(text);
			if (position + length >= capacity) {
				length = capacity - position - 1;
			}
			memcpy(result + position, text, length);
			position += length;
			cursor = spec;
		} else if ((*spec == 'd' || *spec == 'u') && value_index < value_count) {
			uintptr_t value = values[value_index++];
			int written;
			if (*spec == 'd')
				written = snprintf(result + position, capacity - position, "%ld", (long)value);
			else
				written = snprintf(result + position, capacity - position, "%lu", (unsigned long)value);
			if (written > 0)
				position += (size_t)written;
			cursor = spec;
		} else {
			result[position++] = *cursor;
		}
	}
	result[position] = '\0';
	return result;
}


const char* foundation_archive_string_utf8(const void* object)
{
    if (!object || (uintptr_t)object < 4096)
        return NULL;
    const struct shim_objc_header* header = object;
    return header->magic == SHIM_OBJC_MAGIC && header->kind == SHIM_OBJC_STRING
        ? ((const struct shim_objc_string*)object)->utf8 : NULL;
}

static const char* shim_objc_string_utf8(void* object)
{
    const char* result = foundation_archive_string_utf8(object);
    if (result)
        return result;
    struct foundation_string* string = foundation_string_resolve(object);
    return string ? string->utf8 : NULL;
}

static void shim_parse_url_host(const char* url, char* host, size_t host_size)
{
	const char* scheme_end = strstr(url, "://");
	const char* host_start = scheme_end ? scheme_end + 3 : url;
	const char* host_end = strchr(host_start, '/');
	const char* port_start = strchr(host_start, ':');

	if (port_start && (!host_end || port_start < host_end))
		host_end = port_start;

	if (!host_end)
		host_end = host_start + strlen(host_start);
	if (!host_end)
		host_end = host_start;
	size_t length = (size_t)(host_end - host_start);
	if (length >= host_size)
		length = host_size - 1;
	memcpy(host, host_start, length);
	host[length] = '\0';
}

static int shim_domain_matches_host(const char* domain, const char* host)
{
	size_t domain_length = strlen(domain);
	size_t host_length = strlen(host);

	if (domain_length == 0 || host_length == 0)
		return 0;

	if (strcmp(domain, host) == 0)
		return 1;

	if (domain[0] != '.')
		return 0;

	if (host_length >= domain_length &&
	    strcmp(host + host_length - domain_length, domain) == 0)
		return 1;

	return host_length + 1 == domain_length &&
	       strncmp(domain + 1, host, host_length) == 0;
}

static void shim_copy_cstring(char* destination, size_t destination_size,
                              const char* source)
{
	size_t length = strlen(source);

	if (length >= destination_size)
		length = destination_size - 1;
	memcpy(destination, source, length);
	destination[length] = '\0';
}

static int shim_cookie_equals(const struct shim_cookie* left,
                              const struct shim_cookie* right)
{
	return strcmp(left->name, right->name) == 0 &&
	       strcmp(left->domain, right->domain) == 0 &&
	       strcmp(left->path, right->path) == 0;
}

static void shim_cookie_jar_remove(const struct shim_cookie* cookie)
{
	for (uint32_t index = 0; index < shim_cookie_jar_count; index++) {
		if (!shim_cookie_equals(&shim_cookie_jar[index], cookie))
			continue;
		for (uint32_t shift = index; shift + 1 < shim_cookie_jar_count; shift++)
			shim_cookie_jar[shift] = shim_cookie_jar[shift + 1];
		shim_cookie_jar_count--;
		return;
	}
}

static void shim_cookie_jar_store(const struct shim_cookie* cookie)
{
	shim_cookie_jar_remove(cookie);
	if (shim_cookie_jar_count >= 256)
		return;
	shim_cookie_jar[shim_cookie_jar_count++] = *cookie;
}

static void* shim_objc_make_cookie(const struct shim_cookie* cookie)
{
	struct shim_objc_cookie* result = malloc(sizeof(*result));

	if (!result)
		return NULL;
	result->header.magic = SHIM_OBJC_MAGIC;
	result->header.kind = SHIM_OBJC_INSTANCE_NSHTTPCOOKIE;
	result->cookie = *cookie;
	return result;
}

static void* shim_objc_make_url(const char* url)
{
	struct shim_objc_string* result =
		shim_objc_make_string(url);

	if (!result)
		return NULL;
	result->header.kind = SHIM_OBJC_INSTANCE_NSURL;
	return result;
}

static void* shim_objc_make_url_components(void)
{
	struct shim_objc_url_components* result = calloc(1, sizeof(*result));

	if (!result)
		return NULL;
	result->header.magic = SHIM_OBJC_MAGIC;
	result->header.kind = SHIM_OBJC_INSTANCE_NSURLCOMPONENTS;
	return result;
}

static int shim_is_url_path_allowed_character(unsigned char character)
{
	if (isalnum(character))
		return 1;
	return strchr("-._~!$&'()*+,;=:@/", character) != NULL;
}

static const char* shim_objc_url_string(void* url)
{
	if (!shim_objc_is_object(url, SHIM_OBJC_INSTANCE_NSURL))
		return NULL;
	return ((struct shim_objc_string*)url)->utf8;
}

static void* shim_objc_make_dictionary(struct shim_objc_string** keys,
                                       struct shim_objc_string** values,
                                       uint32_t pair_count)
{
	uint32_t capacity = pair_count < 8 ? 8 : pair_count;
	struct shim_objc_dictionary* result =
		calloc(1, sizeof(*result));

	if (!result)
		return NULL;
	result->header.magic = SHIM_OBJC_MAGIC;
	result->header.kind = SHIM_OBJC_INSTANCE_NSMUTABLEDICTIONARY;
	result->pair_count = pair_count;
	result->pair_capacity = capacity;
    result->keys = calloc(2 * capacity, sizeof(*result->keys));
    if (!result->keys) { free(result); return NULL; }
	for (uint32_t pair_index = 0; pair_index < pair_count; pair_index++) {
		result->keys[pair_index * 2] = foundation_retain(keys[pair_index]);
		result->keys[pair_index * 2 + 1] = foundation_retain(values[pair_index]);
	}
	return result;
}

struct shim_objc_userdefaults {
	struct shim_objc_header header;
	struct shim_objc_dictionary* values;
};

struct shim_objc_number {
	struct shim_objc_header header;
	union {
		long long integer_value;
		double double_value;
	} value;
	int is_floating;
};

struct shim_objc_header OBJC_CLASS_$_NSNumber = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSNUMBER,
};

struct shim_objc_header OBJC_CLASS_$_NSArray = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSARRAY,
};

struct shim_objc_header OBJC_CLASS_$_NSMutableArray = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSMUTABLEARRAY,
};

struct shim_objc_header OBJC_CLASS_$_NSDictionary = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSDICTIONARY,
};

struct shim_objc_header OBJC_CLASS_$_NSJSONSerialization = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSJSONSERIALIZATION,
};

void* __NSDictionary0__;
void* __NSArray0__;

__attribute__((constructor))
static void shim_init_empty_collection_singletons(void)
{
	if (!__NSDictionary0__)
		__NSDictionary0__ = shim_objc_make_dictionary(NULL, NULL, 0);
	if (!__NSArray0__)
		__NSArray0__ = shim_objc_make_array(NULL, 0);
}

static void* shim_objc_make_number(long long integer_value)
{
	struct shim_objc_number* result = malloc(sizeof(*result));
	if (!result)
		return NULL;
	result->header.magic = SHIM_OBJC_MAGIC;
	result->header.kind = SHIM_OBJC_INSTANCE_NSNUMBER;
	result->value.integer_value = integer_value;
	result->is_floating = 0;
	return result;
}

static int dictionary_keys_equal(void* left, void* right)
{
    if (left == right) return 1;
    const char* left_text = shim_objc_string_utf8(left);
    const char* right_text = shim_objc_string_utf8(right);
    return left_text && right_text && strcmp(left_text, right_text) == 0;
}

static struct shim_objc_dictionary* shim_objc_dictionary_set(
    struct shim_objc_dictionary* dictionary, void* key, void* value)
{
    if (!dictionary || !key || !value) return dictionary;
    for (uint32_t index = 0; index < dictionary->pair_count; ++index) {
        if (!dictionary_keys_equal(dictionary->keys[index * 2], key)) continue;
        void* previous = dictionary->keys[index * 2 + 1];
        dictionary->keys[index * 2 + 1] = foundation_retain(value);
        foundation_release(previous);
        return dictionary;
    }
    if (dictionary->pair_count == dictionary->pair_capacity) {
        uint32_t capacity = dictionary->pair_capacity * 2;
        if (capacity <= dictionary->pair_capacity) return dictionary;
        struct shim_objc_string** result = realloc(dictionary->keys,
            sizeof(*result) * 2 * capacity);
        if (!result) return dictionary;
        dictionary->keys = result;
        dictionary->pair_capacity = capacity;
    }
    dictionary->keys[dictionary->pair_count * 2] = foundation_retain(key);
    dictionary->keys[dictionary->pair_count * 2 + 1] = foundation_retain(value);
    ++dictionary->pair_count;
    return dictionary;
}

static void shim_objc_dictionary_remove(struct shim_objc_dictionary* dictionary,
    void* key)
{
    if (!dictionary || !key) return;
    for (uint32_t index = 0; index < dictionary->pair_count; ++index) {
        if (!dictionary_keys_equal(dictionary->keys[index * 2], key)) continue;
        foundation_release(dictionary->keys[index * 2]);
        foundation_release(dictionary->keys[index * 2 + 1]);
        for (uint32_t shift = index + 1; shift < dictionary->pair_count; shift++) {
            dictionary->keys[(shift - 1) * 2] = dictionary->keys[shift * 2];
            dictionary->keys[(shift - 1) * 2 + 1] = dictionary->keys[shift * 2 + 1];
        }
        --dictionary->pair_count;
        return;
    }
}

static void* shim_objc_dictionary_get(const struct shim_objc_dictionary* dictionary, void* key)
{
    for (uint32_t index = 0; index < dictionary->pair_count; ++index)
        if (dictionary_keys_equal(dictionary->keys[index * 2], key))
            return dictionary->keys[index * 2 + 1];
    return NULL;
}

static void* shim_objc_array_append(struct shim_objc_array* array, void* value);

static int shim_keyed_archive_is_dictionary(void* object)
{
    return shim_objc_is_object(object, SHIM_OBJC_INSTANCE_NSMUTABLEDICTIONARY);
}

enum {
    KEYED_ARCHIVE_TAG_STRING = 1,
    KEYED_ARCHIVE_TAG_DATA = 2,
    KEYED_ARCHIVE_TAG_DICTIONARY = 3,
    KEYED_ARCHIVE_TAG_ARRAY = 4,
};

struct keyed_archive_writer {
    char* bytes;
    size_t length;
    size_t capacity;
    int failed;
};

static void keyed_archive_reserve(struct keyed_archive_writer* writer,
    size_t additional)
{
    if (writer->failed ||
        writer->length + additional <= writer->capacity)
        return;
    size_t capacity = writer->capacity ? writer->capacity * 2 : 128;
    while (capacity < writer->length + additional)
        capacity *= 2;
    char* bytes = realloc(writer->bytes, capacity);
    if (!bytes) {
        writer->failed = 1;
        return;
    }
    writer->bytes = bytes;
    writer->capacity = capacity;
}

static void keyed_archive_write(struct keyed_archive_writer* writer,
    const void* data, size_t size)
{
    keyed_archive_reserve(writer, size);
    if (writer->failed)
        return;
    memcpy(writer->bytes + writer->length, data, size);
    writer->length += size;
}

static void keyed_archive_write_integer(struct keyed_archive_writer* writer,
    uint64_t value)
{
    unsigned char encoding[10];
    size_t size = 0;
    do {
        encoding[size++] = (unsigned char)(value & 0x7f);
        value >>= 7;
    } while (value);
    for (size_t index = 0; index < size; index++)
        encoding[index] |= (unsigned char)(index + 1 == size ? 0x00 : 0x80);
    keyed_archive_write(writer, encoding, size);
}

static void keyed_archive_encode_object(struct keyed_archive_writer* writer,
    void* object);

static void keyed_archive_encode_pairs(struct keyed_archive_writer* writer,
    struct shim_objc_string* const* pairs, uint32_t pair_count)
{
    keyed_archive_write_integer(writer, pair_count);
    for (uint32_t index = 0; index < pair_count && !writer->failed; index++) {
        keyed_archive_encode_object(writer, pairs[index * 2]);
        keyed_archive_encode_object(writer, pairs[index * 2 + 1]);
    }
}

static void keyed_archive_encode_object(struct keyed_archive_writer* writer,
    void* object)
{
    if (!object) {
        keyed_archive_write_integer(writer, 0);
        return;
    }
    if (shim_objc_is_object(object, SHIM_OBJC_STRING)) {
        const char* text = ((struct shim_objc_string*)object)->utf8;
        size_t length = strlen(text);
        keyed_archive_write_integer(writer, KEYED_ARCHIVE_TAG_STRING);
        keyed_archive_write_integer(writer, length);
        keyed_archive_write(writer, text, length);
        return;
    }
    struct foundation_string* foundation_string = foundation_string_resolve(object);
    const char* text = foundation_string ? foundation_string->utf8 : NULL;
    if (text) {
        size_t length = strlen(text);
        keyed_archive_write_integer(writer, KEYED_ARCHIVE_TAG_STRING);
        keyed_archive_write_integer(writer, length);
        keyed_archive_write(writer, text, length);
        return;
    }
    if (shim_objc_is_object(object, SHIM_OBJC_INSTANCE_NSDATA)) {
        const struct shim_objc_data* data = object;
        keyed_archive_write_integer(writer, KEYED_ARCHIVE_TAG_DATA);
        keyed_archive_write_integer(writer, data->length);
        keyed_archive_write(writer, data->bytes, data->length);
        return;
    }
    if (shim_keyed_archive_is_dictionary(object)) {
        const struct shim_objc_dictionary* dictionary = object;
        keyed_archive_write_integer(writer, KEYED_ARCHIVE_TAG_DICTIONARY);
        keyed_archive_encode_pairs(writer, dictionary->keys,
            dictionary->pair_count);
        return;
    }
    if (shim_objc_is_object(object, SHIM_OBJC_ARRAY)) {
        const struct shim_objc_array* array = object;
        keyed_archive_write_integer(writer, KEYED_ARCHIVE_TAG_ARRAY);
        keyed_archive_write_integer(writer, array->count);
        for (uint32_t index = 0; index < array->count && !writer->failed; index++)
            keyed_archive_encode_object(writer, array->elements[index]);
        return;
    }
    writer->failed = 1;
}

static void* shim_keyed_archive_encode(void* root)
{
    static const unsigned char header[] = {'M', 'G', 'K', 'A', 1};
    struct keyed_archive_writer writer = {0};
    keyed_archive_write(&writer, header, sizeof(header));
    keyed_archive_encode_object(&writer, root);
    if (writer.failed) {
        free(writer.bytes);
        return NULL;
    }
    return foundation_archive_data_create(writer.bytes, writer.length);
}

struct keyed_archive_reader {
    const unsigned char* bytes;
    size_t size;
    size_t position;
};

static int keyed_archive_read_integer(struct keyed_archive_reader* reader,
    uint64_t* value)
{
    uint64_t result = 0;
    unsigned int shift = 0;
    while (reader->position < reader->size) {
        unsigned char byte = reader->bytes[reader->position++];
        result |= (uint64_t)(byte & 0x7f) << shift;
        if (!(byte & 0x80)) {
            *value = result;
            return 1;
        }
        shift += 7;
        if (shift > 63)
            return 0;
    }
    return 0;
}

static int keyed_archive_read_bytes(struct keyed_archive_reader* reader,
    size_t length, const void** bytes)
{
    if (reader->size - reader->position < length)
        return 0;
    *bytes = reader->bytes + reader->position;
    reader->position += length;
    return 1;
}

static void* keyed_archive_decode_object(struct keyed_archive_reader* reader)
{
    uint64_t tag;
    if (!keyed_archive_read_integer(reader, &tag))
        return NULL;
    if (tag == 0)
        return NULL;
    uint64_t length;
    if (tag == KEYED_ARCHIVE_TAG_STRING) {
        if (!keyed_archive_read_integer(reader, &length) ||
            length > reader->size - reader->position)
            return NULL;
        char* text = malloc((size_t)length + 1);
        if (!text)
            return NULL;
        memcpy(text, reader->bytes + reader->position, (size_t)length);
        text[length] = '\0';
        reader->position += (size_t)length;
        void* result = shim_objc_make_string(text);
        free(text);
        return result;
    }
    if (tag == KEYED_ARCHIVE_TAG_DATA) {
        if (!keyed_archive_read_integer(reader, &length))
            return NULL;
        const void* bytes;
        if (!keyed_archive_read_bytes(reader, (size_t)length, &bytes))
            return NULL;
        return foundation_archive_data_create(bytes, (size_t)length);
    }
    if (tag == KEYED_ARCHIVE_TAG_DICTIONARY) {
        if (!keyed_archive_read_integer(reader, &length) ||
            length > SIZE_MAX / sizeof(void*) / 2)
            return NULL;
        struct shim_objc_dictionary* result = shim_objc_make_dictionary(NULL,
            NULL, 0);
        if (!result)
            return NULL;
        for (uint64_t index = 0; index < length; index++) {
            void* key = keyed_archive_decode_object(reader);
            void* value = keyed_archive_decode_object(reader);
            if (!key || !value)
                return result;
            result = shim_objc_dictionary_set(result, key, value);
        }
        return result;
    }
    if (tag == KEYED_ARCHIVE_TAG_ARRAY) {
        if (!keyed_archive_read_integer(reader, &length))
            return NULL;
        struct shim_objc_array* result = shim_objc_make_array(NULL, 0);
        if (!result)
            return NULL;
        for (uint64_t index = 0; index < length; index++) {
            void* element = keyed_archive_decode_object(reader);
            if (!element)
                break;
            shim_objc_array_append(result, element);
        }
        return result;
    }
    return NULL;
}

static void* shim_keyed_archive_decode(const void* bytes, size_t size)
{
    static const unsigned char header[] = {'M', 'G', 'K', 'A', 1};
    if (size < sizeof(header) || memcmp(bytes, header, sizeof(header)) != 0)
        return NULL;
    struct keyed_archive_reader reader = {bytes, size, sizeof(header)};
    return keyed_archive_decode_object(&reader);
}

static void* shim_objc_array_append(struct shim_objc_array* array, void* value)
{
    if (!array || !value) return array;
    if (array->count == array->capacity) {
        uint32_t capacity = array->capacity < 64 ? 64 : array->capacity * 2;
        if (capacity <= array->capacity) return array;        struct shim_objc_string** result = realloc(array->elements, sizeof(*result) * capacity);
        if (!result) return array;
        array->elements = result;
        array->capacity = capacity;
    }
    array->elements[array->count++] = foundation_retain(value);
    return array;
}

static void shim_json_escape_append(const char* text, char* out,
                                    size_t out_size, size_t* position)
{
	for (const char* cursor = text; *cursor; cursor++) {
		char ch = *cursor;
		if (ch == '"' || ch == '\\') {
			if (*position + 2 >= out_size)
				return;
			out[(*position)++] = '\\';
			out[(*position)++] = ch;
		} else if ((unsigned char)ch < 0x20) {
			if (*position + 6 >= out_size)
				return;
			*position += (size_t)snprintf(out + *position,
			                              out_size - *position, "\\u%04x", ch);
		} else {
			if (*position + 1 >= out_size)
				return;
			out[(*position)++] = ch;
		}
	}
	out[*position] = '\0';
}

static char* shim_json_serialize_object(void* object, int depth);

static int shim_json_serializable(const void* object)
{
	if (!object)
		return 1;
	if (!foundation_archive_readable(object, sizeof(struct shim_objc_header)))
		return 0;
	if (shim_objc_string_utf8((void*)object) ||
	    shim_objc_is_object((void*)object, SHIM_OBJC_INSTANCE_NSNUMBER) ||
	    shim_objc_is_object((void*)object, SHIM_OBJC_ARRAY) ||
	    shim_objc_is_object((void*)object,
	                        SHIM_OBJC_INSTANCE_NSMUTABLEDICTIONARY))
		return 1;
	const char* chars = *(const char**)((const char*)object + 16);
	size_t length = *(const size_t*)((const char*)object + 24);
	if ((uintptr_t)chars > 0x1000 && length > 0 && length <= (1 << 20) &&
	    foundation_archive_readable(chars, length + 1) && chars[length] == '\0')
		return 1;
	return 0;
}

static char* shim_json_serialize_object(void* object, int depth)
{
	size_t capacity = 4096;
	char* out;
	size_t position = 0;

	if (depth > 8)
		return NULL;
    if (!shim_json_serializable(object)) return NULL;
	out = malloc(capacity);
	if (!out)
		return NULL;
	out[0] = '\0';

	if (shim_objc_string_utf8(object)) {
		const char* text = shim_objc_string_utf8(object);
		out[position++] = '"';
		shim_json_escape_append(text, out, capacity, &position);
		out[position++] = '"';
		out[position] = '\0';
		return out;
	}
	if (shim_objc_is_object(object, SHIM_OBJC_INSTANCE_NSNUMBER)) {
		const struct shim_objc_number* number = object;
		if (number->is_floating)
			snprintf(out, capacity, "%g", number->value.double_value);
		else
			snprintf(out, capacity, "%lld", number->value.integer_value);
		return out;
	}
	if (shim_objc_is_object(object, SHIM_OBJC_ARRAY)) {
		const struct shim_objc_array* array = object;
		out[position++] = '[';
		for (uint32_t index = 0; index < array->count; index++) {
			char* element = shim_json_serialize_object(
				array->elements[index], depth + 1);
			if (!element) {
				free(out);
				return NULL;
			}
			if (index > 0)
				out[position++] = ',';
			size_t length = strlen(element);
			if (position + length + 2 >= capacity) {
				free(element);
				free(out);
				return NULL;
			}
			memcpy(out + position, element, length);
			position += length;
			free(element);
		}
		out[position++] = ']';
		out[position] = '\0';
		return out;
	}
	if (shim_objc_is_object(object, SHIM_OBJC_INSTANCE_NSMUTABLEDICTIONARY)) {
		const struct shim_objc_dictionary* dictionary = object;
		out[position++] = '{';
		for (uint32_t pair_index = 0;
		     pair_index < dictionary->pair_count; pair_index++) {
			if (pair_index > 0)
				out[position++] = ',';
			char* key_text = shim_json_serialize_object(
				dictionary->keys[pair_index * 2], depth + 1);
			char* value_text = shim_json_serialize_object(
				dictionary->keys[pair_index * 2 + 1], depth + 1);
			if (!key_text || !value_text) {
				free(key_text);
				free(value_text);
				free(out);
				return NULL;
			}
			size_t length = strlen(key_text) + strlen(value_text);
			if (position + length + 3 >= capacity) {
				free(key_text);
				free(value_text);
				free(out);
				return NULL;
			}
			memcpy(out + position, key_text, strlen(key_text));
			position += strlen(key_text);
			out[position++] = ':';
			memcpy(out + position, value_text, strlen(value_text));
			position += strlen(value_text);
			free(key_text);
			free(value_text);
		}
		out[position++] = '}';
		out[position] = '\0';
		return out;
	}
	if (!object) {
		snprintf(out, capacity, "null");
		return out;
	}
	if (foundation_archive_readable(object, sizeof(void*) * 4)) {
		const char* chars = *(const char**)((const char*)object + 16);
		size_t length = *(const size_t*)((const char*)object + 24);
		if ((uintptr_t)chars > 0x1000 && length > 0 && length <= (1 << 20) &&
		    foundation_archive_readable(chars, length + 1) && chars[length] == '\0') {
			out[position++] = '"';
			shim_json_escape_append(chars, out, capacity, &position);
			out[position++] = '"';
			out[position] = '\0';
			return out;
		}
	}
	free(out);
	return NULL;
}

static int shim_json_match(const char** cursor, char expected)
{
	while (**cursor == ' ' || **cursor == '\t' || **cursor == '\n' ||
	       **cursor == '\r')
		(*cursor)++;
	if (**cursor != expected)
		return 0;
	(*cursor)++;
	return 1;
}

static char* shim_json_read_string(const char** cursor)
{
	const char* start;
	size_t length;
	char* out;
	size_t position = 0;

	while (**cursor == ' ' || **cursor == '\t')
		(*cursor)++;
	if (**cursor != '"')
		return NULL;
	(*cursor)++;
	start = *cursor;
	while (**cursor && **cursor != '"') {
		if (**cursor == '\\' && *(*cursor + 1))
			(*cursor)++;
		(*cursor)++;
	}
	length = (size_t)(*cursor - start);
	out = malloc(length + 1);
	if (!out)
		return NULL;
	for (size_t index = 0; index < length; index++) {
		char ch = start[index];
		if (ch == '\\' && index + 1 < length)
			ch = start[++index];
		out[position++] = ch;
	}
	out[position] = '\0';
	if (**cursor == '"')
		(*cursor)++;
	return out;
}

static void* shim_json_parse_value(const char** cursor, int depth);

static void* shim_json_parse_value(const char** cursor, int depth)
{
	while (**cursor == ' ' || **cursor == '\t' || **cursor == '\n' ||
	       **cursor == '\r')
		(*cursor)++;

	if (depth > 8)
		return NULL;
	if (**cursor == '"') {
		char* text = shim_json_read_string(cursor);
		if (!text)
			return NULL;
		void* result = shim_objc_make_string(text);
		free(text);
		return result;
	}
	if (strncmp(*cursor, "true", 4) == 0) {
		*cursor += 4;
		return shim_objc_make_number(1);
	}
	if (strncmp(*cursor, "false", 5) == 0) {
		*cursor += 5;
		return shim_objc_make_number(0);
	}
	if (strncmp(*cursor, "null", 4) == 0) {
		*cursor += 4;
		return NULL;
	}
	if (**cursor == '[') {
		(*cursor)++;
		struct shim_objc_array* array =
			shim_objc_make_array(NULL, 0);
		if (!array)
			return NULL;
		while (**cursor && **cursor != ']') {
			void* value = shim_json_parse_value(cursor, depth + 1);
			if (value)
				array = shim_objc_array_append(array, value);
			while (**cursor == ' ' || **cursor == ',')
				(*cursor)++;
		}
		if (**cursor == ']')
			(*cursor)++;
		return array;
	}
	if (**cursor == '{') {
		(*cursor)++;
		struct shim_objc_dictionary* dictionary =
			shim_objc_make_dictionary(NULL, NULL, 0);
		if (!dictionary)
			return NULL;
		while (**cursor && **cursor != '}') {
			char* key_text = shim_json_read_string(cursor);
			if (!key_text)
				break;
			shim_json_match(cursor, ':');
			void* value = shim_json_parse_value(cursor, depth + 1);
			void* key = shim_objc_make_string(key_text);
			free(key_text);
			if (key)
				dictionary = shim_objc_dictionary_set(dictionary, key, value);
			while (**cursor == ' ' || **cursor == ',')
				(*cursor)++;
		}
		if (**cursor == '}')
			(*cursor)++;
		return dictionary;
	}
	if (**cursor == '-' || (**cursor >= '0' && **cursor <= '9')) {
		long long value = strtoll(*cursor, (char**)cursor, 10);
		return shim_objc_make_number(value);
	}
	return NULL;
}

struct shim_objc_header OBJC_CLASS_$_NSUserDefaults = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSUSERDEFAULTS,
};

#define SHIM_USERDEFAULTS_MAX_SUITES 64

struct shim_userdefaults_suite {
	char name[256];
	struct shim_objc_userdefaults* instance;
	int in_use;
};

static struct shim_userdefaults_suite shim_userdefaults_suites[SHIM_USERDEFAULTS_MAX_SUITES];
static pthread_mutex_t shim_userdefaults_mutex = PTHREAD_MUTEX_INITIALIZER;

static struct shim_objc_userdefaults* shim_userdefaults_for_suite(const char* suite_name)
{
	struct shim_objc_userdefaults* instance;
	int index;

	if (!suite_name)
		suite_name = "";

	for (index = 0; index < SHIM_USERDEFAULTS_MAX_SUITES; index++) {
		if (shim_userdefaults_suites[index].in_use &&
		    strcmp(shim_userdefaults_suites[index].name, suite_name) == 0)
			return shim_userdefaults_suites[index].instance;
	}

	for (index = 0; index < SHIM_USERDEFAULTS_MAX_SUITES; index++) {
		if (!shim_userdefaults_suites[index].in_use)
			break;
	}
	if (index == SHIM_USERDEFAULTS_MAX_SUITES)
		return NULL;

	instance = malloc(sizeof(*instance));
	if (!instance)
		return NULL;
	instance->header.magic = SHIM_OBJC_MAGIC;
	instance->header.kind = SHIM_OBJC_INSTANCE_NSUSERDEFAULTS;
	instance->values = shim_objc_make_dictionary(NULL, NULL, 0);
	if (!instance->values) {
		free(instance);
		return NULL;
	}

	shim_userdefaults_suites[index].in_use = 1;
	snprintf(shim_userdefaults_suites[index].name,
	         sizeof(shim_userdefaults_suites[index].name), "%s",
	         suite_name);
	shim_userdefaults_suites[index].instance = instance;
	return instance;
}

static void shim_userdefaults_set(struct shim_objc_userdefaults* defaults,
    struct shim_objc_string* key, struct shim_objc_string* value)
{
    shim_objc_dictionary_set(defaults->values, key, value);
}

static struct shim_objc_string* shim_userdefaults_get(
	struct shim_objc_userdefaults* defaults, struct shim_objc_string* key)
{
	const struct shim_objc_dictionary* dict = defaults->values;
	const char* key_text = shim_objc_string_utf8(key);
	uint32_t pair_index;

	if (!key_text)
		return NULL;

	for (pair_index = 0; pair_index < dict->pair_count; pair_index++) {
		const char* existing = shim_objc_string_utf8(dict->keys[pair_index * 2]);
		if (existing && strcmp(existing, key_text) == 0)
			return dict->keys[pair_index * 2 + 1];
	}
	return NULL;
}

static void shim_userdefaults_remove(struct shim_objc_userdefaults* defaults,
                                     struct shim_objc_string* key)
{
	struct shim_objc_dictionary* dict = defaults->values;
	const char* key_text = shim_objc_string_utf8(key);
	uint32_t pair_index;

	if (!key_text)
		return;

	for (pair_index = 0; pair_index < dict->pair_count; pair_index++) {
		const char* existing = shim_objc_string_utf8(dict->keys[pair_index * 2]);
		if (existing && strcmp(existing, key_text) == 0) {
			uint32_t last = dict->pair_count - 1;
			dict->keys[pair_index * 2] = dict->keys[last * 2];
			dict->keys[pair_index * 2 + 1] = dict->keys[last * 2 + 1];
			dict->pair_count--;
			return;
		}
	}
}

struct shim_objc_header OBJC_CLASS_$_NSHTTPCookie = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSHTTPCOOKIE,
};

struct shim_objc_header OBJC_CLASS_$_NSHTTPCookieStorage = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSHTTPCOOKIESTORAGE,
};

struct shim_objc_header OBJC_CLASS_$_NSURL = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSURL,
};

struct shim_objc_header OBJC_CLASS_$_NSMutableDictionary = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSMUTABLEDICTIONARY,
};

struct shim_objc_header OBJC_CLASS_$_NSCharacterSet = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSCHARACTERSET,
};

struct shim_objc_header OBJC_CLASS_$_NSURLComponents = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSURLCOMPONENTS,
};

struct shim_objc_header OBJC_CLASS_$_NSDate = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSDATE,
};

struct shim_objc_header OBJC_CLASS_$_NSCondition = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSCONDITION,
};

struct shim_objc_header OBJC_CLASS_$_NSError = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSERROR,
};

struct shim_objc_notification_center OBJC_CLASS_$_NSNotificationCenter = {
	{ SHIM_OBJC_MAGIC, SHIM_OBJC_NOTIFICATION_CENTER },
	PTHREAD_MUTEX_INITIALIZER,
	NULL,
	0,
	0,
};

static const struct shim_objc_string shim_nshttp_cookies_changed_notification = {
	{ SHIM_OBJC_MAGIC, SHIM_OBJC_STRING }, "NSHTTPCookieManagerCookiesChangedNotification",
};

const void* const NSHTTPCookieManagerCookiesChangedNotification =
	&shim_nshttp_cookies_changed_notification;

static struct shim_objc_notification_center shim_notification_center_singleton = {
	{ SHIM_OBJC_MAGIC, SHIM_OBJC_NOTIFICATION_CENTER },
	PTHREAD_MUTEX_INITIALIZER,
	NULL,
	0,
	0,
};

static void shim_notification_release_block(void* block)
{
	void (*release)(void*) = foundation_shim_symbol("_Block_release");
	if (release)
		release(block);
}

static void shim_notification_post(struct shim_objc_notification_center* center,
                                  void* name, void* object, void* user_info)
{
	const char* name_text = shim_objc_string_utf8(name);
	if (!name_text)
		return;
	pthread_mutex_lock(&center->mutex);
	void** blocks = calloc(center->observer_count ? center->observer_count : 1,
	                       sizeof(*blocks));
	if (!blocks) {
		pthread_mutex_unlock(&center->mutex);
		return;
	}
	size_t count = 0;
	for (struct shim_objc_notification* observer = center->observers;
	     observer; observer = observer->next) {
		const char* expected = shim_objc_string_utf8(observer->name);
		if (observer->name && (!expected || strcmp(expected, name_text) != 0))
			continue;
		if (observer->object && observer->object != object)
			continue;
		blocks[count++] = archive_copy_block(observer->block);
	}
	pthread_mutex_unlock(&center->mutex);
	struct shim_objc_generic_object* notification = shim_objc_make_generic();
	if (notification) {
		shim_objc_generic_set_property(notification, "name", foundation_retain(name));
		shim_objc_generic_set_property(notification, "object", foundation_retain(object));
		shim_objc_generic_set_property(notification, "userInfo", foundation_retain(user_info));
	}
	for (size_t index = 0; index < count; index++) {
		if (notification)
			archive_invoke_block(blocks[index], notification);
		shim_notification_release_block(blocks[index]);
	}
	free(blocks);
}

static void shim_cookie_storage_notify(void)
{
	shim_notification_post(&shim_notification_center_singleton,
	                       (void*)NSHTTPCookieManagerCookiesChangedNotification,
	                       &shim_cookie_storage_singleton, NULL);
}

static int shim_cookie_path_matches(const char* cookie_path, const char* url_path)
{
	size_t cookie_length = strlen(cookie_path);
	size_t url_length = strlen(url_path);

	if (cookie_length == 0)
		return 1;
	if (strncmp(url_path, cookie_path, cookie_length) != 0)
		return 0;
	if (url_length == cookie_length)
		return 1;
	if (cookie_path[cookie_length - 1] == '/')
		return 1;
	return url_path[cookie_length] == '/';
}

static int shim_cookie_matches_url(const struct shim_cookie* cookie,
                                   const char* url)
{
	char host[256];
	const char* scheme_end;
	const char* path_start;
	const char* query_start;
	char url_path[1024];
	size_t path_length;
	time_t now;

	shim_parse_url_host(url, host, sizeof(host));
	if (!shim_domain_matches_host(cookie->domain, host))
		return 0;

	if (cookie->secure && strncmp(url, "https", 5) != 0)
		return 0;

	scheme_end = strstr(url, "://");
	path_start = strchr(scheme_end ? scheme_end + 3 : url, '/');
	query_start = strchr(url, '?');
	if (!path_start)
		path_start = "";
	if (!query_start || query_start < path_start)
		query_start = url + strlen(url);
	path_length = (size_t)(query_start - path_start);
	if (path_length >= sizeof(url_path))
		path_length = sizeof(url_path) - 1;
	memcpy(url_path, path_start, path_length);
	url_path[path_length] = '\0';

	if (!shim_cookie_path_matches(cookie->path, url_path))
		return 0;

	if (cookie->has_expiry) {
		now = time(NULL);
		if (now >= cookie->expires_at)
			return 0;
	}
	return 1;
}

static void* shim_cookie_jar_matching_array(const char* url)
{
	struct shim_objc_cookie** cookies;
	uint32_t match_count = 0;

	cookies = malloc(sizeof(*cookies) * (shim_cookie_jar_count ? shim_cookie_jar_count : 1));
	if (!cookies)
		return NULL;

	for (uint32_t index = 0; index < shim_cookie_jar_count; index++) {
		if (!shim_cookie_matches_url(&shim_cookie_jar[index],
		                             url ? url : ""))
			continue;
		void* cookie = shim_objc_make_cookie(&shim_cookie_jar[index]);
		if (!cookie)
			continue;
		cookies[match_count++] = cookie;
	}

	struct shim_objc_array* result =
		shim_objc_make_array(NULL, match_count);
	if (!result) {
		free(cookies);
		return NULL;
	}
	result->header.magic = SHIM_OBJC_MAGIC;
	result->header.kind = SHIM_OBJC_ARRAY;
	result->count = match_count;
	result->capacity = match_count < 64 ? 64 : match_count;
	for (uint32_t index = 0; index < match_count; index++)
		((struct shim_objc_string**)result->elements)[index] =
			(struct shim_objc_string*)cookies[index];
	free(cookies);
	return result;
}

static void* shim_cookie_jar_all_array(void)
{
	struct shim_objc_array* result =
		shim_objc_make_array(NULL, shim_cookie_jar_count);

	if (!result)
		return NULL;
	result->header.magic = SHIM_OBJC_MAGIC;
	result->header.kind = SHIM_OBJC_ARRAY;
	result->count = shim_cookie_jar_count;
	for (uint32_t index = 0; index < shim_cookie_jar_count; index++) {
		void* cookie = shim_objc_make_cookie(&shim_cookie_jar[index]);
		if (!cookie)
			cookie = shim_objc_make_string("");
		((struct shim_objc_string**)result->elements)[index] = cookie;
	}
	return result;
}

static int shim_parse_http_month(const char* name)
{
	static const char* const months[] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
	};

	for (int index = 0; index < 12; index++) {
		if (strncasecmp(name, months[index], 3) == 0)
			return index;
	}
	return -1;
}

static time_t shim_parse_http_date(const char* text)
{
	int day = 0;
	int year = 0;
	int hour = 0;
	int minute = 0;
	int second = 0;
	char month_text[8];
	struct tm parsed;

	if (sscanf(text, "%*3s %d %3s %d %d:%d:%d", &day, month_text,
	           &year, &hour, &minute, &second) >= 6)
		;
	else if (sscanf(text, "%*3s, %d %3s %d %d:%d:%d", &day, month_text,
	                &year, &hour, &minute, &second) >= 6)
		;
	else
		return 0;
	int month = shim_parse_http_month(month_text);
	if (month < 0)
		return 0;
	memset(&parsed, 0, sizeof(parsed));
	parsed.tm_mday = day;
	parsed.tm_mon = month;
	parsed.tm_year = year - 1900;
	parsed.tm_hour = hour;
	parsed.tm_min = minute;
	parsed.tm_sec = second;
	return timegm(&parsed);
}

static void shim_parse_set_cookie_header(const char* header, struct shim_cookie* cookie)
{
	const char* separator = strchr(header, ';');
	size_t name_value_length = separator ? (size_t)(separator - header) : strlen(header);
	char name_value[2304];
	const char* equals_sign;

	if (name_value_length >= sizeof(name_value))
		name_value_length = sizeof(name_value) - 1;
	memcpy(name_value, header, name_value_length);
	name_value[name_value_length] = '\0';

	memset(cookie, 0, sizeof(*cookie));
	shim_copy_cstring(cookie->path, sizeof(cookie->path), "/");

	equals_sign = strchr(name_value, '=');
	if (!equals_sign) {
		shim_copy_cstring(cookie->name, sizeof(cookie->name), name_value);
		return;
	}
	size_t name_length = (size_t)(equals_sign - name_value);
	if (name_length >= sizeof(cookie->name))
		name_length = sizeof(cookie->name) - 1;
	memcpy(cookie->name, name_value, name_length);
	cookie->name[name_length] = '\0';
	shim_copy_cstring(cookie->value, sizeof(cookie->value), equals_sign + 1);

	const char* attribute = separator;
	while (attribute) {
		attribute++;
		while (*attribute == ' ')
			attribute++;
		if (strncasecmp(attribute, "domain=", 7) == 0) {
			const char* domain = attribute + 7;
			char domain_buffer[254];
			size_t domain_length = 0;
			while (domain[domain_length] && domain[domain_length] != ';' &&
			       domain_length < sizeof(domain_buffer) - 1) {
				domain_buffer[domain_length] = domain[domain_length];
				domain_length++;
			}
			domain_buffer[domain_length] = '\0';
			if (domain_length > 0) {
				if (domain_buffer[0] == '.')
					shim_copy_cstring(cookie->domain, sizeof(cookie->domain),
					                  domain_buffer);
				else
					snprintf(cookie->domain, sizeof(cookie->domain), ".%s",
					         domain_buffer);
			}
		} else if (strncasecmp(attribute, "path=", 5) == 0) {
			const char* path = attribute + 5;
			size_t path_length = 0;
			while (path[path_length] && path[path_length] != ';' &&
			       path_length < sizeof(cookie->path) - 1) {
				cookie->path[path_length] = path[path_length];
				path_length++;
			}
			cookie->path[path_length] = '\0';
		} else if (strncasecmp(attribute, "secure", 6) == 0) {
			cookie->secure = 1;
		} else if (strncasecmp(attribute, "httponly", 8) == 0) {
			cookie->http_only = 1;
		} else if (strncasecmp(attribute, "expires=", 8) == 0) {
			char date_text[64];
			size_t date_length = 0;
			const char* date_value = attribute + 8;
			while (date_value[date_length] && date_value[date_length] != ';' &&
			       date_length < sizeof(date_text) - 1) {
				date_text[date_length] = date_value[date_length];
				date_length++;
			}
			date_text[date_length] = '\0';
			time_t expires_at = shim_parse_http_date(date_text);
			if (expires_at != 0) {
				cookie->expires_at = expires_at;
				cookie->has_expiry = 1;
			}
		} else if (strncasecmp(attribute, "max-age=", 8) == 0) {
			char age_text[16];
			size_t age_length = 0;
			const char* age_value = attribute + 8;
			long age_seconds;
			while (age_value[age_length] && age_value[age_length] != ';' &&
			       age_length < sizeof(age_text) - 1) {
				age_text[age_length] = age_value[age_length];
				age_length++;
			}
			age_text[age_length] = '\0';
			age_seconds = strtol(age_text, NULL, 10);
			cookie->expires_at = time(NULL) + age_seconds;
			cookie->has_expiry = 1;
		}
		attribute = strchr(attribute, ';');
	}
}

static void* shim_parse_response_header_cookies(void* header_fields, void* url)
{
	const struct shim_objc_dictionary* fields = header_fields;
	const char* set_cookie_value = NULL;
	const char* url_text = shim_objc_url_string(url);
	struct shim_cookie cookie;
	char host[256];

	if (!fields || !shim_objc_is_object(fields, SHIM_OBJC_INSTANCE_NSMUTABLEDICTIONARY))
		return NULL;

	for (uint32_t pair_index = 0; pair_index < fields->pair_count; pair_index++) {
		const char* key = shim_objc_string_utf8(fields->keys[pair_index * 2]);
		if (key && strcasecmp(key, "Set-Cookie") == 0) {
			set_cookie_value = shim_objc_string_utf8(fields->keys[pair_index * 2 + 1]);
			break;
		}
	}
	if (!set_cookie_value)
		return NULL;

	shim_parse_set_cookie_header(set_cookie_value, &cookie);
	if (cookie.domain[0] == '\0') {
		shim_parse_url_host(url_text ? url_text : "", host, sizeof(host));
		shim_copy_cstring(cookie.domain, sizeof(cookie.domain), host);
	}

	struct shim_objc_array* result =
		shim_objc_make_array(NULL, 1);
	if (!result)
		return NULL;
	result->header.magic = SHIM_OBJC_MAGIC;
	result->header.kind = SHIM_OBJC_ARRAY;
	result->count = 1;
	result->capacity = 64;
	result->elements[0] = shim_objc_make_cookie(&cookie);
	if (!result->elements[0]) {
		free(result);
		return NULL;
	}
	return result;
}

static void* shim_cookie_request_header_fields(void* cookies_array)
{
	struct shim_objc_array* array = cookies_array;
	char* joined = NULL;
	size_t joined_length = 0;
	void* result;

	if (!array || !shim_objc_is_object(array, SHIM_OBJC_ARRAY) || array->count == 0)
		return NULL;

	for (uint32_t index = 0; index < array->count; index++) {
		struct shim_objc_cookie* cookie = (struct shim_objc_cookie*)array->elements[index];
		if (!cookie || !shim_objc_is_object(cookie, SHIM_OBJC_INSTANCE_NSHTTPCOOKIE))
			continue;
		size_t entry_length = strlen(cookie->cookie.name) + 1 +
		                      strlen(cookie->cookie.value) + 3;
		size_t separator_length = joined_length > 0 ? 2 : 0;
		size_t needed = joined_length + separator_length + entry_length;
		char* reallocated = realloc(joined, needed);
		if (!reallocated) {
			free(joined);
			return NULL;
		}
		joined = reallocated;
		if (separator_length == 2) {
			joined[joined_length++] = ';';
			joined[joined_length++] = ' ';
		}
		int written = snprintf(joined + joined_length, needed - joined_length,
		                       "%s=%s", cookie->cookie.name, cookie->cookie.value);
		if (written < 0) {
			free(joined);
			return NULL;
		}
		joined_length += (size_t)written;
	}
	if (!joined)
		return NULL;

	struct shim_objc_string* header_value = shim_objc_make_string(joined);
	free(joined);
	if (!header_value)
		return NULL;

	struct shim_objc_string* header_key = shim_objc_make_string("Cookie");
	if (!header_key) {
		free(header_value);
		return NULL;
	}

	result = shim_objc_make_dictionary(&header_key, &header_value, 1);
	if (!result) {
		free(header_key);
		free(header_value);
	}
	return result;
}

static void* shim_objc_generic_msgsend(void* receiver, const char* selector,
                                       uintptr_t a2, uintptr_t a3,
                                       uintptr_t a4, uintptr_t a5)
{
    (void)a4;
    (void)a5;
	struct shim_objc_generic_object* object = receiver;
	size_t selector_length = strlen(selector);
	char accessor[160];

	if (strcmp(selector, "alloc") == 0 ||
	    strcmp(selector, "allocWithZone:") == 0 ||
	    strcmp(selector, "new") == 0)
		return shim_objc_make_generic();
	if (strcmp(selector, "init") == 0 ||
	    strcmp(selector, "copy") == 0 ||
	    strcmp(selector, "mutableCopy") == 0 ||
	    strcmp(selector, "retain") == 0 ||
	    strcmp(selector, "autorelease") == 0)
		return receiver;

	if (selector_length >= 4 && selector_length < sizeof(accessor) &&
	    strncmp(selector, "set", 3) == 0 &&
	    selector[selector_length - 1] == ':') {
		size_t name_length = selector_length - 4;
		memcpy(accessor, selector + 3, name_length);
		if (name_length > 0)
			accessor[0] = (char)tolower((unsigned char)accessor[0]);
		accessor[name_length] = '\0';
		void* value = (void*)a2;
		if (!value && strcmp(accessor, "object") != 0)
			value = shim_objc_make_string("");
		if (!value)
			return NULL;
		shim_objc_generic_set_property(object, accessor, value);
		return NULL;
	}

	if (selector_length >= 3 && selector_length + 1 < sizeof(accessor)) {
		memcpy(accessor, selector, selector_length);
		accessor[selector_length] = '\0';
		struct shim_objc_property* property =
			shim_objc_generic_find_property(object, accessor);
		if (property)
			return property->value;
		if (strcmp(accessor, "count") == 0)
			return (void*)(uintptr_t)1;
	}

	if (strcmp(selector, "objectAtIndexedSubscript:") == 0 ||
	    strcmp(selector, "objectForKeyedSubscript:") == 0)
		return NULL;
	if (strcmp(selector, "countByEnumeratingWithState:objects:count:") == 0)
		return NULL;
	if (strcmp(selector, "addObject:") == 0) {
		shim_objc_generic_set_property(object, "object", (void*)a2);
		return NULL;
	}
	if (strcmp(selector, "arrayWithObjects:count:") == 0)
		return shim_objc_make_array(NULL, 0);
	if (strcmp(selector, "stringWithFormat:") == 0)
		return shim_objc_make_string("");
	if (strcmp(selector, "stringValue") == 0) {
		struct shim_objc_property* first_argument =
			shim_objc_generic_find_property(object, "value");
		if (first_argument)
			return first_argument->value;
		return shim_objc_make_string("");
	}

	size_t selector_length_for_factory = strlen(selector);
	if (selector_length_for_factory >= 2 &&
	    selector_length_for_factory + 1 < sizeof(accessor) &&
	    selector[selector_length_for_factory - 1] == ':') {
		size_t name_length = selector_length_for_factory - 1;
		int colon_count = 0;
		for (size_t index = 0; index < selector_length_for_factory; index++) {
			if (selector[index] == ':')
				colon_count++;
		}
		memcpy(accessor, selector, name_length);
		accessor[0] = (char)tolower((unsigned char)accessor[0]);
		accessor[name_length] = '\0';
		struct shim_objc_generic_object* created = shim_objc_make_generic();
		if (!created)
			return NULL;
		shim_objc_generic_set_property(created, accessor, (void*)a2);
		if (colon_count >= 2)
			shim_objc_generic_set_property(created, "value", (void*)a3);
		else
			shim_objc_generic_set_property(created, "value", (void*)a2);
		return created;
	}

	return shim_objc_make_generic();
}

static void* archive_message_impl(void* receiver, void* sel, uintptr_t a2, uintptr_t a3,
                             uintptr_t a4, uintptr_t a5, uintptr_t a6, uintptr_t a7,
                             void* sret, const uintptr_t* guest_stack_args)
{
	const char* selector = sel;

	if (!receiver || !selector)
		return NULL;


	if (getenv("MACHGATE_TRACE_OBJC_MSGSEND")) {
		void* trace_isa = foundation_archive_readable(receiver, sizeof(void*))
			? *(void**)receiver : NULL;
		fprintf(stderr,
		        "libsystem_shim: objc_msgSend [%s] recv=%p isa=%p a2=%p a3=%p a4=%p a5=%p a6=%p a7=%p\n",
		        selector, receiver, trace_isa, (void*)a2, (void*)a3,
		        (void*)a4, (void*)a5, (void*)a6, (void*)a7);
	}


	if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_GENERIC) ||
	    shim_objc_is_object(receiver, SHIM_OBJC_CLASS_GENERIC)) {
		void* generic_result = shim_objc_generic_msgsend(
			receiver, selector, a2, a3, a4, a5);
		if (generic_result || strcmp(selector, "alloc") == 0 ||
		    strcmp(selector, "new") == 0)
			return generic_result;
	}

	if (strcmp(selector, "processInfo") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSPROCESSINFO))
			return &shim_processinfo_singleton;
	} else if (strcmp(selector, "mainBundle") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSBUNDLE))
			return &shim_main_bundle_singleton;
	} else if (strcmp(selector, "bundleWithPath:") == 0 ||
	           strcmp(selector, "bundleWithIdentifier:") == 0 ||
	           strcmp(selector, "bundleForClass:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSBUNDLE))
			return &shim_main_bundle_singleton;
	} else if (strcmp(selector, "resourcePath") == 0 ||
	           strcmp(selector, "bundlePath") == 0 ||
	           strcmp(selector, "executablePath") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSBUNDLE)) {
			const char* directory = shim_guest_executable_directory();
			if (!directory)
				return NULL;
			if (strcmp(selector, "executablePath") == 0) {
				const char* path = shim_guest_executable_path();
				return path ? shim_objc_make_string(path) : NULL;
			}
			return shim_objc_make_string(directory);
		}
	} else if (strcmp(selector, "resourceURL") == 0 ||
	           strcmp(selector, "bundleURL") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSBUNDLE)) {
			const char* directory = shim_guest_executable_directory();
			char url[4100];
			if (!directory)
				return NULL;
			if (snprintf(url, sizeof(url), "file://%s", directory)
			        >= (int)sizeof(url))
				return NULL;
			return shim_objc_make_url(url);
		}
	} else if (strcmp(selector, "stringByAppendingString:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_STRING)) {
			const char* base_text =
				((struct shim_objc_string*)receiver)->utf8;
			const char* suffix =
				shim_objc_string_utf8((void*)a2);
			char joined[8192];
			if (!suffix)
				return NULL;
			if (snprintf(joined, sizeof(joined), "%s%s", base_text,
			             suffix) >= (int)sizeof(joined))
				return NULL;
			return shim_objc_make_string(joined);
		}
	} else if (strcmp(selector, "defaultCenter") == 0) {
		return &shim_notification_center_singleton;
	} else if (strcmp(selector, "addObserverForName:object:queue:usingBlock:") == 0) {
		struct shim_objc_notification_center* center = receiver;
		if (shim_objc_is_object(center, SHIM_OBJC_NOTIFICATION_CENTER)) {
			struct shim_objc_notification* observer =
				malloc(sizeof(*observer));
			if (!observer)
				return NULL;
			observer->header.magic = SHIM_OBJC_MAGIC;
			observer->header.kind = SHIM_OBJC_NOTIFICATION_OBSERVER;
			observer->name = foundation_retain((void*)a2);
			observer->object = foundation_retain((void*)a3);
			observer->block = archive_copy_block((void*)a5);
			pthread_mutex_lock(&center->mutex);
			observer->next = center->observers;
			center->observers = observer;
			center->observer_count++;
			pthread_mutex_unlock(&center->mutex);
			return observer;
		}
	} else if (strcmp(selector, "removeObserver:") == 0) {
		struct shim_objc_notification_center* center = receiver;
		if (shim_objc_is_object(center, SHIM_OBJC_NOTIFICATION_CENTER)) {
			pthread_mutex_lock(&center->mutex);
			struct shim_objc_notification** link = &center->observers;
			while (*link) {
				if (*link == (void*)a2) {
					struct shim_objc_notification* removed = *link;
					*link = removed->next;
					center->observer_count--;
					pthread_mutex_unlock(&center->mutex);
					shim_notification_release_block(removed->block);
					foundation_release(removed->name);
					foundation_release(removed->object);
					free(removed);
					return NULL;
				}
				link = &(*link)->next;
			}
			pthread_mutex_unlock(&center->mutex);
			return NULL;
		}
	} else if (strcmp(selector, "postNotificationName:object:") == 0 ||
	           strcmp(selector, "postNotificationName:object:userInfo:") == 0) {
		struct shim_objc_notification_center* center = receiver;
		if (shim_objc_is_object(center, SHIM_OBJC_NOTIFICATION_CENTER)) {
			void* user_info = strcmp(selector, "postNotificationName:object:userInfo:") == 0
				? (void*)a4 : NULL;
			shim_notification_post(center, (void*)a2, (void*)a3, user_info);
			return NULL;
		}
	} else if (strcmp(selector, "operatingSystemVersion") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSPROCESSINFO) && sret) {
			memcpy(sret, shim_objc_os_version, sizeof(shim_objc_os_version));
			return sret;
		}
	} else if (strcmp(selector, "stringWithFormat:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSSTRING)) {
			const char* format = shim_objc_string_utf8((void*)a2);
			char* text;
			void* result;
			if (!format)
				return NULL;
			uintptr_t call_values[9] = {0};
			if (guest_stack_args) {
				for (int value_index = 0; value_index < 8; value_index++)
					call_values[value_index] =
						guest_stack_args[value_index];
			}
			text = shim_objc_format_values(format, call_values, 8);
			if (!text)
				return NULL;
			result = shim_objc_make_string(text);
			free(text);
			return result;
		}
	} else if (strcmp(selector, "UTF8String") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_STRING))
			return ((struct shim_objc_string*)receiver)->utf8;
		return (void*)shim_objc_string_utf8(receiver);
	} else if (strcmp(selector, "cStringUsingEncoding:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_STRING))
			return ((struct shim_objc_string*)receiver)->utf8;
		return (void*)shim_objc_string_utf8(receiver);
	} else if (strcmp(selector, "length") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_STRING))
			return (void*)(uintptr_t)strlen(((struct shim_objc_string*)receiver)->utf8);
		if (shim_objc_is_object(receiver, SHIM_OBJC_ARRAY))
			return (void*)(uintptr_t)((struct shim_objc_array*)receiver)->count;
		const char* guest_text = shim_objc_string_utf8(receiver);
		if (guest_text)
			return (void*)(uintptr_t)strlen(guest_text);
	} else if (strcmp(selector, "count") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_ARRAY))
			return (void*)(uintptr_t)((struct shim_objc_array*)receiver)->count;
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSMUTABLEDICTIONARY))
			return (void*)(uintptr_t)(
				((struct shim_objc_dictionary*)receiver)->pair_count);
	} else if (strcmp(selector, "countByEnumeratingWithState:objects:count:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_ARRAY)) {
			struct shim_objc_array* array = receiver;
			struct {
				unsigned long state;
				void** items_ptr;
				unsigned long* mutations_ptr;
				unsigned long extra[5];
			}* enumeration_state = (void*)a2;
			void** buffer = (void**)a3;
			unsigned long buffer_capacity = (unsigned long)a4;
			unsigned long copied = 0;

			if (!enumeration_state || !buffer || buffer_capacity == 0)
				return NULL;
			if (array->count == 0)
				return NULL;

			if (enumeration_state->state == 0) {
				enumeration_state->state = 1;
				enumeration_state->extra[0] = 0;
				enumeration_state->mutations_ptr = &enumeration_state->extra[0];
				for (uint32_t index = 0; index < array->count && index < buffer_capacity; index++)
					buffer[index] = array->elements[index];
				copied = array->count < buffer_capacity ? array->count : buffer_capacity;
				enumeration_state->items_ptr = buffer;
			} else {
				enumeration_state->state = 0;
				copied = 0;
			}
			return (void*)(uintptr_t)copied;
		}
	} else if (strcmp(selector, "objectAtIndex:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_ARRAY)) {
			const struct shim_objc_array* array = receiver;
			if (a2 < array->count)
				return array->elements[a2];
		}
	} else if (strcmp(selector, "firstObject") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_ARRAY)) {
			const struct shim_objc_array* array = receiver;
			if (array->count > 0)
				return array->elements[0];
		}
	} else if (strcmp(selector, "lastObject") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_ARRAY)) {
			const struct shim_objc_array* array = receiver;
			if (array->count > 0)
				return array->elements[array->count - 1];
		}
	} else if (strcmp(selector, "cookies") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSHTTPCOOKIESTORAGE)) {
			pthread_mutex_lock(&shim_cookie_jar_mutex);
			void* result = shim_cookie_jar_all_array();
			pthread_mutex_unlock(&shim_cookie_jar_mutex);
			return result;
		}
	} else if (strcmp(selector, "cookiesForURL:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSHTTPCOOKIESTORAGE)) {
			const char* url_text = shim_objc_url_string((void*)a2);
			pthread_mutex_lock(&shim_cookie_jar_mutex);
			void* result = shim_cookie_jar_matching_array(url_text);
			pthread_mutex_unlock(&shim_cookie_jar_mutex);
			return result;
		}
	} else if (strcmp(selector, "setCookies:forURL:mainDocumentURL:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSHTTPCOOKIESTORAGE)) {
			const struct shim_objc_array* array = (const struct shim_objc_array*)a2;
			const char* url_text = shim_objc_url_string((void*)a3);
			if (array && shim_objc_is_object(array, SHIM_OBJC_ARRAY)) {
				pthread_mutex_lock(&shim_cookie_jar_mutex);
				for (uint32_t index = 0; index < array->count; index++) {
					const struct shim_objc_cookie* cookie =
						(const struct shim_objc_cookie*)array->elements[index];
					if (!cookie ||
					    !shim_objc_is_object(cookie, SHIM_OBJC_INSTANCE_NSHTTPCOOKIE))
						continue;
					struct shim_cookie stored = cookie->cookie;
					if (stored.domain[0] == '\0' && url_text) {
						char host[256];
						shim_parse_url_host(url_text, host, sizeof(host));
						shim_copy_cstring(stored.domain, sizeof(stored.domain), host);
					}
					shim_cookie_jar_store(&stored);
				}
				pthread_mutex_unlock(&shim_cookie_jar_mutex);
				shim_cookie_storage_notify();
			}
			return NULL;
		}
	} else if (strcmp(selector, "deleteCookie:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSHTTPCOOKIESTORAGE)) {
			const struct shim_objc_cookie* cookie =
				(const struct shim_objc_cookie*)a2;
			if (cookie && shim_objc_is_object(cookie, SHIM_OBJC_INSTANCE_NSHTTPCOOKIE)) {
				pthread_mutex_lock(&shim_cookie_jar_mutex);
				shim_cookie_jar_remove(&cookie->cookie);
				pthread_mutex_unlock(&shim_cookie_jar_mutex);
				shim_cookie_storage_notify();
			}
			return NULL;
		}
	} else if (strcmp(selector, "cookiesWithResponseHeaderFields:forURL:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSHTTPCOOKIE))
			return shim_parse_response_header_cookies((void*)a2, (void*)a3);
	} else if (strcmp(selector, "requestHeaderFieldsWithCookies:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSHTTPCOOKIE))
			return shim_cookie_request_header_fields((void*)a2);
	} else if (strcmp(selector, "name") == 0) {
		const struct shim_objc_cookie* cookie = receiver;
		if (shim_objc_is_object(cookie, SHIM_OBJC_INSTANCE_NSHTTPCOOKIE))
			return shim_objc_make_string(cookie->cookie.name);
	} else if (strcmp(selector, "value") == 0) {
		const struct shim_objc_cookie* cookie = receiver;
		if (shim_objc_is_object(cookie, SHIM_OBJC_INSTANCE_NSHTTPCOOKIE))
			return shim_objc_make_string(cookie->cookie.value);
	} else if (strcmp(selector, "fileURLWithPath:") == 0 ||
	           strcmp(selector, "fileURLWithPath:isDirectory:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSURL)) {
			const char* url_text = shim_objc_string_utf8((void*)a2);
			if (!url_text)
				return NULL;
			return shim_objc_make_url(url_text);
		}
	} else if (strcmp(selector, "fileURLWithPath:relativeToURL:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSURL)) {
			const char* relative = shim_objc_string_utf8((void*)a2);
			const char* base_text = shim_objc_url_string((void*)a3);
			char joined[PATH_MAX * 2];
			if (!relative || !base_text)
				return NULL;
			if (snprintf(joined, sizeof(joined), "%s/%s", base_text,
			             relative) >= (int)sizeof(joined))
				return NULL;
			return shim_objc_make_url(joined);
		}
	} else if (strcmp(selector, "stringByExpandingTildeInPath") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_STRING)) {
			const char* text = ((struct shim_objc_string*)receiver)->utf8;
			char expanded[PATH_MAX];
			if (text[0] == '~') {
				const char* home = getenv("HOME");
				const char* rest = text[1] == '/' ? text + 2 : text + 1;
				if (snprintf(expanded, sizeof(expanded), "%s/%s",
				             home ? home : "", rest) >= (int)sizeof(expanded))
					return NULL;
				return shim_objc_make_string(expanded);
			}
			return shim_objc_make_string(text);
		}
	} else if (strcmp(selector, "stringByAppendingPathComponent:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_STRING)) {
			const char* base_text = ((struct shim_objc_string*)receiver)->utf8;
			const char* component = shim_objc_string_utf8((void*)a2);
			char joined[PATH_MAX * 2];
			if (!component)
				return NULL;
			if (snprintf(joined, sizeof(joined), "%s%s%s", base_text,
			             base_text[0] && base_text[strlen(base_text) - 1] == '/' ? "" : "/",
			             component) >= (int)sizeof(joined))
				return NULL;
			return shim_objc_make_string(joined);
		}
	} else if (strcmp(selector, "defaultManager") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSFILEMANAGER))
			return &shim_filemanager_singleton;
	} else if (strcmp(selector, "createDirectoryAtURL:withIntermediateDirectories:attributes:error:") == 0 ||
	           strcmp(selector, "createDirectoryAtPath:withIntermediateDirectories:attributes:error:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSFILEMANAGER)) {
			const char* path_text = shim_objc_url_string((void*)a2);
			if (!path_text)
				path_text = shim_objc_string_utf8((void*)a2);
			if (!path_text)
				return NULL;
			char mutable_path[PATH_MAX];
			snprintf(mutable_path, sizeof(mutable_path), "%s", path_text);
			for (char* cursor = mutable_path + 1; *cursor; cursor++) {
				if (*cursor != '/')
					continue;
				*cursor = '\0';
				mkdir(mutable_path, 0755);
				*cursor = '/';
			}
			if (mkdir(mutable_path, 0755) != 0 && errno != EEXIST) {
				if (a5)
					*(void**)a5 = NULL;
				return NULL;
			}
			if (a5)
				*(void**)a5 = NULL;
			return (void*)(uintptr_t)1;
		}
	} else if (strcmp(selector, "createFileAtPath:contents:attributes:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSFILEMANAGER)) {
			const char* path_text = shim_objc_string_utf8((void*)a2);
			const struct shim_objc_data* data =
				(const struct shim_objc_data*)a3;
			if (!path_text)
				return NULL;
			FILE* output = fopen(path_text, "wb");
			if (!output)
				return NULL;
			if (data && data->length)
				fwrite(data->bytes, 1, data->length, output);
			fclose(output);
			return (void*)(uintptr_t)1;
		}
	} else if (strcmp(selector, "fileExistsAtPath:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSFILEMANAGER)) {
			const char* path_text = shim_objc_string_utf8((void*)a2);
			if (!path_text)
				return NULL;
			return (void*)(uintptr_t)(access(path_text, F_OK) == 0);
		}
	} else if (strcmp(selector, "removeItemAtPath:error:") == 0 ||
	           strcmp(selector, "removeItemAtURL:error:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSFILEMANAGER)) {
			const char* path_text = shim_objc_string_utf8((void*)a2);
			if (!path_text)
				path_text = shim_objc_url_string((void*)a2);
			if (!path_text)
				return NULL;
			if (a3)
				*(void**)a3 = NULL;
			return (void*)(uintptr_t)(unlink(path_text) == 0);
		}
	} else if (strcmp(selector, "fileHandleForWritingAtPath:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSFILEHANDLE)) {
			const char* path_text = shim_objc_string_utf8((void*)a2);
			struct shim_objc_filehandle* handle;
			if (!path_text)
				return NULL;
			int fd = archive_host_open(path_text, O_WRONLY | O_CREAT | O_APPEND, 0644);
			if (fd < 0)
				return NULL;
			handle = malloc(sizeof(*handle));
			if (!handle) {
				close(fd);
				return NULL;
			}
			handle->header.magic = SHIM_OBJC_MAGIC;
			handle->header.kind = SHIM_OBJC_INSTANCE_NSFILEHANDLE;
			handle->fd = fd;
			return handle;
		}
	} else if (strcmp(selector, "writeData:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSFILEHANDLE)) {
			const struct shim_objc_filehandle* handle = receiver;
			const struct shim_objc_data* data =
				(const struct shim_objc_data*)a2;
			if (data && data->length) {
				ssize_t result = write(handle->fd, data->bytes, data->length);
				(void)result;
			}
			return NULL;
		}
	} else if (strcmp(selector, "seekToEndOfFile") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSFILEHANDLE)) {
			const struct shim_objc_filehandle* handle = receiver;
			lseek(handle->fd, 0, SEEK_END);
			return NULL;
		}
	} else if (strcmp(selector, "closeFile") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSFILEHANDLE)) {
			struct shim_objc_filehandle* handle = receiver;
			close(handle->fd);
			handle->fd = -1;
			return NULL;
		}
	} else if (strcmp(selector, "dataUsingEncoding:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_STRING)) {
			const char* text = ((struct shim_objc_string*)receiver)->utf8;
			size_t text_length = strlen(text);
			struct shim_objc_data* data =
				malloc(sizeof(*data) + text_length);
			if (!data)
				return NULL;
			data->header.magic = SHIM_OBJC_MAGIC;
			data->header.kind = SHIM_OBJC_INSTANCE_NSDATA;
			data->length = text_length;
			memcpy(data->bytes, text, text_length);
			return data;
		}
	} else if (strcmp(selector, "stringWithContentsOfFile:") == 0 ||
	           strcmp(selector, "stringWithContentsOfFile:encoding:error:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSSTRING)) {
			const char* path_text = shim_objc_string_utf8((void*)a2);
			void** error_out =
				strcmp(selector, "stringWithContentsOfFile:encoding:error:") == 0
					? (void**)a4 : NULL;
			FILE* input;
			long file_size;
			char* contents;
			struct shim_objc_string* result;
			if (!path_text)
				return NULL;
			input = fopen(path_text, "rb");
			if (!input) {
				int result = errno;
				long long error_code = result == ENOENT ? 260 :
					result == EACCES ? 257 : 256;
				if (error_out)
					*error_out = shim_objc_make_error(
						"NSCocoaErrorDomain", error_code, strerror(result), NULL);
				return NULL;
			}
			if (error_out)
				*error_out = NULL;
			fseek(input, 0, SEEK_END);
			file_size = ftell(input);
			fseek(input, 0, SEEK_SET);
			contents = malloc((size_t)file_size + 1);
			if (!contents) {
				fclose(input);
				return NULL;
			}
			size_t read_length = fread(contents, 1, (size_t)file_size, input);
			fclose(input);
			contents[read_length] = '\0';
			result = shim_objc_make_string(contents);
			free(contents);
			return result;
		}
	} else if (strcmp(selector, "newlineCharacterSet") == 0 ||
	           strcmp(selector, "whitespaceCharacterSet") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSCHARACTERSET)) {
			if (strcmp(selector, "newlineCharacterSet") == 0)
				return (void*)(uintptr_t)'\n';
			return (void*)(uintptr_t)' ';
		}
	} else if (strcmp(selector, "componentsSeparatedByCharactersInSet:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_STRING)) {
			const char* text = ((struct shim_objc_string*)receiver)->utf8;
			char separator = (char)(uintptr_t)a2;
			struct shim_objc_array* array;
			char** parts = NULL;
			uint32_t component_count = 0;
			char* copy;
			char* scan;
			char* component_start;
			if (!text)
				return NULL;
			copy = strdup(text);
			if (!copy)
				return NULL;
			component_start = copy;
			for (scan = copy; ; scan++) {
				if (*scan == separator || *scan == '\0') {
					char terminator = *scan;
					parts = realloc(parts,
					                (component_count + 1) * sizeof(*parts));
					parts[component_count++] = component_start;
					*scan = '\0';
					if (terminator == '\0')
						break;
					component_start = scan + 1;
				}
			}
			array = shim_objc_make_array(NULL, component_count);
			if (!array) {
				free(parts);
				free(copy);
				return NULL;
			}
			array->header.magic = SHIM_OBJC_MAGIC;
			array->header.kind = SHIM_OBJC_ARRAY;
			array->count = component_count;
			array->capacity = component_count < 64 ? 64 : component_count;
			for (uint32_t index = 0; index < component_count; index++) {
				struct shim_objc_string* element =
					shim_objc_make_string(parts[index]);
				((struct shim_objc_string**)array->elements)[index] = element;
			}
			free(parts);
			free(copy);
			return array;
		}
	} else if (strcmp(selector, "componentsJoinedByString:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_ARRAY)) {
			const struct shim_objc_array* array = receiver;
			const char* joiner = shim_objc_string_utf8((void*)a2);
			size_t total_length = 1;
			char* joined;
			size_t position = 0;
			if (!joiner)
				return NULL;
			for (uint32_t index = 0; index < array->count; index++) {
				const char* element = shim_objc_string_utf8(array->elements[index]);
				if (element)
					total_length += strlen(element);
				if (index + 1 < array->count)
					total_length += strlen(joiner);
			}
			joined = malloc(total_length);
			if (!joined)
				return NULL;
			for (uint32_t index = 0; index < array->count; index++) {
				const char* element = shim_objc_string_utf8(array->elements[index]);
				if (element)
					strcpy(joined + position, element);
				position += element ? strlen(element) : 0;
				if (index + 1 < array->count) {
					strcpy(joined + position, joiner);
					position += strlen(joiner);
				}
			}
			joined[position] = '\0';
			struct shim_objc_string* result = shim_objc_make_string(joined);
			free(joined);
			return result;
		}
	} else if (strcmp(selector, "subarrayWithRange:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_ARRAY)) {
			const struct shim_objc_array* array = receiver;
			uint32_t start = (uint32_t)a2;
			uint32_t length = (uint32_t)a3;
			if (start + length > array->count)
				return NULL;
			struct shim_objc_array* result =
				shim_objc_make_array(NULL, length);
			if (!result)
				return NULL;
			result->header.magic = SHIM_OBJC_MAGIC;
			result->header.kind = SHIM_OBJC_ARRAY;
			result->count = length;
			for (uint32_t index = 0; index < length; index++)
				((struct shim_objc_string**)result->elements)[index] =
					array->elements[start + index];
			return result;
		}
	} else if (strcmp(selector, "localizedDescription") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSERROR))
			return shim_objc_make_string(
				((struct shim_objc_error*)receiver)->description);
		return shim_objc_make_string("operation failed");
	} else if (strcmp(selector, "domain") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSERROR))
			return shim_objc_make_string(
				((struct shim_objc_error*)receiver)->domain);
	} else if (strcmp(selector, "code") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSERROR))
			return (void*)(intptr_t)(
				((struct shim_objc_error*)receiver)->code);
	} else if (strcmp(selector, "userInfo") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSERROR)) {
			struct shim_objc_error* error = receiver;
			if (error->user_info)
				return error->user_info;
			return shim_objc_make_dictionary(NULL, NULL, 0);
		}
	} else if (strcmp(selector, "errorWithDomain:code:userInfo:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSERROR)) {
			const char* domain = shim_objc_string_utf8((void*)a2);
			const struct shim_objc_dictionary* user_info =
				(const void*)a4;
			const char* description = NULL;
			if (user_info &&
			    shim_objc_is_object(user_info,
			                        SHIM_OBJC_INSTANCE_NSMUTABLEDICTIONARY)) {
				for (uint32_t pair_index = 0;
				     pair_index < user_info->pair_count;
				     pair_index++) {
					const char* key = shim_objc_string_utf8(
						user_info->keys[pair_index * 2]);
					if (key &&
					    strcmp(key, "NSLocalizedDescription") == 0) {
						description = shim_objc_string_utf8(
							user_info->keys[pair_index * 2 + 1]);
						break;
					}
				}
			}
			return shim_objc_make_error(domain ? domain : "",
			                            (long long)(intptr_t)a3,
			                            description, (void*)a4);
		}
	} else if (strcmp(selector, "lock") == 0) {
		struct shim_objc_condition* condition = receiver;
		if (shim_objc_is_object(condition, SHIM_OBJC_INSTANCE_NSCONDITION) &&
		    shim_objc_condition_init(condition))
			pthread_mutex_lock(&condition->mutex);
		return NULL;
	} else if (strcmp(selector, "unlock") == 0) {
		struct shim_objc_condition* condition = receiver;
		if (shim_objc_is_object(condition, SHIM_OBJC_INSTANCE_NSCONDITION) &&
		    shim_objc_condition_init(condition))
			pthread_mutex_unlock(&condition->mutex);
		return NULL;
	} else if (strcmp(selector, "signal") == 0) {
		struct shim_objc_condition* condition = receiver;
		if (shim_objc_is_object(condition, SHIM_OBJC_INSTANCE_NSCONDITION) &&
		    shim_objc_condition_init(condition))
			pthread_cond_signal(&condition->cond);
		return NULL;
	} else if (strcmp(selector, "broadcast") == 0) {
		struct shim_objc_condition* condition = receiver;
		if (shim_objc_is_object(condition, SHIM_OBJC_INSTANCE_NSCONDITION) &&
		    shim_objc_condition_init(condition))
			pthread_cond_broadcast(&condition->cond);
		return NULL;
	} else if (strcmp(selector, "wait") == 0) {
		struct shim_objc_condition* condition = receiver;
		if (shim_objc_is_object(condition, SHIM_OBJC_INSTANCE_NSCONDITION) &&
		    shim_objc_condition_init(condition)) {
			pthread_mutex_lock(&condition->mutex);
			pthread_cond_wait(&condition->cond, &condition->mutex);
			pthread_mutex_unlock(&condition->mutex);
		}
		return NULL;
	} else if (strcmp(selector, "waitUntilDate:") == 0) {
		struct shim_objc_condition* condition = receiver;
		const struct shim_objc_date* date = (const void*)a2;
		if (shim_objc_is_object(condition, SHIM_OBJC_INSTANCE_NSCONDITION) &&
		    shim_objc_condition_init(condition)) {
			pthread_mutex_lock(&condition->mutex);
			if (shim_objc_is_object(date, SHIM_OBJC_INSTANCE_NSDATE)) {
				struct timespec now;
				clock_gettime(CLOCK_REALTIME, &now);
				if (date->deadline.tv_sec > now.tv_sec ||
				    (date->deadline.tv_sec == now.tv_sec &&
				     date->deadline.tv_nsec > now.tv_nsec))
					pthread_cond_timedwait(&condition->cond,
					                       &condition->mutex,
					                       &date->deadline);
			} else {
				pthread_cond_wait(&condition->cond, &condition->mutex);
			}
			pthread_mutex_unlock(&condition->mutex);
		}
		return NULL;
	} else if (strcmp(selector, "reason") == 0) {
		return shim_objc_make_string("exception");
	} else if (strcmp(selector, "objectAtIndexedSubscript:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_ARRAY)) {
			const struct shim_objc_array* array = receiver;
			uintptr_t index = a2;
			if (index < array->count)
				return array->elements[index];
			return NULL;
		}
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSMUTABLEDICTIONARY)) {
			const struct shim_objc_dictionary* dict = receiver;
			const char* wanted_key = shim_objc_string_utf8((void*)a2);
			if (!wanted_key)
				return NULL;
			for (uint32_t pair_index = 0; pair_index < dict->pair_count; pair_index++) {
				const char* key = shim_objc_string_utf8(dict->keys[pair_index * 2]);
				if (key && strcmp(key, wanted_key) == 0)
					return dict->keys[pair_index * 2 + 1];
			}
			return NULL;
		}
	} else if (strcmp(selector, "intValue") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSNUMBER))
			return (void*)(intptr_t)(
				((struct shim_objc_number*)receiver)->value.integer_value);
		const char* text = shim_objc_string_utf8(receiver);
		if (text)
			return (void*)(uintptr_t)(int)strtol(text, NULL, 10);
		return NULL;
	} else if (strcmp(selector, "integerValue") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSNUMBER))
			return (void*)(intptr_t)(
				((struct shim_objc_number*)receiver)->value.integer_value);
		const char* text = shim_objc_string_utf8(receiver);
		if (text)
			return (void*)(uintptr_t)strtoll(text, NULL, 10);
		return NULL;
	} else if (strcmp(selector, "URLWithString:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSURL)) {
			const char* url_text = shim_objc_string_utf8((void*)a2);
			if (!url_text)
				return NULL;
			return shim_objc_make_url(url_text);
		}
	} else if (strcmp(selector, "dictionary") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSMUTABLEDICTIONARY)) {
			struct shim_objc_string** no_keys = NULL;
			struct shim_objc_string** no_values = NULL;
			return shim_objc_make_dictionary(no_keys, no_values, 0);
		}
	} else if (strcmp(selector, "setObject:forKey:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSUSERDEFAULTS)) {
			pthread_mutex_lock(&shim_userdefaults_mutex);
			shim_userdefaults_set(receiver, (struct shim_objc_string*)a3,
			                      (struct shim_objc_string*)a2);
			pthread_mutex_unlock(&shim_userdefaults_mutex);
			return NULL;
		}
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSMUTABLEDICTIONARY)) {
			struct shim_objc_dictionary* dict = receiver;
			uint32_t pair_index;
			for (pair_index = 0; pair_index < dict->pair_count; pair_index++) {
				const char* key = shim_objc_string_utf8(dict->keys[pair_index * 2]);
				const char* new_key = shim_objc_string_utf8((void*)a3);
				if (key && new_key && strcmp(key, new_key) == 0) {
					dict->keys[pair_index * 2 + 1] = (struct shim_objc_string*)a2;
					return NULL;
				}
			}
			if (dict->pair_count < dict->pair_capacity) {
				dict->keys[dict->pair_count * 2] = (struct shim_objc_string*)a3;
				dict->keys[dict->pair_count * 2 + 1] = (struct shim_objc_string*)a2;
				dict->pair_count++;
			}
			return NULL;
		}
	} else if (strcmp(selector, "objectForKeyedSubscript:") == 0 ||
	           strcmp(selector, "objectForKey:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSUSERDEFAULTS)) {
			pthread_mutex_lock(&shim_userdefaults_mutex);
			void* stored = shim_userdefaults_get(
				receiver, (struct shim_objc_string*)a2);
			pthread_mutex_unlock(&shim_userdefaults_mutex);
			return stored;
		}
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSMUTABLEDICTIONARY)) {
			const struct shim_objc_dictionary* dict = receiver;
			const char* wanted_key = shim_objc_string_utf8((void*)a2);
			if (!wanted_key)
				return NULL;
			for (uint32_t pair_index = 0; pair_index < dict->pair_count; pair_index++) {
				const char* key = shim_objc_string_utf8(dict->keys[pair_index * 2]);
				if (key && strcmp(key, wanted_key) == 0)
					return dict->keys[pair_index * 2 + 1];
			}
			return NULL;
		}
	} else if (strcmp(selector, "URLByAppendingPathComponent:isDirectory:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSURL)) {
			char* text = foundation_url_append_path(shim_objc_url_string(receiver),
				shim_objc_string_utf8((void*)a2), (unsigned char)a3 != 0);
			void* result = text ? shim_objc_make_url(text) : NULL;
			free(text);
			return result;
		}
	} else if (strcmp(selector, "path") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSURLCOMPONENTS)) {
			struct shim_objc_url_components* components = receiver;
			if (!components->path)
				return NULL;
			return shim_objc_make_string(components->path->utf8);
		}
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSURL)) {
			char* path = foundation_url_copy_path(shim_objc_url_string(receiver));
			void* result = path ? shim_objc_make_string(path) : NULL;
			free(path);
			return result;
		}
	} else if (strcmp(selector, "dateWithTimeIntervalSinceNow:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSDATE)) {
			struct shim_objc_date* result = malloc(sizeof(*result));
			if (!result)
				return NULL;
			result->header.magic = SHIM_OBJC_MAGIC;
			result->header.kind = SHIM_OBJC_INSTANCE_NSDATE;
			clock_gettime(CLOCK_REALTIME, &result->deadline);
			double interval = shim_objc_msgsend_floating0;
			time_t whole_seconds = (time_t)interval;
			double fraction = interval - (double)whole_seconds;
			result->deadline.tv_sec += whole_seconds;
			result->deadline.tv_nsec += (long)(fraction * 1e9);
			if (result->deadline.tv_nsec >= 1000000000L) {
				result->deadline.tv_sec++;
				result->deadline.tv_nsec -= 1000000000L;
			}
			return result;
		}
	} else if (strcmp(selector, "componentsWithURL:resolvingAgainstBaseURL:") == 0) {
		const char* url_text = shim_objc_url_string((void*)a2);
		if (!url_text)
			return NULL;
		struct shim_objc_url_components* components =
			shim_objc_make_url_components();
		if (!components)
			return NULL;
		const char* scheme_end = strstr(url_text, "://");
		size_t scheme_length = scheme_end ?
			(size_t)(scheme_end - url_text) : 0;
		const char* host_start = scheme_end ? scheme_end + 3 : url_text;
		const char* path_start = strchr(host_start, '/');
		size_t host_length = path_start ?
			(size_t)(path_start - host_start) : strlen(host_start);
		const char* path_text = path_start ? path_start : "";
		char buffer[2048];
		if (scheme_length >= sizeof(buffer) || host_length >= sizeof(buffer))
			return components;
		memcpy(buffer, url_text, scheme_length);
		buffer[scheme_length] = '\0';
		components->scheme = shim_objc_make_string(buffer);
		memcpy(buffer, host_start, host_length);
		buffer[host_length] = '\0';
		components->host = shim_objc_make_string(buffer);
		components->path = shim_objc_make_string(path_text);
		return components;
	} else if (strcmp(selector, "URLPathAllowedCharacterSet") == 0) {
		return &shim_characterset_singleton;
	} else if (strcmp(selector, "stringByTrimmingCharactersInSet:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_STRING)) {
			const char* text = ((struct shim_objc_string*)receiver)->utf8;
			size_t start = 0;
			size_t end = strlen(text);
			while (end > start &&
			       shim_is_url_path_allowed_character((unsigned char)text[end - 1]))
				end--;
			while (start < end &&
			       shim_is_url_path_allowed_character((unsigned char)text[start]))
				start++;
			size_t length = end - start;
			struct shim_objc_string* result =
				malloc(sizeof(*result) + length + 1);
			if (!result)
				return NULL;
			result->header.magic = SHIM_OBJC_MAGIC;
			result->header.kind = SHIM_OBJC_STRING;
			memcpy(result->utf8, text + start, length);
			result->utf8[length] = '\0';
			return result;
		}
	} else if (strcmp(selector, "setScheme:") == 0 ||
	           strcmp(selector, "setHost:") == 0 ||
	           strcmp(selector, "setPath:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSURLCOMPONENTS)) {
			struct shim_objc_url_components* components = receiver;
			struct shim_objc_string** slot = NULL;
			if (selector[3] == 'S')
				slot = &components->scheme;
			else if (selector[3] == 'H')
				slot = &components->host;
			else
				slot = &components->path;
			const char* text = shim_objc_string_utf8((void*)a2);
			if (!text) {
				*slot = NULL;
				return NULL;
			}
			struct shim_objc_string* value = shim_objc_make_string(text);
			*slot = value;
			return NULL;
		}
	} else if (strcmp(selector, "URL") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSURLCOMPONENTS)) {
			struct shim_objc_url_components* components = receiver;
			const char* scheme = components->scheme ?
				components->scheme->utf8 : "";
			const char* host = components->host ?
				components->host->utf8 : "";
			const char* path = components->path ?
				components->path->utf8 : "";
			if (!scheme[0] && !host[0] && !path[0])
				return NULL;
			size_t capacity = strlen(scheme) + strlen(host) + strlen(path) + 16;
			char* joined = malloc(capacity);
			if (!joined)
				return NULL;
			snprintf(joined, capacity, "%s://%s%s", scheme, host, path);
			void* result = shim_objc_make_url(joined);
			free(joined);
			return result;
		}
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSURLCOMPONENTS))
			return NULL;
	} else if (strcmp(selector, "absoluteString") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSURL)) {
			const char* text = shim_objc_url_string(receiver);
			if (!text)
				return NULL;
			return shim_objc_make_string(text);
		}
	} else if (strcmp(selector, "sharedCookieStorageForGroupContainerIdentifier:") == 0 ||
	           strcmp(selector, "sharedHTTPCookieStorage") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSHTTPCOOKIESTORAGE))
			return &shim_cookie_storage_singleton;
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSHTTPCOOKIESTORAGE))
			return &shim_cookie_storage_singleton;
	} else if (strcmp(selector, "cookieAcceptPolicy") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSHTTPCOOKIESTORAGE))
			return NULL;
	} else if (strcmp(selector, "dictionaryWithObjectsAndKeys:") == 0) {
		uintptr_t values[32];
		uint32_t value_count = 0;
		values[value_count++] = a2;
		for (uint32_t stack_index = 0; stack_index < 16; stack_index++) {
			if (!guest_stack_args)
				break;
			uintptr_t next_value = guest_stack_args[stack_index];
			if (next_value == 0)
				break;
			if (value_count >= 32)
				break;
			values[value_count++] = next_value;
		}
		uint32_t pair_count = value_count / 2;
		if (pair_count == 0)
			return NULL;
		struct shim_objc_string** keys = malloc(sizeof(*keys) * pair_count);
		struct shim_objc_string** pair_values = malloc(sizeof(*pair_values) * pair_count);
		if (!keys || !pair_values) {
			free(keys);
			free(pair_values);
			return NULL;
		}
		for (uint32_t pair_index = 0; pair_index < pair_count; pair_index++) {
			keys[pair_index] = (struct shim_objc_string*)values[pair_index * 2 + 1];
			pair_values[pair_index] = (struct shim_objc_string*)values[pair_index * 2];
		}
		void* result = shim_objc_make_dictionary(keys, pair_values, pair_count);
		free(keys);
		free(pair_values);
		return result;
	} else if (strcmp(selector, "cookieWithProperties:") == 0) {
		const struct shim_objc_dictionary* properties = (const void*)a2;
		struct shim_cookie cookie;
		const char* origin_url = NULL;
		memset(&cookie, 0, sizeof(cookie));
		if (properties &&
		    shim_objc_is_object(properties, SHIM_OBJC_INSTANCE_NSMUTABLEDICTIONARY)) {
			for (uint32_t pair_index = 0;
			     pair_index < properties->pair_count; pair_index++) {
				const char* key =
					shim_objc_string_utf8(properties->keys[pair_index * 2]);
				void* value_object = properties->keys[pair_index * 2 + 1];
				const char* value =
					shim_objc_string_utf8(value_object);
				if (!key)
					continue;
				if (strcmp(key, "OriginURL") == 0) {
					origin_url = shim_objc_url_string(value_object);
					if (!origin_url)
						origin_url = value;
				} else if (!value) {
					continue;
				} else if (strcmp(key, "Name") == 0) {
					shim_copy_cstring(cookie.name, sizeof(cookie.name), value);
				} else if (strcmp(key, "Value") == 0) {
					shim_copy_cstring(cookie.value, sizeof(cookie.value), value);
				} else if (strcmp(key, "Path") == 0) {
					shim_copy_cstring(cookie.path, sizeof(cookie.path), value);
				} else if (strcmp(key, "Domain") == 0) {
					shim_copy_cstring(cookie.domain, sizeof(cookie.domain), value);
				}
			}
		}
		if (cookie.domain[0] == '\0' && origin_url)
			shim_parse_url_host(origin_url, cookie.domain, sizeof(cookie.domain));
		return shim_objc_make_cookie(&cookie);
	} else if (strcmp(selector, "setCookie:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSHTTPCOOKIESTORAGE)) {
			const struct shim_objc_cookie* cookie = (const struct shim_objc_cookie*)a2;
			if (cookie &&
			    shim_objc_is_object(cookie, SHIM_OBJC_INSTANCE_NSHTTPCOOKIE)) {
				pthread_mutex_lock(&shim_cookie_jar_mutex);
				shim_cookie_jar_store(&cookie->cookie);
				pthread_mutex_unlock(&shim_cookie_jar_mutex);
				shim_cookie_storage_notify();
			}
			return NULL;
		}
	} else if (strcmp(selector, "alloc") == 0 ||
	           strcmp(selector, "allocWithZone:") == 0 ||
	           strcmp(selector, "new") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSURLCOMPONENTS))
			return shim_objc_make_url_components();
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSMUTABLEDICTIONARY))
			return shim_objc_make_dictionary(NULL, NULL, 0);
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSCONDITION)) {
			struct shim_objc_condition* condition =
				calloc(1, sizeof(*condition));
			if (condition) {
				condition->header.magic = SHIM_OBJC_MAGIC;
				condition->header.kind = SHIM_OBJC_INSTANCE_NSCONDITION;
			}
			return condition;
		}
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSERROR)) {
			struct shim_objc_error* error = calloc(1, sizeof(*error));
			if (error) {
				error->header.magic = SHIM_OBJC_MAGIC;
				error->header.kind = SHIM_OBJC_INSTANCE_NSERROR;
			}
			return error;
		}
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSUSERDEFAULTS)) {
			struct shim_objc_userdefaults* defaults = malloc(sizeof(*defaults));
			if (!defaults)
				return NULL;
			defaults->header.magic = SHIM_OBJC_MAGIC;
			defaults->header.kind = SHIM_OBJC_INSTANCE_NSUSERDEFAULTS;
			defaults->values = shim_objc_make_dictionary(NULL, NULL, 0);
			if (!defaults->values) {
				free(defaults);
				return NULL;
			}
			return defaults;
		}
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSARRAY) ||
		    shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSMUTABLEARRAY))
			return shim_objc_make_array(NULL, 0);
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSDICTIONARY))
			return shim_objc_make_dictionary(NULL, NULL, 0);
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSSTRING) ||
		    shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSMUTABLESTRING))
			return shim_objc_make_string("");
		return shim_objc_make_string("");
	} else if (strcmp(selector, "init") == 0) {
		return receiver;
	} else if (strcmp(selector, "standardUserDefaults") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSUSERDEFAULTS)) {
			pthread_mutex_lock(&shim_userdefaults_mutex);
			void* result = shim_userdefaults_for_suite("");
			pthread_mutex_unlock(&shim_userdefaults_mutex);
			return result;
		}
	} else if (strcmp(selector, "initWithSuiteName:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSUSERDEFAULTS)) {
			const char* suite_name = shim_objc_string_utf8((void*)a2);
			struct shim_objc_userdefaults* allocated = receiver;
			pthread_mutex_lock(&shim_userdefaults_mutex);
			struct shim_objc_userdefaults* shared =
				shim_userdefaults_for_suite(suite_name ? suite_name : "");
			if (!shared) {
				pthread_mutex_unlock(&shim_userdefaults_mutex);
				return NULL;
			}
			free(allocated->values);
			free(allocated);
			pthread_mutex_unlock(&shim_userdefaults_mutex);
			return shared;
		}
	} else if (strcmp(selector, "setObject:forKey:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSUSERDEFAULTS)) {
			pthread_mutex_lock(&shim_userdefaults_mutex);
			shim_userdefaults_set(receiver, (struct shim_objc_string*)a3,
			                      (struct shim_objc_string*)a2);
			pthread_mutex_unlock(&shim_userdefaults_mutex);
			return NULL;
		}
	} else if (strcmp(selector, "stringForKey:") == 0 ||
	           strcmp(selector, "objectForKey:") == 0 ||
	           strcmp(selector, "stringArrayForKey:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSUSERDEFAULTS)) {
			pthread_mutex_lock(&shim_userdefaults_mutex);
			void* result = shim_userdefaults_get(
				receiver, (struct shim_objc_string*)a2);
			pthread_mutex_unlock(&shim_userdefaults_mutex);
			return result;
		}
	} else if (strcmp(selector, "removeObjectForKey:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSUSERDEFAULTS)) {
			pthread_mutex_lock(&shim_userdefaults_mutex);
			shim_userdefaults_remove(receiver, (struct shim_objc_string*)a2);
			pthread_mutex_unlock(&shim_userdefaults_mutex);
			return NULL;
		}
	} else if (strcmp(selector, "initWithBytes:length:encoding:") == 0) {
		const char* bytes = (const char*)a2;
		size_t byte_length = (size_t)a3;
		struct shim_objc_string* result;
		if (!bytes || byte_length > (1 << 20))
			return NULL;
		result = malloc(sizeof(*result) + byte_length + 1);
		if (!result)
			return NULL;
		result->header.magic = SHIM_OBJC_MAGIC;
		result->header.kind = SHIM_OBJC_STRING;
		memcpy(result->utf8, bytes, byte_length);
		result->utf8[byte_length] = '\0';
		return result;
	} else if (strcmp(selector, "initWithUTF8String:") == 0) {
		if (!a2)
			return NULL;
		return shim_objc_make_string((const char*)a2);
	} else if (strcmp(selector, "stringWithUTF8String:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSSTRING)) {
			if (!a2)
				return NULL;
			return shim_objc_make_string((const char*)a2);
		}
	} else if (strcmp(selector, "hasPrefix:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_STRING)) {
			const char* prefix = shim_objc_string_utf8((void*)a2);
			const char* text = ((struct shim_objc_string*)receiver)->utf8;
			if (!prefix)
				return NULL;
			return (void*)(uintptr_t)(strncmp(text, prefix, strlen(prefix)) == 0);
		}
	} else if (strcmp(selector, "substringFromIndex:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_STRING)) {
			const char* text = ((struct shim_objc_string*)receiver)->utf8;
			size_t length = strlen(text);
			if (a2 > length)
				return NULL;
			return shim_objc_make_string(text + a2);
		}
	} else if (strcmp(selector, "substringToIndex:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_STRING)) {
			const char* text = ((struct shim_objc_string*)receiver)->utf8;
			size_t length = strlen(text);
			if (a2 > length)
				a2 = length;
			char* copy = strndup(text, a2);
			if (!copy)
				return NULL;
			void* result = shim_objc_make_string(copy);
			free(copy);
			return result;
		}
	} else if (strcmp(selector, "isEqualToString:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_STRING)) {
			const char* other = shim_objc_string_utf8((void*)a2);
			const char* text = ((struct shim_objc_string*)receiver)->utf8;
			return (void*)(uintptr_t)(other && strcmp(text, other) == 0);
		}
	} else if (strcmp(selector, "numberWithBool:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSNUMBER))
			return shim_objc_make_number(a2 ? 1 : 0);
	} else if (strcmp(selector, "numberWithInteger:") == 0 ||
	           strcmp(selector, "numberWithInt:") == 0 ||
	           strcmp(selector, "numberWithLong:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSNUMBER))
			return shim_objc_make_number((long long)(intptr_t)a2);
	} else if (strcmp(selector, "numberWithDouble:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSNUMBER)) {
			struct shim_objc_number* result = malloc(sizeof(*result));
			if (!result)
				return NULL;
			result->header.magic = SHIM_OBJC_MAGIC;
			result->header.kind = SHIM_OBJC_INSTANCE_NSNUMBER;
			memcpy(&result->value, &shim_objc_msgsend_floating0, sizeof(double));
			result->is_floating = 1;
			return result;
		}
	} else if (strcmp(selector, "boolValue") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSNUMBER))
			return (void*)(uintptr_t)(
				((struct shim_objc_number*)receiver)->value.integer_value != 0);
	} else if (strcmp(selector, "integerValue") == 0 ||
	           strcmp(selector, "intValue") == 0 ||
	           strcmp(selector, "longLongValue") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSNUMBER))
			return (void*)(intptr_t)(
				((struct shim_objc_number*)receiver)->value.integer_value);
	} else if (strcmp(selector, "arrayWithObjects:count:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSARRAY)) {
			void** elements = (void**)a2;
			uintptr_t count = a3;
			struct shim_objc_array* result =
				shim_objc_make_array(NULL, 0);
			if (!result)
				return NULL;
			for (uintptr_t index = 0; index < count; index++)
				result = shim_objc_array_append(result, elements[index]);
			return result;
		}
	} else if (strcmp(selector, "array") == 0 ||
	           strcmp(selector, "arrayWithCapacity:") == 0 ||
	           strcmp(selector, "arrayWithArray:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSARRAY) ||
		    shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSMUTABLEARRAY)) {
			if (strcmp(selector, "arrayWithArray:") == 0 &&
			    shim_objc_is_object((void*)a2, SHIM_OBJC_ARRAY)) {
				const struct shim_objc_array* source_array = (const void*)a2;
				return shim_objc_make_array(
					source_array->elements,
					source_array->count);
			}
			struct shim_objc_array* result =
				shim_objc_make_array(NULL, 0);
			if (result)
				result->header.kind = SHIM_OBJC_ARRAY;
			return result;
		}
	} else if (strcmp(selector, "dictionaryWithObjects:forKeys:count:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSMUTABLEDICTIONARY) ||
		    shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSDICTIONARY)) {
			void** objects = (void**)a2;
			void** keys = (void**)a3;
			uintptr_t count = a4;
			struct shim_objc_dictionary* result =
				shim_objc_make_dictionary(NULL, NULL, 0);
			if (!result)
				return NULL;
			for (uintptr_t index = 0; index < count; index++)
				result = shim_objc_dictionary_set(result, keys[index],
				                                   objects[index]);
			return result;
		}
	} else if (strcmp(selector, "dictionary") == 0 ||
	           strcmp(selector, "dictionaryWithCapacity:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSMUTABLEDICTIONARY) ||
		    shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSDICTIONARY))
			return shim_objc_make_dictionary(NULL, NULL, 0);
	} else if (strcmp(selector, "addObject:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_ARRAY)) {
			shim_objc_array_append(receiver, (void*)a2);
			return NULL;
		}
	} else if (strcmp(selector, "setObject:forKeyedSubscript:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSMUTABLEDICTIONARY)) {
			shim_objc_dictionary_set(receiver, (void*)a3, (void*)a2);
			return NULL;
		}
	} else if (strcmp(selector, "dictionaryWithObject:forKey:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSDICTIONARY) ||
		    shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSMUTABLEDICTIONARY)) {
			void* keys[1] = {(void*)a3};
			void* values[1] = {(void*)a2};
			return shim_objc_make_dictionary((struct shim_objc_string**)keys,
			                                  (struct shim_objc_string**)values, 1);
		}
	} else if (strcmp(selector, "objectForKey:") == 0 ||
	           strcmp(selector, "objectForKeyedSubscript:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSMUTABLEDICTIONARY))
			return shim_objc_dictionary_get(receiver, (void*)a2);
	} else if (strcmp(selector, "objectAtIndexedSubscript:") == 0 ||
	           strcmp(selector, "objectAtIndex:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_ARRAY)) {
			const struct shim_objc_array* array = receiver;
			if (a2 < array->count)
				return array->elements[a2];
		}
	} else if (strcmp(selector, "dataWithJSONObject:options:error:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSJSONSERIALIZATION)) {
			if (a4)
				*(void**)a4 = NULL;
			char* json = shim_json_serialize_object((void*)a2, 0);
			if (!json)
				return NULL;
			struct shim_objc_data* data =
				malloc(sizeof(*data) + strlen(json) + 1);
			if (!data) {
				free(json);
				return NULL;
			}
			data->header.magic = SHIM_OBJC_MAGIC;
			data->header.kind = SHIM_OBJC_INSTANCE_NSDATA;
			data->length = strlen(json);
			memcpy(data->bytes, json, data->length + 1);
			free(json);
			return data;
		}
	} else if (strcmp(selector, "JSONObjectWithData:options:error:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSJSONSERIALIZATION)) {
			if (a4)
				*(void**)a4 = NULL;
			if (!shim_objc_is_object((void*)a2, SHIM_OBJC_INSTANCE_NSDATA))
				return NULL;
			const struct shim_objc_data* data = (const void*)a2;
			const char* json = (const char*)data->bytes;
			void* result = shim_json_parse_value(&json, 0);
			if (!result)
				return shim_objc_make_dictionary(NULL, NULL, 0);
			return result;
		}
	} else if (strcmp(selector, "initWithData:encoding:") == 0) {
		if (shim_objc_is_object((void*)a2, SHIM_OBJC_INSTANCE_NSDATA)) {
			const struct shim_objc_data* data = (const void*)a2;
			struct shim_objc_string* result =
				malloc(sizeof(*result) + data->length + 1);
			if (!result)
				return NULL;
			result->header.magic = SHIM_OBJC_MAGIC;
			result->header.kind = SHIM_OBJC_STRING;
			memcpy(result->utf8, data->bytes, data->length);
			result->utf8[data->length] = '\0';
			return result;
		}
	} else if (strcmp(selector, "copy") == 0 ||
	           strcmp(selector, "mutableCopy") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_ARRAY)) {
			const struct shim_objc_array* array = receiver;
			return shim_objc_make_array((struct shim_objc_string**)array->elements,
			                            array->count);
		}
		if (shim_objc_is_object(receiver, SHIM_OBJC_STRING))
			return shim_objc_make_string(
				((struct shim_objc_string*)receiver)->utf8);
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSMUTABLEDICTIONARY)) {
			const struct shim_objc_dictionary* source = receiver;
			struct shim_objc_dictionary* copy =
				shim_objc_make_dictionary(NULL, NULL, 0);
			if (!copy)
				return NULL;
			for (uint32_t pair_index = 0; pair_index < source->pair_count;
			     pair_index++)
				copy = shim_objc_dictionary_set(copy,
					source->keys[pair_index * 2],
					source->keys[pair_index * 2 + 1]);
			return copy;
		}
	} else if (strcmp(selector, "class") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSDICTIONARY) ||
		    shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSMUTABLEDICTIONARY))
			return &OBJC_CLASS_$_NSDictionary;
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSARRAY) ||
		    shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSMUTABLEARRAY))
			return &OBJC_CLASS_$_NSArray;
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSSTRING) ||
		    shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSMUTABLESTRING))
			return &OBJC_CLASS_$_NSString;
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSDATA))
			return &OBJC_CLASS_$_NSData;
		return receiver;
	} else if (strcmp(selector, "characterAtIndex:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_STRING)) {
			const char* text = ((struct shim_objc_string*)receiver)->utf8;
			if (a2 < strlen(text))
				return (void*)(uintptr_t)(unsigned char)text[a2];
		}
	}

	if (shim_trace_enabled())
		fprintf(stderr, "libsystem_shim: objc_msgSend unhandled selector '%s' receiver=%p\n",
		        selector, receiver);
	return NULL;
}

void* foundation_archive_get_class(const char* name)
{
	if (!name)
		return NULL;
	if (strcmp(name, "NSProcessInfo") == 0)
		return &OBJC_CLASS_$_NSProcessInfo;
	if (strcmp(name, "NSUserDefaults") == 0)
		return &OBJC_CLASS_$_NSUserDefaults;
	if (strcmp(name, "NSString") == 0)
		return &OBJC_CLASS_$_NSString;
	if (strcmp(name, "NSBundle") == 0)
		return &OBJC_CLASS_$_NSBundle;
	if (strcmp(name, "NSHTTPCookie") == 0)
		return &OBJC_CLASS_$_NSHTTPCookie;
	if (strcmp(name, "NSHTTPCookieStorage") == 0)
		return &OBJC_CLASS_$_NSHTTPCookieStorage;
	if (strcmp(name, "NSURL") == 0)
		return &OBJC_CLASS_$_NSURL;
	if (strcmp(name, "NSMutableDictionary") == 0 ||
	    strcmp(name, "NSDictionary") == 0)
		return &OBJC_CLASS_$_NSMutableDictionary;
	if (strcmp(name, "NSCharacterSet") == 0)
		return &OBJC_CLASS_$_NSCharacterSet;
	if (strcmp(name, "NSURLComponents") == 0)
		return &OBJC_CLASS_$_NSURLComponents;
	if (strcmp(name, "NSDate") == 0)
		return &OBJC_CLASS_$_NSDate;
	if (strcmp(name, "NSFileManager") == 0)
		return &OBJC_CLASS_$_NSFileManager;
	if (strcmp(name, "NSFileHandle") == 0)
		return &OBJC_CLASS_$_NSFileHandle;
	if (strcmp(name, "NSData") == 0 ||
	    strcmp(name, "NSMutableData") == 0)
		return &OBJC_CLASS_$_NSData;
	if (strcmp(name, "NSNumber") == 0)
		return &OBJC_CLASS_$_NSNumber;
	if (strcmp(name, "NSArray") == 0 ||
	    strcmp(name, "NSMutableArray") == 0)
		return &OBJC_CLASS_$_NSArray;
	if (strcmp(name, "NSDictionary") == 0 ||
	    strcmp(name, "NSMutableDictionary") == 0)
		return &OBJC_CLASS_$_NSMutableDictionary;
	if (strcmp(name, "NSJSONSerialization") == 0)
		return &OBJC_CLASS_$_NSJSONSerialization;
	if (strcmp(name, "NSCondition") == 0)
		return &OBJC_CLASS_$_NSCondition;
	if (strcmp(name, "NSError") == 0)
		return &OBJC_CLASS_$_NSError;
	return foundation_archive_guest_class(name);
}

void* foundation_archive_alloc(void* class_object)
{
	if (shim_objc_is_object(class_object, SHIM_OBJC_CLASS_NSURLCOMPONENTS))
		return shim_objc_make_url_components();
	if (shim_objc_is_object(class_object, SHIM_OBJC_CLASS_NSMUTABLEDICTIONARY))
		return shim_objc_make_dictionary(NULL, NULL, 0);
	if (shim_objc_is_object(class_object, SHIM_OBJC_CLASS_NSCONDITION)) {
		struct shim_objc_condition* condition = calloc(1, sizeof(*condition));
		if (condition) {
			condition->header.magic = SHIM_OBJC_MAGIC;
			condition->header.kind = SHIM_OBJC_INSTANCE_NSCONDITION;
		}
		return condition;
	}
	if (shim_objc_is_object(class_object, SHIM_OBJC_CLASS_NSERROR)) {
		struct shim_objc_error* error = calloc(1, sizeof(*error));
		if (error) {
			error->header.magic = SHIM_OBJC_MAGIC;
			error->header.kind = SHIM_OBJC_INSTANCE_NSERROR;
		}
		return error;
	}
	if (shim_objc_is_object(class_object, SHIM_OBJC_CLASS_NSUSERDEFAULTS)) {
		struct shim_objc_userdefaults* defaults = malloc(sizeof(*defaults));
		if (!defaults)
			return NULL;
		defaults->header.magic = SHIM_OBJC_MAGIC;
		defaults->header.kind = SHIM_OBJC_INSTANCE_NSUSERDEFAULTS;
		defaults->values = shim_objc_make_dictionary(NULL, NULL, 0);
		if (!defaults->values) {
			free(defaults);
			return NULL;
		}
		return defaults;
	}
	if (shim_objc_is_object(class_object, SHIM_OBJC_CLASS_NSARRAY) ||
	    shim_objc_is_object(class_object, SHIM_OBJC_CLASS_NSMUTABLEARRAY))
		return shim_objc_make_array(NULL, 0);
	if (shim_objc_is_object(class_object, SHIM_OBJC_CLASS_NSDICTIONARY))
		return shim_objc_make_dictionary(NULL, NULL, 0);
	if (shim_objc_is_object(class_object, SHIM_OBJC_CLASS_GENERIC) ||
	    shim_objc_is_object(class_object, SHIM_OBJC_INSTANCE_GENERIC))
		return shim_objc_make_generic();
    return foundation_archive_guest_alloc(class_object);
}

void* foundation_archive_data_create(const void* bytes, size_t size)
{
    if ((!bytes && size) || size > SIZE_MAX - sizeof(struct shim_objc_data) - 1)
        return NULL;
    struct shim_objc_data* result = malloc(sizeof(*result) + size + 1);
    if (!result) return NULL;
    result->header = (struct shim_objc_header){SHIM_OBJC_MAGIC, SHIM_OBJC_INSTANCE_NSDATA};
    result->length = size;
    if (size) memcpy(result->bytes, bytes, size);
    result->bytes[size] = 0;
    return result;
}

int foundation_archive_data_resolve(const void* object, const void** bytes, size_t* size)
{
    if (!shim_objc_is_object(object, SHIM_OBJC_INSTANCE_NSDATA)) return 0;
    const struct shim_objc_data* data = object;
    if (bytes) *bytes = data->bytes;
    if (size) *size = data->length;
    return 1;
}

static int dispatch_data(void* receiver, const char* selector,
    const uintptr_t arguments[6], uintptr_t* result)
{
    if (receiver == &OBJC_CLASS_$_NSData) {
        if (strcmp(selector, "dataWithBytes:length:") == 0)
            *result = (uintptr_t)foundation_archive_data_create((void*)arguments[0], arguments[1]);
        else if (strcmp(selector, "alloc") == 0 || strcmp(selector, "data") == 0)
            *result = (uintptr_t)foundation_archive_data_create(NULL, 0);
        else return 0;
        return 1;
    }
    const void* bytes;
    size_t size;
    if (!foundation_archive_data_resolve(receiver, &bytes, &size)) return 0;
    if (strcmp(selector, "bytes") == 0) *result = (uintptr_t)bytes;
    else if (strcmp(selector, "length") == 0) *result = size;
    else if (strcmp(selector, "initWithBytes:length:") == 0)
        *result = (uintptr_t)foundation_archive_data_create((void*)arguments[0], arguments[1]);
    else return 0;
    return 1;
}

static int dispatch_dictionary(void* receiver, const char* selector,
    const uintptr_t arguments[6], uintptr_t* result)
{
    if (!shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSMUTABLEDICTIONARY)) return 0;
    struct shim_objc_dictionary* dictionary = receiver;
    if (strcmp(selector, "objectForKey:") == 0 || strcmp(selector, "objectForKeyedSubscript:") == 0)
        *result = (uintptr_t)shim_objc_dictionary_get(dictionary, (void*)arguments[0]);
    else if (strcmp(selector, "setObject:forKey:") == 0 || strcmp(selector, "setObject:forKeyedSubscript:") == 0) {
        shim_objc_dictionary_set(dictionary, (void*)arguments[1], (void*)arguments[0]);
        *result = 0;
    } else if (strcmp(selector, "removeObjectForKey:") == 0) {
        shim_objc_dictionary_remove(dictionary, (void*)arguments[0]);
        *result = 0;
    } else if (strcmp(selector, "count") == 0) {
        *result = dictionary->pair_count;
    } else return 0;
    return 1;
}

static int dispatch_keyed_archiver(void* receiver, const char* selector,
    const uintptr_t arguments[6], uintptr_t* result);

int foundation_archive_dispatch(void* receiver, const char* selector,
    const uintptr_t arguments[6], const uintptr_t* stack, void* indirect_result,
    double floating_argument, uintptr_t* result, double* floating_result)
{
    if (!receiver || !selector || (uintptr_t)receiver < 4096)
        return 0;
    struct shim_objc_header* header = receiver;
    *floating_result = 0;
    if (dispatch_data(receiver, selector, arguments, result) ||
        dispatch_dictionary(receiver, selector, arguments, result)) return 1;
    if (header->magic != SHIM_OBJC_MAGIC && receiver != &OBJC_CLASS_$_NSString &&
        receiver != &OBJC_CLASS_$_NSProcessInfo) {
        struct foundation_string* string = foundation_string_resolve(receiver);
        if (!string) return foundation_archive_guest_dispatch(receiver, selector, result);
        struct shim_objc_string* temporary = shim_objc_make_string(string->utf8);
        if (!temporary) { *result = 0; return 1; }
        void* answer = archive_message_impl(temporary, (void*)selector,
            arguments[0], arguments[1], arguments[2], arguments[3], arguments[4], arguments[5],
            indirect_result, stack);
        if (answer == temporary->utf8) answer = string->utf8;
        *result = (uintptr_t)(answer == temporary ? receiver : answer);
        free(temporary);
        return 1;
    }
    shim_objc_msgsend_floating0 = floating_argument;
    if (strcmp(selector, "doubleValue") == 0 &&
        shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSNUMBER)) {
        const struct shim_objc_number* number = receiver;
        *floating_result = number->is_floating ? number->value.double_value
            : (double)number->value.integer_value;
        *result = 0;
        return 1;
    }
    if (dispatch_keyed_archiver(receiver, selector, arguments, result))
        return 1;
    if (foundation_archive_category_dispatch(receiver, selector, arguments,
            stack, indirect_result, result))
        return 1;
    *result = (uintptr_t)archive_message_impl(receiver, (void*)selector,
        arguments[0], arguments[1], arguments[2], arguments[3], arguments[4], arguments[5],
        indirect_result, stack);
    return 1;
}

static const char* shim_class_name_for_kind(uint32_t kind)
{
    switch (kind) {
    case SHIM_OBJC_CLASS_NSDICTIONARY:
    case SHIM_OBJC_CLASS_NSMUTABLEDICTIONARY:
    case SHIM_OBJC_INSTANCE_NSMUTABLEDICTIONARY:
        return "NSDictionary";
    case SHIM_OBJC_CLASS_NSARRAY:
    case SHIM_OBJC_CLASS_NSMUTABLEARRAY:
    case SHIM_OBJC_ARRAY:
    case SHIM_OBJC_INSTANCE_NSMUTABLEARRAY:
        return "NSArray";
    case SHIM_OBJC_CLASS_NSSTRING:
    case SHIM_OBJC_CLASS_NSMUTABLESTRING:
    case SHIM_OBJC_STRING:
        return "NSString";
    case SHIM_OBJC_CLASS_NSDATA:
    case SHIM_OBJC_INSTANCE_NSDATA:
        return "NSData";
    case SHIM_OBJC_CLASS_NSNUMBER:
    case SHIM_OBJC_INSTANCE_NSNUMBER:
        return "NSNumber";
    case SHIM_OBJC_CLASS_NSDATE:
    case SHIM_OBJC_INSTANCE_NSDATE:
        return "NSDate";
    case SHIM_OBJC_CLASS_NSURL:
    case SHIM_OBJC_INSTANCE_NSURL:
        return "NSURL";
    case SHIM_OBJC_CLASS_NSERROR:
    case SHIM_OBJC_INSTANCE_NSERROR:
        return "NSError";
    case SHIM_OBJC_CLASS_NSUSERDEFAULTS:
    case SHIM_OBJC_INSTANCE_NSUSERDEFAULTS:
        return "NSUserDefaults";
    case SHIM_OBJC_CLASS_NSHTTPCOOKIE:
    case SHIM_OBJC_INSTANCE_NSHTTPCOOKIE:
        return "NSHTTPCookie";
    case SHIM_OBJC_CLASS_NSHTTPCOOKIESTORAGE:
    case SHIM_OBJC_INSTANCE_NSHTTPCOOKIESTORAGE:
        return "NSHTTPCookieStorage";
    default:
        return NULL;
    }
}

static const char* shim_receiver_class_name(const void* receiver)
{
    const struct shim_objc_header* header = receiver;
    if (!header || (uintptr_t)header < 4096)
        return NULL;
    if (header->magic != SHIM_OBJC_MAGIC)
        return NULL;
    return shim_class_name_for_kind(header->kind);
}

const char* foundation_archive_shim_class_name(const void* class_object)
{
    if (!foundation_archive_readable(class_object,
            sizeof(struct shim_objc_header)))
        return NULL;
    const struct shim_objc_header* header = class_object;
    if (header->magic != SHIM_OBJC_MAGIC)
        return NULL;
    return shim_class_name_for_kind(header->kind);
}

static int dispatch_keyed_archiver(void* receiver, const char* selector,
    const uintptr_t arguments[6], uintptr_t* result)
{
    int archive = shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSKEYEDARCHIVER) &&
        strcmp(selector, "archivedDataWithRootObject:requiringSecureCoding:error:") == 0;
    int unarchive = shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSKEYEDUNARCHIVER) &&
        strcmp(selector, "unarchivedObjectOfClass:fromData:error:") == 0;
    if (!archive && !unarchive)
        return 0;
    *result = 0;
    if (archive)
        *result = (uintptr_t)shim_keyed_archive_encode((void*)arguments[0]);
    else {
        const void* bytes = NULL;
        size_t size = 0;
        if (foundation_archive_data_resolve((void*)arguments[1], &bytes, &size))
            *result = (uintptr_t)shim_keyed_archive_decode(bytes, size);
    }
    return 1;
}

int foundation_archive_category_dispatch(void* receiver, const char* selector,
    const uintptr_t arguments[6], const uintptr_t* stack, void* indirect_result,
    uintptr_t* result)
{
    const char* class_name = shim_receiver_class_name(receiver);
    if (!class_name)
        return 0;
    const struct shim_objc_header* header = receiver;
    int class_method = 0;
    switch (header->kind) {
    case SHIM_OBJC_CLASS_NSDICTIONARY:
    case SHIM_OBJC_CLASS_NSMUTABLEDICTIONARY:
    case SHIM_OBJC_CLASS_NSARRAY:
    case SHIM_OBJC_CLASS_NSMUTABLEARRAY:
    case SHIM_OBJC_CLASS_NSSTRING:
    case SHIM_OBJC_CLASS_NSMUTABLESTRING:
    case SHIM_OBJC_CLASS_NSDATA:
    case SHIM_OBJC_CLASS_NSNUMBER:
    case SHIM_OBJC_CLASS_NSDATE:
    case SHIM_OBJC_CLASS_NSURL:
    case SHIM_OBJC_CLASS_NSERROR:
    case SHIM_OBJC_CLASS_NSUSERDEFAULTS:
    case SHIM_OBJC_CLASS_NSHTTPCOOKIE:
    case SHIM_OBJC_CLASS_NSHTTPCOOKIESTORAGE:
        class_method = 1;
        break;
    default:
        break;
    }
    void* implementation = foundation_archive_category_imp_for_class(class_name,
        class_method, selector);
    if (!implementation)
        return 0;
    extern void* shim_objc_msgSend_call_guest_imp(void*, void*, void*, void*,
        const uintptr_t*, const uintptr_t*);
    uintptr_t registers[6];
    if (arguments)
        memcpy(registers, arguments, sizeof(registers));
    else
        memset(registers, 0, sizeof(registers));
    *result = (uintptr_t)shim_objc_msgSend_call_guest_imp(implementation,
        receiver, (void*)selector, indirect_result, registers, stack);
    return 1;
}

static const char* search_directory_suffix(unsigned directory)
{
    switch (directory) {
    case 5: return "/Library";
    case 9: return "/Documents";
    case 12: return "/Desktop";
    case 13: return "/Library/Caches";
    case 14: return "/Library/Application Support";
    case 15: return "/Downloads";
    case 17: return "/Movies";
    case 18: return "/Music";
    case 19: return "/Pictures";
    default: return NULL;
    }
}

unsigned NSStartSearchPathEnumeration(unsigned directory, unsigned domains)
{
    return (domains & 1) && search_directory_suffix(directory) ? directory : 0;
}

unsigned NSGetNextSearchPathEnumeration(unsigned state, char* path)
{
    if (!state || (state & 0x80000000u) || !path) return 0;
    const char* suffix = search_directory_suffix(state);
    if (!suffix) return 0;
    const char* home = getenv("HOME");
    snprintf(path, 1024, "%.980s%s", home ? home : "/tmp", suffix);
    return state | 0x80000000u;
}

void* NSSearchPathForDirectoriesInDomains(unsigned directory, unsigned domains, unsigned expand_tilde)
{
    const char* suffix = search_directory_suffix(directory);
    if (!(domains & 1) || !suffix) return shim_objc_make_array(NULL, 0);
    const char* home = expand_tilde ? getenv("HOME") : "~";
    if (!home) home = "/tmp";
    size_t size = strlen(home) + strlen(suffix) + 1;
    char* path = malloc(size);
    if (!path) return NULL;
    snprintf(path, size, "%s%s", home, suffix);
    void* result = shim_objc_make_single_element_array(path);
    free(path);
    return result;
}
