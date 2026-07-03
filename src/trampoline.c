/*
 * Trampoline patcher for statically-linked functions in Mach-O binaries.
 *
 * Overwrites function entry points with 16-byte arm64 trampolines
 * that jump to native Linux .so implementations.
 *
 * Trampoline (16 bytes):
 *   ldr x16, #8       // load target address from literal pool
 *   br  x16            // branch to native function
 *   .quad <target>     // 8-byte native function address
 *
 * x16 (IP0) is the intra-procedure-call scratch register on arm64,
 * safe to clobber at function entry.
 */

#include "trampoline.h"
#include "macho_defs.h"
#include "gdb_jit.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <ucontext.h>

/* arm64 trampoline island approach:
 *
 * Problem: inline 16-byte trampolines overflow into adjacent functions
 * when the patched function is smaller than 16 bytes (73 bgfx functions).
 *
 * Solution: write only a 4-byte B (branch) instruction at the function
 * entry, targeting a 16-byte island in a nearby executable pool.
 * The island contains the full indirect branch: ldr x16, #8; br x16; .quad addr
 *
 * B instruction range: ±128MB — pool must be within 128MB of __TEXT.
 */

#define TRAMPOLINE_LDR_X16  0x58000050  /* ldr x16, #8 */
#define TRAMPOLINE_BR_X16   0xd61f0200  /* br x16 */
#define ISLAND_SIZE         16

struct trampoline_island {
	uint32_t ldr_x16;      /* ldr x16, #8 */
	uint32_t br_x16;       /* br x16 */
	uint64_t target_addr;  /* native function address */
};

/* Island pool — executable memory near __TEXT */
#define ISLAND_POOL_SIZE (1024 * 1024)  /* 1MB, enough for ~65K islands */
static uint8_t* island_pool = NULL;
static size_t island_pool_size = 0;
static size_t island_pool_used = 0;

static const char* getenv_compat(const char* name, const char* legacy_name)
{
	const char* value = getenv(name);
	if (value)
		return value;
	return getenv(legacy_name);
}

void trampoline_set_pool(void* base, size_t size)
{
	island_pool = (uint8_t*)base;
	island_pool_size = size;
	island_pool_used = 0;
	machgate_log_startup("trampoline: island pool at %p (%zu KB)\n",
	                     base, size / 1024);
}

static int init_island_pool(uintptr_t text_base, size_t text_size)
{
	(void)text_base; (void)text_size;
	/* Pool must be set up by caller via trampoline_set_pool() before
	 * trampoline_apply(). The loader preallocates it adjacent to __TEXT. */
	if (island_pool) return 0;
	fprintf(stderr, "trampoline: no island pool — call trampoline_set_pool() first\n");
	return -1;
}

/* Allocate an island and return its address, or 0 on failure */
static uintptr_t alloc_island(uintptr_t native_addr)
{
	if (!island_pool || island_pool_used + ISLAND_SIZE > island_pool_size)
		return 0;

	struct trampoline_island* island =
		(struct trampoline_island*)(island_pool + island_pool_used);
	island->ldr_x16 = TRAMPOLINE_LDR_X16;
	island->br_x16 = TRAMPOLINE_BR_X16;
	island->target_addr = native_addr;

	uintptr_t addr = (uintptr_t)island;
	island_pool_used += ISLAND_SIZE;
	return addr;
}

/* Shared return-0 stub island (allocated once, reused for all stubs) */
static uintptr_t stub_island_addr = 0;

static uintptr_t get_stub_island(void)
{
	if (stub_island_addr) return stub_island_addr;
	if (!island_pool || island_pool_used + ISLAND_SIZE > island_pool_size)
		return 0;

	uint32_t* code = (uint32_t*)(island_pool + island_pool_used);
	code[0] = 0x52800000;  /* mov w0, #0 */
	code[1] = 0xD65F03C0;  /* ret */
	code[2] = 0xD503201F;  /* nop */
	code[3] = 0xD503201F;  /* nop */

	stub_island_addr = (uintptr_t)code;
	island_pool_used += ISLAND_SIZE;
	return stub_island_addr;
}

/*
 * Try macOS ↔ Linux C++ mangling variants for the same function.
 * macOS uint64_t = unsigned long long (mangling 'y'),
 * Linux uint64_t = unsigned long (mangling 'm'). Both 8 bytes on aarch64.
 * Same for int64_t: macOS 'x' (long long), Linux 'l' (long).
 *
 * Can't do global replacement because 'y' appears inside name components
 * (e.g. "Memory" → "6MemoryE"). Instead, try all 2^n subsets of y-positions,
 * replacing each subset with 'm'. With n typically 1-3, this is fast.
 */
static void* try_mangling_variants(void* lib, const char* name)
{
	size_t len = strlen(name);

	/* Collect positions of 'y' characters */
	int y_pos[16];
	int ny = 0;
	for (size_t i = 0; i < len && ny < 16; i++) {
		if (name[i] == 'y') y_pos[ny++] = i;
	}

	if (ny == 0) return NULL;

	char* buf = malloc(len + 1);
	if (!buf) return NULL;

	/* Try all non-empty subsets of y→m replacements */
	for (int mask = 1; mask < (1 << ny); mask++) {
		memcpy(buf, name, len + 1);
		for (int j = 0; j < ny; j++) {
			if (mask & (1 << j))
				buf[y_pos[j]] = 'm';
		}
		void* addr = dlsym(lib, buf);
		if (addr) {
			fprintf(stderr, "trampoline: mangling fallback: %s -> %s\n", name, buf);
			free(buf);
			return addr;
		}
	}

	/* Reverse direction: try subsets of m→y */
	int m_pos[16];
	int nm = 0;
	for (size_t i = 0; i < len && nm < 16; i++) {
		if (name[i] == 'm') m_pos[nm++] = i;
	}

	for (int mask = 1; mask < (1 << nm); mask++) {
		memcpy(buf, name, len + 1);
		for (int j = 0; j < nm; j++) {
			if (mask & (1 << j))
				buf[m_pos[j]] = 'y';
		}
		void* addr = dlsym(lib, buf);
		if (addr) {
			fprintf(stderr, "trampoline: mangling fallback: %s -> %s\n", name, buf);
			free(buf);
			return addr;
		}
	}

	free(buf);
	return NULL;
}

/*
 * Abort handler for un-trampolined Mach-O function calls.
 * Any call to a bgfx/SDL2 function still pointing to Mach-O code is a bug.
 */
static void __attribute__((noreturn)) trampoline_abort(const char* symbol_name)
{
	fprintf(stderr, "\nFATAL: un-trampolined Mach-O call to %s\n", symbol_name);
	abort();
}

/* Shared abort caller island: ldr x16, #8; br x16; .quad &trampoline_abort */
static uintptr_t abort_caller_addr = 0;

static uintptr_t get_abort_caller(void)
{
	if (abort_caller_addr) return abort_caller_addr;
	if (!island_pool || island_pool_used + ISLAND_SIZE > island_pool_size)
		return 0;

	struct trampoline_island* island =
		(struct trampoline_island*)(island_pool + island_pool_used);
	island->ldr_x16 = TRAMPOLINE_LDR_X16;
	island->br_x16 = TRAMPOLINE_BR_X16;
	island->target_addr = (uintptr_t)&trampoline_abort;

	abort_caller_addr = (uintptr_t)island;
	island_pool_used += ISLAND_SIZE;
	return abort_caller_addr;
}

/*
 * Allocate an abort trap island for a specific symbol.
 * Layout (16 bytes): ldr x0, #8; b <abort_caller>; .quad name_ptr
 * When called, x0 = symbol name pointer, then branches to shared
 * abort caller which calls trampoline_abort(x0).
 */
static uintptr_t make_abort_island(const char* symbol_name)
{
	uintptr_t caller = get_abort_caller();
	if (!caller) return 0;

	if (!island_pool || island_pool_used + ISLAND_SIZE > island_pool_size)
		return 0;

	uint32_t* code = (uint32_t*)(island_pool + island_pool_used);
	uintptr_t island_addr = (uintptr_t)code;

	/* ldr x0, #+8 — load name pointer from 8 bytes ahead */
	code[0] = 0x58000040;

	/* b <abort_caller> */
	int64_t offset = (int64_t)caller - (int64_t)(island_addr + 4);
	if (offset < -128*1024*1024 || offset >= 128*1024*1024 || (offset & 3)) {
		fprintf(stderr, "trampoline: abort caller too far from island\n");
		return 0;
	}
	uint32_t imm26 = (uint32_t)((offset >> 2) & 0x03FFFFFF);
	code[1] = 0x14000000 | imm26;

	/* .quad symbol_name — pointer to the name string (in Mach-O strtab, always valid) */
	*(uint64_t*)&code[2] = (uint64_t)symbol_name;

	island_pool_used += ISLAND_SIZE;
	return island_addr;
}

/* Convert a file offset to a memory address using segment mappings */
static void* fileoff_to_mem(void* mh, uintptr_t slide, uint32_t fileoff)
{
	struct mach_header_64* header = (struct mach_header_64*)mh;
	uint8_t* cmd_ptr = (uint8_t*)(header + 1);

	for (uint32_t i = 0; i < header->ncmds; i++) {
		struct load_command* lc = (struct load_command*)cmd_ptr;
		if (lc->cmd == LC_SEGMENT_64) {
			struct segment_command_64* seg = (struct segment_command_64*)lc;
			if (fileoff >= seg->fileoff && fileoff < seg->fileoff + seg->filesize) {
				return (void*)(seg->vmaddr + slide + (fileoff - seg->fileoff));
			}
		}
		cmd_ptr += lc->cmdsize;
	}
	return NULL;
}

/* Make a range of __TEXT pages writable for patching.
 * Called once before writing all trampolines, then restored after. */
static long sys_page_size;
static uintptr_t text_patch_base;
static size_t text_patch_size;

static int make_text_writable(void* mh, uintptr_t slide)
{
	struct mach_header_64* header = (struct mach_header_64*)mh;
	uint8_t* cmd_ptr = (uint8_t*)(header + 1);

	sys_page_size = sysconf(_SC_PAGESIZE);
	if (sys_page_size <= 0) sys_page_size = 4096;

	/* Find __TEXT segment and make it writable */
	for (uint32_t i = 0; i < header->ncmds; i++) {
		struct load_command* lc = (struct load_command*)cmd_ptr;
		if (lc->cmd == LC_SEGMENT_64) {
			struct segment_command_64* seg = (struct segment_command_64*)lc;
			if (strcmp(seg->segname, "__TEXT") == 0) {
				text_patch_base = (seg->vmaddr + slide) & ~(uintptr_t)(sys_page_size - 1);
				uintptr_t end = seg->vmaddr + slide + seg->vmsize;
				end = (end + sys_page_size - 1) & ~(uintptr_t)(sys_page_size - 1);
				text_patch_size = end - text_patch_base;

				if (mprotect((void*)text_patch_base, text_patch_size,
				             PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
					fprintf(stderr, "trampoline: mprotect __TEXT writable failed: %s\n",
							strerror(errno));
					return -1;
				}
				/* Allocate island pool near __TEXT */
				if (init_island_pool(text_patch_base, text_patch_size) < 0) {
					fprintf(stderr, "trampoline: failed to allocate island pool\n");
					return -1;
				}
				return 0;
			}
		}
		cmd_ptr += lc->cmdsize;
	}
	fprintf(stderr, "trampoline: __TEXT segment not found\n");
	return -1;
}

static void restore_text_protection(void)
{
	if (text_patch_base && text_patch_size) {
		mprotect((void*)text_patch_base, text_patch_size, PROT_READ | PROT_EXEC);
	}
}

/* Write a trampoline at the given address.
 * Writes a 4-byte B instruction to an island containing the full indirect branch. */
static int write_trampoline(uintptr_t func_addr, uintptr_t native_addr)
{
	uintptr_t island_addr = alloc_island(native_addr);
	if (!island_addr) {
		fprintf(stderr, "trampoline: island pool exhausted\n");
		return -1;
	}

	/* Compute B offset: (target - pc) / 4, must fit in 26-bit signed field */
	int64_t offset = (int64_t)island_addr - (int64_t)func_addr;
	if (offset < -128*1024*1024 || offset >= 128*1024*1024 || (offset & 3) != 0) {
		fprintf(stderr, "trampoline: island at %p too far from %p (offset %ld)\n",
		        (void*)island_addr, (void*)func_addr, (long)offset);
		return -1;
	}

	/* Encode: B imm26 — opcode 0x14000000 | (imm26 & 0x03FFFFFF) */
	uint32_t imm26 = (uint32_t)((offset >> 2) & 0x03FFFFFF);
	uint32_t b_insn = 0x14000000 | imm26;

	/* Write single 4-byte instruction at function entry */
	*(uint32_t*)func_addr = b_insn;

	/* Flush instruction cache for the patched site */
	__builtin___clear_cache((char*)func_addr, (char*)(func_addr + 4));

	return 0;
}

/* Check if a symbol name matches any of the given prefixes */
static int matches_prefix(const char* name, const char** prefixes, int num_prefixes)
{
	for (int i = 0; i < num_prefixes; i++) {
		if (strncmp(name, prefixes[i], strlen(prefixes[i])) == 0)
			return 1;
	}
	return 0;
}

/* Check if a symbol has an override, return target or NULL */
static void* find_override(const char* name, trampoline_override_t* overrides, int num_overrides)
{
	for (int i = 0; i < num_overrides; i++) {
		if (strcmp(name, overrides[i].symbol_name) == 0)
			return overrides[i].target;
	}
	return NULL;
}

int trampoline_patch_lib(void* mh, uintptr_t slide,
                         const char* lib_path,
                         const char** prefixes, int num_prefixes,
                         trampoline_override_t* overrides, int num_overrides)
{
	struct mach_header_64* header = (struct mach_header_64*)mh;
	uint8_t* cmd_ptr = (uint8_t*)(header + 1);

	/* Find LC_SYMTAB */
	struct symtab_command* symtab = NULL;
	for (uint32_t i = 0; i < header->ncmds; i++) {
		struct load_command* lc = (struct load_command*)cmd_ptr;
		if (lc->cmd == LC_SYMTAB) {
			symtab = (struct symtab_command*)lc;
			break;
		}
		cmd_ptr += lc->cmdsize;
	}

	if (!symtab) {
		fprintf(stderr, "trampoline: no LC_SYMTAB found\n");
		return -1;
	}

	/* STUB mode: redirect all matching symbols to a return-0 stub */
	int stub_mode = (lib_path && strcmp(lib_path, "STUB") == 0);

	/* Load the native library (skip in stub mode) */
	void* native_lib = NULL;
	if (!stub_mode) {
		native_lib = dlopen(lib_path, RTLD_LAZY | RTLD_GLOBAL);
		if (!native_lib) {
			fprintf(stderr, "trampoline: cannot load %s: %s\n", lib_path, dlerror());
			return -1;
		}
	}

	/* Get nlist array and string table from mapped memory */
	struct nlist_64* nlist_arr = (struct nlist_64*)fileoff_to_mem(mh, slide, symtab->symoff);
	char* strtab = (char*)fileoff_to_mem(mh, slide, symtab->stroff);

	if (!nlist_arr || !strtab) {
		fprintf(stderr, "trampoline: cannot locate symbol table in memory\n");
		return -1;
	}

	/* Make __TEXT writable for all patches at once */
	if (make_text_writable(mh, slide) < 0) {
		return -1;
	}

	int patched = 0;
	int mangling_fixed = 0;
	int trapped = 0;

	for (uint32_t i = 0; i < symtab->nsyms; i++) {
		struct nlist_64* sym = &nlist_arr[i];

		/* Skip non-global, non-defined symbols */
		if ((sym->n_type & N_TYPE) != N_SECT) continue;
		if (!(sym->n_type & N_EXT)) continue;
		if (sym->n_type & N_STAB) continue;

		/* Get symbol name */
		if (sym->n_strx >= symtab->strsize) continue;
		const char* name = strtab + sym->n_strx;

		/* Check prefix match */
		if (!matches_prefix(name, prefixes, num_prefixes)) continue;

		/* Compute address in mapped memory */
		uintptr_t func_addr = sym->n_value + slide;

		/* Only trampoline symbols in __TEXT — skip data symbols */
		if (func_addr < text_patch_base ||
		    func_addr >= text_patch_base + text_patch_size) {
			continue;
		}

		/* Check for override first */
		void* target = find_override(name, overrides, num_overrides);

		if (!target && stub_mode) {
			/* In stub mode, all symbols go to return-0 stub */
			target = (void*)get_stub_island();
		}

		if (!target && native_lib) {
			/* Strip leading underscore for dlsym lookup */
			const char* lookup_name = name;
			if (lookup_name[0] == '_') lookup_name++;

			target = dlsym(native_lib, lookup_name);

			/* Fallback: try macOS↔Linux C++ mangling variants */
			if (!target && strncmp(lookup_name, "_ZN", 3) == 0) {
				target = try_mangling_variants(native_lib, lookup_name);
				if (target) mangling_fixed++;
			}
		}

		if (!target && !stub_mode) {
			/* No native match — trap this function so we know immediately
			 * if Mach-O code tries to call it. */
			uintptr_t abort_island = make_abort_island(name);
			if (abort_island) {
				extern int machgate_verbose;
				if (machgate_verbose)
					fprintf(stderr, "trampoline: TRAPPED (no native match): %s\n", name);
				if (write_trampoline(func_addr, abort_island) == 0) {
					trapped++;
				}
			}
			continue;
		}

		if (!target) {
			/* STUB mode with no target — shouldn't happen, but skip */
			continue;
		}

		/* Write trampoline */
		if (write_trampoline(func_addr, (uintptr_t)target) == 0) {
			patched++;
		} else {
			fprintf(stderr, "trampoline: failed to patch %s at %p\n", name, (void*)func_addr);
		}
	}

	/* Restore __TEXT to read+execute */
	restore_text_protection();

	fprintf(stderr, "trampoline: %d patched, %d mangling-fixed, %d trapped from %s\n",
			patched, mangling_fixed, trapped, lib_path);

	/* Don't dlclose — the game needs the native library to stay loaded */
	return patched;
}

int trampoline_patch_overrides(void* mh, uintptr_t slide, void* override_handle,
                               int match_local)
{
	struct mach_header_64* header = (struct mach_header_64*)mh;
	uint8_t* cmd_ptr = (uint8_t*)(header + 1);

	/* Find LC_SYMTAB */
	struct symtab_command* symtab = NULL;
	for (uint32_t i = 0; i < header->ncmds; i++) {
		struct load_command* lc = (struct load_command*)cmd_ptr;
		if (lc->cmd == LC_SYMTAB) {
			symtab = (struct symtab_command*)lc;
			break;
		}
		cmd_ptr += lc->cmdsize;
	}

	if (!symtab) {
		fprintf(stderr, "trampoline: override: no LC_SYMTAB found\n");
		return -1;
	}

	struct nlist_64* nlist_arr = (struct nlist_64*)fileoff_to_mem(mh, slide, symtab->symoff);
	char* strtab = (char*)fileoff_to_mem(mh, slide, symtab->stroff);

	if (!nlist_arr || !strtab) {
		fprintf(stderr, "trampoline: override: cannot locate symbol table\n");
		return -1;
	}

	/* Make __TEXT writable (idempotent if already writable from a prior call) */
	if (make_text_writable(mh, slide) < 0)
		return -1;

	int patched = 0;

	for (uint32_t i = 0; i < symtab->nsyms; i++) {
		struct nlist_64* sym = &nlist_arr[i];

		if ((sym->n_type & N_TYPE) != N_SECT) continue;
		if (!match_local && !(sym->n_type & N_EXT)) continue;
		if (sym->n_type & N_STAB) continue;
		if (sym->n_strx >= symtab->strsize) continue;

		const char* name = strtab + sym->n_strx;
		uintptr_t func_addr = sym->n_value + slide;

		/* Only patch symbols in __TEXT */
		if (func_addr < text_patch_base ||
		    func_addr >= text_patch_base + text_patch_size)
			continue;

		/* Strip one leading underscore (Mach-O convention) for dlsym */
		const char* lookup_name = name;
		if (lookup_name[0] == '_') lookup_name++;

		void* target = dlsym(override_handle, lookup_name);
		if (!target) continue;

		if (write_trampoline(func_addr, (uintptr_t)target) == 0) {
			fprintf(stderr, "trampoline: override: %s -> %p\n", name, target);
			patched++;
		}
	}

	restore_text_protection();

	fprintf(stderr, "trampoline: override: %d functions replaced%s\n",
	        patched, match_local ? " (incl. local symbols)" : "");
	return patched;
}

/* Legacy env-var based API */
int trampoline_patch(void* mh, uintptr_t slide)
{
	const char* lib_path = getenv_compat("MACHGATE_TRAMPOLINE_LIB",
	                                     "MACHISMO_TRAMPOLINE_LIB");
	const char* prefix = getenv_compat("MACHGATE_TRAMPOLINE_PREFIX",
	                                   "MACHISMO_TRAMPOLINE_PREFIX");

	if (!lib_path) lib_path = "libSDL2-2.0.so.0";
	if (!prefix) prefix = "_SDL_";

	const char* prefixes[] = { prefix };
	return trampoline_patch_lib(mh, slide, lib_path, prefixes, 1, NULL, 0);
}

/*
 * Stale __DATA access detection.
 *
 * After all trampolines are applied, find library data symbols in __DATA
 * and guard pages that contain ONLY library symbols. Any access to a guarded
 * page means un-trampolined or inlined code is touching Mach-O library state
 * instead of native .so state — always a bug.
 */

/* Track guarded page ranges for SIGSEGV handler */
#define MAX_GUARDED_PAGES 4096
static struct {
	uintptr_t base;
	size_t size;
} guarded_pages[MAX_GUARDED_PAGES];
static int num_guarded_pages = 0;
static void* signal_diag_mh = NULL;
static uintptr_t signal_diag_slide = 0;
static int signal_diag_installed = 0;
static const char* signal_diag_init_kind = NULL;
static int signal_diag_init_index = -1;
static int signal_diag_init_total = 0;
static uintptr_t signal_diag_init_address = 0;

void trampoline_note_init_context(const char* kind, int index, int total,
                                  uintptr_t address)
{
	signal_diag_init_kind = kind;
	signal_diag_init_index = index;
	signal_diag_init_total = total;
	signal_diag_init_address = address;
}

void trampoline_clear_init_context(void)
{
	signal_diag_init_kind = NULL;
	signal_diag_init_index = -1;
	signal_diag_init_total = 0;
	signal_diag_init_address = 0;
}

static uintptr_t ucontext_pc(const void* ucontext)
{
	const ucontext_t* uc = (const ucontext_t*)ucontext;

#ifdef __aarch64__
	return uc->uc_mcontext.pc;
#elif defined(__x86_64__)
	return uc->uc_mcontext.gregs[REG_RIP];
#else
	(void)uc;
	return 0;
#endif
}

static uintptr_t ucontext_reg(const void* ucontext, int reg)
{
	const ucontext_t* uc = (const ucontext_t*)ucontext;

#ifdef __aarch64__
	if (reg >= 0 && reg <= 30)
		return uc->uc_mcontext.regs[reg];
	if (reg == 31)
		return uc->uc_mcontext.sp;
	return 0;
#elif defined(__x86_64__)
	switch (reg) {
	case 29: return uc->uc_mcontext.gregs[REG_RBP];
	case 30: return 0;
	case 31: return uc->uc_mcontext.gregs[REG_RSP];
	default: return 0;
	}
#else
	(void)uc;
	(void)reg;
	return 0;
#endif
}

static void print_signal_macho_context(const char* label, uintptr_t pc,
                                       uintptr_t highlight)
{
	if (!signal_diag_mh)
		return;
	if (!pc) {
		fprintf(stderr, "machgate: guest context %s=(nil)\n", label);
		return;
	}

	struct mach_header_64* header = (struct mach_header_64*)signal_diag_mh;
	uint8_t* cmd_ptr = (uint8_t*)(header + 1);

	for (uint32_t i = 0; i < header->ncmds; i++) {
		struct load_command* lc = (struct load_command*)cmd_ptr;
		if (lc->cmd != LC_SEGMENT_64) {
			cmd_ptr += lc->cmdsize;
			continue;
		}

		struct segment_command_64* seg = (struct segment_command_64*)lc;
		uintptr_t seg_start = seg->vmaddr + signal_diag_slide;
		uintptr_t seg_end = seg_start + seg->vmsize;
		if (pc < seg_start || pc >= seg_end) {
			cmd_ptr += lc->cmdsize;
			continue;
		}

		const char* section_name = "<none>";
		uint64_t section_fileoff = seg->fileoff + (pc - seg_start);
		struct section_64* sect = (struct section_64*)(seg + 1);
		for (uint32_t section_index = 0; section_index < seg->nsects; section_index++, sect++) {
			uintptr_t section_start = sect->addr + signal_diag_slide;
			uintptr_t section_end = section_start + sect->size;
			if (pc >= section_start && pc < section_end) {
				section_name = sect->sectname;
				section_fileoff = sect->offset + (pc - section_start);
				break;
			}
		}

		const char* symbol = gdb_jit_lookup_addr(pc);
		uintptr_t symbol_addr = symbol ? gdb_jit_lookup_name(symbol) : 0;
		unsigned long symbol_offset = symbol_addr ? (unsigned long)(pc - symbol_addr) : 0;
		uint32_t prev_insn = 0;
		uint32_t insn = 0;
		uint32_t next_insn = 0;
		if (pc >= seg_start + 4 && pc + 8 <= seg_end) {
			prev_insn = *(uint32_t*)(pc - 4);
			insn = *(uint32_t*)pc;
			next_insn = *(uint32_t*)(pc + 4);
		} else if (pc + 4 <= seg_end) {
			insn = *(uint32_t*)pc;
		}

		if (symbol) {
			fprintf(stderr,
			        "machgate: guest context %s=%p vmaddr=0x%lx symbol=%s+0x%lx segment=%.*s section=%.*s fileoff=0x%llx insn=0x%08x prev=0x%08x next=0x%08x\n",
			        label,
			        (void*)pc, (unsigned long)(pc - signal_diag_slide),
			        symbol, symbol_offset, 16, seg->segname, 16, section_name,
			        (unsigned long long)section_fileoff,
			        insn, prev_insn, next_insn);
		} else {
			fprintf(stderr,
			        "machgate: guest context %s=%p vmaddr=0x%lx segment=%.*s section=%.*s fileoff=0x%llx insn=0x%08x prev=0x%08x next=0x%08x\n",
			        label,
			        (void*)pc, (unsigned long)(pc - signal_diag_slide),
			        16, seg->segname, 16, section_name,
			        (unsigned long long)section_fileoff,
			        insn, prev_insn, next_insn);
		}
		if ((pc & 3u) == 0 && highlight) {
			uintptr_t window_start = pc >= seg_start + 20 ? pc - 20 : seg_start;
			uintptr_t window_end = pc + 16 <= seg_end ? pc + 16 : seg_end;
			window_start &= ~(uintptr_t)3u;
			window_end &= ~(uintptr_t)3u;
			for (uintptr_t cursor = window_start; cursor < window_end; cursor += 4) {
				uint64_t cursor_fileoff = section_fileoff;
				if (cursor >= pc)
					cursor_fileoff += cursor - pc;
				else
					cursor_fileoff -= pc - cursor;
				fprintf(stderr,
				        "machgate: guest context %s window %c %p vmaddr=0x%lx fileoff=0x%llx insn=0x%08x\n",
				        label, cursor == highlight ? '>' : ' ',
				        (void*)cursor,
				        (unsigned long)(cursor - signal_diag_slide),
				        (unsigned long long)cursor_fileoff,
				        *(uint32_t*)cursor);
			}
		}
		return;
	}

	fprintf(stderr, "machgate: guest context %s=%p outside main Mach-O image\n",
	        label, (void*)pc);
}

static void print_signal_init_context(void)
{
	if (!signal_diag_init_kind)
		return;

	fprintf(stderr,
	        "machgate: current initializer kind=%s progress=%d/%d index=%d address=%p\n",
	        signal_diag_init_kind, signal_diag_init_index + 1,
	        signal_diag_init_total, signal_diag_init_index,
	        (void*)signal_diag_init_address);
}

static void print_signal_indirect_branch(uintptr_t call_site, const void* ucontext)
{
	if (!call_site)
		return;

	uint32_t insn = *(uint32_t*)call_site;
	uint32_t masked = insn & 0xfffffc1fu;
	const char* mnemonic = NULL;

	if (masked == 0xd61f0000u)
		mnemonic = "br";
	else if (masked == 0xd63f0000u)
		mnemonic = "blr";
	else if (masked == 0xd65f0000u)
		mnemonic = "ret";

	if (!mnemonic)
		return;

	int reg = (int)((insn >> 5) & 0x1f);
	fprintf(stderr,
	        "machgate: signal callsite %p insn=0x%08x %s x%d target=%p\n",
	        (void*)call_site, insn, mnemonic, reg,
	        (void*)ucontext_reg(ucontext, reg));
}

static void stale_data_sigsegv(int sig, siginfo_t* info, void* ucontext)
{
	(void)sig;
	uintptr_t fault_addr = (uintptr_t)info->si_addr;

	/* Check if fault is in a guarded page */
	for (int i = 0; i < num_guarded_pages; i++) {
		if (fault_addr >= guarded_pages[i].base &&
		    fault_addr < guarded_pages[i].base + guarded_pages[i].size) {
			uintptr_t pc = ucontext_pc(ucontext);
			fprintf(stderr,
				"\nFATAL: stale Mach-O data access at %p from PC=%p\n"
				"This means un-trampolined or inlined code is accessing\n"
				"Mach-O library state instead of native .so state.\n",
				(void*)fault_addr, (void*)pc);
			abort();
		}
	}

	if (getenv_compat("MACHGATE_TRACE_SIGNALS", "MACHISMO_TRACE_SIGNALS")) {
		uintptr_t pc = ucontext_pc(ucontext);
		uintptr_t lr = ucontext_reg(ucontext, 30);
		uintptr_t sp = ucontext_reg(ucontext, 31);
		uintptr_t fp = ucontext_reg(ucontext, 29);
		fprintf(stderr,
		        "machgate: SIGSEGV code=%d addr=%p pc=%p lr=%p sp=%p fp=%p\n",
		        info->si_code, (void*)fault_addr, (void*)pc,
		        (void*)lr, (void*)sp, (void*)fp);
		print_signal_init_context();
		if (lr >= 4)
			print_signal_indirect_branch(lr - 4, ucontext);
		print_signal_macho_context("signal.pc", pc, 0);
		print_signal_macho_context("signal.lr", lr, 0);
		if (lr >= 4)
			print_signal_macho_context("signal.lr-4", lr - 4, lr - 4);
		fprintf(stderr,
		        "machgate: regs x0=%p x1=%p x2=%p x3=%p x4=%p x5=%p x6=%p x7=%p x8=%p\n",
		        (void*)ucontext_reg(ucontext, 0),
		        (void*)ucontext_reg(ucontext, 1),
		        (void*)ucontext_reg(ucontext, 2),
		        (void*)ucontext_reg(ucontext, 3),
		        (void*)ucontext_reg(ucontext, 4),
		        (void*)ucontext_reg(ucontext, 5),
		        (void*)ucontext_reg(ucontext, 6),
		        (void*)ucontext_reg(ucontext, 7),
		        (void*)ucontext_reg(ucontext, 8));
		fprintf(stderr,
		        "machgate: regs x9=%p x10=%p x11=%p x12=%p x13=%p x14=%p x15=%p x16=%p x17=%p\n",
		        (void*)ucontext_reg(ucontext, 9),
		        (void*)ucontext_reg(ucontext, 10),
		        (void*)ucontext_reg(ucontext, 11),
		        (void*)ucontext_reg(ucontext, 12),
		        (void*)ucontext_reg(ucontext, 13),
		        (void*)ucontext_reg(ucontext, 14),
		        (void*)ucontext_reg(ucontext, 15),
		        (void*)ucontext_reg(ucontext, 16),
		        (void*)ucontext_reg(ucontext, 17));
		fprintf(stderr,
		        "machgate: regs x18=%p x19=%p x20=%p x21=%p x22=%p x23=%p x24=%p x25=%p x26=%p x27=%p x28=%p\n",
		        (void*)ucontext_reg(ucontext, 18),
		        (void*)ucontext_reg(ucontext, 19),
		        (void*)ucontext_reg(ucontext, 20),
		        (void*)ucontext_reg(ucontext, 21),
		        (void*)ucontext_reg(ucontext, 22),
		        (void*)ucontext_reg(ucontext, 23),
		        (void*)ucontext_reg(ucontext, 24),
		        (void*)ucontext_reg(ucontext, 25),
		        (void*)ucontext_reg(ucontext, 26),
		        (void*)ucontext_reg(ucontext, 27),
		        (void*)ucontext_reg(ucontext, 28));
	}

	/* Not our fault — re-raise with default handler */
	struct sigaction sa;
	sa.sa_handler = SIG_DFL;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGSEGV, &sa, NULL);
	raise(SIGSEGV);
}

void trampoline_install_signal_diagnostics(void* mh, uintptr_t slide)
{
	signal_diag_mh = mh;
	signal_diag_slide = slide;

	if (signal_diag_installed)
		return;

	struct sigaction sa;
	sa.sa_sigaction = stale_data_sigsegv;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_SIGINFO;
	sigaction(SIGSEGV, &sa, NULL);
	signal_diag_installed = 1;
}

void trampoline_guard_stale_data(void* mh, uintptr_t slide,
                                  const char** prefixes, int num_prefixes)
{
	struct mach_header_64* header = (struct mach_header_64*)mh;
	uint8_t* cmd_ptr = (uint8_t*)(header + 1);

	long page_size = sysconf(_SC_PAGESIZE);
	if (page_size <= 0) page_size = 4096;

	/* Find __DATA segment boundaries */
	uintptr_t data_base = 0, data_end = 0;
	for (uint32_t i = 0; i < header->ncmds; i++) {
		struct load_command* lc = (struct load_command*)cmd_ptr;
		if (lc->cmd == LC_SEGMENT_64) {
			struct segment_command_64* seg = (struct segment_command_64*)lc;
			if (strcmp(seg->segname, "__DATA") == 0) {
				data_base = seg->vmaddr + slide;
				data_end = data_base + seg->vmsize;
				break;
			}
		}
		cmd_ptr += lc->cmdsize;
	}
	if (!data_base) return;

	size_t num_pages = (data_end - data_base + page_size - 1) / page_size;
	if (num_pages == 0) return;

	/* Bitmap: has_library[i] = page i has a library symbol
	 *         has_other[i]   = page i has a non-library symbol */
	uint8_t* has_library = calloc(num_pages, 1);
	uint8_t* has_other = calloc(num_pages, 1);
	if (!has_library || !has_other) {
		free(has_library);
		free(has_other);
		return;
	}

	/* Find LC_SYMTAB */
	struct symtab_command* symtab = NULL;
	cmd_ptr = (uint8_t*)(header + 1);
	for (uint32_t i = 0; i < header->ncmds; i++) {
		struct load_command* lc = (struct load_command*)cmd_ptr;
		if (lc->cmd == LC_SYMTAB) {
			symtab = (struct symtab_command*)lc;
			break;
		}
		cmd_ptr += lc->cmdsize;
	}
	if (!symtab) { free(has_library); free(has_other); return; }

	struct nlist_64* nlist_arr = (struct nlist_64*)fileoff_to_mem(mh, slide, symtab->symoff);
	char* strtab = (char*)fileoff_to_mem(mh, slide, symtab->stroff);
	if (!nlist_arr || !strtab) { free(has_library); free(has_other); return; }

	int lib_data_syms = 0;

	/* Walk all symbols, categorize pages */
	for (uint32_t i = 0; i < symtab->nsyms; i++) {
		struct nlist_64* sym = &nlist_arr[i];
		if ((sym->n_type & N_TYPE) != N_SECT) continue;
		if (sym->n_type & N_STAB) continue;
		if (sym->n_strx >= symtab->strsize) continue;

		uintptr_t addr = sym->n_value + slide;
		if (addr < data_base || addr >= data_end) continue;

		size_t page_idx = (addr - data_base) / page_size;
		if (page_idx >= num_pages) continue;

		const char* name = strtab + sym->n_strx;
		if (matches_prefix(name, prefixes, num_prefixes)) {
			has_library[page_idx] = 1;
			lib_data_syms++;
			extern int machgate_verbose;
			if (machgate_verbose)
				fprintf(stderr, "trampoline: stale data symbol: %s at %p\n",
				        name, (void*)addr);
		} else {
			has_other[page_idx] = 1;
		}
	}

	if (lib_data_syms == 0) {
		free(has_library);
		free(has_other);
		return;
	}

	/* Guard pages that have ONLY library symbols */
	int guarded = 0;
	int mixed = 0;
	for (size_t i = 0; i < num_pages; i++) {
		if (!has_library[i]) continue;
		if (has_other[i]) {
			mixed++;
			continue;
		}
		if (num_guarded_pages >= MAX_GUARDED_PAGES) break;

		uintptr_t page_addr = data_base + i * page_size;
		if (mprotect((void*)page_addr, page_size, PROT_NONE) == 0) {
			guarded_pages[num_guarded_pages].base = page_addr;
			guarded_pages[num_guarded_pages].size = page_size;
			num_guarded_pages++;
			guarded++;
		}
	}

	free(has_library);
	free(has_other);

	/* Install SIGSEGV handler */
	if (guarded > 0) {
		trampoline_install_signal_diagnostics(mh, slide);
	}

	fprintf(stderr, "trampoline: __DATA guard: %d library data symbols, "
	        "%d pages guarded, %d mixed pages (cannot guard)\n",
	        lib_data_syms, guarded, mixed);
}
