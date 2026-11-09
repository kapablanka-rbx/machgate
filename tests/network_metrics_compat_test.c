#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern int sysctl(const int*, unsigned int, void*, size_t*, const void*, size_t);

static void check_unavailable_metric(int selector)
{
	int name[] = {4, 2, 17, selector};
	unsigned char output[256];
	memset(output, 0xa5, sizeof(output));
	size_t size = sizeof(output);
	assert(sysctl(name, 4, output, &size, NULL, 0) == -1);
	assert(errno == ENOENT && size == sizeof(output));
	for (size_t index = 0; index < sizeof(output); index++)
		assert(output[index] == 0xa5);
	size = 0;
	assert(sysctl(name, 4, NULL, &size, NULL, 0) == -1);
	assert(errno == ENOENT && size == 0);
	assert(sysctl(name, 4, NULL, &size, output, 1) == -1);
	assert(errno == EPERM);
}

int main(void)
{
	check_unavailable_metric(2);
	check_unavailable_metric(5);
	int name[] = {6, 3};
	uint32_t result = 0;
	size_t size = sizeof(result);
	assert(sysctl(name, 2, &result, &size, NULL, 0) == 0);
	assert(size == sizeof(result) && result > 0);
	return 0;
}
