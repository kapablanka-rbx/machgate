#ifndef _RESOLVER_H_
#define _RESOLVER_H_

#include <stdint.h>
#include <stdbool.h>

/*
 * Resolve chained fixups in a loaded Mach-O binary.
 *
 * Parses LC_DYLD_CHAINED_FIXUPS, walks the pointer chains in __DATA_CONST
 * and __DATA segments, and patches bind slots with native Linux .so symbols
 * via dlopen/dlsym. Also applies rebase fixups for ASLR slide.
 *
 * Parameters:
 *   mh        - pointer to the mapped mach_header_64 in memory
 *   slide     - ASLR slide applied during loading (usually mh - preferred_load_address)
 *   map_file  - path to dylib mapping config file (NULL for default behavior)
 *
 * Returns 0 on success, -1 on error.
 */
int resolver_resolve_fixups(void* mh, uintptr_t slide, const char* map_file);

/*
 * Look up a symbol in a loaded Mach-O binary's nlist symbol table.
 * The name should include the Mach-O leading underscore (e.g. "_main").
 * Returns the address (with slide applied) or 0 if not found.
 */
uintptr_t resolver_lookup_symbol(void* mh, uintptr_t slide, const char* name);

/*
 * Look up a public section-defined symbol in a loaded Mach-O binary's nlist
 * symbol table. Local, private-external, undefined, debug, and stub-section
 * symbols are ignored.
 */
uintptr_t resolver_lookup_external_definition(void* mh, uintptr_t slide,
                                               const char* name);

/*
 * Find the slid memory extent of a named symbol.
 * On success sets *out_start to the symbol's slid address and *out_end to
 * the slid address of the next N_SECT symbol in the text (or the end of
 * that symbol's containing section if it's the last one).
 * Returns 0 on success, -1 if the symbol wasn't found.
 */
int resolver_symbol_extent(void* mh, uintptr_t slide, const char* name,
                           uintptr_t* out_start, uintptr_t* out_end);

struct resolver_bind_slot_info {
	uintptr_t slot_addr;
	uintptr_t resolved;
	const char* context;
	const char* sym_name;
	const char* lookup_name;
	const char* dylib_name;
	const char* source_kind;
	const char* source_path;
};

int resolver_lookup_bind_slot(uintptr_t slot_addr,
                              struct resolver_bind_slot_info* out_info);

#endif /* _RESOLVER_H_ */
