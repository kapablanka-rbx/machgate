typedef unsigned long darwin_pthread_key_t;

extern int pthread_key_create(darwin_pthread_key_t*, void (*)(void*));
extern int pthread_key_delete(darwin_pthread_key_t);
extern int pthread_setspecific(darwin_pthread_key_t, const void*);
extern void* pthread_getspecific(darwin_pthread_key_t);

int main(void)
{
	struct {
		darwin_pthread_key_t key;
		unsigned long sentinel;
	} output = {~0UL, 0x123456789abcdef0UL};
	if (pthread_key_create(&output.key, 0) != 0)
		return 41;
	if (output.key > 511 || output.sentinel != 0x123456789abcdef0UL)
		return 42;
	if (pthread_setspecific(output.key, &output) != 0 ||
	    pthread_getspecific(output.key) != &output)
		return 43;
	darwin_pthread_key_t invalid = output.key | (1UL << 32);
	if (pthread_getspecific(invalid) != 0 ||
	    pthread_setspecific(invalid, &output) != 22)
		return 44;
	if (pthread_key_delete(invalid) != 22)
		return 48;
	if (pthread_key_delete(output.key) != 0 || pthread_getspecific(output.key) != 0)
		return 49;
	for (int index = 0; index < 1000; index++) {
		if (pthread_key_create(&output.key, 0) != 0 ||
		    pthread_getspecific(output.key) != 0 ||
		    pthread_setspecific(output.key, &output) != 0 ||
		    pthread_key_delete(output.key) != 0)
			return 50;
	}
	for (int index = 0; index < 385; index++) {
		output.key = ~0UL;
		int result = pthread_key_create(&output.key, 0);
		if (result != 0)
			return result == 35 && output.key == ~0UL ? 0 : 45;
		if (output.key > 511)
			return 46;
	}
	return 47;
}
