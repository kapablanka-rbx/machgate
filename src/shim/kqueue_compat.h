#ifndef MACHGATE_KQUEUE_COMPAT_H
#define MACHGATE_KQUEUE_COMPAT_H

#include <stdint.h>
#include <time.h>

struct machgate_darwin_kevent {
	uint64_t ident;
	int16_t filter;
	uint16_t flags;
	uint32_t fflags;
	int64_t data;
	void* udata;
};

_Static_assert(sizeof(struct machgate_darwin_kevent) == 32,
	"Darwin kevent layout must remain 32 bytes");

struct machgate_darwin_kevent64 {
	uint64_t ident;
	int16_t filter;
	uint16_t flags;
	uint32_t fflags;
	int64_t data;
	void* udata;
	uint64_t ext[2];
};

_Static_assert(sizeof(struct machgate_darwin_kevent64) == 48,
	"Darwin kevent64_s layout must remain 48 bytes");

int machgate_kqueue_open(void);
int machgate_kqueue_fd_is_queue(int fd);
int machgate_kqueue_event(int kq,
                          const struct machgate_darwin_kevent* changelist,
                          int nchanges,
                          struct machgate_darwin_kevent* eventlist,
                          int nevents, const struct timespec* timeout);
int machgate_kqueue_event64(
	int kq, const struct machgate_darwin_kevent64* changelist, int nchanges,
	struct machgate_darwin_kevent64* eventlist, int nevents,
	const struct timespec* timeout);
void machgate_kqueue_fd_did_close(int fd);
void machgate_kqueue_fd_did_duplicate(int from_fd, int to_fd);

#endif
