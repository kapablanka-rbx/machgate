/*
 * Compact unwind → DWARF .eh_frame converter for Mach-O on Linux.
 *
 * Parses Apple's __TEXT,__unwind_info section and generates a synthetic
 * DWARF .eh_frame section that can be registered with the system unwinder
 * via __register_frame(). This enables C++ exceptions thrown in Mach-O
 * code to be caught by try/catch handlers.
 */

#ifndef _MACHGATE_EH_FRAME_H_
#define _MACHGATE_EH_FRAME_H_

#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>

struct startup_cache_patch;

/*
 * Register Mach-O exception handling frames with the system unwinder.
 *
 * Parses __TEXT,__unwind_info (compact unwind), converts all entries to
 * DWARF .eh_frame FDEs, and registers them via __register_frame().
 * Also registers the existing __TEXT,__eh_frame section.
 *
 * Must be called after the resolver (GOT entries for personality functions
 * must be patched) but before any Mach-O code runs (__init_offsets, _main).
 *
 * Returns 0 on success, -1 on failure.
 */
int eh_frame_register_macho(void* mh, uintptr_t slide);

/*
 * Record the main executable's path and stat for the startup cache.
 * Call before eh_frame_register_macho.
 */
void eh_frame_note_binary(const char* binary_path,
                          const struct stat* binary_st);

int eh_frame_cache_context(const char** out_path, struct stat* out_st);

/*
 * Publish the most recently generated synthetic .eh_frame plus the given
 * text patch list into the per-binary startup cache.
 */
void eh_frame_cache_capture(const char* binary_path,
                            const struct stat* binary_st,
                            const struct startup_cache_patch* patches,
                            size_t patch_count);

#endif /* _MACHGATE_EH_FRAME_H_ */
