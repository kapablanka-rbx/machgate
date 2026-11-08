#define _GNU_SOURCE
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

typedef const void* CFAllocatorRef;
typedef const void* CFArrayRef;
typedef const void* CFStringRef;
typedef void* CFRunLoopRef;
typedef void* FSEventStreamRef;
typedef int64_t CFIndex;
typedef double CFTimeInterval;
typedef double CFAbsoluteTime;
typedef uint64_t FSEventStreamEventId;
typedef uint32_t FSEventStreamEventFlags;
typedef uint32_t FSEventStreamCreateFlags;
typedef void (*FSEventStreamCallback)(void*, void*, size_t, void*, const uint32_t*, const uint64_t*);
extern CFIndex CFArrayGetCount(CFArrayRef);
extern const void* CFArrayGetValueAtIndex(CFArrayRef, CFIndex);
extern CFArrayRef CFArrayCreate(CFAllocatorRef, const void**, CFIndex, const void*);
extern CFStringRef CFStringCreateWithCString(CFAllocatorRef, const char*, uint32_t);
extern CFStringRef CFUUIDCreateString(CFAllocatorRef, const void*);
static int read_file_metadata(const char* path, struct stat* result)
{
    return (int)syscall(SYS_newfstatat, AT_FDCWD, path, result, AT_SYMLINK_NOFOLLOW);
}

struct fsevent_stream_context {
	CFIndex version;
	void* info;
	const void* retain;
	const void* release;
	const void* copy_description;
};

struct fsevent_entry {
	char path[PATH_MAX];
	time_t modified_seconds;
	long modified_nanoseconds;
	off_t size;
	int is_directory;
};

struct fsevent_stream {
	FSEventStreamCallback callback;
	struct fsevent_stream_context context;
	CFArrayRef paths;
	uint64_t device;
	FSEventStreamEventId latest_event_id;
	uint8_t started;
	uint8_t invalidated;
	struct fsevent_entry* snapshot;
	size_t snapshot_count;
	size_t snapshot_capacity;
	pthread_mutex_t mutex;
};

enum {
	DARWIN_FSEVENT_FLAG_MUST_SCAN_SUBDIRS = 0x00000008,
	DARWIN_FSEVENT_FLAG_ROOT_CHANGED = 0x00000020,
	DARWIN_FSEVENT_FLAG_ITEM_CREATED = 0x00000100,
	DARWIN_FSEVENT_FLAG_ITEM_REMOVED = 0x00000200,
	DARWIN_FSEVENT_FLAG_ITEM_RENAMED = 0x00000800,
	DARWIN_FSEVENT_FLAG_ITEM_MODIFIED = 0x00001000,
};

static int shim_fsevent_collect_directory(const char* directory,
                                          struct fsevent_entry** entries,
                                          size_t* entry_count,
                                          size_t* entry_capacity)
{
	DIR* dir_stream = opendir(directory);

	if (!dir_stream)
		return 0;

	struct dirent* entry;
	while ((entry = readdir(dir_stream)) != NULL) {
		if (strcmp(entry->d_name, ".") == 0 ||
		    strcmp(entry->d_name, "..") == 0)
			continue;

		char child_path[PATH_MAX];
		if (snprintf(child_path, sizeof(child_path), "%s/%s",
		             directory, entry->d_name) >= (int)sizeof(child_path))
			continue;

		struct stat file_stat;
		if (read_file_metadata(child_path, &file_stat) != 0)
			continue;

		if (*entry_count == *entry_capacity) {
			size_t new_capacity = *entry_capacity * 2 + 16;
			struct fsevent_entry* reallocated =
				realloc(*entries, new_capacity * sizeof(**entries));
			if (!reallocated) {
				closedir(dir_stream);
				return -1;
			}
			*entries = reallocated;
			*entry_capacity = new_capacity;
		}

		struct fsevent_entry* slot =
			&(*entries)[(*entry_count)++];
		snprintf(slot->path, sizeof(slot->path), "%s", child_path);
		slot->modified_seconds = file_stat.st_mtim.tv_sec;
		slot->modified_nanoseconds = file_stat.st_mtim.tv_nsec;
		slot->size = file_stat.st_size;
		slot->is_directory = S_ISDIR(file_stat.st_mode);

		if (slot->is_directory)
			if (shim_fsevent_collect_directory(child_path, entries,
			                               entry_count, entry_capacity) != 0) {
				closedir(dir_stream);
				return -1;
			}
	}
	closedir(dir_stream);
	return 0;
}

static int shim_fsevent_snapshot(struct fsevent_stream* stream)
{
	CFArrayRef paths = stream->paths;
	struct fsevent_entry* entries = NULL;
	size_t entry_count = 0;
	size_t entry_capacity = 0;

	if (!paths)
		return -1;

	for (CFIndex index = 0; index < CFArrayGetCount(paths); index++) {
		const char* watch_path = CFArrayGetValueAtIndex(paths, index);
		struct stat file_stat;

		if (!watch_path || read_file_metadata(watch_path, &file_stat) != 0)
			continue;

		if (entry_count == entry_capacity) {
			size_t new_capacity = entry_capacity * 2 + 16;
			struct fsevent_entry* reallocated =
				realloc(entries, new_capacity * sizeof(*entries));
			if (!reallocated) {
				free(entries);
				return -1;
			}
			entries = reallocated;
			entry_capacity = new_capacity;
		}
		struct fsevent_entry* slot = &entries[entry_count++];
		snprintf(slot->path, sizeof(slot->path), "%s", watch_path);
		slot->modified_seconds = file_stat.st_mtim.tv_sec;
		slot->modified_nanoseconds = file_stat.st_mtim.tv_nsec;
		slot->size = file_stat.st_size;
		slot->is_directory = S_ISDIR(file_stat.st_mode);

		if (slot->is_directory &&
		    shim_fsevent_collect_directory(watch_path, &entries,
		                                   &entry_count,
		                                   &entry_capacity) != 0) {
			free(entries);
			return -1;
		}
	}

	free(stream->snapshot);
	stream->snapshot = entries;
	stream->snapshot_count = entry_count;
	return 0;
}

static int shim_fsevent_entry_compare(const void* left, const void* right)
{
	return strcmp(((const struct fsevent_entry*)left)->path,
	              ((const struct fsevent_entry*)right)->path);
}

static void shim_fsevent_invoke(struct fsevent_stream* stream,
                                char* const* event_paths,
                                const uint32_t* event_flags,
                                size_t event_count)
{
	if (!stream->callback || event_count == 0)
		return;

	uint64_t* event_ids = calloc(event_count, sizeof(*event_ids));
	if (!event_ids)
		return;

	for (size_t index = 0; index < event_count; index++)
		event_ids[index] = ++stream->latest_event_id;
	stream->callback(stream, stream->context.info, event_count,
	                 (void*)event_paths, event_flags, event_ids);
	free(event_ids);
}

static int shim_fsevent_is_watch_root(const struct fsevent_stream* stream,
                                       const char* path)
{
	CFArrayRef paths = stream->paths;
	struct stat file_stat;

	if (!paths || read_file_metadata(path, &file_stat) != 0 || !S_ISDIR(file_stat.st_mode))
		return 0;

	for (CFIndex index = 0; index < CFArrayGetCount(paths); index++)
		if (CFArrayGetValueAtIndex(paths, index) &&
		    strcmp(CFArrayGetValueAtIndex(paths, index), path) == 0)
			return 1;
	return 0;
}

static void shim_fsevent_diff(struct fsevent_stream* stream)
{
	struct fsevent_entry* current = NULL;
	size_t current_count = 0;

	struct fsevent_stream probe;
	probe = *stream;
	probe.snapshot = NULL;
	probe.snapshot_count = 0;
	if (shim_fsevent_snapshot(&probe) != 0)
		return;
	current = probe.snapshot;
	current_count = probe.snapshot_count;

	qsort(stream->snapshot, stream->snapshot_count,
	      sizeof(*stream->snapshot), shim_fsevent_entry_compare);
	qsort(current, current_count, sizeof(*current),
	      shim_fsevent_entry_compare);

	char** event_paths = NULL;
	uint32_t* event_flags = NULL;
	size_t event_count = 0;

	size_t old_index = 0;
	size_t new_index = 0;
	while (old_index < stream->snapshot_count ||
	       new_index < current_count) {
		int comparison;
		if (old_index >= stream->snapshot_count)
			comparison = 1;
		else if (new_index >= current_count)
			comparison = -1;
		else
			comparison = strcmp(stream->snapshot[old_index].path,
			                    current[new_index].path);

		uint32_t flags = 0;
		const char* path = NULL;

		if (comparison < 0) {
			path = stream->snapshot[old_index].path;
			flags = DARWIN_FSEVENT_FLAG_ITEM_REMOVED;
			old_index++;
		} else if (comparison > 0) {
			path = current[new_index].path;
			flags = DARWIN_FSEVENT_FLAG_ITEM_CREATED |
			        DARWIN_FSEVENT_FLAG_ITEM_MODIFIED;
			new_index++;
		} else {
			const struct fsevent_entry* before =
				&stream->snapshot[old_index];
			const struct fsevent_entry* after = &current[new_index];
			if (before->modified_seconds != after->modified_seconds ||
			    before->modified_nanoseconds != after->modified_nanoseconds ||
			    before->size != after->size) {
				path = after->path;
				flags = DARWIN_FSEVENT_FLAG_ITEM_MODIFIED;
			}
			old_index++;
			new_index++;
		}

		if (!path || shim_fsevent_is_watch_root(stream, path))
			continue;

		char** reallocated_paths =
			realloc(event_paths, (event_count + 1) * sizeof(*event_paths));
		uint32_t* reallocated_flags =
			realloc(event_flags, (event_count + 1) * sizeof(*event_flags));
		if (!reallocated_paths || !reallocated_flags) {
			free(reallocated_paths ? reallocated_paths : event_paths);
			free(reallocated_flags ? reallocated_flags : event_flags);
			free(current);
			return;
		}
		event_paths = reallocated_paths;
		event_flags = reallocated_flags;
		event_paths[event_count] = (char*)path;
		event_flags[event_count] = flags;
		event_count++;
	}

	shim_fsevent_invoke(stream, event_paths, event_flags, event_count);
	free(event_paths);
	free(event_flags);

	free(stream->snapshot);
	stream->snapshot = current;
	stream->snapshot_count = current_count;
}

FSEventStreamRef FSEventStreamCreate(CFAllocatorRef allocator,
                                     FSEventStreamCallback callback,
                                     const struct fsevent_stream_context* context,
                                     CFArrayRef paths,
                                     FSEventStreamEventId since_when,
                                     CFTimeInterval latency,
                                     FSEventStreamCreateFlags flags)
{
	(void)allocator;
	(void)latency;
	(void)flags;

	struct fsevent_stream* stream = calloc(1, sizeof(*stream));
	if (!stream)
		return NULL;

	stream->callback = callback;
	if (context)
		stream->context = *context;
	stream->paths = paths;
	stream->latest_event_id = since_when == UINT64_MAX ? 1 : since_when;
	pthread_mutex_init(&stream->mutex, NULL);
	return stream;
}

FSEventStreamRef FSEventStreamCreateRelativeToDevice(
	CFAllocatorRef allocator, FSEventStreamCallback callback,
	const struct fsevent_stream_context* context, uint64_t device,
	CFArrayRef paths, FSEventStreamEventId since_when,
	CFTimeInterval latency, FSEventStreamCreateFlags flags)
{
	struct fsevent_stream* stream = FSEventStreamCreate(
		allocator, callback, context, paths, since_when, latency, flags);
	if (stream)
		stream->device = device;
	return stream;
}

CFStringRef FSEventStreamCopyDescription(FSEventStreamRef stream_ref)
{
	(void)stream_ref;
	return CFStringCreateWithCString(NULL, "MachGate/FSEventStream", 0);
}

CFArrayRef FSEventStreamCopyPathsBeingWatched(FSEventStreamRef stream_ref)
{
	struct fsevent_stream* stream = stream_ref;
	if (!stream || !stream->paths)
		return CFArrayCreate(NULL, NULL, 0, NULL);
	return stream->paths;
}

void FSEventStreamFlushAsync(FSEventStreamRef stream_ref)
{
	(void)stream_ref;
}

void FSEventStreamFlushSync(FSEventStreamRef stream_ref)
{
	struct fsevent_stream* stream = stream_ref;

	if (!stream || !stream->started || stream->invalidated)
		return;

	pthread_mutex_lock(&stream->mutex);
	shim_fsevent_diff(stream);
	pthread_mutex_unlock(&stream->mutex);
}

uint64_t FSEventStreamGetDeviceBeingWatched(FSEventStreamRef stream_ref)
{
	struct fsevent_stream* stream = stream_ref;
	return stream ? stream->device : 0;
}

FSEventStreamEventId FSEventStreamGetLatestEventId(FSEventStreamRef stream_ref)
{
	struct fsevent_stream* stream = stream_ref;
	return stream ? stream->latest_event_id : 0;
}

void FSEventStreamInvalidate(FSEventStreamRef stream_ref)
{
	struct fsevent_stream* stream = stream_ref;
	if (stream)
		stream->invalidated = 1;
}

void FSEventStreamRelease(FSEventStreamRef stream_ref)
{
	struct fsevent_stream* stream = stream_ref;
	if (!stream)
		return;
	free(stream->snapshot);
	pthread_mutex_destroy(&stream->mutex);
	free(stream);
}

void FSEventStreamScheduleWithRunLoop(FSEventStreamRef stream_ref,
                                      CFRunLoopRef run_loop,
                                      CFStringRef run_loop_mode)
{
	(void)stream_ref;
	(void)run_loop;
	(void)run_loop_mode;
}

void FSEventStreamSetDispatchQueue(FSEventStreamRef stream_ref, void* queue)
{
	(void)stream_ref;
	(void)queue;
}

int FSEventStreamStart(FSEventStreamRef stream_ref)
{
	struct fsevent_stream* stream = stream_ref;
	if (!stream || stream->invalidated)
		return 0;

	pthread_mutex_lock(&stream->mutex);
	int snapshot_result = shim_fsevent_snapshot(stream);
	pthread_mutex_unlock(&stream->mutex);
	if (snapshot_result != 0)
		return 0;

	stream->started = 1;
	return 1;
}

void FSEventStreamStop(FSEventStreamRef stream_ref)
{
	struct fsevent_stream* stream = stream_ref;
	if (stream)
		stream->started = 0;
}

CFStringRef FSEventsCopyUUIDForDevice(uint64_t device)
{
	(void)device;
	return CFUUIDCreateString(NULL, NULL);
}

FSEventStreamEventId FSEventsGetCurrentEventId(void)
{
	return 1;
}

FSEventStreamEventId FSEventsGetLastEventIdForDeviceBeforeTime(
	uint64_t device, CFAbsoluteTime time)
{
	(void)device;
	(void)time;
	return 1;
}
