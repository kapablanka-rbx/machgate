#include "guest_cpus.h"

#include <stdlib.h>
#include <unistd.h>

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

	if (!result) {
		long online = sysconf(_SC_NPROCESSORS_ONLN);
		result = (online >= 1) ? (int)online : 1;
	}

	if (result > 255)
		result = 255;

	cached_result = result;
	return result;
}
