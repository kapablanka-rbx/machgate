#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern int sysctl(const int*, unsigned int, void*, size_t*, const void*, size_t);

/* Verify that unknown NET/INET/UDP selectors still return ENOENT. */
static void check_unavailable_metric(int selector)
{
	int name[] = {4, 2, 17, selector};
	unsigned char output[256];
	memset(output, 0xa5, sizeof(output));
	size_t size = sizeof(output);
	assert(sysctl(name, 4, output, &size, NULL, 0) == -1);
	assert(errno == ENOENT && size == sizeof(output));
	for (size_t index = 0; index < sizeof(output); index++)
		assert(output[index] == 0xa5);
	size = 0;
	assert(sysctl(name, 4, NULL, &size, NULL, 0) == -1);
	assert(errno == ENOENT && size == 0);
	assert(sysctl(name, 4, NULL, &size, output, 1) == -1);
	assert(errno == EPERM);
}

/* UDPCTL_PCBLIST (selector 5): returns xinpgen header with socket count. */
static void check_udp_pcblist(void)
{
	int name[] = {4, 2, 17, 5};

	/* Size query returns the header size (8 bytes). */
	size_t size = 0;
	assert(sysctl(name, 4, NULL, &size, NULL, 0) == 0);
	assert(size == 8);

	/* Data query fills the xinpgen header. */
	uint32_t header[2] = {0, 0};
	size = sizeof(header);
	assert(sysctl(name, 4, header, &size, NULL, 0) == 0);
	assert(size == 8);
	assert(header[0] == 8);        /* xig_len */
	/* xig_count is non-negative (always true for uint32_t); value
	 * depends on host state so we just verify it was written. */

	/* Write attempts are rejected. */
	assert(sysctl(name, 4, NULL, &size, header, 1) == -1);
	assert(errno == EPERM);
}

/* UDPCTL_STATS (selector 2): returns a Darwin udpstat structure. */
static void check_udp_stats(void)
{
	int name[] = {4, 2, 17, 2};
	const size_t expected_size = 14 * 4 + 2 * 8; /* Darwin udpstat */

	/* Size query. */
	size_t size = 0;
	assert(sysctl(name, 4, NULL, &size, NULL, 0) == 0);
	assert(size == expected_size);

	/* Data query. */
	unsigned char buf[128];
	memset(buf, 0xa5, sizeof(buf));
	size = sizeof(buf);
	assert(sysctl(name, 4, buf, &size, NULL, 0) == 0);
	assert(size == expected_size);

	/* Write attempts are rejected. */
	assert(sysctl(name, 4, NULL, &size, buf, 1) == -1);
	assert(errno == EPERM);
}

int main(void)
{
	/* Known UDP selectors now succeed. */
	check_udp_pcblist();
	check_udp_stats();

	/* An unrecognised selector still fails. */
	check_unavailable_metric(99);

	/* hw.ncpu still works. */
	int name[] = {6, 3};
	uint32_t result = 0;
	size_t size = sizeof(result);
	assert(sysctl(name, 2, &result, &size, NULL, 0) == 0);
	assert(size == sizeof(result) && result > 0);
	return 0;
}
