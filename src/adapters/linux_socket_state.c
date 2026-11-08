#include "ports/socket_state.h"

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <unistd.h>

struct socket_state {
	int descriptor;
	int nosigpipe;
	uint64_t cookie;
};

static pthread_mutex_t socket_state_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct socket_state* socket_states;
static size_t socket_state_count;
static size_t socket_state_capacity;

static int read_socket_cookie(int descriptor, uint64_t* cookie)
{
	socklen_t size = sizeof(*cookie);
	return (int)syscall(SYS_getsockopt, descriptor, SOL_SOCKET, SO_COOKIE,
	                    cookie, &size);
}

static struct socket_state* remember_socket_unlocked(int descriptor, uint64_t cookie)
{
	int enabled = 0;
	for (size_t index = 0; index < socket_state_count; index++) {
		if (socket_states[index].descriptor == descriptor &&
		    socket_states[index].cookie == cookie)
			return &socket_states[index];
		if (socket_states[index].cookie == cookie)
			enabled = socket_states[index].nosigpipe;
	}
	if (socket_state_count == socket_state_capacity) {
		size_t capacity = socket_state_capacity ? socket_state_capacity * 2 : 32;
		struct socket_state* result = realloc(socket_states, capacity * sizeof(*result));
		if (!result)
			return NULL;
		socket_states = result;
		socket_state_capacity = capacity;
	}
	struct socket_state* result = &socket_states[socket_state_count++];
	*result = (struct socket_state){ descriptor, enabled, cookie };
	return result;
}

int machgate_socket_nosigpipe_set(int descriptor, int enabled)
{
	uint64_t cookie;
	if (read_socket_cookie(descriptor, &cookie) < 0)
		return -1;
	pthread_mutex_lock(&socket_state_mutex);
	struct socket_state* state = remember_socket_unlocked(descriptor, cookie);
	if (state) {
		for (size_t index = 0; index < socket_state_count; index++) {
			if (socket_states[index].cookie == cookie)
				socket_states[index].nosigpipe = enabled != 0;
		}
	}
	pthread_mutex_unlock(&socket_state_mutex);
	if (!state)
		errno = ENOMEM;
	return state ? 0 : -1;
}

int machgate_socket_nosigpipe_get(int descriptor)
{
	uint64_t cookie;
	if (read_socket_cookie(descriptor, &cookie) < 0)
		return -1;
	pthread_mutex_lock(&socket_state_mutex);
	struct socket_state* state = remember_socket_unlocked(descriptor, cookie);
	int result = state ? state->nosigpipe : -1;
	pthread_mutex_unlock(&socket_state_mutex);
	if (!state)
		errno = ENOMEM;
	return result;
}

int machgate_socket_send_flags(int descriptor, int flags)
{
	pthread_mutex_lock(&socket_state_mutex);
	for (size_t index = 0; index < socket_state_count; index++) {
		if (socket_states[index].descriptor == descriptor) {
			if (socket_states[index].nosigpipe)
				flags |= MSG_NOSIGNAL;
			break;
		}
	}
	pthread_mutex_unlock(&socket_state_mutex);
	return flags;
}

void machgate_socket_state_close(int descriptor)
{
	pthread_mutex_lock(&socket_state_mutex);
	for (size_t index = 0; index < socket_state_count; index++) {
		if (socket_states[index].descriptor == descriptor) {
			socket_states[index] = socket_states[--socket_state_count];
			break;
		}
	}
	pthread_mutex_unlock(&socket_state_mutex);
}

void machgate_socket_state_duplicate(int source, int destination)
{
	if (source == destination)
		return;
	uint64_t cookie;
	int saved_errno = errno;
	if (read_socket_cookie(source, &cookie) == 0) {
		pthread_mutex_lock(&socket_state_mutex);
		remember_socket_unlocked(source, cookie);
		remember_socket_unlocked(destination, cookie);
		pthread_mutex_unlock(&socket_state_mutex);
	}
	errno = saved_errno;
}

long machgate_socket_write(int descriptor, const void* buffer, size_t size)
{
	int flags = machgate_socket_send_flags(descriptor, 0);
	if (flags & MSG_NOSIGNAL)
		return syscall(SYS_sendto, descriptor, buffer, size, flags, NULL, 0);
	return syscall(SYS_write, descriptor, buffer, size);
}

long machgate_socket_writev(int descriptor, const struct iovec* vectors, int count)
{
	int flags = machgate_socket_send_flags(descriptor, 0);
	if (!(flags & MSG_NOSIGNAL))
		return syscall(SYS_writev, descriptor, vectors, count);
	if (count < 0) {
		errno = EINVAL;
		return -1;
	}
	struct msghdr message = { .msg_iov = (struct iovec*)vectors, .msg_iovlen = (size_t)count };
	return syscall(SYS_sendmsg, descriptor, &message, flags);
}

int machgate_socket_state_accept(int listener, int accepted)
{
	if (machgate_socket_send_flags(listener, 0) & MSG_NOSIGNAL)
		return machgate_socket_nosigpipe_set(accepted, 1);
	return 0;
}
