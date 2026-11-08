#define _GNU_SOURCE

#include "pthread_attr_compat.h"
#include "darwin_errno.h"

#include <dlfcn.h>
#include <errno.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

enum darwin_mutex_type {
	DARWIN_MUTEX_NORMAL = 0,
	DARWIN_MUTEX_ERROR_CHECK = 1,
	DARWIN_MUTEX_RECURSIVE = 2,
};

int machgate_darwin_pthread_mutexattr_settype(pthread_mutexattr_t* attributes, int type)
{
	int native_type;
	switch (type) {
	case DARWIN_MUTEX_NORMAL: native_type = PTHREAD_MUTEX_NORMAL; break;
	case DARWIN_MUTEX_ERROR_CHECK: native_type = PTHREAD_MUTEX_ERRORCHECK; break;
	case DARWIN_MUTEX_RECURSIVE: native_type = PTHREAD_MUTEX_RECURSIVE; break;
	default: return machgate_darwin_errno_from_linux(EINVAL);
	}
	return machgate_darwin_errno_from_linux(pthread_mutexattr_settype(attributes, native_type));
}

int machgate_darwin_pthread_mutexattr_gettype(const pthread_mutexattr_t* attributes, int* type)
{
	int native_type;
	int result = pthread_mutexattr_gettype(attributes, &native_type);
	if (result != 0)
		return machgate_darwin_errno_from_linux(result);
	switch (native_type) {
	case PTHREAD_MUTEX_NORMAL: *type = DARWIN_MUTEX_NORMAL; return 0;
	case PTHREAD_MUTEX_ERRORCHECK: *type = DARWIN_MUTEX_ERROR_CHECK; return 0;
	case PTHREAD_MUTEX_RECURSIVE: *type = DARWIN_MUTEX_RECURSIVE; return 0;
	default: return machgate_darwin_errno_from_linux(EINVAL);
	}
}

struct native_pthread_attr_api {
	int (*init)(pthread_attr_t*);
	int (*destroy)(pthread_attr_t*);
	int (*setstacksize)(pthread_attr_t*, size_t);
	int (*getstacksize)(const pthread_attr_t*, size_t*);
	int (*setdetachstate)(pthread_attr_t*, int);
	int (*getdetachstate)(const pthread_attr_t*, int*);
	int (*setschedparam)(pthread_attr_t*, const struct sched_param*);
	int (*getschedparam)(const pthread_attr_t*, struct sched_param*);
	int (*setschedpolicy)(pthread_attr_t*, int);
	int (*getschedpolicy)(const pthread_attr_t*, int*);
	int (*setstack)(pthread_attr_t*, void*, size_t);
	int (*create)(pthread_t*, const pthread_attr_t*, void* (*)(void*), void*);
	void* (*calloc)(size_t, size_t);
	void (*free)(void*);
};

struct pthread_attr_slot {
	const pthread_attr_t* guest;
	pthread_attr_t native;
	size_t guest_stack_size;
	int caller_owned_stack;
	struct pthread_attr_slot* next;
};

static struct native_pthread_attr_api native_api;
static _Atomic int native_api_state;
static atomic_flag attr_slots_lock = ATOMIC_FLAG_INIT;
static struct pthread_attr_slot* attr_slots;

static int resolve_native_api(void)
{
	int state = atomic_load_explicit(&native_api_state, memory_order_acquire);

	if (state == 2)
		return 0;
	int expected = 0;
	if (atomic_compare_exchange_strong_explicit(
		    &native_api_state, &expected, 1, memory_order_acquire,
		    memory_order_relaxed)) {
		native_api.init = (int (*)(pthread_attr_t*))dlsym(
			RTLD_NEXT, "pthread_attr_init");
		native_api.destroy = (int (*)(pthread_attr_t*))dlsym(
			RTLD_NEXT, "pthread_attr_destroy");
		native_api.setstacksize = (int (*)(pthread_attr_t*, size_t))dlsym(
			RTLD_NEXT, "pthread_attr_setstacksize");
		native_api.getstacksize =
			(int (*)(const pthread_attr_t*, size_t*))dlsym(
				RTLD_NEXT, "pthread_attr_getstacksize");
		native_api.setdetachstate = (int (*)(pthread_attr_t*, int))dlsym(
			RTLD_NEXT, "pthread_attr_setdetachstate");
		native_api.getdetachstate =
			(int (*)(const pthread_attr_t*, int*))dlsym(
				RTLD_NEXT, "pthread_attr_getdetachstate");
		native_api.setschedparam =
			(int (*)(pthread_attr_t*, const struct sched_param*))dlsym(
				RTLD_NEXT, "pthread_attr_setschedparam");
		native_api.getschedparam =
			(int (*)(const pthread_attr_t*, struct sched_param*))dlsym(
				RTLD_NEXT, "pthread_attr_getschedparam");
		native_api.setschedpolicy = (int (*)(pthread_attr_t*, int))dlsym(
			RTLD_NEXT, "pthread_attr_setschedpolicy");
		native_api.getschedpolicy = (int (*)(const pthread_attr_t*, int*))dlsym(
			RTLD_NEXT, "pthread_attr_getschedpolicy");
		native_api.setstack = (int (*)(pthread_attr_t*, void*, size_t))dlsym(
			RTLD_NEXT, "pthread_attr_setstack");
		native_api.create = (int (*)(pthread_t*, const pthread_attr_t*,
		                             void* (*)(void*), void*))dlsym(
			RTLD_NEXT, "pthread_create");
		native_api.calloc = (void* (*)(size_t, size_t))dlsym(
			RTLD_NEXT, "calloc");
		native_api.free = (void (*)(void*))dlsym(RTLD_NEXT, "free");
		if (!native_api.init || !native_api.destroy ||
		    !native_api.setstacksize || !native_api.getstacksize ||
		    !native_api.setdetachstate || !native_api.getdetachstate ||
		    !native_api.setschedparam || !native_api.getschedparam ||
		    !native_api.setschedpolicy || !native_api.getschedpolicy || !native_api.setstack ||
		    !native_api.create || !native_api.calloc || !native_api.free) {
			atomic_store_explicit(&native_api_state, 0, memory_order_release);
			return ENOSYS;
		}
		atomic_store_explicit(&native_api_state, 2, memory_order_release);
		return 0;
	}
	while (atomic_load_explicit(&native_api_state, memory_order_acquire) == 1)
		sched_yield();
	return atomic_load_explicit(&native_api_state, memory_order_acquire) == 2
		? 0 : ENOSYS;
}

static void lock_attr_slots(void)
{
	while (atomic_flag_test_and_set_explicit(&attr_slots_lock,
	                                         memory_order_acquire))
		sched_yield();
}

static void unlock_attr_slots(void)
{
	atomic_flag_clear_explicit(&attr_slots_lock, memory_order_release);
}

static struct pthread_attr_slot** find_attr_slot(const pthread_attr_t* guest)
{
	struct pthread_attr_slot** link = &attr_slots;

	while (*link) {
		if ((*link)->guest == guest)
			return link;
		link = &(*link)->next;
	}
	return NULL;
}

int machgate_native_pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                                   void* (*start_routine)(void*), void* arg)
{
	int result = resolve_native_api();
	const pthread_attr_t* native = NULL;

	if (result != 0)
		return result;
	lock_attr_slots();
	if (attr) {
		struct pthread_attr_slot** link = find_attr_slot(attr);
		if (link)
			native = &(*link)->native;
	}
	result = native_api.create(thread, native, start_routine, arg);
	unlock_attr_slots();
	return result;
}

int pthread_attr_init(pthread_attr_t* attr)
{
	int result = resolve_native_api();
	struct pthread_attr_slot* slot;
	int created = 0;

	if (result != 0)
		return result;
	lock_attr_slots();
	struct pthread_attr_slot** link = find_attr_slot(attr);
	if (link) {
		slot = *link;
		result = native_api.destroy(&slot->native);
	} else {
		slot = native_api.calloc(1, sizeof(*slot));
		if (slot) {
			slot->guest = attr;
			slot->next = attr_slots;
			attr_slots = slot;
			created = 1;
		}
	}
	if (slot && result == 0) {
		result = native_api.init(&slot->native);
		slot->guest_stack_size = 0;
		slot->caller_owned_stack = 0;
	}
	unlock_attr_slots();
	if (!slot)
		return ENOMEM;
	if (result != 0 && created) {
		lock_attr_slots();
		link = find_attr_slot(attr);
		if (link && *link == slot) {
			*link = slot->next;
			native_api.free(slot);
		}
		unlock_attr_slots();
	}
	if (result == 0)
		memset(attr, 0, sizeof(uint64_t));
	return result;
}

int pthread_attr_destroy(pthread_attr_t* attr)
{
	int result = resolve_native_api();

	if (result != 0)
		return result;
	lock_attr_slots();
	struct pthread_attr_slot** link = find_attr_slot(attr);
	if (!link) {
		unlock_attr_slots();
		return EINVAL;
	}
	struct pthread_attr_slot* slot = *link;
	result = native_api.destroy(&slot->native);
	*link = slot->next;
	native_api.free(slot);
	unlock_attr_slots();
	return result;
}

static int set_stack_size(pthread_attr_t* attr, size_t stack_size, int darwin)
{
	int result = resolve_native_api();
	struct pthread_attr_slot* slot = NULL;

	if (result != 0)
		return result;
	lock_attr_slots();
	struct pthread_attr_slot** link = find_attr_slot(attr);
	if (link)
		slot = *link;
	if (!slot) {
		unlock_attr_slots();
		return EINVAL;
	}
	size_t native_stack_size = stack_size;
	if (darwin && !slot->caller_owned_stack && native_stack_size < PTHREAD_STACK_MIN)
		native_stack_size = PTHREAD_STACK_MIN;
	result = native_api.setstacksize(&slot->native, native_stack_size);
	if (result == 0)
		slot->guest_stack_size = darwin ? stack_size : 0;
	unlock_attr_slots();
	return result;
}

int pthread_attr_setstacksize(pthread_attr_t* attr, size_t stack_size)
{
	return set_stack_size(attr, stack_size, 0);
}

int machgate_darwin_pthread_attr_setstacksize(pthread_attr_t* attr, size_t stack_size)
{
	extern uint64_t vm_page_size;
	if (stack_size < 16384 || stack_size % vm_page_size != 0)
		return EINVAL;
	return set_stack_size(attr, stack_size, 1);
}

static int get_stack_size(const pthread_attr_t* attr, size_t* stack_size, int darwin)
{
	int result = resolve_native_api();
	struct pthread_attr_slot* slot = NULL;

	if (result != 0)
		return result;
	lock_attr_slots();
	struct pthread_attr_slot** link = find_attr_slot(attr);
	if (link)
		slot = *link;
	if (!slot) {
		unlock_attr_slots();
		return EINVAL;
	}
	if (darwin && slot->guest_stack_size)
		*stack_size = slot->guest_stack_size;
	else
		result = native_api.getstacksize(&slot->native, stack_size);
	unlock_attr_slots();
	return result;
}

int pthread_attr_getstacksize(const pthread_attr_t* attr, size_t* stack_size)
{
	return get_stack_size(attr, stack_size, 0);
}

int machgate_darwin_pthread_attr_getstacksize(const pthread_attr_t* attr, size_t* stack_size)
{
	return get_stack_size(attr, stack_size, 1);
}

int pthread_attr_setdetachstate(pthread_attr_t* attr, int detach_state)
{
	int result = resolve_native_api();
	struct pthread_attr_slot* slot = NULL;
	int linux_state;

	if (result != 0)
		return result;
	lock_attr_slots();
	struct pthread_attr_slot** link = find_attr_slot(attr);
	if (link)
		slot = *link;
	if (!slot) {
		unlock_attr_slots();
		return EINVAL;
	}
	if (detach_state == 1)
		linux_state = PTHREAD_CREATE_JOINABLE;
	else if (detach_state == 2)
		linux_state = PTHREAD_CREATE_DETACHED;
	else {
		unlock_attr_slots();
		return EINVAL;
	}
	result = native_api.setdetachstate(&slot->native, linux_state);
	unlock_attr_slots();
	return result;
}

int pthread_attr_getdetachstate(const pthread_attr_t* attr, int* detach_state)
{
	int result = resolve_native_api();
	struct pthread_attr_slot* slot = NULL;
	int linux_state;

	if (result != 0)
		return result;
	lock_attr_slots();
	struct pthread_attr_slot** link = find_attr_slot(attr);
	if (link)
		slot = *link;
	if (!slot) {
		unlock_attr_slots();
		return EINVAL;
	}
	result = native_api.getdetachstate(&slot->native, &linux_state);
	if (result == 0)
		*detach_state = linux_state == PTHREAD_CREATE_DETACHED ? 2 : 1;
	unlock_attr_slots();
	return result;
}

int pthread_attr_setschedparam(pthread_attr_t* attr,
                               const struct sched_param* sched_param)
{
	int result = resolve_native_api();
	struct pthread_attr_slot* slot = NULL;

	if (result != 0)
		return result;
	lock_attr_slots();
	struct pthread_attr_slot** link = find_attr_slot(attr);
	if (link)
		slot = *link;
	if (!slot) {
		unlock_attr_slots();
		return EINVAL;
	}
	result = native_api.setschedparam(&slot->native, sched_param);
	unlock_attr_slots();
	return result;
}

int pthread_attr_getschedparam(const pthread_attr_t* attr,
                               struct sched_param* sched_param)
{
	int result = resolve_native_api();
	struct pthread_attr_slot* slot = NULL;

	if (result != 0)
		return result;
	lock_attr_slots();
	struct pthread_attr_slot** link = find_attr_slot(attr);
	if (link)
		slot = *link;
	if (!slot) {
		unlock_attr_slots();
		return EINVAL;
	}
	result = native_api.getschedparam(&slot->native, sched_param);
	unlock_attr_slots();
	return result;
}

int pthread_attr_setschedpolicy(pthread_attr_t* attr, int policy)
{
	int result = resolve_native_api();
	struct pthread_attr_slot* slot = NULL;
	int linux_policy;

	if (result != 0)
		return result;
	lock_attr_slots();
	struct pthread_attr_slot** link = find_attr_slot(attr);
	if (link)
		slot = *link;
	if (!slot) {
		unlock_attr_slots();
		return EINVAL;
	}
	switch (policy) {
	case 1:
		linux_policy = SCHED_OTHER;
		break;
	case 4:
		linux_policy = SCHED_FIFO;
		break;
	case 2:
		linux_policy = SCHED_RR;
		break;
	default:
		unlock_attr_slots();
		return EINVAL;
	}
	result = native_api.setschedpolicy(&slot->native, linux_policy);
	unlock_attr_slots();
	return result;
}

int machgate_darwin_pthread_attr_getschedpolicy(const pthread_attr_t* attr, int* policy)
{
	int result = resolve_native_api();
	if (result)
		return result;
	if (!policy)
		return EINVAL;
	lock_attr_slots();
	struct pthread_attr_slot** link = find_attr_slot(attr);
	if (!link) {
		unlock_attr_slots();
		return EINVAL;
	}
	int native_policy;
	result = native_api.getschedpolicy(&(*link)->native, &native_policy);
	if (result == 0) {
		switch (native_policy) {
		case SCHED_OTHER: *policy = 1; break;
		case SCHED_FIFO: *policy = 4; break;
		case SCHED_RR: *policy = 2; break;
		default: result = EINVAL; break;
		}
	}
	unlock_attr_slots();
	return result;
}

int pthread_attr_setstack(pthread_attr_t* attr, void* stack_base,
                          size_t stack_size)
{
	int result = resolve_native_api();
	struct pthread_attr_slot* slot = NULL;

	if (result != 0)
		return result;
	lock_attr_slots();
	struct pthread_attr_slot** link = find_attr_slot(attr);
	if (link)
		slot = *link;
	if (!slot) {
		unlock_attr_slots();
		return EINVAL;
	}
	result = native_api.setstack(&slot->native, stack_base, stack_size);
	if (result == 0) {
		slot->caller_owned_stack = 1;
		slot->guest_stack_size = 0;
	}
	unlock_attr_slots();
	return result;
}
