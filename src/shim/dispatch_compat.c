#define _GNU_SOURCE

#include <errno.h>
#include <dlfcn.h>
#include <limits.h>
#include <sched.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum machgate_dispatch_kind {
	MACHGATE_DISPATCH_SEMAPHORE = 1,
	MACHGATE_DISPATCH_QUEUE = 2,
	MACHGATE_DISPATCH_DATA = 3,
	MACHGATE_DISPATCH_GROUP = 4,
};

struct machgate_dispatch_header {
	int kind;
	_Atomic int reference_count;
};

struct machgate_dispatch_semaphore {
	struct machgate_dispatch_header header;
	sem_t native_semaphore;
};

struct machgate_dispatch_queue {
	struct machgate_dispatch_header header;
	char label[64];
};

struct machgate_dispatch_data {
	struct machgate_dispatch_header header;
	const void* bytes;
	size_t size;
};

struct machgate_dispatch_source_type {
	int availability;
};

const struct machgate_dispatch_source_type _dispatch_source_type_mach_recv = {
	0,
};

struct machgate_block_literal {
	void* isa;
	uint32_t flags;
	uint32_t reserved;
	void (*invoke)(void*);
	void* descriptor;
};

_Static_assert(sizeof(struct machgate_dispatch_queue) == 72,
	       "dispatch queue compatibility object size must remain stable");

struct machgate_dispatch_queue _dispatch_main_q = {
	{ MACHGATE_DISPATCH_QUEUE, ATOMIC_VAR_INIT(INT_MAX) },
	"com.apple.main-thread",
};

static struct machgate_dispatch_queue global_default_queue = {
	{ MACHGATE_DISPATCH_QUEUE, ATOMIC_VAR_INIT(INT_MAX) },
	"com.apple.root.default",
};

static void dispose_dispatch_extensions(void* object);

static uint64_t read_clock_nanoseconds(clockid_t clock)
{
	struct timespec current;

	if (clock_gettime(clock, &current) != 0)
		return 0;
	return (uint64_t)current.tv_sec * 1000000000ULL +
	       (uint64_t)current.tv_nsec;
}

static clockid_t deadline_clock(uint64_t deadline)
{
	return (int64_t)deadline < 0 ? CLOCK_REALTIME : CLOCK_MONOTONIC;
}

static uint64_t deadline_value(uint64_t deadline)
{
	return (int64_t)deadline < 0 ? 0 - deadline : deadline;
}

static struct timespec realtime_deadline(uint64_t remaining_nanoseconds)
{
	struct timespec result;

	clock_gettime(CLOCK_REALTIME, &result);
	result.tv_sec += (time_t)(remaining_nanoseconds / 1000000000ULL);
	result.tv_nsec += (long)(remaining_nanoseconds % 1000000000ULL);
	if (result.tv_nsec >= 1000000000L) {
		result.tv_sec++;
		result.tv_nsec -= 1000000000L;
	}
	return result;
}

void* dispatch_semaphore_create(long value)
{
	if (value < 0) {
		errno = EINVAL;
		return NULL;
	}

	struct machgate_dispatch_semaphore* result = calloc(1, sizeof(*result));
	if (!result)
		return NULL;
	result->header.kind = MACHGATE_DISPATCH_SEMAPHORE;
	atomic_init(&result->header.reference_count, 1);
	if (sem_init(&result->native_semaphore, 0, (unsigned int)value) != 0) {
		free(result);
		return NULL;
	}
	return result;
}

long dispatch_semaphore_wait(void* semaphore, uint64_t deadline)
{
	struct machgate_dispatch_semaphore* value = semaphore;

	if (!value) {
		errno = EINVAL;
		return -1;
	}
	if (deadline == 0)
		return sem_trywait(&value->native_semaphore);
	if (deadline == UINT64_MAX) {
		int result;
		do {
			result = sem_wait(&value->native_semaphore);
		} while (result != 0 && errno == EINTR);
		return result;
	}

	clockid_t clock = deadline_clock(deadline);
	uint64_t expiration = deadline_value(deadline);
	uint64_t now = read_clock_nanoseconds(clock);
	if (expiration <= now)
		return sem_trywait(&value->native_semaphore);
	struct timespec absolute = clock == CLOCK_REALTIME
		? (struct timespec){(time_t)(expiration / 1000000000ULL),
			(long)(expiration % 1000000000ULL)}
		: realtime_deadline(expiration - now);
	int result;
	do {
		result = sem_timedwait(&value->native_semaphore, &absolute);
	} while (result != 0 && errno == EINTR);
	return result;
}

long dispatch_semaphore_signal(void* semaphore)
{
	struct machgate_dispatch_semaphore* value = semaphore;

	if (!value)
		return 0;
	return sem_post(&value->native_semaphore);
}

void* dispatch_get_global_queue(long priority, unsigned long flags)
{
	(void)priority;
	(void)flags;
	return &global_default_queue;
}

void* dispatch_queue_create(const char* label, void* attributes)
{
	(void)attributes;
	struct machgate_dispatch_queue* result = calloc(1, sizeof(*result));

	if (!result)
		return NULL;
	result->header.kind = MACHGATE_DISPATCH_QUEUE;
	atomic_init(&result->header.reference_count, 1);
	if (label) {
		size_t length = strnlen(label, sizeof(result->label) - 1);
		memcpy(result->label, label, length);
	}
	return result;
}

static void invoke_block(void* block_pointer)
{
	struct machgate_block_literal* block = block_pointer;

	if (block && block->invoke)
		block->invoke(block);
}

void dispatch_async(void* queue, void* block)
{
	(void)queue;
	invoke_block(block);
}

void dispatch_sync(void* queue, void* block)
{
	(void)queue;
	invoke_block(block);
}

static int begin_once(long* predicate)
{
	long expected = 0;

	if (__atomic_compare_exchange_n(predicate, &expected, 1, 0,
	                                __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
		return 1;
	while (__atomic_load_n(predicate, __ATOMIC_ACQUIRE) != -1)
		sched_yield();
	return 0;
}

static void complete_once(long* predicate)
{
	__atomic_store_n(predicate, -1, __ATOMIC_RELEASE);
}

void dispatch_once(long* predicate, void* block)
{
	if (!predicate)
		return;
	if (begin_once(predicate)) {
		invoke_block(block);
		complete_once(predicate);
	}
}

void dispatch_once_f(long* predicate, void* context,
	             void (*function)(void*))
{
	if (!predicate)
		return;
	if (begin_once(predicate)) {
		if (function)
			function(context);
		complete_once(predicate);
	}
}

void* dispatch_source_create(const void* type, uintptr_t handle,
	                     uintptr_t mask, void* queue)
{
	(void)type;
	(void)handle;
	(void)mask;
	(void)queue;
	errno = ENOTSUP;
	return NULL;
}

void dispatch_source_set_event_handler(void* source, void* handler)
{
	(void)source;
	(void)handler;
}

void dispatch_resume(void* object)
{
	(void)object;
}

uint64_t dispatch_time(uint64_t base, int64_t delta)
{
	if (base == UINT64_MAX)
		return UINT64_MAX;
	if (base == 0)
		base = read_clock_nanoseconds(CLOCK_MONOTONIC);
	int wall = deadline_clock(base) == CLOCK_REALTIME;
	uint64_t value = deadline_value(base);
	if (value > INT64_MAX)
		return UINT64_MAX;
	if (delta >= 0) {
		uint64_t addition = (uint64_t)delta;
		if (addition > (uint64_t)INT64_MAX - value)
			return UINT64_MAX;
		value += addition;
	} else {
		uint64_t subtraction = (uint64_t)(-(delta + 1)) + 1;
		value = subtraction >= value ? (wall ? 2 : 1) : value - subtraction;
	}
	if (wall && value < 2)
		value = 2;
	return wall ? 0 - value : value;
}

uint64_t dispatch_walltime(const struct timespec* time, int64_t delta)
{
	__int128 result = time
		? (__int128)time->tv_sec * 1000000000 + time->tv_nsec
		: read_clock_nanoseconds(CLOCK_REALTIME);
	result += delta;
	if (result > INT64_MAX)
		return UINT64_MAX;
	if (result < 2)
		return delta >= 0 ? UINT64_MAX : UINT64_MAX - 1;
	return 0 - (uint64_t)result;
}

void* dispatch_data_create(const void* buffer, size_t size, void* queue,
	                   void* destructor)
{
	(void)queue;
	(void)destructor;
	struct machgate_dispatch_data* result = calloc(1, sizeof(*result));

	if (!result)
		return NULL;
	result->header.kind = MACHGATE_DISPATCH_DATA;
	atomic_init(&result->header.reference_count, 1);
	result->bytes = buffer;
	result->size = size;
	return result;
}

void dispatch_retain(void* object)
{
	struct machgate_dispatch_header* header = object;

	if (!header || object == &_dispatch_main_q || object == &global_default_queue)
		return;
	atomic_fetch_add_explicit(&header->reference_count, 1,
	                          memory_order_relaxed);
}

void dispatch_release(void* object)
{
	struct machgate_dispatch_header* header = object;

	if (!header || object == &_dispatch_main_q || object == &global_default_queue)
		return;
	int previous = atomic_fetch_sub_explicit(&header->reference_count, 1,
	                                         memory_order_acq_rel);
	if (previous != 1)
		return;
	dispose_dispatch_extensions(object);
	if (header->kind == MACHGATE_DISPATCH_SEMAPHORE) {
		struct machgate_dispatch_semaphore* semaphore = object;
		sem_destroy(&semaphore->native_semaphore);
	}
	free(object);
}

extern void* _Block_copy(const void*);
extern void _Block_release(const void*);

struct dispatch_metadata {
	void* object;
	void* context;
	void (*finalizer)(void*);
	struct dispatch_metadata* next;
};

struct dispatch_notification {
	void* queue;
	void* block;
	struct dispatch_notification* next;
};

struct dispatch_group {
	struct machgate_dispatch_header header;
	unsigned long pending;
	struct dispatch_notification* head;
	struct dispatch_notification* tail;
};

static atomic_flag extensions_lock = ATOMIC_FLAG_INIT;
static struct dispatch_metadata* metadata;

static void lock_extensions(void)
{
	while (atomic_flag_test_and_set_explicit(&extensions_lock, memory_order_acquire))
		sched_yield();
}

static void unlock_extensions(void)
{
	atomic_flag_clear_explicit(&extensions_lock, memory_order_release);
}

static struct dispatch_metadata* object_metadata(void* object, int create)
{
	for (struct dispatch_metadata* entry = metadata; entry; entry = entry->next)
		if (entry->object == object)
			return entry;
	if (!create || !object)
		return NULL;
	struct dispatch_metadata* result = calloc(1, sizeof(*result));
	if (result) {
		result->object = object;
		result->next = metadata;
		metadata = result;
	}
	return result;
}

void dispatch_set_context(void* object, void* context)
{
	lock_extensions();
	struct dispatch_metadata* entry = object_metadata(object, 1);
	if (entry)
		entry->context = context;
	unlock_extensions();
}

void* dispatch_get_context(void* object)
{
	lock_extensions();
	struct dispatch_metadata* entry = object_metadata(object, 0);
	void* result = entry ? entry->context : NULL;
	unlock_extensions();
	return result;
}

void dispatch_set_finalizer_f(void* object, void (*finalizer)(void*))
{
	lock_extensions();
	struct dispatch_metadata* entry = object_metadata(object, 1);
	if (entry)
		entry->finalizer = finalizer;
	unlock_extensions();
}

static void dispose_dispatch_extensions(void* object)
{
	lock_extensions();
	struct dispatch_metadata** link = &metadata;
	while (*link && (*link)->object != object)
		link = &(*link)->next;
	struct dispatch_metadata* entry = *link;
	if (entry)
		*link = entry->next;
	unlock_extensions();
	if (entry && entry->finalizer && entry->context)
		entry->finalizer(entry->context);
	free(entry);
	struct machgate_dispatch_header* header = object;
	if (header->kind != MACHGATE_DISPATCH_GROUP)
		return;
	struct dispatch_group* group = object;
	while (group->head) {
		struct dispatch_notification* notification = group->head;
		group->head = notification->next;
		_Block_release(notification->block);
		dispatch_release(notification->queue);
		free(notification);
	}
}

void* dispatch_queue_create_with_target(const char* label, void* attributes, void* target)
{
	(void)target;
	return dispatch_queue_create(label, attributes);
}

const char* dispatch_queue_get_label(void* queue)
{
	struct machgate_dispatch_queue* value = queue ? queue : &_dispatch_main_q;
	return value->header.kind == MACHGATE_DISPATCH_QUEUE ? value->label : "";
}

void dispatch_async_f(void* queue, void* context, void (*function)(void*))
{
	(void)queue;
	if (function)
		function(context);
}

void dispatch_sync_f(void* queue, void* context, void (*function)(void*))
{
	dispatch_async_f(queue, context, function);
}

void* dispatch_group_create(void)
{
	struct dispatch_group* result = calloc(1, sizeof(*result));
	if (result) {
		result->header.kind = MACHGATE_DISPATCH_GROUP;
		atomic_init(&result->header.reference_count, 1);
	}
	return result;
}

void dispatch_group_enter(void* object)
{
	struct dispatch_group* group = object;
	if (!group)
		return;
	lock_extensions();
	group->pending++;
	unlock_extensions();
}

static void post_notifications(struct dispatch_notification* notification)
{
	while (notification) {
		struct dispatch_notification* next = notification->next;
		dispatch_async(notification->queue, notification->block);
		_Block_release(notification->block);
		dispatch_release(notification->queue);
		free(notification);
		notification = next;
	}
}

void dispatch_group_leave(void* object)
{
	struct dispatch_group* group = object;
	if (!group)
		return;
	lock_extensions();
	if (!group->pending) {
		unlock_extensions();
		abort();
	}
	struct dispatch_notification* ready = NULL;
	if (--group->pending == 0) {
		ready = group->head;
		group->head = group->tail = NULL;
	}
	unlock_extensions();
	post_notifications(ready);
}

void dispatch_group_notify(void* object, void* queue, void* block)
{
	struct dispatch_group* group = object;
	if (!group || !block)
		return;
	struct dispatch_notification* notification = calloc(1, sizeof(*notification));
	if (!notification)
		return;
	notification->block = _Block_copy(block);
	notification->queue = queue;
	dispatch_retain(queue);
	lock_extensions();
	if (group->pending) {
		if (group->tail)
			group->tail->next = notification;
		else
			group->head = notification;
		group->tail = notification;
		unlock_extensions();
		return;
	}
	unlock_extensions();
	post_notifications(notification);
}

struct delayed_dispatch {
	uint64_t deadline;
	void* queue;
	void* block;
	void* context;
	void (*function)(void*);
};

static void* invoke_delayed(void* argument)
{
	struct delayed_dispatch* item = argument;
	uint64_t expiration = deadline_value(item->deadline);
	struct timespec deadline = {
		.tv_sec = (time_t)(expiration / 1000000000ULL),
		.tv_nsec = (long)(expiration % 1000000000ULL),
	};
	int result;
	do {
		result = clock_nanosleep(deadline_clock(item->deadline), TIMER_ABSTIME, &deadline, NULL);
	} while (result == EINTR);
	if (result == 0) {
		if (item->block)
			dispatch_async(item->queue, item->block);
		else
			dispatch_async_f(item->queue, item->context, item->function);
	}
	_Block_release(item->block);
	dispatch_release(item->queue);
	free(item);
	return NULL;
}

static void schedule_delayed(uint64_t when, void* queue, void* block,
	void* context, void (*function)(void*))
{
	if (when == UINT64_MAX)
		return;
	if (deadline_value(when) <= read_clock_nanoseconds(deadline_clock(when))) {
		if (block)
			dispatch_async(queue, block);
		else
			dispatch_async_f(queue, context, function);
		return;
	}
	int (*create_thread)(pthread_t*, const pthread_attr_t*, void*(*)(void*), void*) =
		dlvsym(RTLD_NEXT, "pthread_create", "GLIBC_2.17");
	struct delayed_dispatch* item = create_thread ? malloc(sizeof(*item)) : NULL;
	if (!item)
		return;
	*item = (struct delayed_dispatch){when, queue, _Block_copy(block), context, function};
	dispatch_retain(queue);
	pthread_t thread;
	if (create_thread(&thread, NULL, invoke_delayed, item) == 0) {
		pthread_detach(thread);
		return;
	}
	_Block_release(item->block);
	dispatch_release(queue);
	free(item);
}

void dispatch_after_f(uint64_t when, void* queue, void* context, void (*function)(void*))
{
	if (function)
		schedule_delayed(when, queue, NULL, context, function);
}

void dispatch_after(uint64_t when, void* queue, void* block)
{
	if (block)
		schedule_delayed(when, queue, block, NULL, NULL);
}

void dispatch_source_set_timer(void* source, uint64_t start, uint64_t interval, uint64_t leeway)
{
	(void)source;
	(void)start;
	(void)interval;
	(void)leeway;
}

void dispatch_source_set_cancel_handler(void* source, void* handler)
{
	(void)source;
	(void)handler;
}

void dispatch_source_cancel(void* source)
{
	(void)source;
}

const void* machgate_dispatch_data_bytes(void* object, size_t* size)
{
	struct machgate_dispatch_data* data = object;
	if (size)
		*size = data && data->header.kind == MACHGATE_DISPATCH_DATA ? data->size : 0;
	return data && data->header.kind == MACHGATE_DISPATCH_DATA ? data->bytes : NULL;
}
