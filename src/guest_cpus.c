#include "guest_cpus.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define IOS_DEFAULT_CPU_COUNT 6

/*
 * The guest platform is a load-time fact: the binary's LC_BUILD_VERSION
 * declares it and the loader records it once before guest execution. It never
 * changes after being set — one machgate process runs one guest, and the
 * guest's mode is that guest's identity. This is what lets a single host run
 * iOS and macOS guests side by side: each process reads its own guest's mode.
 */

static int detected_platform = 0;

void machgate_set_guest_platform(int macho_platform)
{
	if (detected_platform == 0)
		detected_platform = macho_platform;
}

int machgate_guest_platform(void)
{
	return detected_platform;
}

const char* machgate_guest_platform_name(void)
{
	switch (detected_platform) {
	case 2:
		return "ios";
	case 1:
		return "macos";
	case 7:
		return "ios-simulator";
	default:
		return "host";
	}
}

int guest_cpu_count(void)
{
	static int cached_result = -1;
	if (cached_result > 0)
		return cached_result;

	int result = 0;
	const char* override = getenv("MACHGATE_GUEST_NCPU");
	if (override && *override) {
		int parsed = atoi(override);
		if (parsed >= 1 && parsed <= 255)
			result = parsed;
	}

	if (!result && (detected_platform == 2 || detected_platform == 7))
		result = IOS_DEFAULT_CPU_COUNT;

	if (!result) {
		long online = sysconf(_SC_NPROCESSORS_ONLN);
		result = (online >= 1) ? (int)online : 1;
	}

	if (result > 255)
		result = 255;

	cached_result = result;
	return result;
}
