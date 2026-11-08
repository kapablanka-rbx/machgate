#include <dlfcn.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

typedef int64_t (*add_function)(int64_t, volatile int64_t*);
typedef int64_t (*step_function)(volatile int64_t*);
typedef uint8_t (*swap_function)(uint64_t, uint64_t, volatile uint64_t*);

static add_function add64;
static volatile int64_t shared_value;

static void* add_repeatedly(void* argument)
{
	(void)argument;
	for (int index = 0; index < 10000; index++)
		add64(1, &shared_value);
	return NULL;
}

int main(void)
{
	void* shim = dlopen("libsystem_shim.so", RTLD_NOW | RTLD_LOCAL);
	if (!shim) {
		fprintf(stderr, "%s\n", dlerror());
		return 1;
	}

	add64 = (add_function)dlsym(shim, "OSAddAtomic64");
	step_function increment64 =
		(step_function)dlsym(shim, "OSIncrementAtomic64");
	step_function decrement64 =
		(step_function)dlsym(shim, "OSDecrementAtomic64");
	swap_function compare_and_swap64 =
		(swap_function)dlsym(shim, "OSCompareAndSwap64");
	if (!add64 || !increment64 || !decrement64 || !compare_and_swap64)
		return 2;

	volatile int64_t value = 40;
	if (add64(2, &value) != 40 || value != 42)
		return 3;
	if (add64(-5, &value) != 42 || value != 37)
		return 4;
	if (increment64(&value) != 37 || value != 38)
		return 5;
	if (decrement64(&value) != 38 || value != 37)
		return 6;

	volatile uint64_t swap_value = 37;
	if (compare_and_swap64(36, 99, &swap_value) != 0 || swap_value != 37)
		return 7;
	if (compare_and_swap64(37, 99, &swap_value) != 1 || swap_value != 99)
		return 8;

	pthread_t threads[4];
	for (size_t index = 0; index < 4; index++) {
		if (pthread_create(&threads[index], NULL, add_repeatedly, NULL) != 0)
			return 9;
	}
	for (size_t index = 0; index < 4; index++)
		pthread_join(threads[index], NULL);
	if (shared_value != 40000)
		return 10;

	puts("osatomic64-ok");
	return 0;
}
