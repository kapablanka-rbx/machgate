#include "ports/native_blocks.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK_REFCOUNT_MASK 0xffffu
#define BLOCK_NEEDS_FREE (1u << 24)
#define BLOCK_HAS_COPY_DISPOSE (1u << 25)
#define BLOCK_IS_GLOBAL (1u << 28)
#define BLOCK_FIELD_IS_OBJECT 3
#define BLOCK_FIELD_IS_BLOCK 7
#define BLOCK_FIELD_IS_BYREF 8
#define BLOCK_FIELD_IS_WEAK 16
#define BLOCK_BYREF_CALLER 128

struct block_descriptor {
	uint64_t reserved;
	uint64_t size;
	void (*copy)(void* destination, const void* source);
	void (*dispose)(const void* block);
};

struct block_layout {
	void* isa;
	uint32_t flags;
	uint32_t reserved;
	void (*invoke)(void* block, ...);
	const struct block_descriptor* descriptor;
};

struct block_byref {
	void* isa;
	struct block_byref* forwarding;
	uint32_t flags;
	uint32_t size;
};

struct block_byref_helpers {
	struct block_byref header;
	void (*keep)(struct block_byref* destination, struct block_byref* source);
	void (*destroy)(struct block_byref* source);
};

void* _NSConcreteGlobalBlock;
void* _NSConcreteStackBlock;
void* _NSConcreteMallocBlock;

static void* (*retain_object)(void*);
static void (*release_object)(void*);

void machgate_register_block_object_lifetime_v1(void* (*retain)(void*), void (*release)(void*))
{
	retain_object = retain;
	release_object = release;
}

int machgate_is_block_v1(const void* object)
{
	if ((uintptr_t)object < 4096 || (uintptr_t)object % sizeof(void*))
		return 0;
	const void* isa = *(const void* const*)object;
	return isa == &_NSConcreteStackBlock || isa == &_NSConcreteGlobalBlock ||
		isa == &_NSConcreteMallocBlock;
}

static uint32_t change_reference_count(uint32_t* flags, int change)
{
	uint32_t previous = __atomic_load_n(flags, __ATOMIC_RELAXED);
	for (;;) {
		uint32_t count = previous & BLOCK_REFCOUNT_MASK;
		if (count == BLOCK_REFCOUNT_MASK || (change < 0 && !count))
			return count;
		uint32_t result = previous + change;
		if (__atomic_compare_exchange_n(flags, &previous, result, 0,
			__ATOMIC_ACQ_REL, __ATOMIC_RELAXED))
			return result & BLOCK_REFCOUNT_MASK;
	}
}

void* _Block_copy(const void* block)
{
	const struct block_layout* source = block;
	if (!source)
		return NULL;
	if (source->flags & BLOCK_NEEDS_FREE) {
		change_reference_count(&((struct block_layout*)source)->flags, 1);
		return (void*)source;
	}
	if (source->flags & BLOCK_IS_GLOBAL)
		return (void*)source;
	if (!source->descriptor || !source->invoke ||
		source->descriptor->size < sizeof(*source))
		return NULL;
	struct block_layout* result = malloc(source->descriptor->size);
	if (!result)
		return NULL;
	memcpy(result, source, source->descriptor->size);
	result->flags = (source->flags & ~BLOCK_REFCOUNT_MASK) | BLOCK_NEEDS_FREE | 1;
	result->isa = &_NSConcreteMallocBlock;
	if ((source->flags & BLOCK_HAS_COPY_DISPOSE) && source->descriptor->copy)
		source->descriptor->copy(result, source);
	return result;
}

void _Block_release(const void* block)
{
	struct block_layout* source = (struct block_layout*)block;
	if (!source || !(source->flags & BLOCK_NEEDS_FREE))
		return;
	if (change_reference_count(&source->flags, -1))
		return;
	if ((source->flags & BLOCK_HAS_COPY_DISPOSE) && source->descriptor->dispose)
		source->descriptor->dispose(source);
	free(source);
}

static void* copy_byref(const void* object)
{
	struct block_byref* source = (struct block_byref*)object;
	if (!source || !source->forwarding)
		return NULL;
	source = source->forwarding;
	if (source->flags & BLOCK_NEEDS_FREE) {
		change_reference_count(&source->flags, 1);
		return source;
	}
	if (source->size < sizeof(*source) || ((source->flags & BLOCK_HAS_COPY_DISPOSE) &&
		source->size < sizeof(struct block_byref_helpers)))
		return NULL;
	struct block_byref* result = malloc(source->size);
	if (!result)
		return NULL;
	memcpy(result, source, source->size);
	result->flags = (source->flags & ~BLOCK_REFCOUNT_MASK) | BLOCK_NEEDS_FREE | 2;
	result->forwarding = result;
	source->forwarding = result;
	if (source->flags & BLOCK_HAS_COPY_DISPOSE) {
		const struct block_byref_helpers* helpers = (void*)source;
		if (helpers->keep)
			helpers->keep(result, source);
	}
	return result;
}

static void release_byref(const void* object)
{
	struct block_byref* source = (struct block_byref*)object;
	if (!source || !source->forwarding)
		return;
	source = source->forwarding;
	if (!(source->flags & BLOCK_NEEDS_FREE) || change_reference_count(&source->flags, -1))
		return;
	if (source->flags & BLOCK_HAS_COPY_DISPOSE) {
		const struct block_byref_helpers* helpers = (void*)source;
		if (helpers->destroy)
			helpers->destroy(source);
	}
	free(source);
}

void _Block_object_assign(void* destination, const void* object, int flags)
{
	if (!destination)
		return;
	void* result = (void*)object;
	if (!(flags & BLOCK_BYREF_CALLER)) {
		if (flags & BLOCK_FIELD_IS_BYREF)
			result = copy_byref(object);
		else if ((flags & BLOCK_FIELD_IS_BLOCK) == BLOCK_FIELD_IS_BLOCK)
			result = _Block_copy(object);
		else if ((flags & BLOCK_FIELD_IS_OBJECT) == BLOCK_FIELD_IS_OBJECT &&
			!(flags & BLOCK_FIELD_IS_WEAK) && retain_object)
			result = retain_object(result);
	}
	*(void**)destination = result;
}

void _Block_object_dispose(const void* object, int flags)
{
	if (!object)
		return;
	if (flags & BLOCK_FIELD_IS_BYREF)
		release_byref(object);
	else if ((flags & (BLOCK_FIELD_IS_BLOCK | BLOCK_BYREF_CALLER)) == BLOCK_FIELD_IS_BLOCK)
		_Block_release(object);
	else if ((flags & (BLOCK_FIELD_IS_WEAK | BLOCK_FIELD_IS_BLOCK | BLOCK_BYREF_CALLER)) ==
		BLOCK_FIELD_IS_OBJECT && release_object)
		release_object((void*)object);
}
