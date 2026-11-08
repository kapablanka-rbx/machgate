#ifndef MACHGATE_EVENT_TIMER_H
#define MACHGATE_EVENT_TIMER_H

#include <stdint.h>

int machgate_event_timer_create(uint64_t deadline_ns, uint64_t interval_ns,
                                int absolute_realtime);
int64_t machgate_event_timer_consume(int descriptor);
void machgate_event_timer_close(int descriptor);

#endif
