struct darwin_attr { long signature; char opaque[56]; };
typedef unsigned long thread_t;
extern int pthread_attr_init(struct darwin_attr*);
extern int pthread_attr_destroy(struct darwin_attr*);
extern int pthread_attr_setstacksize(struct darwin_attr*, unsigned long);
extern int pthread_attr_getstacksize(struct darwin_attr*, unsigned long*);
extern int pthread_attr_setschedpolicy(struct darwin_attr*, int);
extern int pthread_attr_getschedpolicy(struct darwin_attr*, int*);
extern int pthread_create(thread_t*, struct darwin_attr*, void* (*)(void*), void*);
extern int pthread_join(thread_t, void**);

static void* worker(void* argument)
{
	volatile char storage[4096];
	storage[0] = 17;
	storage[4095] = 23;
	return storage[0] + storage[4095] == 40 ? argument : (void*)0;
}

int main(void)
{
	struct darwin_attr attr;
	if (pthread_attr_init(&attr))
		return 41;
	if (pthread_attr_setstacksize(&attr, 114688))
		return 42;
	unsigned long size = 0;
	if (pthread_attr_getstacksize(&attr, &size) || size != 114688)
		return 43;
	if (pthread_attr_setstacksize(&attr, 8192) != 22 ||
	    pthread_attr_setstacksize(&attr, 16385) != 22)
		return 44;
	int policy = 0;
	if (pthread_attr_setschedpolicy(&attr, 4) ||
	    pthread_attr_getschedpolicy(&attr, &policy) || policy != 4 ||
	    pthread_attr_setschedpolicy(&attr, 2) ||
	    pthread_attr_getschedpolicy(&attr, &policy) || policy != 2 ||
	    pthread_attr_setschedpolicy(&attr, 3) != 22 ||
	    pthread_attr_setschedpolicy(&attr, 1))
		return 47;
	thread_t thread;
	void* result = (void*)0;
	if (pthread_create(&thread, &attr, worker, &attr) ||
	    pthread_join(thread, &result) || result != &attr)
		return 45;
	if (pthread_attr_destroy(&attr))
		return 46;
	return 0;
}
