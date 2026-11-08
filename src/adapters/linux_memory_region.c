#define _GNU_SOURCE
#include "ports/memory_region.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

struct map_reader {
	int descriptor;
	char buffer[8192];
	size_t position;
	size_t length;
};

static int read_line(struct map_reader* reader, char* line, size_t capacity)
{
	size_t length = 0;
	for (;;) {
		if (reader->position == reader->length) {
			long result;
			do {
				result = syscall(SYS_read, reader->descriptor,
					reader->buffer, sizeof(reader->buffer));
			} while (result < 0 && errno == EINTR);
			if (result <= 0) {
				line[length] = '\0';
				return result < 0 ? -1 : length != 0;
			}
			reader->position = 0;
			reader->length = result;
		}
		char character = reader->buffer[reader->position++];
		if (character == '\n') {
			line[length] = '\0';
			return 1;
		}
		if (length + 1 < capacity)
			line[length++] = character;
	}
}

static int parse_mapping(const char* line, struct machgate_memory_region* region)
{
	char* next;
	uint64_t start = strtoull(line, &next, 16);
	if (next == line || *next != '-')
		return 0;
	uint64_t end = strtoull(next + 1, &next, 16);
	if (*next != ' ' || end <= start)
		return 0;
	while (*next == ' ')
		++next;
	if (strlen(next) < 5)
		return 0;
	*region = (struct machgate_memory_region){
		.start = start,
		.size = end - start,
		.readable = next[0] == 'r',
		.writable = next[1] == 'w',
		.executable = next[2] == 'x',
		.shared = next[3] == 's',
		.offset = strtoull(next + 4, NULL, 16),
	};
	return 1;
}

static void parse_vm_flags(const char* line, struct machgate_memory_region* region)
{
	region->may_read = strstr(line, " mr ") != NULL;
	region->may_write = strstr(line, " mw ") != NULL;
	region->may_execute = strstr(line, " me ") != NULL;
	region->excluded_from_fork = strstr(line, " dc ") != NULL;
	region->random_access = strstr(line, " rr ") != NULL;
	region->sequential_access = strstr(line, " sr ") != NULL;
	region->locked = strstr(line, " lo ") != NULL;
}

int machgate_query_memory_region(uint64_t address, struct machgate_memory_region* region)
{
	int descriptor = syscall(SYS_openat, AT_FDCWD, "/proc/self/smaps", O_RDONLY | O_CLOEXEC, 0);
	if (descriptor < 0)
		return -1;
	struct map_reader reader = {.descriptor = descriptor};
	char line[512];
	int selected = 0;
	int result;
	while ((result = read_line(&reader, line, sizeof(line))) > 0) {
		struct machgate_memory_region candidate;
		if (parse_mapping(line, &candidate)) {
			if (selected)
				break;
			if (candidate.start + candidate.size > address) {
				*region = candidate;
				selected = 1;
			}
		} else if (selected && strncmp(line, "VmFlags:", 8) == 0) {
			parse_vm_flags(line, region);
			break;
		}
	}
	syscall(SYS_close, descriptor);
	return result < 0 ? -1 : selected;
}
