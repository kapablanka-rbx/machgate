#include "guest_cpus.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Guests size thread pools and parallel work from the reported CPU count.
 * On the 64-core devspace that made every resident guest daemon spawn
 * 64-thread pools, oversubscribing the host. MACHGATE_GUEST_NCPU caps the
 * count; when unset, iOS binaries default to iPhone-class hardware (6 cores)
 * because the guest was built for that device class, and everything else
 * falls back to the host count.
 */

#define IOS_DEFAULT_CPU_COUNT 6

static int guest_binary_is_ios(void)
{
	const char* ios_hint = getenv("MACHGATE_GUEST_IOS");
	if (ios_hint && *ios_hint) {
		return ios_hint[0] == '1' ||
		       strcmp(ios_hint, "on") == 0 ||
		       strcmp(ios_hint, "true") == 0;
	}
	const char* platform = getenv("MACHGATE_GUEST_PLATFORM");
	return platform && strcmp(platform, "ios") == 0;
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

	if (!result && guest_binary_is_ios())
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
