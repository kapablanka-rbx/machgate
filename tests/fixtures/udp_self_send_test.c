/* Minimal test: does sendto(self) wake a blocking recvfrom on the same UDP socket?
   Exit 0 = OK, nonzero = broken. */

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef long ssize_t;
typedef unsigned long size_t;
typedef unsigned int socklen_t;

struct sockaddr_in_darwin {
    uint8_t  sin_len;
    uint8_t  sin_family;
    uint16_t sin_port;
    uint32_t sin_addr;
    char     sin_zero[8];
};

extern int socket(int domain, int type, int protocol);
extern int bind(int fd, const void* addr, socklen_t len);
extern int getsockname(int fd, void* addr, socklen_t* len);
extern ssize_t sendto(int fd, const void* buf, size_t len, int flags,
                      const void* addr, socklen_t addrlen);
extern ssize_t recvfrom(int fd, void* buf, size_t len, int flags,
                        void* addr, socklen_t* addrlen);
extern int close(int fd);
extern int pthread_create(void* thread, const void* attr,
                          void* (*start)(void*), void* arg);
extern int pthread_join(void* thread, void** retval);
extern void _exit(int status) __attribute__((noreturn));

/* Darwin constants */
#define AF_INET   2
#define SOCK_DGRAM 2

static int g_fd;
static volatile int g_recv_done;
static volatile int g_recv_bytes;

static void* recv_thread(void* arg)
{
    (void)arg;
    char buf[64];
    struct sockaddr_in_darwin from;
    socklen_t fromlen = sizeof(from);
    /* This blocks until a packet arrives */
    ssize_t n = recvfrom(g_fd, buf, sizeof(buf), 0, &from, &fromlen);
    g_recv_bytes = (int)n;
    g_recv_done = 1;
    return (void*)0;
}

int main(void)
{
    /* Create UDP socket bound to 127.0.0.1:0 */
    g_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_fd < 0)
        return 1;

    struct sockaddr_in_darwin bind_addr;
    __builtin_memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_len = 16;
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = 0;
    bind_addr.sin_addr = 0x00000000; /* INADDR_ANY — like RakNet's CreateBoundSocket */

    if (bind(g_fd, &bind_addr, sizeof(bind_addr)) < 0)
        return 2;

    /* Get the actual bound port */
    struct sockaddr_in_darwin bound;
    socklen_t blen = sizeof(bound);
    if (getsockname(g_fd, &bound, &blen) < 0)
        return 3;

    /* Start recv thread — will block in recvfrom */
    unsigned long thr;
    if (pthread_create((void*)&thr, (void*)0, recv_thread, (void*)0) != 0)
        return 4;

    /* Brief delay: ~50ms spin (no nanosleep in freestanding) */
    for (volatile int i = 0; i < 5000000; i++)
        ;

    /* RakNet sends to 127.0.0.1:<bound_port>, not the getsockname addr (0.0.0.0) */
    struct sockaddr_in_darwin send_addr;
    __builtin_memset(&send_addr, 0, sizeof(send_addr));
    send_addr.sin_len = 16;
    send_addr.sin_family = AF_INET;
    send_addr.sin_port = bound.sin_port; /* ephemeral port from getsockname */
    send_addr.sin_addr = 0x0100007f; /* 127.0.0.1 */

    char pkt = 0x42;
    ssize_t sent = sendto(g_fd, &pkt, 1, 0, &send_addr, sizeof(send_addr));
    if (sent < 0)
        return 5;

    /* Wait for recv thread with a spin timeout (~2 seconds) */
    for (volatile int i = 0; i < 200000000 && !g_recv_done; i++)
        ;

    if (!g_recv_done)
        return 10; /* recv thread still blocked — this is the bug */

    if (g_recv_bytes != 1)
        return 11; /* wrong byte count */

    pthread_join((void*)thr, (void*)0);
    close(g_fd);
    return 0;
}
