#include <asm/hwcap.h>
#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/auxv.h>

typedef int (*sysctl_function)(const char*, void*, size_t*, const void*, size_t);

int main(int argc, char** argv)
{
	assert(argc == 2);
	void* library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
	assert(library);
	sysctl_function read_feature = (sysctl_function)dlsym(library, "sysctlbyname");
	assert(read_feature);
	const struct {
		const char* name;
		unsigned long required;
	} features[] = {
		{"hw.optional.arm.FEAT_FP16", HWCAP_FPHP | HWCAP_ASIMDHP},
		{"hw.optional.neon_fp16", HWCAP_FPHP | HWCAP_ASIMDHP},
		{"hw.optional.arm.FEAT_DotProd", HWCAP_ASIMDDP},
		{"hw.optional.arm.FEAT_FHM", HWCAP_ASIMDFHM},
		{"hw.optional.armv8_2_fhm", HWCAP_ASIMDFHM},
		{"hw.optional.arm.AdvSIMD", HWCAP_ASIMD},
		{"hw.optional.neon", HWCAP_ASIMD},
	};
	unsigned long capabilities = getauxval(AT_HWCAP);
	for (size_t index = 0; index < sizeof(features) / sizeof(features[0]); index++) {
		size_t size = 0;
		assert(read_feature(features[index].name, NULL, &size, NULL, 0) == 0);
		assert(size == sizeof(int32_t));
		uint32_t output[2] = {0xa5a5a5a5, 0xa5a5a5a5};
		size = sizeof(output);
		assert(read_feature(features[index].name, output, &size, NULL, 0) == 0);
		assert(size == sizeof(int32_t));
		assert(output[0] == ((capabilities & features[index].required) == features[index].required));
		assert(output[1] == 0xa5a5a5a5);
		size = 1;
		output[0] = 0xa5a5a5a5;
		assert(read_feature(features[index].name, output, &size, NULL, 0) == -1);
		assert(errno == ENOMEM && size == sizeof(int32_t));
		assert(output[0] == 0xa5a5a5a5);
		assert(read_feature(features[index].name, NULL, &size, output, sizeof(int32_t)) == -1);
		assert(errno == EPERM);
	}
	size_t size = sizeof(int32_t);
	int32_t result = 42;
	assert(read_feature("hw.optional.arm.FEAT_NotAnArmFeature", &result, &size, NULL, 0) == -1);
	assert(errno == ENOENT && result == 42);
	puts("ARM feature sysctls match native capabilities, Darwin integer widths and read-only behavior");
	return 0;
}
