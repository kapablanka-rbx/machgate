#include "domain/dwarf_unwind.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

struct dwarf_reader {
	const uint8_t* cursor;
	const uint8_t* end;
};

static int read_unsigned(struct dwarf_reader* reader, size_t size,
			 uint64_t* result)
{
	if (size > sizeof(*result) || size > (size_t)(reader->end - reader->cursor))
		return -1;
	*result = 0;
	memcpy(result, reader->cursor, size);
	reader->cursor += size;
	return 0;
}

static int read_leb(struct dwarf_reader* reader, int signed_value,
		    uint64_t* result)
{
	uint64_t value = 0;
	for (unsigned shift = 0; shift < 64; shift += 7) {
		uint64_t byte;
		if (read_unsigned(reader, 1, &byte) != 0)
			return -1;
		uint64_t payload = byte & 0x7f;
		if (shift == 63) {
			if (signed_value ? (payload != 0 && payload != 0x7f) : payload > 1)
				return -1;
		}
		value |= payload << shift;
		if (!(byte & 0x80)) {
			if (signed_value && (byte & 0x40) && shift < 57)
				value |= UINT64_MAX << (shift + 7);
			*result = value;
			return 0;
		}
	}
	return -1;
}

static int read_record(const void* address, struct dwarf_reader* result,
		       size_t* identifier_size)
{
	uint32_t short_length;
	memcpy(&short_length, address, sizeof(short_length));
	uint64_t length = short_length;
	size_t header_size = sizeof(short_length);
	*identifier_size = 4;
	if (short_length == UINT32_MAX) {
		memcpy(&length, (const uint8_t*)address + 4, sizeof(length));
		header_size += sizeof(length);
		*identifier_size = 8;
	}
	uintptr_t start = (uintptr_t)address;
	if (length < *identifier_size || start > UINTPTR_MAX - header_size ||
	    length > UINTPTR_MAX - start - header_size || length > PTRDIFF_MAX)
		return -1;
	result->cursor = (const uint8_t*)(start + header_size);
	result->end = result->cursor + length;
	return 0;
}

static int read_scalar(struct dwarf_reader* reader, unsigned format,
		       uint64_t* result)
{
	if (format == 1 || format == 9)
		return read_leb(reader, format == 9, result);
	size_t size;
	switch (format) {
	case 0: size = sizeof(uintptr_t); break;
	case 2: case 10: size = 2; break;
	case 3: case 11: size = 4; break;
	case 4: case 12: size = 8; break;
	default: return -1;
	}
	if (read_unsigned(reader, size, result) != 0)
		return -1;
	if (format >= 10 && size < sizeof(*result)) {
		unsigned shift = 64 - (unsigned)size * 8;
		*result = (uint64_t)((int64_t)(*result << shift) >> shift);
	}
	return 0;
}

static int read_pointer(struct dwarf_reader* reader, unsigned encoding,
			int dereference, uintptr_t* result)
{
	unsigned relative = encoding & 0x70;
	if (relative == 0x50) {
		uintptr_t padding = -(uintptr_t)reader->cursor & (sizeof(uintptr_t) - 1);
		if (padding > (uintptr_t)(reader->end - reader->cursor))
			return -1;
		reader->cursor += padding;
		relative = 0;
	}
	if (relative != 0 && relative != 0x10)
		return -1;
	uintptr_t base = relative ? (uintptr_t)reader->cursor : 0;
	uint64_t value;
	if (read_scalar(reader, encoding & 0xf, &value) != 0)
		return -1;
	uintptr_t address = base + value;
	if ((encoding & 0x80) && dereference)
		memcpy(&address, (const void*)address, sizeof(address));
	*result = address;
	return 0;
}

static int read_augmentation(struct dwarf_reader* reader,
			     const uint8_t* augmentation,
			     const uint8_t* augmentation_end,
			     unsigned* result)
{
	*result = 0;
	if (augmentation == augmentation_end)
		return 0;
	if (*augmentation++ != 'z')
		return -1;
	uint64_t size;
	if (read_leb(reader, 0, &size) != 0 ||
	    size > (uint64_t)(reader->end - reader->cursor))
		return -1;
	struct dwarf_reader payload = {reader->cursor, reader->cursor + size};
	for (; augmentation < augmentation_end; augmentation++) {
		uint64_t encoding;
		uintptr_t personality;
		switch (*augmentation) {
		case 'R':
			if (read_unsigned(&payload, 1, &encoding) != 0)
				return -1;
			*result = encoding;
			break;
		case 'L':
			if (read_unsigned(&payload, 1, &encoding) != 0)
				return -1;
			break;
		case 'P':
			if (read_unsigned(&payload, 1, &encoding) != 0 ||
			    read_pointer(&payload, encoding, 0, &personality) != 0)
				return -1;
			break;
		case 'S': case 'B':
			break;
		default:
			return -1;
		}
	}
	return 0;
}

static int read_frame_encoding(const void* cie, unsigned* result)
{
	struct dwarf_reader reader;
	size_t identifier_size;
	uint64_t identifier;
	uint64_t version;
	if (read_record(cie, &reader, &identifier_size) != 0 ||
	    read_unsigned(&reader, identifier_size, &identifier) != 0 || identifier ||
	    read_unsigned(&reader, 1, &version) != 0 ||
	    (version != 1 && version != 3 && version != 4))
		return -1;
	const uint8_t* augmentation = reader.cursor;
	while (reader.cursor < reader.end && *reader.cursor)
		reader.cursor++;
	if (reader.cursor == reader.end)
		return -1;
	const uint8_t* augmentation_end = reader.cursor++;
	uint64_t ignored;
	if (version == 4) {
		uint64_t address_size;
		uint64_t segment_size;
		if (read_unsigned(&reader, 1, &address_size) != 0 ||
		    read_unsigned(&reader, 1, &segment_size) != 0 ||
		    address_size != sizeof(uintptr_t) || segment_size)
			return -1;
	}
	if (read_leb(&reader, 0, &ignored) != 0 ||
	    read_leb(&reader, 1, &ignored) != 0)
		return -1;
	if (version == 1 ? read_unsigned(&reader, 1, &ignored) != 0 :
	                   read_leb(&reader, 0, &ignored) != 0)
		return -1;
	return read_augmentation(&reader, augmentation, augmentation_end, result);
}

int machgate_decode_dwarf_frame(const void* frame,
			       struct machgate_dwarf_frame* result)
{
	if (!frame || !result)
		return -1;
	struct dwarf_reader reader;
	size_t identifier_size;
	if (read_record(frame, &reader, &identifier_size) != 0)
		return -1;
	uintptr_t cie_pointer = (uintptr_t)reader.cursor;
	uint64_t cie_distance;
	if (read_unsigned(&reader, identifier_size, &cie_distance) != 0 ||
	    !cie_distance || cie_distance > cie_pointer)
		return -1;
	unsigned encoding;
	if (read_frame_encoding((const void*)(cie_pointer - cie_distance), &encoding) != 0)
		return -1;
	uintptr_t start;
	uint64_t range;
	if (read_pointer(&reader, encoding, 1, &start) != 0 ||
	    read_scalar(&reader, encoding & 0xf, &range) != 0 ||
	    !range || range > UINTPTR_MAX - start)
		return -1;
	*result = (struct machgate_dwarf_frame){start, start + range};
	return 0;
}
