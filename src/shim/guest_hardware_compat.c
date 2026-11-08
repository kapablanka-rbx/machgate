#include "guest_hardware_compat.h"
#include "host_facts.h"

#include <asm/hwcap.h>
#include <stddef.h>
#include <string.h>
#include <sys/auxv.h>

int machgate_shim_guest_ncpu;

int machgate_guest_cpu_count(void)
{
	return machgate_shim_guest_ncpu > 0
		? machgate_shim_guest_ncpu : machgate_host_cpu_count();
}

int machgate_guest_arm_feature(const char* name, int* result)
{
	static const struct {
		const char* name;
		const char* legacy_name;
		unsigned long required;
	} features[] = {
		{"AdvSIMD", "neon", HWCAP_ASIMD},
		{"FEAT_FP16", "neon_fp16", HWCAP_FPHP | HWCAP_ASIMDHP},
		{"FEAT_DotProd", NULL, HWCAP_ASIMDDP},
		{"FEAT_FHM", "armv8_2_fhm", HWCAP_ASIMDFHM},
		{"FEAT_CRC32", "armv8_crc32", HWCAP_CRC32},
		{"FEAT_LSE", "armv8_1_atomics", HWCAP_ATOMICS},
		{"FEAT_SHA512", "armv8_2_sha512", HWCAP_SHA512},
		{"FEAT_SHA3", "armv8_2_sha3", HWCAP_SHA3},
		{"FEAT_FCMA", "armv8_3_compnum", HWCAP_FCMA},
		{"FEAT_AES", NULL, HWCAP_AES},
		{"FEAT_PMULL", NULL, HWCAP_PMULL},
		{"FEAT_SHA1", NULL, HWCAP_SHA1},
		{"FEAT_SHA256", NULL, HWCAP_SHA2},
		{"FEAT_RDM", NULL, HWCAP_ASIMDRDM},
		{"FEAT_LRCPC", NULL, HWCAP_LRCPC},
		{"FEAT_LRCPC2", NULL, HWCAP_ILRCPC},
		{"FEAT_JSCVT", NULL, HWCAP_JSCVT},
		{"FEAT_DPB", NULL, HWCAP_DCPOP},
	};
	const char prefix[] = "hw.optional.";
	if (!name || strncmp(name, prefix, sizeof(prefix) - 1) != 0)
		return 0;
	name += sizeof(prefix) - 1;
	int canonical = strncmp(name, "arm.", 4) == 0;
	if (canonical)
		name += 4;
	for (size_t index = 0; index < sizeof(features) / sizeof(features[0]); index++) {
		const char* candidate = canonical ? features[index].name : features[index].legacy_name;
		if (!candidate || strcmp(candidate, name) != 0)
			continue;
		unsigned long capabilities = getauxval(AT_HWCAP);
		*result = (capabilities & features[index].required) == features[index].required;
		return 1;
	}
	return 0;
}
