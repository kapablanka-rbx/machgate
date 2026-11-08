#include "ports/provider_lifetime.h"

#include <stdlib.h>

struct deferred_release {
	void* object;
	machgate_release_function release;
	struct deferred_release* next;
};

struct autorelease_pool {
	struct autorelease_pool* parent;
	struct deferred_release* entries;
};

static _Thread_local struct autorelease_pool* current_pool;

int machgate_defer_release_v1(void* object, machgate_release_function release)
{
	if (!object || !release || !current_pool)
		return 0;
	struct deferred_release* result = malloc(sizeof(*result));
	if (!result)
		return 0;
	*result = (struct deferred_release){object, release, current_pool->entries};
	current_pool->entries = result;
	return 1;
}

void* objc_autoreleasePoolPush(void)
{
	struct autorelease_pool* result = calloc(1, sizeof(*result));
	if (!result)
		abort();
	result->parent = current_pool;
	current_pool = result;
	return result;
}

void objc_autoreleasePoolPop(void* token)
{
	if (!token)
		return;
	struct autorelease_pool* target = current_pool;
	while (target && target != token)
		target = target->parent;
	if (!target)
		return;
	while (current_pool) {
		struct autorelease_pool* pool = current_pool;
		while (pool->entries) {
			struct deferred_release* entry = pool->entries;
			pool->entries = entry->next;
			entry->release(entry->object);
			free(entry);
		}
		current_pool = pool->parent;
		int matched = pool == token;
		free(pool);
		if (matched)
			return;
	}
}
