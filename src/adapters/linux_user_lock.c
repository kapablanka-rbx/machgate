#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "ports/user_lock.h"

#include <errno.h>
#include <limits.h>
#include <linux/futex.h>
#include <stdatomic.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>

struct private_waiter {
	void* address;
	struct private_waiter* next;
};

struct private_waiter_bucket {
	_Atomic uint64_t owner;
	struct private_waiter* waiters;
};

static struct private_waiter_bucket private_waiters[256];

static void lock_waiter_bucket(struct private_waiter_bucket* bucket)
{
	uint64_t process = (uint64_t)(uint32_t)syscall(SYS_getpid) << 32;
	uint64_t owner = process | (uint32_t)syscall(SYS_gettid);
	for (;;) {
		uint64_t previous = atomic_load_explicit(&bucket->owner, memory_order_relaxed);
		int different_process = (previous & 0xffffffff00000000ULL) != process;
		if (((uint32_t)previous == 0 || different_process) &&
		    atomic_compare_exchange_weak_explicit(&bucket->owner, &previous, owner,
		                                         memory_order_acquire, memory_order_relaxed)) {
			if (different_process)
				bucket->waiters = NULL;
			return;
		}
		syscall(SYS_sched_yield);
	}
}

static void unlock_waiter_bucket(struct private_waiter_bucket* bucket)
{
	uint64_t process = atomic_load_explicit(&bucket->owner, memory_order_relaxed) &
	                   0xffffffff00000000ULL;
	atomic_store_explicit(&bucket->owner, process, memory_order_release);
}

static struct private_waiter_bucket* register_private_waiter(struct private_waiter* waiter)
{
	uintptr_t key = (uintptr_t)waiter->address >> 2;
	struct private_waiter_bucket* bucket = &private_waiters[(key ^ (key >> 8)) & 255];
	lock_waiter_bucket(bucket);
	waiter->next = bucket->waiters;
	bucket->waiters = waiter;
	unlock_waiter_bucket(bucket);
	return bucket;
}

static int remove_private_waiter(struct private_waiter_bucket* bucket,
	struct private_waiter* waiter)
{
	if (!bucket)
		return 0;
	lock_waiter_bucket(bucket);
	int result = 0;
	struct private_waiter** current = &bucket->waiters;
	while (*current) {
		if (*current == waiter) {
			*current = waiter->next;
			continue;
		}
		if ((*current)->address == waiter->address && result < INT_MAX)
			++result;
		current = &(*current)->next;
	}
	unlock_waiter_bucket(bucket);
	return result;
}

static struct timespec nanoseconds_to_timespec(uint64_t value)
{
	return (struct timespec){ value / 1000000000, value % 1000000000 };
}

static long wait_on_wide_word(void* address, uint64_t expected, int shared,
                              uint64_t timeout_ns, int absolute)
{
	struct futex_waitv waiters[2] = {
		{ .val = (uint32_t)expected, .uaddr = (uintptr_t)address,
		  .flags = FUTEX_32 | (shared ? 0 : FUTEX_PRIVATE_FLAG) },
		{ .val = expected >> 32, .uaddr = (uintptr_t)address + 4,
		  .flags = FUTEX_32 | (shared ? 0 : FUTEX_PRIVATE_FLAG) },
	};
	struct timespec deadline;
	struct timespec* deadline_ptr = NULL;
	if (timeout_ns) {
		if (!absolute) {
			struct timespec now;
			if (syscall(SYS_clock_gettime, CLOCK_MONOTONIC, &now) < 0)
				return -1;
			uint64_t now_ns = (uint64_t)now.tv_sec * 1000000000 + now.tv_nsec;
			timeout_ns = timeout_ns > UINT64_MAX - now_ns ? UINT64_MAX : timeout_ns + now_ns;
		}
		deadline = nanoseconds_to_timespec(timeout_ns);
		deadline_ptr = &deadline;
	}
	return syscall(SYS_futex_waitv, waiters, 2, 0, deadline_ptr, CLOCK_MONOTONIC);
}

static long compare_narrow_word_with_wide_value(void* address)
{
	uint32_t observed;
	struct iovec local = { &observed, sizeof(observed) };
	struct iovec remote = { address, sizeof(observed) };
	return syscall(SYS_process_vm_readv, syscall(SYS_getpid), &local, 1,
	               &remote, 1, 0);
}

int machgate_wait_on_word(void* address, uint64_t expected, unsigned width,
                         int shared, uint64_t timeout_ns, int absolute)
{
	int saved_errno = errno;
	struct private_waiter waiter = { .address = address };
	struct private_waiter_bucket* bucket = shared ? NULL : register_private_waiter(&waiter);
	long result;
	if (width == 8) {
		result = wait_on_wide_word(address, expected, shared, timeout_ns, absolute);
	} else if (expected > UINT32_MAX) {
		result = compare_narrow_word_with_wide_value(address);
	} else {
		int operation = absolute ? FUTEX_WAIT_BITSET : FUTEX_WAIT;
		if (!shared)
			operation |= FUTEX_PRIVATE_FLAG;
		struct timespec timeout = nanoseconds_to_timespec(timeout_ns);
		result = syscall(SYS_futex, address, operation, (uint32_t)expected,
		                 timeout_ns ? &timeout : NULL, NULL, FUTEX_BITSET_MATCH_ANY);
	}
	int failure = errno;
	int remaining = remove_private_waiter(bucket, &waiter);
	errno = saved_errno;
	return result >= 0 || failure == EAGAIN ? remaining : -failure;
}

int machgate_wake_word(void* address, int shared, int wake_all)
{
	int saved_errno = errno;
	long result = syscall(SYS_futex, address,
		FUTEX_WAKE | (shared ? 0 : FUTEX_PRIVATE_FLAG),
		wake_all ? INT_MAX : 1, NULL, NULL, 0);
	int failure = errno;
	errno = saved_errno;
	if (result < 0)
		return -failure;
	return result > 0 ? 0 : -ENOENT;
}
