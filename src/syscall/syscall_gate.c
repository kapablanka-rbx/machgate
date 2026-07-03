#include "syscall_gate.h"
#include "arm64_enc.h"
#include "macho_defs.h"
#include "syscall_range_000_099.h"
#include "syscall_range_100_199.h"
#include "syscall_range_200_399.h"
#include "syscall_range_400_plus.h"

#include "guest_vm_dispatch.h"
#include "log.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <unistd.h>

#define DARWIN_SYS_exit  1
#define DARWIN_SYS_read  3
#define DARWIN_SYS_write 4
#define DARWIN_SYS_open  5
#define DARWIN_SYS_close 6
#define DARWIN_SYS_munmap 73
#define DARWIN_SYS_mprotect 74
#define DARWIN_SYS_fstat 189
#define DARWIN_SYS_mmap 197
#define DARWIN_SYS_lseek 199
#define DARWIN_ENOSYS    78

#define DARWIN_O_NONBLOCK  0x00000004
#define DARWIN_O_APPEND    0x00000008
#define DARWIN_O_NOFOLLOW  0x00000100
#define DARWIN_O_CREAT     0x00000200
#define DARWIN_O_TRUNC     0x00000400
#define DARWIN_O_EXCL      0x00000800
#define DARWIN_O_NOCTTY    0x00020000
#define DARWIN_O_DIRECTORY 0x00100000
#define DARWIN_O_CLOEXEC   0x01000000

#define DARWIN_MAP_SHARED  0x0001
#define DARWIN_MAP_PRIVATE 0x0002
#define DARWIN_MAP_FIXED   0x0010
#define DARWIN_MAP_ANON    0x1000

#define DARWIN_SVC_0X80 0xD4001001u
#define SYSCALL_GATE_POOL_SIZE (256 * 1024)
#define SYSCALL_GATE_ISLAND_WORDS 96
#define SYSCALL_GATE_CARRY 0x20000000u
#define SYSCALL_GATE_MAX_POOLS 256
#define SYSCALL_GATE_SEARCH_STEP (1024 * 1024)

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

struct syscall_gate_pool {
	uint32_t* cur;
	uint32_t* end;
	void* base;
	size_t size;
};

static struct syscall_gate_pool gate_pools[SYSCALL_GATE_MAX_POOLS];
static size_t gate_pool_count;

static FILE* syscall_gate_open_trace_file(void)
{
	const char* trace_file = getenv("MACHGATE_EXECVE_TRACE_FILE");

	if (!trace_file || !*trace_file)
		return NULL;
	return fopen(trace_file, "a");
}

static void trace_syscall_gate_exit(struct syscall_gate_state* state,
                                    const char* name,
                                    int status)
{
	FILE* trace_file = syscall_gate_open_trace_file();

	if (!trace_file)
		return;
	fprintf(trace_file,
	        "syscall_gate: %s self=%d tid=%d ppid=%d status=%d pc=%p lr=%p x16=%llu\n",
	        name, (int)syscall(SYS_getpid), (int)syscall(SYS_gettid),
	        (int)syscall(SYS_getppid), status, (void*)state->pc,
	        (void*)state->x30, (unsigned long long)state->x[16]);
	fclose(trace_file);
}

static uint32_t enc_stp_sp(int rt, int rt2, int offset)
{
	return 0xA9000000u | (((uint32_t)offset / 8) << 15) | ((uint32_t)rt2 << 10) | (31u << 5) | (uint32_t)rt;
}

static uint32_t enc_ldp_sp(int rt, int rt2, int offset)
{
	return 0xA9400000u | (((uint32_t)offset / 8) << 15) | ((uint32_t)rt2 << 10) | (31u << 5) | (uint32_t)rt;
}

static uint32_t enc_str_sp(int rt, int offset)
{
	return 0xF9000000u | (((uint32_t)offset / 8) << 10) | (31u << 5) | (uint32_t)rt;
}

static uint32_t enc_ldr_sp(int rt, int offset)
{
	return 0xF9400000u | (((uint32_t)offset / 8) << 10) | (31u << 5) | (uint32_t)rt;
}

static uint32_t enc_sub_sp(int imm)
{
	return 0xD10003FFu | ((uint32_t)imm << 10);
}

static uint32_t enc_add_sp(int imm)
{
	return 0x910003FFu | ((uint32_t)imm << 10);
}

static uint32_t enc_mrs_nzcv(int rt)
{
	return 0xD53B4200u | (uint32_t)rt;
}

static uint32_t enc_msr_nzcv(int rt)
{
	return 0xD51B4200u | (uint32_t)rt;
}

static uint32_t enc_add_x0_sp_0(void)
{
	return 0x910003E0u;
}

static uint32_t enc_ldr_literal(int rt, uint32_t* instr, uint32_t* literal)
{
	int64_t word_offset = literal - instr;
	return 0x58000000u | (((uint32_t)word_offset & 0x7FFFFu) << 5) | (uint32_t)rt;
}

static uint32_t enc_blr(int rn)
{
	return 0xD63F0000u | ((uint32_t)rn << 5);
}

static uint32_t enc_br(int rn)
{
	return 0xD61F0000u | ((uint32_t)rn << 5);
}

static int native_prot_from_macho(int prot)
{
	int result = 0;
	if (prot & VM_PROT_READ) result |= PROT_READ;
	if (prot & VM_PROT_WRITE) result |= PROT_WRITE;
	if (prot & VM_PROT_EXECUTE) result |= PROT_EXEC;
	return result;
}

static void set_success(struct syscall_gate_state* state, uint64_t result)
{
	state->x[0] = result;
	state->nzcv &= ~SYSCALL_GATE_CARRY;
}

static void set_failure(struct syscall_gate_state* state, int err)
{
	state->x[0] = (uint64_t)err;
	state->nzcv |= SYSCALL_GATE_CARRY;
}

struct darwin_stat {
	int32_t         st_dev;
	uint16_t        st_mode;
	uint16_t        st_nlink;
	uint64_t        st_ino;
	uint32_t        st_uid;
	uint32_t        st_gid;
	int32_t         st_rdev;
	int32_t         __pad0;
	struct timespec st_atimespec;
	struct timespec st_mtimespec;
	struct timespec st_ctimespec;
	struct timespec st_birthtimespec;
	int64_t         st_size;
	int64_t         st_blocks;
	int32_t         st_blksize;
	uint32_t        st_flags;
	uint32_t        st_gen;
	int32_t         st_lspare;
	int64_t         st_qspare[2];
};

_Static_assert(sizeof(struct darwin_stat) == 144,
	"darwin_stat must be 144 bytes");

static void linux_to_darwin_stat(const struct stat* linux_stat,
                                 struct darwin_stat* darwin_stat)
{
	memset(darwin_stat, 0, sizeof(*darwin_stat));
	darwin_stat->st_dev = (int32_t)linux_stat->st_dev;
	darwin_stat->st_mode = (uint16_t)linux_stat->st_mode;
	darwin_stat->st_nlink = (uint16_t)linux_stat->st_nlink;
	darwin_stat->st_ino = linux_stat->st_ino;
	darwin_stat->st_uid = linux_stat->st_uid;
	darwin_stat->st_gid = linux_stat->st_gid;
	darwin_stat->st_rdev = (int32_t)linux_stat->st_rdev;
	darwin_stat->st_atimespec = linux_stat->st_atim;
	darwin_stat->st_mtimespec = linux_stat->st_mtim;
	darwin_stat->st_ctimespec = linux_stat->st_ctim;
	darwin_stat->st_birthtimespec = linux_stat->st_ctim;
	darwin_stat->st_size = linux_stat->st_size;
	darwin_stat->st_blocks = linux_stat->st_blocks;
	darwin_stat->st_blksize = linux_stat->st_blksize;
}

static int translate_open_flags(int darwin_flags)
{
	int linux_flags = darwin_flags & O_ACCMODE;

	if (darwin_flags & DARWIN_O_NONBLOCK) linux_flags |= O_NONBLOCK;
	if (darwin_flags & DARWIN_O_APPEND) linux_flags |= O_APPEND;
	if (darwin_flags & DARWIN_O_NOFOLLOW) linux_flags |= O_NOFOLLOW;
	if (darwin_flags & DARWIN_O_CREAT) linux_flags |= O_CREAT;
	if (darwin_flags & DARWIN_O_TRUNC) linux_flags |= O_TRUNC;
	if (darwin_flags & DARWIN_O_EXCL) linux_flags |= O_EXCL;
	if (darwin_flags & DARWIN_O_NOCTTY) linux_flags |= O_NOCTTY;
	if (darwin_flags & DARWIN_O_DIRECTORY) linux_flags |= O_DIRECTORY;
	if (darwin_flags & DARWIN_O_CLOEXEC) linux_flags |= O_CLOEXEC;

	return linux_flags;
}

static int translate_mmap_flags(int darwin_flags)
{
	int linux_flags = 0;

	if (darwin_flags & DARWIN_MAP_SHARED) linux_flags |= MAP_SHARED;
	if (darwin_flags & DARWIN_MAP_PRIVATE) linux_flags |= MAP_PRIVATE;
	if (darwin_flags & DARWIN_MAP_FIXED) linux_flags |= MAP_FIXED;
	if (darwin_flags & DARWIN_MAP_ANON) linux_flags |= MAP_ANONYMOUS;

	return linux_flags;
}

static void finish_syscall_result(struct syscall_gate_state* state, long result)
{
	if (result < 0)
		set_failure(state, errno ? errno : EIO);
	else
		set_success(state, (uint64_t)result);
}

static long write_syscall_with_sigpipe_guard(long syscall_number, int fd,
                                             const void* buffer,
                                             size_t size_or_count)
{
	struct sigaction ignore_action;
	struct sigaction old_action;
	int changed = 0;

	memset(&ignore_action, 0, sizeof(ignore_action));
	ignore_action.sa_handler = SIG_IGN;
	sigemptyset(&ignore_action.sa_mask);
	if (sigaction(SIGPIPE, &ignore_action, &old_action) == 0)
		changed = 1;

	errno = 0;
	long result = syscall(syscall_number, fd, buffer, size_or_count);
	int saved_errno = errno;

	if (changed && old_action.sa_handler != SIG_DFL)
		sigaction(SIGPIPE, &old_action, NULL);

	errno = saved_errno;
	return result;
}

long machgate_syscall_write_no_sigpipe(int fd, const void* buffer, size_t size)
{
	return write_syscall_with_sigpipe_guard(SYS_write, fd, buffer, size);
}

long machgate_syscall_writev_no_sigpipe(int fd, const struct iovec* iov,
                                        int iovcnt)
{
	return write_syscall_with_sigpipe_guard(SYS_writev, fd, iov, (size_t)iovcnt);
}

void syscall_gate_dispatch(struct syscall_gate_state* state)
{
	switch (state->x[16]) {
	case DARWIN_SYS_exit:
		trace_syscall_gate_exit(state, "exit", (int)state->x[0]);
		syscall(SYS_exit_group, (int)state->x[0]);
		_exit((int)state->x[0]);
	case DARWIN_SYS_read: {
		errno = 0;
		long result = syscall(SYS_read, (int)state->x[0],
		                      (void*)state->x[1],
		                      (size_t)state->x[2]);
		finish_syscall_result(state, result);
		return;
	}
	case DARWIN_SYS_write: {
		long result = machgate_syscall_write_no_sigpipe((int)state->x[0],
		                                                (const void*)state->x[1],
		                                                (size_t)state->x[2]);
		finish_syscall_result(state, result);
		return;
	}
	case DARWIN_SYS_open: {
		errno = 0;
		long result = syscall(SYS_openat, AT_FDCWD,
		                      (const char*)state->x[0],
		                      translate_open_flags((int)state->x[1]),
		                      (mode_t)state->x[2]);
		finish_syscall_result(state, result);
		return;
	}
	case DARWIN_SYS_close: {
		errno = 0;
		long result = syscall(SYS_close, (int)state->x[0]);
		finish_syscall_result(state, result);
		return;
	}
	case DARWIN_SYS_munmap: {
		long result = machgate_dispatch_darwin_munmap((void*)state->x[0],
		                                    (size_t)state->x[1]);
		finish_syscall_result(state, result);
		return;
	}
	case DARWIN_SYS_mprotect: {
		errno = 0;
		long result = syscall(SYS_mprotect, (void*)state->x[0],
		                      (size_t)state->x[1], (int)state->x[2]);
		finish_syscall_result(state, result);
		return;
	}
	case DARWIN_SYS_fstat: {
		struct stat linux_stat;
		errno = 0;
		long result = syscall(SYS_fstat, (int)state->x[0], &linux_stat);
		if (result < 0) {
			set_failure(state, errno ? errno : EIO);
		} else {
			linux_to_darwin_stat(&linux_stat, (struct darwin_stat*)state->x[1]);
			set_success(state, 0);
		}
		return;
	}
	case DARWIN_SYS_mmap: {
		long result = machgate_dispatch_darwin_mmap((void*)state->x[0],
		                                  (size_t)state->x[1],
		                                  (int)state->x[2],
		                                  translate_mmap_flags((int)state->x[3]),
		                                  (int)state->x[4],
		                                  (off_t)state->x[5]);
		finish_syscall_result(state, result);
		return;
	}
	case DARWIN_SYS_lseek: {
		errno = 0;
		long result = syscall(SYS_lseek, (int)state->x[0],
		                      (off_t)state->x[1], (int)state->x[2]);
		finish_syscall_result(state, result);
		return;
	}
	default:
		if (syscall_range_000_099_dispatch(state) ||
		    syscall_range_100_199_dispatch(state) ||
		    syscall_range_200_399_dispatch(state) ||
		    syscall_range_400_plus_dispatch(state))
			return;
		fprintf(stderr,
		        "syscall_gate: unsupported Darwin syscall %llu pc=%p "
		        "x0=%#llx x1=%#llx x2=%#llx x3=%#llx x4=%#llx x5=%#llx\n",
		        (unsigned long long)state->x[16], (void*)state->pc,
		        (unsigned long long)state->x[0],
		        (unsigned long long)state->x[1],
		        (unsigned long long)state->x[2],
		        (unsigned long long)state->x[3],
		        (unsigned long long)state->x[4],
		        (unsigned long long)state->x[5]);
		set_failure(state, DARWIN_ENOSYS);
		return;
	}
}

static int emit_island(uint32_t* island, uint32_t* original)
{
	uint32_t* p = island;
	uint32_t* dispatcher_literal = island + SYSCALL_GATE_ISLAND_WORDS - 4;
	uint32_t* resume_literal = island + SYSCALL_GATE_ISLAND_WORDS - 2;
	uintptr_t resume_pc = (uintptr_t)(original + 1);
	uintptr_t dispatcher = (uintptr_t)&syscall_gate_dispatch;

	*p++ = enc_sub_sp(192);
	*p++ = enc_stp_sp(0, 1, 0);
	*p++ = enc_stp_sp(2, 3, 16);
	*p++ = enc_stp_sp(4, 5, 32);
	*p++ = enc_stp_sp(6, 7, 48);
	*p++ = enc_stp_sp(8, 9, 64);
	*p++ = enc_stp_sp(10, 11, 80);
	*p++ = enc_stp_sp(12, 13, 96);
	*p++ = enc_stp_sp(14, 15, 112);
	*p++ = enc_stp_sp(16, 17, 128);
	*p++ = enc_str_sp(18, 144);
	*p++ = enc_str_sp(30, 152);
	*p++ = enc_mrs_nzcv(16);
	*p++ = enc_str_sp(16, 160);
	*p = enc_ldr_literal(16, p, resume_literal);
	p++;
	*p++ = enc_str_sp(16, 168);
	*p++ = enc_add_x0_sp_0();
	*p = enc_ldr_literal(16, p, dispatcher_literal);
	p++;
	*p++ = enc_blr(16);
	*p++ = enc_ldp_sp(1, 2, 8);
	*p++ = enc_ldp_sp(3, 4, 24);
	*p++ = enc_ldp_sp(5, 6, 40);
	*p++ = enc_ldp_sp(7, 8, 56);
	*p++ = enc_ldp_sp(9, 10, 72);
	*p++ = enc_ldp_sp(11, 12, 88);
	*p++ = enc_ldp_sp(13, 14, 104);
	*p++ = enc_ldr_sp(15, 120);
	*p++ = enc_ldr_sp(17, 136);
	*p++ = enc_ldr_sp(18, 144);
	*p++ = enc_ldr_sp(30, 152);
	*p++ = enc_ldr_sp(16, 160);
	*p++ = enc_msr_nzcv(16);
	*p++ = enc_ldr_sp(0, 0);
	*p++ = enc_ldr_sp(16, 168);
	*p++ = enc_add_sp(192);
	*p++ = enc_br(16);

	while (p < dispatcher_literal)
		*p++ = ENC_NOP;

	memcpy(dispatcher_literal, &dispatcher, sizeof(dispatcher));
	memcpy(resume_literal, &resume_pc, sizeof(resume_pc));
	return 0;
}

static size_t page_size(void)
{
	long result = sysconf(_SC_PAGESIZE);
	if (result <= 0)
		return 4096;
	return (size_t)result;
}

static uintptr_t align_down(uintptr_t value, size_t alignment)
{
	return value & ~((uintptr_t)alignment - 1);
}

static uintptr_t align_up(uintptr_t value, size_t alignment)
{
	return (value + alignment - 1) & ~((uintptr_t)alignment - 1);
}

static int island_reachable(uint32_t* instr, uint32_t* island)
{
	int64_t byte_offset = (char*)island - (char*)instr;
	return arm64_branch_in_range(byte_offset);
}

static struct syscall_gate_pool* register_gate_pool(void* base, size_t size)
{
	if (!base || size < SYSCALL_GATE_ISLAND_WORDS * sizeof(uint32_t))
		return NULL;
	if (gate_pool_count >= SYSCALL_GATE_MAX_POOLS) {
		fprintf(stderr, "syscall_gate: too many island pools\n");
		return NULL;
	}

	struct syscall_gate_pool* pool = &gate_pools[gate_pool_count++];
	pool->base = base;
	pool->size = size;
	pool->cur = (uint32_t*)base;
	pool->end = pool->cur + size / sizeof(uint32_t);
	return pool;
}

static struct syscall_gate_pool* find_gate_pool(uint32_t* instr)
{
	for (size_t i = 0; i < gate_pool_count; i++) {
		struct syscall_gate_pool* pool = &gate_pools[i];
		if (pool->cur + SYSCALL_GATE_ISLAND_WORDS > pool->end)
			continue;
		if (island_reachable(instr, pool->cur))
			return pool;
	}
	return NULL;
}

static void* map_gate_pool_at(uintptr_t address, size_t size)
{
	void* result = mmap((void*)address, size, PROT_READ | PROT_WRITE | PROT_EXEC,
	                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
	                    -1, 0);
	if (result == MAP_FAILED)
		return NULL;
	if ((uintptr_t)result == address)
		return result;
	munmap(result, size);
	return NULL;
}

static struct syscall_gate_pool* allocate_gate_pool_below(uint32_t* instr,
                                                          size_t size,
                                                          size_t page)
{
	uintptr_t site = (uintptr_t)instr;
	uintptr_t range = (uintptr_t)(1u << 27);
	uintptr_t start = align_down(site > size ? site - size : page, page);
	uintptr_t minimum = site > range ? align_up(site - range, page) : page;

	for (uintptr_t address = start; address >= minimum; ) {
		void* base = map_gate_pool_at(address, size);
		if (base)
			return register_gate_pool(base, size);
		if (address < minimum + SYSCALL_GATE_SEARCH_STEP)
			break;
		address -= SYSCALL_GATE_SEARCH_STEP;
	}

	void* base = map_gate_pool_at(minimum, size);
	if (base)
		return register_gate_pool(base, size);
	return NULL;
}

static struct syscall_gate_pool* allocate_gate_pool_above(uint32_t* instr,
                                                          size_t size,
                                                          size_t page)
{
	uintptr_t site = (uintptr_t)instr;
	uintptr_t range = (uintptr_t)(1u << 27);
	uintptr_t start = align_up(site + page, page);
	uintptr_t maximum = align_down(site + range - size, page);

	for (uintptr_t address = start; address <= maximum; address += SYSCALL_GATE_SEARCH_STEP) {
		void* base = map_gate_pool_at(address, size);
		if (base)
			return register_gate_pool(base, size);
		if (maximum - address < SYSCALL_GATE_SEARCH_STEP)
			break;
	}

	void* base = map_gate_pool_at(maximum, size);
	if (base)
		return register_gate_pool(base, size);
	return NULL;
}

static struct syscall_gate_pool* allocate_gate_pool_near(uint32_t* instr)
{
	size_t page = page_size();
	size_t size = align_up(SYSCALL_GATE_POOL_SIZE, page);
	struct syscall_gate_pool* result = allocate_gate_pool_below(instr, size, page);
	if (result)
		return result;
	return allocate_gate_pool_above(instr, size, page);
}

static int patch_instruction(uint32_t* instr)
{
	struct syscall_gate_pool* pool = find_gate_pool(instr);
	if (!pool)
		pool = allocate_gate_pool_near(instr);
	if (!pool) {
		fprintf(stderr, "syscall_gate: no island pool within B range of %p\n",
		        (void*)instr);
		return -1;
	}

	uint32_t* island = pool->cur;
	emit_island(island, instr);
	*instr = enc_b((int32_t)(island - instr));
	pool->cur += SYSCALL_GATE_ISLAND_WORDS;
	return 0;
}

static int patch_section(uint32_t* code, size_t size)
{
	int patched = 0;
	size_t count = size / 4;

	for (size_t i = 0; i < count; i++) {
		if (code[i] != DARWIN_SVC_0X80)
			continue;
		if (patch_instruction(&code[i]) < 0)
			return -1;
		patched++;
	}

	return patched;
}

int syscall_gate_patch(struct load_results* lr)
{
	if (!lr || !lr->mh)
		return 0;

	gate_pool_count = 0;

	void* pool = machgate_pool_alloc(lr, SYSCALL_GATE_POOL_SIZE);
	if (!pool) {
		fprintf(stderr, "syscall_gate: no adjacent pool space for syscall islands\n");
	} else if (!register_gate_pool(pool, SYSCALL_GATE_POOL_SIZE)) {
		return -1;
	}

	struct mach_header_64* mh = (struct mach_header_64*)lr->mh;
	uint8_t* cmds = (uint8_t*)(mh + 1);
	int total = 0;
	uint32_t offset = 0;

	for (uint32_t i = 0; i < mh->ncmds && offset < mh->sizeofcmds; i++) {
		struct load_command* lc = (struct load_command*)(cmds + offset);
		if (lc->cmd == LC_SEGMENT_64) {
			struct segment_command_64* seg = (struct segment_command_64*)lc;
			if (seg->maxprot & VM_PROT_EXECUTE) {
				void* seg_addr = (void*)(seg->vmaddr + lr->slide);
				mprotect(seg_addr, seg->vmsize, PROT_READ | PROT_WRITE | PROT_EXEC);

				struct section_64* sect = (struct section_64*)(seg + 1);
				for (uint32_t s = 0; s < seg->nsects; s++, sect++) {
					if (!(sect->flags & S_ATTR_SOME_INSTRUCTIONS) &&
					    !(sect->flags & S_ATTR_PURE_INSTRUCTIONS))
						continue;
					uint32_t* code = (uint32_t*)(sect->addr + lr->slide);
					int result = patch_section(code, sect->size);
					if (result < 0)
						return -1;
					total += result;
				}

				mprotect(seg_addr, seg->vmsize, native_prot_from_macho(seg->initprot));
				__builtin___clear_cache((char*)seg_addr, (char*)seg_addr + seg->vmsize);
			}
		}
		offset += lc->cmdsize;
	}

	for (size_t i = 0; i < gate_pool_count; i++) {
		char* base = (char*)gate_pools[i].base;
		__builtin___clear_cache(base, base + gate_pools[i].size);
	}
	if (total > 0)
		machgate_log_startup("syscall_gate: patched %d Darwin syscalls\n", total);
	return total;
}
