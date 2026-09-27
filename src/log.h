#ifndef MACHGATE_LOG_H
#define MACHGATE_LOG_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern int machgate_verbose;

static inline const char* machgate_getenv_compat(const char* name,
                                                 const char* legacy_name)
{
	const char* value = getenv(name);
	if (value)
		return value;
	return getenv(legacy_name);
}

static inline int machgate_env_truthy(const char* value)
{
	return value && value[0] && strcmp(value, "0") != 0 &&
	       strcmp(value, "false") != 0 && strcmp(value, "FALSE") != 0 &&
	       strcmp(value, "no") != 0 && strcmp(value, "NO") != 0;
}

static inline int machgate_log_startup_enabled(void)
{
	return machgate_verbose ||
	       machgate_env_truthy(machgate_getenv_compat("MACHGATE_VERBOSE",
	                                                  "MACHISMO_VERBOSE")) ||
	       machgate_env_truthy(machgate_getenv_compat("MACHGATE_LOG_STARTUP",
	                                                  "MACHISMO_LOG_STARTUP"));
}

static inline void machgate_log_startup(const char* format, ...)
{
	va_list args;

	if (!machgate_log_startup_enabled())
		return;

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}

static inline uint64_t machgate_phase_now_ms(void)
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (uint64_t)now.tv_sec * 1000 + (uint64_t)now.tv_nsec / 1000000;
}

static inline void machgate_phase_log(const char* phase, uint64_t start_ms)
{
	uint64_t elapsed_ms = machgate_phase_now_ms() - start_ms;
	machgate_log_startup("%s: %llu ms\n", phase,
	                     (unsigned long long)elapsed_ms);
}

#endif
