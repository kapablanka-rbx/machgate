typedef struct {
	long signature;
	char opaque[8];
} darwin_mutex_attributes;

typedef struct {
	long signature;
	char opaque[56];
} darwin_mutex;

extern int pthread_mutexattr_init(darwin_mutex_attributes*);
extern int pthread_mutexattr_destroy(darwin_mutex_attributes*);
extern int pthread_mutexattr_settype(darwin_mutex_attributes*, int);
extern int pthread_mutexattr_gettype(const darwin_mutex_attributes*, int*);
extern int pthread_mutex_init(darwin_mutex*, const darwin_mutex_attributes*);
extern int pthread_mutex_destroy(darwin_mutex*);
extern int pthread_mutex_lock(darwin_mutex*);
extern int pthread_mutex_trylock(darwin_mutex*);
extern int pthread_mutex_unlock(darwin_mutex*);

_Static_assert(sizeof(darwin_mutex_attributes) == 16, "Darwin LP64 mutex attributes");
_Static_assert(sizeof(darwin_mutex) == 64, "Darwin LP64 mutex");

int main(int argc, char** argv)
{
	if (argc != 2)
		return 1;
	int type = argv[1][0] - '0';
	darwin_mutex_attributes attributes;
	darwin_mutex mutex;
	if (pthread_mutexattr_init(&attributes) != 0)
		return 2;
	int result = pthread_mutexattr_settype(&attributes, type);
	if (type == 3)
		return result == 22 && pthread_mutexattr_destroy(&attributes) == 0 ? 0 : 3;
	if (result != 0)
		return 4;
	int observed = -1;
	if (pthread_mutexattr_gettype(&attributes, &observed) != 0 || observed != type)
		return 5;
	if (pthread_mutex_init(&mutex, &attributes) != 0)
		return 6;
	if (pthread_mutexattr_destroy(&attributes) != 0 || pthread_mutex_lock(&mutex) != 0)
		return 7;
	result = pthread_mutex_trylock(&mutex);
	if (result != (type == 2 ? 0 : 16))
		return 8;
	if (type == 1 && pthread_mutex_lock(&mutex) != 11)
		return 9;
	if (type == 2 && pthread_mutex_unlock(&mutex) != 0)
		return 10;
	if (pthread_mutex_unlock(&mutex) != 0 || pthread_mutex_destroy(&mutex) != 0)
		return 11;
	return 0;
}
