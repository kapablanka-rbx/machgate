#include "kqueue_compat.h"
#include "ports/event_timer.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>

#define DARWIN_EVFILT_READ (-1)
#define DARWIN_EVFILT_WRITE (-2)
#define DARWIN_EVFILT_USER (-10)
#define DARWIN_EVFILT_TIMER (-7)
#define DARWIN_EV_ADD 0x0001
#define DARWIN_EV_ENABLE 0x0004
#define DARWIN_EV_ONESHOT 0x0010
#define DARWIN_NOTE_ABSOLUTE 0x0008
#define DARWIN_EV_DELETE 0x0002
#define DARWIN_EV_DISABLE 0x0008
#define DARWIN_EV_CLEAR 0x0020
#define DARWIN_EV_RECEIPT 0x0040
#define DARWIN_EV_ERROR 0x4000
#define DARWIN_EV_EOF 0x8000
#define DARWIN_NOTE_TRIGGER 0x01000000
#define KQUEUE_REGISTRATION_COUNT 1024
#define KQUEUE_QUEUE_COUNT 1024
#define KQUEUE_POLL_COUNT 64

struct shim_kqueue_registration {
	int used;
	int kq;
	uint64_t ident;
	int16_t filter;
	uint16_t flags;
	uint32_t fflags;
	int64_t data;
	void* udata;
	int timer_descriptor;
	int triggered;
	int ready;
	uint64_t generation;
};

struct shim_kqueue_poll_registration {
	struct shim_kqueue_registration* registration;
	struct shim_kqueue_registration snapshot;
};

struct shim_kqueue_alias {
	int fd;
	int identity;
};

struct shim_kqueue_queue {
	int used;
	int identity;
	unsigned int references;
};

static pthread_mutex_t kqueue_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct shim_kqueue_registration kqueue_registrations[KQUEUE_REGISTRATION_COUNT];
static struct shim_kqueue_queue kqueue_queues[KQUEUE_QUEUE_COUNT];
static struct shim_kqueue_alias* kqueue_aliases;
static size_t kqueue_alias_count;
static size_t kqueue_alias_capacity;
static uint64_t kqueue_next_generation = 1;

static struct shim_kqueue_queue* find_kqueue_queue_unlocked(int identity)
{
	for (int index = 0; index < KQUEUE_QUEUE_COUNT; index++) {
		if (kqueue_queues[index].used &&
		    kqueue_queues[index].identity == identity)
			return &kqueue_queues[index];
	}
	return NULL;
}

static struct shim_kqueue_alias* find_kqueue_alias_unlocked(int fd)
{
	for (size_t index = 0; index < kqueue_alias_count; index++) {
		if (kqueue_aliases[index].fd == fd)
			return &kqueue_aliases[index];
	}
	return NULL;
}

static int resolve_kqueue_fd_unlocked(int kq)
{
	struct shim_kqueue_alias* alias = find_kqueue_alias_unlocked(kq);

	return alias ? alias->identity : kq;
}

static int allocate_kqueue_queue_unlocked(void)
{
	for (int index = 0; index < KQUEUE_QUEUE_COUNT; index++) {
		if (kqueue_queues[index].used)
			continue;
		kqueue_queues[index].used = 1;
		kqueue_queues[index].identity = index + 1;
		kqueue_queues[index].references = 0;
		return index + 1;
	}
	return -1;
}

static int grow_kqueue_aliases_unlocked(void)
{
	size_t capacity = kqueue_alias_capacity ? kqueue_alias_capacity * 2 : 16;
	struct shim_kqueue_alias* aliases = realloc(
		kqueue_aliases, capacity * sizeof(*aliases));

	if (!aliases)
		return -1;
	kqueue_aliases = aliases;
	kqueue_alias_capacity = capacity;
	return 0;
}

static int remember_kqueue_fd_unlocked(int fd, int identity)
{
	struct shim_kqueue_alias* alias = find_kqueue_alias_unlocked(fd);
	struct shim_kqueue_queue* queue;

	if (alias) {
		queue = find_kqueue_queue_unlocked(alias->identity);
		if (queue && queue->references > 0)
			queue->references--;
	} else {
		if (kqueue_alias_count == kqueue_alias_capacity &&
		    grow_kqueue_aliases_unlocked() < 0)
			return -1;
		alias = &kqueue_aliases[kqueue_alias_count++];
	}
	alias->fd = fd;
	alias->identity = identity;
	queue = find_kqueue_queue_unlocked(identity);
	if (queue)
		queue->references++;
	return 0;
}

static void discard_kqueue_alias_unlocked(int fd)
{
	for (size_t index = 0; index < kqueue_alias_count; index++) {
		if (kqueue_aliases[index].fd != fd)
			continue;
		kqueue_aliases[index] = kqueue_aliases[kqueue_alias_count - 1];
		kqueue_alias_count--;
		return;
	}
}

static void discard_kqueue_registration(struct shim_kqueue_registration* registration)
{
	if (registration->used && registration->filter == DARWIN_EVFILT_TIMER)
		machgate_event_timer_close(registration->timer_descriptor);
	memset(registration, 0, sizeof(*registration));
}

static void destroy_kqueue_registrations_unlocked(int identity)
{
	for (int index = 0; index < KQUEUE_REGISTRATION_COUNT; index++) {
		if (kqueue_registrations[index].used &&
		    kqueue_registrations[index].kq == identity)
			discard_kqueue_registration(&kqueue_registrations[index]);
	}
}

int machgate_kqueue_open(void)
{
	int result = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);

	if (result < 0)
		return result;

	pthread_mutex_lock(&kqueue_mutex);
	int identity = allocate_kqueue_queue_unlocked();
	if (identity < 0 || remember_kqueue_fd_unlocked(result, identity) < 0) {
		struct shim_kqueue_queue* queue = find_kqueue_queue_unlocked(identity);
		if (queue)
			memset(queue, 0, sizeof(*queue));
		discard_kqueue_alias_unlocked(result);
		pthread_mutex_unlock(&kqueue_mutex);
		syscall(SYS_close, result);
		errno = ENOMEM;
		return -1;
	}
	pthread_mutex_unlock(&kqueue_mutex);
	return result;
}

int machgate_kqueue_fd_is_queue(int fd)
{
	int result;

	pthread_mutex_lock(&kqueue_mutex);
	result = find_kqueue_alias_unlocked(fd) != NULL;
	pthread_mutex_unlock(&kqueue_mutex);
	return result;
}

void machgate_kqueue_fd_did_duplicate(int from_fd, int to_fd)
{
	if (from_fd == to_fd)
		return;

	pthread_mutex_lock(&kqueue_mutex);
	struct shim_kqueue_alias* alias = find_kqueue_alias_unlocked(from_fd);
	if (alias && remember_kqueue_fd_unlocked(to_fd, alias->identity) < 0)
		discard_kqueue_alias_unlocked(to_fd);
	pthread_mutex_unlock(&kqueue_mutex);
}

void machgate_kqueue_fd_did_close(int fd)
{
	pthread_mutex_lock(&kqueue_mutex);
	struct shim_kqueue_alias* alias = find_kqueue_alias_unlocked(fd);
	struct shim_kqueue_queue* queue = alias ?
	    find_kqueue_queue_unlocked(alias->identity) : NULL;

	if (queue && queue->references > 0)
		queue->references--;
	for (int index = 0; index < KQUEUE_REGISTRATION_COUNT; index++) {
		struct shim_kqueue_registration* registration =
			&kqueue_registrations[index];
		if (!registration->used)
			continue;
		if ((registration->filter == DARWIN_EVFILT_READ ||
		     registration->filter == DARWIN_EVFILT_WRITE) &&
		    (int)registration->ident == fd)
			memset(registration, 0, sizeof(*registration));
	}

	if (queue && queue->references == 0) {
		destroy_kqueue_registrations_unlocked(queue->identity);
		memset(queue, 0, sizeof(*queue));
	}
	discard_kqueue_alias_unlocked(fd);
	pthread_mutex_unlock(&kqueue_mutex);
}

static struct shim_kqueue_registration* find_kqueue_registration(
	int kq, uint64_t ident, int16_t filter)
{
	for (int index = 0; index < KQUEUE_REGISTRATION_COUNT; index++) {
		struct shim_kqueue_registration* registration =
			&kqueue_registrations[index];
		if (registration->used &&
		    registration->kq == resolve_kqueue_fd_unlocked(kq) &&
		    registration->ident == ident && registration->filter == filter)
			return registration;
	}
	return NULL;
}

static struct shim_kqueue_registration* alloc_kqueue_registration(void)
{
	for (int index = 0; index < KQUEUE_REGISTRATION_COUNT; index++) {
		if (!kqueue_registrations[index].used)
			return &kqueue_registrations[index];
	}
	return NULL;
}

static void ensure_kqueue_poll_fd_nonblocking(int fd)
{
	if (fd < 0)
		return;

	int flags = (int)syscall(SYS_fcntl, fd, F_GETFL, 0);
	if (flags < 0 || (flags & O_NONBLOCK))
		return;
	syscall(SYS_fcntl, fd, F_SETFL, (unsigned long)(flags | O_NONBLOCK));
}

static int create_kqueue_timer(const struct machgate_darwin_kevent* change)
{
	uint64_t multiplier;
	switch (change->fflags & 7) {
	case 0: multiplier = 1000000; break;
	case 1: multiplier = 1000000000; break;
	case 2: multiplier = 1000; break;
	case 4: multiplier = 1; break;
	default: errno = EINVAL; return -1;
	}
	if (change->fflags & ~0x7fu) {
		errno = ENOTSUP;
		return -1;
	}
	int absolute = (change->fflags & DARWIN_NOTE_ABSOLUTE) != 0;
	if (!absolute && change->data <= 0)
		return machgate_event_timer_create(1, 0, 0);
	uint64_t duration = (uint64_t)change->data;
	if (duration > INT64_MAX / multiplier) {
		errno = ERANGE;
		return -1;
	}
	duration *= multiplier;
	uint64_t interval = absolute || (change->flags & DARWIN_EV_ONESHOT) ?
		0 : duration;
	return machgate_event_timer_create(duration, interval, absolute);
}

static int update_kqueue_registration(
	int kq, const struct machgate_darwin_kevent* change)
{
	struct shim_kqueue_registration* registration =
		find_kqueue_registration(kq, change->ident, change->filter);

	if (change->flags & DARWIN_EV_DELETE) {
		if (registration)
			discard_kqueue_registration(registration);
		return 0;
	}
	if (change->filter != DARWIN_EVFILT_READ &&
	    change->filter != DARWIN_EVFILT_WRITE &&
	    change->filter != DARWIN_EVFILT_USER &&
	    change->filter != DARWIN_EVFILT_TIMER)
		return 0;
	if (registration && !(change->flags & DARWIN_EV_ADD) &&
	    change->filter == DARWIN_EVFILT_TIMER) {
		if (change->flags & DARWIN_EV_DISABLE)
			registration->flags |= DARWIN_EV_DISABLE;
		if (change->flags & DARWIN_EV_ENABLE)
			registration->flags &= ~DARWIN_EV_DISABLE;
		return 0;
	}
	if (change->filter == DARWIN_EVFILT_READ ||
	    change->filter == DARWIN_EVFILT_WRITE)
		ensure_kqueue_poll_fd_nonblocking((int)change->ident);
	if (!registration) {
		registration = alloc_kqueue_registration();
		if (!registration) {
			errno = ENOMEM;
			return -1;
		}
	}

	int timer_descriptor = -1;
	if (change->filter == DARWIN_EVFILT_TIMER) {
		timer_descriptor = create_kqueue_timer(change);
		if (timer_descriptor < 0)
			return -1;
	}
	discard_kqueue_registration(registration);
	registration->used = 1;
	registration->timer_descriptor = timer_descriptor;
	registration->kq = resolve_kqueue_fd_unlocked(kq);
	registration->ident = change->ident;
	registration->filter = change->filter;
	registration->flags = change->flags;
	if (change->filter == DARWIN_EVFILT_TIMER) {
		registration->flags |= DARWIN_EV_CLEAR;
		if (change->fflags & DARWIN_NOTE_ABSOLUTE)
			registration->flags |= DARWIN_EV_ONESHOT;
	}
	registration->fflags = change->fflags;
	registration->data = change->data;
	registration->udata = change->udata;
	registration->generation = kqueue_next_generation++;
	if (change->fflags & DARWIN_NOTE_TRIGGER)
		registration->triggered = 1;
	if (change->filter == DARWIN_EVFILT_USER &&
	    (change->fflags & DARWIN_NOTE_TRIGGER)) {
		uint64_t value = 1;
		ssize_t write_result = write(kq, &value, sizeof(value));
		(void)write_result;
	}
	return 0;
}

static int apply_kqueue_changes(
	int kq, const struct machgate_darwin_kevent* changelist, int nchanges)
{
	for (int index = 0; index < nchanges; index++) {
		if (update_kqueue_registration(kq, &changelist[index]) < 0)
			return -1;
	}
	return 0;
}

static int emit_kqueue_receipts(
	const struct machgate_darwin_kevent* changelist, int nchanges,
	struct machgate_darwin_kevent* eventlist, int nevents)
{
	int result = 0;

	for (int index = 0; index < nchanges && result < nevents; index++) {
		const struct machgate_darwin_kevent* change = &changelist[index];
		if (!(change->flags & DARWIN_EV_RECEIPT))
			continue;
		eventlist[result] = *change;
		eventlist[result].flags |= DARWIN_EV_ERROR;
		eventlist[result].data = 0;
		result++;
	}
	return result;
}

static int kevent_timeout_ms(const struct timespec* timeout)
{
	if (!timeout)
		return -1;
	if (timeout->tv_sec > INT32_MAX / 1000)
		return INT32_MAX;
	int result = (int)(timeout->tv_sec * 1000);
	result += (int)((timeout->tv_nsec + 999999L) / 1000000L);
	return result;
}

static int kevent_timeout_is_valid(const struct timespec* timeout)
{
	return !timeout || (timeout->tv_sec >= 0 && timeout->tv_nsec >= 0 &&
	                    timeout->tv_nsec < 1000000000L);
}

static short kqueue_poll_events(int16_t filter)
{
	if (filter == DARWIN_EVFILT_READ || filter == DARWIN_EVFILT_USER)
		return POLLIN;
	if (filter == DARWIN_EVFILT_WRITE)
		return POLLOUT;
	return 0;
}

static int collect_kqueue_pollfds(
	int kq, struct pollfd* pollfds,
	struct shim_kqueue_poll_registration* registrations, int max_events)
{
	int count = 0;

	for (int index = 0;
	     index < KQUEUE_REGISTRATION_COUNT && count < max_events; index++) {
		struct shim_kqueue_registration* registration =
			&kqueue_registrations[index];
		if (!registration->used ||
		    registration->kq != resolve_kqueue_fd_unlocked(kq) ||
		    (registration->flags & DARWIN_EV_DISABLE))
			continue;
		if (registration->filter == DARWIN_EVFILT_USER) {
			pollfds[count].fd = kq;
			pollfds[count].events = POLLIN;
		} else if (registration->filter == DARWIN_EVFILT_TIMER) {
			pollfds[count].fd = registration->timer_descriptor;
			pollfds[count].events = POLLIN;
		} else {
			pollfds[count].fd = (int)registration->ident;
			pollfds[count].events = kqueue_poll_events(registration->filter);
		}
		pollfds[count].revents = 0;
		registrations[count].registration = registration;
		registrations[count].snapshot = *registration;
		count++;
	}
	return count;
}

static int kqueue_registration_matches_snapshot(
	const struct shim_kqueue_registration* registration,
	const struct shim_kqueue_registration* snapshot)
{
	return registration && registration->used &&
	    registration->generation == snapshot->generation &&
	    registration->kq == snapshot->kq &&
	    registration->ident == snapshot->ident &&
	    registration->filter == snapshot->filter;
}

static int listening_socket_event_data(int fd)
{
	int accepting = 0;
	socklen_t length = sizeof(accepting);

	if (syscall(SYS_getsockopt, fd, SOL_SOCKET, SO_ACCEPTCONN,
	            &accepting, &length) == 0 && accepting)
		return 1;
	return 0;
}

static int64_t kqueue_event_data(
	const struct shim_kqueue_registration* registration, short revents)
{
	if (registration->filter == DARWIN_EVFILT_READ) {
		int available = 0;
		long ioctl_result = syscall(SYS_ioctl, (int)registration->ident,
		                            FIONREAD, &available);
		if (ioctl_result == 0)
			return available;
		return listening_socket_event_data((int)registration->ident);
	}
	if (registration->filter == DARWIN_EVFILT_WRITE) {
		if (revents & (POLLERR | POLLHUP | POLLNVAL))
			return 0;
		return 1;
	}
	return 0;
}

static int emit_kqueue_events(
	int kq, struct pollfd* pollfds,
	struct shim_kqueue_poll_registration* registrations, int pollfd_count,
	struct machgate_darwin_kevent* eventlist, int nevents)
{
	int result = 0;

	for (int index = 0; index < pollfd_count && result < nevents; index++) {
		struct shim_kqueue_registration* live_registration =
			registrations[index].registration;
		struct shim_kqueue_registration* registration =
			&registrations[index].snapshot;
		int live_matches = kqueue_registration_matches_snapshot(
			live_registration, registration);
		short revents = pollfds[index].revents;
		int ready = revents != 0;

		if (!live_matches)
			continue;
		if (!ready && registration->filter != DARWIN_EVFILT_USER) {
			live_registration->ready = 0;
			continue;
		}
		if (!ready && !registration->triggered)
			continue;
		if ((registration->flags & DARWIN_EV_CLEAR) &&
		    registration->filter == DARWIN_EVFILT_WRITE) {
			if (live_registration->ready)
				continue;
			live_registration->ready = 1;
		}

		int64_t event_data = registration->filter == DARWIN_EVFILT_TIMER ?
			machgate_event_timer_consume(registration->timer_descriptor) :
			kqueue_event_data(registration, revents);
		if (registration->filter == DARWIN_EVFILT_TIMER && event_data == 0)
			continue;
		eventlist[result].ident = registration->ident;
		eventlist[result].filter = registration->filter;
		eventlist[result].flags = registration->flags;
		eventlist[result].fflags = registration->fflags;
		eventlist[result].data = event_data;
		eventlist[result].udata = registration->udata;
		if (revents & (POLLHUP | POLLERR | POLLNVAL))
			eventlist[result].flags |= DARWIN_EV_EOF;
		if (registration->filter == DARWIN_EVFILT_USER) {
			uint64_t value;
			while (read(kq, &value, sizeof(value)) > 0) {
			}
			live_registration->triggered = 0;
		}
		if (registration->flags & DARWIN_EV_ONESHOT)
			discard_kqueue_registration(live_registration);
		result++;
	}
	return result;
}

static void sleep_for_kevent_timeout(const struct timespec* timeout)
{
	if (!timeout || (timeout->tv_sec == 0 && timeout->tv_nsec == 0))
		return;

	struct timespec remaining = *timeout;
	while (syscall(SYS_nanosleep, &remaining, &remaining) < 0 &&
	       errno == EINTR) {
	}
}

int machgate_kqueue_event(
	int kq, const struct machgate_darwin_kevent* changelist, int nchanges,
	struct machgate_darwin_kevent* eventlist, int nevents,
	const struct timespec* timeout)
{
	if (!machgate_kqueue_fd_is_queue(kq)) {
		errno = EBADF;
		return -1;
	}
	if (nchanges < 0 || nevents < 0 || (nchanges > 0 && !changelist) ||
	    (nevents > 0 && !eventlist) || !kevent_timeout_is_valid(timeout)) {
		errno = EINVAL;
		return -1;
	}

	pthread_mutex_lock(&kqueue_mutex);
	int result = apply_kqueue_changes(kq, changelist, nchanges);
	pthread_mutex_unlock(&kqueue_mutex);
	if (result < 0 || nevents == 0)
		return result;

	result = emit_kqueue_receipts(changelist, nchanges, eventlist, nevents);
	if (result == nevents)
		return result;

	struct pollfd pollfds[KQUEUE_POLL_COUNT];
	struct shim_kqueue_poll_registration registrations[KQUEUE_POLL_COUNT];
	int event_capacity = nevents - result;
	int max_events = event_capacity < KQUEUE_POLL_COUNT ?
	                 event_capacity : KQUEUE_POLL_COUNT;

	pthread_mutex_lock(&kqueue_mutex);
	int pollfd_count = collect_kqueue_pollfds(kq, pollfds, registrations,
	                                          max_events);
	pthread_mutex_unlock(&kqueue_mutex);
	if (pollfd_count == 0) {
		sleep_for_kevent_timeout(timeout);
		return result;
	}

	int poll_result = poll(pollfds, (nfds_t)pollfd_count,
	                       kevent_timeout_ms(timeout));
	if (poll_result < 0)
		return -1;

	pthread_mutex_lock(&kqueue_mutex);
	result += emit_kqueue_events(kq, pollfds, registrations, pollfd_count,
	                             eventlist + result, nevents - result);
	pthread_mutex_unlock(&kqueue_mutex);
	return result;
}

int machgate_kqueue_event64(
	int kq, const struct machgate_darwin_kevent64* changelist, int nchanges,
	struct machgate_darwin_kevent64* eventlist, int nevents,
	const struct timespec* timeout)
{
	if (nchanges < 0 || nevents < 0 || (nchanges > 0 && !changelist) ||
	    (nevents > 0 && !eventlist)) {
		errno = EINVAL;
		return -1;
	}

	struct machgate_darwin_kevent* changes = NULL;
	struct machgate_darwin_kevent* events = NULL;
	if (nchanges > 0) {
		changes = calloc((size_t)nchanges, sizeof(*changes));
		if (!changes) {
			errno = ENOMEM;
			return -1;
		}
		for (int index = 0; index < nchanges; index++) {
			changes[index].ident = changelist[index].ident;
			changes[index].filter = changelist[index].filter;
			changes[index].flags = changelist[index].flags;
			changes[index].fflags = changelist[index].fflags;
			changes[index].data = changelist[index].data;
			changes[index].udata = changelist[index].udata;
		}
	}
	if (nevents > 0) {
		events = calloc((size_t)nevents, sizeof(*events));
		if (!events) {
			free(changes);
			errno = ENOMEM;
			return -1;
		}
	}

	int result = machgate_kqueue_event(kq, changes, nchanges, events, nevents,
	                                   timeout);
	if (result > 0) {
		for (int index = 0; index < result; index++) {
			eventlist[index].ident = events[index].ident;
			eventlist[index].filter = events[index].filter;
			eventlist[index].flags = events[index].flags;
			eventlist[index].fflags = events[index].fflags;
			eventlist[index].data = events[index].data;
			eventlist[index].udata = events[index].udata;
			eventlist[index].ext[0] = index < nchanges ?
			    changelist[index].ext[0] : 0;
			eventlist[index].ext[1] = index < nchanges ?
			    changelist[index].ext[1] : 0;
		}
	}
	free(events);
	free(changes);
	return result;
}
