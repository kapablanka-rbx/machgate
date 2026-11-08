#ifndef MACHGATE_DOMAIN_DWARF_UNWIND_H
#define MACHGATE_DOMAIN_DWARF_UNWIND_H

#include <stdint.h>

struct machgate_dwarf_frame {
	uintptr_t function_start;
	uintptr_t function_end;
};

int machgate_decode_dwarf_frame(const void* frame,
			       struct machgate_dwarf_frame* result);

#endif
