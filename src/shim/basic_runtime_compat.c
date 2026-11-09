#define _GNU_SOURCE

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>

int memset_s(void* destination, size_t capacity, int value, size_t length)
{
	if (!destination)
		return 22;
	if (capacity > (SIZE_MAX >> 1))
		return 7;

	int result = 0;
	if (length > (SIZE_MAX >> 1)) {
		result = 7;
		length = capacity;
	} else if (length > capacity) {
		result = 84;
		length = capacity;
	}

	volatile unsigned char* bytes = destination;
	for (size_t index = 0; index < length; index++)
		bytes[index] = (unsigned char)value;
	return result;
}

FILE* __stdinp;
FILE* __stdoutp;
FILE* __stderrp;

struct machgate_float_pair {
	float sine;
	float cosine;
};

struct machgate_double_pair {
	double sine;
	double cosine;
};

__attribute__((constructor))
static void publish_standard_streams(void)
{
	FILE** destinations[] = {&__stdinp, &__stdoutp, &__stderrp};
	FILE* sources[] = {stdin, stdout, stderr};

	for (size_t stream_index = 0; stream_index < 3; stream_index++)
		*destinations[stream_index] = sources[stream_index];
}

int* __error(void)
{
	return __errno_location();
}

void __chkstk_darwin(void)
{
}

struct machgate_float_pair __sincosf_stret(float angle)
{
	struct machgate_float_pair result;
	result.cosine = cosf(angle);
	result.sine = sinf(angle);
	return result;
}

struct machgate_double_pair __sincos_stret(double angle)
{
	struct machgate_double_pair result;
	result.cosine = cos(angle);
	result.sine = sin(angle);
	return result;
}

struct machgate_float_pair __sincospif_stret(float turns)
{
	float angle = turns * (float)M_PI;
	return __sincosf_stret(angle);
}

struct machgate_double_pair __sincospi_stret(double turns)
{
	double angle = turns * M_PI;
	return __sincos_stret(angle);
}

double __exp10(double exponent)
{
	return exp(exponent * M_LN10);
}

float __exp10f(float exponent)
{
	return expf(exponent * (float)M_LN10);
}
