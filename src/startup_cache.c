/*
 * Per-binary startup cache for machgate.
 *
 * Caches the deterministic products of Mach-O startup translation so
 * repeated boots of the same guest binary skip the expensive generation
 * passes. The cache is keyed on (dev, ino, size, mtime_ns) of the guest
 * binary and verified on load; a stale or corrupt cache is never used.
 *
 * Cached artifacts and their position-independence:
 *
 *  - synthetic .eh_frame blob for the main executable. All FDE initial
 *    locations and LSDA pointers are full guest vmaddrs; the main
 *    executable always maps at its preferred base with slide 0 (loader.c
 *    maps segments at seg->vmaddr), so the blob is identical across boots.
 *    The only per-boot variance is the personality function pointer
 *    embedded in the zPLR CIE (it points at the shim's
 *    __gxx_personality_v0 whose address is ASLR-dependent) — the cache
 *    stores a zero placeholder at cie_personality_offset and the loader
 *    re-patches it with the runtime address before registration.
 *
 *  - the .eh_frame_hdr table pairs (initial_location, fde_offset). Both
 *    fields are slide-independent: initial locations are guest vmaddrs
 *    and fde offsets are relative to the freshly mmap'd copy of the
 *    cached eh_frame blob, which always lands within +-2GB of the guest
 *    __TEXT (mmap_near_address enforces the same ±2GB window the
 *    generator relies on).
 *
 *  - the RCPC/TPIDRRO instruction rewrite patch list. The scan is
 *    deterministic; the cache stores (offset, instruction) pairs that are
 *    replayed as a write loop instead of a decode+scan of __TEXT.
 *
 * The cache directory defaults to <binary_dir>/.machgate-cache/ with the
 * file named by inode+size (<ino>-<size>.cache). When the sidecar
 * directory is not writable (read-only build trees), it falls back to
 * $XDG_CACHE_HOME/machgate or /tmp/machgate-cache, keyed by realpath
 * hash + inode + size. Writers publish atomically via rename(2), so
 * concurrent boots of the same binary are safe; a reader that sees a
 * partial or invalid file falls back to full generation.
 * MACHGATE_STARTUP_CACHE=0 disables the cache entirely.
 */

#include "startup_cache.h"
#include "log.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static int cache_problem_logged = 0;

static void log_cache_problem(const char* reason)
{
	if (cache_problem_logged)
		return;
	cache_problem_logged = 1;
	fprintf(stderr, "machgate: startup cache: %s — regenerating\n", reason);
}

int startup_cache_enabled(void)
{
	const char* value = machgate_getenv_compat("MACHGATE_STARTUP_CACHE",
	                                            "MACHISMO_STARTUP_CACHE");
	if (value && value[0] &&
	    (strcmp(value, "0") == 0 || strcmp(value, "false") == 0 ||
	     strcmp(value, "FALSE") == 0 || strcmp(value, "no") == 0 ||
	     strcmp(value, "NO") == 0))
		return 0;
	return 1;
}

static uint64_t fnv1a64(const void* data, size_t size)
{
	const uint8_t* bytes = (const uint8_t*)data;
	uint64_t hash = UINT64_C(1469598103934665603);
	for (size_t i = 0; i < size; i++) {
		hash ^= bytes[i];
		hash *= UINT64_C(1099511628211);
	}
	return hash;
}

static void fill_cache_key(struct startup_cache_key* key,
                           const struct stat* binary_st)
{
	memset(key, 0, sizeof(*key));
	key->dev = (uint64_t)binary_st->st_dev;
	key->ino = (uint64_t)binary_st->st_ino;
	key->size = (uint64_t)binary_st->st_size;
	key->mtime_sec = (uint64_t)binary_st->st_mtim.tv_sec;
	key->mtime_nsec = (uint64_t)binary_st->st_mtim.tv_nsec;
}

static int key_matches(const struct startup_cache_key* cached,
                       const struct startup_cache_key* current)
{
	return cached->dev == current->dev &&
	       cached->ino == current->ino &&
	       cached->size == current->size &&
	       cached->mtime_sec == current->mtime_sec &&
	       cached->mtime_nsec == current->mtime_nsec;
}

static uint64_t header_checksum(const struct startup_cache_header* header)
{
	struct startup_cache_header copy = *header;
	copy.checksum = 0;
	copy.payload_checksum = 0;
	return fnv1a64(&copy, sizeof(copy));
}

static void build_sidecar_path(char* buffer, size_t buffer_size,
                               const char* binary_path,
                               const struct stat* binary_st)
{
	const char* slash = strrchr(binary_path, '/');
	size_t dir_length = slash ? (size_t)(slash - binary_path) : 0;

	if (slash) {
		snprintf(buffer, buffer_size,
		         "%.*s/.machgate-cache/%llu-%llu.cache",
		         (int)dir_length, binary_path,
		         (unsigned long long)binary_st->st_ino,
		         (unsigned long long)binary_st->st_size);
	} else {
		snprintf(buffer, buffer_size,
		         ".machgate-cache/%llu-%llu.cache",
		         (unsigned long long)binary_st->st_ino,
		         (unsigned long long)binary_st->st_size);
	}
}

static void build_fallback_path(char* buffer, size_t buffer_size,
                                const char* binary_path,
                                const struct stat* binary_st)
{
	char resolved[4096];
	if (!realpath(binary_path, resolved))
		snprintf(resolved, sizeof(resolved), "%s", binary_path);

	uint64_t path_hash = fnv1a64(resolved, strlen(resolved));

	const char* cache_home = getenv("XDG_CACHE_HOME");
	char base[4096];

	if (cache_home && cache_home[0]) {
		snprintf(base, sizeof(base), "%s/machgate", cache_home);
	} else {
		const char* home = getenv("HOME");
		if (home && home[0])
			snprintf(base, sizeof(base), "%s/.cache/machgate", home);
		else
			snprintf(base, sizeof(base), "/tmp/machgate-cache");
	}

	snprintf(buffer, buffer_size, "%s/%016llx-%llu-%llu.cache", base,
	         (unsigned long long)path_hash,
	         (unsigned long long)binary_st->st_ino,
	         (unsigned long long)binary_st->st_size);
}

static void ensure_parent_directory(const char* file_path)
{
	char parent[4096];
	snprintf(parent, sizeof(parent), "%s", file_path);
	char* slash = strrchr(parent, '/');
	if (!slash)
		return;
	*slash = '\0';
	mkdir(parent, 0777);
}

static int sidecar_path_is_writable(const char* file_path)
{
	ensure_parent_directory(file_path);

	char probe_path[4200];
	snprintf(probe_path, sizeof(probe_path), "%s.probe.%d",
	         file_path, (int)getpid());

	int probe_fd = open(probe_path, O_WRONLY | O_CREAT | O_EXCL, 0644);
	if (probe_fd >= 0) {
		close(probe_fd);
		unlink(probe_path);
		return 1;
	}
	return 0;
}

static void build_cache_path(char* buffer, size_t buffer_size,
                             const char* binary_path,
                             const struct stat* binary_st)
{
	build_sidecar_path(buffer, buffer_size, binary_path, binary_st);
	if (sidecar_path_is_writable(buffer))
		return;

	build_fallback_path(buffer, buffer_size, binary_path, binary_st);
	ensure_parent_directory(buffer);
}

/* ---------- read path ---------- */

static void* cache_mapping = NULL;
static size_t cache_mapping_size = 0;
static size_t cache_eh_frame_size = 0;

int startup_cache_open(struct startup_cache_header* out_header,
                       const void** out_eh_frame,
                       const uint64_t** out_hdr_pairs,
                       const struct startup_cache_patch** out_patches,
                       const char* binary_path,
                       const struct stat* binary_st)
{
	if (!startup_cache_enabled())
		return -1;

	char path[4096];
	build_cache_path(path, sizeof(path), binary_path, binary_st);

	int fd = open(path, O_RDONLY);
	if (fd < 0)
		return -1;

	struct stat cache_st;
	if (fstat(fd, &cache_st) < 0) {
		close(fd);
		return -1;
	}

	size_t file_size = (size_t)cache_st.st_size;
	if (file_size < sizeof(struct startup_cache_header)) {
		close(fd);
		return -1;
	}

	void* mapping = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	if (mapping == MAP_FAILED)
		return -1;

	const struct startup_cache_header* header =
		(const struct startup_cache_header*)mapping;

	if (header->magic != MACHGATE_STARTUP_CACHE_MAGIC) {
		munmap(mapping, file_size);
		log_cache_problem("bad magic");
		return -1;
	}
	if (header->version != MACHGATE_STARTUP_CACHE_VERSION ||
	    header->header_size != sizeof(struct startup_cache_header)) {
		munmap(mapping, file_size);
		log_cache_problem("cache version mismatch");
		return -1;
	}
	if (header->checksum != header_checksum(header)) {
		munmap(mapping, file_size);
		log_cache_problem("header checksum mismatch");
		return -1;
	}

	struct startup_cache_key current_key;
	fill_cache_key(&current_key, binary_st);
	if (!key_matches(&header->key, &current_key)) {
		munmap(mapping, file_size);
		return -1;
	}

	uint64_t patches_size = header->patch_count *
	                        (uint64_t)sizeof(struct startup_cache_patch);
	uint64_t total_needed = header->header_size + header->eh_frame_size +
	                        header->hdr_pairs_size + patches_size;
	if (total_needed != (uint64_t)file_size) {
		munmap(mapping, file_size);
		log_cache_problem("payload size mismatch");
		return -1;
	}

	const uint8_t* payload = (const uint8_t*)mapping + header->header_size;
	uint64_t payload_checksum =
		fnv1a64(payload, (size_t)(file_size - header->header_size));
	if (payload_checksum != header->payload_checksum) {
		munmap(mapping, file_size);
		log_cache_problem("payload checksum mismatch");
		return -1;
	}
	if (out_eh_frame)
		*out_eh_frame = payload;
	if (out_hdr_pairs)
		*out_hdr_pairs = (const uint64_t*)(payload + header->eh_frame_size);
	if (out_patches)
		*out_patches = (const struct startup_cache_patch*)
			(payload + header->eh_frame_size + header->hdr_pairs_size);
	if (out_header)
		*out_header = *header;

	cache_mapping = mapping;
	cache_mapping_size = file_size;
	cache_eh_frame_size = (size_t)header->eh_frame_size;
	return 0;
}


const void* startup_cache_eh_frame_blob(size_t* out_size)
{
	if (!cache_mapping) {
		if (out_size)
			*out_size = 0;
		return NULL;
	}
	if (out_size)
		*out_size = cache_eh_frame_size;
	return (const uint8_t*)cache_mapping +
	       sizeof(struct startup_cache_header);
}

void* startup_cache_map_eh_frame_near(uintptr_t base)
{
	if (cache_eh_frame_size == 0)
		return NULL;

	const int prot = PROT_READ | PROT_WRITE;
	const uintptr_t step = 0x02000000ULL;
	const uintptr_t limit = 0x70000000ULL;
	const uintptr_t page_mask = 0xfffULL;

	for (uintptr_t delta = step; delta < limit; delta += step) {
		uintptr_t candidates[2] = {
			(base + delta) & ~page_mask,
			(base > delta) ? ((base - delta) & ~page_mask) : 0
		};

		for (int i = 0; i < 2; i++) {
			if (!candidates[i])
				continue;

			int flags = MAP_PRIVATE | MAP_ANONYMOUS;
#ifdef MAP_FIXED_NOREPLACE
			flags |= MAP_FIXED_NOREPLACE;
#endif
			void* mapping = mmap((void*)candidates[i], cache_eh_frame_size,
			                     prot, flags, -1, 0);
			if (mapping == MAP_FAILED)
				continue;

			int64_t distance = (int64_t)(uintptr_t)mapping - (int64_t)base;
			if (distance > -INT32_MAX && distance < INT32_MAX)
				return mapping;

			munmap(mapping, cache_eh_frame_size);
		}
	}

	return NULL;
}

void startup_cache_close(void)
{
	if (cache_mapping) {
		munmap(cache_mapping, cache_mapping_size);
		cache_mapping = NULL;
		cache_mapping_size = 0;
		cache_eh_frame_size = 0;
	}
}

/* ---------- write path ---------- */

static int write_all(int fd, const void* data, size_t size)
{
	const uint8_t* cursor = (const uint8_t*)data;
	while (size > 0) {
		ssize_t written = write(fd, cursor, size);
		if (written < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		cursor += written;
		size -= (size_t)written;
	}
	return 0;
}

int startup_cache_publish(const void* eh_frame, size_t eh_frame_size,
                          const uint64_t* hdr_pairs, size_t hdr_pairs_size,
                          uint64_t cie_personality_offset,
                          const struct startup_cache_patch* patches,
                          size_t patch_count, uint32_t fde_count,
                          const char* binary_path,
                          const struct stat* binary_st)
{
	if (!startup_cache_enabled())
		return -1;

	char final_path[4096];
	build_cache_path(final_path, sizeof(final_path), binary_path, binary_st);

	char temp_path[4200];
	snprintf(temp_path, sizeof(temp_path), "%s.tmp.%d",
	         final_path, (int)getpid());

	struct startup_cache_header header;
	memset(&header, 0, sizeof(header));
	header.magic = MACHGATE_STARTUP_CACHE_MAGIC;
	header.version = MACHGATE_STARTUP_CACHE_VERSION;
	header.header_size = sizeof(struct startup_cache_header);
	fill_cache_key(&header.key, binary_st);
	header.eh_frame_size = eh_frame_size;
	header.hdr_pairs_size = hdr_pairs_size;
	header.patch_count = patch_count;
	header.cie_personality_offset = cie_personality_offset;
	header.fde_count = fde_count;
	header.checksum = header_checksum(&header);

	int fd = open(temp_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0)
		return -1;

	int result = 0;
	result |= write_all(fd, &header, sizeof(header));
	if (eh_frame_size > 0)
		result |= write_all(fd, eh_frame, eh_frame_size);
	if (hdr_pairs_size > 0)
		result |= write_all(fd, hdr_pairs, hdr_pairs_size);
	if (patch_count > 0)
		result |= write_all(fd, patches,
		                    patch_count * sizeof(struct startup_cache_patch));

	if (fsync(fd) < 0)
		result = -1;
	if (close(fd) < 0)
		result = -1;

	if (result != 0) {
		unlink(temp_path);
		log_cache_problem("write failed");
		return -1;
	}

	uint64_t payload_size = eh_frame_size + hdr_pairs_size +
	                        patch_count * sizeof(struct startup_cache_patch);
	uint8_t* payload = (uint8_t*)malloc(payload_size ? (size_t)payload_size : 1);
	if (!payload) {
		unlink(temp_path);
		return -1;
	}

	int checksum_fd = open(temp_path, O_RDONLY);
	size_t payload_read = 0;
	if (checksum_fd >= 0)
		lseek(checksum_fd, (off_t)header.header_size, SEEK_SET);
	while (checksum_fd >= 0 && payload_read < (size_t)payload_size) {
		ssize_t got = read(checksum_fd, payload + payload_read,
		                   (size_t)payload_size - payload_read);
		if (got < 0 && errno == EINTR)
			continue;
		if (got <= 0)
			break;
		payload_read += (size_t)got;
	}
	close(checksum_fd);
	if (payload_read != (size_t)payload_size) {
		free(payload);
		unlink(temp_path);
		log_cache_problem("checksum read failed");
		return -1;
	}
	header.payload_checksum = fnv1a64(payload, (size_t)payload_size);
	header.checksum = header_checksum(&header);
	free(payload);

	int patch_fd = open(temp_path, O_WRONLY);
	if (patch_fd < 0) {
		unlink(temp_path);
		log_cache_problem("reopen for header patch failed");
		return -1;
	}
	if (pwrite(patch_fd, &header, sizeof(header), 0) != (ssize_t)sizeof(header))
		result = -1;
	if (fsync(patch_fd) < 0)
		result = -1;
	if (close(patch_fd) < 0)
		result = -1;
	if (result != 0) {
		unlink(temp_path);
		log_cache_problem("header patch failed");
		return -1;
	}

	if (rename(temp_path, final_path) < 0) {
		unlink(temp_path);
		log_cache_problem("publish failed");
		return -1;
	}

	return 0;
}
