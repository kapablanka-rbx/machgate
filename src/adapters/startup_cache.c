#include "adapters/startup_cache.h"

#include "log.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define STARTUP_CACHE_MAGIC UINT64_C(0x5441474341474348)
#define STARTUP_CACHE_VERSION 2u
#define STARTUP_CACHE_MAX_BYTES (128u * 1024u * 1024u)

struct cache_file_identity {
	uint64_t device;
	uint64_t inode;
	uint64_t size;
	uint64_t mtime_seconds;
	uint64_t mtime_nanoseconds;
	uint64_t ctime_seconds;
	uint64_t ctime_nanoseconds;
};

struct cache_key {
	struct cache_file_identity guest;
	struct cache_file_identity runtime;
	struct cache_file_identity redirection_library;
	uint64_t redirection_hash;
	uint64_t slice_offset;
	uint64_t slice_size;
};

struct cache_header {
	uint64_t magic;
	uint64_t version;
	struct cache_key key;
	uint64_t sizes[MACHGATE_CACHE_PART_COUNT];
	uint64_t checksum;
};

struct cache_part {
	void* data;
	size_t size;
};

struct cached_section {
	uint64_t address;
	uint64_t size;
	uint64_t count;
};

struct cached_instruction {
	uint64_t offset;
	uint32_t encoding;
	uint32_t reserved;
};

struct machgate_startup_cache {
	char path[PATH_MAX];
	char* guest_path;
	struct cache_key key;
	struct cache_part parts[MACHGATE_CACHE_PART_COUNT];
	int dirty;
};

static void cache_trace(const char* message)
{
	if (machgate_env_truthy(getenv("MACHGATE_LOG_STARTUP")) ||
	    machgate_env_truthy(getenv("MACHGATE_VERBOSE")))
		fprintf(stderr, "machgate: startup cache: %s\n", message);
}

static uint64_t checksum_bytes(uint64_t checksum, const void* data, size_t size)
{
	const unsigned char* bytes = data;
	for (size_t index = 0; index < size; index++) {
		checksum ^= bytes[index];
		checksum *= UINT64_C(1099511628211);
	}
	return checksum;
}

static struct cache_file_identity file_identity(const struct stat* status)
{
	return (struct cache_file_identity){
		.device = status->st_dev, .inode = status->st_ino,
		.size = status->st_size,
		.mtime_seconds = status->st_mtim.tv_sec,
		.mtime_nanoseconds = status->st_mtim.tv_nsec,
		.ctime_seconds = status->st_ctim.tv_sec,
		.ctime_nanoseconds = status->st_ctim.tv_nsec,
	};
}

static int identify_redirections(struct cache_key* key)
{
	const char* library = getenv("MACHGATE_TRAMPOLINE_LIB");
	const char* prefix = getenv("MACHGATE_TRAMPOLINE_PREFIX");
	if (!library || !*library || !prefix || !*prefix)
		return 0;
	if (!strchr(library, '/'))
		return -1;
	struct stat status;
	if (stat(library, &status) != 0)
		return -1;
	key->redirection_library = file_identity(&status);
	uint64_t checksum = checksum_bytes(UINT64_C(1469598103934665603), library, strlen(library) + 1);
	key->redirection_hash = checksum_bytes(checksum, prefix, strlen(prefix) + 1);
	return 0;
}

static int make_directory(char* path)
{
	for (char* cursor = path + 1; *cursor; cursor++) {
		if (*cursor != '/')
			continue;
		*cursor = '\0';
		int result = mkdir(path, 0700);
		*cursor = '/';
		if (result != 0 && errno != EEXIST)
			return -1;
	}
	if (mkdir(path, 0700) != 0 && errno != EEXIST)
		return -1;
	struct stat status;
	return stat(path, &status) == 0 && S_ISDIR(status.st_mode) &&
	       status.st_uid == geteuid() && access(path, W_OK | X_OK) == 0 ? 0 : -1;
}

static int select_cache_path(struct machgate_startup_cache* cache, const char* guest_path)
{
	char directory[PATH_MAX];
	const char* override = getenv("MACHGATE_STARTUP_CACHE_DIR");
	int length;
	if (override && *override) {
		length = snprintf(directory, sizeof(directory), "%s", override);
	} else {
		const char* slash = strrchr(guest_path, '/');
		length = snprintf(directory, sizeof(directory), "%.*s/.machgate-cache",
				  slash ? (int)(slash - guest_path) : 1,
				  slash ? guest_path : ".");
	}
	if (length < 0 || (size_t)length >= sizeof(directory))
		return -1;
	if (make_directory(directory) != 0) {
		if (override && *override)
			return -1;
		const char* cache_home = getenv("XDG_CACHE_HOME");
		const char* user_home = getenv("HOME");
		if (cache_home && *cache_home)
			length = snprintf(directory, sizeof(directory), "%s/machgate", cache_home);
		else if (user_home && *user_home)
			length = snprintf(directory, sizeof(directory), "%s/.cache/machgate", user_home);
		else
			length = snprintf(directory, sizeof(directory), "/tmp/machgate-cache-%lu",
					  (unsigned long)geteuid());
		if (length < 0 || (size_t)length >= sizeof(directory) ||
		    make_directory(directory) != 0)
			return -1;
	}
	uint64_t path_hash = checksum_bytes(UINT64_C(1469598103934665603),
					    guest_path, strlen(guest_path));
	length = snprintf(cache->path, sizeof(cache->path),
			  "%s/v2-%016" PRIx64 ".cache", directory, path_hash);
	return length >= 0 && (size_t)length < sizeof(cache->path) ? 0 : -1;
}

static int read_all(int descriptor, void* data, size_t size)
{
	unsigned char* cursor = data;
	while (size) {
		ssize_t result = read(descriptor, cursor, size);
		if (result < 0 && errno == EINTR)
			continue;
		if (result <= 0)
			return -1;
		cursor += result;
		size -= result;
	}
	return 0;
}

static int load_cache(struct machgate_startup_cache* cache)
{
	int descriptor = open(cache->path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
	if (descriptor < 0)
		return 0;
	struct stat status;
	struct cache_header header = {0};
	int valid = fstat(descriptor, &status) == 0 && S_ISREG(status.st_mode) &&
		status.st_uid == geteuid() && status.st_size >= (off_t)sizeof(header) &&
		status.st_size <= STARTUP_CACHE_MAX_BYTES &&
		read_all(descriptor, &header, sizeof(header)) == 0 &&
		header.magic == STARTUP_CACHE_MAGIC && header.version == STARTUP_CACHE_VERSION &&
		memcmp(&header.key, &cache->key, sizeof(header.key)) == 0;
	size_t payload_size = 0;
	for (size_t index = 0; valid && index < MACHGATE_CACHE_PART_COUNT; index++) {
		if (header.sizes[index] == UINT64_MAX)
			continue;
		if (header.sizes[index] > STARTUP_CACHE_MAX_BYTES - payload_size)
			valid = 0;
		else
			payload_size += header.sizes[index];
	}
	valid = valid && payload_size + sizeof(header) == (uint64_t)status.st_size;
	void* payload = valid ? malloc(payload_size ? payload_size : 1) : NULL;
	valid = valid && payload && read_all(descriptor, payload, payload_size) == 0;
	close(descriptor);
	uint64_t expected = header.checksum;
	header.checksum = 0;
	if (valid) {
		uint64_t checksum = checksum_bytes(UINT64_C(1469598103934665603),
						    &header, sizeof(header));
		valid = checksum_bytes(checksum, payload, payload_size) == expected;
	}
	if (valid) {
		size_t offset = 0;
		for (size_t index = 0; index < MACHGATE_CACHE_PART_COUNT; index++) {
			if (header.sizes[index] == UINT64_MAX)
				continue;
			machgate_startup_cache_put(cache, index, (char*)payload + offset, header.sizes[index]);
			offset += header.sizes[index];
		}
		cache->dirty = 0;
	}
	free(payload);
	return valid;
}

struct machgate_startup_cache* machgate_startup_cache_begin(
	const char* path, const struct stat* status,
	uint64_t slice_offset, uint64_t slice_size)
{
	const char* enabled = getenv("MACHGATE_STARTUP_CACHE");
	if (enabled && *enabled && !machgate_env_truthy(enabled))
		return NULL;
	struct stat runtime_status;
	if (stat("/proc/self/exe", &runtime_status) != 0)
		return NULL;
	struct machgate_startup_cache* cache = calloc(1, sizeof(*cache));
	if (!cache)
		return NULL;
	if (identify_redirections(&cache->key) != 0) {
		machgate_startup_cache_close(cache);
		cache_trace("unavailable redirection identity; using normal startup");
		return NULL;
	}
	cache->guest_path = realpath(path, NULL);
	if (!cache->guest_path || select_cache_path(cache, cache->guest_path) != 0) {
		machgate_startup_cache_close(cache);
		cache_trace("unavailable; using normal startup");
		return NULL;
	}
	cache->key.guest = file_identity(status);
	cache->key.runtime = file_identity(&runtime_status);
	cache->key.slice_offset = slice_offset;
	cache->key.slice_size = slice_size;
	cache_trace(load_cache(cache) ? "hit" : "miss");
	return cache;
}

const void* machgate_startup_cache_get(struct machgate_startup_cache* cache, enum machgate_startup_cache_part part,
				     size_t* size)
{
	*size = cache ? cache->parts[part].size : 0;
	return cache ? cache->parts[part].data : NULL;
}

void machgate_startup_cache_put(struct machgate_startup_cache* cache, enum machgate_startup_cache_part part,
			       const void* data, size_t size)
{
	if (!cache || !cache->path[0] || size > STARTUP_CACHE_MAX_BYTES)
		return;
	void* result = malloc(size ? size : 1);
	if (!result)
		return;
	if (size)
		memcpy(result, data, size);
	free(cache->parts[part].data);
	cache->parts[part] = (struct cache_part){result, size};
	cache->dirty = 1;
}

static const struct cached_section* find_cached_section(
	struct machgate_startup_cache* cache, enum machgate_startup_cache_part part, uint32_t* code, size_t size,
	uint32_t mask, uint32_t encoding)
{
	size_t remaining;
	const unsigned char* cursor = machgate_startup_cache_get(cache, part, &remaining);
	while (cursor && remaining >= sizeof(struct cached_section)) {
		const struct cached_section* section = (const void*)cursor;
		size_t available = remaining - sizeof(*section);
		if (section->count > available / sizeof(struct cached_instruction))
			return NULL;
		const struct cached_instruction* instructions = (const void*)(section + 1);
		if (section->address == (uintptr_t)code && section->size == size) {
			for (size_t index = 0; index < section->count; index++) {
				uint64_t offset = instructions[index].offset;
				if (size < sizeof(uint32_t) || offset > size - sizeof(uint32_t) ||
				    offset % sizeof(uint32_t) ||
				    (index && offset <= instructions[index - 1].offset) ||
				    (instructions[index].encoding & mask) != encoding ||
				    code[offset / sizeof(uint32_t)] != instructions[index].encoding)
					return NULL;
			}
			return section;
		}
		size_t bytes = sizeof(*section) + section->count * sizeof(*instructions);
		cursor += bytes;
		remaining -= bytes;
	}
	return NULL;
}

int machgate_startup_cache_patch(struct machgate_startup_cache* cache, enum machgate_startup_cache_part part,
				uint32_t* code, size_t size, uint32_t mask,
				uint32_t encoding,
				int (*patch)(uint32_t*, void*), void* context)
{
	const struct cached_section* cached = find_cached_section(cache, part, code, size, mask, encoding);
	if (cached) {
		const struct cached_instruction* instructions = (const void*)(cached + 1);
		for (size_t index = 0; index < cached->count; index++)
			if (patch(code + instructions[index].offset / sizeof(uint32_t), context) < 0)
				return -1;
		return (int)cached->count;
	}

	size_t count = 0;
	size_t capacity = 0;
	struct cached_instruction* instructions = NULL;
	int recording = cache && cache->path[0] != '\0';
	for (size_t index = 0; index < size / sizeof(uint32_t); index++) {
		if ((code[index] & mask) != encoding)
			continue;
		if (recording && count == capacity) {
			size_t next_capacity = capacity ? capacity * 2 : 32;
			void* result = next_capacity <= STARTUP_CACHE_MAX_BYTES / sizeof(*instructions)
				? realloc(instructions, next_capacity * sizeof(*instructions)) : NULL;
			if (!result)
				recording = 0;
			else {
				instructions = result;
				capacity = next_capacity;
			}
		}
		if (recording)
			instructions[count] = (struct cached_instruction){index * sizeof(uint32_t), code[index], 0};
		count++;
		if (patch(&code[index], context) < 0) {
			free(instructions);
			return -1;
		}
	}
	if (recording) {
		struct cache_part* destination = &cache->parts[part];
		size_t bytes = sizeof(struct cached_section) + count * sizeof(*instructions);
		void* result = bytes <= STARTUP_CACHE_MAX_BYTES - destination->size
			? realloc(destination->data, destination->size + bytes) : NULL;
		if (result) {
			destination->data = result;
			struct cached_section* section = (void*)((char*)result + destination->size);
			*section = (struct cached_section){(uintptr_t)code, size, count};
			if (count)
				memcpy(section + 1, instructions, count * sizeof(*instructions));
			destination->size += bytes;
			cache->dirty = 1;
		}
	}
	free(instructions);
	return (int)count;
}

static int write_all(int descriptor, const void* data, size_t size)
{
	const unsigned char* cursor = data;
	while (size) {
		ssize_t result = write(descriptor, cursor, size);
		if (result < 0 && errno == EINTR)
			continue;
		if (result <= 0)
			return -1;
		cursor += result;
		size -= result;
	}
	return 0;
}

void machgate_startup_cache_publish(struct machgate_startup_cache* cache)
{
	if (!cache || !cache->path[0] || !cache->dirty)
		return;
	struct stat status;
	if (stat(cache->guest_path, &status) != 0)
		return;
	struct cache_file_identity identity = file_identity(&status);
	if (memcmp(&identity, &cache->key.guest, sizeof(identity)) != 0)
		return;
	struct cache_header header = {
		.magic = STARTUP_CACHE_MAGIC, .version = STARTUP_CACHE_VERSION,
		.key = cache->key,
	};
	size_t total_size = sizeof(header);
	for (size_t index = 0; index < MACHGATE_CACHE_PART_COUNT; index++) {
		struct cache_part* part = &cache->parts[index];
		header.sizes[index] = part->data ? part->size : UINT64_MAX;
		if (part->size > STARTUP_CACHE_MAX_BYTES - total_size)
			return;
		total_size += part->size;
	}
	uint64_t checksum = checksum_bytes(UINT64_C(1469598103934665603), &header, sizeof(header));
	for (size_t index = 0; index < MACHGATE_CACHE_PART_COUNT; index++)
		checksum = checksum_bytes(checksum, cache->parts[index].data, cache->parts[index].size);
	header.checksum = checksum;
	char temporary[PATH_MAX];
	int length = snprintf(temporary, sizeof(temporary), "%s.tmp.XXXXXX", cache->path);
	if (length < 0 || (size_t)length >= sizeof(temporary))
		return;
	int descriptor = mkostemp(temporary, O_CLOEXEC);
	if (descriptor < 0)
		return;
	int result = write_all(descriptor, &header, sizeof(header));
	for (size_t index = 0; result == 0 && index < MACHGATE_CACHE_PART_COUNT; index++)
		result = write_all(descriptor, cache->parts[index].data, cache->parts[index].size);
	if (result == 0 && fsync(descriptor) != 0)
		result = -1;
	if (close(descriptor) != 0)
		result = -1;
	if (result == 0 && rename(temporary, cache->path) == 0) {
		cache->dirty = 0;
		cache_trace("published");
	} else {
		unlink(temporary);
		cache_trace("write failed; using normal startup");
	}
}

void machgate_startup_cache_close(struct machgate_startup_cache* cache)
{
	if (!cache)
		return;
	for (size_t index = 0; index < MACHGATE_CACHE_PART_COUNT; index++)
		free(cache->parts[index].data);
	free(cache->guest_path);
	free(cache);
}
