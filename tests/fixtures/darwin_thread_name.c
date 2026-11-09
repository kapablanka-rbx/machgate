typedef unsigned long size_t;
typedef unsigned long pthread_t;
extern int* __error(void);
extern pthread_t pthread_self(void);
extern int pthread_setname_np(const char* name);
extern int pthread_getname_np(pthread_t thread, char* name, size_t capacity);

static int equal(const char* left, const char* right)
{
	while (*left && *left == *right) { left++; right++; }
	return *left == *right;
}

int main(void)
{
	char output[65];
	const char* name = "this-is-a-very-long-thread-name";
	if (pthread_setname_np(name) || pthread_getname_np(pthread_self(), output, sizeof(output)))
		return 1;
	if (!equal(output, name))
		return 2;
	if (pthread_getname_np(pthread_self(), output, 5) || !equal(output, "this"))
		return 3;
	output[0] = 'x';
	if (pthread_getname_np(pthread_self(), output, 0) || output[0] != 'x')
		return 4;
	char boundary[65];
	for (size_t index = 0; index < 64; index++) boundary[index] = 'a';
	boundary[63] = 0;
	if (pthread_setname_np(boundary) || pthread_getname_np(pthread_self(), output, sizeof(output)) || !equal(output, boundary))
		return 5;
	boundary[63] = 'a';
	boundary[64] = 0;
	if (pthread_setname_np(boundary) != -1 || *__error() != 63)
		return 6;
	if (pthread_getname_np(pthread_self(), output, sizeof(output)) || output[63] != 0)
		return 7;
	if (pthread_setname_np(0) || pthread_getname_np(pthread_self(), output, sizeof(output)) || output[0])
		return 8;
	return 0;
}
