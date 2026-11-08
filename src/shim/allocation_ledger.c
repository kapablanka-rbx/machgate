#define _GNU_SOURCE
#include "allocation_ledger.h"

#include <linux/futex.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#define MACHGATE_ALLOCATION_SHARD_COUNT 32
#define MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE 8192
#define MACHGATE_ALLOCATION_MAX_TABLE_SIZE 4194304
#define MACHGATE_ALLOCATION_LOCK_SPIN_LIMIT 64

struct machgate_allocation_record {
	void* ptr;
	size_t size;
	void* zone;
	unsigned flags;
};

struct machgate_allocation_shard {
	struct machgate_allocation_record* records;
	size_t capacity;
	size_t live_count;
	size_t tombstone_count;
	int drop_warned;
	unsigned lock_state;
} __attribute__((aligned(64)));

static struct machgate_allocation_record allocation_records_static[
    MACHGATE_ALLOCATION_SHARD_COUNT]
    [MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE];

static struct machgate_allocation_shard allocation_shards[MACHGATE_ALLOCATION_SHARD_COUNT] = {
	{ allocation_records_static[0], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[1], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[2], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[3], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[4], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[5], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[6], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[7], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[8], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[9], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[10], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[11], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[12], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[13], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[14], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[15], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[16], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[17], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[18], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[19], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[20], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[21], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[22], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[23], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[24], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[25], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[26], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[27], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[28], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[29], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[30], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[31], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
};

#if defined(__aarch64__)
#define MACHGATE_ALLOCATION_SPIN_HINT() __asm__ volatile("yield" ::: "memory")
#else
#define MACHGATE_ALLOCATION_SPIN_HINT() do { } while (0)
#endif

static void allocation_shard_lock(struct machgate_allocation_shard* shard)
{
	unsigned previous;
	unsigned expected = 0;

	if (__atomic_compare_exchange_n(&shard->lock_state, &expected,
	                                1, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
		return;
	for (unsigned spin = 0; spin < MACHGATE_ALLOCATION_LOCK_SPIN_LIMIT; spin++) {
		previous = __atomic_load_n(&shard->lock_state, __ATOMIC_RELAXED);
		if (previous == 0) {
			unsigned retry_expected = 0;
			if (__atomic_compare_exchange_n(&shard->lock_state, &retry_expected,
			                                1, 0, __ATOMIC_ACQUIRE,
			                                __ATOMIC_RELAXED))
				return;
		}
		MACHGATE_ALLOCATION_SPIN_HINT();
	}
	previous = __atomic_exchange_n(&shard->lock_state, 2, __ATOMIC_ACQUIRE);
	while (previous != 0) {
		syscall(SYS_futex, &shard->lock_state, FUTEX_WAIT_PRIVATE, 2, NULL,
		        NULL, 0);
		previous = __atomic_exchange_n(&shard->lock_state, 2,
		                               __ATOMIC_ACQUIRE);
	}
}

static void allocation_shard_unlock(struct machgate_allocation_shard* shard)
{
	if (__atomic_fetch_sub(&shard->lock_state, 1, __ATOMIC_RELEASE) == 1)
		return;
	__atomic_store_n(&shard->lock_state, 0, __ATOMIC_RELEASE);
	syscall(SYS_futex, &shard->lock_state, FUTEX_WAKE_PRIVATE, 1, NULL, NULL,
	        0);
}

static uintptr_t allocation_record_hash(const void* ptr)
{
	uintptr_t value = (uintptr_t)ptr >> 4;

	value *= 0x9E3779B97F4A7C15ULL;
	value ^= value >> 29;
	return value;
}

static struct machgate_allocation_shard* allocation_shard_for_hash(uintptr_t hash)
{
	return &allocation_shards[hash & (MACHGATE_ALLOCATION_SHARD_COUNT - 1)];
}

static struct machgate_allocation_record* allocation_shard_static_base(
    const struct machgate_allocation_shard* shard)
{
	size_t shard_index = (size_t)(shard - allocation_shards);

	return allocation_records_static[shard_index];
}

static size_t allocation_record_index(uintptr_t hash, size_t capacity)
{
	return (hash / MACHGATE_ALLOCATION_SHARD_COUNT) & (capacity - 1);
}

static int insert_allocation_record(
    struct machgate_allocation_record* records, size_t capacity,
    uintptr_t hash, void* ptr, size_t size, void* zone, unsigned flags,
    int* inserted_out, int* reused_tombstone_out)
{
	size_t index = allocation_record_index(hash, capacity);
	struct machgate_allocation_record* reusable_record = NULL;

	if (inserted_out)
		*inserted_out = 0;
	if (reused_tombstone_out)
		*reused_tombstone_out = 0;

	for (size_t probe = 0; probe < capacity; probe++) {
		struct machgate_allocation_record* record =
		    &records[(index + probe) & (capacity - 1)];

		if (record->ptr == ptr) {
			if (record->size == 0) {
				if (inserted_out)
					*inserted_out = 1;
				if (reused_tombstone_out)
					*reused_tombstone_out = 1;
			}
			record->ptr = ptr;
			record->size = size;
			record->zone = zone;
			record->flags = flags;
			return 1;
		}
		if (record->ptr && record->size == 0 && !reusable_record)
			reusable_record = record;
		if (!record->ptr) {
			if (reusable_record)
				record = reusable_record;
			if (inserted_out)
				*inserted_out = 1;
			if (reused_tombstone_out && reusable_record)
				*reused_tombstone_out = 1;
			record->ptr = ptr;
			record->size = size;
			record->zone = zone;
			record->flags = flags;
			return 1;
		}
	}
	if (reusable_record) {
		if (inserted_out)
			*inserted_out = 1;
		if (reused_tombstone_out)
			*reused_tombstone_out = 1;
		reusable_record->ptr = ptr;
		reusable_record->size = size;
		reusable_record->zone = zone;
		reusable_record->flags = flags;
		return 1;
	}
	return 0;
}

static void note_inserted_allocation_record(
    struct machgate_allocation_shard* shard, int inserted, int reused_tombstone)
{
	if (!inserted)
		return;
	shard->live_count++;
	if (reused_tombstone && shard->tombstone_count > 0)
		shard->tombstone_count--;
}

static int compact_allocation_shard(struct machgate_allocation_shard* shard)
{
	size_t capacity = shard->capacity;
	size_t bytes = capacity * sizeof(*shard->records);
	struct machgate_allocation_record* old_records = shard->records;
	struct machgate_allocation_record* new_records;
	size_t live_count = 0;

	new_records = (struct machgate_allocation_record*)syscall(
	    SYS_mmap, NULL, bytes, PROT_READ | PROT_WRITE,
	    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (new_records == MAP_FAILED)
		return 0;

	for (size_t index = 0; index < capacity; index++) {
		struct machgate_allocation_record* record = &old_records[index];
		if (!record->ptr || record->size == 0)
			continue;
		if (!insert_allocation_record(new_records, capacity,
		                              allocation_record_hash(record->ptr),
		                              record->ptr, record->size,
		                              record->zone, record->flags,
		                              NULL, NULL)) {
			syscall(SYS_munmap, new_records, bytes);
			return 0;
		}
		live_count++;
	}

	shard->records = new_records;
	shard->live_count = live_count;
	shard->tombstone_count = 0;
	if (old_records != allocation_shard_static_base(shard))
		syscall(SYS_munmap, old_records, bytes);
	return 1;
}

static int allocation_shard_should_compact(struct machgate_allocation_shard* shard)
{
	return shard->tombstone_count > 4096 &&
	       (shard->tombstone_count > shard->live_count ||
	        shard->tombstone_count > shard->capacity / 4);
}

static void warn_allocation_record_drop_once(struct machgate_allocation_shard* shard,
                                             void* ptr, size_t size, void* zone,
                                             unsigned flags)
{
	if (shard->drop_warned)
		return;
	shard->drop_warned = 1;
	fprintf(stderr,
	        "libsystem_shim: WARNING: allocation ledger full after %zu slots; first dropped ptr=%p size=%zu zone=%p flags=%#x\n",
	        shard->capacity, ptr, size, zone, flags);
}

static int grow_allocation_shard(struct machgate_allocation_shard* shard)
{
	size_t old_capacity = shard->capacity;
	size_t new_capacity = old_capacity * 2;
	size_t new_bytes;
	struct machgate_allocation_record* old_records = shard->records;
	struct machgate_allocation_record* new_records;

	if (old_capacity >= MACHGATE_ALLOCATION_MAX_TABLE_SIZE)
		return 0;
	if (new_capacity > MACHGATE_ALLOCATION_MAX_TABLE_SIZE)
		new_capacity = MACHGATE_ALLOCATION_MAX_TABLE_SIZE;
	if (new_capacity <= old_capacity)
		return 0;

	new_bytes = new_capacity * sizeof(*new_records);
	new_records = (struct machgate_allocation_record*)syscall(
	    SYS_mmap, NULL, new_bytes, PROT_READ | PROT_WRITE,
	    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (new_records == MAP_FAILED)
		return 0;

	for (size_t index = 0; index < old_capacity; index++) {
		struct machgate_allocation_record* record = &old_records[index];
		if (!record->ptr || record->size == 0)
			continue;
		if (!insert_allocation_record(new_records, new_capacity,
		                              allocation_record_hash(record->ptr),
		                              record->ptr, record->size,
		                              record->zone, record->flags,
		                              NULL, NULL)) {
			syscall(SYS_munmap, new_records, new_bytes);
			return 0;
		}
	}

	shard->records = new_records;
	shard->capacity = new_capacity;
	shard->tombstone_count = 0;
	if (old_records != allocation_shard_static_base(shard))
		syscall(SYS_munmap, old_records, old_capacity * sizeof(*old_records));
	return 1;
}

void machgate_record_allocation_with_zone(void* ptr, size_t size, void* zone)
{
	int inserted = 0;
	int reused_tombstone = 0;
	uintptr_t hash = allocation_record_hash(ptr);
	struct machgate_allocation_shard* shard = allocation_shard_for_hash(hash);

	if (!ptr)
		return;

	allocation_shard_lock(shard);
	if (allocation_shard_should_compact(shard))
		compact_allocation_shard(shard);
	while (!insert_allocation_record(shard->records, shard->capacity, hash,
	                                 ptr, size, zone, 0, &inserted,
	                                 &reused_tombstone)) {
		if (grow_allocation_shard(shard))
			continue;
		warn_allocation_record_drop_once(shard, ptr, size, zone, 0);
		break;
	}
	note_inserted_allocation_record(shard, inserted, reused_tombstone);
	allocation_shard_unlock(shard);
}

void machgate_record_allocation(void* ptr, size_t size)
{
	machgate_record_allocation_with_zone(ptr, size, NULL);
}

int machgate_lookup_allocation(const void* ptr, size_t* size_out, void** zone_out,
                             unsigned* flags_out)
{
	uintptr_t hash = allocation_record_hash(ptr);
	struct machgate_allocation_shard* shard = allocation_shard_for_hash(hash);
	size_t index;

	if (!ptr)
		return 0;

	allocation_shard_lock(shard);
	index = allocation_record_index(hash, shard->capacity);
	for (size_t probe = 0; probe < shard->capacity; probe++) {
		const struct machgate_allocation_record* record =
		    &shard->records[(index + probe) & (shard->capacity - 1)];

		if (!record->ptr) {
			allocation_shard_unlock(shard);
			return 0;
		}
		if (record->ptr == ptr) {
			if (size_out)
				*size_out = record->size;
			if (zone_out)
				*zone_out = record->zone;
			if (flags_out)
				*flags_out = record->flags;
			allocation_shard_unlock(shard);
			return 1;
		}
	}

	allocation_shard_unlock(shard);
	return 0;
}

int machgate_lookup_allocation_size(const void* ptr, size_t* size_out)
{
	return machgate_lookup_allocation(ptr, size_out, NULL, NULL);
}

size_t machgate_recorded_allocation_size(void* ptr, size_t requested_size)
{
	(void)ptr;
	if (requested_size > 0)
		return requested_size;
	return 1;
}

int machgate_take_allocation(const void* ptr, size_t* size_out)
{
	uintptr_t hash = allocation_record_hash(ptr);
	struct machgate_allocation_shard* shard = allocation_shard_for_hash(hash);
	size_t index;

	if (!ptr)
		return 0;

	allocation_shard_lock(shard);
	index = allocation_record_index(hash, shard->capacity);
	for (size_t probe = 0; probe < shard->capacity; probe++) {
		struct machgate_allocation_record* record =
		    &shard->records[(index + probe) & (shard->capacity - 1)];

		if (!record->ptr) {
			allocation_shard_unlock(shard);
			return 0;
		}
		if (record->ptr == ptr) {
			if (size_out)
				*size_out = record->size;
			if (record->size != 0) {
				shard->live_count--;
				shard->tombstone_count++;
			}
			record->size = 0;
			record->zone = NULL;
			record->flags = 0;
			allocation_shard_unlock(shard);
			return 1;
		}
	}
	allocation_shard_unlock(shard);
	return 0;
}

void machgate_forget_allocation(const void* ptr)
{
	machgate_take_allocation(ptr, NULL);
}

void machgate_mark_guest_cxx_allocation(void* ptr, size_t size)
{
	uintptr_t hash = allocation_record_hash(ptr);
	struct machgate_allocation_shard* shard = allocation_shard_for_hash(hash);
	size_t index;
	struct machgate_allocation_record* reusable_record = NULL;

	if (!ptr)
		return;

	allocation_shard_lock(shard);
	if (allocation_shard_should_compact(shard))
		compact_allocation_shard(shard);
retry:
	index = allocation_record_index(hash, shard->capacity);
	reusable_record = NULL;
	for (size_t probe = 0; probe < shard->capacity; probe++) {
		struct machgate_allocation_record* record =
		    &shard->records[(index + probe) & (shard->capacity - 1)];

		if (!record->ptr) {
			if (reusable_record)
				record = reusable_record;
			shard->live_count++;
			if (reusable_record && shard->tombstone_count > 0)
				shard->tombstone_count--;
			record->ptr = ptr;
			record->size = machgate_recorded_allocation_size(ptr, size);
			record->zone = NULL;
			record->flags = MACHGATE_ALLOCATION_GUEST_CXX;
			allocation_shard_unlock(shard);
			return;
		}
		if (record->ptr && record->size == 0 && !reusable_record)
			reusable_record = record;
		if (record->ptr == ptr) {
			if (record->size == 0) {
				shard->live_count++;
				if (shard->tombstone_count > 0)
					shard->tombstone_count--;
				record->size = machgate_recorded_allocation_size(ptr, size);
			}
			record->flags |= MACHGATE_ALLOCATION_GUEST_CXX;
			allocation_shard_unlock(shard);
			return;
		}
	}
	if (reusable_record) {
		shard->live_count++;
		if (shard->tombstone_count > 0)
			shard->tombstone_count--;
		reusable_record->ptr = ptr;
		reusable_record->size = machgate_recorded_allocation_size(ptr, size);
		reusable_record->zone = NULL;
		reusable_record->flags = MACHGATE_ALLOCATION_GUEST_CXX;
		allocation_shard_unlock(shard);
		return;
	}
	if (grow_allocation_shard(shard))
		goto retry;
	warn_allocation_record_drop_once(shard, ptr, size, NULL,
	                                 MACHGATE_ALLOCATION_GUEST_CXX);
	allocation_shard_unlock(shard);
}
