/*
 * INI-style config file parser for machgate.
 *
 * Format:
 *   [general]
 *   dylib_map = dylib_map.conf
 *   patches = patches/necrodancer.conf
 *
 *   [trampoline.sdl2]
 *   lib = libSDL2-2.0.so.0
 *   prefix = _SDL_
 *
 *   [trampoline.bgfx]
 *   lib = ./build-bgfx/libbgfx-shared.so
 *   prefix = _bgfx_
 *   ...
 *
 *   [dylib_patch.libfbxsdk.dylib]
 *   patches = patches/fbxsdk.conf
 */

#ifndef CONFIG_H
#define CONFIG_H

#define CONFIG_MAX_TRAMPOLINES 8
#define CONFIG_MAX_PREFIXES    8
#define CONFIG_MAX_DYLIB_PATCHES 8

typedef struct {
	char* name;                              /* section suffix, e.g. "sdl2" */
	char* lib;                               /* native .so path */
	char* prefixes[CONFIG_MAX_PREFIXES];     /* symbol prefix patterns */
	int num_prefixes;
	int init_wrapper;                        /* intercept bgfx init */
	char* renderer;                          /* force renderer type */
	char* override_lib;                      /* .so with exact-symbol replacements */
	int match_local;                         /* override pass also matches LOCAL (non-N_EXT) defined symbols */
} machgate_trampoline_config_t;

typedef struct {
	char* dylib;                             /* guest dylib basename to patch */
	char* patches;                           /* patch file applied to that dylib */
} machgate_dylib_patch_config_t;

typedef struct {
	char* dylib_map;
	char* patches;
	machgate_trampoline_config_t trampolines[CONFIG_MAX_TRAMPOLINES];
	int num_trampolines;
	machgate_dylib_patch_config_t dylib_patches[CONFIG_MAX_DYLIB_PATCHES];
	int num_dylib_patches;
} machgate_config_t;

/* Load config from file. Returns 0 on success, -1 if file not found (not an error). */
int config_load(const char* path, machgate_config_t* cfg);

/* Free all allocated strings in config. */
void config_free(machgate_config_t* cfg);

#endif /* CONFIG_H */
