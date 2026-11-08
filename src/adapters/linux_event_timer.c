#include "ports/event_timer.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <sys/timerfd.h>
#include <unistd.h>

int machgate_event_timer_create(uint64_t deadline_ns, uint64_t interval_ns,
                                int absolute_realtime)
{
	if (absolute_realtime) {
		struct timespec now;
		if (syscall(SYS_clock_gettime, CLOCK_REALTIME, &now) < 0)
			return -1;
		uint64_t now_ns = (uint64_t)now.tv_sec * 1000000000 + now.tv_nsec;
		deadline_ns = deadline_ns > now_ns ? deadline_ns - now_ns : 1;
	}
	int result = (int)syscall(SYS_timerfd_create, CLOCK_MONOTONIC,
		TFD_NONBLOCK | TFD_CLOEXEC);
	if (result < 0)
		return result;
	struct itimerspec specification = {
		.it_value = { deadline_ns / 1000000000, deadline_ns % 1000000000 },
		.it_interval = { interval_ns / 1000000000, interval_ns % 1000000000 },
	};
	if (syscall(SYS_timerfd_settime, result, 0,
	            &specification, NULL) == 0)
		return result;
	int saved_errno = errno;
	syscall(SYS_close, result);
	errno = saved_errno;
	return -1;
}

int64_t machgate_event_timer_consume(int descriptor)
{
	uint64_t expirations = 0;
	if (syscall(SYS_read, descriptor, &expirations, sizeof(expirations)) !=
	    sizeof(expirations))
		return 0;
	return expirations > INT64_MAX ? INT64_MAX : (int64_t)expirations;
}

void machgate_event_timer_close(int descriptor)
{
	syscall(SYS_close, descriptor);
}
