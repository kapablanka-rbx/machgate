#include "ports/event_timer.h"

#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <sys/timerfd.h>
#include <time.h>

static struct itimerspec armed_timer;
static int timer_clock;
static int timer_flags;

long __wrap_syscall(long number, ...)
{
	va_list arguments;
	va_start(arguments, number);
	if (number == SYS_clock_gettime) {
		assert(va_arg(arguments, int) == CLOCK_REALTIME);
		struct timespec* result = va_arg(arguments, struct timespec*);
		*result = (struct timespec){ .tv_sec = 100, .tv_nsec = 250000000 };
	} else if (number == SYS_timerfd_create) {
		timer_clock = va_arg(arguments, int);
		va_end(arguments);
		return 17;
	} else if (number == SYS_timerfd_settime) {
		assert(va_arg(arguments, int) == 17);
		timer_flags = va_arg(arguments, int);
		armed_timer = *va_arg(arguments, const struct itimerspec*);
	} else {
		assert(number == SYS_close);
	}
	va_end(arguments);
	return 0;
}

int main(void)
{
	assert(machgate_event_timer_create(UINT64_C(103000000000), 0, 1) == 17);
	assert(timer_clock == CLOCK_MONOTONIC && timer_flags == 0);
	assert(armed_timer.it_value.tv_sec == 2);
	assert(armed_timer.it_value.tv_nsec == 750000000);
	assert(armed_timer.it_interval.tv_sec == 0);
	assert(armed_timer.it_interval.tv_nsec == 0);
	assert(machgate_event_timer_create(UINT64_C(99000000000), 0, 1) == 17);
	assert(armed_timer.it_value.tv_sec == 0);
	assert(armed_timer.it_value.tv_nsec == 1);
	return 0;
}
