#define _GNU_SOURCE
#include <dlfcn.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

enum { WORKER_COUNT = 8, ALLOCATIONS_PER_WORKER = 100000 };

static void* (*guest_malloc)(size_t);
static void* (*guest_realloc)(void*, size_t);
static void (*guest_free)(void*);
static size_t (*guest_malloc_size)(const void*);
static pthread_barrier_t phase_barrier;
static void* allocations[WORKER_COUNT][ALLOCATIONS_PER_WORKER];

static void require(int condition, const char* message)
{
	if (condition)
		return;

	fprintf(stderr, "%s\n", message);
	exit(1);
}

static void* exercise_allocations(void* argument)
{
	size_t worker = (uintptr_t)argument;
	for (size_t index = 0; index < ALLOCATIONS_PER_WORKER; index++) {
		size_t size = 17 + index % 47;
		unsigned char* result = guest_malloc(size);
		require(result != NULL, "allocation failed");
		require(guest_malloc_size(result) >= size, "lost allocation size during growth");
		result[0] = (unsigned char)worker;
		allocations[worker][index] = result;
	}
	pthread_barrier_wait(&phase_barrier);
	size_t owner = (worker + 1) % WORKER_COUNT;
	for (size_t index = 0; index < ALLOCATIONS_PER_WORKER; index++) {
		unsigned char* pointer = allocations[owner][index];
		require(pointer[0] == owner, "allocation contents changed");
		if (index % 3 == 0) {
			unsigned char* result = guest_realloc(pointer, 96);
			require(result != NULL && result[0] == owner, "cross-thread realloc lost contents");
			require(guest_malloc_size(result) >= 96, "cross-thread realloc lost ownership");
			pointer = result;
		}
		guest_free(pointer);
	}
	pthread_barrier_wait(&phase_barrier);
	for (size_t index = 0; index < ALLOCATIONS_PER_WORKER; index++) {
		void* result = guest_malloc(33);
		require(result != NULL && guest_malloc_size(result) >= 33,
		        "allocation ownership lost after churn");
		guest_free(result);
	}
	return NULL;
}

int main(int argc, char** argv)
{
	require(argc == 2, "expected shim path");
	void* library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
	require(library != NULL, "cannot load shim");
	guest_malloc = dlsym(library, "malloc");
	guest_realloc = dlsym(library, "realloc");
	guest_free = dlsym(library, "free");
	guest_malloc_size = dlsym(library, "malloc_size");
	require(guest_malloc && guest_realloc && guest_free && guest_malloc_size,
	        "allocator exports missing");
	void* result = guest_malloc(1);
	guest_free(result);

	struct timespec started;
	struct timespec finished;
	clock_gettime(CLOCK_MONOTONIC, &started);
	require(pthread_barrier_init(&phase_barrier, NULL, WORKER_COUNT) == 0,
	        "cannot initialize worker barrier");
	pthread_t workers[WORKER_COUNT];
	for (size_t index = 0; index < WORKER_COUNT; index++)
		require(pthread_create(&workers[index], NULL, exercise_allocations,
		                       (void*)(uintptr_t)index) == 0, "cannot start worker");
	for (size_t index = 0; index < WORKER_COUNT; index++)
		require(pthread_join(workers[index], NULL) == 0, "cannot join worker");
	pthread_barrier_destroy(&phase_barrier);
	clock_gettime(CLOCK_MONOTONIC, &finished);
	double seconds = (finished.tv_sec - started.tv_sec) +
	                 (finished.tv_nsec - started.tv_nsec) / 1000000000.0;
	printf("Allocation ownership: growth, cross-thread realloc/free, and churn passed in %.3fs\n", seconds);
	return 0;
}
