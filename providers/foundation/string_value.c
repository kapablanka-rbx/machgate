#include "string_value.h"
#include "archive/runtime.h"
#include "ports/native_blocks.h"

#include <iconv.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define FOUNDATION_MAX_STRING_BYTES (16u * 1024u * 1024u)

struct constant_string {
	const void* isa;
	uint32_t flags;
	uint32_t reserved;
	const void* bytes;
	uint64_t length;
};

struct constant_translation {
	const void* guest;
	struct foundation_string* string;
	struct constant_translation* next;
};

static pthread_mutex_t constants_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct constant_translation* constants;

struct foundation_encoded_string {
	uintptr_t encoding;
	char* bytes;
	size_t size;
	struct foundation_encoded_string* next;
};

const char* foundation_encoding_name(uintptr_t encoding)
{
	switch (encoding) {
	case 1: return "ASCII";
	case 4: return "UTF-8";
	case 5: return "ISO-8859-1";
	case 10: return "UTF-16LE";
	case 12: return "WINDOWS-1252";
	case 0x90000100: return "UTF-16BE";
	case 0x94000100: return "UTF-16LE";
	case 0x8c000100: return "UTF-32LE";
	case 0x98000100: return "UTF-32BE";
	case 0x9c000100: return "UTF-32LE";
	default: return NULL;
	}
}

static void* convert_string(const void* bytes, size_t size, const char* source,
	const char* destination, size_t* converted_size)
{
	if ((!bytes && size) || size > FOUNDATION_MAX_STRING_BYTES)
		return NULL;
	iconv_t converter = iconv_open(destination, source);
	if (converter == (iconv_t)-1)
		return NULL;
	size_t capacity = size * 4 + 4;
	char* result = calloc(capacity, 1);
	if (!result) {
		iconv_close(converter);
		return NULL;
	}
	char* input = (char*)bytes;
	char* output = result;
	size_t remaining = capacity - 4;
	if (size && iconv(converter, &input, &size, &output, &remaining) == (size_t)-1) {
		free(result);
		result = NULL;
	} else {
		*converted_size = (size_t)(output - result);
	}
	iconv_close(converter);
	return result;
}

const void* foundation_string_encoded_bytes(struct foundation_string* string,
	uintptr_t encoding, size_t* size)
{
	*size = 0;
	const char* name = foundation_encoding_name(encoding);
	if (!name)
		return NULL;
	if (encoding == 4) {
		*size = string->byte_count;
		return string->utf8;
	}
	pthread_mutex_lock(&constants_mutex);
	for (struct foundation_encoded_string* entry = string->encodings; entry; entry = entry->next) {
		if (entry->encoding == encoding) {
			*size = entry->size;
			pthread_mutex_unlock(&constants_mutex);
			return entry->bytes;
		}
	}
	char* result = convert_string(string->utf8, string->byte_count, "UTF-8", name, size);
	struct foundation_encoded_string* entry = result ? malloc(sizeof(*entry)) : NULL;
	if (!entry) {
		free(result);
		pthread_mutex_unlock(&constants_mutex);
		return NULL;
	}
	*entry = (struct foundation_encoded_string){encoding, result, *size, string->encodings};
	string->encodings = entry;
	pthread_mutex_unlock(&constants_mutex);
	return result;
}

const char* foundation_string_encoded(struct foundation_string* string, uintptr_t encoding)
{
	size_t size;
	return foundation_string_encoded_bytes(string, encoding, &size);
}

struct foundation_string* foundation_string_append(struct foundation_string* string,
	struct foundation_string* other)
{
	if (!other || other->byte_count > FOUNDATION_MAX_STRING_BYTES - string->byte_count)
		return NULL;
	size_t size = string->byte_count + other->byte_count;
	char* bytes = malloc(size ? size : 1);
	if (!bytes)
		return NULL;
	memcpy(bytes, string->utf8, string->byte_count);
	memcpy(bytes + string->byte_count, other->utf8, other->byte_count);
	struct foundation_string* result = foundation_string_create(bytes, size, "UTF-8");
	free(bytes);
	return result;
}

struct foundation_string* foundation_string_substring(struct foundation_string* string,
	size_t start, size_t count)
{
	if (start > string->character_count || count > string->character_count - start)
		return NULL;
	return foundation_string_create(string->characters + start, count * sizeof(uint16_t), "UTF-16LE");
}

static void release_string_contents(struct foundation_string* string)
{
	free(string->characters);
	free(string->utf8);
	while (string->encodings) {
		struct foundation_encoded_string* entry = string->encodings;
		string->encodings = entry->next;
		free(entry->bytes);
		free(entry);
	}
}

int foundation_string_append_in_place(struct foundation_string* string,
	struct foundation_string* other)
{
	if (string->isa != &OBJC_CLASS_$_NSMutableString)
		return 0;
	struct foundation_string* result = foundation_string_append(string, other);
	if (!result)
		return 0;
	release_string_contents(string);
	string->utf8 = result->utf8;
	string->byte_count = result->byte_count;
	string->characters = result->characters;
	string->character_count = result->character_count;
	free(result);
	return 1;
}

struct foundation_string* foundation_string_create(const void* bytes, size_t size,
	const char* encoding)
{
	struct foundation_string* result = calloc(1, sizeof(*result));
	if (!result)
		return NULL;
	result->isa = &OBJC_CLASS_$_NSString;
	atomic_init(&result->references, 1);
	result->utf8 = convert_string(bytes, size, encoding, "UTF-8", &result->byte_count);
	size_t character_bytes = 0;
	if (result->utf8)
		result->characters = convert_string(result->utf8, result->byte_count,
			"UTF-8", "UTF-16LE", &character_bytes);
	if (!result->utf8 || !result->characters) {
		free(result->utf8);
		free(result->characters);
		free(result);
		return NULL;
	}
	result->character_count = character_bytes / sizeof(uint16_t);
	return result;
}

static struct foundation_string* translated_constant(const struct constant_string* guest)
{
	if (guest->length > FOUNDATION_MAX_STRING_BYTES / 2 ||
	    (guest->flags != 0x7c8 && guest->flags != 0x7d0))
		return NULL;
	pthread_mutex_lock(&constants_mutex);
	for (struct constant_translation* entry = constants; entry; entry = entry->next) {
		if (entry->guest == guest) {
			pthread_mutex_unlock(&constants_mutex);
			return entry->string;
		}
	}
	int wide = guest->flags == 0x7d0;
	struct foundation_string* result = foundation_string_create(guest->bytes,
		guest->length * (wide ? 2 : 1), wide ? "UTF-16LE" : "UTF-8");
	struct constant_translation* entry = result ? malloc(sizeof(*entry)) : NULL;
	if (!entry) {
		foundation_release(result);
		pthread_mutex_unlock(&constants_mutex);
		return NULL;
	}
	atomic_store(&result->references, SIZE_MAX);
	*entry = (struct constant_translation){guest, result, constants};
	constants = entry;
	pthread_mutex_unlock(&constants_mutex);
	return result;
}

struct foundation_string* foundation_string_resolve(const void* object)
{
	if (!object || (uintptr_t)object < 4096 || (uintptr_t)object % sizeof(void*))
		return NULL;
	const void* isa = *(const void* const*)object;
	if (isa == &OBJC_CLASS_$_NSString || isa == &OBJC_CLASS_$_NSMutableString)
		return (struct foundation_string*)object;
	if (isa == foundation_shim_symbol("__CFConstantStringClassReference"))
		return translated_constant(object);
	const char* legacy = foundation_archive_string_utf8(object);
	if (legacy) {
		struct constant_string view = {NULL, 0x7c8, 0, legacy, strlen(legacy)};
		pthread_mutex_lock(&constants_mutex);
		for (struct constant_translation* entry = constants; entry; entry = entry->next) {
			if (entry->guest == object) {
				pthread_mutex_unlock(&constants_mutex);
				return entry->string;
			}
		}
		struct foundation_string* result = foundation_string_create(view.bytes, view.length, "UTF-8");
		struct constant_translation* entry = result ? malloc(sizeof(*entry)) : NULL;
		if (entry) {
			atomic_store(&result->references, SIZE_MAX);
			*entry = (struct constant_translation){object, result, constants};
			constants = entry;
		}
		pthread_mutex_unlock(&constants_mutex);
		if (!entry) {
			foundation_release(result);
			return NULL;
		}
		return result;
	}
	return NULL;
}

void* foundation_retain(void* object)
{
	if (machgate_is_block_v1(object))
		return _Block_copy(object);
	struct foundation_string* string = foundation_string_resolve(object);
	if (string) {
		size_t references = atomic_load(&string->references);
		while (references && references != SIZE_MAX) {
			if (atomic_compare_exchange_weak(&string->references, &references, references + 1))
				return object;
		}
		if (!references)
			return NULL;
	}
	return object;
}

void foundation_release(void* object)
{
	if (machgate_is_block_v1(object)) {
		_Block_release(object);
		return;
	}
	struct foundation_string* string = foundation_string_resolve(object);
	if (!string || atomic_load(&string->references) == SIZE_MAX ||
	    atomic_fetch_sub(&string->references, 1) != 1)
		return;
	foundation_archive_forget_object(object);
	release_string_contents(string);
	free(string);
}
