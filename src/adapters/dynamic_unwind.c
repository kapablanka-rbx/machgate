#include "adapters/dynamic_unwind.h"
#include "domain/dwarf_unwind.h"
#include "ports/native_unwind.h"

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct dynamic_frame_registration {
	const void* frame;
	struct machgate_dwarf_frame location;
	uint8_t header[32];
	struct dynamic_frame_registration* next;
};

static pthread_mutex_t registration_lock = PTHREAD_MUTEX_INITIALIZER;
static struct dynamic_frame_registration* registrations;

static void initialize_header(struct dynamic_frame_registration* registration)
{
	uint8_t* header = registration->header;
	header[0] = 1;
	header[1] = 0;
	header[2] = 3;
	header[3] = 0;
	uintptr_t frame = (uintptr_t)registration->frame;
	uint32_t count = 1;
	memcpy(header + 4, &frame, sizeof(frame));
	memcpy(header + 12, &count, sizeof(count));
	memcpy(header + 16, &registration->location.function_start, sizeof(uintptr_t));
	memcpy(header + 24, &frame, sizeof(frame));
}

void machgate_guest_register_frame(const void* frame)
{
	struct machgate_dwarf_frame location;
	if (machgate_decode_dwarf_frame(frame, &location) != 0)
		return;
	struct dynamic_frame_registration* result = calloc(1, sizeof(*result));
	if (!result)
		return;
	result->frame = frame;
	result->location = location;
	initialize_header(result);
	pthread_mutex_lock(&registration_lock);
	for (struct dynamic_frame_registration* entry = registrations; entry;
	     entry = entry->next) {
		if (entry->frame == frame) {
			pthread_mutex_unlock(&registration_lock);
			free(result);
			return;
		}
	}
	result->next = registrations;
	registrations = result;
	pthread_mutex_unlock(&registration_lock);
}

void machgate_guest_deregister_frame(const void* frame)
{
	pthread_mutex_lock(&registration_lock);
	struct dynamic_frame_registration** link = &registrations;
	while (*link && (*link)->frame != frame)
		link = &(*link)->next;
	struct dynamic_frame_registration* result = *link;
	if (result)
		*link = result->next;
	pthread_mutex_unlock(&registration_lock);
	free(result);
}

int machgate_find_dynamic_unwind(void* address, struct dl_find_object* result)
{
	uintptr_t value = (uintptr_t)address;
	pthread_mutex_lock(&registration_lock);
	for (struct dynamic_frame_registration* entry = registrations; entry;
	     entry = entry->next) {
		if (value < entry->location.function_start ||
		    value >= entry->location.function_end)
			continue;
		memset(result, 0, sizeof(*result));
		result->dlfo_map_start = (void*)entry->location.function_start;
		result->dlfo_map_end = (void*)entry->location.function_end;
		result->dlfo_eh_frame = entry->header;
		pthread_mutex_unlock(&registration_lock);
		return 0;
	}
	pthread_mutex_unlock(&registration_lock);
	return -1;
}
