#ifndef MACHGATE_SOCKET_STATE_H
#define MACHGATE_SOCKET_STATE_H

#include <stddef.h>
struct iovec;

long machgate_socket_write(int descriptor, const void* buffer, size_t size);
long machgate_socket_writev(int descriptor, const struct iovec* vectors, int count);

int machgate_socket_nosigpipe_set(int descriptor, int enabled);
int machgate_socket_nosigpipe_get(int descriptor);
int machgate_socket_send_flags(int descriptor, int flags);
int machgate_socket_state_accept(int listener, int accepted);
void machgate_socket_state_close(int descriptor);
void machgate_socket_state_duplicate(int source, int destination);
void machgate_socket_wake_pollers(int descriptor);

#endif
