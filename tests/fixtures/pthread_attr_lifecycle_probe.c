#define _GNU_SOURCE

#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define LIFECYCLE_ITERATIONS 2000
#define WORKER_COUNT 4
#define STACK_SIZE (256 * 1024)

typedef int (*attr_init_function)(pthread_attr_t*);
typedef int (*attr_destroy_function)(pthread_attr_t*);
typedef int (*attr_setstacksize_function)(pthread_attr_t*, size_t);
typedef int (*attr_getstacksize_function)(const pthread_attr_t*, size_t*);
typedef int (*attr_setdetachstate_function)(pthread_attr_t*, int);
typedef int (*attr_setschedparam_function)(pthread_attr_t*,
                                           const struct sched_param*);
typedef int (*attr_setschedpolicy_function)(pthread_attr_t*, int);
typedef int (*create_function)(pthread_t*, const pthread_attr_t*,
                               void* (*)(void*), void*);
typedef int (*join_function)(pthread_t, void**);

struct attr_api {
	attr_init_function init;
	attr_destroy_function destroy;
	attr_setstacksize_function setstacksize;
	attr_getstacksize_function getstacksize;
	attr_setdetachstate_function setdetachstate;
	attr_setschedparam_function setschedparam;
	attr_setschedpolicy_function setschedpolicy;
	create_function create;
	join_function join;
};

struct stress_context {
	struct attr_api api;
	pthread_attr_t attr;
	atomic_int start;
	atomic_int lifecycle_done;
	atomic_int failed;
	atomic_ulong successful_creates;
};

struct worker_context {
	struct stress_context* stress;
};

static void* created_thread(void* argument)
{
	return argument;
}

static int acceptable_race_result(int result)
{
	return result == 0 || result == EINVAL;
}

static void record_result(struct stress_context* stress, int result)
{
	if (!acceptable_race_result(result))
		atomic_store(&stress->failed, 1);
}

static void* attribute_worker(void* raw_context)
{
	struct worker_context* worker = raw_context;
	struct stress_context* stress = worker->stress;
	uintptr_t marker = (uintptr_t)pthread_self();

	while (!atomic_load(&stress->start))
		sched_yield();
	while (!atomic_load(&stress->lifecycle_done)) {
		size_t stack_size = 0;
		struct sched_param param = {.sched_priority = 0};
		pthread_t thread;
		void* thread_result = NULL;

		record_result(stress, stress->api.setstacksize(&stress->attr, STACK_SIZE));
		record_result(stress, stress->api.getstacksize(&stress->attr, &stack_size));
		record_result(stress, stress->api.setdetachstate(&stress->attr, 1));
		record_result(stress, stress->api.setschedparam(&stress->attr, &param));
		record_result(stress, stress->api.setschedpolicy(&stress->attr, 1));
		int result = stress->api.create(&thread, &stress->attr, created_thread,
		                                (void*)marker);
		if (result == 0) {
			if (stress->api.join(thread, &thread_result) != 0 ||
			    thread_result != (void*)marker)
				atomic_store(&stress->failed, 1);
			atomic_fetch_add(&stress->successful_creates, 1);
		} else if (result != EINVAL && result != EAGAIN) {
			atomic_store(&stress->failed, 1);
		}
	}
	return NULL;
}

static void* attribute_lifecycle(void* raw_context)
{
	struct stress_context* stress = raw_context;

	while (!atomic_load(&stress->start))
		sched_yield();
	for (int iteration = 0; iteration < LIFECYCLE_ITERATIONS; iteration++) {
		if (stress->api.init(&stress->attr) != 0)
			atomic_store(&stress->failed, 1);
		record_result(stress, stress->api.setstacksize(&stress->attr, STACK_SIZE));
		sched_yield();
		if (stress->api.destroy(&stress->attr) != 0)
			atomic_store(&stress->failed, 1);
		sched_yield();
	}
	atomic_store(&stress->lifecycle_done, 1);
	return NULL;
}

static int resolve_api(void* shim, struct attr_api* api)
{
	api->init = (attr_init_function)dlsym(shim, "pthread_attr_init");
	api->destroy = (attr_destroy_function)dlsym(shim, "pthread_attr_destroy");
	api->setstacksize = (attr_setstacksize_function)dlsym(
		shim, "pthread_attr_setstacksize");
	api->getstacksize = (attr_getstacksize_function)dlsym(
		shim, "pthread_attr_getstacksize");
	api->setdetachstate = (attr_setdetachstate_function)dlsym(
		shim, "pthread_attr_setdetachstate");
	api->setschedparam = (attr_setschedparam_function)dlsym(
		shim, "pthread_attr_setschedparam");
	api->setschedpolicy = (attr_setschedpolicy_function)dlsym(
		shim, "pthread_attr_setschedpolicy");
	api->create = (create_function)dlsym(shim, "pthread_create");
	api->join = (join_function)dlsym(shim, "pthread_join");
	return api->init && api->destroy && api->setstacksize &&
	       api->getstacksize && api->setdetachstate &&
	       api->setschedparam && api->setschedpolicy && api->create && api->join;
}

int main(int argument_count, char** arguments)
{
	if (argument_count != 2)
		return 2;
	void* shim = dlopen(arguments[1], RTLD_NOW | RTLD_LOCAL);
	if (!shim) {
		fprintf(stderr, "%s\n", dlerror());
		return 3;
	}

	struct stress_context stress = {0};
	if (!resolve_api(shim, &stress.api))
		return 4;
	struct worker_context workers[WORKER_COUNT];
	pthread_t worker_threads[WORKER_COUNT];
	pthread_t lifecycle_thread;
	for (int index = 0; index < WORKER_COUNT; index++) {
		workers[index].stress = &stress;
		if (pthread_create(&worker_threads[index], NULL, attribute_worker,
		                   &workers[index]) != 0)
			return 6;
	}
	if (pthread_create(&lifecycle_thread, NULL, attribute_lifecycle, &stress) != 0)
		return 7;
	atomic_store(&stress.start, 1);
	if (pthread_join(lifecycle_thread, NULL) != 0)
		return 8;
	for (int index = 0; index < WORKER_COUNT; index++) {
		if (pthread_join(worker_threads[index], NULL) != 0)
			return 9;
	}
	unsigned long successful_creates = atomic_load(&stress.successful_creates);
	printf("pthread_attr_lifecycle: %d reinitializations, %lu successful creates\n",
	       LIFECYCLE_ITERATIONS, successful_creates);
	return atomic_load(&stress.failed) || successful_creates == 0 ? 10 : 0;
}
