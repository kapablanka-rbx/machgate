#include "initializer_context.h"

#include <stdio.h>

static _Thread_local struct {
	const char* kind;
	int index;
	int total;
	uintptr_t address;
} initializer;

void machgate_shim_note_init_context(const char* kind, int index, int total,
	uintptr_t address)
{
	initializer.kind = kind;
	initializer.index = index;
	initializer.total = total;
	initializer.address = address;
}

void machgate_shim_clear_init_context(void)
{
	initializer.kind = NULL;
	initializer.index = -1;
	initializer.total = 0;
	initializer.address = 0;
}

void machgate_trace_initializer_context(void)
{
	if (initializer.kind)
		fprintf(stderr, "libsystem_shim: current initializer kind=%s progress=%d/%d index=%d address=%p\n",
			initializer.kind, initializer.index + 1, initializer.total,
			initializer.index, (void*)initializer.address);
}
