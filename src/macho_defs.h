#ifndef MACHGATE_MACHO_DEFS_H
#define MACHGATE_MACHO_DEFS_H

#include <stdint.h>

#define MH_MAGIC_64 0xfeedfacfu
#define FAT_CIGAM 0xbebafecau
#define FAT_CIGAM_64 0xbfbafecau

#define CPU_TYPE_ARM64 0x0100000cu
#define MH_EXECUTE 2u
#define MH_DYLIB 6u

#define LC_SEGMENT_64 0x19u
#define LC_SYMTAB 0x2u
#define LC_DYSYMTAB 0xbu
#define LC_UNIXTHREAD 0x5u
#define LC_MAIN 0x80000028u
#define LC_BUILD_VERSION 0x32u
#define LC_LOAD_DYLIB 0xcu
#define LC_LOAD_WEAK_DYLIB 0x80000018u
#define LC_REEXPORT_DYLIB 0x8000001fu
#define LC_LOAD_UPWARD_DYLIB 0x80000023u
#define LC_DYLD_INFO_ONLY 0x80000022u
#define LC_DYLD_CHAINED_FIXUPS 0x80000034u

#define VM_PROT_READ 1
#define VM_PROT_WRITE 2
#define VM_PROT_EXECUTE 4

#define S_ATTR_PURE_INSTRUCTIONS 0x80000000u
#define S_ATTR_SOME_INSTRUCTIONS 0x00000400u

#define N_STAB 0xe0u
#define N_EXT 0x01u
#define N_TYPE 0x0eu
#define N_SECT 0x0eu

#define SECTION_TYPE 0x000000ffu
#define S_LAZY_SYMBOL_POINTERS 0x00000007u
#define S_SYMBOL_STUBS 0x00000008u
#define S_MOD_INIT_FUNC_POINTERS 0x00000009u
#define S_THREAD_LOCAL_REGULAR 0x00000011u
#define S_THREAD_LOCAL_ZEROFILL 0x00000012u
#define S_THREAD_LOCAL_VARIABLES 0x00000013u
#define S_INIT_FUNC_OFFSETS 0x00000016u

#define INDIRECT_SYMBOL_LOCAL 0x80000000u
#define INDIRECT_SYMBOL_ABS 0x40000000u

struct fat_header {
	uint32_t magic;
	uint32_t nfat_arch;
};

struct fat_arch {
	uint32_t cputype;
	uint32_t cpusubtype;
	uint32_t offset;
	uint32_t size;
	uint32_t align;
};

struct fat_arch_64 {
	uint32_t cputype;
	uint32_t cpusubtype;
	uint64_t offset;
	uint64_t size;
	uint32_t align;
	uint32_t reserved;
};

struct mach_header_64 {
	uint32_t magic;
	uint32_t cputype;
	uint32_t cpusubtype;
	uint32_t filetype;
	uint32_t ncmds;
	uint32_t sizeofcmds;
	uint32_t flags;
	uint32_t reserved;
};

struct load_command {
	uint32_t cmd;
	uint32_t cmdsize;
};

struct segment_command_64 {
	uint32_t cmd;
	uint32_t cmdsize;
	char segname[16];
	uint64_t vmaddr;
	uint64_t vmsize;
	uint64_t fileoff;
	uint64_t filesize;
	int32_t maxprot;
	int32_t initprot;
	uint32_t nsects;
	uint32_t flags;
};

struct section_64 {
	char sectname[16];
	char segname[16];
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

struct build_version_command {
	uint32_t cmd;
	uint32_t cmdsize;
	uint32_t platform;
	uint32_t minos;
	uint32_t sdk;
	uint32_t ntools;
};

struct entry_point_command {
	uint32_t cmd;
	uint32_t cmdsize;
	uint64_t entryoff;
	uint64_t stacksize;
};

struct symtab_command {
	uint32_t cmd;
	uint32_t cmdsize;
	uint32_t symoff;
	uint32_t nsyms;
	uint32_t stroff;
	uint32_t strsize;
};

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

struct dylib_info {
	uint32_t name_offset;
	uint32_t timestamp;
	uint32_t current_version;
	uint32_t compatibility_version;
};

struct dylib_command {
	uint32_t cmd;
	uint32_t cmdsize;
	struct dylib_info dylib;
};

struct dyld_info_command {
	uint32_t cmd;
	uint32_t cmdsize;
	uint32_t rebase_off;
	uint32_t rebase_size;
	uint32_t bind_off;
	uint32_t bind_size;
	uint32_t weak_bind_off;
	uint32_t weak_bind_size;
	uint32_t lazy_bind_off;
	uint32_t lazy_bind_size;
	uint32_t export_off;
	uint32_t export_size;
};

struct linkedit_data_command {
	uint32_t cmd;
	uint32_t cmdsize;
	uint32_t dataoff;
	uint32_t datasize;
};

struct nlist_64 {
	uint32_t n_strx;
	uint8_t n_type;
	uint8_t n_sect;
	uint16_t n_desc;
	uint64_t n_value;
};

struct arm_thread_state64 {
	uint64_t x[29];
	uint64_t fp;
	uint64_t lr;
	uint64_t sp;
	uint64_t pc;
	uint32_t cpsr;
	uint32_t pad;
};

#endif
