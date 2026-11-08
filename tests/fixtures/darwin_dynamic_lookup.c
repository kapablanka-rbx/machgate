extern void* dlsym(void* handle, const char* symbol);
extern unsigned long mach_absolute_time(void);
extern int pthread_key_create(unsigned long* key, void (*destructor)(void*));
extern int getrlimit(int resource, void* limits);

__attribute__((noreturn))
static void exit_guest(int status)
{
	register unsigned long argument __asm__("x0") = (unsigned long)status;
	register unsigned long number __asm__("x16") = 1;
	__asm__ volatile("svc #0x80" : "+r"(argument) : "r"(number) : "memory");
	__builtin_unreachable();
}

int main(void)
{
	const long handles[] = {-1, -2, -3, -5};
	for (unsigned int index = 0; index < 4; index++) {
		void* handle = (void*)handles[index];
		unsigned long (*clock_function)(void) =
			dlsym(handle, "mach_absolute_time");
		if (!clock_function || clock_function() == 0)
			exit_guest(10 + (int)index);
		if (dlsym(handle, "pthread_key_create") != (void*)pthread_key_create)
			exit_guest(20 + (int)index);
		if (dlsym(handle, "getrlimit") != (void*)getrlimit)
			exit_guest(30 + (int)index);
		if (dlsym(handle, "dlsym") != (void*)dlsym)
			exit_guest(40 + (int)index);
		if (dlsym(handle, "machgate_fixture_nonexistent_symbol"))
			exit_guest(50 + (int)index);
		int (*process_id_function)(void) = dlsym(handle, "getpid");
		if (!process_id_function || process_id_function() <= 0)
			exit_guest(60 + (int)index);
	}
	void* (*pool_push)(void) = dlsym((void*)-1L, "objc_autoreleasePoolPush");
	void (*pool_pop)(void*) = dlsym((void*)-1L, "objc_autoreleasePoolPop");
	if (!pool_push || !pool_pop)
		exit_guest(79);
	void* pools[32];
	for (unsigned int index = 0; index < 32; index++) {
		pools[index] = pool_push();
		if (!pools[index])
			exit_guest(80);
		for (unsigned int earlier = 0; earlier < index; earlier++) {
			if (pools[earlier] == pools[index])
				exit_guest(81);
		}
	}
	for (unsigned int index = 32; index > 0; index--)
		pool_pop(pools[index - 1]);
	exit_guest(0);
}
