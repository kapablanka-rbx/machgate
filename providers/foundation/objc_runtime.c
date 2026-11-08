#include "string_value.h"
#include "call_frame.h"
#include "archive/runtime.h"
#include "ports/provider_lifetime.h"
#include "ports/native_blocks.h"

#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>

struct foundation_class OBJC_CLASS_$_NSProcessInfo;
struct foundation_class OBJC_CLASS_$_NSString;
struct foundation_class OBJC_CLASS_$_NSMutableString;

void* objc_msgSend(void* receiver, const char* selector, ...);

struct operating_system_version {
	int64_t major;
	int64_t minor;
	int64_t patch;
};

_Static_assert(sizeof(struct operating_system_version) == 24, "Darwin version ABI");
_Static_assert(offsetof(struct foundation_call, result) == 208, "entry result offset");
_Static_assert(offsetof(struct foundation_call, fallback) == 224, "entry fallback offset");

static struct foundation_class process_info = {(uintptr_t)&OBJC_CLASS_$_NSProcessInfo};
static const struct operating_system_version system_version = {15, 0, 0};
static void* shim_handle;

__attribute__((constructor))
static void connect_runtime(void)
{
	shim_handle = dlopen("libsystem_shim.so", RTLD_NOW | RTLD_NOLOAD);
	machgate_register_block_object_lifetime_v1(foundation_retain, foundation_release);
}

void* foundation_shim_symbol(const char* name)
{
	return shim_handle ? dlsym(shim_handle, name) : NULL;
}

void* objc_getClass(const char* name)
{
	if (!name)
		return NULL;
	if (strcmp(name, "NSProcessInfo") == 0)
		return &OBJC_CLASS_$_NSProcessInfo;
	if (strcmp(name, "NSString") == 0)
		return &OBJC_CLASS_$_NSString;
	if (strcmp(name, "NSMutableString") == 0)
		return &OBJC_CLASS_$_NSMutableString;
	void* result = foundation_archive_get_class(name);
	if (result)
		return result;
	void* (*fallback)(const char*) = foundation_shim_symbol("objc_getClass");
	return fallback ? fallback(name) : NULL;
}

void* objc_lookUpClass(const char* name)
{
	return objc_getClass(name);
}

void* objc_alloc(void* class_object)
{
	if (class_object == &OBJC_CLASS_$_NSString || class_object == &OBJC_CLASS_$_NSMutableString) {
		struct foundation_string* result = foundation_string_create("", 0, "UTF-8");
		if (result)
			result->isa = class_object;
		return result;
	}
	void* result = foundation_archive_alloc(class_object);
	if (result)
		return result;
	void* (*fallback)(void*) = foundation_shim_symbol("objc_alloc");
	return fallback ? fallback(class_object) : NULL;
}

void* objc_allocWithZone(void* class_object, void* zone)
{
	(void)zone;
	return objc_alloc(class_object);
}

void* objc_alloc_init(void* class_object)
{
	void* instance = objc_alloc(class_object);
	if (!instance)
		return NULL;
	return objc_msgSend(instance, "init");
}

void* objc_retain(void* object)
{
	return foundation_retain(object);
}

void objc_release(void* object)
{
	foundation_release(object);
}

void* foundation_autorelease(void* object)
{
	if (foundation_string_resolve(object))
		machgate_defer_release_v1(object, foundation_release);
	return object;
}

void* objc_autorelease(void* object)
{
	return foundation_autorelease(object);
}

void* objc_retainAutoreleasedReturnValue(void* object)
{
	return foundation_retain(object);
}

void* objc_retainAutorelease(void* object)
{
	return foundation_autorelease(foundation_retain(object));
}

void* objc_retainAutoreleaseReturnValue(void* object)
{
	return objc_retainAutorelease(object);
}

void* objc_autoreleaseReturnValue(void* object)
{
	return foundation_autorelease(object);
}

static int dispatch_string_factory(struct foundation_call* call, const char* selector)
{
	const void* bytes = (void*)call->arguments[2];
	struct foundation_string* result;
	if (strcmp(selector, "stringWithFormat:") == 0)
		result = foundation_string_format(bytes, call->stack);
	else if (strcmp(selector, "stringWithUTF8String:") == 0)
		result = bytes ? foundation_string_create(bytes, strlen(bytes), "UTF-8") : NULL;
	else if (strcmp(selector, "stringWithCharacters:length:") == 0)
		result = call->arguments[3] <= SIZE_MAX / 2
			? foundation_string_create(bytes, call->arguments[3] * 2, "UTF-16LE") : NULL;
	else if (strcmp(selector, "stringWithString:") == 0) {
		struct foundation_string* source = foundation_string_resolve(bytes);
		result = source ? foundation_string_create(source->utf8, source->byte_count, "UTF-8") : NULL;
	}
	else
		return 0;
	if (result && (void*)call->arguments[0] == &OBJC_CLASS_$_NSMutableString)
		result->isa = &OBJC_CLASS_$_NSMutableString;
	call->result = (uintptr_t)foundation_autorelease(result);
	return 1;
}

static int dispatch_string(struct foundation_call* call, const char* selector,
	struct foundation_string* string)
{
	void* receiver = (void*)call->arguments[0];
	if (strcmp(selector, "appendString:") == 0 && string->isa == &OBJC_CLASS_$_NSMutableString)
		foundation_string_append_in_place(string, foundation_string_resolve((void*)call->arguments[2]));
	else if (strcmp(selector, "appendFormat:") == 0 && string->isa == &OBJC_CLASS_$_NSMutableString) {
		struct foundation_string* suffix = foundation_string_format((void*)call->arguments[2], call->stack);
		foundation_string_append_in_place(string, suffix);
		foundation_release(suffix);
	}
	else if (strcmp(selector, "mutableCopy") == 0 || strcmp(selector, "mutableCopyWithZone:") == 0 ||
		((strcmp(selector, "copy") == 0 || strcmp(selector, "copyWithZone:") == 0) &&
		string->isa == &OBJC_CLASS_$_NSMutableString)) {
		struct foundation_string* result = foundation_string_create(string->utf8, string->byte_count, "UTF-8");
		if (result && strncmp(selector, "mutableCopy", 11) == 0)
			result->isa = &OBJC_CLASS_$_NSMutableString;
		call->result = (uintptr_t)result;
	}
	else if (strcmp(selector, "UTF8String") == 0)
		call->result = (uintptr_t)string->utf8;
	else if (strcmp(selector, "cStringUsingEncoding:") == 0)
		call->result = (uintptr_t)foundation_string_encoded(string, call->arguments[2]);
	else if (strcmp(selector, "dataUsingEncoding:") == 0) {
		size_t size;
		const void* bytes = foundation_string_encoded_bytes(string, call->arguments[2], &size);
		call->result = bytes ? (uintptr_t)foundation_archive_data_create(bytes, size) : 0;
	}
	else if (strcmp(selector, "stringByAppendingString:") == 0)
		call->result = (uintptr_t)foundation_autorelease(foundation_string_append(string,
			foundation_string_resolve((void*)call->arguments[2])));
	else if (strcmp(selector, "substringFromIndex:") == 0)
		call->result = (uintptr_t)foundation_autorelease(foundation_string_substring(string,
			call->arguments[2], string->character_count - call->arguments[2]));
	else if (strcmp(selector, "substringToIndex:") == 0)
		call->result = (uintptr_t)foundation_autorelease(foundation_string_substring(string,
			0, call->arguments[2]));
	else if (strcmp(selector, "isEqualToString:") == 0 || strcmp(selector, "hasPrefix:") == 0) {
		struct foundation_string* other = foundation_string_resolve((void*)call->arguments[2]);
		call->result = other && other->byte_count <= string->byte_count &&
			(strcmp(selector, "hasPrefix:") == 0 || other->byte_count == string->byte_count) &&
			memcmp(string->utf8, other->utf8, other->byte_count) == 0;
	}
	else if (strcmp(selector, "length") == 0)
		call->result = string->character_count;
	else if (strcmp(selector, "characterAtIndex:") == 0)
		call->result = call->arguments[2] < string->character_count
			? string->characters[call->arguments[2]] : 0;
	else if (strcmp(selector, "integerValue") == 0 || strcmp(selector, "longLongValue") == 0)
		call->result = (uintptr_t)strtoll(string->utf8, NULL, 10);
	else if (strcmp(selector, "intValue") == 0)
		call->result = (uintptr_t)(int)strtol(string->utf8, NULL, 10);
	else if (strcmp(selector, "doubleValue") == 0)
		call->floating = strtod(string->utf8, NULL);
	else if (strcmp(selector, "initWithUTF8String:") == 0) {
		const char* bytes = (void*)call->arguments[2];
		call->result = (uintptr_t)(bytes ? foundation_string_create(bytes, strlen(bytes), "UTF-8") : NULL);
		foundation_release(receiver);
	} else if (strcmp(selector, "initWithBytes:length:encoding:") == 0) {
		const char* encoding = foundation_encoding_name(call->arguments[4]);
		call->result = (uintptr_t)(encoding ? foundation_string_create((void*)call->arguments[2],
			call->arguments[3], encoding) : NULL);
		foundation_release(receiver);
	} else if (strcmp(selector, "initWithData:encoding:") == 0) {
		const void* bytes = NULL;
		size_t size = 0;
		const char* encoding = foundation_encoding_name(call->arguments[3]);
		call->result = (uintptr_t)(encoding && foundation_archive_data_resolve(
			(void*)call->arguments[2], &bytes, &size) ? foundation_string_create(bytes, size, encoding) : NULL);
		foundation_release(receiver);
	} else if (strcmp(selector, "retain") == 0 || strcmp(selector, "copy") == 0 ||
		strcmp(selector, "copyWithZone:") == 0)
		call->result = (uintptr_t)foundation_retain(receiver);
	else if (strcmp(selector, "autorelease") == 0)
		call->result = (uintptr_t)foundation_autorelease(receiver);
	else if (strcmp(selector, "release") == 0)
		foundation_release(receiver);
	else if (strcmp(selector, "init") == 0 || strcmp(selector, "description") == 0)
		call->result = (uintptr_t)receiver;
	else
		return 0;
	return 1;
}

static void dispatch_method(struct foundation_call* call, int lookup_receiver_methods)
{
	call->result = 0;
	call->floating = 0;
	call->fallback = NULL;
	void* receiver = (void*)call->arguments[0];
	const char* selector = (void*)call->arguments[1];
	if (!receiver || !selector)
		return;
	if (receiver == &OBJC_CLASS_$_NSProcessInfo && strcmp(selector, "processInfo") == 0) {
		call->result = (uintptr_t)&process_info;
		return;
	}
	if (receiver == &process_info && strcmp(selector, "operatingSystemVersion") == 0) {
		if (call->arguments[8])
			memcpy((void*)call->arguments[8], &system_version, sizeof(system_version));
		return;
	}
	if (receiver == &OBJC_CLASS_$_NSString || receiver == &OBJC_CLASS_$_NSMutableString) {
		if (strcmp(selector, "alloc") == 0 || strcmp(selector, "new") == 0) {
			call->result = (uintptr_t)objc_alloc(receiver);
			return;
		}
		if (dispatch_string_factory(call, selector))
			return;
	}
	struct foundation_string* string = foundation_string_resolve(receiver);
	if (string && dispatch_string(call, selector, string))
		return;
	if (lookup_receiver_methods) {
		call->fallback = foundation_archive_guest_imp(receiver, selector);
		if (call->fallback)
			return;
	}
	double floating_argument;
	memcpy(&floating_argument, &call->vectors[0], sizeof(floating_argument));
	if (foundation_archive_dispatch(receiver, selector, &call->arguments[2], call->stack,
		(void*)call->arguments[8], floating_argument, &call->result, &call->floating))
		return;
	call->fallback = foundation_shim_symbol("objc_msgSend");
}

void foundation_dispatch(struct foundation_call* call)
{
	dispatch_method(call, 1);
}

void foundation_dispatch_super(struct foundation_call* call)
{
	call->result = 0;
	call->floating = 0;
	void* receiver = NULL;
	call->fallback = foundation_archive_super_imp((void*)call->arguments[0],
		(void*)call->arguments[1], &receiver);
	call->arguments[0] = (uintptr_t)receiver;
	if (!call->fallback)
		dispatch_method(call, 0);
}
