#ifndef MACHGATE_FOUNDATION_STRING_VALUE_H
#define MACHGATE_FOUNDATION_STRING_VALUE_H

#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>

struct foundation_class {
	uintptr_t isa;
};

struct foundation_string {
	const struct foundation_class* isa;
	_Atomic size_t references;
	char* utf8;
	size_t byte_count;
	uint16_t* characters;
	size_t character_count;
	struct foundation_encoded_string* encodings;
};

extern struct foundation_class OBJC_CLASS_$_NSString;
extern struct foundation_class OBJC_CLASS_$_NSMutableString;
void* foundation_shim_symbol(const char* name);
struct foundation_string* foundation_string_create(const void* bytes, size_t size,
	const char* encoding);
struct foundation_string* foundation_string_resolve(const void* object);
struct foundation_string* foundation_string_format(const void* format,
	const uintptr_t* arguments);
void* foundation_retain(void* object);
void foundation_release(void* object);
void* foundation_autorelease(void* object);
const char* foundation_encoding_name(uintptr_t encoding);
const char* foundation_string_encoded(struct foundation_string* string, uintptr_t encoding);
const void* foundation_string_encoded_bytes(struct foundation_string* string,
	uintptr_t encoding, size_t* size);
struct foundation_string* foundation_string_append(struct foundation_string* string,
	struct foundation_string* other);
int foundation_string_append_in_place(struct foundation_string* string,
	struct foundation_string* other);
struct foundation_string* foundation_string_substring(struct foundation_string* string,
	size_t start, size_t count);

#endif
