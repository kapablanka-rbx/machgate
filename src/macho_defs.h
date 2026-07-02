/*
 * Minimal Mach-O definitions for standalone loader.
 * Replaces the full cctools mach-o/loader.h, mach-o/fat.h, mach/machine.h
 * without requiring their transitive dependencies.
 */
#ifndef _MACHO_DEFS_H_
#define _MACHO_DEFS_H_

#include <stdint.h>

/* --- mach/machine.h types --- */

typedef int      cpu_type_t;
typedef int      cpu_subtype_t;
typedef int      vm_prot_t;

#define CPU_ARCH_ABI64       0x01000000
#define CPU_ARCH_ABI64_32    0x02000000

#define CPU_TYPE_ANY         ((cpu_type_t) -1)
#define CPU_TYPE_X86         ((cpu_type_t) 7)
#define CPU_TYPE_I386        CPU_TYPE_X86
#define CPU_TYPE_X86_64      ((cpu_type_t)(CPU_TYPE_X86 | CPU_ARCH_ABI64))
#define CPU_TYPE_ARM         ((cpu_type_t) 12)
#define CPU_TYPE_ARM64       ((cpu_type_t)(CPU_TYPE_ARM | CPU_ARCH_ABI64))
#define CPU_TYPE_ARM64_32    ((cpu_type_t)(CPU_TYPE_ARM | CPU_ARCH_ABI64_32))

#define CPU_SUBTYPE_ARM64_ALL  ((cpu_subtype_t) 0)
#define CPU_SUBTYPE_ARM64_V8   ((cpu_subtype_t) 1)
#define CPU_SUBTYPE_ARM64E     ((cpu_subtype_t) 2)

/* --- VM protection --- */

#define VM_PROT_READ    0x01
#define VM_PROT_WRITE   0x02
#define VM_PROT_EXECUTE 0x04

/* --- Mach-O header magic --- */

#define MH_MAGIC      0xfeedface
#define MH_CIGAM      0xcefaedfe
#define MH_MAGIC_64   0xfeedfacf
#define MH_CIGAM_64   0xcffaedfe

/* --- Mach-O file types --- */

#define MH_OBJECT      0x1
#define MH_EXECUTE     0x2
#define MH_FVMLIB      0x3
#define MH_CORE        0x4
#define MH_PRELOAD     0x5
#define MH_DYLIB       0x6
#define MH_DYLINKER    0x7
#define MH_BUNDLE      0x8
#define MH_DSYM        0xa

/* --- Mach-O flags --- */

#define MH_NOUNDEFS     0x1
#define MH_DYLDLINK     0x4
#define MH_PIE          0x200000
#define MH_TWOLEVEL     0x80

/* --- Mach-O headers --- */

struct mach_header {
	uint32_t      magic;
	cpu_type_t    cputype;
	cpu_subtype_t cpusubtype;
	uint32_t      filetype;
	uint32_t      ncmds;
	uint32_t      sizeofcmds;
	uint32_t      flags;
};

struct mach_header_64 {
	uint32_t      magic;
	cpu_type_t    cputype;
	cpu_subtype_t cpusubtype;
	uint32_t      filetype;
	uint32_t      ncmds;
	uint32_t      sizeofcmds;
	uint32_t      flags;
	uint32_t      reserved;
};

/* --- Load commands --- */

#define LC_REQ_DYLD          0x80000000

#define LC_SEGMENT           0x1
#define LC_SYMTAB            0x2
#define LC_UNIXTHREAD        0x5
#define LC_DYSYMTAB          0xb
#define LC_LOAD_DYLIB        0xc
#define LC_ID_DYLIB          0xd
#define LC_LOAD_DYLINKER     0xe
#define LC_ID_DYLINKER       0xf
#define LC_ROUTINES          0x11
#define LC_SEGMENT_64        0x19
#define LC_ROUTINES_64       0x1a
#define LC_UUID              0x1b
#define LC_CODE_SIGNATURE    0x1d
#define LC_DYLD_INFO         0x22
#define LC_DYLD_INFO_ONLY    (0x22 | LC_REQ_DYLD)
#define LC_LOAD_WEAK_DYLIB   (0x18 | LC_REQ_DYLD)
#define LC_MAIN              (0x28 | LC_REQ_DYLD)
#define LC_SOURCE_VERSION    0x2A
#define LC_REEXPORT_DYLIB    (0x1f | LC_REQ_DYLD)

/* --- LC_DYLD_INFO / LC_DYLD_INFO_ONLY --- */

struct dyld_info_command {
	uint32_t cmd;		/* LC_DYLD_INFO or LC_DYLD_INFO_ONLY */
	uint32_t cmdsize;
	uint32_t rebase_off;	/* file offset to rebase opcodes */
	uint32_t rebase_size;
	uint32_t bind_off;	/* file offset to bind opcodes */
	uint32_t bind_size;
	uint32_t weak_bind_off;
	uint32_t weak_bind_size;
	uint32_t lazy_bind_off;
	uint32_t lazy_bind_size;
	uint32_t export_off;	/* file offset to export trie */
	uint32_t export_size;
};

struct load_command {
	uint32_t cmd;
	uint32_t cmdsize;
};

union lc_str {
	uint32_t offset;
};

/* --- Segments --- */

#define SEG_PAGEZERO   "__PAGEZERO"
#define SEG_TEXT       "__TEXT"
#define SEG_DATA       "__DATA"
#define SEG_LINKEDIT   "__LINKEDIT"

struct segment_command {
	uint32_t  cmd;
	uint32_t  cmdsize;
	char      segname[16];
	uint32_t  vmaddr;
	uint32_t  vmsize;
	uint32_t  fileoff;
	uint32_t  filesize;
	vm_prot_t maxprot;
	vm_prot_t initprot;
	uint32_t  nsects;
	uint32_t  flags;
};

struct segment_command_64 {
	uint32_t  cmd;
	uint32_t  cmdsize;
	char      segname[16];
	uint64_t  vmaddr;
	uint64_t  vmsize;
	uint64_t  fileoff;
	uint64_t  filesize;
	vm_prot_t maxprot;
	vm_prot_t initprot;
	uint32_t  nsects;
	uint32_t  flags;
};

struct section {
	char     sectname[16];
	char     segname[16];
	uint32_t addr;
	uint32_t size;
	uint32_t offset;
	uint32_t align;
	uint32_t reloff;
	uint32_t nreloc;
	uint32_t flags;
	uint32_t reserved1;
	uint32_t reserved2;
};

struct section_64 {
	char     sectname[16];
	char     segname[16];
	uint64_t addr;
	uint64_t size;
	uint32_t offset;
	uint32_t align;
	uint32_t reloff;
	uint32_t nreloc;
	uint32_t flags;
	uint32_t reserved1;
	uint32_t reserved2;
	uint32_t reserved3;
};

/* Section attribute flags (from section_64.flags) */
#define SECTION_TYPE                0x000000ff
#define S_NON_LAZY_SYMBOL_POINTERS  0x6
#define S_LAZY_SYMBOL_POINTERS      0x7
#define S_SYMBOL_STUBS              0x8
#define S_MOD_INIT_FUNC_POINTERS    0x9
#define S_INIT_FUNC_OFFSETS         0x16
#define S_ATTR_PURE_INSTRUCTIONS  0x80000000
#define S_ATTR_SOME_INSTRUCTIONS  0x00000400

struct routines_command {
	uint32_t cmd;
	uint32_t cmdsize;
	uint32_t init_address;
	uint32_t init_module;
	uint32_t reserved1;
	uint32_t reserved2;
	uint32_t reserved3;
	uint32_t reserved4;
	uint32_t reserved5;
	uint32_t reserved6;
};

struct routines_command_64 {
	uint32_t cmd;
	uint32_t cmdsize;
	uint64_t init_address;
	uint64_t init_module;
	uint64_t reserved1;
	uint64_t reserved2;
	uint64_t reserved3;
	uint64_t reserved4;
	uint64_t reserved5;
	uint64_t reserved6;
};

/* --- Dynamic symbol table --- */

struct dysymtab_command {
	uint32_t cmd;
	uint32_t cmdsize;
	uint32_t ilocalsym;
	uint32_t nlocalsym;
	uint32_t iextdefsym;
	uint32_t nextdefsym;
	uint32_t iundefsym;
	uint32_t nundefsym;
	uint32_t tocoff;
	uint32_t ntoc;
	uint32_t modtaboff;
	uint32_t nmodtab;
	uint32_t extrefsymoff;
	uint32_t nextrefsyms;
	uint32_t indirectsymoff;
	uint32_t nindirectsyms;
	uint32_t extreloff;
	uint32_t nextrel;
	uint32_t locreloff;
	uint32_t nlocrel;
};

#define INDIRECT_SYMBOL_LOCAL 0x80000000u
#define INDIRECT_SYMBOL_ABS   0x40000000u

/* --- Dylib command --- */

struct dylib {
	union lc_str  name;
	uint32_t      timestamp;
	uint32_t      current_version;
	uint32_t      compatibility_version;
};

struct dylib_command {
	uint32_t      cmd;
	uint32_t      cmdsize;
	struct dylib  dylib;
};

/* --- Dynamic linker command --- */

struct dylinker_command {
	uint32_t     cmd;
	uint32_t     cmdsize;
	union lc_str name;
};

/* --- Entry point command --- */

struct entry_point_command {
	uint32_t cmd;
	uint32_t cmdsize;
	uint64_t entryoff;
	uint64_t stacksize;
};

/* --- UUID command --- */

struct uuid_command {
	uint32_t cmd;
	uint32_t cmdsize;
	uint8_t  uuid[16];
};

/* --- Fat binary --- */

#define FAT_MAGIC     0xcafebabe
#define FAT_CIGAM     0xbebafeca
#define FAT_MAGIC_64  0xcafebabf
#define FAT_CIGAM_64  0xbfbafeca

struct fat_header {
	uint32_t magic;
	uint32_t nfat_arch;
};

struct fat_arch {
	cpu_type_t    cputype;
	cpu_subtype_t cpusubtype;
	uint32_t      offset;
	uint32_t      size;
	uint32_t      align;
};

struct fat_arch_64 {
	cpu_type_t    cputype;
	cpu_subtype_t cpusubtype;
	uint64_t      offset;
	uint64_t      size;
	uint32_t      align;
	uint32_t      reserved;
};

/* --- Symbol table --- */

struct symtab_command {
	uint32_t cmd;       /* LC_SYMTAB */
	uint32_t cmdsize;   /* sizeof(struct symtab_command) */
	uint32_t symoff;    /* file offset of nlist array */
	uint32_t nsyms;     /* number of symbol entries */
	uint32_t stroff;    /* file offset of string table */
	uint32_t strsize;   /* string table size in bytes */
};

struct nlist_64 {
	uint32_t n_strx;    /* index into string table */
	uint8_t  n_type;    /* type flag */
	uint8_t  n_sect;    /* section ordinal */
	uint16_t n_desc;    /* description */
	uint64_t n_value;   /* value (address) */
};

/* n_type masks */
#define N_STAB  0xe0    /* stab entry */
#define N_PEXT  0x10    /* private external */
#define N_TYPE  0x0e    /* type mask */
#define N_EXT   0x01    /* external (global) */

/* n_type values (masked with N_TYPE) */
#define N_UNDF  0x0     /* undefined */
#define N_ABS   0x2     /* absolute */
#define N_SECT  0xe     /* defined in section */
#define N_INDR  0xa     /* indirect */

/* n_desc flags */
#define N_WEAK_REF 0x0040
#define N_WEAK_DEF 0x0080

/* Compatibility: Darling's loader.c expects MAP_32BIT on non-64bit */
#ifndef MAP_32BIT
#define MAP_32BIT 0
#endif

#endif /* _MACHO_DEFS_H_ */
