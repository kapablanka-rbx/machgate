#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/*
 * Mach-O Chained Fixup Resolver for aarch64 Linux.
 *
 * Parses LC_DYLD_CHAINED_FIXUPS from a loaded Mach-O binary and resolves
 * symbol bindings by mapping dylib names to native Linux .so files.
 *
 * Supports:
 *   - DYLD_CHAINED_PTR_64_OFFSET (format 6) — used by modern arm64 binaries
 *   - DYLD_CHAINED_PTR_64 (format 2) — older 64-bit format
 *   - DYLD_CHAINED_IMPORT (format 1) — standard import table
 *
 * Reference: Apple's fixup-chains.h, dyld MachOLoaded.cpp
 */

#include "resolver.h"
#include "dylib_loader.h"
#include "lua_entity_opt.h"
#include "macho_defs.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <link.h>        /* struct link_map, ELF types, DT_* */
#include <string.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>

/* macOS malloc returns zero-initialized pages for most allocations.
 * Hook _Znwm/_Znam (operator new/new[]) in the Mach-O GOT so only
 * guest allocations are zeroed, avoiding the reentrancy issues of
 * globally interposing malloc. */
static void *(*shim_malloc_fn)(size_t) = NULL;
static void *(*shim_memalign_fn)(size_t, size_t) = NULL;
static void (*shim_free_fn)(void *) = NULL;

static void *zeroing_operator_new(size_t size)
{
	/* Call the shim's malloc (which does heap tracing) if loaded,
	 * otherwise fall back to glibc malloc. */
	void *p;
	if (shim_malloc_fn)
		p = shim_malloc_fn(size);
	else {
		p = malloc(size);
		if (p) memset(p, 0, size);
	}
	return p;
}

static void *zeroing_operator_new_aligned(size_t size, size_t alignment)
{
	void *p = NULL;

	if (alignment < sizeof(void *))
		alignment = sizeof(void *);
	if (shim_memalign_fn)
		p = shim_memalign_fn(alignment, size);
	else if (posix_memalign(&p, alignment, size) == 0 && p)
		memset(p, 0, size);
	return p;
}

static void zeroing_operator_delete(void *ptr)
{
	if (shim_free_fn)
		shim_free_fn(ptr);
	else
		free(ptr);
}

static const char *normalize_cxx_operator_symbol(const char *sym_name)
{
	if (sym_name && sym_name[0] == '_' && sym_name[1] == '_')
		return sym_name + 1;
	return sym_name;
}

static int is_operator_new_symbol(const char *sym_name)
{
	const char *name = normalize_cxx_operator_symbol(sym_name);

	return strcmp(name, "_Znwm") == 0 ||
	       strcmp(name, "_Znam") == 0 ||
	       strcmp(name, "_ZnwmRKSt9nothrow_t") == 0 ||
	       strcmp(name, "_ZnamRKSt9nothrow_t") == 0;
}

static int is_operator_new_aligned_symbol(const char *sym_name)
{
	const char *name = normalize_cxx_operator_symbol(sym_name);

	return strcmp(name, "_ZnwmSt11align_val_t") == 0 ||
	       strcmp(name, "_ZnamSt11align_val_t") == 0 ||
	       strcmp(name, "_ZnwmSt11align_val_tRKSt9nothrow_t") == 0 ||
	       strcmp(name, "_ZnamSt11align_val_tRKSt9nothrow_t") == 0;
}

static int is_operator_delete_symbol(const char *sym_name)
{
	const char *name = normalize_cxx_operator_symbol(sym_name);

	return strcmp(name, "_ZdlPv") == 0 ||
	       strcmp(name, "_ZdaPv") == 0 ||
	       strcmp(name, "_ZdlPvm") == 0 ||
	       strcmp(name, "_ZdaPvm") == 0 ||
	       strcmp(name, "_ZdlPvRKSt9nothrow_t") == 0 ||
	       strcmp(name, "_ZdaPvRKSt9nothrow_t") == 0 ||
	       strcmp(name, "_ZdlPvSt11align_val_t") == 0 ||
	       strcmp(name, "_ZdaPvSt11align_val_t") == 0 ||
	       strcmp(name, "_ZdlPvmSt11align_val_t") == 0 ||
	       strcmp(name, "_ZdaPvmSt11align_val_t") == 0 ||
	       strcmp(name, "_ZdlPvSt11align_val_tRKSt9nothrow_t") == 0 ||
	       strcmp(name, "_ZdaPvSt11align_val_tRKSt9nothrow_t") == 0;
}

static int is_cxx_operator_symbol(const char *sym_name)
{
	return is_operator_new_symbol(sym_name) ||
	       is_operator_new_aligned_symbol(sym_name) ||
	       is_operator_delete_symbol(sym_name);
}

static uintptr_t resolve_cxx_operator_hook(const char *sym_name)
{
	if (is_operator_new_symbol(sym_name))
		return (uintptr_t)zeroing_operator_new;
	if (is_operator_new_aligned_symbol(sym_name))
		return (uintptr_t)zeroing_operator_new_aligned;
	if (is_operator_delete_symbol(sym_name))
		return (uintptr_t)zeroing_operator_delete;
	return 0;
}

/* ---- LuaJIT profiler hooks ----
 *
 * When MACHGATE_LUA_PROFILE is set, hook luaopen_jit/lua_close to inject
 * LuaJIT's built-in profiler (jit.p).  Also supports MACHGATE_LUA_JITV for
 * JIT verbose logging (shows which traces compile/abort).
 *
 * We hook luaopen_jit rather than luaL_openlibs because the game (via sol2)
 * opens individual Lua libraries instead of calling luaL_openlibs.
 *
 * No LuaJIT headers needed — all calls go through dlsym'd function pointers.
 */
static int (*real_luaopen_jit)(void *L) = NULL;
static void (*real_lua_close)(void *L) = NULL;

static const char* getenv_compat(const char* name, const char* legacy_name)
{
	const char* value = getenv(name);
	if (value)
		return value;
	return getenv(legacy_name);
}

/* Execute a Lua string via luaL_loadstring + lua_pcall (luaL_dostring is a macro).
 * Returns 0 on success, non-zero on error (logs error to stderr). */
static int lua_dostring(void *L, const char *s)
{
	static int (*ls)(void *, const char *) = NULL;
	static int (*pc)(void *, int, int, int) = NULL;
	static const char *(*tolstring)(void *, int, size_t *) = NULL;
	static void (*settop)(void *, int) = NULL;

	if (!ls) ls = dlsym(RTLD_DEFAULT, "luaL_loadstring");
	if (!pc) pc = dlsym(RTLD_DEFAULT, "lua_pcall");
	if (!tolstring) tolstring = dlsym(RTLD_DEFAULT, "lua_tolstring");
	if (!settop) settop = dlsym(RTLD_DEFAULT, "lua_settop");
	if (!ls || !pc) return -1;

	int err = ls(L, s);
	if (err == 0)
		err = pc(L, 0, 0, 0);

	if (err != 0) {
		const char *msg = tolstring ? tolstring(L, -1, NULL) : NULL;
		fprintf(stderr, "resolver: lua_dostring failed: %s\n",
				msg ? msg : "(unknown error)");
		if (settop) settop(L, -2);
		return err;
	}
	return 0;
}

static void *profiled_lua_state = NULL;

/* Hook luaopen_jit instead of luaL_openlibs: the game (via sol2) opens
 * individual Lua libraries rather than calling luaL_openlibs.  luaopen_jit
 * is called last in the standard order, after luaopen_package, so require()
 * is available for loading jit.p. */
static int wrapped_luaopen_jit(void *L)
{
	if (!real_luaopen_jit)
		real_luaopen_jit = dlsym(RTLD_DEFAULT, "luaopen_jit");
	int result = real_luaopen_jit(L);

	/* Only start profiler on the first Lua state (the game's main state) */
	if (profiled_lua_state)
		return result;

	const char *profile_path = getenv_compat("MACHGATE_LUA_PROFILE",
	                                         "MACHISMO_LUA_PROFILE");
	if (profile_path) {
		char lua_cmd[512];
		snprintf(lua_cmd, sizeof(lua_cmd),
			"require('jit.p').start('vFl', '%s')", profile_path);
		if (lua_dostring(L, lua_cmd) == 0) {
			profiled_lua_state = L;
			fprintf(stderr, "resolver: LuaJIT profiler started -> %s\n", profile_path);
		}
	}

	if (getenv_compat("MACHGATE_LUA_JITV", "MACHISMO_LUA_JITV")) {
		if (lua_dostring(L, "require('jit.v').start()") == 0)
			fprintf(stderr, "resolver: LuaJIT verbose JIT logging enabled\n");
	}


	/* Run an arbitrary Lua script file for debugging/introspection */
	const char *inject_path = getenv_compat("MACHGATE_LUA_INJECT",
	                                        "MACHISMO_LUA_INJECT");
	if (inject_path) {
		int (*loadfile)(void *, const char *) = dlsym(RTLD_DEFAULT, "luaL_loadfile");
		int (*pcall)(void *, int, int, int) = dlsym(RTLD_DEFAULT, "lua_pcall");
		if (loadfile && pcall) {
			if (loadfile(L, inject_path) == 0) {
				pcall(L, 0, 0, 0);
				fprintf(stderr, "resolver: injected Lua script %s\n", inject_path);
			} else {
				fprintf(stderr, "resolver: failed to load %s\n", inject_path);
			}
		}
	}

	return result;
}

static void wrapped_lua_close(void *L)
{
	if (L == profiled_lua_state) {
		lua_dostring(L, "require('jit.p').stop()");
		fprintf(stderr, "resolver: LuaJIT profiler stopped, output at %s\n",
				getenv_compat("MACHGATE_LUA_PROFILE",
				              "MACHISMO_LUA_PROFILE"));
		profiled_lua_state = NULL;
	}

	if (!real_lua_close)
		real_lua_close = dlsym(RTLD_DEFAULT, "lua_close");
	real_lua_close(L);
}

/* ---- Entity metamethod optimization hook ----
 *
 * Hook lua_pcall to attempt entity opt installation after the entity
 * system is loaded.  Once installed, becomes a single not-taken branch. */
static int (*real_lua_pcall)(void *L, int nargs, int nres, int errfunc) = NULL;
static int entity_opt_installed = 0;

static int wrapped_lua_pcall(void *L, int nargs, int nres, int errfunc)
{
	if (!real_lua_pcall)
		real_lua_pcall = dlsym(RTLD_DEFAULT, "lua_pcall");
	int result = real_lua_pcall(L, nargs, nres, errfunc);
	if (__builtin_expect(!entity_opt_installed, 0))
		entity_opt_installed = try_install_entity_opt(L);
	return result;
}

/*
 * Try macOS ↔ Linux C++ mangling variants for the same function.
 * See trampoline.c for detailed rationale — combinatorial y↔m substitution.
 */
static void* try_mangling_variants(void* lib, const char* name)
{
	size_t len = strlen(name);

	if (strcmp(name, "_ZNSt3__120__libcpp_atomic_waitEPVKvx") == 0) {
		void* result = dlsym(lib, "_ZNSt3__120__libcpp_atomic_waitEPVKvi");
		if (result) {
			fprintf(stderr, "resolver: mangling fallback: %s -> %s\n",
			        name, "_ZNSt3__120__libcpp_atomic_waitEPVKvi");
			return result;
		}
	}

	int y_pos[16];
	int ny = 0;
	for (size_t i = 0; i < len && ny < 16; i++) {
		if (name[i] == 'y') y_pos[ny++] = i;
	}

	char* buf = malloc(len + 1);
	if (!buf) return NULL;

	/* Try all non-empty subsets of y→m */
	for (int mask = 1; mask < (1 << ny); mask++) {
		memcpy(buf, name, len + 1);
		for (int j = 0; j < ny; j++) {
			if (mask & (1 << j))
				buf[y_pos[j]] = 'm';
		}
		void* addr = dlsym(lib, buf);
		if (addr) {
			fprintf(stderr, "resolver: mangling fallback: %s -> %s\n", name, buf);
			free(buf);
			return addr;
		}
	}

	/* Reverse: m→y */
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
			fprintf(stderr, "resolver: mangling fallback: %s -> %s\n", name, buf);
			free(buf);
			return addr;
		}
	}

	free(buf);
	return NULL;
}

/* ---- Chained fixup structures (from Apple's fixup-chains.h) ---- */

#define LC_DYLD_CHAINED_FIXUPS  (0x34 | LC_REQ_DYLD)
#define LC_DYLD_EXPORTS_TRIE    (0x33 | LC_REQ_DYLD)

struct linkedit_data_command {
	uint32_t cmd;
	uint32_t cmdsize;
	uint32_t dataoff;
	uint32_t datasize;
};

struct dyld_chained_fixups_header {
	uint32_t fixups_version;
	uint32_t starts_offset;
	uint32_t imports_offset;
	uint32_t symbols_offset;
	uint32_t imports_count;
	uint32_t imports_format;
	uint32_t symbols_format;
};

struct dyld_chained_starts_in_image {
	uint32_t seg_count;
	uint32_t seg_info_offset[];
};

struct dyld_chained_starts_in_segment {
	uint32_t size;
	uint16_t page_size;
	uint16_t pointer_format;
	uint64_t segment_offset;
	uint32_t max_valid_pointer;
	uint16_t page_count;
	uint16_t page_start[];
};

/* Pointer formats */
#define DYLD_CHAINED_PTR_64           2
#define DYLD_CHAINED_PTR_64_OFFSET    6

/* Import formats */
#define DYLD_CHAINED_IMPORT           1
#define DYLD_CHAINED_IMPORT_ADDEND    2
#define DYLD_CHAINED_IMPORT_ADDEND64  3

/* Page start sentinel */
#define DYLD_CHAINED_PTR_START_NONE   0xFFFF

/* Chained pointer bitfield access for DYLD_CHAINED_PTR_64 and PTR_64_OFFSET */
#define FIXUP_PTR_BIND(raw)     (((raw) >> 63) & 1)
#define FIXUP_PTR_NEXT(raw)     (((raw) >> 51) & 0xFFF)
#define FIXUP_BIND_ORDINAL(raw) ((raw) & 0xFFFFFF)
#define FIXUP_BIND_ADDEND(raw)  (((raw) >> 24) & 0xFF)
#define FIXUP_REBASE_TARGET(raw)  ((raw) & 0xFFFFFFFFF)     /* 36 bits */
#define FIXUP_REBASE_HIGH8(raw)   (((raw) >> 36) & 0xFF)

struct dyld_chained_import {
	uint32_t lib_ordinal  :  8;
	uint32_t weak_import  :  1;
	uint32_t name_offset  : 23;
};

struct dyld_chained_import_addend {
	uint32_t lib_ordinal  :  8;
	uint32_t weak_import  :  1;
	uint32_t name_offset  : 23;
	int32_t  addend;
};

struct dyld_chained_import_addend64 {
	uint64_t lib_ordinal  : 16;
	uint64_t weak_import  :  1;
	uint64_t reserved     : 15;
	uint64_t name_offset  : 32;
	uint64_t addend;
};

/* ---- Dylib mapping ---- */

#define MAX_DYLIBS 64
#define MAX_MAPPINGS 128
#define MAX_NAME 256

/* Actions for unmapped dylibs */
enum dylib_action {
	DYLIB_MAP,      /* map to a Linux .so */
	DYLIB_STUB,     /* return stub (NULL/0) for all symbols */
	DYLIB_SKIP,     /* silently skip — symbols remain unresolved */
	DYLIB_MACHO,    /* look up in a loaded Mach-O dylib */
	DYLIB_DEFERRED, /* library path recorded, dlopen deferred until GL context */
};

struct dylib_entry {
	int ordinal;                     /* 1-based ordinal from LC_LOAD_DYLIB order */
	char name[MAX_NAME];             /* dylib install name (basename) */
	enum dylib_action action;
	char so_path[MAX_NAME];          /* Linux .so path (for DYLIB_MAP) or Mach-O path */
	void* handle;                    /* dlopen handle (DYLIB_MAP) */
	struct macho_dylib_info* macho_info; /* loaded Mach-O dylib (DYLIB_MACHO) */
};

struct dylib_mapping {
	char dylib_pattern[MAX_NAME];
	char so_path[MAX_NAME];          /* "STUB", "SKIP", or a .so path */
};

/* ---- Segment info for converting file offsets to memory ---- */

struct seg_info {
	uint64_t vmaddr;
	uint64_t vmsize;
	uint64_t fileoff;
	uint64_t filesize;
	int prot;
};

/* ---- Resolver state ---- */

struct resolver_state {
	struct mach_header_64* mh;
	uintptr_t slide;
	uintptr_t mh_addr;

	/* Load commands data */
	struct dylib_entry dylibs[MAX_DYLIBS];
	int ndylibs;

	/* Segment info */
	struct seg_info linkedit;
	struct seg_info segments[16];
	int nsegments;

	/* Chained fixups from LC_DYLD_CHAINED_FIXUPS */
	uint32_t fixups_dataoff;
	uint32_t fixups_datasize;
	bool has_chained_fixups;

	/* LC_DYLD_INFO / LC_DYLD_INFO_ONLY */
	bool has_dyld_info;
	uint32_t dyld_info_rebase_off;
	uint32_t dyld_info_rebase_size;
	uint32_t dyld_info_bind_off;
	uint32_t dyld_info_bind_size;
	uint32_t dyld_info_weak_bind_off;
	uint32_t dyld_info_weak_bind_size;
	uint32_t dyld_info_lazy_bind_off;
	uint32_t dyld_info_lazy_bind_size;

	/* Dylib mapping config */
	struct dylib_mapping mappings[MAX_MAPPINGS];
	int nmappings;

	/* Stats */
	int binds_resolved;
	int binds_stubbed;
	int binds_failed;
	int rebases_applied;
};

#define BIND_SLOT_REGISTRY_MAX 32768

static struct resolver_bind_slot_info bind_slot_registry[BIND_SLOT_REGISTRY_MAX];
static int bind_slot_registry_count;

static void register_bind_slot(const char* context,
                               const char* sym_name,
                               const char* lookup_name,
                               const struct dylib_entry* de,
                               uintptr_t slot_addr,
                               uintptr_t resolved,
                               const char* source_kind,
                               const char* source_path)
{
	if (!slot_addr)
		return;

	struct resolver_bind_slot_info* info = NULL;
	for (int i = bind_slot_registry_count - 1; i >= 0; i--) {
		if (bind_slot_registry[i].slot_addr == slot_addr) {
			info = &bind_slot_registry[i];
			break;
		}
	}

	if (!info) {
		if (bind_slot_registry_count >= BIND_SLOT_REGISTRY_MAX)
			return;
		info = &bind_slot_registry[bind_slot_registry_count++];
	}

	info->slot_addr = slot_addr;
	info->resolved = resolved;
	info->context = context;
	info->sym_name = sym_name;
	info->lookup_name = lookup_name;
	info->dylib_name = de ? de->name : NULL;
	info->source_kind = source_kind;
	info->source_path = source_path;
}

int resolver_lookup_bind_slot(uintptr_t slot_addr,
                              struct resolver_bind_slot_info* out_info)
{
	if (!slot_addr || !out_info)
		return -1;

	for (int i = bind_slot_registry_count - 1; i >= 0; i--) {
		if (bind_slot_registry[i].slot_addr == slot_addr) {
			*out_info = bind_slot_registry[i];
			return 0;
		}
	}
	return -1;
}

static const char* dylib_action_name(enum dylib_action action)
{
	switch (action) {
	case DYLIB_MAP: return "map";
	case DYLIB_STUB: return "stub";
	case DYLIB_SKIP: return "skip";
	case DYLIB_MACHO: return "macho";
	case DYLIB_DEFERRED: return "deferred";
	}
	return "unknown";
}

static void log_failed_bind(struct resolver_state* rs, const char* context,
                            const char* sym_name, const char* lookup_name,
                            int lib_ordinal, int weak,
                            const struct dylib_entry* de)
{
	if (weak || rs->binds_failed >= 20)
		return;

	if (de) {
		machgate_log_startup(
		    "resolver: failed bind (%s): symbol '%s' lookup '%s' dylib[%d] '%s' action=%s target='%s'\n",
		    context, sym_name, lookup_name, lib_ordinal, de->name,
		    dylib_action_name(de->action), de->so_path);
		return;
	}

	machgate_log_startup(
	    "resolver: failed bind (%s): symbol '%s' lookup '%s' lib_ordinal=%d\n",
	    context, sym_name, lookup_name, lib_ordinal);
}

static int trace_bindings_enabled(void)
{
	const char* trace_bindings = getenv_compat("MACHGATE_TRACE_BINDINGS",
	                                           "MACHISMO_TRACE_BINDINGS");
	const char* trace_shim = getenv_compat("MACHGATE_TRACE_SHIM",
	                                       "MACHISMO_TRACE_SHIM");
	return (trace_bindings && trace_bindings[0] && strcmp(trace_bindings, "0") != 0) ||
	       (trace_shim && strcmp(trace_shim, "1") == 0);
}

static int trace_binding_symbol(const char* lookup_name)
{
	const char* trace_bindings = getenv_compat("MACHGATE_TRACE_BINDINGS",
	                                           "MACHISMO_TRACE_BINDINGS");
	if (trace_bindings && strcmp(trace_bindings, "all") == 0)
		return 1;

	return strcmp(lookup_name, "mmap") == 0 ||
	       strcmp(lookup_name, "munmap") == 0 ||
	       strcmp(lookup_name, "readlink") == 0 ||
	       strcmp(lookup_name, "readlinkat") == 0 ||
	       strcmp(lookup_name, "sigaltstack") == 0 ||
	       strcmp(lookup_name, "pthread_create") == 0 ||
	       strcmp(lookup_name, "pthread_detach") == 0 ||
	       strcmp(lookup_name, "pthread_setname_np") == 0;
}

static void trace_target_binding(const char* context,
                                 const char* sym_name,
                                 const char* lookup_name,
                                 int lib_ordinal,
                                 const struct dylib_entry* de,
                                 uintptr_t slot_addr,
                                 uintptr_t resolved,
                                 const char* source_kind,
                                 const char* source_path)
{
	register_bind_slot(context, sym_name, lookup_name, de, slot_addr, resolved,
	                   source_kind, source_path);

	if (!trace_bindings_enabled() || !trace_binding_symbol(lookup_name))
		return;

	fprintf(stderr,
	        "resolver: trace binding context=%s symbol='%s' lookup='%s' ordinal=%d dylib='%s' slot=%p resolved=%p source='%s' path='%s'\n",
	        context, sym_name, lookup_name, lib_ordinal,
	        de ? de->name : "(none)", (void*)slot_addr, (void*)resolved,
	        source_kind ? source_kind : "(unknown)",
	        source_path ? source_path : "(unknown)");
}

static uintptr_t resolve_non_gui_framework_data(const struct dylib_entry* de,
                                                const char* sym_name)
{
	static const void* ns_pasteboard_type_string = NULL;

	if (!de)
		return 0;

	if (strcmp(de->name, "AppKit") == 0 &&
	    strcmp(sym_name, "_NSPasteboardTypeString") == 0)
		return (uintptr_t)&ns_pasteboard_type_string;

	return 0;
}

static int is_darwin_runtime_symbol(const char* lookup_name)
{
	if (strncmp(lookup_name, "pthread_", 8) == 0)
		return 1;
	return strcmp(lookup_name, "mmap") == 0 ||
	       strcmp(lookup_name, "munmap") == 0 ||
	       strcmp(lookup_name, "mprotect") == 0 ||
	       strcmp(lookup_name, "sigaction") == 0 ||
	       strcmp(lookup_name, "sigaltstack") == 0;
}

static int is_darwin_allocator_symbol(const char* lookup_name)
{
	return strcmp(lookup_name, "malloc") == 0 ||
	       strcmp(lookup_name, "free") == 0 ||
	       strcmp(lookup_name, "calloc") == 0 ||
	       strcmp(lookup_name, "realloc") == 0 ||
	       strcmp(lookup_name, "posix_memalign") == 0 ||
	       strcmp(lookup_name, "memalign") == 0 ||
	       strcmp(lookup_name, "aligned_alloc") == 0 ||
	       strcmp(lookup_name, "valloc") == 0 ||
	       strcmp(lookup_name, "malloc_size") == 0 ||
	       strcmp(lookup_name, "malloc_good_size") == 0 ||
	       strcmp(lookup_name, "malloc_default_zone") == 0 ||
	       strcmp(lookup_name, "malloc_create_zone") == 0 ||
	       strcmp(lookup_name, "malloc_destroy_zone") == 0 ||
	       strcmp(lookup_name, "malloc_set_zone_name") == 0 ||
	       strcmp(lookup_name, "malloc_get_zone_name") == 0 ||
	       strcmp(lookup_name, "malloc_zone_from_ptr") == 0 ||
	       strcmp(lookup_name, "malloc_get_all_zones") == 0 ||
	       strcmp(lookup_name, "malloc_num_zones") == 0 ||
	       strcmp(lookup_name, "malloc_zones") == 0 ||
	       strcmp(lookup_name, "malloc_zone_malloc") == 0 ||
	       strcmp(lookup_name, "malloc_zone_calloc") == 0 ||
	       strcmp(lookup_name, "malloc_zone_valloc") == 0 ||
	       strcmp(lookup_name, "malloc_zone_realloc") == 0 ||
	       strcmp(lookup_name, "malloc_zone_free") == 0 ||
	       strcmp(lookup_name, "malloc_zone_destroy") == 0 ||
	       strcmp(lookup_name, "malloc_zone_size") == 0 ||
	       strcmp(lookup_name, "malloc_zone_batch_malloc") == 0 ||
	       strcmp(lookup_name, "malloc_zone_batch_free") == 0 ||
	       strcmp(lookup_name, "malloc_zone_memalign") == 0 ||
	       strcmp(lookup_name, "malloc_zone_free_definite_size") == 0 ||
	       strcmp(lookup_name, "malloc_zone_pressure_relief") == 0 ||
	       strcmp(lookup_name, "malloc_zone_claimed_address") == 0 ||
	       strcmp(lookup_name, "malloc_zone_malloc_with_options") == 0 ||
	       strcmp(lookup_name, "malloc_zone_register") == 0 ||
	       strcmp(lookup_name, "malloc_zone_unregister") == 0;
}

static void* lookup_libsystem_symbol(struct resolver_state* rs,
                                     const char* lookup_name,
                                     const struct dylib_entry** provider)
{
	for (int i = 0; i < rs->ndylibs && i < MAX_DYLIBS; i++) {
		struct dylib_entry* de = &rs->dylibs[i];
		if (de->action != DYLIB_MAP || !de->handle)
			continue;
		if (!strstr(de->name, "libSystem"))
			continue;
		void* addr = dlsym(de->handle, lookup_name);
		if (addr) {
			if (provider)
				*provider = de;
			return addr;
		}
	}
	return NULL;
}

static void* lookup_flat_symbol(struct resolver_state* rs,
                                const char* lookup_name,
                                const char** source_kind,
                                const char** source_path)
{
	if (is_darwin_runtime_symbol(lookup_name) || is_darwin_allocator_symbol(lookup_name)) {
		const struct dylib_entry* provider = NULL;
		void* addr = lookup_libsystem_symbol(rs, lookup_name, &provider);
		if (addr) {
			if (source_kind)
				*source_kind = "mapped dylib handle";
			if (source_path)
				*source_path = provider ? provider->so_path : "(unknown)";
			return addr;
		}
	}
	void* addr = dlsym(RTLD_DEFAULT, lookup_name);
	if (addr) {
		if (source_kind)
			*source_kind = "RTLD_DEFAULT";
		if (source_path)
			*source_path = "RTLD_DEFAULT";
	}
	return addr;
}

/* ---- Deferred library loading ----
 *
 * Libraries with a DEFERRED: prefix in dylib_map.conf are not dlopen'd
 * during initial resolution. Instead, their bind slots get return-0 stubs
 * and are recorded here. When the game calls SDL_GL_CreateContext (hooked
 * during resolution), we dlopen the deferred library and re-resolve all
 * recorded binds with real symbols.
 *
 * Primary use: gl4es on KMSDRM — its constructor needs an EGL display that
 * only exists after SDL creates a window. */

#define MAX_DEFERRED_BINDS 512

struct deferred_bind {
	uintptr_t got_slot;       /* GOT entry address to patch */
	const char* sym_name;     /* pointer into __LINKEDIT — stable for process life */
	int lib_ordinal;          /* which deferred library (1-based) */
	int weak;
	int64_t addend;
};

static struct {
	struct deferred_bind binds[MAX_DEFERRED_BINDS];
	int count;
	bool active;   /* set in open_dylibs when DEFERRED: library found */
	bool resolved;
	struct mach_header_64* mh;
	uintptr_t slide;
	uintptr_t mh_addr;
	struct dylib_entry dylibs[MAX_DYLIBS]; /* copy of dylib entries for completion */
	int ndylibs;
} g_deferred;

static void* (*g_real_SDL_GL_CreateContext)(void*) = NULL;
static int (*g_real_SDL_GL_SetAttribute)(int, int) = NULL;

/* Forward declarations for deferred loading */
static uintptr_t alloc_return0_stub(void);
static void resolver_complete_deferred(void);

/* SDL_video.h constants for GL attribute overrides.
 * SDL3 removed SDL_GL_CONTEXT_EGL (was enum 19), shifting PROFILE_MASK
 * from 21 (SDL2) to 20 (SDL3).  Handle both so gl4es works with either. */
#define MACHGATE_SDL_GL_CONTEXT_MAJOR_VERSION      17
#define MACHGATE_SDL_GL_CONTEXT_MINOR_VERSION      18
#define MACHGATE_SDL_GL_CONTEXT_PROFILE_MASK_SDL2   21
#define MACHGATE_SDL_GL_CONTEXT_PROFILE_MASK_SDL3   20
#define MACHGATE_SDL_GL_CONTEXT_PROFILE_ES    0x0004

/* Force GLES 2.0 profile when using gl4es on GLES-only hardware. */
static int wrapped_SDL_GL_SetAttribute(int attr, int value)
{
	switch (attr) {
	case MACHGATE_SDL_GL_CONTEXT_PROFILE_MASK_SDL2:
	case MACHGATE_SDL_GL_CONTEXT_PROFILE_MASK_SDL3:
		value = MACHGATE_SDL_GL_CONTEXT_PROFILE_ES;
		break;
	case MACHGATE_SDL_GL_CONTEXT_MAJOR_VERSION:
		value = 2;
		break;
	case MACHGATE_SDL_GL_CONTEXT_MINOR_VERSION:
		value = 0;
		break;
	}
	return g_real_SDL_GL_SetAttribute(attr, value);
}

static void* wrapped_SDL_GL_CreateContext(void* window)
{
	void* ctx = g_real_SDL_GL_CreateContext(window);
	if (ctx)
		resolver_complete_deferred();
	return ctx;
}

/* ---- C++ constructor/destructor ABI adapters ----
 *
 * Apple's ARM64 ABI: C1/C2 constructors and D1/D2 destructors return `this`
 * in x0. Standard Itanium ABI on Linux: they return void (x0 is clobbered).
 *
 * When a Mach-O binary calls a constructor in a native Linux .so, the caller
 * expects x0 = this afterward. We wrap such calls with a small trampoline:
 *
 *   stp x30, x0, [sp, #-16]!  // save LR + this
 *   ldr x16, [pc, #20]        // load real function address
 *   blr x16                   // call real ctor/dtor
 *   ldp x30, x0, [sp], #16   // restore LR + this
 *   ret
 *   nop
 *   .quad <real_addr>
 *
 * IMPORTANT: This adapter shifts sp by -16 before calling. This is safe for
 * constructors where ALL parameters fit in registers (≤8 float/SIMD params
 * in s0-s7, ≤7 integer params in x1-x7 since x0=this). But constructors
 * with stack-passed parameters (e.g., sf::Transform(9 floats)) would have
 * those parameters corrupted because [sp] now points to saved x30 instead
 * of the 9th parameter.
 *
 * For constructors with potential stack-passed parameters, we skip the
 * adapter entirely. In practice, Linux constructors usually preserve x0
 * (the this pointer is used throughout and not explicitly clobbered), and
 * most callers save the pointer before calling the constructor anyway.
 */


/* Pool of executable trampoline memory */
static uint8_t* ctor_tramp_pool = NULL;
static size_t ctor_tramp_used = 0;
static size_t ctor_tramp_capacity = 0;
static int ctor_tramp_count = 0;

struct ctor_abi_trampoline {
	uint32_t stp_lr_x0;     /* stp x30, x0, [sp, #-16]! — save LR + this */
	uint32_t ldr_x16;       /* ldr x16, [pc, #20]       — load target */
	uint32_t blr_x16;       /* blr x16                  — call (clobbers x0,x16,x17,x30) */
	uint32_t ldp_lr_x0;     /* ldp x30, x0, [sp], #16  — restore LR + this */
	uint32_t ret;            /* ret                      — return to caller */
	uint32_t pad;            /* nop (alignment for .quad) */
	uint64_t target_addr;   /* real function address */
};

/* Check if a mangled symbol name is a C++ constructor or destructor */
static int is_ctor_or_dtor(const char* name)
{
	if (!name) return 0;
	/* Itanium mangling: _ZN...C1E, _ZN...C2E (ctors), _ZN...D1E, _ZN...D2E (dtors)
	 * Also C1Ev, C2Ev, D1Ev, D2Ev for void param ctors/dtors */
	const char* p = name;
	/* Skip leading underscore (Mach-O convention) */
	if (*p == '_') p++;
	/* Must start with _ZN or _ZNK */
	if (p[0] != '_' || p[1] != 'Z') return 0;

	/* Search for C1E, C2E, D0E, D1E, D2E patterns */
	for (; *p; p++) {
		if ((p[0] == 'C' || p[0] == 'D') &&
		    (p[1] == '0' || p[1] == '1' || p[1] == '2') &&
		    p[2] == 'E') {
			return 1;
		}
	}
	return 0;
}

/* Check if a ctor/dtor has parameters that would be passed on the stack.
 * ARM64 passes floats in s0-s7 (8 regs) and ints in x1-x7 (7 regs, x0=this).
 * If more params than registers, the extras go on the stack and the ctor
 * adapter's sp shift would corrupt them.
 *
 * We detect this from the Itanium-mangled parameter types after the 'E'.
 * Single-letter type codes: f=float, d=double go in float regs;
 * i,j,l,m,x,y,c,s,b,a,h,t,w go in integer regs.
 * Complex types (pointers, references, classes) also use integer regs.
 */
static int ctor_has_stack_params(const char* name)
{
	if (!name) return 0;
	const char* p = name;
	if (*p == '_') p++;
	if (p[0] != '_' || p[1] != 'Z') return 0;

	/* Find the parameter list after C[012]E or D[012]E */
	const char* params = NULL;
	for (; *p; p++) {
		if ((p[0] == 'C' || p[0] == 'D') &&
		    (p[1] == '0' || p[1] == '1' || p[1] == '2') &&
		    p[2] == 'E') {
			params = p + 3;
			break;
		}
	}
	if (!params || !*params || *params == 'v') return 0; /* no params or void */

	int float_count = 0;
	int int_count = 0;
	for (p = params; *p; p++) {
		switch (*p) {
		case 'f': case 'd': /* float, double → float regs */
			float_count++;
			break;
		case 'i': case 'j': case 'l': case 'm': /* int types → int regs */
		case 'x': case 'y': case 'c': case 's':
		case 'b': case 'a': case 'h': case 't': case 'w':
			int_count++;
			break;
		default:
			/* Complex type (pointer, reference, class, etc.) — stop
			 * counting simple types. Complex types use int regs but
			 * we can't easily count them, so be conservative and
			 * assume no overflow. */
			return (float_count > 8 || int_count > 7);
		}
	}
	return (float_count > 8 || int_count > 7);
}

/* Create an ABI adapter trampoline that saves/restores x0 around the call */
static uintptr_t wrap_ctor_for_apple_abi(uintptr_t real_addr)
{
	/* Allocate trampoline pool on first use */
	if (!ctor_tramp_pool) {
		long page_size = sysconf(_SC_PAGESIZE);
		ctor_tramp_capacity = page_size * 4; /* 4 pages, room for ~500 trampolines */
		ctor_tramp_pool = mmap(NULL, ctor_tramp_capacity,
		                       PROT_READ | PROT_WRITE | PROT_EXEC,
		                       MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
		if (ctor_tramp_pool == MAP_FAILED) {
			fprintf(stderr, "resolver: failed to allocate ctor trampoline pool\n");
			ctor_tramp_pool = NULL;
			return real_addr; /* fall back to unwrapped */
		}
	}

	if (ctor_tramp_used + sizeof(struct ctor_abi_trampoline) > ctor_tramp_capacity) {
		fprintf(stderr, "resolver: ctor trampoline pool exhausted\n");
		return real_addr;
	}

	struct ctor_abi_trampoline* t =
		(struct ctor_abi_trampoline*)(ctor_tramp_pool + ctor_tramp_used);
	ctor_tramp_used += sizeof(struct ctor_abi_trampoline);

	t->stp_lr_x0     = 0xA9BF03FE; /* stp x30, x0, [sp, #-16]! — save LR + this */
	t->ldr_x16      = 0x580000B0; /* ldr x16, [pc, #20] — load target from .quad */
	t->blr_x16      = 0xD63F0200; /* blr x16 — call real ctor/dtor */
	t->ldp_lr_x0    = 0xA8C103FE; /* ldp x30, x0, [sp], #16 — restore LR + this */
	t->ret           = 0xD65F03C0; /* ret */
	t->pad           = 0xD503201F; /* nop (alignment for .quad) */
	t->target_addr   = real_addr;

	__builtin___clear_cache((char*)t, (char*)(t + 1));
	ctor_tramp_count++;

	return (uintptr_t)t;
}

/* ---- Apple ARM64 variadic calling convention adapter ----
 *
 * On macOS ARM64, ALL variadic arguments are passed on the stack.
 * On Linux AAPCS64, variadic arguments go in registers (x0-x7) first.
 *
 * The fix: wrap each variadic function with a thunk that builds a Linux
 * va_list struct pointing at the macOS stack args, then calls the v*
 * counterpart (sprintf → vsprintf, printf → vprintf, etc.).
 *
 * The Linux va_list has a __stack pointer and offset fields. When
 * __gr_offs = 0 and __vr_offs = 0, va_arg always reads from __stack.
 * Since macOS and Linux use the same 8-byte-aligned stack layout for
 * spilled args, we just point __stack at the macOS data.
 *
 * This handles unlimited args and mixed int/float types correctly.
 */

struct variadic_thunk {
	uint32_t stp;           /* +0:  stp x29, x30, [sp, #-48]! */
	uint32_t mov_fp;        /* +4:  mov x29, sp */
	uint32_t load_source;   /* +8:  add x8, x29, #48 (stack) OR mov x8, x{N} (va_list) */
	uint32_t str_stack;     /* +12: str x8, [sp, #16]    — va_list.__stack */
	uint32_t str_gr_top;    /* +16: str xzr, [sp, #24]   — va_list.__gr_top = NULL */
	uint32_t str_vr_top;    /* +20: str xzr, [sp, #32]   — va_list.__vr_top = NULL */
	uint32_t stp_offs;      /* +24: stp wzr, wzr, [sp, #40] — __gr_offs=0, __vr_offs=0 */
	uint32_t set_va_ptr;    /* +28: add x{D}, sp, #16    — &linux_va_list */
	uint32_t ldr_target;    /* +32: ldr x16, [pc, #16]   — load from +48 */
	uint32_t blr_target;    /* +36: blr x16 */
	uint32_t ldp_restore;   /* +40: ldp x29, x30, [sp], #48 */
	uint32_t ret_instr;     /* +44: ret */
	uint64_t target_addr;   /* +48: v* function address (8-byte aligned) */
};

struct variadic_info {
	const char *name;       /* import symbol (without leading underscore) */
	const char *v_name;     /* v* counterpart to dlsym */
	int dest_reg;           /* register that receives &linux_va_list */
	int is_va_list;         /* 1 = source is a register (va_list passthrough) */
	int source_reg;         /* for va_list: which register holds macOS va_list */
};

static const struct variadic_info variadic_functions[] = {
	/* True variadic → v* counterpart.
	 * dest_reg = number of fixed args = where va_list ptr goes in v* call */
	{"printf",          "vprintf",          1, 0, 0},
	{"fprintf",         "vfprintf",         2, 0, 0},
	{"sprintf",         "vsprintf",         2, 0, 0},
	{"snprintf",        "vsnprintf",        3, 0, 0},
	{"sscanf",          "vsscanf",          2, 0, 0},
	{"__sprintf_chk",   "__vsprintf_chk",   4, 0, 0},
	/* va_list passthrough — same glibc function, converted va_list.
	 * source_reg = register holding macOS va_list (char*) */
	{"vsnprintf",       "vsnprintf",        3, 1, 3},
	{"vfprintf",        "vfprintf",         2, 1, 2},
	{"vsscanf",         "vsscanf",          2, 1, 2},
	{"__vsnprintf_chk", "__vsnprintf_chk",  5, 1, 5},
	{NULL, NULL, 0, 0, 0}
};

static const struct variadic_info* lookup_variadic(const char* name)
{
	for (int i = 0; variadic_functions[i].name; i++) {
		if (strcmp(name, variadic_functions[i].name) == 0)
			return &variadic_functions[i];
	}
	return NULL;
}

static uint8_t* variadic_pool = NULL;
static size_t variadic_pool_used = 0;
static size_t variadic_pool_capacity = 0;
static uint8_t* stack_arg_pool = NULL;
static size_t stack_arg_pool_used = 0;
static size_t stack_arg_pool_capacity = 0;
static int variadic_thunk_count = 0;
static int stack_arg_thunk_count = 0;

static uintptr_t create_variadic_thunk(uintptr_t v_func_addr,
                                       const struct variadic_info* vi)
{
	if (!variadic_pool) {
		long page_size = sysconf(_SC_PAGESIZE);
		variadic_pool_capacity = page_size * 2; /* 2 pages, room for ~120 thunks */
		variadic_pool = mmap(NULL, variadic_pool_capacity,
		                     PROT_READ | PROT_WRITE | PROT_EXEC,
		                     MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
		if (variadic_pool == MAP_FAILED) {
			fprintf(stderr, "resolver: failed to allocate variadic thunk pool\n");
			variadic_pool = NULL;
			return 0;
		}
	}

	if (variadic_pool_used + sizeof(struct variadic_thunk) > variadic_pool_capacity) {
		fprintf(stderr, "resolver: variadic thunk pool exhausted\n");
		return 0;
	}

	struct variadic_thunk* t =
		(struct variadic_thunk*)(variadic_pool + variadic_pool_used);
	variadic_pool_used += sizeof(struct variadic_thunk);

	t->stp         = 0xA9BD7BFD; /* stp x29, x30, [sp, #-48]! */
	t->mov_fp      = 0x910003FD; /* mov x29, sp */

	if (vi->is_va_list)
		/* mov x8, x{source_reg} — grab macOS va_list from register */
		t->load_source = 0xAA0003E8 | ((uint32_t)vi->source_reg << 16);
	else
		/* add x8, x29, #48 — compute caller's sp (macOS variadic args) */
		t->load_source = 0x9100C3A8;

	t->str_stack   = 0xF9000BE8; /* str x8, [sp, #16]    — __stack */
	t->str_gr_top  = 0xF9000FFF; /* str xzr, [sp, #24]   — __gr_top = NULL */
	t->str_vr_top  = 0xF90013FF; /* str xzr, [sp, #32]   — __vr_top = NULL */
	t->stp_offs    = 0x29057FFF; /* stp wzr, wzr, [sp, #40] — __gr_offs=0, __vr_offs=0 */

	/* add x{dest_reg}, sp, #16 */
	t->set_va_ptr  = 0x910043E0 | (uint32_t)vi->dest_reg;

	t->ldr_target  = 0x58000090; /* ldr x16, [pc, #16]   — load from +48 */
	t->blr_target  = 0xD63F0200; /* blr x16 */
	t->ldp_restore = 0xA8C37BFD; /* ldp x29, x30, [sp], #48 */
	t->ret_instr   = 0xD65F03C0; /* ret */
	t->target_addr = v_func_addr;

	__builtin___clear_cache((char*)t, (char*)(t + 1));
	variadic_thunk_count++;

	return (uintptr_t)t;
}

struct stack_arg3_thunk {
	uint32_t stp;
	uint32_t mov_fp;
	uint32_t ldr_arg2;
	uint32_t ldr_target;
	uint32_t blr_target;
	uint32_t ldp_restore;
	uint32_t ret_instr;
	uint32_t nop;
	uint64_t target_addr;
};

static uintptr_t create_stack_arg3_thunk(uintptr_t func_addr)
{
	if (!stack_arg_pool) {
		long page_size = sysconf(_SC_PAGESIZE);
		stack_arg_pool_capacity = page_size;
		stack_arg_pool = mmap(NULL, stack_arg_pool_capacity,
		                      PROT_READ | PROT_WRITE | PROT_EXEC,
		                      MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
		if (stack_arg_pool == MAP_FAILED) {
			fprintf(stderr, "resolver: failed to allocate stack-arg thunk pool\n");
			stack_arg_pool = NULL;
			return 0;
		}
	}

	if (stack_arg_pool_used + sizeof(struct stack_arg3_thunk) > stack_arg_pool_capacity) {
		fprintf(stderr, "resolver: stack-arg thunk pool exhausted\n");
		return 0;
	}

	struct stack_arg3_thunk* t =
		(struct stack_arg3_thunk*)(stack_arg_pool + stack_arg_pool_used);
	stack_arg_pool_used += sizeof(struct stack_arg3_thunk);

	t->stp = 0xA9BF7BFD;         /* stp x29, x30, [sp, #-16]! */
	t->mov_fp = 0x910003FD;      /* mov x29, sp */
	t->ldr_arg2 = 0xF9400BA2;    /* ldr x2, [x29, #16] */
	t->ldr_target = 0x580000B0;  /* ldr x16, [pc, #20] */
	t->blr_target = 0xD63F0200;  /* blr x16 */
	t->ldp_restore = 0xA8C17BFD; /* ldp x29, x30, [sp], #16 */
	t->ret_instr = 0xD65F03C0;   /* ret */
	t->nop = 0xD503201F;
	t->target_addr = func_addr;

	__builtin___clear_cache((char*)t, (char*)(t + 1));
	stack_arg_thunk_count++;

	return (uintptr_t)t;
}

static uintptr_t wrap_fixed_stack_arg_symbol(const char* lookup_name,
                                             uintptr_t result,
                                             void* provider_handle)
{
	const char* fixed_target_name = NULL;

	if (strcmp(lookup_name, "ioctl") == 0)
		fixed_target_name = "machgate_darwin_ioctl_fixed";
	else
		return result;

	void* fixed_target = NULL;
	if (provider_handle)
		fixed_target = dlsym(provider_handle, fixed_target_name);
	if (!fixed_target)
		fixed_target = dlsym(RTLD_DEFAULT, fixed_target_name);
	if (!fixed_target)
		return result;

	uintptr_t thunk = create_stack_arg3_thunk((uintptr_t)fixed_target);
	return thunk ? thunk : result;
}

/* ---- Mach-O symbol table lookup ----
 *
 * Resolves symbols defined in the Mach-O binary itself.
 * Used for lib_ordinal -1 (main executable) and -3 (weak lookup).
 */
struct macho_symbol_result {
	uintptr_t addr;
	uint8_t type;
	uint16_t desc;
	uint8_t sect;
};

static int lookup_macho_symbol_result(struct resolver_state* rs,
                                      const char* name,
                                      struct macho_symbol_result* result)
{
	struct mach_header_64* mh = rs->mh;
	uint8_t* cmds = (uint8_t*)(mh + 1);
	uint32_t p = 0;
	struct symtab_command* symtab = NULL;

	for (uint32_t i = 0; i < mh->ncmds && p < mh->sizeofcmds; i++) {
		struct load_command* lc = (struct load_command*)&cmds[p];
		if (lc->cmd == LC_SYMTAB)
			symtab = (struct symtab_command*)lc;
		p += lc->cmdsize;
	}
	if (!symtab) return 0;

	/* Convert file offsets to memory addresses using segment mappings */
	struct nlist_64* syms = NULL;
	char* strtab = NULL;

	/* Find LINKEDIT segment to convert file offsets */
	p = 0;
	for (uint32_t i = 0; i < mh->ncmds && p < mh->sizeofcmds; i++) {
		struct load_command* lc = (struct load_command*)&cmds[p];
		if (lc->cmd == LC_SEGMENT_64) {
			struct segment_command_64* seg = (struct segment_command_64*)lc;
			if (symtab->symoff >= seg->fileoff &&
			    symtab->symoff < seg->fileoff + seg->filesize) {
				uintptr_t base = seg->vmaddr + rs->slide;
				syms = (struct nlist_64*)(base + (symtab->symoff - seg->fileoff));
				strtab = (char*)(base + (symtab->stroff - seg->fileoff));
			}
		}
		p += lc->cmdsize;
	}
	if (!syms || !strtab) return 0;

	for (uint32_t i = 0; i < symtab->nsyms; i++) {
		struct nlist_64* nl = &syms[i];
		if (nl->n_type & N_STAB) continue;
		if ((nl->n_type & N_TYPE) != N_SECT) continue;
		if (nl->n_strx >= symtab->strsize) continue;

		const char* sym = &strtab[nl->n_strx];
		if (strcmp(sym, name) == 0) {
			if (result) {
				result->addr = nl->n_value + rs->slide;
				result->type = nl->n_type;
				result->desc = nl->n_desc;
				result->sect = nl->n_sect;
			}
			return 1;
		}
	}
	return 0;
}

static uintptr_t lookup_macho_symbol(struct resolver_state* rs, const char* name)
{
	struct macho_symbol_result result;

	if (!lookup_macho_symbol_result(rs, name, &result))
		return 0;
	return result.addr;
}

static int macho_symbol_is_external_definition(const struct macho_symbol_result* result)
{
	if (!result)
		return 0;
	if (!(result->type & N_EXT))
		return 0;
	if (result->type & N_PEXT)
		return 0;
	return 1;
}

static uint32_t macho_section_flags(struct mach_header_64* mh, uint8_t section_ordinal)
{
	uint8_t* cmds;
	uint32_t p = 0;
	uint8_t ordinal = 1;

	if (!mh || !section_ordinal)
		return 0;

	cmds = (uint8_t*)(mh + 1);
	for (uint32_t i = 0; i < mh->ncmds && p < mh->sizeofcmds; i++) {
		struct load_command* lc = (struct load_command*)&cmds[p];
		if (lc->cmd == LC_SEGMENT_64) {
			struct segment_command_64* seg = (struct segment_command_64*)lc;
			struct section_64* sect = (struct section_64*)(seg + 1);
			for (uint32_t j = 0; j < seg->nsects; j++, sect++, ordinal++) {
				if (ordinal == section_ordinal)
					return sect->flags;
			}
		}
		p += lc->cmdsize;
	}

	return 0;
}

static int macho_symbol_is_stub_definition(struct mach_header_64* mh,
                                           const struct macho_symbol_result* result)
{
	if (!result)
		return 0;
	return (macho_section_flags(mh, result->sect) & SECTION_TYPE) == S_SYMBOL_STUBS;
}

static int elf_symbol_is_weak(void* addr)
{
#ifdef RTLD_DL_SYMENT
	Dl_info info;
	ElfW(Sym)* sym = NULL;

	if (!addr)
		return 0;
	if (!dladdr1(addr, &info, (void**)&sym, RTLD_DL_SYMENT) || !sym)
		return 0;
	return ELF64_ST_BIND(sym->st_info) == STB_WEAK;
#else
	(void)addr;
	return 0;
#endif
}

static uintptr_t resolve_main_cxx_operator_override(struct resolver_state* rs,
                                                    const char* sym_name,
                                                    void* provider_addr,
                                                    int64_t addend,
                                                    const char** source_kind,
                                                    const char** source_path)
{
	struct macho_symbol_result result;

	if (!rs || !rs->mh || !is_cxx_operator_symbol(sym_name))
		return 0;
	if (provider_addr && !elf_symbol_is_weak(provider_addr))
		return 0;
	if (!lookup_macho_symbol_result(rs, sym_name, &result))
		return 0;
	if (!macho_symbol_is_external_definition(&result))
		return 0;
	if (macho_symbol_is_stub_definition(rs->mh, &result))
		return 0;

	if (source_kind)
		*source_kind = "main executable C++ allocator";
	if (source_path)
		*source_path = "main executable";
	return result.addr + addend;
}

static uintptr_t resolve_cxx_operator_fallback(const char* sym_name,
                                               const char** source_kind,
                                               const char** source_path)
{
	uintptr_t addr;

	if (!is_cxx_operator_symbol(sym_name))
		return 0;

	addr = resolve_cxx_operator_hook(sym_name);
	if (addr) {
		if (source_kind)
			*source_kind = "machgate c++ allocator hook";
		if (source_path)
			*source_path = "libsystem_shim";
		return addr;
	}

	return 0;
}

/* Public API: look up symbol by name in a Mach-O nlist */
uintptr_t resolver_lookup_symbol(void* mh_ptr, uintptr_t slide, const char* name)
{
	struct resolver_state rs = {0};
	rs.mh = (struct mach_header_64*)mh_ptr;
	rs.slide = slide;
	return lookup_macho_symbol(&rs, name);
}

uintptr_t resolver_lookup_external_definition(void* mh_ptr, uintptr_t slide,
                                              const char* name)
{
	struct resolver_state rs = {0};
	struct macho_symbol_result result;

	rs.mh = (struct mach_header_64*)mh_ptr;
	rs.slide = slide;
	if (!lookup_macho_symbol_result(&rs, name, &result))
		return 0;
	if (!macho_symbol_is_external_definition(&result))
		return 0;
	if (macho_symbol_is_stub_definition(rs.mh, &result))
		return 0;
	return result.addr;
}

/* ---- Symbol extent (sorted N_SECT cache) ---- */

struct sym_entry { uint64_t vmaddr; uint32_t n_strx; uint8_t n_sect; };

struct extent_cache {
	struct mach_header_64* mh;
	struct sym_entry* syms;    /* sorted by vmaddr */
	size_t nsyms;
	char* strtab;
	size_t strsize;
	uint64_t* section_ends;    /* vmaddr + size per n_sect index (1-based) */
	size_t nsects;
};

static struct extent_cache g_extent_cache;

static int sym_cmp(const void* a, const void* b)
{
	uint64_t va = ((const struct sym_entry*)a)->vmaddr;
	uint64_t vb = ((const struct sym_entry*)b)->vmaddr;
	if (va < vb) return -1;
	if (va > vb) return 1;
	return 0;
}

static int build_extent_cache(struct mach_header_64* mh, uintptr_t slide)
{
	if (g_extent_cache.mh == mh && g_extent_cache.syms)
		return 0;

	free(g_extent_cache.syms);
	free(g_extent_cache.section_ends);
	memset(&g_extent_cache, 0, sizeof(g_extent_cache));

	uint8_t* cmds = (uint8_t*)(mh + 1);
	struct symtab_command* symtab = NULL;

	/* First pass: count sections and find symtab */
	uint32_t p = 0;
	size_t total_sects = 0;
	for (uint32_t i = 0; i < mh->ncmds && p < mh->sizeofcmds; i++) {
		struct load_command* lc = (struct load_command*)&cmds[p];
		if (lc->cmd == LC_SYMTAB)
			symtab = (struct symtab_command*)lc;
		else if (lc->cmd == LC_SEGMENT_64)
			total_sects += ((struct segment_command_64*)lc)->nsects;
		p += lc->cmdsize;
	}
	if (!symtab) return -1;

	/* Allocate section_ends (1-based: index 1..total_sects) */
	g_extent_cache.nsects = total_sects + 1;
	g_extent_cache.section_ends = calloc(g_extent_cache.nsects, sizeof(uint64_t));
	if (!g_extent_cache.section_ends) return -1;

	/* Second pass: locate symtab/strtab via __LINKEDIT, fill section_ends */
	struct nlist_64* syms_mem = NULL;
	char* strtab = NULL;
	p = 0;
	uint32_t sect_idx = 1;
	for (uint32_t i = 0; i < mh->ncmds && p < mh->sizeofcmds; i++) {
		struct load_command* lc = (struct load_command*)&cmds[p];
		if (lc->cmd == LC_SEGMENT_64) {
			struct segment_command_64* seg = (struct segment_command_64*)lc;
			if (symtab->symoff >= seg->fileoff &&
			    symtab->symoff < seg->fileoff + seg->filesize) {
				uintptr_t base = seg->vmaddr + slide;
				syms_mem = (struct nlist_64*)(base + (symtab->symoff - seg->fileoff));
				strtab = (char*)(base + (symtab->stroff - seg->fileoff));
			}
			struct section_64* sects = (struct section_64*)(seg + 1);
			for (uint32_t j = 0; j < seg->nsects && sect_idx < g_extent_cache.nsects; j++)
				g_extent_cache.section_ends[sect_idx++] = sects[j].addr + sects[j].size;
		}
		p += lc->cmdsize;
	}
	if (!syms_mem || !strtab) return -1;

	g_extent_cache.strtab = strtab;
	g_extent_cache.strsize = symtab->strsize;

	/* Collect all N_SECT entries into a sortable array */
	struct sym_entry* arr = malloc(symtab->nsyms * sizeof(*arr));
	if (!arr) return -1;
	size_t n = 0;
	for (uint32_t i = 0; i < symtab->nsyms; i++) {
		struct nlist_64* nl = &syms_mem[i];
		if (nl->n_type & N_STAB) continue;
		if ((nl->n_type & N_TYPE) != N_SECT) continue;
		if (nl->n_strx >= symtab->strsize) continue;
		arr[n].vmaddr = nl->n_value;
		arr[n].n_strx = nl->n_strx;
		arr[n].n_sect = nl->n_sect;
		n++;
	}
	qsort(arr, n, sizeof(*arr), sym_cmp);
	g_extent_cache.syms = arr;
	g_extent_cache.nsyms = n;
	g_extent_cache.mh = mh;
	return 0;
}

int resolver_symbol_extent(void* mh_ptr, uintptr_t slide, const char* name,
                           uintptr_t* out_start, uintptr_t* out_end)
{
	struct mach_header_64* mh = (struct mach_header_64*)mh_ptr;
	if (build_extent_cache(mh, slide) != 0)
		return -1;

	for (size_t i = 0; i < g_extent_cache.nsyms; i++) {
		const char* s = g_extent_cache.strtab + g_extent_cache.syms[i].n_strx;
		if (strcmp(s, name) != 0) continue;

		uint64_t start = g_extent_cache.syms[i].vmaddr;
		uint8_t sect = g_extent_cache.syms[i].n_sect;
		uint64_t end = 0;
		/* Next symbol in the same section gives the tightest bound. */
		for (size_t j = i + 1; j < g_extent_cache.nsyms; j++) {
			if (g_extent_cache.syms[j].n_sect != sect) continue;
			end = g_extent_cache.syms[j].vmaddr;
			break;
		}
		if (!end && sect < g_extent_cache.nsects)
			end = g_extent_cache.section_ends[sect];
		if (end <= start) return -1;
		*out_start = (uintptr_t)start + slide;
		*out_end = (uintptr_t)end + slide;
		return 0;
	}
	return -1;
}

/* ---- ULEB128 / SLEB128 decoders ----
 * Based on libiberty (FSF) — used by dyld for LC_DYLD_INFO opcode streams. */

static inline size_t read_uleb128(const uint8_t* buf, const uint8_t* end, uint64_t* r)
{
	const uint8_t* p = buf;
	unsigned int shift = 0;
	uint64_t result = 0;
	while (p < end) {
		uint8_t byte = *p++;
		result |= ((uint64_t)(byte & 0x7f)) << shift;
		if ((byte & 0x80) == 0) break;
		shift += 7;
	}
	*r = result;
	return p - buf;
}

static inline size_t read_sleb128(const uint8_t* buf, const uint8_t* end, int64_t* r)
{
	const uint8_t* p = buf;
	unsigned int shift = 0;
	int64_t result = 0;
	uint8_t byte = 0;
	while (p < end) {
		byte = *p++;
		result |= ((uint64_t)(byte & 0x7f)) << shift;
		shift += 7;
		if ((byte & 0x80) == 0) break;
	}
	if (shift < 64 && (byte & 0x40))
		result |= -(((uint64_t)1) << shift);
	*r = result;
	return p - buf;
}

/* ---- LC_DYLD_INFO rebase opcodes ---- */

#define REBASE_TYPE_POINTER                  1

#define REBASE_OPCODE_MASK                   0xF0
#define REBASE_IMMEDIATE_MASK                0x0F
#define REBASE_OPCODE_DONE                   0x00
#define REBASE_OPCODE_SET_TYPE_IMM           0x10
#define REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB 0x20
#define REBASE_OPCODE_ADD_ADDR_ULEB          0x30
#define REBASE_OPCODE_ADD_ADDR_IMM_SCALED    0x40
#define REBASE_OPCODE_DO_REBASE_IMM_TIMES    0x50
#define REBASE_OPCODE_DO_REBASE_ULEB_TIMES   0x60
#define REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB 0x70
#define REBASE_OPCODE_DO_REBASE_ULEB_TIMES_SKIPPING_ULEB 0x80

/* ---- LC_DYLD_INFO bind opcodes ---- */

#define BIND_TYPE_POINTER                    1

#define BIND_OPCODE_MASK                     0xF0
#define BIND_IMMEDIATE_MASK                  0x0F
#define BIND_OPCODE_DONE                     0x00
#define BIND_OPCODE_SET_DYLIB_ORDINAL_IMM    0x10
#define BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB   0x20
#define BIND_OPCODE_SET_DYLIB_SPECIAL_IMM    0x30
#define BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM 0x40
#define BIND_OPCODE_SET_TYPE_IMM             0x50
#define BIND_OPCODE_SET_ADDEND_SLEB          0x60
#define BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB 0x70
#define BIND_OPCODE_ADD_ADDR_ULEB            0x80
#define BIND_OPCODE_DO_BIND                  0x90
#define BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB    0xA0
#define BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED 0xB0
#define BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB 0xC0

#define BIND_SYMBOL_FLAGS_WEAK_IMPORT        0x01

/* ---- Forward declarations ---- */

static int parse_load_commands(struct resolver_state* rs);
static int load_mapping_config(struct resolver_state* rs, const char* path);
static int open_dylibs(struct resolver_state* rs);
static int process_chained_fixups(struct resolver_state* rs);
static int process_dyld_info(struct resolver_state* rs);
static int walk_chain(struct resolver_state* rs, uint64_t* chain_start, uint16_t pointer_format,
                      const struct dyld_chained_fixups_header* header, const char* chain_data);
static void close_dylibs(struct resolver_state* rs);
static const char* basename_from_path(const char* path);

/* ---- Implementation ---- */

int resolver_resolve_fixups(void* mh, uintptr_t slide, const char* map_file)
{
	struct resolver_state rs = {0};
	int ret = -1;

	rs.mh = (struct mach_header_64*)mh;
	rs.slide = slide;
	rs.mh_addr = (uintptr_t)mh;

	machgate_log_startup("resolver: mach header at %p, slide=0x%lx\n", mh, slide);

	/* Step 1: Parse load commands to find dylibs, segments, fixup info */
	if (parse_load_commands(&rs) < 0) {
		fprintf(stderr, "resolver: failed to parse load commands\n");
		goto out;
	}

	if (!rs.has_chained_fixups && !rs.has_dyld_info) {
		machgate_log_startup("resolver: no LC_DYLD_CHAINED_FIXUPS or LC_DYLD_INFO found — nothing to resolve\n");
		ret = 0;
		goto out;
	}

	if (rs.has_chained_fixups)
		machgate_log_startup("resolver: found %d dylibs, chained fixups at file offset 0x%x (size %u)\n",
		                      rs.ndylibs, rs.fixups_dataoff, rs.fixups_datasize);
	else
		machgate_log_startup("resolver: found %d dylibs, LC_DYLD_INFO (bind=%u rebase=%u weak=%u lazy=%u bytes)\n",
		                      rs.ndylibs, rs.dyld_info_bind_size, rs.dyld_info_rebase_size,
		                      rs.dyld_info_weak_bind_size, rs.dyld_info_lazy_bind_size);

	/* Step 2: Load dylib mapping config */
	if (load_mapping_config(&rs, map_file) < 0) {
		fprintf(stderr, "resolver: warning: no mapping config loaded, using defaults\n");
	}

	/* Step 3: Open Linux .so files for each mapped dylib */
	if (open_dylibs(&rs) < 0) {
		fprintf(stderr, "resolver: failed to open dylibs\n");
		goto out;
	}

	/* Pre-load libgcc_s globally so compiler runtime symbols (__clear_cache,
	 * __register_frame, _Unwind_*, etc.) are available via RTLD_DEFAULT.
	 * Without this, they aren't found because libgcc_s is not in the
	 * global symbol scope by default on some Linux configurations. */
	dlopen("libgcc_s.so.1", RTLD_GLOBAL | RTLD_LAZY);

	/* Resolve shim's malloc/free for operator new/delete hooks.
	 * Find the libSystem.B dylib entry — its handle is the shim .so.
	 * Must use the specific handle, not RTLD_DEFAULT, since glibc's malloc
	 * would be found first in the global search. */
	if (!shim_malloc_fn) {
		for (int i = 0; i < rs.ndylibs && i < MAX_DYLIBS; i++) {
			if (rs.dylibs[i].action == DYLIB_MAP &&
			    strstr(rs.dylibs[i].name, "libSystem")) {
				shim_malloc_fn = (void *(*)(size_t))dlsym(rs.dylibs[i].handle, "malloc");
				shim_memalign_fn = (void *(*)(size_t, size_t))dlsym(rs.dylibs[i].handle,
				                                                    "machgate_shim_memalign");
				shim_free_fn = (void (*)(void *))dlsym(rs.dylibs[i].handle, "free");
				break;
			}
		}
	}

	/* Step 4: Walk fixups and resolve */
	if (rs.has_chained_fixups) {
		if (process_chained_fixups(&rs) < 0) {
			fprintf(stderr, "resolver: failed processing chained fixups\n");
			goto out;
		}
	} else if (rs.has_dyld_info) {
		if (process_dyld_info(&rs) < 0) {
			fprintf(stderr, "resolver: failed processing LC_DYLD_INFO\n");
			goto out;
		}
	}

	machgate_log_startup("resolver: done — %d binds resolved, %d stubbed, %d failed, %d rebases, %d ctor/dtor ABI adapters, %d variadic thunks, %d stack-arg thunks\n",
	                      rs.binds_resolved, rs.binds_stubbed, rs.binds_failed, rs.rebases_applied, ctor_tramp_count, variadic_thunk_count, stack_arg_thunk_count);

	/* Save dylib state for deferred completion (runs later from game thread) */
	if (g_deferred.active && !g_deferred.resolved) {
		g_deferred.mh = rs.mh;
		g_deferred.slide = rs.slide;
		g_deferred.mh_addr = rs.mh_addr;
		memcpy(g_deferred.dylibs, rs.dylibs, sizeof(rs.dylibs));
		g_deferred.ndylibs = rs.ndylibs;
		fprintf(stderr, "resolver: %d deferred binds recorded, waiting for SDL_GL_CreateContext\n",
				g_deferred.count);
	}

	ret = 0;

out:
	close_dylibs(&rs);
	return ret;
}

/* ---- Parse Mach-O load commands ---- */

static int parse_load_commands(struct resolver_state* rs)
{
	uint8_t* cmd_ptr = (uint8_t*)(rs->mh + 1); /* right after the header */
	int dylib_ordinal = 0;

	for (uint32_t i = 0; i < rs->mh->ncmds; i++) {
		struct load_command* lc = (struct load_command*)cmd_ptr;

		switch (lc->cmd) {
		case LC_LOAD_DYLIB:
		case LC_LOAD_WEAK_DYLIB:
		case LC_REEXPORT_DYLIB: {
			struct dylib_command* dc = (struct dylib_command*)lc;
			const char* name = ((char*)dc) + dc->dylib.name.offset;

			dylib_ordinal++;
			if (dylib_ordinal <= MAX_DYLIBS) {
				struct dylib_entry* de = &rs->dylibs[dylib_ordinal - 1];
				de->ordinal = dylib_ordinal;
				const char* base = basename_from_path(name);
				strncpy(de->name, base, MAX_NAME - 1);
				de->action = DYLIB_SKIP; /* default: skip until mapped */
				de->handle = NULL;
			}
			break;
		}
		case LC_SEGMENT_64: {
			struct segment_command_64* seg = (struct segment_command_64*)lc;

			if (strcmp(seg->segname, "__LINKEDIT") == 0) {
				rs->linkedit.vmaddr = seg->vmaddr;
				rs->linkedit.vmsize = seg->vmsize;
				rs->linkedit.fileoff = seg->fileoff;
				rs->linkedit.filesize = seg->filesize;
			}

			if (rs->nsegments < 16) {
				struct seg_info* si = &rs->segments[rs->nsegments];
				si->vmaddr = seg->vmaddr;
				si->vmsize = seg->vmsize;
				si->fileoff = seg->fileoff;
				si->filesize = seg->filesize;
				si->prot = seg->initprot;
				rs->nsegments++;
			}
			break;
		}
		case LC_DYLD_CHAINED_FIXUPS: {
			struct linkedit_data_command* ldc = (struct linkedit_data_command*)lc;
			rs->fixups_dataoff = ldc->dataoff;
			rs->fixups_datasize = ldc->datasize;
			rs->has_chained_fixups = true;
			break;
		}
		case LC_DYLD_INFO:
		case LC_DYLD_INFO_ONLY: {
			struct dyld_info_command* dic = (struct dyld_info_command*)lc;
			rs->dyld_info_rebase_off = dic->rebase_off;
			rs->dyld_info_rebase_size = dic->rebase_size;
			rs->dyld_info_bind_off = dic->bind_off;
			rs->dyld_info_bind_size = dic->bind_size;
			rs->dyld_info_weak_bind_off = dic->weak_bind_off;
			rs->dyld_info_weak_bind_size = dic->weak_bind_size;
			rs->dyld_info_lazy_bind_off = dic->lazy_bind_off;
			rs->dyld_info_lazy_bind_size = dic->lazy_bind_size;
			rs->has_dyld_info = true;
			break;
		}
		}

		cmd_ptr += lc->cmdsize;
	}

	rs->ndylibs = dylib_ordinal;
	return 0;
}

/* ---- Convert file offset to memory address via LINKEDIT mapping ---- */

static void* fileoff_to_mem(struct resolver_state* rs, uint32_t fileoff)
{
	/* __LINKEDIT maps a range of file offsets into memory */
	if (fileoff >= rs->linkedit.fileoff &&
	    fileoff < rs->linkedit.fileoff + rs->linkedit.filesize) {
		return (void*)(rs->linkedit.vmaddr + rs->slide +
		               (fileoff - rs->linkedit.fileoff));
	}
	/* Fall back: try all segments */
	for (int i = 0; i < rs->nsegments; i++) {
		struct seg_info* si = &rs->segments[i];
		if (fileoff >= si->fileoff && fileoff < si->fileoff + si->filesize) {
			return (void*)(si->vmaddr + rs->slide + (fileoff - si->fileoff));
		}
	}
	return NULL;
}

/* ---- Dylib mapping config ---- */

static int load_mapping_config(struct resolver_state* rs, const char* path)
{
	FILE* f;
	char line[512];

	if (!path) {
		/* Try default location next to mldr */
		path = "dylib_map.conf";
	}

	f = fopen(path, "r");
	if (!f) {
		/* Not an error — will use defaults */
		return -1;
	}

	while (fgets(line, sizeof(line), f) && rs->nmappings < MAX_MAPPINGS) {
		/* Strip newline */
		char* nl = strchr(line, '\n');
		if (nl) *nl = '\0';

		/* Skip comments and empty lines */
		char* p = line;
		while (*p == ' ' || *p == '\t') p++;
		if (*p == '#' || *p == '\0') continue;

		/* Parse: dylib_pattern = so_path */
		char* eq = strchr(p, '=');
		if (!eq) continue;

		*eq = '\0';
		char* key = p;
		char* val = eq + 1;

		/* Trim whitespace */
		while (*key == ' ' || *key == '\t') key++;
		char* end = key + strlen(key) - 1;
		while (end > key && (*end == ' ' || *end == '\t')) *end-- = '\0';

		while (*val == ' ' || *val == '\t') val++;
		end = val + strlen(val) - 1;
		while (end > val && (*end == ' ' || *end == '\t')) *end-- = '\0';

		struct dylib_mapping* m = &rs->mappings[rs->nmappings++];
		snprintf(m->dylib_pattern, MAX_NAME, "%s", key);
		snprintf(m->so_path, MAX_NAME, "%s", val);
	}

	fclose(f);
	machgate_log_startup("resolver: loaded %d dylib mappings from %s\n", rs->nmappings, path);
	return 0;
}

/* ---- Match a dylib name against mappings ---- */

static const struct dylib_mapping* find_mapping(struct resolver_state* rs, const char* dylib_name)
{
	for (int i = 0; i < rs->nmappings; i++) {
		/* Simple substring match — pattern can match anywhere in the name */
		if (strstr(dylib_name, rs->mappings[i].dylib_pattern)) {
			return &rs->mappings[i];
		}
	}
	return NULL;
}

/* ---- Open Linux .so files for mapped dylibs ---- */

static int open_dylibs(struct resolver_state* rs)
{
	for (int i = 0; i < rs->ndylibs && i < MAX_DYLIBS; i++) {
		struct dylib_entry* de = &rs->dylibs[i];
		const struct dylib_mapping* m = find_mapping(rs, de->name);

		if (!m) {
			machgate_log_startup("resolver: dylib[%d] '%s' — no mapping, skipping\n",
			                      de->ordinal, de->name);
			de->action = DYLIB_SKIP;
			continue;
		}

		if (strcmp(m->so_path, "STUB") == 0) {
			machgate_log_startup("resolver: dylib[%d] '%s' — stubbed\n",
			                      de->ordinal, de->name);
			de->action = DYLIB_STUB;
			continue;
		}

		if (strcmp(m->so_path, "SKIP") == 0) {
			machgate_log_startup("resolver: dylib[%d] '%s' — skipped\n",
			                      de->ordinal, de->name);
			de->action = DYLIB_SKIP;
			continue;
		}

		/* MACHO: prefix — look up in loaded Mach-O dylib */
		if (strncmp(m->so_path, "MACHO:", 6) == 0) {
			const char* macho_path = m->so_path + 6;
			/* Check if already loaded */
			struct macho_dylib_info* mdi = dylib_loader_find(macho_path);
			if (!mdi) {
				/* Load it now */
				mdi = dylib_loader_load(macho_path);
			}
			if (mdi) {
				de->macho_info = mdi;
				de->action = DYLIB_MACHO;
				strncpy(de->so_path, macho_path, MAX_NAME - 1);
				machgate_log_startup("resolver: dylib[%d] '%s' → MACHO '%s' — loaded (%u symbols)\n",
				                      de->ordinal, de->name, macho_path, mdi->nsyms);
			} else {
				fprintf(stderr, "resolver: dylib[%d] '%s' → MACHO '%s' — load FAILED\n",
						de->ordinal, de->name, macho_path);
				de->action = DYLIB_SKIP;
			}
			continue;
		}

		/* DEFERRED: prefix — defer dlopen until trigger (SDL_GL_CreateContext) */
		if (strncmp(m->so_path, "DEFERRED:", 9) == 0) {
			de->action = DYLIB_DEFERRED;
			snprintf(de->so_path, MAX_NAME, "%s", m->so_path + 9);
			g_deferred.active = true;
			machgate_log_startup("resolver: dylib[%d] '%s' → DEFERRED '%s'\n",
			                      de->ordinal, de->name, de->so_path);
			continue;
		}

		/* Try to dlopen the Linux .so */
		de->handle = dlopen(m->so_path, RTLD_LAZY | RTLD_GLOBAL);
		if (!de->handle) {
			fprintf(stderr, "resolver: dylib[%d] '%s' → '%s' — dlopen FAILED: %s\n",
					de->ordinal, de->name, m->so_path, dlerror());
			de->action = DYLIB_SKIP;
			continue;
		}

		snprintf(de->so_path, MAX_NAME, "%s", m->so_path);
		de->action = DYLIB_MAP;
		machgate_log_startup("resolver: dylib[%d] '%s' → '%s' — loaded\n",
		                      de->ordinal, de->name, m->so_path);
	}

	return 0;
}

/* ---- Make segment writable for patching ---- */

static void make_segment_writable(struct resolver_state* rs, uintptr_t seg_base,
                                   uint16_t page_count, uint16_t page_size)
{
	long sys_page = sysconf(_SC_PAGESIZE);
	uintptr_t start = seg_base & ~(uintptr_t)(sys_page - 1);
	uintptr_t end = seg_base + (uintptr_t)page_count * page_size;
	end = (end + sys_page - 1) & ~(uintptr_t)(sys_page - 1);
	if (mprotect((void*)start, end - start, PROT_READ | PROT_WRITE) != 0) {
		fprintf(stderr, "resolver: WARNING: mprotect(%p, 0x%lx) failed: %s\n",
				(void*)start, (unsigned long)(end - start), strerror(errno));
	}
}

/* ---- Process chained fixups ---- */

static int process_chained_fixups(struct resolver_state* rs)
{
	/* Get the chained fixups data from mapped memory */
	char* chain_data = (char*)fileoff_to_mem(rs, rs->fixups_dataoff);
	if (!chain_data) {
		fprintf(stderr, "resolver: cannot locate chained fixups data in memory\n");
		return -1;
	}

	struct dyld_chained_fixups_header* header = (struct dyld_chained_fixups_header*)chain_data;

	fprintf(stderr, "resolver: fixups version=%u, imports_count=%u, imports_format=%u\n",
			header->fixups_version, header->imports_count, header->imports_format);

	/* Get the starts-in-image structure */
	struct dyld_chained_starts_in_image* starts =
		(struct dyld_chained_starts_in_image*)(chain_data + header->starts_offset);

	fprintf(stderr, "resolver: %u segments with fixup chains\n", starts->seg_count);

	/* Walk each segment's chains */
	for (uint32_t seg = 0; seg < starts->seg_count; seg++) {
		if (starts->seg_info_offset[seg] == 0)
			continue;

		struct dyld_chained_starts_in_segment* seg_starts =
			(struct dyld_chained_starts_in_segment*)(
				(char*)starts + starts->seg_info_offset[seg]);

		fprintf(stderr, "resolver: segment %u: page_size=0x%x, pointer_format=%u, "
				"segment_offset=0x%lx, page_count=%u\n",
				seg, seg_starts->page_size, seg_starts->pointer_format,
				(unsigned long)seg_starts->segment_offset, seg_starts->page_count);

		if (seg_starts->pointer_format != DYLD_CHAINED_PTR_64 &&
		    seg_starts->pointer_format != DYLD_CHAINED_PTR_64_OFFSET) {
			fprintf(stderr, "resolver: unsupported pointer format %u in segment %u\n",
					seg_starts->pointer_format, seg);
			continue;
		}

		/* Make the target segment writable for patching */
		uintptr_t seg_base = rs->mh_addr + seg_starts->segment_offset;
		make_segment_writable(rs, seg_base, seg_starts->page_count,
		                      seg_starts->page_size);

		/* Walk each page */
		for (uint16_t page = 0; page < seg_starts->page_count; page++) {
			uint16_t page_start = seg_starts->page_start[page];

			if (page_start == DYLD_CHAINED_PTR_START_NONE)
				continue;

			/* Calculate address of first fixup in this page */
			uintptr_t page_addr = seg_base + (page * seg_starts->page_size);
			uint64_t* chain_ptr = (uint64_t*)(page_addr + page_start);

			if (walk_chain(rs, chain_ptr, seg_starts->pointer_format,
			               header, chain_data) < 0) {
				fprintf(stderr, "resolver: error walking chain at page %u of segment %u\n",
						page, seg);
			}
		}
	}

	return 0;
}

/* ---- Shared stub allocator ---- */

static uintptr_t alloc_return0_stub(void)
{
	static uint8_t* pool = NULL;
	static size_t used = 0;
	static const size_t cap = 256 * 4096; /* 1MB */
	if (!pool) {
		pool = mmap(NULL, cap, PROT_READ | PROT_WRITE | PROT_EXEC,
		            MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
		if (pool == MAP_FAILED) { pool = NULL; return 0; }
	}
	if (used + 128 > cap) return 0;
	uintptr_t slot = (uintptr_t)(pool + used);
	uint32_t* code = (uint32_t*)(pool + used);
	code[0] = 0x52800000; /* mov w0, #0 */
	code[1] = 0xd65f03c0; /* ret */
	__builtin___clear_cache((char*)code, (char*)(code + 2));
	used += 128;
	return slot;
}

/* ---- Deferred library completion ----
 *
 * Called from wrapped_SDL_GL_CreateContext after the GL context is created.
 * dlopen's deferred libraries and resolves all recorded deferred binds. */

static void resolver_complete_deferred(void)
{
	if (g_deferred.resolved) return;
	g_deferred.resolved = true;

	/* Phase 1: dlopen all deferred libraries */
	for (int i = 0; i < g_deferred.ndylibs; i++) {
		struct dylib_entry* de = &g_deferred.dylibs[i];
		if (de->action != DYLIB_DEFERRED) continue;

		de->handle = dlopen(de->so_path, RTLD_LAZY | RTLD_GLOBAL);
		if (!de->handle) {
			fprintf(stderr, "resolver: deferred dlopen '%s' FAILED: %s\n",
					de->so_path, dlerror());
			continue;
		}
		de->action = DYLIB_MAP;
		fprintf(stderr, "resolver: deferred dlopen '%s' — loaded\n", de->so_path);
	}

	/* Phase 2: resolve all recorded deferred binds */
	long page_size = sysconf(_SC_PAGESIZE);
	int resolved = 0, failed = 0;
	struct resolver_state main_rs = {0};
	main_rs.mh = g_deferred.mh;
	main_rs.slide = g_deferred.slide;
	main_rs.mh_addr = g_deferred.mh_addr;
	for (int i = 0; i < g_deferred.count; i++) {
		struct deferred_bind* db = &g_deferred.binds[i];
		struct dylib_entry* de = &g_deferred.dylibs[db->lib_ordinal - 1];
		if (!de->handle) continue;

		const char* lookup = db->sym_name;
		if (lookup[0] == '_') lookup++;

		/* Strip $ suffixes */
		char stripped[256];
		const char* dollar = strchr(lookup, '$');
		if (dollar && (size_t)(dollar - lookup) < sizeof(stripped)) {
			memcpy(stripped, lookup, dollar - lookup);
			stripped[dollar - lookup] = '\0';
			lookup = stripped;
		}

		const char* source_kind = "mapped dylib handle";
		const char* source_path = de->so_path;
		void* addr = dlsym(de->handle, lookup);
		if (!addr && strncmp(lookup, "_ZN", 3) == 0)
			addr = try_mangling_variants(de->handle, lookup);
		uintptr_t main_override = resolve_main_cxx_operator_override(
		    main_rs.mh ? &main_rs : NULL, db->sym_name, addr, db->addend,
		    &source_kind, &source_path);
		if (main_override) {
			uintptr_t result = main_override;
			if (is_ctor_or_dtor(db->sym_name) && !ctor_has_stack_params(db->sym_name))
				result = wrap_ctor_for_apple_abi(result);

			/* mprotect GOT page (permissions may have been restored) */
			uintptr_t page = db->got_slot & ~(uintptr_t)(page_size - 1);
			mprotect((void*)page, page_size, PROT_READ | PROT_WRITE);
			*(uint64_t*)db->got_slot = result;
			trace_target_binding("deferred-cxx-main-override", db->sym_name, lookup,
			                     db->lib_ordinal, de, db->got_slot, result,
			                     source_kind, source_path);
			resolved++;
			continue;
		}
		if (!addr) {
			addr = (void*)resolve_cxx_operator_fallback(
			    db->sym_name, &source_kind, &source_path);
		}
		if (!addr) {
			addr = dlsym(RTLD_DEFAULT, lookup);
			if (addr) {
				source_kind = "RTLD_DEFAULT";
				source_path = "RTLD_DEFAULT";
			}
		}

		if (addr) {
			uintptr_t result = (uintptr_t)addr + db->addend;
			if (is_ctor_or_dtor(db->sym_name) && !ctor_has_stack_params(db->sym_name))
				result = wrap_ctor_for_apple_abi(result);

			/* mprotect GOT page (permissions may have been restored) */
			uintptr_t page = db->got_slot & ~(uintptr_t)(page_size - 1);
			mprotect((void*)page, page_size, PROT_READ | PROT_WRITE);
			*(uint64_t*)db->got_slot = result;
			trace_target_binding("deferred-complete", db->sym_name, lookup,
			                     db->lib_ordinal, de, db->got_slot, result,
			                     source_kind, source_path);
			resolved++;
		} else if (!db->weak) {
			trace_target_binding("deferred-complete-fail", db->sym_name, lookup,
			                     db->lib_ordinal, de, db->got_slot, 0,
			                     "stub/fail", de->so_path);
			if (failed < 10)
				fprintf(stderr, "resolver: deferred symbol '%s' not found in '%s'\n",
						lookup, de->so_path);
			failed++;
		}
	}

	fprintf(stderr, "resolver: deferred complete — %d resolved, %d failed\n",
			resolved, failed);
}

/* ---- Resolve a single bind entry ---- */

static uintptr_t resolve_import(struct resolver_state* rs,
                                uint32_t ordinal,
                                const struct dyld_chained_fixups_header* header,
                                const char* chain_data,
                                int64_t addend,
                                uintptr_t slot_addr)
{
	if (ordinal >= header->imports_count) {
		fprintf(stderr, "resolver: bind ordinal %u out of range (max %u)\n",
				ordinal, header->imports_count);
		return 0;
	}

	/* Get the import entry */
	const char* imports_base = chain_data + header->imports_offset;
	const char* symbols_base = chain_data + header->symbols_offset;

	int lib_ordinal;
	int weak;
	uint32_t name_offset;

	switch (header->imports_format) {
	case DYLD_CHAINED_IMPORT: {
		const struct dyld_chained_import* imp =
			&((const struct dyld_chained_import*)imports_base)[ordinal];
		lib_ordinal = (int8_t)imp->lib_ordinal; /* sign-extend 8-bit */
		weak = imp->weak_import;
		name_offset = imp->name_offset;
		break;
	}
	case DYLD_CHAINED_IMPORT_ADDEND: {
		const struct dyld_chained_import_addend* imp =
			&((const struct dyld_chained_import_addend*)imports_base)[ordinal];
		lib_ordinal = (int8_t)imp->lib_ordinal;
		weak = imp->weak_import;
		name_offset = imp->name_offset;
		addend += imp->addend;
		break;
	}
	case DYLD_CHAINED_IMPORT_ADDEND64: {
		const struct dyld_chained_import_addend64* imp =
			&((const struct dyld_chained_import_addend64*)imports_base)[ordinal];
		lib_ordinal = (int16_t)imp->lib_ordinal;
		weak = imp->weak_import;
		name_offset = imp->name_offset;
		addend += imp->addend;
		break;
	}
	default:
		fprintf(stderr, "resolver: unsupported import format %u\n", header->imports_format);
		return 0;
	}

	const char* sym_name = symbols_base + name_offset;
	const struct dylib_entry* trace_de = NULL;

	/* Strip leading underscore (Mach-O convention) */
	const char* lookup_name = sym_name;
	if (lookup_name[0] == '_')
		lookup_name++;

	/* Strip Apple $ suffixes ($DARWIN_EXTSN, $UNIX2003, $NOCANCEL).
	 * On Linux, glibc provides the modern behavior by default. */
	char dollar_stripped[256];
	{
		const char* dollar = strchr(lookup_name, '$');
		if (dollar) {
			size_t len = dollar - lookup_name;
			if (len < sizeof(dollar_stripped)) {
				memcpy(dollar_stripped, lookup_name, len);
				dollar_stripped[len] = '\0';
				lookup_name = dollar_stripped;
			}
		}
	}

	if (strstr(sym_name, "registr") && strstr(sym_name, "s_instance") && !strstr(sym_name, "ZGV"))
		fprintf(stderr, "resolver: TRACE s_instance: ordinal=%u lib_ordinal=%d weak=%d name='%s'\n",
				ordinal, lib_ordinal, weak, sym_name);

	/* Hook LuaJIT functions for profiler injection.
	 * The game (via sol2) opens individual Lua libraries instead of calling
	 * luaL_openlibs, so we hook luaopen_jit (last lib opened) to inject the
	 * profiler, and lua_close to flush it. */
	if (strcmp(sym_name, "_luaopen_jit") == 0) {
		rs->binds_resolved++;
		return (uintptr_t)wrapped_luaopen_jit;
	}
	if (strcmp(sym_name, "_lua_close") == 0) {
		rs->binds_resolved++;
		return (uintptr_t)wrapped_lua_close;
	}
	if (strcmp(sym_name, "_lua_pcall") == 0) {
		rs->binds_resolved++;
		return (uintptr_t)wrapped_lua_pcall;
	}

	/* Special ordinals */
	if (lib_ordinal == -1) {
		/* BIND_SPECIAL_DYLIB_MAIN_EXECUTABLE — look up in our own symbol table */
		uintptr_t addr = lookup_macho_symbol(rs, sym_name);
		if (addr) {
			uintptr_t result = addr + addend;
			trace_target_binding("chained-main-executable", sym_name, lookup_name,
			                     lib_ordinal, NULL, slot_addr, result,
			                     "main executable", "main executable");
			rs->binds_resolved++;
			return result;
		}
		log_failed_bind(rs, "chained-main-executable", sym_name, lookup_name,
		                lib_ordinal, weak, NULL);
		rs->binds_failed++;
		goto alloc_stub_slot;
	}
	if (lib_ordinal == -2) {
		/* BIND_SPECIAL_DYLIB_FLAT_LOOKUP — search all */
		const char* source_kind = NULL;
		const char* source_path = NULL;
		void* addr = lookup_flat_symbol(rs, lookup_name, &source_kind, &source_path);
		if (addr) {
			uintptr_t result = (uintptr_t)addr + addend;
			if (is_ctor_or_dtor(sym_name) && !ctor_has_stack_params(sym_name))
				result = wrap_ctor_for_apple_abi(result);
			result = wrap_fixed_stack_arg_symbol(lookup_name, result, NULL);
			const struct variadic_info* vi = lookup_variadic(lookup_name);
			if (vi) {
				void* v_func = dlsym(RTLD_DEFAULT, vi->v_name);
				if (v_func) {
					uintptr_t thunk = create_variadic_thunk((uintptr_t)v_func, vi);
					if (thunk) result = thunk;
				}
			}
			trace_target_binding("chained-flat-lookup", sym_name, lookup_name,
			                     lib_ordinal, NULL, slot_addr, result,
			                     source_kind, source_path);
			rs->binds_resolved++;
			return result;
		}
		if (!weak) {
			log_failed_bind(rs, "chained-flat-lookup", sym_name, lookup_name,
			                lib_ordinal, weak, NULL);
			rs->binds_failed++;
		}
		goto alloc_stub_slot;
	}
	if (lib_ordinal == -3) {
		/* BIND_SPECIAL_DYLIB_WEAK_LOOKUP — search Mach-O binary first, then .so files */
		uintptr_t macho_addr = lookup_macho_symbol(rs, sym_name);
		if (macho_addr) {
			uintptr_t result = macho_addr + addend;
			trace_target_binding("chained-weak-lookup", sym_name, lookup_name,
			                     lib_ordinal, NULL, slot_addr, result,
			                     "main executable", "main executable");
			rs->binds_resolved++;
			return result;
		}
		void* addr = dlsym(RTLD_DEFAULT, lookup_name);
		if (addr) {
			uintptr_t result = (uintptr_t)addr + addend;
			if (is_ctor_or_dtor(sym_name) && !ctor_has_stack_params(sym_name))
				result = wrap_ctor_for_apple_abi(result);
			result = wrap_fixed_stack_arg_symbol(lookup_name, result, NULL);
			const struct variadic_info* vi = lookup_variadic(lookup_name);
			if (vi) {
				void* v_func = dlsym(RTLD_DEFAULT, vi->v_name);
				if (v_func) {
					uintptr_t thunk = create_variadic_thunk((uintptr_t)v_func, vi);
					if (thunk) result = thunk;
				}
			}
			rs->binds_resolved++;
			if (result == 0)
				fprintf(stderr, "resolver: WARNING: weak sym '%s' resolved to %p + addend %ld = 0!\n",
						sym_name, addr, (long)addend);
			trace_target_binding("chained-weak-lookup", sym_name, lookup_name,
			                     lib_ordinal, NULL, slot_addr, result,
			                     "RTLD_DEFAULT", "RTLD_DEFAULT");
			return result;
		}
		/* For unresolved weak data references (guard variables, etc.),
		 * provide a unique zero-initialized slot for each bind so that
		 * writes to one don't corrupt another (e.g., atomic counter vs mutex). */
		static uint8_t* weak_pool = NULL;
		static size_t weak_pool_used = 0;
		static size_t weak_pool_capacity = 0;
		if (!weak_pool) {
			weak_pool_capacity = 64 * 4096; /* 256KB — room for many weak slots */
			weak_pool = mmap(NULL, weak_pool_capacity, PROT_READ | PROT_WRITE,
			                 MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
			if (weak_pool == MAP_FAILED) weak_pool = NULL;
		}
		if (weak_pool) {
			/* Allocate 128 bytes per weak symbol (enough for any pthread type) */
			size_t slot_size = 128;
			if (weak_pool_used + slot_size <= weak_pool_capacity) {
				uintptr_t slot = (uintptr_t)(weak_pool + weak_pool_used);
				weak_pool_used += slot_size;
				rs->binds_stubbed++;
				if (strstr(sym_name, "s_instance"))
					fprintf(stderr, "resolver: WEAK POOL gave %s -> %p\n", sym_name, (void*)slot);
				trace_target_binding("chained-weak-lookup", sym_name, lookup_name,
				                     lib_ordinal, NULL, slot_addr, slot,
				                     "stub/fail", "weak stub pool");
				return slot;
			}
		}
		rs->binds_stubbed++;
		goto alloc_stub_slot;
	}

	/* Normal library ordinal (1-based) */
	if (lib_ordinal < 1 || lib_ordinal > rs->ndylibs) {
		fprintf(stderr, "resolver: bind ordinal %u references lib_ordinal %d (out of range)\n",
				ordinal, lib_ordinal);
		log_failed_bind(rs, "chained-out-of-range", sym_name, lookup_name,
		                lib_ordinal, weak, NULL);
		rs->binds_failed++;
		goto alloc_stub_slot;
	}

	struct dylib_entry* de = &rs->dylibs[lib_ordinal - 1];
	trace_de = de;
	uintptr_t non_gui_data = resolve_non_gui_framework_data(de, sym_name);
	if (non_gui_data) {
		uintptr_t result = non_gui_data + addend;
		fprintf(stderr, "resolver: non-GUI data stub: %s from %s\n",
		        sym_name, de->name);
		trace_target_binding("chained-non-gui-data", sym_name, lookup_name,
		                     lib_ordinal, de, slot_addr, result,
		                     "stub/fail", "non-GUI data stub");
		rs->binds_resolved++;
		return result;
	}

	switch (de->action) {
	case DYLIB_MAP: {
		void* addr = dlsym(de->handle, lookup_name);
		/* Fallback: try macOS↔Linux C++ mangling variants */
		if (!addr && strncmp(lookup_name, "_ZN", 3) == 0)
			addr = try_mangling_variants(de->handle, lookup_name);
		if (addr) {
			const char* cxx_source_kind = NULL;
			const char* cxx_source_path = NULL;
			uintptr_t cxx_override = resolve_main_cxx_operator_override(
			    rs, sym_name, addr, addend, &cxx_source_kind, &cxx_source_path);
			if (cxx_override) {
				trace_target_binding("chained-map-cxx-main-override",
				                     sym_name, lookup_name, lib_ordinal, de,
				                     slot_addr, cxx_override,
				                     cxx_source_kind, cxx_source_path);
				rs->binds_resolved++;
				return cxx_override;
			}
			uintptr_t result = (uintptr_t)addr + addend;
			/* Wrap C++ ctors/dtors for Apple ARM64 ABI compatibility:
			 * Apple ABI returns `this` in x0, Linux ABI returns void */
			if (is_ctor_or_dtor(sym_name)) {
				extern int machgate_verbose;
				if (ctor_has_stack_params(sym_name)) {
					if (machgate_verbose)
						fprintf(stderr, "resolver: ctor/dtor SKIP adapter (stack params): %s\n", sym_name);
				} else {
					if (machgate_verbose)
						fprintf(stderr, "resolver: ctor/dtor ABI wrap: %s\n", sym_name);
					result = wrap_ctor_for_apple_abi(result);
				}
			}
			result = wrap_fixed_stack_arg_symbol(lookup_name, result, de->handle);
			const struct variadic_info* vi = lookup_variadic(lookup_name);
			if (vi) {
				void* v_func = dlsym(RTLD_DEFAULT, vi->v_name);
				if (v_func) {
					uintptr_t thunk = create_variadic_thunk((uintptr_t)v_func, vi);
					if (thunk) result = thunk;
				}
			}
			/* Hook SDL GL functions for deferred library loading */
			if (g_deferred.active && !g_deferred.resolved) {
				if (strcmp(lookup_name, "SDL_GL_CreateContext") == 0) {
					g_real_SDL_GL_CreateContext = (void* (*)(void*))result;
					result = (uintptr_t)wrapped_SDL_GL_CreateContext;
				} else if (strcmp(lookup_name, "SDL_GL_SetAttribute") == 0) {
					g_real_SDL_GL_SetAttribute = (int (*)(int, int))result;
					result = (uintptr_t)wrapped_SDL_GL_SetAttribute;
				}
			}
			trace_target_binding("chained-map", sym_name, lookup_name,
			                     lib_ordinal, de, slot_addr, result,
			                     "mapped dylib handle", de->so_path);
			rs->binds_resolved++;
			return result;
		}
		const char* cxx_source_kind = NULL;
		const char* cxx_source_path = NULL;
		uintptr_t cxx_override = resolve_main_cxx_operator_override(
		    rs, sym_name, NULL, addend, &cxx_source_kind, &cxx_source_path);
		if (cxx_override) {
			trace_target_binding("chained-map-cxx-main-override",
			                     sym_name, lookup_name, lib_ordinal, de,
			                     slot_addr, cxx_override,
			                     cxx_source_kind, cxx_source_path);
			rs->binds_resolved++;
			return cxx_override;
		}
		uintptr_t cxx_operator = resolve_cxx_operator_fallback(
		    sym_name, &cxx_source_kind, &cxx_source_path);
		if (cxx_operator) {
			uintptr_t result = cxx_operator + addend;
			trace_target_binding("chained-map-cxx-fallback", sym_name, lookup_name,
			                     lib_ordinal, de, slot_addr, result,
			                     cxx_source_kind, cxx_source_path);
			rs->binds_resolved++;
			return result;
		}
		/* Fallback: try RTLD_DEFAULT for symbols provided by
		 * the compiler runtime (libgcc_s: __clear_cache, _Unwind_*,
		 * __register_frame, etc.) or other loaded libraries */
		addr = dlsym(RTLD_DEFAULT, lookup_name);
		if (addr) {
			uintptr_t result = (uintptr_t)addr + addend;
			trace_target_binding("chained-map", sym_name, lookup_name,
			                     lib_ordinal, de, slot_addr, result,
			                     "RTLD_DEFAULT", "RTLD_DEFAULT");
			rs->binds_resolved++;
			return result;
		}
		if (!weak) {
			log_failed_bind(rs, "chained-map", sym_name, lookup_name,
			                lib_ordinal, weak, de);
			rs->binds_failed++;
		} else {
			rs->binds_stubbed++;
		}
		goto alloc_stub_slot;
	}
	case DYLIB_MACHO: {
		/* Look up symbol in loaded Mach-O dylib's LC_SYMTAB.
		 * Mach-O symbols have a leading underscore, but lookup_name
		 * has already been stripped. Use the original sym_name. */
		uintptr_t addr = dylib_loader_lookup(de->macho_info, sym_name);
		/* Fallback: try macOS↔Linux C++ mangling variants */
		if (!addr && strstr(sym_name, "_ZN")) {
			/* For Mach-O→Mach-O, mangling should match, but try anyway */
			char* variant = strdup(sym_name);
			if (variant) {
				for (char* c = variant; *c; c++) {
					if (*c == 'y') { *c = 'm'; break; }
					else if (*c == 'm') { *c = 'y'; break; }
				}
				addr = dylib_loader_lookup(de->macho_info, variant);
				free(variant);
			}
		}
		if (addr) {
			/* NO ctor/dtor ABI adapter needed — both sides are Apple ABI.
			 * The caller (Mach-O) expects ctor to return this in x0,
			 * and the callee (Mach-O dylib) does return this in x0. */
			uintptr_t result = addr + addend;
			trace_target_binding("chained-macho", sym_name, lookup_name,
			                     lib_ordinal, de, slot_addr, result,
			                     "Mach-O dylib", de->so_path);
			rs->binds_resolved++;
			return result;
		}
		if (!weak) {
			log_failed_bind(rs, "chained-macho", sym_name, lookup_name,
			                lib_ordinal, weak, de);
			rs->binds_failed++;
		} else {
			rs->binds_stubbed++;
		}
		goto alloc_stub_slot;
	}
	case DYLIB_STUB:
		rs->binds_stubbed++;
		goto alloc_stub_slot;
	case DYLIB_SKIP:
		if (!weak) {
			log_failed_bind(rs, "chained-skip", sym_name, lookup_name,
			                lib_ordinal, weak, de);
			rs->binds_failed++;
		} else {
			rs->binds_stubbed++;
		}
		goto alloc_stub_slot;
	case DYLIB_DEFERRED:
		/* Should not reach here — deferred binds are handled at the
		 * walk_chain/do_bind_one level before calling resolve_import.
		 * Fall through to stub as a safety net. */
		rs->binds_stubbed++;
		goto alloc_stub_slot;
	}

alloc_stub_slot:
	{
		uintptr_t result = alloc_return0_stub();
		trace_target_binding("chained-stub", sym_name, lookup_name,
		                     lib_ordinal, trace_de, slot_addr, result,
		                     "stub/fail", trace_de ? trace_de->so_path : "stub/fail");
		return result;
	}
}

/* ---- Walk a single fixup chain ---- */

static int walk_chain(struct resolver_state* rs, uint64_t* chain_start, uint16_t pointer_format,
                      const struct dyld_chained_fixups_header* header, const char* chain_data)
{
	uint64_t* loc = chain_start;
	bool is_offset = (pointer_format == DYLD_CHAINED_PTR_64_OFFSET);

	for (;;) {
		uint64_t raw = *loc;
		uint32_t next = FIXUP_PTR_NEXT(raw);

		if (FIXUP_PTR_BIND(raw)) {
			/* Bind: resolve symbol from a dylib */
			uint32_t ordinal = FIXUP_BIND_ORDINAL(raw);
			int64_t addend = (int64_t)(int8_t)FIXUP_BIND_ADDEND(raw);

			uintptr_t resolved = resolve_import(rs, ordinal, header, chain_data, addend,
			                                    (uintptr_t)loc);
			if (resolved == 0 &&
			    getenv_compat("MACHGATE_VERBOSE_BINDS",
			                  "MACHISMO_VERBOSE_BINDS")) {
				/* Log unresolved binds for debugging */
				const char* symbols_base = chain_data + header->symbols_offset;
				const char* imports_base = chain_data + header->imports_offset;
				const struct dyld_chained_import* imp =
					&((const struct dyld_chained_import*)imports_base)[ordinal];
				const char* sym_name = &symbols_base[imp->name_offset];
				fprintf(stderr, "resolver: NULL bind at %p: %s (ordinal %u, lib_ordinal %d)\n",
						(void*)loc, sym_name, ordinal, (int)(int8_t)imp->lib_ordinal);
			}
			*loc = resolved;
		} else {
			/* Rebase: fix up an internal pointer for ASLR slide */
			uint64_t target = FIXUP_REBASE_TARGET(raw);
			uint8_t high8 = FIXUP_REBASE_HIGH8(raw);

			uintptr_t value;
			if (is_offset) {
				/* PTR_64_OFFSET: target is offset from mach header */
				value = rs->mh_addr + target;
			} else {
				/* PTR_64: target is vmaddr, apply slide */
				value = target + rs->slide;
			}

			/* Apply high8 */
			value |= ((uintptr_t)high8 << 56);

			*loc = value;
			rs->rebases_applied++;
		}

		if (next == 0)
			break;

		/* Stride is 4 bytes for DYLD_CHAINED_PTR_64 and PTR_64_OFFSET */
		loc = (uint64_t*)((uint8_t*)loc + (next * 4));
	}

	return 0;
}

/* ---- LC_DYLD_INFO_ONLY: resolve a single bind by lib_ordinal + symbol name ----
 *
 * Reuses the same resolution logic as resolve_import() but takes the already-
 * extracted bind parameters directly (no chained fixup header dependency).
 */
static uintptr_t resolve_bind_by_name(struct resolver_state* rs,
                                      int lib_ordinal, const char* sym_name,
                                      int weak, int64_t addend,
                                      uintptr_t slot_addr)
{
	/* Strip leading underscore (Mach-O convention) */
	const char* lookup_name = sym_name;
	if (lookup_name[0] == '_')
		lookup_name++;
	const struct dylib_entry* trace_de = NULL;

	/* Strip Apple $ suffixes ($DARWIN_EXTSN, $UNIX2003, $NOCANCEL) */
	char dollar_stripped[256];
	{
		const char* dollar = strchr(lookup_name, '$');
		if (dollar) {
			size_t len = dollar - lookup_name;
			if (len < sizeof(dollar_stripped)) {
				memcpy(dollar_stripped, lookup_name, len);
				dollar_stripped[len] = '\0';
				lookup_name = dollar_stripped;
			}
		}
	}

	/* Hook LuaJIT functions for profiler injection */
	if (strcmp(sym_name, "_luaopen_jit") == 0) {
		rs->binds_resolved++;
		return (uintptr_t)wrapped_luaopen_jit;
	}
	if (strcmp(sym_name, "_lua_close") == 0) {
		rs->binds_resolved++;
		return (uintptr_t)wrapped_lua_close;
	}
	if (strcmp(sym_name, "_lua_pcall") == 0) {
		rs->binds_resolved++;
		return (uintptr_t)wrapped_lua_pcall;
	}

	/* Special ordinals */
	if (lib_ordinal == 0 || lib_ordinal == -1) {
		/* 0 = BIND_SPECIAL_DYLIB_SELF (this image)
		 * -1 = BIND_SPECIAL_DYLIB_MAIN_EXECUTABLE */
		uintptr_t addr = lookup_macho_symbol(rs, sym_name);
		if (addr) {
			uintptr_t result = addr + addend;
			trace_target_binding("dyld-info-self", sym_name, lookup_name,
			                     lib_ordinal, NULL, slot_addr, result,
			                     "main executable", "main executable");
			rs->binds_resolved++;
			return result;
		}
		/* Fallback: try RTLD_DEFAULT for C++ RTTI and other symbols
		 * that may come from linked shared libraries */
		void* fallback = dlsym(RTLD_DEFAULT, lookup_name);
		if (fallback) {
			uintptr_t result = (uintptr_t)fallback + addend;
			trace_target_binding("dyld-info-self", sym_name, lookup_name,
			                     lib_ordinal, NULL, slot_addr, result,
			                     "RTLD_DEFAULT", "RTLD_DEFAULT");
			rs->binds_resolved++;
			return result;
		}
		if (!weak) {
			log_failed_bind(rs, "dyld-info-self", sym_name, lookup_name,
			                lib_ordinal, weak, NULL);
			rs->binds_failed++;
		} else {
			rs->binds_stubbed++;
		}
		goto alloc_stub;
	}
	if (lib_ordinal == -2) {
		/* BIND_SPECIAL_DYLIB_FLAT_LOOKUP */
		const char* source_kind = NULL;
		const char* source_path = NULL;
		void* addr = lookup_flat_symbol(rs, lookup_name, &source_kind, &source_path);
		if (addr) {
			uintptr_t result = (uintptr_t)addr + addend;
			if (is_ctor_or_dtor(sym_name) && !ctor_has_stack_params(sym_name))
				result = wrap_ctor_for_apple_abi(result);
			result = wrap_fixed_stack_arg_symbol(lookup_name, result, NULL);
			const struct variadic_info* vi = lookup_variadic(lookup_name);
			if (vi) {
				void* v_func = dlsym(RTLD_DEFAULT, vi->v_name);
				if (v_func) {
					uintptr_t thunk = create_variadic_thunk((uintptr_t)v_func, vi);
					if (thunk) result = thunk;
				}
			}
			trace_target_binding("dyld-info-flat-lookup", sym_name, lookup_name,
			                     lib_ordinal, NULL, slot_addr, result,
			                     source_kind, source_path);
			rs->binds_resolved++;
			return result;
		}
		if (!weak) {
			log_failed_bind(rs, "dyld-info-flat-lookup", sym_name, lookup_name,
			                lib_ordinal, weak, NULL);
			rs->binds_failed++;
		}
		goto alloc_stub;
	}
	if (lib_ordinal == -3) {
		/* BIND_SPECIAL_DYLIB_WEAK_LOOKUP */
		uintptr_t macho_addr = lookup_macho_symbol(rs, sym_name);
		if (macho_addr) {
			uintptr_t result = macho_addr + addend;
			trace_target_binding("dyld-info-weak-lookup", sym_name, lookup_name,
			                     lib_ordinal, NULL, slot_addr, result,
			                     "main executable", "main executable");
			rs->binds_resolved++;
			return result;
		}
		void* addr = dlsym(RTLD_DEFAULT, lookup_name);
		if (addr) {
			uintptr_t result = (uintptr_t)addr + addend;
			if (is_ctor_or_dtor(sym_name) && !ctor_has_stack_params(sym_name))
				result = wrap_ctor_for_apple_abi(result);
			result = wrap_fixed_stack_arg_symbol(lookup_name, result, NULL);
			trace_target_binding("dyld-info-weak-lookup", sym_name, lookup_name,
			                     lib_ordinal, NULL, slot_addr, result,
			                     "RTLD_DEFAULT", "RTLD_DEFAULT");
			rs->binds_resolved++;
			return result;
		}
		rs->binds_stubbed++;
		goto alloc_stub;
	}

	/* Normal library ordinal (1-based) */
	if (lib_ordinal < 1 || lib_ordinal > rs->ndylibs) {
		extern int machgate_verbose;
		if (machgate_verbose)
			fprintf(stderr, "resolver: dyld_info bind '%s' lib_ordinal %d out of range\n",
					sym_name, lib_ordinal);
		log_failed_bind(rs, "dyld-info-out-of-range", sym_name, lookup_name,
		                lib_ordinal, weak, NULL);
		rs->binds_failed++;
		goto alloc_stub;
	}

	struct dylib_entry* de = &rs->dylibs[lib_ordinal - 1];
	trace_de = de;
	uintptr_t non_gui_data = resolve_non_gui_framework_data(de, sym_name);
	if (non_gui_data) {
		uintptr_t result = non_gui_data + addend;
		fprintf(stderr, "resolver: non-GUI data stub: %s from %s\n",
		        sym_name, de->name);
		trace_target_binding("dyld-info-non-gui-data", sym_name, lookup_name,
		                     lib_ordinal, de, slot_addr, result,
		                     "stub/fail", "non-GUI data stub");
		rs->binds_resolved++;
		return result;
	}

	switch (de->action) {
	case DYLIB_MAP: {
		void* addr = dlsym(de->handle, lookup_name);
		if (!addr && strncmp(lookup_name, "_ZN", 3) == 0)
			addr = try_mangling_variants(de->handle, lookup_name);
		if (addr) {
			const char* cxx_source_kind = NULL;
			const char* cxx_source_path = NULL;
			uintptr_t cxx_override = resolve_main_cxx_operator_override(
			    rs, sym_name, addr, addend, &cxx_source_kind, &cxx_source_path);
			if (cxx_override) {
				trace_target_binding("dyld-info-map-cxx-main-override",
				                     sym_name, lookup_name, lib_ordinal, de,
				                     slot_addr, cxx_override,
				                     cxx_source_kind, cxx_source_path);
				rs->binds_resolved++;
				return cxx_override;
			}
			uintptr_t result = (uintptr_t)addr + addend;
			if (is_ctor_or_dtor(sym_name)) {
				if (!ctor_has_stack_params(sym_name))
					result = wrap_ctor_for_apple_abi(result);
			}
			result = wrap_fixed_stack_arg_symbol(lookup_name, result, de->handle);
			const struct variadic_info* vi = lookup_variadic(lookup_name);
			if (vi) {
				void* v_func = dlsym(RTLD_DEFAULT, vi->v_name);
				if (v_func) {
					uintptr_t thunk = create_variadic_thunk((uintptr_t)v_func, vi);
					if (thunk) result = thunk;
				}
			}
			/* Hook SDL GL functions for deferred library loading */
			if (g_deferred.active && !g_deferred.resolved) {
				if (strcmp(lookup_name, "SDL_GL_CreateContext") == 0) {
					g_real_SDL_GL_CreateContext = (void* (*)(void*))result;
					result = (uintptr_t)wrapped_SDL_GL_CreateContext;
				} else if (strcmp(lookup_name, "SDL_GL_SetAttribute") == 0) {
					g_real_SDL_GL_SetAttribute = (int (*)(int, int))result;
					result = (uintptr_t)wrapped_SDL_GL_SetAttribute;
				}
			}
			trace_target_binding("dyld-info-map", sym_name, lookup_name,
			                     lib_ordinal, de, slot_addr, result,
			                     "mapped dylib handle", de->so_path);
			rs->binds_resolved++;
			return result;
		}
		const char* cxx_source_kind = NULL;
		const char* cxx_source_path = NULL;
		uintptr_t cxx_override = resolve_main_cxx_operator_override(
		    rs, sym_name, NULL, addend, &cxx_source_kind, &cxx_source_path);
		if (cxx_override) {
			trace_target_binding("dyld-info-map-cxx-main-override",
			                     sym_name, lookup_name, lib_ordinal, de,
			                     slot_addr, cxx_override,
			                     cxx_source_kind, cxx_source_path);
			rs->binds_resolved++;
			return cxx_override;
		}
		uintptr_t cxx_operator = resolve_cxx_operator_fallback(
		    sym_name, &cxx_source_kind, &cxx_source_path);
		if (cxx_operator) {
			uintptr_t result = cxx_operator + addend;
			trace_target_binding("dyld-info-map-cxx-fallback", sym_name, lookup_name,
			                     lib_ordinal, de, slot_addr, result,
			                     cxx_source_kind, cxx_source_path);
			rs->binds_resolved++;
			return result;
		}
		/* Fallback: try RTLD_DEFAULT for compiler runtime symbols */
		addr = dlsym(RTLD_DEFAULT, lookup_name);
		if (addr) {
			uintptr_t result = (uintptr_t)addr + addend;
			trace_target_binding("dyld-info-map", sym_name, lookup_name,
			                     lib_ordinal, de, slot_addr, result,
			                     "RTLD_DEFAULT", "RTLD_DEFAULT");
			rs->binds_resolved++;
			return result;
		}
		if (!weak) {
			log_failed_bind(rs, "dyld-info-map", sym_name, lookup_name,
			                lib_ordinal, weak, de);
			rs->binds_failed++;
		} else {
			rs->binds_stubbed++;
		}
		goto alloc_stub;
	}
	case DYLIB_MACHO: {
		uintptr_t addr = dylib_loader_lookup(de->macho_info, sym_name);
		if (!addr && strstr(sym_name, "_ZN")) {
			char* variant = strdup(sym_name);
			if (variant) {
				for (char* c = variant; *c; c++) {
					if (*c == 'y') { *c = 'm'; break; }
					else if (*c == 'm') { *c = 'y'; break; }
				}
				addr = dylib_loader_lookup(de->macho_info, variant);
				free(variant);
			}
		}
		if (addr) {
			uintptr_t result = addr + addend;
			trace_target_binding("dyld-info-macho", sym_name, lookup_name,
			                     lib_ordinal, de, slot_addr, result,
			                     "Mach-O dylib", de->so_path);
			rs->binds_resolved++;
			return result;
		}
		if (!weak) {
			log_failed_bind(rs, "dyld-info-macho", sym_name, lookup_name,
			                lib_ordinal, weak, de);
			rs->binds_failed++;
		} else {
			rs->binds_stubbed++;
		}
		goto alloc_stub;
	}
	case DYLIB_STUB:
		rs->binds_stubbed++;
		goto alloc_stub;
	case DYLIB_SKIP:
		if (!weak) {
			log_failed_bind(rs, "dyld-info-skip", sym_name, lookup_name,
			                lib_ordinal, weak, de);
			rs->binds_failed++;
		} else {
			rs->binds_stubbed++;
		}
		goto alloc_stub;
	case DYLIB_DEFERRED:
		/* Handled by do_bind_one before calling this function.
		 * If we reach here, fall through to stub as safety net. */
		rs->binds_stubbed++;
		goto alloc_stub;
	}

alloc_stub:
	{
		uintptr_t result = alloc_return0_stub();
		trace_target_binding("dyld-info-stub", sym_name, lookup_name,
		                     lib_ordinal, trace_de, slot_addr, result,
		                     "stub/fail", trace_de ? trace_de->so_path : "stub/fail");
		return result;
	}
}

/* ---- Bind helper with deferred library support ---- */

static void do_bind_one(struct resolver_state* rs, int seg_index,
                        uint64_t seg_offset, int lib_ordinal,
                        const char* sym_name, int weak, int64_t addend)
{
	if (seg_index >= rs->nsegments) return;
	uintptr_t addr = rs->segments[seg_index].vmaddr + rs->slide + seg_offset;

	/* Check for deferred library — record bind for later resolution */
	if (lib_ordinal >= 1 && lib_ordinal <= rs->ndylibs &&
	    rs->dylibs[lib_ordinal - 1].action == DYLIB_DEFERRED) {
		if (g_deferred.count < MAX_DEFERRED_BINDS) {
			struct deferred_bind* db = &g_deferred.binds[g_deferred.count++];
			db->got_slot = addr;
			db->sym_name = sym_name;
			db->lib_ordinal = lib_ordinal;
			db->weak = weak;
			db->addend = addend;
		}
		const char* lookup_name = sym_name;
		if (lookup_name[0] == '_')
			lookup_name++;
		char stripped[256];
		const char* dollar = strchr(lookup_name, '$');
		if (dollar && (size_t)(dollar - lookup_name) < sizeof(stripped)) {
			memcpy(stripped, lookup_name, dollar - lookup_name);
			stripped[dollar - lookup_name] = '\0';
			lookup_name = stripped;
		}
		uintptr_t stub = alloc_return0_stub();
		trace_target_binding("dyld-info-deferred", sym_name, lookup_name,
		                     lib_ordinal, &rs->dylibs[lib_ordinal - 1], addr, stub,
		                     "stub/fail", rs->dylibs[lib_ordinal - 1].so_path);
		*(uint64_t*)addr = stub;
		rs->binds_stubbed++;
		return;
	}

	uintptr_t resolved = resolve_bind_by_name(rs, lib_ordinal, sym_name, weak, addend, addr);
	*(uint64_t*)addr = resolved;
}

/* ---- LC_DYLD_INFO_ONLY: process rebase + bind opcode streams ---- */

static int process_dyld_info(struct resolver_state* rs)
{
	extern int machgate_verbose;

	/* Make writable segments temporarily writable for patching */
	for (int i = 0; i < rs->nsegments; i++) {
		struct seg_info* seg = &rs->segments[i];
		if (seg->vmsize == 0) continue;
		uintptr_t addr = seg->vmaddr + rs->slide;
		if (mprotect((void*)addr, seg->vmsize, PROT_READ | PROT_WRITE | PROT_EXEC) < 0) {
			/* Not all segments are writable (e.g., __TEXT) — only warn on DATA segments */
			if (seg->prot & VM_PROT_WRITE)
				fprintf(stderr, "resolver: mprotect writable failed for segment at 0x%lx: %s\n",
						addr, strerror(errno));
		}
	}

	/* ---- Process rebases ---- */
	if (rs->dyld_info_rebase_size > 0) {
		const uint8_t* p = (const uint8_t*)fileoff_to_mem(rs, rs->dyld_info_rebase_off);
		const uint8_t* end = p + rs->dyld_info_rebase_size;
		if (!p) {
			fprintf(stderr, "resolver: cannot map rebase data at offset 0x%x\n",
					rs->dyld_info_rebase_off);
			return -1;
		}

		int seg_index = 0;
		uint64_t seg_offset = 0;
		bool done = false;

		while (p < end && !done) {
			uint8_t byte = *p++;
			uint8_t opcode = byte & REBASE_OPCODE_MASK;
			uint8_t imm = byte & REBASE_IMMEDIATE_MASK;
			uint64_t uleb_val;

			switch (opcode) {
			case REBASE_OPCODE_DONE:
				done = true;
				break;
			case REBASE_OPCODE_SET_TYPE_IMM:
				/* type = imm; only REBASE_TYPE_POINTER matters for arm64 */
				break;
			case REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
				seg_index = imm;
				p += read_uleb128(p, end, &seg_offset);
				break;
			case REBASE_OPCODE_ADD_ADDR_ULEB:
				p += read_uleb128(p, end, &uleb_val);
				seg_offset += uleb_val;
				break;
			case REBASE_OPCODE_ADD_ADDR_IMM_SCALED:
				seg_offset += (uint64_t)imm * 8;
				break;
			case REBASE_OPCODE_DO_REBASE_IMM_TIMES:
				for (int i = 0; i < imm; i++) {
					if (seg_index < rs->nsegments) {
						uintptr_t addr = rs->segments[seg_index].vmaddr + rs->slide + seg_offset;
						uint64_t* loc = (uint64_t*)addr;
						*loc += rs->slide;
						rs->rebases_applied++;
					}
					seg_offset += 8;
				}
				break;
			case REBASE_OPCODE_DO_REBASE_ULEB_TIMES: {
				p += read_uleb128(p, end, &uleb_val);
				for (uint64_t i = 0; i < uleb_val; i++) {
					if (seg_index < rs->nsegments) {
						uintptr_t addr = rs->segments[seg_index].vmaddr + rs->slide + seg_offset;
						uint64_t* loc = (uint64_t*)addr;
						*loc += rs->slide;
						rs->rebases_applied++;
					}
					seg_offset += 8;
				}
				break;
			}
			case REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB:
				if (seg_index < rs->nsegments) {
					uintptr_t addr = rs->segments[seg_index].vmaddr + rs->slide + seg_offset;
					uint64_t* loc = (uint64_t*)addr;
					*loc += rs->slide;
					rs->rebases_applied++;
				}
				p += read_uleb128(p, end, &uleb_val);
				seg_offset += uleb_val + 8;
				break;
			case REBASE_OPCODE_DO_REBASE_ULEB_TIMES_SKIPPING_ULEB: {
				uint64_t count, skip;
				p += read_uleb128(p, end, &count);
				p += read_uleb128(p, end, &skip);
				for (uint64_t i = 0; i < count; i++) {
					if (seg_index < rs->nsegments) {
						uintptr_t addr = rs->segments[seg_index].vmaddr + rs->slide + seg_offset;
						uint64_t* loc = (uint64_t*)addr;
						*loc += rs->slide;
						rs->rebases_applied++;
					}
					seg_offset += skip + 8;
				}
				break;
			}
			default:
				fprintf(stderr, "resolver: unknown rebase opcode 0x%x\n", opcode);
				return -1;
			}
		}
	}

	/* ---- Process binds (regular, weak, lazy) ---- */
	struct {
		uint32_t off;
		uint32_t size;
		const char* name;
	} bind_passes[] = {
		{ rs->dyld_info_bind_off, rs->dyld_info_bind_size, "bind" },
		{ rs->dyld_info_weak_bind_off, rs->dyld_info_weak_bind_size, "weak_bind" },
		{ rs->dyld_info_lazy_bind_off, rs->dyld_info_lazy_bind_size, "lazy_bind" },
	};

	for (int pass = 0; pass < 3; pass++) {
		if (bind_passes[pass].size == 0)
			continue;

		const uint8_t* p = (const uint8_t*)fileoff_to_mem(rs, bind_passes[pass].off);
		const uint8_t* end = p + bind_passes[pass].size;
		if (!p) {
			fprintf(stderr, "resolver: cannot map %s data at offset 0x%x\n",
					bind_passes[pass].name, bind_passes[pass].off);
			return -1;
		}

		int seg_index = 0;
		uint64_t seg_offset = 0;
		int lib_ordinal = 0;
		const char* sym_name = "";
		int sym_flags = 0;
		int64_t addend = 0;
		bool done = false;

		while (p < end && !done) {
			uint8_t byte = *p++;
			uint8_t opcode = byte & BIND_OPCODE_MASK;
			uint8_t imm = byte & BIND_IMMEDIATE_MASK;
			uint64_t uleb_val;
			int64_t sleb_val;

			switch (opcode) {
			case BIND_OPCODE_DONE:
				/* For lazy binds, DONE separates entries; for others, it terminates */
				if (pass != 2) /* not lazy_bind */
					done = true;
				break;
			case BIND_OPCODE_SET_DYLIB_ORDINAL_IMM:
				lib_ordinal = imm;
				break;
			case BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB:
				p += read_uleb128(p, end, &uleb_val);
				lib_ordinal = (int)uleb_val;
				break;
			case BIND_OPCODE_SET_DYLIB_SPECIAL_IMM:
				if (imm == 0)
					lib_ordinal = 0;
				else
					lib_ordinal = (int8_t)(BIND_OPCODE_MASK | imm); /* sign-extend 4-bit */
				break;
			case BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM:
				sym_flags = imm;
				sym_name = (const char*)p;
				p += strlen(sym_name) + 1;
				break;
			case BIND_OPCODE_SET_TYPE_IMM:
				/* type = imm; only BIND_TYPE_POINTER for arm64 */
				break;
			case BIND_OPCODE_SET_ADDEND_SLEB:
				p += read_sleb128(p, end, &sleb_val);
				addend = sleb_val;
				break;
			case BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
				seg_index = imm;
				p += read_uleb128(p, end, &seg_offset);
				break;
			case BIND_OPCODE_ADD_ADDR_ULEB:
				p += read_uleb128(p, end, &uleb_val);
				seg_offset += uleb_val;
				break;
			case BIND_OPCODE_DO_BIND: {
				int weak = (sym_flags & BIND_SYMBOL_FLAGS_WEAK_IMPORT) != 0;
				do_bind_one(rs, seg_index, seg_offset, lib_ordinal, sym_name, weak, addend);
				seg_offset += 8;
				break;
			}
			case BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB: {
				int weak = (sym_flags & BIND_SYMBOL_FLAGS_WEAK_IMPORT) != 0;
				do_bind_one(rs, seg_index, seg_offset, lib_ordinal, sym_name, weak, addend);
				p += read_uleb128(p, end, &uleb_val);
				seg_offset += uleb_val + 8;
				break;
			}
			case BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED: {
				int weak = (sym_flags & BIND_SYMBOL_FLAGS_WEAK_IMPORT) != 0;
				do_bind_one(rs, seg_index, seg_offset, lib_ordinal, sym_name, weak, addend);
				seg_offset += ((uint64_t)imm * 8) + 8;
				break;
			}
			case BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB: {
				uint64_t count, skip;
				p += read_uleb128(p, end, &count);
				p += read_uleb128(p, end, &skip);
				int weak = (sym_flags & BIND_SYMBOL_FLAGS_WEAK_IMPORT) != 0;
				for (uint64_t i = 0; i < count; i++) {
					do_bind_one(rs, seg_index, seg_offset, lib_ordinal, sym_name, weak, addend);
					seg_offset += skip + 8;
				}
				break;
			}
			default:
				fprintf(stderr, "resolver: unknown bind opcode 0x%x in %s\n",
						opcode, bind_passes[pass].name);
				return -1;
			}
		}
	}

	/* Restore segment protections */
	for (int i = 0; i < rs->nsegments; i++) {
		struct seg_info* seg = &rs->segments[i];
		if (seg->vmsize == 0) continue;
		uintptr_t addr = seg->vmaddr + rs->slide;
		mprotect((void*)addr, seg->vmsize, seg->prot);
	}

	return 0;
}

/* ---- Close dlopen handles ---- */

static void close_dylibs(struct resolver_state* rs)
{
	/* Don't actually close — the game needs these to stay loaded */
	(void)rs;
}

/* ---- Utility: extract basename from dylib path ---- */

static const char* basename_from_path(const char* path)
{
	const char* slash = strrchr(path, '/');
	return slash ? slash + 1 : path;
}
