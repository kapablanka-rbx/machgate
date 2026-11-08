#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum {
	MACHGATE_CTYPE_ALPHA = 0x00000100,
	MACHGATE_CTYPE_CONTROL = 0x00000200,
	MACHGATE_CTYPE_DIGIT = 0x00000400,
	MACHGATE_CTYPE_GRAPH = 0x00000800,
	MACHGATE_CTYPE_LOWER = 0x00001000,
	MACHGATE_CTYPE_PUNCT = 0x00002000,
	MACHGATE_CTYPE_SPACE = 0x00004000,
	MACHGATE_CTYPE_UPPER = 0x00008000,
	MACHGATE_CTYPE_HEX = 0x00010000,
	MACHGATE_CTYPE_BLANK = 0x00020000,
	MACHGATE_CTYPE_PRINT = 0x00040000,
};

struct machgate_rune_locale {
	unsigned char prefix[0x3c];
	uint32_t runetype[256];
	int32_t lowercase[256];
	int32_t uppercase[256];
};

struct machgate_rune_locale _DefaultRuneLocale;

static uint32_t ascii_rune_flags(unsigned int character)
{
	uint32_t flags = 0;
	if (character <= 0x1f || character == 0x7f)
		flags |= MACHGATE_CTYPE_CONTROL;
	if (character == ' ' || character == '\t')
		flags |= MACHGATE_CTYPE_BLANK;
	if (character == ' ' || (character >= '\t' && character <= '\r'))
		flags |= MACHGATE_CTYPE_SPACE;
	if (character >= '0' && character <= '9')
		flags |= MACHGATE_CTYPE_DIGIT;
	if (character >= 'A' && character <= 'Z')
		flags |= MACHGATE_CTYPE_ALPHA | MACHGATE_CTYPE_UPPER;
	if (character >= 'a' && character <= 'z')
		flags |= MACHGATE_CTYPE_ALPHA | MACHGATE_CTYPE_LOWER;
	if ((character >= '0' && character <= '9') ||
	    (character >= 'A' && character <= 'F') ||
	    (character >= 'a' && character <= 'f'))
		flags |= MACHGATE_CTYPE_HEX;
	if (character >= 0x21 && character <= 0x7e) {
		flags |= MACHGATE_CTYPE_GRAPH;
		if (!(flags & (MACHGATE_CTYPE_ALPHA | MACHGATE_CTYPE_DIGIT)))
			flags |= MACHGATE_CTYPE_PUNCT;
	}
	if (character >= 0x20 && character <= 0x7e)
		flags |= MACHGATE_CTYPE_PRINT;
	return flags;
}

__attribute__((constructor))
static void publish_default_rune_locale(void)
{
	memcpy(_DefaultRuneLocale.prefix, "RuneMagi", 8);
	for (unsigned int character = 0; character < 256; character++) {
		_DefaultRuneLocale.runetype[character] = ascii_rune_flags(character);
		_DefaultRuneLocale.lowercase[character] =
			character >= 'A' && character <= 'Z'
				? (int32_t)(character + ('a' - 'A'))
				: (int32_t)character;
		_DefaultRuneLocale.uppercase[character] =
			character >= 'a' && character <= 'z'
				? (int32_t)(character - ('a' - 'A'))
				: (int32_t)character;
	}
}

int __maskrune(int character, unsigned long mask)
{
	if (character < 0 || character > 255)
		return 0;
	return (int)(_DefaultRuneLocale.runetype[character] & mask);
}

int __tolower(int character)
{
	if (character < 0 || character > 255)
		return character;
	return _DefaultRuneLocale.lowercase[character];
}

int __toupper(int character)
{
	if (character < 0 || character > 255)
		return character;
	return _DefaultRuneLocale.uppercase[character];
}

size_t strlcpy(char* destination, const char* source, size_t capacity)
{
	size_t source_length = strlen(source);
	if (capacity) {
		size_t copied = source_length;
		if (copied >= capacity)
			copied = capacity - 1;
		memcpy(destination, source, copied);
		destination[copied] = '\0';
	}
	return source_length;
}

size_t strlcat(char* destination, const char* source, size_t capacity)
{
	size_t destination_length = strnlen(destination, capacity);
	if (destination_length == capacity)
		return capacity + strlen(source);
	return destination_length + strlcpy(
		destination + destination_length, source,
		capacity - destination_length);
}

static void fill_repeating_pattern(void* destination, const void* pattern,
				   size_t pattern_size, size_t length)
{
	unsigned char* output = destination;
	while (length) {
		size_t chunk = length < pattern_size ? length : pattern_size;
		memcpy(output, pattern, chunk);
		output += chunk;
		length -= chunk;
	}
}

void memset_pattern4(void* destination, const void* pattern, size_t length)
{
	fill_repeating_pattern(destination, pattern, 4, length);
}

void memset_pattern8(void* destination, const void* pattern, size_t length)
{
	fill_repeating_pattern(destination, pattern, 8, length);
}

void memset_pattern16(void* destination, const void* pattern, size_t length)
{
	fill_repeating_pattern(destination, pattern, 16, length);
}
