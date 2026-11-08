#include "../src/adapters/guest_unwind.c"

struct machgate_startup_cache* machgate_guest_image_startup_cache(
	const struct machgate_guest_image* image)
{
	(void)image;
	return NULL;
}

struct expected_cfi {
	unsigned cfa_register;
	uint64_t cfa_offset;
	struct {
		unsigned reg;
		int64_t offset;
	} registers[6];
	size_t register_count;
};

struct decoded_cfi {
	unsigned cfa_register;
	uint64_t cfa_offset;
	int64_t register_offsets[128];
	int register_present[128];
};

void* machgate_guest_image_validated_header(
	const struct machgate_guest_image* image)
{
	(void)image;
	return NULL;
}

intptr_t machgate_guest_image_slide(const struct machgate_guest_image* image)
{
	(void)image;
	return 0;
}

uintptr_t machgate_guest_image_mapping_start(
	const struct machgate_guest_image* image)
{
	(void)image;
	return 0;
}

uintptr_t machgate_guest_image_mapping_end(
	const struct machgate_guest_image* image)
{
	(void)image;
	return 0;
}

void* machgate_pool_alloc(struct machgate_guest_image* image, size_t size)
{
	(void)image;
	(void)size;
	return NULL;
}

static int decode_uleb(const uint8_t* bytes, size_t size, size_t* offset,
		       uint64_t* result)
{
	uint64_t value = 0;
	unsigned shift = 0;
	for (;;) {
		if (*offset == size || shift >= 64)
			return -1;
		uint8_t byte = bytes[(*offset)++];
		value |= (uint64_t)(byte & 0x7f) << shift;
		if (!(byte & 0x80)) {
			*result = value;
			return 0;
		}
		shift += 7;
	}
}

static int decode_fde_cfi(const uint8_t* bytes, size_t size,
			  struct decoded_cfi* result)
{
	if (size < 25)
		return -1;
	uint32_t length;
	memcpy(&length, bytes, sizeof(length));
	if (length + sizeof(length) > size)
		return -1;
	size_t offset = 24;
	uint64_t augmentation_size;
	if (decode_uleb(bytes, length + sizeof(length), &offset,
	                &augmentation_size) != 0 ||
	    augmentation_size > length + sizeof(length) - offset)
		return -1;
	offset += augmentation_size;

	memset(result, 0, sizeof(*result));
	while (offset < length + sizeof(length)) {
		uint8_t opcode = bytes[offset++];
		uint64_t reg;
		uint64_t factor;
		if (opcode == DW_CFA_NOP)
			continue;
		if (opcode == DW_CFA_DEF_CFA) {
			if (decode_uleb(bytes, length + sizeof(length), &offset, &reg) != 0 ||
			    decode_uleb(bytes, length + sizeof(length), &offset,
			                &result->cfa_offset) != 0)
				return -1;
			result->cfa_register = reg;
			continue;
		}
		if ((opcode & 0xc0) == 0x80) {
			reg = opcode & 0x3f;
		} else if (opcode == DW_CFA_OFFSET_EXTENDED) {
			if (decode_uleb(bytes, length + sizeof(length), &offset, &reg) != 0)
				return -1;
		} else {
			return -1;
		}
		if (reg >= sizeof(result->register_present) ||
		    decode_uleb(bytes, length + sizeof(length), &offset, &factor) != 0)
			return -1;
		result->register_offsets[reg] = -(int64_t)(factor * 8);
		result->register_present[reg] = 1;
	}
	return 0;
}

static int check_encoding(uint32_t encoding,
			  const struct expected_cfi* expected)
{
	uint8_t bytes[256] = {0};
	struct byte_writer writer = {
		.bytes = bytes,
		.capacity = sizeof(bytes),
	};
	size_t cie_offset;
	uintptr_t fde;
	if (emit_cie(&writer, 0, 0, &cie_offset, NULL) != 0 ||
	    emit_fde(&writer, cie_offset, 0x1000, 0x1100, encoding, 0, 0,
	             &fde) != 0) {
		fprintf(stderr, "could not emit FDE for encoding 0x%08x\n", encoding);
		return -1;
	}

	struct decoded_cfi decoded;
	if (decode_fde_cfi((const uint8_t*)fde, sizeof(bytes) - ((uint8_t*)fde - bytes),
	                   &decoded) != 0) {
		fprintf(stderr, "could not decode FDE for encoding 0x%08x\n", encoding);
		return -1;
	}
	if (decoded.cfa_register != expected->cfa_register ||
	    decoded.cfa_offset != expected->cfa_offset) {
		fprintf(stderr,
		        "CFA mismatch for 0x%08x: got r%u+%llu, expected r%u+%llu\n",
		        encoding, decoded.cfa_register,
		        (unsigned long long)decoded.cfa_offset,
		        expected->cfa_register,
		        (unsigned long long)expected->cfa_offset);
		return -1;
	}
	for (size_t index = 0; index < expected->register_count; index++) {
		unsigned reg = expected->registers[index].reg;
		int64_t expected_offset = expected->registers[index].offset;
		if (!decoded.register_present[reg] ||
		    decoded.register_offsets[reg] != expected_offset) {
			fprintf(stderr,
			        "register r%u mismatch for 0x%08x: got %lld, expected %lld\n",
			        reg, encoding,
			        decoded.register_present[reg] ?
			            (long long)decoded.register_offsets[reg] : 0LL,
			        (long long)expected_offset);
			return -1;
		}
	}
	return 0;
}

int main(void)
{
	static const struct expected_cfi frameless = {
		.cfa_register = 31,
		.cfa_offset = 32,
		.registers = {
			{19, -8}, {20, -16}, {72, -24}, {73, -32},
		},
		.register_count = 4,
	};
	static const struct expected_cfi framed = {
		.cfa_register = 29,
		.cfa_offset = 16,
		.registers = {
			{29, -16}, {30, -8}, {19, -24},
			{20, -32}, {72, -40}, {73, -48},
		},
		.register_count = 6,
	};
	if (check_encoding(0x02002101u, &frameless) != 0 ||
	    check_encoding(0x04000101u, &framed) != 0)
		return 1;
	puts("frameless and framed compact-unwind FDE CFI validated");
	return 0;
}
