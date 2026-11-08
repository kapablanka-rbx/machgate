#ifndef MACHGATE_INITIALIZER_CONTEXT_H
#define MACHGATE_INITIALIZER_CONTEXT_H

#include <stdint.h>

void machgate_shim_note_init_context(const char* kind, int index, int total,
	uintptr_t address);
void machgate_shim_clear_init_context(void);

__attribute__((visibility("hidden")))
void machgate_trace_initializer_context(void);

#endif
