/*
 * libSystem.B.dylib shim for aarch64 Linux.
 *
 * Maps Apple libSystem symbols to glibc equivalents. Also translates
 * POSIX functions whose constants differ between Darwin and Linux
 * (O_* flags, AT_* flags, fcntl commands, struct stat layout, etc.).
 * Remaining standard C/POSIX symbols (malloc, pthread_create, strlen,
 * etc.) resolve through normal glibc linking.
 *
 * Built as a shared library:
 *   gcc -shared -fPIC -o libsystem_shim.so libsystem_shim.c -lm -lpthread -ldl
 */

#define _GNU_SOURCE
#include "../syscall/execve_reexec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <wctype.h>
#include <ctype.h>
#include <pthread.h>
#include <signal.h>
#include <ucontext.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/uio.h>

static int shim_startup_log_enabled(void)
{
	const char* value = getenv("MACHGATE_VERBOSE");
	if (!value)
		value = getenv("MACHISMO_VERBOSE");
	if (!value)
		value = getenv("MACHGATE_LOG_STARTUP");
	if (!value)
		value = getenv("MACHISMO_LOG_STARTUP");
	return value && value[0] && strcmp(value, "0") != 0 &&
	       strcmp(value, "false") != 0 && strcmp(value, "FALSE") != 0 &&
	       strcmp(value, "no") != 0 && strcmp(value, "NO") != 0;
}
#include <linux/futex.h>
#include <poll.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <dirent.h>
#include <stdarg.h>
#include <malloc.h>
#include <locale.h>
#include <sched.h>
#include <pwd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/mount.h>

#define MACHGATE_SHIM_CALLER() __builtin_extract_return_addr(__builtin_return_address(0))

extern char **environ;

static int shim_hw_ncpu(void);
static int shim_trace_enabled(void);
static int shim_objc_msgsend_trace_enabled(void);
static int shim_cxx_init_full_trace_enabled(void);
static int shim_delta_vm_trace_enabled(void);
static int shim_wait_trace_enabled(void);
static int shim_alloc_trace_enabled(void);
static int shim_alloc_mismatch_trace_enabled(void);
static int shim_alloc_signal_dump_enabled(void);
static int shim_host_sigchld_handler_enabled(void);
static FILE* shim_open_trace_file(void);
static void shim_fd_trace_log(const char* format, ...);
static void shim_run_tlv_term_funcs(void);
static void shim_run_thread_tsd_destructors(void);
static pid_t shim_trace_tid(void);
static unsigned long shim_trace_pthread_self(void);
static const char* shim_trace_path(const char* path);
static char* getenv_from_guest_envp(const char* name);
static int shim_errno_from_linux(int linux_errno);
static int translate_oflags(int darwin_flags);
static int libc_open(const char* pathname, int linux_flags, mode_t mode);
static void trace_guest_address_context(const char* label, uintptr_t address);
static uintptr_t trace_ucontext_reg(void* ucontext, int reg);
static void trace_signal_indirect_branch(uintptr_t call_site, void* ucontext);
static void shim_dump_recent_alloc_events(const char* reason, const void* ptr);
static pid_t machgate_shim_process_pid;
static const char* shim_init_kind;
static int shim_init_index = -1;
static int shim_init_total;
static uintptr_t shim_init_address;
static int shim_last_wait_valid;
static pid_t shim_last_wait_owner_pid;
static pid_t shim_last_wait_result_pid;
static int shim_last_wait_linux_status;
static int shim_last_wait_darwin_status;
static uintptr_t shim_last_wait_status_ptr;

#if defined(__GNUC__) || defined(__clang__)
#define SHIM_CALLER_RETURN_ADDRESS() \
	__builtin_extract_return_addr(__builtin_return_address(0))
#else
#define SHIM_CALLER_RETURN_ADDRESS() NULL
#endif

__attribute__((constructor))
static void init_machgate_process_pid(void)
{
	machgate_shim_process_pid = (pid_t)syscall(SYS_getpid);
}

static int machgate_shim_in_fork_child(void)
{
	return machgate_shim_process_pid &&
	       (pid_t)syscall(SYS_getpid) != machgate_shim_process_pid;
}

struct darwin_sigaltstack {
	uint64_t sp;
	uint64_t size;
	uint32_t flags;
	uint32_t pad;
};

struct darwin_sigaction {
	uint64_t handler;
	uint32_t mask;
	uint32_t flags;
};

struct darwin_timeval {
	int64_t tv_sec;
	int32_t tv_usec;
	int32_t pad;
};

struct darwin_rusage {
	struct darwin_timeval ru_utime;
	struct darwin_timeval ru_stime;
	int64_t ru_maxrss;
	int64_t ru_ixrss;
	int64_t ru_idrss;
	int64_t ru_isrss;
	int64_t ru_minflt;
	int64_t ru_majflt;
	int64_t ru_nswap;
	int64_t ru_inblock;
	int64_t ru_oublock;
	int64_t ru_msgsnd;
	int64_t ru_msgrcv;
	int64_t ru_nsignals;
	int64_t ru_nvcsw;
	int64_t ru_nivcsw;
};

/* ===== Mach time ===== */

/* mach_absolute_time returns nanoseconds on arm64 (timebase is always 1:1) */
uint64_t mach_absolute_time(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

struct mach_timebase_info_data {
	uint32_t numer;
	uint32_t denom;
};

int mach_timebase_info(struct mach_timebase_info_data *info)
{
	/* On arm64, the timebase is always 1:1 (nanoseconds) */
	info->numer = 1;
	info->denom = 1;
	return 0; /* KERN_SUCCESS */
}

/*
 * clock_gettime_nsec_np(clockid_t) — Darwin's nanosecond clock accessor.
 *
 * Returns the named clock as a uint64 nanosecond count. The Darwin clock ids
 * differ from Linux's, so translate: CLOCK_REALTIME(0) → REALTIME; the per-CPU
 * accounting clocks → their Linux equivalents; everything else (the various
 * MONOTONIC/UPTIME_RAW flavors, incl. CLOCK_UPTIME_RAW=8 used by the Gothic game
 * thread's frame pacing) → CLOCK_MONOTONIC. Without this the import binds to the
 * return-0 stub, the frame delta is always 0, and the fixed-update loop never
 * ticks. Returns 0 on error, matching Darwin.
 */
uint64_t clock_gettime_nsec_np(int clock_id)
{
	clockid_t linux_clk;
	switch (clock_id) {
	case 0:  /* CLOCK_REALTIME */
		linux_clk = CLOCK_REALTIME;
		break;
	case 12: /* CLOCK_PROCESS_CPUTIME_ID */
		linux_clk = CLOCK_PROCESS_CPUTIME_ID;
		break;
	case 16: /* CLOCK_THREAD_CPUTIME_ID */
		linux_clk = CLOCK_THREAD_CPUTIME_ID;
		break;
	default: /* MONOTONIC / MONOTONIC_RAW / UPTIME_RAW (+APPROX) */
		linux_clk = CLOCK_MONOTONIC;
		break;
	}

	struct timespec ts;
	if (clock_gettime(linux_clk, &ts) != 0)
		return 0;
	return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

double log2(double value)
{
	return log(value) / log(2.0);
}

double exp2(double value)
{
	return exp(value * log(2.0));
}

/* ===== Mach ports / task info (stubs) ===== */

/* mach_task_self_ is a global variable in libSystem, not a function */
uint32_t mach_task_self_ = 0x103; /* dummy port number */
uint32_t vm_page_size = 4096;
uint32_t vm_kernel_page_size = 4096;
uint32_t kIOMasterPortDefault = 0;

struct machgate_mach_semaphore {
	uint32_t name;
	int value;
	int used;
};

#define MACHGATE_MACH_SEMAPHORE_MAX 64

static struct machgate_mach_semaphore
	machgate_mach_semaphores[MACHGATE_MACH_SEMAPHORE_MAX];
static uint32_t next_mach_semaphore_name = 0x4000;
static uint32_t next_mach_port_name = 0x5000;

uint32_t mach_host_self(void)
{
	return 0x104;
}

uint32_t mach_thread_self(void)
{
	return (uint32_t)syscall(SYS_gettid);
}

uint64_t mach_continuous_time(void)
{
	return mach_absolute_time();
}

uint64_t mach_approximate_time(void)
{
	return mach_absolute_time();
}

uint64_t mach_continuous_approximate_time(void)
{
	return mach_continuous_time();
}

#define DARWIN_TASK_VM_INFO 22

struct darwin_task_vm_info_prefix {
	uint64_t virtual_size;
	uint32_t region_count;
	uint32_t page_size;
	uint64_t resident_size;
	uint64_t resident_size_peak;
	uint64_t device;
	uint64_t device_peak;
	uint64_t internal;
	uint64_t internal_peak;
	uint64_t external;
	uint64_t external_peak;
	uint64_t reusable;
	uint64_t reusable_peak;
	uint64_t purgeable_volatile_pmap;
	uint64_t purgeable_volatile_resident;
	uint64_t purgeable_volatile_virtual;
	uint64_t compressed;
	uint64_t compressed_peak;
	uint64_t compressed_lifetime;
	uint64_t phys_footprint;
};

static uint64_t guest_resident_bytes(void)
{
	char line_buffer[256];
	FILE* status_file;
	uint64_t rss_kilobytes = 0;

	status_file = fopen("/proc/self/status", "r");
	if (!status_file)
		return 0;
	while (fgets(line_buffer, sizeof(line_buffer), status_file)) {
		if (sscanf(line_buffer, "VmRSS: %llu kB",
		           (unsigned long long*)&rss_kilobytes) == 1)
			break;
	}
	fclose(status_file);
	return rss_kilobytes * 1024ull;
}

static uint64_t guest_virtual_bytes(void)
{
	unsigned long long virtual_pages = 0;
	FILE* statm_file;
	uint64_t virtual_bytes = 0;
	int parsed;

	statm_file = fopen("/proc/self/statm", "r");
	if (!statm_file)
		return 0;
	parsed = fscanf(statm_file, "%llu", &virtual_pages);
	fclose(statm_file);
	if (parsed == 1)
		virtual_bytes = (uint64_t)virtual_pages *
		                (uint64_t)sysconf(_SC_PAGESIZE);
	return virtual_bytes;
}

int task_info(uint32_t target_task, uint32_t flavor,
              void *task_info_out, uint32_t *task_info_count)
{
	(void)target_task;

	if (flavor != DARWIN_TASK_VM_INFO || !task_info_out ||
	    !task_info_count)
		return 5;

	if ((*task_info_count * sizeof(uint32_t)) <
	    sizeof(struct darwin_task_vm_info_prefix))
		return 4;

	{
		struct darwin_task_vm_info_prefix info;
		memset(&info, 0, sizeof(info));
		info.virtual_size = guest_virtual_bytes();
		info.page_size = (uint32_t)sysconf(_SC_PAGESIZE);
		info.resident_size = guest_resident_bytes();
		info.resident_size_peak = info.resident_size;
		info.internal = info.resident_size;
		info.internal_peak = info.resident_size;
		info.phys_footprint = info.resident_size;
		memcpy(task_info_out, &info, sizeof(info));
	}
	return 0;
}

int task_policy_set(uint32_t task, int flavor, void* policy_info,
                    uint32_t policy_info_count)
{
	(void)task;
	(void)flavor;
	(void)policy_info;
	(void)policy_info_count;
	return 0;
}

int thread_info(uint32_t target_thread, int flavor, void* thread_info_out,
                uint32_t* thread_info_count)
{
	(void)target_thread;
	(void)flavor;

	if (thread_info_out && thread_info_count)
		memset(thread_info_out, 0, (size_t)*thread_info_count * sizeof(int));
	return 0;
}

int thread_get_state(uint32_t target_thread, int flavor, void* state,
                     uint32_t* state_count)
{
	(void)target_thread;
	(void)flavor;

	if (state && state_count)
		memset(state, 0, (size_t)*state_count * sizeof(uint32_t));
	return 0;
}

int thread_suspend(uint32_t target_thread)
{
	(void)target_thread;
	return 0;
}

int thread_resume(uint32_t target_thread)
{
	(void)target_thread;
	return 0;
}

int thread_set_exception_ports(uint32_t thread, uint32_t exception_mask,
                               uint32_t new_port, int behavior,
                               int new_flavor)
{
	(void)thread;
	(void)exception_mask;
	(void)new_port;
	(void)behavior;
	(void)new_flavor;
	return 0;
}

int thread_switch(uint32_t thread_name, int option, uint32_t option_time)
{
	(void)thread_name;
	(void)option;

	if (option_time) {
		struct timespec ts;
		ts.tv_sec = option_time / 1000;
		ts.tv_nsec = (long)(option_time % 1000) * 1000000L;
		nanosleep(&ts, NULL);
		return 0;
	}

	sched_yield();
	return 0;
}

/* mach_msg — stub, returns MACH_MSG_SUCCESS (0) */
int mach_msg(void *msg, uint32_t option, uint32_t send_size,
             uint32_t rcv_size, uint32_t rcv_name,
             uint32_t timeout, uint32_t notify)
{
	(void)msg; (void)option; (void)send_size;
	(void)rcv_size; (void)rcv_name; (void)timeout; (void)notify;
	return 0;
}

/* mach_port_deallocate — stub */
int mach_port_deallocate(uint32_t task, uint32_t name)
{
	(void)task; (void)name;
	return 0;
}

int mach_port_allocate(uint32_t task, uint32_t right, uint32_t* name)
{
	(void)task;
	(void)right;

	if (!name)
		return 4;
	*name = __sync_fetch_and_add(&next_mach_port_name, 1);
	return 0;
}

int mach_port_construct(uint32_t task, void* options, uint64_t context,
                        uint32_t* name)
{
	(void)task;
	(void)options;
	(void)context;
	return mach_port_allocate(task, 0, name);
}

int mach_port_insert_right(uint32_t task, uint32_t name, uint32_t poly,
                           uint32_t poly_poly)
{
	(void)task;
	(void)name;
	(void)poly;
	(void)poly_poly;
	return 0;
}

int mach_port_set_attributes(uint32_t task, uint32_t name, int flavor,
                             void* info, uint32_t info_count)
{
	(void)task;
	(void)name;
	(void)flavor;
	(void)info;
	(void)info_count;
	return 0;
}

int vm_deallocate(uint32_t target_task, uint64_t address, uint64_t size)
{
	(void)target_task;

	if (!address || !size)
		return 0;
	if (munmap((void*)(uintptr_t)address, (size_t)size) == 0)
		return 0;
	return 5;
}

static int mach_vm_prot_to_linux(int protection)
{
	int result = PROT_NONE;

	if (protection & 0x1)
		result |= PROT_READ;
	if (protection & 0x2)
		result |= PROT_WRITE;
	if (protection & 0x4)
		result |= PROT_EXEC;
	return result;
}

int vm_allocate(uint32_t target_task, uint64_t* address, uint64_t size,
                int flags)
{
	(void)target_task;

	if (!address || !size)
		return 4;

	void* requested = *address ? (void*)(uintptr_t)*address : NULL;
	int mmap_flags = MAP_PRIVATE | MAP_ANONYMOUS;
	if (requested && !(flags & 0x1))
		mmap_flags |= MAP_FIXED_NOREPLACE;

	void* result = mmap(requested, (size_t)size, PROT_READ | PROT_WRITE,
	                    mmap_flags, -1, 0);
	if (result == MAP_FAILED)
		return 3;

	*address = (uint64_t)(uintptr_t)result;
	return 0;
}

int vm_protect(uint32_t target_task, uint64_t address, uint64_t size,
               uint8_t set_maximum, int new_protection)
{
	(void)target_task;
	(void)set_maximum;

	if (!address || !size)
		return 4;
	if (mprotect((void*)(uintptr_t)address, (size_t)size,
	             mach_vm_prot_to_linux(new_protection)) == 0)
		return 0;
	return 2;
}

#define DARWIN_HOST_VM_INFO 4
#define DARWIN_HOST_SCHED_INFO 2

int host_statistics(uint32_t host_priv, int flavor, void* host_info_out,
                    uint32_t* host_info_count)
{
	(void)host_priv;

	if (!host_info_out || !host_info_count)
		return 1;

	if (flavor == DARWIN_HOST_VM_INFO || flavor == DARWIN_HOST_SCHED_INFO) {
		struct sysinfo linux_info;
		unsigned int page_count;
		unsigned int* counts = host_info_out;
		uint32_t capacity = *host_info_count;
		unsigned long page_size = (unsigned long)sysconf(_SC_PAGESIZE);

		if (sysinfo(&linux_info) != 0)
			return 5;
		page_size = page_size ? page_size : 4096;
		memset(host_info_out, 0,
		       (size_t)capacity * sizeof(unsigned int));
		if (capacity < 3)
			return 0;
		page_count = (unsigned int)(linux_info.freeram / page_size);
		counts[0] = page_count > 64 ? page_count - 64 : page_count;
		page_count = (unsigned int)((linux_info.totalram - linux_info.freeram) / page_size / 4);
		counts[1] = page_count;
		page_count = (unsigned int)(linux_info.freeram / page_size / 8);
		counts[2] = page_count;
		return 0;
	}

	memset(host_info_out, 0, (size_t)*host_info_count * sizeof(int));
	return 0;
}

struct darwin_host_basic_info {
	int32_t max_cpus;
	int32_t avail_cpus;
	uint32_t memory_size;
	int32_t cpu_type;
	int32_t cpu_subtype;
	int32_t cpu_threadtype;
	int32_t physical_cpu;
	int32_t physical_cpu_max;
	int32_t logical_cpu;
	int32_t logical_cpu_max;
	uint64_t max_mem;
};

int host_info(uint32_t host, int flavor, void* info, uint32_t* info_count)
{
	(void)host;

	if (!info || !info_count)
		return 4;

	if (flavor == 1) {
		struct darwin_host_basic_info basic;
		memset(&basic, 0, sizeof(basic));
		basic.max_cpus = shim_hw_ncpu();
		basic.avail_cpus = basic.max_cpus;
		basic.memory_size = 0x80000000U;
		basic.cpu_type = 0x0100000c;
		basic.physical_cpu = basic.max_cpus;
		basic.physical_cpu_max = basic.max_cpus;
		basic.logical_cpu = basic.max_cpus;
		basic.logical_cpu_max = basic.max_cpus;
		basic.max_mem = (uint64_t)sysconf(_SC_PHYS_PAGES) *
		                (uint64_t)sysconf(_SC_PAGESIZE);

		size_t available = (size_t)*info_count * sizeof(int32_t);
		size_t copy_size = available < sizeof(basic) ? available : sizeof(basic);
		memcpy(info, &basic, copy_size);
		*info_count = (uint32_t)(sizeof(basic) / sizeof(int32_t));
		return 0;
	}

	memset(info, 0, (size_t)*info_count * sizeof(int32_t));
	return 0;
}

int mach_msg_server_once(void* demux, uint32_t max_size, uint32_t rcv_name,
                         uint32_t options)
{
	(void)demux;
	(void)max_size;
	(void)rcv_name;
	(void)options;
	return 0;
}

int host_processor_info(uint32_t host, int flavor, uint32_t* out_processor_count,
                        void** out_processor_info,
                        uint32_t* out_processor_info_count)
{
#define DARWIN_PROCESSOR_CPU_LOAD_INFO 2
#define DARWIN_CPU_STATE_MAX 4
#define DARWIN_CPU_STATE_SYSTEM 1
#define DARWIN_CPU_STATE_IDLE 2
	(void)host;

	if (!out_processor_count || !out_processor_info ||
	    !out_processor_info_count)
		return 4;
	if (flavor != DARWIN_PROCESSOR_CPU_LOAD_INFO)
		return 5;

	uint32_t processor_count = (uint32_t)shim_hw_ncpu();
	uint32_t info_count = processor_count * DARWIN_CPU_STATE_MAX;
	size_t info_size = (size_t)info_count * sizeof(uint32_t);
	uint32_t* processor_info = mmap(NULL, info_size, PROT_READ | PROT_WRITE,
	                                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (processor_info == MAP_FAILED)
		return 3;

	for (uint32_t index = 0; index < processor_count; index++) {
		uint32_t* cpu = processor_info + index * DARWIN_CPU_STATE_MAX;
		cpu[DARWIN_CPU_STATE_SYSTEM] = 1;
		cpu[DARWIN_CPU_STATE_IDLE] = 1;
	}

	*out_processor_count = processor_count;
	*out_processor_info = processor_info;
	*out_processor_info_count = info_count;
	return 0;
#undef DARWIN_CPU_STATE_IDLE
#undef DARWIN_CPU_STATE_SYSTEM
#undef DARWIN_CPU_STATE_MAX
#undef DARWIN_PROCESSOR_CPU_LOAD_INFO
}

int host_page_size(uint32_t host, uint64_t* out_page_size)
{
	(void)host;

	if (!out_page_size)
		return 4;
	*out_page_size = (uint64_t)sysconf(_SC_PAGESIZE);
	return 0;
}

struct darwin_vm_statistics64 {
	uint32_t free_count;
	uint32_t active_count;
	uint32_t inactive_count;
	uint32_t wire_count;
	uint64_t zero_fill_count;
	uint64_t reactivations;
	uint64_t pageins;
	uint64_t pageouts;
	uint64_t faults;
	uint64_t cow_faults;
	uint64_t lookups;
	uint64_t hits;
	uint64_t purges;
	uint32_t purgeable_count;
	uint32_t speculative_count;
	uint64_t decompressions;
	uint64_t compressions;
	uint64_t swapins;
	uint64_t swapouts;
	uint32_t compressor_page_count;
	uint32_t throttled_count;
	uint32_t external_page_count;
	uint32_t internal_page_count;
	uint64_t total_uncompressed_pages_in_compressor;
	uint64_t swapped_count;
};

#define DARWIN_HOST_VM_INFO64 4

static void fill_vm_statistics64_from_sysinfo(
	struct darwin_vm_statistics64* stats)
{
	struct sysinfo linux_info;
	uint64_t page_size = (uint64_t)sysconf(_SC_PAGESIZE);
	uint64_t total_pages;
	uint64_t free_pages;
	uint64_t cached_pages;
	uint64_t used_pages;

	memset(stats, 0, sizeof(*stats));
	if (sysinfo(&linux_info) != 0)
		return;
	if (page_size == 0)
		page_size = 4096;

	total_pages = (uint64_t)linux_info.totalram / page_size;
	free_pages = (uint64_t)linux_info.freeram / page_size;
	cached_pages = (uint64_t)linux_info.bufferram / page_size;
	if (free_pages > total_pages)
		free_pages = total_pages;
	used_pages = total_pages > free_pages ? total_pages - free_pages : 0;

	stats->free_count = (uint32_t)free_pages;
	stats->active_count = (uint32_t)used_pages;
	stats->inactive_count = (uint32_t)cached_pages;
	stats->wire_count = (uint32_t)(linux_info.sharedram / page_size);
	stats->purgeable_count = (uint32_t)cached_pages;
	stats->speculative_count = 0;
	stats->external_page_count = (uint32_t)cached_pages;
	stats->internal_page_count =
		(uint32_t)(used_pages > cached_pages ? used_pages - cached_pages
		                                     : 0);
}

int host_statistics64(uint32_t host, int flavor, void* info,
                      uint32_t* info_count)
{
	(void)host;

	if (flavor != DARWIN_HOST_VM_INFO64 || !info || !info_count ||
	    *info_count == 0)
		return 5;

	if ((*info_count * sizeof(uint32_t)) <
	    sizeof(struct darwin_vm_statistics64))
		return 4;

	{
		struct darwin_vm_statistics64 stats;
		fill_vm_statistics64_from_sysinfo(&stats);
		memcpy(info, &stats, sizeof(stats));
	}
	return 0;
}

static struct machgate_mach_semaphore* find_mach_semaphore(uint32_t name)
{
	for (int index = 0; index < MACHGATE_MACH_SEMAPHORE_MAX; index++) {
		if (machgate_mach_semaphores[index].used &&
		    machgate_mach_semaphores[index].name == name)
			return &machgate_mach_semaphores[index];
	}
	return NULL;
}

int semaphore_create(uint32_t task, uint32_t* semaphore, int policy,
                     int value)
{
	(void)task;
	(void)policy;

	if (!semaphore)
		return 4;

	for (int index = 0; index < MACHGATE_MACH_SEMAPHORE_MAX; index++) {
		struct machgate_mach_semaphore* slot =
			&machgate_mach_semaphores[index];
		if (slot->used)
			continue;
		slot->used = 1;
		slot->value = value;
		slot->name = next_mach_semaphore_name++;
		*semaphore = slot->name;
		return 0;
	}

	return 3;
}

int semaphore_destroy(uint32_t task, uint32_t semaphore)
{
	(void)task;

	struct machgate_mach_semaphore* slot = find_mach_semaphore(semaphore);
	if (!slot)
		return 0;
	memset(slot, 0, sizeof(*slot));
	return 0;
}

int semaphore_signal(uint32_t semaphore)
{
	struct machgate_mach_semaphore* slot = find_mach_semaphore(semaphore);
	if (slot)
		slot->value++;
	return 0;
}

int semaphore_wait(uint32_t semaphore)
{
	struct machgate_mach_semaphore* slot = find_mach_semaphore(semaphore);
	if (slot && slot->value > 0)
		slot->value--;
	return 0;
}

int semaphore_timedwait(uint32_t semaphore, uint32_t seconds,
                        uint32_t nanoseconds)
{
	(void)seconds;
	(void)nanoseconds;
	return semaphore_wait(semaphore);
}

uint8_t NDR_record[8] = { 0 };
int __mb_cur_max = 4;

enum {
	DARWIN_FP_NAN = 1,
	DARWIN_FP_INFINITE = 2,
	DARWIN_FP_ZERO = 3,
	DARWIN_FP_NORMAL = 4,
	DARWIN_FP_SUBNORMAL = 5,
};

int __fpclassifyd(double value)
{
	return __builtin_fpclassify(DARWIN_FP_NAN, DARWIN_FP_INFINITE,
	                            DARWIN_FP_NORMAL, DARWIN_FP_SUBNORMAL,
	                            DARWIN_FP_ZERO, value);
}

/* ===== dyld / process bootstrap compatibility ===== */

void dyld_stub_binder(void)
{
}

int* _NSGetArgc(void)
{
	static int argc = 0;
	int* machgate_argc = dlsym(RTLD_DEFAULT, "__machgate_guest_argc");

	if (machgate_argc)
		return machgate_argc;
	return &argc;
}

char*** _NSGetArgv(void)
{
	static char** argv = NULL;
	char*** machgate_argv = dlsym(RTLD_DEFAULT, "__machgate_guest_argv");

	if (machgate_argv)
		return machgate_argv;
	return &argv;
}

char*** _NSGetEnviron(void)
{
	char*** machgate_envp = dlsym(RTLD_DEFAULT, "__machgate_guest_envp");

	if (machgate_envp && *machgate_envp)
		return machgate_envp;
	return &environ;
}

uint32_t _dyld_image_count(void)
{
	return 1;
}

const void* _dyld_get_image_header(uint32_t image_index)
{
	(void)image_index;
	return NULL;
}

const char* _dyld_get_image_name(uint32_t image_index)
{
	(void)image_index;
	return NULL;
}

intptr_t _dyld_get_image_vmaddr_slide(uint32_t image_index)
{
	(void)image_index;
	return 0;
}

int issetugid(void)
{
	return 0;
}

int CCRandomGenerateBytes(void* bytes, size_t count)
{
	if (!bytes && count)
		return -1;

	size_t offset = 0;
	while (offset < count) {
		long result = syscall(SYS_getrandom, (char*)bytes + offset,
		                      count - offset, 0);
		if (result < 0) {
			if (errno == EINTR)
				continue;
			int fd = libc_open("/dev/urandom", O_RDONLY | O_CLOEXEC, 0);
			if (fd < 0)
				return -1;
			while (offset < count) {
				ssize_t nread = read(fd, (char*)bytes + offset,
				                     count - offset);
				if (nread < 0 && errno == EINTR)
					continue;
				if (nread <= 0) {
					close(fd);
					return -1;
				}
				offset += (size_t)nread;
			}
			close(fd);
			return 0;
		}
		offset += (size_t)result;
	}
	return 0;
}

int CCCryptorCreateWithMode(uint32_t operation, uint32_t mode,
                            uint32_t algorithm, uint32_t padding,
                            const void* iv, const void* key,
                            size_t key_length, const void* tweak,
                            size_t tweak_length, int num_rounds,
                            uint32_t options, void** cryptor_ref)
{
	(void)operation;
	(void)mode;
	(void)algorithm;
	(void)padding;
	(void)iv;
	(void)key;
	(void)key_length;
	(void)tweak;
	(void)tweak_length;
	(void)num_rounds;
	(void)options;

	if (cryptor_ref)
		*cryptor_ref = NULL;
	return -4;
}

int CCCryptorReset(void* cryptor_ref, const void* iv)
{
	(void)cryptor_ref;
	(void)iv;
	return -4;
}

int CCCryptorUpdate(void* cryptor_ref, const void* data_in,
                    size_t data_in_length, void* data_out,
                    size_t data_out_available,
                    size_t* data_out_moved)
{
	(void)cryptor_ref;
	(void)data_in;
	(void)data_in_length;
	(void)data_out;
	(void)data_out_available;

	if (data_out_moved)
		*data_out_moved = 0;
	return -4;
}

void CCHmacInit(void* context, uint32_t algorithm, const void* key,
                size_t key_length)
{
	(void)context;
	(void)algorithm;
	(void)key;
	(void)key_length;
}

void CCHmacUpdate(void* context, const void* data, size_t data_length)
{
	(void)context;
	(void)data;
	(void)data_length;
}

void CCHmacFinal(void* context, void* mac_out)
{
	(void)context;
	if (mac_out)
		memset(mac_out, 0, 64);
}

struct cc_sha256_ctx {
	uint32_t count[2];
	uint32_t hash[8];
	uint32_t wbuf[16];
};

static uint32_t cc_sha256_rotr(uint32_t value, unsigned int bits)
{
	return (value >> bits) | (value << (32 - bits));
}

static uint32_t cc_sha256_load_be32(const uint8_t* bytes)
{
	return ((uint32_t)bytes[0] << 24) |
	       ((uint32_t)bytes[1] << 16) |
	       ((uint32_t)bytes[2] << 8) |
	       (uint32_t)bytes[3];
}

static void cc_sha256_store_be32(uint8_t* bytes, uint32_t value)
{
	bytes[0] = (uint8_t)(value >> 24);
	bytes[1] = (uint8_t)(value >> 16);
	bytes[2] = (uint8_t)(value >> 8);
	bytes[3] = (uint8_t)value;
}

static void cc_sha256_transform(struct cc_sha256_ctx* context,
                                const uint8_t block[64])
{
	static const uint32_t constants[64] = {
		0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
		0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
		0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
		0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
		0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
		0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
		0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
		0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
		0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
		0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
		0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
		0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
		0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
		0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
		0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
		0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
	};

	uint32_t words[64];
	for (int index = 0; index < 16; index++)
		words[index] = cc_sha256_load_be32(block + index * 4);
	for (int index = 16; index < 64; index++) {
		uint32_t s0 = cc_sha256_rotr(words[index - 15], 7) ^
		              cc_sha256_rotr(words[index - 15], 18) ^
		              (words[index - 15] >> 3);
		uint32_t s1 = cc_sha256_rotr(words[index - 2], 17) ^
		              cc_sha256_rotr(words[index - 2], 19) ^
		              (words[index - 2] >> 10);
		words[index] = words[index - 16] + s0 + words[index - 7] + s1;
	}

	uint32_t a = context->hash[0];
	uint32_t b = context->hash[1];
	uint32_t c = context->hash[2];
	uint32_t d = context->hash[3];
	uint32_t e = context->hash[4];
	uint32_t f = context->hash[5];
	uint32_t g = context->hash[6];
	uint32_t h = context->hash[7];

	for (int index = 0; index < 64; index++) {
		uint32_t s1 = cc_sha256_rotr(e, 6) ^ cc_sha256_rotr(e, 11) ^
		              cc_sha256_rotr(e, 25);
		uint32_t choice = (e & f) ^ (~e & g);
		uint32_t temp1 = h + s1 + choice + constants[index] + words[index];
		uint32_t s0 = cc_sha256_rotr(a, 2) ^ cc_sha256_rotr(a, 13) ^
		              cc_sha256_rotr(a, 22);
		uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
		uint32_t temp2 = s0 + majority;

		h = g;
		g = f;
		f = e;
		e = d + temp1;
		d = c;
		c = b;
		b = a;
		a = temp1 + temp2;
	}

	context->hash[0] += a;
	context->hash[1] += b;
	context->hash[2] += c;
	context->hash[3] += d;
	context->hash[4] += e;
	context->hash[5] += f;
	context->hash[6] += g;
	context->hash[7] += h;
}

int CC_SHA256_Init(struct cc_sha256_ctx* context)
{
	if (!context)
		return 0;

	context->count[0] = 0;
	context->count[1] = 0;
	context->hash[0] = 0x6a09e667U;
	context->hash[1] = 0xbb67ae85U;
	context->hash[2] = 0x3c6ef372U;
	context->hash[3] = 0xa54ff53aU;
	context->hash[4] = 0x510e527fU;
	context->hash[5] = 0x9b05688cU;
	context->hash[6] = 0x1f83d9abU;
	context->hash[7] = 0x5be0cd19U;
	memset(context->wbuf, 0, sizeof(context->wbuf));
	return 1;
}

int CC_SHA256_Update(struct cc_sha256_ctx* context, const void* data,
                     uint32_t length)
{
	if (!context)
		return 0;
	if (!data && length)
		return 0;

	const uint8_t* bytes = data;
	uint64_t bit_count = ((uint64_t)context->count[1] << 32) |
	                     context->count[0];
	size_t used = (size_t)((bit_count >> 3) & 63);
	bit_count += (uint64_t)length << 3;
	context->count[0] = (uint32_t)bit_count;
	context->count[1] = (uint32_t)(bit_count >> 32);

	if (used) {
		size_t available = 64 - used;
		if (length < available) {
			memcpy((uint8_t*)context->wbuf + used, bytes, length);
			return 1;
		}
		memcpy((uint8_t*)context->wbuf + used, bytes, available);
		cc_sha256_transform(context, (const uint8_t*)context->wbuf);
		bytes += available;
		length -= (uint32_t)available;
	}

	while (length >= 64) {
		cc_sha256_transform(context, bytes);
		bytes += 64;
		length -= 64;
	}

	if (length)
		memcpy(context->wbuf, bytes, length);
	return 1;
}

int CC_SHA256_Final(uint8_t* digest, struct cc_sha256_ctx* context)
{
	if (!context || !digest)
		return 0;

	uint8_t padding[64] = { 0x80 };
	uint8_t length_bytes[8];
	uint64_t bit_count = ((uint64_t)context->count[1] << 32) |
	                     context->count[0];
	size_t used = (size_t)((bit_count >> 3) & 63);
	size_t pad_length = used < 56 ? 56 - used : 120 - used;

	for (int index = 0; index < 8; index++)
		length_bytes[7 - index] = (uint8_t)(bit_count >> (index * 8));

	CC_SHA256_Update(context, padding, (uint32_t)pad_length);
	CC_SHA256_Update(context, length_bytes, sizeof(length_bytes));

	for (int index = 0; index < 8; index++)
		cc_sha256_store_be32(digest + index * 4, context->hash[index]);
	memset(context, 0, sizeof(*context));
	return 1;
}

int CC_MD5_Init(void* context)
{
	(void)context;
	return 1;
}

int CC_MD5_Update(void* context, const void* data, uint32_t length)
{
	(void)context;
	(void)data;
	(void)length;
	return 1;
}

int CC_MD5_Final(uint8_t* digest, void* context)
{
	(void)context;
	if (!digest)
		return 0;
	memset(digest, 0, 16);
	return 1;
}

int CC_SHA1_Init(void* context)
{
	(void)context;
	return 1;
}

int CC_SHA1_Update(void* context, const void* data, uint32_t length)
{
	(void)context;
	(void)data;
	(void)length;
	return 1;
}

int CC_SHA1_Final(uint8_t* digest, void* context)
{
	(void)context;
	if (!digest)
		return 0;
	memset(digest, 0, 20);
	return 1;
}

int CC_SHA384_Init(void* context)
{
	(void)context;
	return 1;
}

int CC_SHA384_Update(void* context, const void* data, uint32_t length)
{
	(void)context;
	(void)data;
	(void)length;
	return 1;
}

int CC_SHA384_Final(uint8_t* digest, void* context)
{
	(void)context;
	if (!digest)
		return 0;
	memset(digest, 0, 48);
	return 1;
}

int CC_SHA512_Init(void* context)
{
	(void)context;
	return 1;
}

int CC_SHA512_Update(void* context, const void* data, uint32_t length)
{
	(void)context;
	(void)data;
	(void)length;
	return 1;
}

int CC_SHA512_Final(uint8_t* digest, void* context)
{
	(void)context;
	if (!digest)
		return 0;
	memset(digest, 0, 64);
	return 1;
}

int CCKeyDerivationPBKDF(uint32_t algorithm, const char* password,
                         size_t password_length, const uint8_t* salt,
                         size_t salt_length, uint32_t prf,
                         uint32_t rounds, uint8_t* derived_key,
                         size_t derived_key_length)
{
	(void)algorithm;
	(void)password;
	(void)password_length;
	(void)salt;
	(void)salt_length;
	(void)prf;
	(void)rounds;

	if (derived_key && derived_key_length)
		memset(derived_key, 0, derived_key_length);
	return -4;
}

void sys_icache_invalidate(void* start, size_t len)
{
	if (!start || len == 0)
		return;
	__builtin___clear_cache((char*)start, (char*)start + len);
}

typedef int64_t CFIndex;
typedef uint32_t CFStringEncoding;
typedef const void* CFAllocatorRef;
typedef const void* CFArrayRef;
typedef void* CFMutableArrayRef;
typedef const void* CFDataRef;
typedef const void* CFErrorRef;
typedef const void* CFStringRef;
typedef const void* CFTimeZoneRef;
typedef const void* CFDictionaryRef;
typedef void* CFMutableDictionaryRef;
typedef double CFAbsoluteTime;
typedef unsigned long CFTypeID;
typedef const void* SecCertificateRef;
typedef const void* SecPolicyRef;
typedef void* SecTrustRef;
typedef int32_t OSStatus;
typedef double CFTimeInterval;
typedef void* CFRunLoopRef;
typedef void* FSEventStreamRef;
typedef uint64_t FSEventStreamEventId;
typedef uint32_t FSEventStreamEventFlags;
typedef uint32_t FSEventStreamCreateFlags;
typedef void (*FSEventStreamCallback)(FSEventStreamRef stream,
                                      void* info,
                                      size_t number_of_events,
                                      void* event_paths,
                                      const FSEventStreamEventFlags* event_flags,
                                      const FSEventStreamEventId* event_ids);

enum {
	CF_TYPE_ID_STRING = 1,
	CF_TYPE_ID_ARRAY = 2,
	CF_TYPE_ID_DATA = 3,
	CF_TYPE_ID_DICTIONARY = 4,
	CF_TYPE_ID_BOOLEAN = 5,
	CF_TYPE_ID_NUMBER = 6,
};

struct cf_range {
	CFIndex location;
	CFIndex length;
};

struct cf_array {
	CFTypeID type_id;
	CFIndex count;
	CFIndex capacity;
	const void** values;
};

struct cf_data {
	CFTypeID type_id;
	CFIndex length;
	uint8_t bytes[];
};

struct cf_dictionary {
	CFTypeID type_id;
	CFIndex count;
	const void** keys;
	const void** values;
};

struct cf_number {
	CFTypeID type_id;
	int type;
	int64_t signed_value;
	double double_value;
};

struct cf_array_callbacks {
	CFIndex version;
	const void* (*retain)(CFAllocatorRef allocator, const void* value);
	void (*release)(CFAllocatorRef allocator, const void* value);
	CFStringRef (*copy_description)(const void* value);
	uint8_t (*equal)(const void* value_a, const void* value_b);
};

struct cf_dictionary_key_callbacks {
	CFIndex version;
	const void* (*retain)(CFAllocatorRef allocator, const void* value);
	void (*release)(CFAllocatorRef allocator, const void* value);
	CFStringRef (*copy_description)(const void* value);
	uint8_t (*equal)(const void* value_a, const void* value_b);
	unsigned long (*hash)(const void* value);
};

struct cf_dictionary_value_callbacks {
	CFIndex version;
	const void* (*retain)(CFAllocatorRef allocator, const void* value);
	void (*release)(CFAllocatorRef allocator, const void* value);
	CFStringRef (*copy_description)(const void* value);
	uint8_t (*equal)(const void* value_a, const void* value_b);
};

struct fsevent_stream_context {
	CFIndex version;
	void* info;
	const void* retain;
	const void* release;
	const void* copy_description;
};

struct fsevent_entry {
	char path[PATH_MAX];
	time_t modified_seconds;
	long modified_nanoseconds;
	off_t size;
	int is_directory;
};

struct fsevent_stream {
	FSEventStreamCallback callback;
	struct fsevent_stream_context context;
	CFArrayRef paths;
	uint64_t device;
	FSEventStreamEventId latest_event_id;
	uint8_t started;
	uint8_t invalidated;
	struct fsevent_entry* snapshot;
	size_t snapshot_count;
	size_t snapshot_capacity;
	pthread_mutex_t mutex;
};

CFDataRef CFDataCreate(CFAllocatorRef allocator, const uint8_t* bytes,
                       CFIndex length);
CFDictionaryRef CFDictionaryCreate(CFAllocatorRef allocator,
                                   const void** keys,
                                   const void** values,
                                   CFIndex number_of_values,
                                   const void* key_callbacks,
                                   const void* value_callbacks);

static const void* cf_callback_retain(CFAllocatorRef allocator,
                                      const void* value)
{
	(void)allocator;
	return value;
}

static void cf_callback_release(CFAllocatorRef allocator, const void* value)
{
	(void)allocator;
	(void)value;
}

static CFStringRef cf_callback_copy_description(const void* value)
{
	return value;
}

static uint8_t cf_callback_equal(const void* value_a, const void* value_b)
{
	return value_a == value_b;
}

static unsigned long cf_callback_hash(const void* value)
{
	uintptr_t bits = (uintptr_t)value;
	return (unsigned long)(bits ^ (bits >> 32));
}

static const char cf_timezone_name[] = "UTC";
static const char cf_timezone_object[] = "MachGate/UTC";
static const char cf_error_description[] = "MachGate CoreFoundation error";
static const char cf_allocator_null_object[] = "MachGate/CFAllocatorNull";
static const char cf_boolean_true_object[] = "MachGate/kCFBooleanTrue";
static const char cf_run_loop_default_mode_object[] = "kCFRunLoopDefaultMode";
static const char cf_url_volume_available_capacity_important_key[] = "NSURLVolumeAvailableCapacityForImportantUsageKey";
static const char cf_url_volume_available_capacity_key[] = "NSURLVolumeAvailableCapacityKey";
static const char cf_url_volume_is_browsable_key[] = "NSURLVolumeIsBrowsableKey";
static const char cf_url_volume_is_ejectable_key[] = "NSURLVolumeIsEjectableKey";
static const char cf_url_volume_is_internal_key[] = "NSURLVolumeIsInternalKey";
static const char cf_url_volume_is_local_key[] = "NSURLVolumeIsLocalKey";
static const char cf_url_volume_is_removable_key[] = "NSURLVolumeIsRemovableKey";
static const char cf_url_volume_name_key[] = "NSURLVolumeNameKey";
static const char cf_url_volume_total_capacity_key[] = "NSURLVolumeTotalCapacityKey";
static const char cf_url_volume_uuid_string_key[] = "NSURLVolumeUUIDStringKey";
static const char security_class_key[] = "kSecClass";
static const char security_class_certificate_object[] = "kSecClassCertificate";
static const char security_match_limit_key[] = "kSecMatchLimit";
static const char security_match_limit_all_object[] = "kSecMatchLimitAll";
static const char security_policy_apple_ssl_object[] = "kSecPolicyAppleSSL";
static const char security_policy_oid_key[] = "kSecPolicyOid";
static const char security_policy_ssl_object[] = "MachGate/SecPolicySSL";
static const char security_return_ref_key[] = "kSecReturnRef";
static const char security_trust_object[] = "MachGate/SecTrust";

const double kCFAbsoluteTimeIntervalSince1970 = 978307200.0;
const void* kCFAllocatorDefault = NULL;
const void* kCFAllocatorNull = cf_allocator_null_object;
const void* kCFBooleanTrue = cf_boolean_true_object;
const void* kCFRunLoopDefaultMode = cf_run_loop_default_mode_object;
const void* kCFURLVolumeAvailableCapacityForImportantUsageKey = cf_url_volume_available_capacity_important_key;
const void* kCFURLVolumeAvailableCapacityKey = cf_url_volume_available_capacity_key;
const void* kCFURLVolumeIsBrowsableKey = cf_url_volume_is_browsable_key;
const void* kCFURLVolumeIsEjectableKey = cf_url_volume_is_ejectable_key;
const void* kCFURLVolumeIsInternalKey = cf_url_volume_is_internal_key;
const void* kCFURLVolumeIsLocalKey = cf_url_volume_is_local_key;
const void* kCFURLVolumeIsRemovableKey = cf_url_volume_is_removable_key;
const void* kCFURLVolumeNameKey = cf_url_volume_name_key;
const void* kCFURLVolumeTotalCapacityKey = cf_url_volume_total_capacity_key;
const void* kCFURLVolumeUUIDStringKey = cf_url_volume_uuid_string_key;
const void* kSecClass = security_class_key;
const void* kSecClassCertificate = security_class_certificate_object;
const void* kSecMatchLimit = security_match_limit_key;
const void* kSecMatchLimitAll = security_match_limit_all_object;
const void* kSecPolicyAppleSSL = security_policy_apple_ssl_object;
const void* kSecPolicyOid = security_policy_oid_key;
const void* kSecReturnRef = security_return_ref_key;
const char __CFConstantStringClassReference[] = "__CFConstantStringClassReference";
const struct cf_array_callbacks kCFTypeArrayCallBacks = {
	0,
	cf_callback_retain,
	cf_callback_release,
	cf_callback_copy_description,
	cf_callback_equal,
};
const struct cf_dictionary_key_callbacks kCFTypeDictionaryKeyCallBacks = {
	0,
	cf_callback_retain,
	cf_callback_release,
	cf_callback_copy_description,
	cf_callback_equal,
	cf_callback_hash,
};
const struct cf_dictionary_value_callbacks kCFTypeDictionaryValueCallBacks = {
	0,
	cf_callback_retain,
	cf_callback_release,
	cf_callback_copy_description,
	cf_callback_equal,
};

CFTimeZoneRef CFTimeZoneCopySystem(void)
{
	return cf_timezone_object;
}

CFTimeZoneRef CFTimeZoneCopyDefault(void)
{
	return CFTimeZoneCopySystem();
}

void CFTimeZoneResetSystem(void)
{
}

CFStringRef CFTimeZoneGetName(CFTimeZoneRef tz)
{
	(void)tz;
	return cf_timezone_name;
}

CFIndex CFStringGetLength(CFStringRef string)
{
	if (!string)
		return 0;
	return (CFIndex)strlen((const char*)string);
}

CFStringEncoding CFStringGetSystemEncoding(void)
{
	return 0;
}

const char* CFStringGetCStringPtr(CFStringRef string, CFStringEncoding encoding)
{
	(void)encoding;
	return (const char*)string;
}

uint8_t CFStringGetCString(CFStringRef string, char* buffer,
                           CFIndex buffer_size, CFStringEncoding encoding)
{
	(void)encoding;

	if (!string || !buffer || buffer_size <= 0)
		return 0;

	snprintf(buffer, (size_t)buffer_size, "%s", (const char*)string);
	return 1;
}

CFIndex CFStringGetMaximumSizeForEncoding(CFIndex length,
                                          CFStringEncoding encoding)
{
	(void)encoding;

	if (length < 0)
		return 0;
	return length * 4 + 1;
}

CFIndex CFStringGetBytes(CFStringRef string, struct cf_range range,
                         CFStringEncoding encoding, uint8_t loss_byte,
                         uint8_t is_external_representation, uint8_t* buffer,
                         CFIndex max_buffer_length, CFIndex* used_buffer_length)
{
	(void)encoding;
	(void)loss_byte;
	(void)is_external_representation;

	if (!string) {
		if (used_buffer_length)
			*used_buffer_length = 0;
		return 0;
	}

	const char* source = (const char*)string;
	CFIndex source_length = (CFIndex)strlen(source);
	if (range.location < 0 || range.location > source_length) {
		if (used_buffer_length)
			*used_buffer_length = 0;
		return 0;
	}

	CFIndex available = source_length - range.location;
	CFIndex requested = range.length < available ? range.length : available;
	CFIndex copied = requested < max_buffer_length ? requested : max_buffer_length;
	if (buffer && copied > 0)
		memcpy(buffer, source + range.location, (size_t)copied);
	if (used_buffer_length)
		*used_buffer_length = copied;
	return copied;
}

CFStringRef CFStringCreateWithBytes(CFAllocatorRef allocator,
                                    const uint8_t* bytes,
                                    CFIndex number_of_bytes,
                                    CFStringEncoding encoding,
                                    uint8_t is_external_representation)
{
	(void)allocator;
	(void)encoding;
	(void)is_external_representation;

	if (!bytes && number_of_bytes > 0)
		return NULL;

	if (number_of_bytes < 0)
		return NULL;

	char* string = calloc((size_t)number_of_bytes + 1, 1);
	if (!string)
		return NULL;

	if (number_of_bytes > 0)
		memcpy(string, bytes, (size_t)number_of_bytes);
	return string;
}

CFStringRef CFStringCreateWithBytesNoCopy(CFAllocatorRef allocator,
                                          const uint8_t* bytes,
                                          CFIndex number_of_bytes,
                                          CFStringEncoding encoding,
                                          uint8_t is_external_representation,
                                          CFAllocatorRef contents_deallocator)
{
	(void)contents_deallocator;
	return CFStringCreateWithBytes(allocator, bytes, number_of_bytes,
	                               encoding, is_external_representation);
}

CFStringRef CFStringCreateWithCString(CFAllocatorRef allocator,
                                      const char* string,
                                      CFStringEncoding encoding)
{
	if (!string)
		return NULL;
	return CFStringCreateWithBytes(allocator, (const uint8_t*)string,
	                               (CFIndex)strlen(string), encoding, 0);
}

CFStringRef CFStringCreateCopy(CFAllocatorRef allocator, CFStringRef string)
{
	return CFStringCreateWithCString(allocator, (const char*)string, 0);
}

int CFStringCompare(CFStringRef string_a, CFStringRef string_b,
                    unsigned long options)
{
	(void)options;

	if (string_a == string_b)
		return 0;
	if (!string_a)
		return -1;
	if (!string_b)
		return 1;

	int result = strcmp((const char*)string_a, (const char*)string_b);
	if (result < 0)
		return -1;
	if (result > 0)
		return 1;
	return 0;
}

CFStringRef CFURLCreateFromFileSystemRepresentation(CFAllocatorRef allocator,
                                                    const uint8_t* buffer,
                                                    CFIndex buffer_length,
                                                    uint8_t is_directory)
{
	(void)is_directory;
	return CFStringCreateWithBytes(allocator, buffer, buffer_length, 0, 0);
}

CFStringRef CFURLCreateWithFileSystemPath(CFAllocatorRef allocator,
                                          CFStringRef file_path,
                                          int path_style,
                                          uint8_t is_directory)
{
	(void)path_style;
	(void)is_directory;
	return CFStringCreateCopy(allocator, file_path);
}

uint8_t CFURLGetFileSystemRepresentation(CFStringRef url,
                                          uint8_t resolve_against_base,
                                          uint8_t* buffer,
                                          CFIndex max_buffer_length)
{
	(void)resolve_against_base;

	if (!url || !buffer || max_buffer_length <= 0)
		return 0;

	snprintf((char*)buffer, (size_t)max_buffer_length, "%s",
	         (const char*)url);
	return 1;
}

CFStringRef CFURLCreateFilePathURL(CFAllocatorRef allocator, CFStringRef url,
                                   void* error)
{
	(void)error;
	return CFStringCreateCopy(allocator, url);
}

CFStringRef CFURLCreateFileReferenceURL(CFAllocatorRef allocator,
                                        CFStringRef url, void* error)
{
	(void)error;
	return CFStringCreateCopy(allocator, url);
}

CFStringRef CFURLCopyAbsoluteURL(CFStringRef url)
{
	return CFStringCreateCopy(NULL, url);
}

CFStringRef CFURLCopyFileSystemPath(CFStringRef url, int path_style)
{
	(void)path_style;
	return CFStringCreateCopy(NULL, url);
}

CFStringRef CFURLCopyLastPathComponent(CFStringRef url)
{
	if (!url)
		return NULL;

	const char* path = (const char*)url;
	const char* last_slash = strrchr(path, '/');
	const char* component = last_slash ? last_slash + 1 : path;
	return CFStringCreateWithCString(NULL, component, 0);
}

CFStringRef CFURLCreateCopyAppendingPathComponent(CFAllocatorRef allocator,
                                                  CFStringRef url,
                                                  CFStringRef path_component,
                                                  uint8_t is_directory)
{
	(void)is_directory;

	if (!url)
		return CFStringCreateCopy(allocator, path_component);
	if (!path_component)
		return CFStringCreateCopy(allocator, url);

	const char* base = (const char*)url;
	const char* component = (const char*)path_component;
	size_t base_length = strlen(base);
	size_t component_length = strlen(component);
	int needs_slash = base_length > 0 && base[base_length - 1] != '/';
	char* path = malloc(base_length + (size_t)needs_slash +
	                    component_length + 1);
	if (!path)
		return NULL;

	memcpy(path, base, base_length);
	if (needs_slash)
		path[base_length++] = '/';
	memcpy(path + base_length, component, component_length);
	path[base_length + component_length] = '\0';
	return path;
}

CFStringRef CFURLCreateCopyDeletingLastPathComponent(CFAllocatorRef allocator,
                                                     CFStringRef url)
{
	(void)allocator;

	if (!url)
		return NULL;

	const char* path = (const char*)url;
	const char* last_slash = strrchr(path, '/');
	if (!last_slash)
		return CFStringCreateWithCString(NULL, "", 0);
	if (last_slash == path)
		return CFStringCreateWithCString(NULL, "/", 0);

	return CFStringCreateWithBytes(NULL, (const uint8_t*)path,
	                               (CFIndex)(last_slash - path), 0, 0);
}

uint8_t CFURLResourceIsReachable(CFStringRef url, void* error)
{
	(void)error;

	if (!url)
		return 0;
	return access((const char*)url, F_OK) == 0;
}

CFDictionaryRef CFURLCopyResourcePropertiesForKeys(CFStringRef url,
                                                   CFArrayRef keys,
                                                   void* error)
{
	(void)url;
	(void)keys;
	(void)error;
	return CFDictionaryCreate(NULL, NULL, NULL, 0,
	                          &kCFTypeDictionaryKeyCallBacks,
	                          &kCFTypeDictionaryValueCallBacks);
}

CFStringRef CFBundleCreate(CFAllocatorRef allocator, CFStringRef bundle_url)
{
	return CFStringCreateCopy(allocator, bundle_url);
}

CFStringRef CFBundleCopyExecutableURL(CFStringRef bundle)
{
	return CFStringCreateCopy(NULL, bundle);
}

static void shim_fill_random_bytes(void* buffer, size_t length)
{
	unsigned char* bytes = buffer;
	size_t offset = 0;

	while (offset < length) {
		long result = syscall(SYS_getrandom, bytes + offset, length - offset, 0);
		if (result > 0) {
			offset += (size_t)result;
			continue;
		}
		if (result < 0 && errno == EINTR)
			continue;
		int fd = libc_open("/dev/urandom", O_RDONLY | O_CLOEXEC, 0);
		if (fd < 0)
			abort();
		while (offset < length) {
			ssize_t nread = read(fd, bytes + offset, length - offset);
			if (nread < 0 && errno == EINTR)
				continue;
			if (nread <= 0)
				abort();
			offset += (size_t)nread;
		}
		close(fd);
		return;
	}
}

static void shim_format_random_uuid(char output[37])
{
	unsigned char raw[16];

	shim_fill_random_bytes(raw, sizeof(raw));
	raw[6] = (unsigned char)((raw[6] & 0x0f) | 0x40);
	raw[8] = (unsigned char)((raw[8] & 0x3f) | 0x80);
	static const char hex_digits[] = "0123456789abcdef";
	size_t position = 0;
	for (size_t byte_index = 0; byte_index < sizeof(raw); byte_index++) {
		if (byte_index == 4 || byte_index == 6 || byte_index == 8 ||
		    byte_index == 10)
			output[position++] = '-';
		output[position++] = hex_digits[raw[byte_index] >> 4];
		output[position++] = hex_digits[raw[byte_index] & 0x0f];
	}
	output[position] = '\0';
}

CFStringRef CFUUIDCreate(CFAllocatorRef allocator)
{
	char uuid_string[37];

	shim_format_random_uuid(uuid_string);
	return CFStringCreateWithCString(allocator, uuid_string, 0);
}

CFStringRef CFUUIDCreateString(CFAllocatorRef allocator, CFStringRef uuid)
{
	if (uuid)
		return CFStringCreateCopy(allocator, uuid);
	return CFStringCreateWithCString(allocator,
	                                 "00000000-0000-0000-0000-000000000000",
	                                 0);
}

OSStatus LSOpenCFURLRef(CFStringRef url, CFStringRef* launched_url)
{
	if (launched_url)
		*launched_url = url ? CFStringCreateCopy(NULL, url) : NULL;
	return 0;
}

CFDataRef CFStringCreateExternalRepresentation(CFAllocatorRef allocator,
                                               CFStringRef string,
                                               CFStringEncoding encoding,
                                               uint8_t loss_byte)
{
	(void)allocator;
	(void)encoding;
	(void)loss_byte;

	if (!string)
		return NULL;

	const char* bytes = (const char*)string;
	CFIndex length = (CFIndex)strlen(bytes);
	return CFDataCreate(NULL, (const uint8_t*)bytes, length);
}

CFDataRef CFDataCreate(CFAllocatorRef allocator, const uint8_t* bytes,
                       CFIndex length)
{
	(void)allocator;

	if (!bytes && length > 0)
		return NULL;

	if (length < 0)
		return NULL;

	struct cf_data* data = malloc(sizeof(*data) + (size_t)length);
	if (!data)
		return NULL;

	data->type_id = CF_TYPE_ID_DATA;
	data->length = length;
	if (length > 0)
		memcpy(data->bytes, bytes, (size_t)length);
	return data;
}

const uint8_t* CFDataGetBytePtr(CFDataRef data)
{
	if (!data)
		return NULL;
	return ((const struct cf_data*)data)->bytes;
}

CFIndex CFDataGetLength(CFDataRef data)
{
	if (!data)
		return 0;
	return ((const struct cf_data*)data)->length;
}

void CFDataGetBytes(CFDataRef data, struct cf_range range, uint8_t* buffer)
{
	if (!data || !buffer)
		return;
	if (range.location < 0 || range.length < 0)
		return;

	const struct cf_data* cf_data = data;
	if (range.location > cf_data->length)
		return;

	CFIndex available = cf_data->length - range.location;
	CFIndex copied = range.length < available ? range.length : available;
	if (copied > 0)
		memcpy(buffer, cf_data->bytes + range.location, (size_t)copied);
}

CFMutableArrayRef CFArrayCreateMutable(CFAllocatorRef allocator,
                                       CFIndex capacity,
                                       const void* callbacks)
{
	(void)allocator;
	(void)callbacks;

	if (capacity < 0)
		return NULL;

	struct cf_array* array = calloc(1, sizeof(*array));
	if (!array)
		return NULL;

	array->type_id = CF_TYPE_ID_ARRAY;
	array->capacity = capacity > 0 ? capacity : 4;
	array->values = calloc((size_t)array->capacity, sizeof(*array->values));
	if (!array->values) {
		free(array);
		return NULL;
	}
	return array;
}

CFArrayRef CFArrayCreate(CFAllocatorRef allocator, const void** values,
                         CFIndex number_of_values, const void* callbacks)
{
	CFMutableArrayRef array_ref = CFArrayCreateMutable(allocator,
	                                                  number_of_values,
	                                                  callbacks);
	struct cf_array* array = array_ref;
	if (!array)
		return NULL;

	for (CFIndex index = 0; index < number_of_values; index++)
		array->values[index] = values ? values[index] : NULL;
	array->count = number_of_values;
	return array;
}

void CFArrayAppendValue(CFMutableArrayRef array_ref, const void* value)
{
	struct cf_array* array = array_ref;
	if (!array)
		return;

	if (array->count == array->capacity) {
		CFIndex new_capacity = array->capacity * 2;
		const void** values = realloc(array->values,
		                              (size_t)new_capacity *
		                              sizeof(*array->values));
		if (!values)
			return;
		array->values = values;
		array->capacity = new_capacity;
	}

	array->values[array->count] = value;
	array->count++;
}

void CFArrayInsertValueAtIndex(CFMutableArrayRef array_ref, CFIndex index,
                               const void* value)
{
	struct cf_array* array = array_ref;
	if (!array || index < 0 || index > array->count)
		return;

	if (array->count == array->capacity) {
		CFIndex new_capacity = array->capacity * 2;
		const void** values = realloc(array->values,
		                              (size_t)new_capacity *
		                              sizeof(*array->values));
		if (!values)
			return;
		array->values = values;
		array->capacity = new_capacity;
	}

	memmove(array->values + index + 1, array->values + index,
	        (size_t)(array->count - index) * sizeof(*array->values));
	array->values[index] = value;
	array->count++;
}

void CFArrayRemoveValueAtIndex(CFMutableArrayRef array_ref, CFIndex index)
{
	struct cf_array* array = array_ref;
	if (!array || index < 0 || index >= array->count)
		return;

	memmove(array->values + index, array->values + index + 1,
	        (size_t)(array->count - index - 1) * sizeof(*array->values));
	array->count--;
}

void CFArraySetValueAtIndex(CFMutableArrayRef array_ref, CFIndex index,
                            const void* value)
{
	struct cf_array* array = array_ref;
	if (!array || index < 0)
		return;

	while (index >= array->capacity) {
		CFIndex new_capacity = array->capacity * 2;
		const void** values = realloc(array->values,
		                              (size_t)new_capacity *
		                              sizeof(*array->values));
		if (!values)
			return;
		memset(values + array->capacity, 0,
		       (size_t)(new_capacity - array->capacity) *
		       sizeof(*array->values));
		array->values = values;
		array->capacity = new_capacity;
	}

	array->values[index] = value;
	if (index >= array->count)
		array->count = index + 1;
}

CFIndex CFArrayGetCount(CFArrayRef array_ref)
{
	const struct cf_array* array = array_ref;
	if (!array)
		return 0;
	return array->count;
}

const void* CFArrayGetValueAtIndex(CFArrayRef array_ref, CFIndex index)
{
	const struct cf_array* array = array_ref;
	if (!array || index < 0 || index >= array->count)
		return NULL;
	return array->values[index];
}

uint8_t CFEqual(const void* object_a, const void* object_b)
{
	return object_a == object_b;
}

CFDictionaryRef CFDictionaryCreate(CFAllocatorRef allocator,
                                   const void** keys,
                                   const void** values,
                                   CFIndex number_of_values,
                                   const void* key_callbacks,
                                   const void* value_callbacks)
{
	(void)allocator;
	(void)key_callbacks;
	(void)value_callbacks;

	if (number_of_values < 0)
		return NULL;

	struct cf_dictionary* dictionary = calloc(1, sizeof(*dictionary));
	if (!dictionary)
		return NULL;

	dictionary->type_id = CF_TYPE_ID_DICTIONARY;
	dictionary->count = number_of_values;
	if (number_of_values == 0)
		return dictionary;

	dictionary->keys = calloc((size_t)number_of_values,
	                          sizeof(*dictionary->keys));
	dictionary->values = calloc((size_t)number_of_values,
	                            sizeof(*dictionary->values));
	if (!dictionary->keys || !dictionary->values) {
		free(dictionary->keys);
		free(dictionary->values);
		free(dictionary);
		return NULL;
	}

	for (CFIndex index = 0; index < number_of_values; index++) {
		dictionary->keys[index] = keys ? keys[index] : NULL;
		dictionary->values[index] = values ? values[index] : NULL;
	}
	return dictionary;
}

uint8_t CFDictionaryContainsKey(CFDictionaryRef dictionary_ref,
                                const void* key)
{
	const struct cf_dictionary* dictionary = dictionary_ref;
	if (!dictionary)
		return 0;

	for (CFIndex index = 0; index < dictionary->count; index++) {
		if (CFEqual(dictionary->keys[index], key))
			return 1;
	}
	return 0;
}

const void* CFDictionaryGetValue(CFDictionaryRef dictionary_ref,
                                 const void* key)
{
	const struct cf_dictionary* dictionary = dictionary_ref;
	if (!dictionary)
		return NULL;

	for (CFIndex index = 0; index < dictionary->count; index++) {
		if (CFEqual(dictionary->keys[index], key))
			return dictionary->values[index];
	}
	return NULL;
}

void CFDictionaryAddValue(CFMutableDictionaryRef dictionary_ref,
                          const void* key, const void* value)
{
	struct cf_dictionary* dictionary = dictionary_ref;
	if (!dictionary)
		return;

	for (CFIndex index = 0; index < dictionary->count; index++) {
		if (CFEqual(dictionary->keys[index], key)) {
			dictionary->values[index] = value;
			return;
		}
	}

	CFIndex new_count = dictionary->count + 1;
	const void** keys = realloc(dictionary->keys,
	                            (size_t)new_count * sizeof(*keys));
	if (!keys)
		return;
	dictionary->keys = keys;

	const void** values = realloc(dictionary->values,
	                              (size_t)new_count * sizeof(*values));
	if (!values)
		return;
	dictionary->values = values;

	dictionary->keys[dictionary->count] = key;
	dictionary->values[dictionary->count] = value;
	dictionary->count = new_count;
}

uint8_t CFDictionaryGetValueIfPresent(CFDictionaryRef dictionary_ref,
                                      const void* key, const void** value)
{
	const void* result = CFDictionaryGetValue(dictionary_ref, key);
	if (!result)
		return 0;
	if (value)
		*value = result;
	return 1;
}

SecCertificateRef SecCertificateCreateWithData(CFAllocatorRef allocator,
                                               CFDataRef data)
{
	(void)allocator;
	return data;
}

CFDataRef SecCertificateCopyData(SecCertificateRef certificate)
{
	return certificate;
}

CFStringRef SecCertificateCopySubjectSummary(SecCertificateRef certificate)
{
	(void)certificate;
	return CFStringCreateWithCString(NULL, "MachGate Certificate", 0);
}

CFStringRef SecCopyErrorMessageString(OSStatus status, void* reserved)
{
	(void)status;
	(void)reserved;
	return CFStringCreateWithCString(NULL, "Security operation failed", 0);
}

SecPolicyRef SecPolicyCreateSSL(uint8_t server, CFStringRef hostname)
{
	(void)server;
	(void)hostname;
	return security_policy_ssl_object;
}

OSStatus SecTrustCreateWithCertificates(const void* certificates,
                                        SecPolicyRef policies,
                                        SecTrustRef* trust)
{
	(void)certificates;
	(void)policies;

	if (!trust)
		return -50;
	*trust = (SecTrustRef)security_trust_object;
	return 0;
}

OSStatus SecTrustSetAnchorCertificates(SecTrustRef trust,
                                       CFArrayRef anchor_certificates)
{
	(void)trust;
	(void)anchor_certificates;
	return 0;
}

OSStatus SecTrustSetAnchorCertificatesOnly(SecTrustRef trust,
                                           uint8_t anchor_certificates_only)
{
	(void)trust;
	(void)anchor_certificates_only;
	return 0;
}

OSStatus SecTrustSetOCSPResponse(SecTrustRef trust, const void* response)
{
	(void)trust;
	(void)response;
	return 0;
}

OSStatus SecTrustSetVerifyDate(SecTrustRef trust, const void* verify_date)
{
	(void)trust;
	(void)verify_date;
	return 0;
}

uint8_t SecTrustEvaluateWithError(SecTrustRef trust, CFErrorRef* error)
{
	(void)trust;

	if (error)
		*error = NULL;
	return 0;
}

CFArrayRef SecTrustCopyCertificateChain(SecTrustRef trust)
{
	(void)trust;
	return CFArrayCreate(NULL, NULL, 0, &kCFTypeArrayCallBacks);
}

OSStatus SecItemCopyMatching(CFDictionaryRef query, const void** result)
{
	(void)query;

	if (result)
		*result = NULL;
	return -25300;
}

CFDictionaryRef SecPolicyCopyProperties(SecPolicyRef policy)
{
	(void)policy;
	return CFDictionaryCreate(NULL, NULL, NULL, 0, NULL, NULL);
}

OSStatus SecTrustSettingsCopyTrustSettings(SecCertificateRef certificate,
                                           int domain,
                                           CFArrayRef* trust_settings)
{
	(void)certificate;
	(void)domain;

	if (trust_settings)
		*trust_settings = NULL;
	return -25300;
}

int res_9_ninit(void* state)
{
	(void)state;
	return -1;
}

void res_9_nclose(void* state)
{
	(void)state;
}

int res_9_nsearch(void* state, const char* name, int dns_class,
                  int type, uint8_t* answer, int answer_length)
{
	(void)state;
	(void)name;
	(void)dns_class;
	(void)type;
	(void)answer;
	(void)answer_length;
	return -1;
}

enum {
	DARWIN_FSEVENT_FLAG_MUST_SCAN_SUBDIRS = 0x00000008,
	DARWIN_FSEVENT_FLAG_ROOT_CHANGED = 0x00000020,
	DARWIN_FSEVENT_FLAG_ITEM_CREATED = 0x00000100,
	DARWIN_FSEVENT_FLAG_ITEM_REMOVED = 0x00000200,
	DARWIN_FSEVENT_FLAG_ITEM_RENAMED = 0x00000800,
	DARWIN_FSEVENT_FLAG_ITEM_MODIFIED = 0x00001000,
};

static int shim_fsevent_collect_directory(const char* directory,
                                          struct fsevent_entry** entries,
                                          size_t* entry_count,
                                          size_t* entry_capacity)
{
	DIR* dir_stream = opendir(directory);

	if (!dir_stream)
		return 0;

	struct dirent* entry;
	while ((entry = readdir(dir_stream)) != NULL) {
		if (strcmp(entry->d_name, ".") == 0 ||
		    strcmp(entry->d_name, "..") == 0)
			continue;

		char child_path[PATH_MAX];
		if (snprintf(child_path, sizeof(child_path), "%s/%s",
		             directory, entry->d_name) >= (int)sizeof(child_path))
			continue;

		struct stat file_stat;
		if (stat(child_path, &file_stat) != 0)
			continue;

		if (*entry_count == *entry_capacity) {
			size_t new_capacity = *entry_capacity * 2 + 16;
			struct fsevent_entry* reallocated =
				realloc(*entries, new_capacity * sizeof(**entries));
			if (!reallocated) {
				closedir(dir_stream);
				return -1;
			}
			*entries = reallocated;
			*entry_capacity = new_capacity;
		}

		struct fsevent_entry* slot =
			&(*entries)[(*entry_count)++];
		snprintf(slot->path, sizeof(slot->path), "%s", child_path);
		slot->modified_seconds = file_stat.st_mtim.tv_sec;
		slot->modified_nanoseconds = file_stat.st_mtim.tv_nsec;
		slot->size = file_stat.st_size;
		slot->is_directory = S_ISDIR(file_stat.st_mode);

		if (slot->is_directory)
			shim_fsevent_collect_directory(child_path, entries,
			                               entry_count, entry_capacity);
	}
	closedir(dir_stream);
	return 0;
}

static int shim_fsevent_snapshot(struct fsevent_stream* stream)
{
	const struct cf_array* paths = stream->paths;
	struct fsevent_entry* entries = NULL;
	size_t entry_count = 0;
	size_t entry_capacity = 0;

	if (!paths)
		return -1;

	for (CFIndex index = 0; index < paths->count; index++) {
		const char* watch_path = paths->values[index];
		struct stat file_stat;

		if (!watch_path || stat(watch_path, &file_stat) != 0)
			continue;

		if (entry_count == entry_capacity) {
			size_t new_capacity = entry_capacity * 2 + 16;
			struct fsevent_entry* reallocated =
				realloc(entries, new_capacity * sizeof(*entries));
			if (!reallocated) {
				free(entries);
				return -1;
			}
			entries = reallocated;
			entry_capacity = new_capacity;
		}
		struct fsevent_entry* slot = &entries[entry_count++];
		snprintf(slot->path, sizeof(slot->path), "%s", watch_path);
		slot->modified_seconds = file_stat.st_mtim.tv_sec;
		slot->modified_nanoseconds = file_stat.st_mtim.tv_nsec;
		slot->size = file_stat.st_size;
		slot->is_directory = S_ISDIR(file_stat.st_mode);

		if (slot->is_directory &&
		    shim_fsevent_collect_directory(watch_path, &entries,
		                                   &entry_count,
		                                   &entry_capacity) != 0)
			return -1;
	}

	free(stream->snapshot);
	stream->snapshot = entries;
	stream->snapshot_count = entry_count;
	return 0;
}

static int shim_fsevent_entry_compare(const void* left, const void* right)
{
	return strcmp(((const struct fsevent_entry*)left)->path,
	              ((const struct fsevent_entry*)right)->path);
}

static void shim_fsevent_invoke(struct fsevent_stream* stream,
                                char* const* event_paths,
                                const uint32_t* event_flags,
                                size_t event_count)
{
	if (!stream->callback || event_count == 0)
		return;

	uint64_t* event_ids = calloc(event_count, sizeof(*event_ids));
	if (!event_ids)
		return;

	stream->callback(stream, stream->context.info, event_count,
	                 (void*)event_paths, event_flags, event_ids);
	free(event_ids);
}

static int shim_fsevent_is_watch_root(const struct fsevent_stream* stream,
                                       const char* path)
{
	const struct cf_array* paths = stream->paths;
	struct stat file_stat;

	if (!paths || stat(path, &file_stat) != 0 || !S_ISDIR(file_stat.st_mode))
		return 0;

	for (CFIndex index = 0; index < paths->count; index++)
		if (paths->values[index] &&
		    strcmp(paths->values[index], path) == 0)
			return 1;
	return 0;
}

static void shim_fsevent_diff(struct fsevent_stream* stream)
{
	struct fsevent_entry* current = NULL;
	size_t current_count = 0;

	struct fsevent_stream probe;
	probe = *stream;
	probe.snapshot = NULL;
	probe.snapshot_count = 0;
	if (shim_fsevent_snapshot(&probe) != 0)
		return;
	current = probe.snapshot;
	current_count = probe.snapshot_count;

	qsort(stream->snapshot, stream->snapshot_count,
	      sizeof(*stream->snapshot), shim_fsevent_entry_compare);
	qsort(current, current_count, sizeof(*current),
	      shim_fsevent_entry_compare);

	char** event_paths = NULL;
	uint32_t* event_flags = NULL;
	size_t event_count = 0;

	size_t old_index = 0;
	size_t new_index = 0;
	while (old_index < stream->snapshot_count ||
	       new_index < current_count) {
		int comparison;
		if (old_index >= stream->snapshot_count)
			comparison = 1;
		else if (new_index >= current_count)
			comparison = -1;
		else
			comparison = strcmp(stream->snapshot[old_index].path,
			                    current[new_index].path);

		uint32_t flags = 0;
		const char* path = NULL;

		if (comparison < 0) {
			path = stream->snapshot[old_index].path;
			flags = DARWIN_FSEVENT_FLAG_ITEM_REMOVED;
			old_index++;
		} else if (comparison > 0) {
			path = current[new_index].path;
			flags = DARWIN_FSEVENT_FLAG_ITEM_CREATED |
			        DARWIN_FSEVENT_FLAG_ITEM_MODIFIED;
			new_index++;
		} else {
			const struct fsevent_entry* before =
				&stream->snapshot[old_index];
			const struct fsevent_entry* after = &current[new_index];
			if (before->modified_seconds != after->modified_seconds ||
			    before->modified_nanoseconds != after->modified_nanoseconds ||
			    before->size != after->size) {
				path = after->path;
				flags = DARWIN_FSEVENT_FLAG_ITEM_MODIFIED;
			}
			old_index++;
			new_index++;
		}

		if (!path || shim_fsevent_is_watch_root(stream, path))
			continue;

		char** reallocated_paths =
			realloc(event_paths, (event_count + 1) * sizeof(*event_paths));
		uint32_t* reallocated_flags =
			realloc(event_flags, (event_count + 1) * sizeof(*event_flags));
		if (!reallocated_paths || !reallocated_flags) {
			free(reallocated_paths ? reallocated_paths : event_paths);
			free(reallocated_flags ? reallocated_flags : event_flags);
			free(current);
			return;
		}
		event_paths = reallocated_paths;
		event_flags = reallocated_flags;
		event_paths[event_count] = (char*)path;
		event_flags[event_count] = flags;
		event_count++;
	}

	shim_fsevent_invoke(stream, event_paths, event_flags, event_count);
	free(event_paths);
	free(event_flags);

	free(stream->snapshot);
	stream->snapshot = current;
	stream->snapshot_count = current_count;
}

FSEventStreamRef FSEventStreamCreate(CFAllocatorRef allocator,
                                     FSEventStreamCallback callback,
                                     const struct fsevent_stream_context* context,
                                     CFArrayRef paths,
                                     FSEventStreamEventId since_when,
                                     CFTimeInterval latency,
                                     FSEventStreamCreateFlags flags)
{
	(void)allocator;
	(void)latency;
	(void)flags;

	struct fsevent_stream* stream = calloc(1, sizeof(*stream));
	if (!stream)
		return NULL;

	stream->callback = callback;
	if (context)
		stream->context = *context;
	stream->paths = paths;
	stream->latest_event_id = since_when == UINT64_MAX ? 1 : since_when;
	pthread_mutex_init(&stream->mutex, NULL);
	return stream;
}

FSEventStreamRef FSEventStreamCreateRelativeToDevice(
	CFAllocatorRef allocator, FSEventStreamCallback callback,
	const struct fsevent_stream_context* context, uint64_t device,
	CFArrayRef paths, FSEventStreamEventId since_when,
	CFTimeInterval latency, FSEventStreamCreateFlags flags)
{
	struct fsevent_stream* stream = FSEventStreamCreate(
		allocator, callback, context, paths, since_when, latency, flags);
	if (stream)
		stream->device = device;
	return stream;
}

CFStringRef FSEventStreamCopyDescription(FSEventStreamRef stream_ref)
{
	(void)stream_ref;
	return CFStringCreateWithCString(NULL, "MachGate/FSEventStream", 0);
}

CFArrayRef FSEventStreamCopyPathsBeingWatched(FSEventStreamRef stream_ref)
{
	struct fsevent_stream* stream = stream_ref;
	if (!stream || !stream->paths)
		return CFArrayCreate(NULL, NULL, 0, &kCFTypeArrayCallBacks);
	return stream->paths;
}

void FSEventStreamFlushAsync(FSEventStreamRef stream_ref)
{
	(void)stream_ref;
}

void FSEventStreamFlushSync(FSEventStreamRef stream_ref)
{
	struct fsevent_stream* stream = stream_ref;

	if (!stream || !stream->started || stream->invalidated)
		return;

	pthread_mutex_lock(&stream->mutex);
	shim_fsevent_diff(stream);
	pthread_mutex_unlock(&stream->mutex);
}

uint64_t FSEventStreamGetDeviceBeingWatched(FSEventStreamRef stream_ref)
{
	struct fsevent_stream* stream = stream_ref;
	return stream ? stream->device : 0;
}

FSEventStreamEventId FSEventStreamGetLatestEventId(FSEventStreamRef stream_ref)
{
	struct fsevent_stream* stream = stream_ref;
	return stream ? stream->latest_event_id : 0;
}

void FSEventStreamInvalidate(FSEventStreamRef stream_ref)
{
	struct fsevent_stream* stream = stream_ref;
	if (stream)
		stream->invalidated = 1;
}

void FSEventStreamRelease(FSEventStreamRef stream_ref)
{
	struct fsevent_stream* stream = stream_ref;
	if (!stream)
		return;
	free(stream->snapshot);
	pthread_mutex_destroy(&stream->mutex);
	free(stream);
}

void FSEventStreamScheduleWithRunLoop(FSEventStreamRef stream_ref,
                                      CFRunLoopRef run_loop,
                                      CFStringRef run_loop_mode)
{
	(void)stream_ref;
	(void)run_loop;
	(void)run_loop_mode;
}

void FSEventStreamSetDispatchQueue(FSEventStreamRef stream_ref, void* queue)
{
	(void)stream_ref;
	(void)queue;
}

int FSEventStreamStart(FSEventStreamRef stream_ref)
{
	struct fsevent_stream* stream = stream_ref;
	if (!stream || stream->invalidated)
		return 0;

	pthread_mutex_lock(&stream->mutex);
	int snapshot_result = shim_fsevent_snapshot(stream);
	pthread_mutex_unlock(&stream->mutex);
	if (snapshot_result != 0)
		return 0;

	stream->started = 1;
	return 1;
}

void FSEventStreamStop(FSEventStreamRef stream_ref)
{
	struct fsevent_stream* stream = stream_ref;
	if (stream)
		stream->started = 0;
}

CFStringRef FSEventsCopyUUIDForDevice(uint64_t device)
{
	(void)device;
	return CFUUIDCreateString(NULL, NULL);
}

FSEventStreamEventId FSEventsGetCurrentEventId(void)
{
	return 1;
}

FSEventStreamEventId FSEventsGetLastEventIdForDeviceBeforeTime(
	uint64_t device, CFAbsoluteTime time)
{
	(void)device;
	(void)time;
	return 1;
}

CFMutableDictionaryRef IOBSDNameMatching(uint32_t master_port,
                                         uint32_t options,
                                         const char* bsd_name)
{
	(void)master_port;
	(void)options;
	(void)bsd_name;
	return (CFMutableDictionaryRef)CFDictionaryCreate(NULL, NULL, NULL, 0,
	                                                 &kCFTypeDictionaryKeyCallBacks,
	                                                 &kCFTypeDictionaryValueCallBacks);
}

CFMutableDictionaryRef IOServiceMatching(const char* name)
{
	(void)name;
	return (CFMutableDictionaryRef)CFDictionaryCreate(NULL, NULL, NULL, 0,
	                                                 &kCFTypeDictionaryKeyCallBacks,
	                                                 &kCFTypeDictionaryValueCallBacks);
}

int IOServiceGetMatchingServices(uint32_t master_port, CFDictionaryRef matching,
                                 uint32_t* iterator)
{
	(void)master_port;
	(void)matching;
	if (iterator)
		*iterator = 0;
	return 0;
}

uint32_t IOIteratorNext(uint32_t iterator)
{
	(void)iterator;
	return 0;
}

int IOObjectConformsTo(uint32_t object, const char* class_name)
{
	(void)object;
	(void)class_name;
	return 0;
}

int IOObjectRelease(uint32_t object)
{
	(void)object;
	return 0;
}

const void* IORegistryEntryCreateCFProperty(uint32_t entry, CFStringRef key,
                                            CFAllocatorRef allocator,
                                            uint32_t options)
{
	(void)entry;
	(void)key;
	(void)allocator;
	(void)options;
	return NULL;
}

int IORegistryEntryGetName(uint32_t entry, char* name)
{
	(void)entry;
	if (name)
		name[0] = '\0';
	return 0;
}

int IORegistryEntryGetParentEntry(uint32_t entry, const char* plane,
                                  uint32_t* parent)
{
	(void)entry;
	(void)plane;
	if (parent)
		*parent = 0;
	return -1;
}

void* IOHIDEventSystemClientCreate(CFAllocatorRef allocator)
{
	(void)allocator;
	return calloc(1, 1);
}

void IOHIDEventSystemClientSetMatching(void* client, CFDictionaryRef matching)
{
	(void)client;
	(void)matching;
}

CFArrayRef IOHIDEventSystemClientCopyServices(void* client)
{
	(void)client;
	return CFArrayCreate(NULL, NULL, 0, &kCFTypeArrayCallBacks);
}

const void* IOHIDServiceClientCopyEvent(void* service, int64_t type,
                                        int32_t options, int64_t timeout)
{
	(void)service;
	(void)type;
	(void)options;
	(void)timeout;
	return NULL;
}

const void* IOHIDServiceClientCopyProperty(void* service, CFStringRef key)
{
	(void)service;
	(void)key;
	return NULL;
}

double IOHIDEventGetFloatValue(const void* event, int32_t field)
{
	(void)event;
	(void)field;
	return 0.0;
}

CFTypeID CFStringGetTypeID(void)
{
	return CF_TYPE_ID_STRING;
}

CFTypeID CFArrayGetTypeID(void)
{
	return CF_TYPE_ID_ARRAY;
}

CFTypeID CFDataGetTypeID(void)
{
	return CF_TYPE_ID_DATA;
}

CFTypeID CFDictionaryGetTypeID(void)
{
	return CF_TYPE_ID_DICTIONARY;
}

CFTypeID CFBooleanGetTypeID(void)
{
	return CF_TYPE_ID_BOOLEAN;
}

CFTypeID CFNumberGetTypeID(void)
{
	return CF_TYPE_ID_NUMBER;
}

CFTypeID CFGetTypeID(const void* object)
{
	if (!object)
		return 0;
	if (object == kCFBooleanTrue)
		return CF_TYPE_ID_BOOLEAN;

	CFTypeID type_id = *(const CFTypeID*)object;
	if (type_id == CF_TYPE_ID_ARRAY || type_id == CF_TYPE_ID_DATA ||
	    type_id == CF_TYPE_ID_DICTIONARY || type_id == CF_TYPE_ID_NUMBER)
		return type_id;
	return CF_TYPE_ID_STRING;
}

uint8_t CFBooleanGetValue(const void* boolean)
{
	return boolean == kCFBooleanTrue;
}

const void* CFRetain(const void* object)
{
	return object;
}

CFArrayRef CFLocaleCopyPreferredLanguages(void)
{
	static const void* languages[] = { "en-US" };
	return CFArrayCreate(NULL, languages, 1, &kCFTypeArrayCallBacks);
}

uint8_t LocaleRefGetPartString(void* locale, uint32_t part_mask,
                               int max_string_len, uint16_t* part_string)
{
	(void)locale;
	(void)part_mask;

	if (part_string && max_string_len > 0)
		part_string[0] = 0;
	return 0;
}

typedef int32_t UChar32;
typedef int32_t UErrorCode;

#define U_ZERO_ERROR 0
#define U_ILLEGAL_ARGUMENT_ERROR 1
#define U_BUFFER_OVERFLOW_ERROR 15
#define UBRK_DONE (-1)
#define UCOL_DEFAULT (-1)
#define UCOL_LESS (-1)
#define UCOL_EQUAL 0
#define UCOL_GREATER 1
#define UCOL_TERTIARY 2
#define UCOL_OFF 16
#define UCOL_STRENGTH 5
#define UCOL_ATTRIBUTE_COUNT 8

struct machgate_ubrk {
	int32_t position;
	int32_t text_length;
};

struct machgate_ucal {
	double millis;
};

struct machgate_ucfpos {
	int32_t category;
	int32_t field;
	int32_t start;
	int32_t limit;
};

struct machgate_ucol {
	int32_t attributes[UCOL_ATTRIBUTE_COUNT];
};

struct machgate_uenum {
	int32_t index;
	int32_t count;
	const char* const* values;
};

struct machgate_udat {
	struct machgate_ucal calendar;
};

struct machgate_formatted_value {
	int32_t length;
	uint16_t text[256];
};

struct machgate_unumsys {
	char name[16];
};

struct machgate_utext {
	uint32_t magic;
	int32_t flags;
	int32_t provider_properties;
	int32_t size_of_struct;
};

struct machgate_uidna_info {
	int16_t size;
	uint8_t is_transitional_different;
	uint8_t reserved_b3;
	uint32_t errors;
	int32_t reserved_i2;
	int32_t reserved_i3;
};

static int icu_is_ascii_alpha(UChar32 value)
{
	return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
}

static int icu_is_ascii_digit(UChar32 value)
{
	return value >= '0' && value <= '9';
}

static int icu_is_ascii_space(UChar32 value)
{
	return value == ' ' || (value >= '\t' && value <= '\r');
}

int u_charDirection(UChar32 value)
{
	(void)value;
	return 0;
}

int8_t u_charType(UChar32 value)
{
	if (value >= 'A' && value <= 'Z')
		return 1;
	if (value >= 'a' && value <= 'z')
		return 2;
	if (icu_is_ascii_digit(value))
		return 9;
	if (value >= 0 && value < 0x20)
		return 15;
	if (value == ' ')
		return 12;
	return 0;
}

const char* u_errorName(UErrorCode code)
{
	switch (code) {
	case U_ZERO_ERROR:
		return "U_ZERO_ERROR";
	case U_ILLEGAL_ARGUMENT_ERROR:
		return "U_ILLEGAL_ARGUMENT_ERROR";
	case U_BUFFER_OVERFLOW_ERROR:
		return "U_BUFFER_OVERFLOW_ERROR";
	default:
		return "[BOGUS UErrorCode]";
	}
}

void u_getVersion(uint8_t version_array[4])
{
	if (!version_array)
		return;
	version_array[0] = 76;
	version_array[1] = 1;
	version_array[2] = 0;
	version_array[3] = 0;
}

uint8_t u_hasBinaryProperty(UChar32 value, int property)
{
	switch (property) {
	case 0:
		return icu_is_ascii_alpha(value);
	case 1:
	case 48:
		return isxdigit((unsigned char)value) ? 1 : 0;
	case 22:
		return value >= 'a' && value <= 'z';
	case 30:
		return value >= 'A' && value <= 'Z';
	case 31:
		return icu_is_ascii_space(value);
	case 44:
		return icu_is_ascii_alpha(value) || icu_is_ascii_digit(value);
	case 45:
		return value == ' ' || value == '\t';
	case 46:
		return value >= 0x21 && value <= 0x7e;
	case 47:
		return value >= 0x20 && value <= 0x7e;
	case 49:
	case 34:
		return icu_is_ascii_alpha(value);
	default:
		return 0;
	}
}

UChar32 u_tolower(UChar32 value)
{
	if (value >= 'A' && value <= 'Z')
		return value + ('a' - 'A');
	return value;
}

UChar32 u_toupper(UChar32 value)
{
	if (value >= 'a' && value <= 'z')
		return value - ('a' - 'A');
	return value;
}

static int32_t icu_str_map(uint16_t* dest, int32_t dest_capacity,
                           const uint16_t* src, int32_t src_length,
                           UErrorCode* status, int to_upper)
{
	if (!src || src_length < -1) {
		if (status)
			*status = U_ILLEGAL_ARGUMENT_ERROR;
		return 0;
	}

	if (src_length < 0) {
		src_length = 0;
		while (src[src_length])
			src_length++;
	}

	if (dest && dest_capacity > 0) {
		int32_t copy_length = src_length < dest_capacity ? src_length : dest_capacity;
		for (int32_t index = 0; index < copy_length; index++) {
			UChar32 value = src[index];
			dest[index] = (uint16_t)(to_upper ? u_toupper(value) : u_tolower(value));
		}
		if (copy_length < dest_capacity)
			dest[copy_length] = 0;
	}

	if (status)
		*status = src_length >= dest_capacity ? U_BUFFER_OVERFLOW_ERROR : U_ZERO_ERROR;
	return src_length;
}

int32_t u_strToLower(uint16_t* dest, int32_t dest_capacity,
                     const uint16_t* src, int32_t src_length,
                     const char* locale, UErrorCode* status)
{
	(void)locale;
	return icu_str_map(dest, dest_capacity, src, src_length, status, 0);
}

int32_t u_strToUpper(uint16_t* dest, int32_t dest_capacity,
                     const uint16_t* src, int32_t src_length,
                     const char* locale, UErrorCode* status)
{
	(void)locale;
	return icu_str_map(dest, dest_capacity, src, src_length, status, 1);
}

void* ubrk_clone(const void* break_iterator, UErrorCode* status)
{
	struct machgate_ubrk* result = calloc(1, sizeof(*result));
	const struct machgate_ubrk* source = break_iterator;

	if (!result) {
		if (status)
			*status = U_ILLEGAL_ARGUMENT_ERROR;
		return NULL;
	}
	if (source)
		*result = *source;
	if (status)
		*status = U_ZERO_ERROR;
	return result;
}

void ubrk_close(void* break_iterator)
{
	free(break_iterator);
}

int32_t ubrk_countAvailable(void)
{
	return 1;
}

int32_t ubrk_current(const void* break_iterator)
{
	const struct machgate_ubrk* iterator = break_iterator;
	return iterator ? iterator->position : UBRK_DONE;
}

int32_t ubrk_first(void* break_iterator)
{
	struct machgate_ubrk* iterator = break_iterator;
	if (!iterator)
		return UBRK_DONE;
	iterator->position = 0;
	return iterator->position;
}

int32_t ubrk_following(void* break_iterator, int32_t offset)
{
	struct machgate_ubrk* iterator = break_iterator;
	if (!iterator || offset < 0)
		return UBRK_DONE;
	if (iterator->text_length >= 0 && offset >= iterator->text_length)
		return UBRK_DONE;
	iterator->position = offset + 1;
	return iterator->position;
}

int32_t ubrk_preceding(void* break_iterator, int32_t offset)
{
	struct machgate_ubrk* iterator = break_iterator;
	if (!iterator || offset <= 0)
		return UBRK_DONE;
	iterator->position = offset - 1;
	return iterator->position;
}

const char* ubrk_getAvailable(int32_t index)
{
	return index == 0 ? "en_US" : NULL;
}

int32_t ubrk_getRuleStatus(void* break_iterator)
{
	(void)break_iterator;
	return 0;
}

uint8_t ubrk_isBoundary(void* break_iterator, int32_t offset)
{
	struct machgate_ubrk* iterator = break_iterator;
	if (iterator)
		iterator->position = offset;
	return offset >= 0 ? 1 : 0;
}

int32_t ubrk_next(void* break_iterator)
{
	struct machgate_ubrk* iterator = break_iterator;
	if (!iterator)
		return UBRK_DONE;
	if (iterator->text_length >= 0 && iterator->position >= iterator->text_length)
		return UBRK_DONE;
	iterator->position++;
	return iterator->position;
}

void* ubrk_open(int type, const char* locale, const uint16_t* text,
                int32_t text_length, UErrorCode* status)
{
	(void)type;
	(void)locale;
	(void)text;
	struct machgate_ubrk* iterator = ubrk_clone(NULL, status);
	if (iterator)
		iterator->text_length = text_length;
	return iterator;
}

void ubrk_setText(void* break_iterator, const uint16_t* text,
                  int32_t text_length, UErrorCode* status)
{
	struct machgate_ubrk* iterator = break_iterator;
	(void)text;

	if (!iterator) {
		if (status)
			*status = U_ILLEGAL_ARGUMENT_ERROR;
		return;
	}
	iterator->position = 0;
	iterator->text_length = text_length;
	if (status)
		*status = U_ZERO_ERROR;
}

void ubrk_setUText(void* break_iterator, void* text, UErrorCode* status)
{
	(void)text;
	ubrk_setText(break_iterator, NULL, -1, status);
}

static double icu_now_millis(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

static int32_t utf16_copy_ascii(uint16_t* result, int32_t capacity,
                                const char* text, UErrorCode* status)
{
	int32_t length = (int32_t)strlen(text);

	if (result && capacity > 0) {
		int32_t copy_length = length < capacity ? length : capacity - 1;
		for (int32_t index = 0; index < copy_length; index++)
			result[index] = (uint16_t)(unsigned char)text[index];
		result[copy_length] = 0;
	}

	if (status)
		*status = length >= capacity ? U_BUFFER_OVERFLOW_ERROR : U_ZERO_ERROR;
	return length;
}

void* ucal_open(const uint16_t* zone_id, int32_t length, const char* locale,
                int type, UErrorCode* status)
{
	struct machgate_ucal* calendar = calloc(1, sizeof(*calendar));
	(void)zone_id;
	(void)length;
	(void)locale;
	(void)type;

	if (!calendar) {
		if (status)
			*status = U_ILLEGAL_ARGUMENT_ERROR;
		return NULL;
	}
	calendar->millis = icu_now_millis();
	if (status)
		*status = U_ZERO_ERROR;
	return calendar;
}

void ucal_close(void* calendar)
{
	free(calendar);
}

void* ucal_clone(const void* calendar, UErrorCode* status)
{
	struct machgate_ucal* result = calloc(1, sizeof(*result));
	const struct machgate_ucal* source = calendar;

	if (!result) {
		if (status)
			*status = U_ILLEGAL_ARGUMENT_ERROR;
		return NULL;
	}
	result->millis = source ? source->millis : icu_now_millis();
	if (status)
		*status = U_ZERO_ERROR;
	return result;
}

int32_t ucal_getAttribute(const void* calendar, int attribute)
{
	(void)calendar;

	switch (attribute) {
	case 0:
		return 1;
	case 1:
		return 1;
	case 2:
		return 1;
	default:
		return 0;
	}
}

void ucal_setMillis(void* calendar, double date_time, UErrorCode* status)
{
	struct machgate_ucal* cal = calendar;
	if (cal)
		cal->millis = date_time;
	if (status)
		*status = cal ? U_ZERO_ERROR : U_ILLEGAL_ARGUMENT_ERROR;
}

int32_t ucal_get(const void* calendar, int field, UErrorCode* status)
{
	const struct machgate_ucal* cal = calendar;
	time_t seconds = (time_t)((cal ? cal->millis : icu_now_millis()) / 1000.0);
	struct tm tm_value;

	gmtime_r(&seconds, &tm_value);
	if (status)
		*status = U_ZERO_ERROR;

	switch (field) {
	case 1:
	case 19:
		return tm_value.tm_year + 1900;
	case 2:
	case 22:
		return tm_value.tm_mon;
	case 5:
		return tm_value.tm_mday;
	case 6:
		return tm_value.tm_yday + 1;
	case 7:
		return tm_value.tm_wday + 1;
	case 9:
		return tm_value.tm_hour >= 12;
	case 10:
		return tm_value.tm_hour % 12;
	case 11:
		return tm_value.tm_hour;
	case 12:
		return tm_value.tm_min;
	case 13:
		return tm_value.tm_sec;
	case 14:
		return cal ? (int32_t)((int64_t)cal->millis % 1000) : 0;
	case 15:
	case 16:
		return 0;
	default:
		return 0;
	}
}

int32_t ucal_getCanonicalTimeZoneID(const uint16_t* id, int32_t length,
                                    uint16_t* result, int32_t capacity,
                                    uint8_t* is_system_id,
                                    UErrorCode* status)
{
	(void)id;
	(void)length;
	if (is_system_id)
		*is_system_id = 1;
	return utf16_copy_ascii(result, capacity, "Etc/UTC", status);
}

int32_t ucal_getHostTimeZone(uint16_t* result, int32_t capacity,
                             UErrorCode* status)
{
	return utf16_copy_ascii(result, capacity, "Etc/UTC", status);
}

int32_t ucal_getTimeZoneDisplayName(const void* calendar, int type,
                                    const char* locale, uint16_t* result,
                                    int32_t capacity, UErrorCode* status)
{
	(void)calendar;
	(void)type;
	(void)locale;
	return utf16_copy_ascii(result, capacity, "UTC", status);
}

void ucal_getTimeZoneOffsetFromLocal(const void* calendar,
                                     int non_existing_time_opt,
                                     int duplicated_time_opt,
                                     int32_t* raw_offset,
                                     int32_t* dst_offset,
                                     UErrorCode* status)
{
	(void)calendar;
	(void)non_existing_time_opt;
	(void)duplicated_time_opt;
	if (raw_offset)
		*raw_offset = 0;
	if (dst_offset)
		*dst_offset = 0;
	if (status)
		*status = U_ZERO_ERROR;
}

int ucal_getDayOfWeekType(const void* calendar, int day_of_week,
                          UErrorCode* status)
{
	(void)calendar;
	(void)day_of_week;
	if (status)
		*status = U_ZERO_ERROR;
	return 0;
}

void* ucal_getKeywordValuesForLocale(const char* key, const char* locale,
                                     uint8_t commonly_used,
                                     UErrorCode* status)
{
	(void)key;
	(void)locale;
	(void)commonly_used;
	if (status)
		*status = U_ZERO_ERROR;
	return NULL;
}

void* ucal_openTimeZoneIDEnumeration(int zone_type, const char* region,
                                     const int32_t* raw_offset,
                                     UErrorCode* status)
{
	(void)zone_type;
	(void)region;
	(void)raw_offset;
	if (status)
		*status = U_ZERO_ERROR;
	return NULL;
}

void* ucal_openTimeZones(UErrorCode* status)
{
	if (status)
		*status = U_ZERO_ERROR;
	return NULL;
}

void ucal_setGregorianChange(void* calendar, double date, UErrorCode* status)
{
	(void)calendar;
	(void)date;
	if (status)
		*status = U_ZERO_ERROR;
}

void ucfpos_close(void* field_position)
{
	free(field_position);
}

void* ucfpos_open(UErrorCode* status)
{
	struct machgate_ucfpos* field_position = calloc(1, sizeof(*field_position));
	if (!field_position) {
		if (status)
			*status = U_ILLEGAL_ARGUMENT_ERROR;
		return NULL;
	}
	if (status)
		*status = U_ZERO_ERROR;
	return field_position;
}

void ucfpos_reset(void* field_position, UErrorCode* status)
{
	struct machgate_ucfpos* position = field_position;
	if (position)
		memset(position, 0, sizeof(*position));
	if (status)
		*status = position ? U_ZERO_ERROR : U_ILLEGAL_ARGUMENT_ERROR;
}

void ucfpos_constrainCategory(void* field_position, int32_t category,
                              UErrorCode* status)
{
	struct machgate_ucfpos* position = field_position;
	if (position) {
		position->category = category;
		position->field = 0;
	}
	if (status)
		*status = position ? U_ZERO_ERROR : U_ILLEGAL_ARGUMENT_ERROR;
}

void ucfpos_constrainField(void* field_position, int32_t category,
                           int32_t field, UErrorCode* status)
{
	struct machgate_ucfpos* position = field_position;
	if (position) {
		position->category = category;
		position->field = field;
	}
	if (status)
		*status = position ? U_ZERO_ERROR : U_ILLEGAL_ARGUMENT_ERROR;
}

int32_t ucfpos_getCategory(const void* field_position, UErrorCode* status)
{
	const struct machgate_ucfpos* position = field_position;
	if (status)
		*status = position ? U_ZERO_ERROR : U_ILLEGAL_ARGUMENT_ERROR;
	return position ? position->category : 0;
}

int32_t ucfpos_getField(const void* field_position, UErrorCode* status)
{
	const struct machgate_ucfpos* position = field_position;
	if (status)
		*status = position ? U_ZERO_ERROR : U_ILLEGAL_ARGUMENT_ERROR;
	return position ? position->field : 0;
}

void ucfpos_getIndexes(const void* field_position, int32_t* start,
                       int32_t* limit, UErrorCode* status)
{
	const struct machgate_ucfpos* position = field_position;
	if (start)
		*start = position ? position->start : 0;
	if (limit)
		*limit = position ? position->limit : 0;
	if (status)
		*status = position ? U_ZERO_ERROR : U_ILLEGAL_ARGUMENT_ERROR;
}

static void* ucol_alloc(UErrorCode* status)
{
	struct machgate_ucol* collator = calloc(1, sizeof(*collator));
	if (!collator) {
		if (status)
			*status = U_ILLEGAL_ARGUMENT_ERROR;
		return NULL;
	}

	for (int index = 0; index < UCOL_ATTRIBUTE_COUNT; index++)
		collator->attributes[index] = UCOL_DEFAULT;
	collator->attributes[UCOL_STRENGTH] = UCOL_TERTIARY;
	if (status)
		*status = U_ZERO_ERROR;
	return collator;
}

void* ucol_open(const char* locale, UErrorCode* status)
{
	(void)locale;
	return ucol_alloc(status);
}

void ucol_close(void* collator)
{
	free(collator);
}

int32_t ucol_countAvailable(void)
{
	return 1;
}

const char* ucol_getAvailable(int32_t index)
{
	return index == 0 ? "en_US" : NULL;
}

void* ucol_getKeywordValues(const char* keyword, UErrorCode* status)
{
	if (keyword && strcmp(keyword, "collation") != 0) {
		if (status)
			*status = U_ILLEGAL_ARGUMENT_ERROR;
		return NULL;
	}
	if (status)
		*status = U_ZERO_ERROR;
	return NULL;
}

void* ucol_getKeywordValuesForLocale(const char* key, const char* locale,
                                     uint8_t commonly_used,
                                     UErrorCode* status)
{
	(void)key;
	(void)locale;
	(void)commonly_used;
	if (status)
		*status = U_ZERO_ERROR;
	return NULL;
}

const uint16_t* ucol_getRules(const void* collator, int32_t* length)
{
	static const uint16_t empty_rules[1] = { 0 };
	(void)collator;
	if (length)
		*length = 0;
	return empty_rules;
}

int32_t ucol_getAttribute(const void* collator, int32_t attribute,
                          UErrorCode* status)
{
	const struct machgate_ucol* col = collator;
	if (status)
		*status = col ? U_ZERO_ERROR : U_ILLEGAL_ARGUMENT_ERROR;
	if (!col)
		return UCOL_DEFAULT;
	if (attribute < 0 || attribute >= UCOL_ATTRIBUTE_COUNT)
		return UCOL_DEFAULT;
	return col->attributes[attribute];
}

void ucol_setAttribute(void* collator, int32_t attribute, int32_t value,
                       UErrorCode* status)
{
	struct machgate_ucol* col = collator;
	if (!col || attribute < 0 || attribute >= UCOL_ATTRIBUTE_COUNT) {
		if (status)
			*status = U_ILLEGAL_ARGUMENT_ERROR;
		return;
	}
	col->attributes[attribute] = value;
	if (status)
		*status = U_ZERO_ERROR;
}

static int32_t ucol_utf16_length(const uint16_t* text, int32_t length)
{
	if (!text)
		return 0;
	if (length >= 0)
		return length;

	length = 0;
	while (text[length])
		length++;
	return length;
}

static int ucol_compare_int32(int32_t left, int32_t right)
{
	if (left < right)
		return UCOL_LESS;
	if (left > right)
		return UCOL_GREATER;
	return UCOL_EQUAL;
}

int32_t ucol_strcoll(const void* collator, const uint16_t* source,
                     int32_t source_length, const uint16_t* target,
                     int32_t target_length)
{
	(void)collator;
	source_length = ucol_utf16_length(source, source_length);
	target_length = ucol_utf16_length(target, target_length);

	int32_t common_length = source_length < target_length ? source_length : target_length;
	for (int32_t index = 0; index < common_length; index++) {
		int result = ucol_compare_int32(source[index], target[index]);
		if (result != UCOL_EQUAL)
			return result;
	}
	return ucol_compare_int32(source_length, target_length);
}

int32_t ucol_strcollUTF8(const void* collator, const char* source,
                         int32_t source_length, const char* target,
                         int32_t target_length, UErrorCode* status)
{
	(void)collator;
	if (!source)
		source = "";
	if (!target)
		target = "";
	if (source_length < 0)
		source_length = (int32_t)strlen(source);
	if (target_length < 0)
		target_length = (int32_t)strlen(target);

	int32_t common_length = source_length < target_length ? source_length : target_length;
	int result = memcmp(source, target, (size_t)common_length);
	if (status)
		*status = U_ZERO_ERROR;
	if (result < 0)
		return UCOL_LESS;
	if (result > 0)
		return UCOL_GREATER;
	return ucol_compare_int32(source_length, target_length);
}

const uint16_t* ucurr_getName(const uint16_t* currency, const char* locale,
                              int32_t name_style, uint8_t* is_choice_format,
                              int32_t* length, UErrorCode* status)
{
	static const uint16_t empty_name[1] = { 0 };
	(void)locale;
	(void)name_style;
	if (is_choice_format)
		*is_choice_format = 0;
	if (length) {
		int32_t name_length = 0;
		if (currency) {
			while (currency[name_length])
				name_length++;
		}
		*length = name_length;
	}
	if (status)
		*status = U_ZERO_ERROR;
	return currency ? currency : empty_name;
}

void* ucurr_openISOCurrencies(uint32_t currency_type, UErrorCode* status)
{
	(void)currency_type;
	if (status)
		*status = U_ZERO_ERROR;
	return NULL;
}

void uenum_close(void* enumeration)
{
	free(enumeration);
}

int32_t uenum_count(void* enumeration, UErrorCode* status)
{
	const struct machgate_uenum* en = enumeration;
	if (status)
		*status = U_ZERO_ERROR;
	return en ? en->count : 0;
}

const char* uenum_next(void* enumeration, int32_t* result_length,
                       UErrorCode* status)
{
	struct machgate_uenum* en = enumeration;
	const char* result = NULL;
	if (en && en->index < en->count && en->values)
		result = en->values[en->index++];
	if (result_length)
		*result_length = result ? (int32_t)strlen(result) : 0;
	if (status)
		*status = U_ZERO_ERROR;
	return result;
}

const uint16_t* uenum_unext(void* enumeration, int32_t* result_length,
                            UErrorCode* status)
{
	(void)enumeration;
	if (result_length)
		*result_length = 0;
	if (status)
		*status = U_ZERO_ERROR;
	return NULL;
}

void uenum_reset(void* enumeration, UErrorCode* status)
{
	struct machgate_uenum* en = enumeration;
	if (en)
		en->index = 0;
	if (status)
		*status = U_ZERO_ERROR;
}

void* udat_open(int32_t time_style, int32_t date_style, const char* locale,
                const uint16_t* tz_id, int32_t tz_id_length,
                const uint16_t* pattern, int32_t pattern_length,
                UErrorCode* status)
{
	struct machgate_udat* date_format = calloc(1, sizeof(*date_format));
	(void)time_style;
	(void)date_style;
	(void)locale;
	(void)tz_id;
	(void)tz_id_length;
	(void)pattern;
	(void)pattern_length;

	if (!date_format) {
		if (status)
			*status = U_ILLEGAL_ARGUMENT_ERROR;
		return NULL;
	}
	date_format->calendar.millis = icu_now_millis();
	if (status)
		*status = U_ZERO_ERROR;
	return date_format;
}

void udat_close(void* date_format)
{
	free(date_format);
}

int32_t udat_format(const void* date_format, double date_to_format,
                    uint16_t* result, int32_t result_length,
                    void* position, UErrorCode* status)
{
	(void)date_format;
	(void)date_to_format;
	(void)position;
	return utf16_copy_ascii(result, result_length, "1970-01-01", status);
}

int32_t udat_formatForFields(const void* date_format, double date_to_format,
                             uint16_t* result, int32_t result_length,
                             void* field_position_iterator,
                             UErrorCode* status)
{
	(void)field_position_iterator;
	return udat_format(date_format, date_to_format, result, result_length,
	                   NULL, status);
}

const void* udat_getCalendar(const void* date_format)
{
	const struct machgate_udat* format = date_format;
	return format ? &format->calendar : NULL;
}

int32_t udat_toPattern(const void* date_format, uint8_t localized,
                       uint16_t* result, int32_t result_length,
                       UErrorCode* status)
{
	(void)date_format;
	(void)localized;
	return utf16_copy_ascii(result, result_length, "yyyy-MM-dd", status);
}

void* udatpg_open(const char* locale, UErrorCode* status)
{
	int* generator = calloc(1, sizeof(*generator));
	(void)locale;
	if (!generator) {
		if (status)
			*status = U_ILLEGAL_ARGUMENT_ERROR;
		return NULL;
	}
	if (status)
		*status = U_ZERO_ERROR;
	return generator;
}

void udatpg_close(void* generator)
{
	free(generator);
}

static int32_t utf16_copy(uint16_t* result, int32_t capacity,
                          const uint16_t* text, int32_t length,
                          UErrorCode* status)
{
	if (!text)
		return utf16_copy_ascii(result, capacity, "", status);
	if (length < 0) {
		length = 0;
		while (text[length])
			length++;
	}

	if (result && capacity > 0) {
		int32_t copy_length = length < capacity ? length : capacity - 1;
		for (int32_t index = 0; index < copy_length; index++)
			result[index] = text[index];
		result[copy_length] = 0;
	}

	if (status)
		*status = length >= capacity ? U_BUFFER_OVERFLOW_ERROR : U_ZERO_ERROR;
	return length;
}

int32_t udatpg_getBestPatternWithOptions(void* generator,
                                         const uint16_t* skeleton,
                                         int32_t length, int32_t options,
                                         uint16_t* best_pattern,
                                         int32_t capacity,
                                         UErrorCode* status)
{
	(void)generator;
	(void)options;
	return utf16_copy(best_pattern, capacity, skeleton, length, status);
}

int32_t udatpg_getFieldDisplayName(const void* generator, int32_t field,
                                   int32_t width, uint16_t* field_name,
                                   int32_t capacity, UErrorCode* status)
{
	(void)generator;
	(void)field;
	(void)width;
	return utf16_copy_ascii(field_name, capacity, "field", status);
}

int32_t udatpg_getSkeleton(void* generator, const uint16_t* pattern,
                           int32_t length, uint16_t* skeleton,
                           int32_t capacity, UErrorCode* status)
{
	(void)generator;
	return utf16_copy(skeleton, capacity, pattern, length, status);
}

void* udtitvfmt_open(const char* locale, const uint16_t* skeleton,
                     int32_t skeleton_length, const uint16_t* tz_id,
                     int32_t tz_id_length, UErrorCode* status)
{
	int* formatter = calloc(1, sizeof(*formatter));
	(void)locale;
	(void)skeleton;
	(void)skeleton_length;
	(void)tz_id;
	(void)tz_id_length;
	if (!formatter) {
		if (status)
			*status = U_ILLEGAL_ARGUMENT_ERROR;
		return NULL;
	}
	if (status)
		*status = U_ZERO_ERROR;
	return formatter;
}

void udtitvfmt_close(void* formatter)
{
	free(formatter);
}

void* udtitvfmt_openResult(UErrorCode* status)
{
	struct machgate_formatted_value* result = calloc(1, sizeof(*result));
	if (!result) {
		if (status)
			*status = U_ILLEGAL_ARGUMENT_ERROR;
		return NULL;
	}
	if (status)
		*status = U_ZERO_ERROR;
	return result;
}

void udtitvfmt_closeResult(void* result)
{
	free(result);
}

void udtitvfmt_formatCalendarToResult(const void* formatter,
                                      void* from_calendar,
                                      void* to_calendar, void* result,
                                      UErrorCode* status)
{
	(void)formatter;
	(void)from_calendar;
	(void)to_calendar;
	(void)result;
	if (status)
		*status = U_ZERO_ERROR;
}

void udtitvfmt_formatToResult(const void* formatter, double from_date,
                              double to_date, void* result,
                              UErrorCode* status)
{
	(void)formatter;
	(void)from_date;
	(void)to_date;
	(void)result;
	if (status)
		*status = U_ZERO_ERROR;
}

const void* udtitvfmt_resultAsValue(const void* result, UErrorCode* status)
{
	if (status)
		*status = result ? U_ZERO_ERROR : U_ILLEGAL_ARGUMENT_ERROR;
	return result;
}

void* ufieldpositer_open(UErrorCode* status)
{
	int* iterator = calloc(1, sizeof(*iterator));
	if (!iterator) {
		if (status)
			*status = U_ILLEGAL_ARGUMENT_ERROR;
		return NULL;
	}
	if (status)
		*status = U_ZERO_ERROR;
	return iterator;
}

void ufieldpositer_close(void* iterator)
{
	free(iterator);
}

int32_t ufieldpositer_next(void* iterator, int32_t* begin_index,
                           int32_t* end_index)
{
	(void)iterator;
	if (begin_index)
		*begin_index = 0;
	if (end_index)
		*end_index = 0;
	return -1;
}

const uint16_t* ufmtval_getString(const void* formatted_value,
                                  int32_t* length, UErrorCode* status)
{
	static const uint16_t empty[1] = { 0 };
	const struct machgate_formatted_value* value = formatted_value;
	if (length)
		*length = value ? value->length : 0;
	if (status)
		*status = U_ZERO_ERROR;
	return value ? value->text : empty;
}

uint8_t ufmtval_nextPosition(const void* formatted_value,
                             void* field_position, UErrorCode* status)
{
	(void)formatted_value;
	(void)field_position;
	if (status)
		*status = U_ZERO_ERROR;
	return 0;
}

void* uidna_openUTS46(uint32_t options, UErrorCode* status)
{
	int* idna = calloc(1, sizeof(*idna));
	(void)options;
	if (!idna) {
		if (status)
			*status = U_ILLEGAL_ARGUMENT_ERROR;
		return NULL;
	}
	if (status)
		*status = U_ZERO_ERROR;
	return idna;
}

void uidna_close(void* idna)
{
	free(idna);
}

static int32_t uidna_copy_name(const uint16_t* name, int32_t length,
                               uint16_t* dest, int32_t capacity,
                               struct machgate_uidna_info* info,
                               UErrorCode* status)
{
	if (info) {
		info->is_transitional_different = 0;
		info->errors = 0;
	}
	return utf16_copy(dest, capacity, name, length, status);
}

int32_t uidna_nameToASCII(const void* idna, const uint16_t* name,
                          int32_t length, uint16_t* dest, int32_t capacity,
                          struct machgate_uidna_info* info,
                          UErrorCode* status)
{
	(void)idna;
	return uidna_copy_name(name, length, dest, capacity, info, status);
}

int32_t uidna_nameToUnicode(const void* idna, const uint16_t* name,
                            int32_t length, uint16_t* dest, int32_t capacity,
                            struct machgate_uidna_info* info,
                            UErrorCode* status)
{
	(void)idna;
	return uidna_copy_name(name, length, dest, capacity, info, status);
}

void* uldn_openForContext(const char* locale, const int32_t* contexts,
                          int32_t length, UErrorCode* status)
{
	int* display_names = calloc(1, sizeof(*display_names));
	(void)locale;
	(void)contexts;
	(void)length;
	if (!display_names) {
		if (status)
			*status = U_ILLEGAL_ARGUMENT_ERROR;
		return NULL;
	}
	if (status)
		*status = U_ZERO_ERROR;
	return display_names;
}

void uldn_close(void* display_names)
{
	free(display_names);
}

static int32_t uldn_copy_display_name(const char* text, uint16_t* result,
                                      int32_t capacity, UErrorCode* status)
{
	return utf16_copy_ascii(result, capacity, text ? text : "", status);
}

int32_t uldn_keyValueDisplayName(const void* display_names, const char* key,
                                 const char* value, uint16_t* result,
                                 int32_t capacity, UErrorCode* status)
{
	(void)display_names;
	(void)key;
	return uldn_copy_display_name(value, result, capacity, status);
}

int32_t uldn_localeDisplayName(const void* display_names, const char* locale,
                               uint16_t* result, int32_t capacity,
                               UErrorCode* status)
{
	(void)display_names;
	return uldn_copy_display_name(locale, result, capacity, status);
}

int32_t uldn_regionDisplayName(const void* display_names, const char* region,
                               uint16_t* result, int32_t capacity,
                               UErrorCode* status)
{
	(void)display_names;
	return uldn_copy_display_name(region, result, capacity, status);
}

int32_t uldn_scriptDisplayName(const void* display_names, const char* script,
                               uint16_t* result, int32_t capacity,
                               UErrorCode* status)
{
	(void)display_names;
	return uldn_copy_display_name(script, result, capacity, status);
}

void* ulistfmt_openForType(const char* locale, int32_t type, int32_t width,
                           UErrorCode* status)
{
	int* formatter = calloc(1, sizeof(*formatter));
	(void)locale;
	(void)type;
	(void)width;
	if (!formatter) {
		if (status)
			*status = U_ILLEGAL_ARGUMENT_ERROR;
		return NULL;
	}
	if (status)
		*status = U_ZERO_ERROR;
	return formatter;
}

void ulistfmt_close(void* formatter)
{
	free(formatter);
}

void* ulistfmt_openResult(UErrorCode* status)
{
	struct machgate_formatted_value* result = calloc(1, sizeof(*result));
	if (!result) {
		if (status)
			*status = U_ILLEGAL_ARGUMENT_ERROR;
		return NULL;
	}
	if (status)
		*status = U_ZERO_ERROR;
	return result;
}

void ulistfmt_closeResult(void* result)
{
	free(result);
}

static int32_t ulistfmt_copy_first(const uint16_t* const strings[],
                                   const int32_t* string_lengths,
                                   int32_t string_count, uint16_t* result,
                                   int32_t capacity, UErrorCode* status)
{
	if (!strings || string_count <= 0 || !strings[0])
		return utf16_copy_ascii(result, capacity, "", status);
	int32_t length = string_lengths ? string_lengths[0] : -1;
	return utf16_copy(result, capacity, strings[0], length, status);
}

int32_t ulistfmt_format(const void* formatter, const uint16_t* const strings[],
                        const int32_t* string_lengths, int32_t string_count,
                        uint16_t* result, int32_t capacity,
                        UErrorCode* status)
{
	(void)formatter;
	return ulistfmt_copy_first(strings, string_lengths, string_count, result,
	                           capacity, status);
}

void ulistfmt_formatStringsToResult(const void* formatter,
                                    const uint16_t* const strings[],
                                    const int32_t* string_lengths,
                                    int32_t string_count, void* result,
                                    UErrorCode* status)
{
	struct machgate_formatted_value* value = result;
	(void)formatter;
	if (value) {
		value->length = ulistfmt_copy_first(strings, string_lengths, string_count,
		                                    value->text, 256, status);
		return;
	}
	if (status)
		*status = U_ILLEGAL_ARGUMENT_ERROR;
}

const void* ulistfmt_resultAsValue(const void* result, UErrorCode* status)
{
	if (status)
		*status = result ? U_ZERO_ERROR : U_ILLEGAL_ARGUMENT_ERROR;
	return result;
}

static int32_t char_copy(char* result, int32_t capacity, const char* text,
                         UErrorCode* status)
{
	int32_t length = (int32_t)strlen(text);

	if (result && capacity > 0) {
		int32_t copy_length = length < capacity ? length : capacity - 1;
		memcpy(result, text, (size_t)copy_length);
		result[copy_length] = 0;
	}

	if (status)
		*status = length >= capacity ? U_BUFFER_OVERFLOW_ERROR : U_ZERO_ERROR;
	return length;
}

static const char* uloc_or_default(const char* locale)
{
	return locale && locale[0] ? locale : "en_US";
}

int32_t uloc_addLikelySubtags(const char* locale, char* result,
                              int32_t capacity, UErrorCode* status)
{
	return char_copy(result, capacity, uloc_or_default(locale), status);
}

int32_t uloc_canonicalize(const char* locale, char* result,
                          int32_t capacity, UErrorCode* status)
{
	return char_copy(result, capacity, uloc_or_default(locale), status);
}

int32_t uloc_countAvailable(void)
{
	return 1;
}

int32_t uloc_forLanguageTag(const char* language_tag, char* locale,
                            int32_t capacity, int32_t* parsed_length,
                            UErrorCode* status)
{
	const char* source = uloc_or_default(language_tag);
	if (parsed_length)
		*parsed_length = (int32_t)strlen(source);
	return char_copy(locale, capacity, source, status);
}

const char* uloc_getAvailable(int32_t index)
{
	return index == 0 ? "en_US" : NULL;
}

static int32_t uloc_copy_part(const char* locale, char* result,
                              int32_t capacity, UErrorCode* status,
                              int part)
{
	const char* source = uloc_or_default(locale);
	char buffer[16] = "";
	const char* separator = strchr(source, '_');

	if (part == 0) {
		size_t length = separator ? (size_t)(separator - source) : strlen(source);
		if (length >= sizeof(buffer))
			length = sizeof(buffer) - 1;
		memcpy(buffer, source, length);
		buffer[length] = 0;
	} else if (separator) {
		const char* country = separator + 1;
		size_t length = strcspn(country, "_@.-");
		if (length >= sizeof(buffer))
			length = sizeof(buffer) - 1;
		memcpy(buffer, country, length);
		buffer[length] = 0;
	}

	return char_copy(result, capacity, buffer, status);
}

int32_t uloc_getBaseName(const char* locale, char* name, int32_t capacity,
                         UErrorCode* status)
{
	const char* source = uloc_or_default(locale);
	char buffer[128];
	size_t length = strcspn(source, "@");
	if (length >= sizeof(buffer))
		length = sizeof(buffer) - 1;
	memcpy(buffer, source, length);
	buffer[length] = 0;
	return char_copy(name, capacity, buffer, status);
}

int32_t uloc_getCharacterOrientation(const char* locale, UErrorCode* status)
{
	(void)locale;
	if (status)
		*status = U_ZERO_ERROR;
	return 0;
}

int32_t uloc_getCountry(const char* locale, char* country, int32_t capacity,
                        UErrorCode* status)
{
	return uloc_copy_part(locale, country, capacity, status, 1);
}

const char* uloc_getDefault(void)
{
	return "en_US";
}

int32_t uloc_getKeywordValue(const char* locale, const char* keyword,
                             char* buffer, int32_t capacity,
                             UErrorCode* status)
{
	(void)locale;
	(void)keyword;
	return char_copy(buffer, capacity, "", status);
}

int32_t uloc_getLanguage(const char* locale, char* language,
                         int32_t capacity, UErrorCode* status)
{
	return uloc_copy_part(locale, language, capacity, status, 0);
}

int32_t uloc_getScript(const char* locale, char* script, int32_t capacity,
                       UErrorCode* status)
{
	(void)locale;
	return char_copy(script, capacity, "", status);
}

int32_t uloc_minimizeSubtags(const char* locale, char* result,
                             int32_t capacity, UErrorCode* status)
{
	return char_copy(result, capacity, uloc_or_default(locale), status);
}

void* uloc_openKeywords(const char* locale, UErrorCode* status)
{
	(void)locale;
	if (status)
		*status = U_ZERO_ERROR;
	return NULL;
}

int32_t uloc_setKeywordValue(const char* keyword, const char* value,
                             char* buffer, int32_t capacity,
                             UErrorCode* status)
{
	(void)keyword;
	(void)value;
	return char_copy(buffer, capacity, uloc_or_default(buffer), status);
}

int32_t uloc_toLanguageTag(const char* locale, char* language_tag,
                           int32_t capacity, uint8_t strict,
                           UErrorCode* status)
{
	(void)strict;
	return char_copy(language_tag, capacity, uloc_or_default(locale), status);
}

const char* uloc_toUnicodeLocaleType(const char* keyword, const char* value)
{
	(void)keyword;
	return value;
}

static int machgate_unorm2_nfc;
static int machgate_unorm2_nfd;
static int machgate_unorm2_nfkc;
static int machgate_unorm2_nfkd;

const void* unorm2_getNFCInstance(UErrorCode* status)
{
	if (status)
		*status = U_ZERO_ERROR;
	return &machgate_unorm2_nfc;
}

const void* unorm2_getNFDInstance(UErrorCode* status)
{
	if (status)
		*status = U_ZERO_ERROR;
	return &machgate_unorm2_nfd;
}

const void* unorm2_getNFKCInstance(UErrorCode* status)
{
	if (status)
		*status = U_ZERO_ERROR;
	return &machgate_unorm2_nfkc;
}

const void* unorm2_getNFKDInstance(UErrorCode* status)
{
	if (status)
		*status = U_ZERO_ERROR;
	return &machgate_unorm2_nfkd;
}

int32_t unorm2_normalize(const void* normalizer, const uint16_t* source,
                         int32_t length, uint16_t* result,
                         int32_t capacity, UErrorCode* status)
{
	(void)normalizer;
	return utf16_copy(result, capacity, source, length, status);
}

int32_t unorm2_normalizeSecondAndAppend(const void* normalizer,
                                        uint16_t* first, int32_t first_length,
                                        int32_t first_capacity,
                                        const uint16_t* second,
                                        int32_t second_length,
                                        UErrorCode* status)
{
	(void)normalizer;
	if (!first || first_capacity <= 0) {
		if (status)
			*status = U_ILLEGAL_ARGUMENT_ERROR;
		return 0;
	}
	if (first_length < 0) {
		first_length = 0;
		while (first[first_length])
			first_length++;
	}
	int32_t appended = utf16_copy(first + first_length,
	                              first_capacity - first_length,
	                              second, second_length, status);
	return first_length + appended;
}

int32_t unorm2_getDecomposition(const void* normalizer, UChar32 value,
                                uint16_t* decomposition, int32_t capacity,
                                UErrorCode* status)
{
	(void)normalizer;
	if (decomposition && capacity > 0)
		decomposition[0] = 0;
	if (status)
		*status = U_ZERO_ERROR;
	(void)value;
	return 0;
}

uint8_t unorm2_isNormalized(const void* normalizer, const uint16_t* source,
                            int32_t length, UErrorCode* status)
{
	(void)normalizer;
	(void)source;
	(void)length;
	if (status)
		*status = U_ZERO_ERROR;
	return 1;
}

void* unum_open(int32_t style, const uint16_t* pattern, int32_t pattern_length,
                const char* locale, void* parse_error, UErrorCode* status)
{
	int* formatter = calloc(1, sizeof(*formatter));
	(void)style;
	(void)pattern;
	(void)pattern_length;
	(void)locale;
	(void)parse_error;
	if (!formatter) {
		if (status)
			*status = U_ILLEGAL_ARGUMENT_ERROR;
		return NULL;
	}
	if (status)
		*status = U_ZERO_ERROR;
	return formatter;
}

void unum_close(void* formatter)
{
	free(formatter);
}

void unum_setAttribute(void* formatter, int32_t attribute, int32_t value)
{
	(void)formatter;
	(void)attribute;
	(void)value;
}

static void formatted_value_set_ascii(struct machgate_formatted_value* value,
                                      const char* text)
{
	if (!value)
		return;
	value->length = utf16_copy_ascii(value->text, 256, text, NULL);
}

static void formatted_value_set_double(struct machgate_formatted_value* value,
                                       double number)
{
	char buffer[64];
	snprintf(buffer, sizeof(buffer), "%.15g", number);
	formatted_value_set_ascii(value, buffer);
}

void* unumf_openForSkeletonAndLocale(const uint16_t* skeleton,
                                     int32_t skeleton_length,
                                     const char* locale,
                                     UErrorCode* status)
{
	int* formatter = calloc(1, sizeof(*formatter));
	(void)skeleton;
	(void)skeleton_length;
	(void)locale;
	if (!formatter) {
		if (status)
			*status = U_ILLEGAL_ARGUMENT_ERROR;
		return NULL;
	}
	if (status)
		*status = U_ZERO_ERROR;
	return formatter;
}

void unumf_close(void* formatter)
{
	free(formatter);
}

void* unumf_openResult(UErrorCode* status)
{
	struct machgate_formatted_value* result = calloc(1, sizeof(*result));
	if (!result) {
		if (status)
			*status = U_ILLEGAL_ARGUMENT_ERROR;
		return NULL;
	}
	if (status)
		*status = U_ZERO_ERROR;
	return result;
}

void unumf_closeResult(void* result)
{
	free(result);
}

void unumf_formatDecimal(const void* formatter, const char* value,
                         int32_t value_length, void* result,
                         UErrorCode* status)
{
	struct machgate_formatted_value* formatted = result;
	char buffer[128];
	(void)formatter;
	if (!value)
		value = "";
	if (value_length < 0)
		value_length = (int32_t)strlen(value);
	if (value_length >= (int32_t)sizeof(buffer))
		value_length = (int32_t)sizeof(buffer) - 1;
	memcpy(buffer, value, (size_t)value_length);
	buffer[value_length] = 0;
	formatted_value_set_ascii(formatted, buffer);
	if (status)
		*status = formatted ? U_ZERO_ERROR : U_ILLEGAL_ARGUMENT_ERROR;
}

void unumf_formatDouble(const void* formatter, double value, void* result,
                        UErrorCode* status)
{
	(void)formatter;
	formatted_value_set_double(result, value);
	if (status)
		*status = result ? U_ZERO_ERROR : U_ILLEGAL_ARGUMENT_ERROR;
}

void unumf_resultGetAllFieldPositions(const void* result, void* iterator,
                                      UErrorCode* status)
{
	(void)result;
	(void)iterator;
	if (status)
		*status = U_ZERO_ERROR;
}

int32_t unumf_resultToString(const void* result, uint16_t* buffer,
                             int32_t capacity, UErrorCode* status)
{
	const struct machgate_formatted_value* formatted = result;
	return utf16_copy(buffer, capacity,
	                  formatted ? formatted->text : NULL,
	                  formatted ? formatted->length : 0, status);
}

const void* unumf_resultAsValue(const void* result, UErrorCode* status)
{
	if (status)
		*status = result ? U_ZERO_ERROR : U_ILLEGAL_ARGUMENT_ERROR;
	return result;
}

void* unumrf_openForSkeletonWithCollapseAndIdentityFallback(
	const uint16_t* skeleton, int32_t skeleton_length, int32_t collapse,
	int32_t identity_fallback, const char* locale, UErrorCode* status)
{
	int* formatter = calloc(1, sizeof(*formatter));
	(void)skeleton;
	(void)skeleton_length;
	(void)collapse;
	(void)identity_fallback;
	(void)locale;
	if (!formatter) {
		if (status)
			*status = U_ILLEGAL_ARGUMENT_ERROR;
		return NULL;
	}
	if (status)
		*status = U_ZERO_ERROR;
	return formatter;
}

void unumrf_close(void* formatter)
{
	free(formatter);
}

void* unumrf_openResult(UErrorCode* status)
{
	return unumf_openResult(status);
}

void unumrf_closeResult(void* result)
{
	free(result);
}

void unumrf_formatDecimalRange(const void* formatter, const char* first,
                               int32_t first_length, const char* second,
                               int32_t second_length, void* result,
                               UErrorCode* status)
{
	(void)formatter;
	(void)second;
	(void)second_length;
	unumf_formatDecimal(NULL, first, first_length, result, status);
}

void unumrf_formatDoubleRange(const void* formatter, double first,
                              double second, void* result,
                              UErrorCode* status)
{
	(void)formatter;
	(void)second;
	unumf_formatDouble(NULL, first, result, status);
}

const void* unumrf_resultAsValue(const void* result, UErrorCode* status)
{
	return unumf_resultAsValue(result, status);
}

void unumsys_close(void* numbering_system)
{
	free(numbering_system);
}

static void* unumsys_alloc(const char* name, UErrorCode* status)
{
	struct machgate_unumsys* numbering_system = calloc(1, sizeof(*numbering_system));
	if (!numbering_system) {
		if (status)
			*status = U_ILLEGAL_ARGUMENT_ERROR;
		return NULL;
	}
	snprintf(numbering_system->name, sizeof(numbering_system->name), "%s",
	         name && name[0] ? name : "latn");
	if (status)
		*status = U_ZERO_ERROR;
	return numbering_system;
}

void* unumsys_open(const char* locale, UErrorCode* status)
{
	(void)locale;
	return unumsys_alloc("latn", status);
}

void* unumsys_openByName(const char* name, UErrorCode* status)
{
	return unumsys_alloc(name, status);
}

void* unumsys_openAvailableNames(UErrorCode* status)
{
	static const char* const values[] = { "latn" };
	struct machgate_uenum* enumeration = calloc(1, sizeof(*enumeration));
	if (!enumeration) {
		if (status)
			*status = U_ILLEGAL_ARGUMENT_ERROR;
		return NULL;
	}
	enumeration->count = 1;
	enumeration->values = values;
	if (status)
		*status = U_ZERO_ERROR;
	return enumeration;
}

const char* unumsys_getName(const void* numbering_system)
{
	const struct machgate_unumsys* system = numbering_system;
	return system ? system->name : "latn";
}

uint8_t unumsys_isAlgorithmic(const void* numbering_system)
{
	(void)numbering_system;
	return 0;
}

void* uplrules_openForType(const char* locale, int32_t type, UErrorCode* status)
{
	int* rules = calloc(1, sizeof(*rules));
	(void)locale;
	(void)type;
	if (!rules) {
		if (status)
			*status = U_ILLEGAL_ARGUMENT_ERROR;
		return NULL;
	}
	if (status)
		*status = U_ZERO_ERROR;
	return rules;
}

void uplrules_close(void* rules)
{
	free(rules);
}

void* uplrules_getKeywords(const void* rules, UErrorCode* status)
{
	static const char* const values[] = { "other" };
	struct machgate_uenum* enumeration = calloc(1, sizeof(*enumeration));
	(void)rules;
	if (!enumeration) {
		if (status)
			*status = U_ILLEGAL_ARGUMENT_ERROR;
		return NULL;
	}
	enumeration->count = 1;
	enumeration->values = values;
	if (status)
		*status = U_ZERO_ERROR;
	return enumeration;
}

int32_t uplrules_selectFormatted(const void* rules, const void* number,
                                 uint16_t* keyword, int32_t capacity,
                                 UErrorCode* status)
{
	(void)rules;
	(void)number;
	return utf16_copy_ascii(keyword, capacity, "other", status);
}

int32_t uplrules_selectForRange(const void* rules, const void* range,
                                uint16_t* keyword, int32_t capacity,
                                UErrorCode* status)
{
	(void)rules;
	(void)range;
	return utf16_copy_ascii(keyword, capacity, "other", status);
}

void* ureldatefmt_open(const char* locale, void* number_format, int32_t width,
                       int32_t capitalization_context, UErrorCode* status)
{
	int* formatter = calloc(1, sizeof(*formatter));
	(void)locale;
	free(number_format);
	(void)width;
	(void)capitalization_context;
	if (!formatter) {
		if (status)
			*status = U_ILLEGAL_ARGUMENT_ERROR;
		return NULL;
	}
	if (status)
		*status = U_ZERO_ERROR;
	return formatter;
}

void ureldatefmt_close(void* formatter)
{
	free(formatter);
}

void* ureldatefmt_openResult(UErrorCode* status)
{
	return unumf_openResult(status);
}

void ureldatefmt_closeResult(void* result)
{
	free(result);
}

const void* ureldatefmt_resultAsValue(const void* result, UErrorCode* status)
{
	return unumf_resultAsValue(result, status);
}

static int32_t ureldatefmt_format_value(double offset, uint16_t* result,
                                        int32_t capacity, UErrorCode* status)
{
	char buffer[64];
	snprintf(buffer, sizeof(buffer), "%.15g", offset);
	return utf16_copy_ascii(result, capacity, buffer, status);
}

int32_t ureldatefmt_format(const void* formatter, double offset, int32_t unit,
                           uint16_t* result, int32_t capacity,
                           UErrorCode* status)
{
	(void)formatter;
	(void)unit;
	return ureldatefmt_format_value(offset, result, capacity, status);
}

int32_t ureldatefmt_formatNumeric(const void* formatter, double offset,
                                  int32_t unit, uint16_t* result,
                                  int32_t capacity, UErrorCode* status)
{
	return ureldatefmt_format(formatter, offset, unit, result, capacity, status);
}

void ureldatefmt_formatToResult(const void* formatter, double offset,
                                int32_t unit, void* result,
                                UErrorCode* status)
{
	(void)formatter;
	(void)unit;
	formatted_value_set_double(result, offset);
	if (status)
		*status = result ? U_ZERO_ERROR : U_ILLEGAL_ARGUMENT_ERROR;
}

void ureldatefmt_formatNumericToResult(const void* formatter, double offset,
                                       int32_t unit, void* result,
                                       UErrorCode* status)
{
	ureldatefmt_formatToResult(formatter, offset, unit, result, status);
}

void* ures_open(const char* package_name, const char* locale, UErrorCode* status)
{
	int* resource = calloc(1, sizeof(*resource));
	(void)package_name;
	(void)locale;
	if (!resource) {
		if (status)
			*status = U_ILLEGAL_ARGUMENT_ERROR;
		return NULL;
	}
	if (status)
		*status = U_ZERO_ERROR;
	return resource;
}

void ures_close(void* resource)
{
	free(resource);
}

void* ures_getByKey(const void* resource, const char* key, void* fill_in,
                    UErrorCode* status)
{
	(void)resource;
	(void)key;
	(void)fill_in;
	return ures_open(NULL, NULL, status);
}

const uint16_t* ures_getStringByKey(const void* resource, const char* key,
                                    int32_t* length, UErrorCode* status)
{
	static const uint16_t empty[1] = { 0 };
	(void)resource;
	(void)key;
	if (length)
		*length = 0;
	if (status)
		*status = U_ZERO_ERROR;
	return empty;
}

void* utext_setup(void* text, int32_t extra_space, UErrorCode* status)
{
	struct machgate_utext* result = text;
	(void)extra_space;
	if (!result) {
		result = calloc(1, sizeof(*result));
		if (!result) {
			if (status)
				*status = U_ILLEGAL_ARGUMENT_ERROR;
			return NULL;
		}
		result->flags = 1;
	} else {
		memset(result, 0, sizeof(*result));
	}
	result->magic = 0x345ad82c;
	result->size_of_struct = (int32_t)sizeof(*result);
	if (status)
		*status = U_ZERO_ERROR;
	return result;
}

void* utext_close(void* text)
{
	struct machgate_utext* utext = text;
	if (!utext)
		return NULL;
	if (utext->flags & 1)
		free(utext);
	return NULL;
}

void* CFRunLoopGetCurrent(void)
{
	static int run_loop;
	return &run_loop;
}

uint8_t CFRunLoopIsWaiting(void* run_loop)
{
	(void)run_loop;
	return 0;
}

void CFRunLoopRun(void)
{
}

void CFRunLoopStop(void* run_loop)
{
	(void)run_loop;
}

void CFRunLoopWakeUp(void* run_loop)
{
	(void)run_loop;
}

const void* CFNumberCreate(CFAllocatorRef allocator, int type,
                           const void* value)
{
	(void)allocator;

	if (!value)
		return NULL;

	struct cf_number* number = calloc(1, sizeof(*number));
	if (!number)
		return NULL;

	number->type_id = CF_TYPE_ID_NUMBER;
	number->type = type;
	number->signed_value = *(const int64_t*)value;
	number->double_value = (double)number->signed_value;
	return number;
}

uint8_t CFNumberGetValue(const void* number, int type, void* value)
{
	if (!number || !value)
		return 0;

	if (number == kCFBooleanTrue) {
		*(int*)value = 1;
		return 1;
	}

	if (CFGetTypeID(number) != CF_TYPE_ID_NUMBER)
		return 0;

	const struct cf_number* cf_number = number;
	switch (type) {
	case 16:
	case 17:
	case 18:
	case 19:
		*(double*)value = cf_number->double_value;
		break;
	case 7:
	case 8:
	case 9:
	case 10:
	case 11:
	case 12:
	case 13:
	case 14:
	case 15:
		*(int64_t*)value = cf_number->signed_value;
		break;
	default:
		*(int*)value = (int)cf_number->signed_value;
		break;
	}
	return 1;
}

const void* CFDateCreate(CFAllocatorRef allocator, CFAbsoluteTime at)
{
	(void)allocator;

	double* date = malloc(sizeof(*date));
	if (!date)
		return NULL;
	*date = at;
	return date;
}

CFStringRef CFErrorCopyDescription(CFErrorRef error)
{
	(void)error;
	return cf_error_description;
}

CFIndex CFErrorGetCode(CFErrorRef error)
{
	(void)error;
	return 0;
}

void CFRelease(const void* object)
{
	(void)object;
}

#define DARWIN_AF_INET 2
#define DARWIN_AF_INET6 30
#define DARWIN_EAI_ADDRFAMILY 1
#define DARWIN_EAI_AGAIN 2
#define DARWIN_EAI_FAIL 4
#define DARWIN_EAI_MEMORY 6
#define DARWIN_EAI_NODATA 7
#define DARWIN_EAI_NONAME 8
#define DARWIN_EAI_SERVICE 9
#define DARWIN_EAI_SYSTEM 11
#define DARWIN_EAI_OVERFLOW 14
#define DARWIN_AI_PASSIVE 0x1
#define DARWIN_AI_CANONNAME 0x2
#define DARWIN_AI_NUMERICHOST 0x4
#define DARWIN_AI_NUMERICSERV 0x1000

struct darwin_addrinfo {
	int32_t flags;
	int32_t family;
	int32_t socktype;
	int32_t protocol;
	uint32_t addrlen;
	char* canonname;
	void* addr;
	struct darwin_addrinfo* next;
};

static void free_darwin_addrinfo(struct darwin_addrinfo* info)
{
	while (info) {
		struct darwin_addrinfo* next = info->next;
		free(info->canonname);
		free(info->addr);
		free(info);
		info = next;
	}
}

static int darwin_family_to_linux_addrinfo(int family)
{
	switch (family) {
	case 0:
		return AF_UNSPEC;
	case DARWIN_AF_INET:
		return AF_INET;
	case DARWIN_AF_INET6:
		return AF_INET6;
	default:
		return family;
	}
}

static int linux_family_to_darwin_addrinfo(int family)
{
	switch (family) {
	case AF_INET:
		return DARWIN_AF_INET;
	case AF_INET6:
		return DARWIN_AF_INET6;
	default:
		return family;
	}
}

static int darwin_gai_error_from_linux(int result)
{
	if (result == 0)
		return 0;
	if (result == EAI_AGAIN)
		return DARWIN_EAI_AGAIN;
	if (result == EAI_FAIL)
		return DARWIN_EAI_FAIL;
	if (result == EAI_MEMORY)
		return DARWIN_EAI_MEMORY;
	if (result == EAI_NONAME)
		return DARWIN_EAI_NONAME;
	if (result == EAI_SERVICE)
		return DARWIN_EAI_SERVICE;
	if (result == EAI_SYSTEM)
		return DARWIN_EAI_SYSTEM;
#ifdef EAI_ADDRFAMILY
	if (result == EAI_ADDRFAMILY)
		return DARWIN_EAI_ADDRFAMILY;
#endif
#ifdef EAI_NODATA
	if (result == EAI_NODATA)
		return DARWIN_EAI_NODATA;
#endif
#ifdef EAI_OVERFLOW
	if (result == EAI_OVERFLOW)
		return DARWIN_EAI_OVERFLOW;
#endif
	return DARWIN_EAI_FAIL;
}

static int linux_gai_error_from_darwin(int result)
{
	if (result == 0)
		return 0;
	if (result == DARWIN_EAI_AGAIN)
		return EAI_AGAIN;
	if (result == DARWIN_EAI_FAIL)
		return EAI_FAIL;
	if (result == DARWIN_EAI_MEMORY)
		return EAI_MEMORY;
	if (result == DARWIN_EAI_NONAME)
		return EAI_NONAME;
	if (result == DARWIN_EAI_SERVICE)
		return EAI_SERVICE;
	if (result == DARWIN_EAI_SYSTEM)
		return EAI_SYSTEM;
#ifdef EAI_ADDRFAMILY
	if (result == DARWIN_EAI_ADDRFAMILY)
		return EAI_ADDRFAMILY;
#endif
#ifdef EAI_NODATA
	if (result == DARWIN_EAI_NODATA)
		return EAI_NODATA;
#endif
#ifdef EAI_OVERFLOW
	if (result == DARWIN_EAI_OVERFLOW)
		return EAI_OVERFLOW;
#endif
	return EAI_FAIL;
}

static int darwin_ai_flags_to_linux(int flags)
{
	int result = 0;

	if (flags & DARWIN_AI_PASSIVE)
		result |= AI_PASSIVE;
	if (flags & DARWIN_AI_CANONNAME)
		result |= AI_CANONNAME;
	if (flags & DARWIN_AI_NUMERICHOST)
		result |= AI_NUMERICHOST;
#ifdef AI_NUMERICSERV
	if (flags & DARWIN_AI_NUMERICSERV)
		result |= AI_NUMERICSERV;
#endif
	return result;
}

static int copy_linux_sockaddr_to_darwin_addrinfo(const struct sockaddr* linux_addr,
                                                  void** out_addr,
                                                  uint32_t* out_addrlen)
{
	unsigned char* darwin_addr;

	if (!linux_addr || !out_addr || !out_addrlen)
		return 0;

	switch (linux_addr->sa_family) {
	case AF_INET: {
		const struct sockaddr_in* inet_addr =
			(const struct sockaddr_in*)linux_addr;
		darwin_addr = calloc(1, 16);
		if (!darwin_addr)
			return 0;
		darwin_addr[0] = 16;
		darwin_addr[1] = DARWIN_AF_INET;
		memcpy(darwin_addr + 2, &inet_addr->sin_port, 2);
		memcpy(darwin_addr + 4, &inet_addr->sin_addr, 4);
		*out_addr = darwin_addr;
		*out_addrlen = 16;
		return 1;
	}
	case AF_INET6: {
		const struct sockaddr_in6* inet6_addr =
			(const struct sockaddr_in6*)linux_addr;
		darwin_addr = calloc(1, 28);
		if (!darwin_addr)
			return 0;
		darwin_addr[0] = 28;
		darwin_addr[1] = DARWIN_AF_INET6;
		memcpy(darwin_addr + 2, &inet6_addr->sin6_port, 2);
		memcpy(darwin_addr + 4, &inet6_addr->sin6_flowinfo, 4);
		memcpy(darwin_addr + 8, &inet6_addr->sin6_addr, 16);
		memcpy(darwin_addr + 24, &inet6_addr->sin6_scope_id, 4);
		*out_addr = darwin_addr;
		*out_addrlen = 28;
		return 1;
	}
	default:
		return 0;
	}
}

static struct darwin_addrinfo* copy_linux_addrinfo_to_darwin(
	const struct addrinfo* linux_info)
{
	struct darwin_addrinfo* head = NULL;
	struct darwin_addrinfo** tail = &head;

	for (const struct addrinfo* current = linux_info; current;
	     current = current->ai_next) {
		struct darwin_addrinfo* entry;

		if (current->ai_family != AF_INET && current->ai_family != AF_INET6)
			continue;

		entry = calloc(1, sizeof(*entry));
		if (!entry) {
			free_darwin_addrinfo(head);
			errno = ENOMEM;
			return NULL;
		}

		entry->flags = current->ai_flags;
		entry->family = linux_family_to_darwin_addrinfo(current->ai_family);
		entry->socktype = current->ai_socktype;
		entry->protocol = current->ai_protocol;
		if (!copy_linux_sockaddr_to_darwin_addrinfo(current->ai_addr,
		                                            &entry->addr,
		                                            &entry->addrlen)) {
			free(entry);
			continue;
		}
		if (current->ai_canonname) {
			entry->canonname = strdup(current->ai_canonname);
			if (!entry->canonname) {
				free(entry->addr);
				free(entry);
				free_darwin_addrinfo(head);
				errno = ENOMEM;
				return NULL;
			}
		}

		*tail = entry;
		tail = &entry->next;
	}

	return head;
}

int getaddrinfo(const char* node, const char* service,
                const struct addrinfo* hints, struct addrinfo** result)
{
	int (*real_getaddrinfo)(const char*, const char*, const struct addrinfo*,
	                        struct addrinfo**) = dlsym(RTLD_NEXT,
	                                                   "getaddrinfo");
	void (*real_freeaddrinfo)(struct addrinfo*) = dlsym(RTLD_NEXT,
	                                                   "freeaddrinfo");
	const struct darwin_addrinfo* darwin_hints =
		(const struct darwin_addrinfo*)hints;
	struct addrinfo linux_hints;
	struct addrinfo* linux_result = NULL;
	struct darwin_addrinfo* darwin_result;
	int linux_error;

	if (!result)
		return DARWIN_EAI_FAIL;
	if (!real_getaddrinfo || !real_freeaddrinfo)
		return DARWIN_EAI_SYSTEM;

	*result = NULL;
	memset(&linux_hints, 0, sizeof(linux_hints));
	if (darwin_hints) {
		linux_hints.ai_flags = darwin_ai_flags_to_linux(darwin_hints->flags);
		linux_hints.ai_family =
			darwin_family_to_linux_addrinfo(darwin_hints->family);
		linux_hints.ai_socktype = darwin_hints->socktype;
		linux_hints.ai_protocol = darwin_hints->protocol;
	}

	errno = 0;
	linux_error = real_getaddrinfo(node, service,
	                               darwin_hints ? &linux_hints : NULL,
	                               &linux_result);
	if (linux_error != 0)
		return darwin_gai_error_from_linux(linux_error);

	darwin_result = copy_linux_addrinfo_to_darwin(linux_result);
	real_freeaddrinfo(linux_result);
	if (!darwin_result) {
		if (errno == ENOMEM)
			return DARWIN_EAI_MEMORY;
		return DARWIN_EAI_NONAME;
	}

	*result = (struct addrinfo*)darwin_result;
	if (shim_trace_enabled())
		fprintf(stderr, "libsystem_shim: getaddrinfo(%s,%s) -> 0 result=%p\n",
		        node ? node : "(nil)", service ? service : "(nil)",
		        (void*)darwin_result);
	return 0;
}

void freeaddrinfo(struct addrinfo* info)
{
	free_darwin_addrinfo((struct darwin_addrinfo*)info);
}

const char* gai_strerror(int error)
{
	const char* (*real_gai_strerror)(int) = dlsym(RTLD_NEXT, "gai_strerror");

	if (!real_gai_strerror)
		return "getaddrinfo error";
	return real_gai_strerror(linux_gai_error_from_darwin(error));
}

int machgate_xsi_strerror_r(int errnum, char* buf, size_t buflen)
{
	const char* message = strerror(errnum);
	size_t length;

	if (!message)
		return EINVAL;
	if (buflen == 0)
		return ERANGE;

	length = strlen(message);
	if (length >= buflen) {
		buf[0] = '\0';
		return ERANGE;
	}

	memcpy(buf, message, length + 1);
	return 0;
}

__asm__(".globl strerror_r\n\t.set strerror_r, machgate_xsi_strerror_r");

/* ===== Darwin process and filesystem surface ===== */

int mach_vm_region(uint32_t target_task, uint64_t* address, uint64_t* size,
                   int flavor, void* info, uint32_t* info_count,
                   uint32_t* object_name)
{
	(void)target_task;
	(void)address;
	(void)size;
	(void)flavor;
	(void)info;
	(void)info_count;
	(void)object_name;
	return 1;
}

int mach_vm_map(uint32_t target_task, uint64_t* address, uint64_t size,
                uint64_t mask, int flags, uint32_t object, uint64_t offset,
                uint8_t copy, int cur_protection, int max_protection,
                int inheritance)
{
	(void)target_task;
	(void)mask;
	(void)flags;
	(void)object;
	(void)offset;
	(void)copy;
	(void)max_protection;
	(void)inheritance;

	if (!address || !size)
		return 4;

	int protection = mach_vm_prot_to_linux(cur_protection);
	if (protection == PROT_NONE)
		protection = PROT_READ | PROT_WRITE;

	void* requested = *address ? (void*)(uintptr_t)*address : NULL;
	int mmap_flags = MAP_PRIVATE | MAP_ANONYMOUS;
	if (requested)
		mmap_flags |= MAP_FIXED_NOREPLACE;

	void* result = mmap(requested, (size_t)size, protection, mmap_flags, -1, 0);
	if (result == MAP_FAILED)
		return 3;

	*address = (uint64_t)(uintptr_t)result;
	return 0;
}

int mach_make_memory_entry_64(uint32_t target_task, uint64_t* size,
                              uint64_t offset, int permission,
                              uint32_t* object_handle, uint32_t parent_entry)
{
	(void)target_task;
	(void)size;
	(void)offset;
	(void)permission;
	(void)parent_entry;

	if (object_handle)
		*object_handle = 0;
	return 5;
}

int mach_vm_remap(uint32_t target_task, uint64_t* target_address,
                  uint64_t size, uint64_t mask, int flags,
                  uint32_t src_task, uint64_t src_address, uint8_t copy,
                  int* cur_protection, int* max_protection,
                  int inheritance)
{
	(void)target_task;
	(void)target_address;
	(void)size;
	(void)mask;
	(void)flags;
	(void)src_task;
	(void)src_address;
	(void)copy;
	(void)inheritance;

	if (cur_protection)
		*cur_protection = 0;
	if (max_protection)
		*max_protection = 0;
	return 5;
}

int vm_remap(uint32_t target_task, uint64_t* target_address,
             uint64_t size, uint64_t mask, int flags,
             uint32_t src_task, uint64_t src_address, uint8_t copy,
             int* cur_protection, int* max_protection, int inheritance)
{
	return mach_vm_remap(target_task, target_address, size, mask, flags,
	                     src_task, src_address, copy, cur_protection,
	                     max_protection, inheritance);
}

int proc_regionfilename(int pid, uint64_t address, void* buffer,
                        uint32_t buffer_size)
{
	(void)pid;
	(void)address;
	if (buffer && buffer_size)
		((char*)buffer)[0] = '\0';
	return 0;
}

int proc_listpids(uint32_t type, uint32_t typeinfo, void* buffer,
                  int buffer_size)
{
	(void)type;
	(void)typeinfo;

	if (!buffer)
		return (int)sizeof(pid_t);

	if (buffer_size < (int)sizeof(pid_t))
		return 0;

	pid_t pid = getpid();
	memcpy(buffer, &pid, sizeof(pid));
	return (int)sizeof(pid);
}

int proc_listallpids(void* buffer, int buffer_size)
{
	return proc_listpids(0, 0, buffer, buffer_size);
}

int proc_listchildpids(pid_t parent_pid, void* buffer, int buffer_size)
{
	(void)parent_pid;
	(void)buffer;
	(void)buffer_size;
	return 0;
}

int proc_pid_rusage(int pid, int flavor, void* buffer)
{
	(void)pid;
	(void)flavor;
	(void)buffer;
	errno = ENOTSUP;
	return -1;
}

int proc_pidfdinfo(int pid, int fd, int flavor, void* buffer, int buffer_size)
{
	(void)pid;
	(void)fd;
	(void)flavor;
	(void)buffer;
	(void)buffer_size;
	errno = ENOTSUP;
	return -1;
}

int proc_pidinfo(int pid, int flavor, uint64_t arg, void* buffer,
                 int buffer_size)
{
	(void)pid;
	(void)flavor;
	(void)arg;
	(void)buffer;
	(void)buffer_size;
	return 0;
}

int posix_spawn_file_actions_addinherit_np(void* file_actions, int filedes)
{
	(void)file_actions;
	(void)filedes;
	return 0;
}

struct shim_spawn_file_action {
	int type;
	int fd;
	int newfd;
	char* path;
	int flags;
	mode_t mode;
};

struct shim_spawn_file_actions {
	size_t count;
	size_t capacity;
	struct shim_spawn_file_action actions[];
};

struct shim_spawn_attr {
	short flags;
	pid_t pgroup;
};

#define SHIM_SPAWN_ACTION_CLOSE 1
#define SHIM_SPAWN_ACTION_DUP2 2
#define SHIM_SPAWN_ACTION_OPEN 3

static int grow_spawn_file_actions(struct shim_spawn_file_actions** actions_ptr)
{
	struct shim_spawn_file_actions* actions = *actions_ptr;
	size_t capacity = actions->capacity ? actions->capacity * 2 : 4;
	size_t size = sizeof(*actions) + capacity * sizeof(actions->actions[0]);
	actions = realloc(actions, size);
	if (!actions)
		return ENOMEM;
	memset(&actions->actions[actions->capacity], 0,
	       (capacity - actions->capacity) * sizeof(actions->actions[0]));
	actions->capacity = capacity;
	*actions_ptr = actions;
	return 0;
}

static int add_spawn_file_action(void** file_actions,
                                 struct shim_spawn_file_action action)
{
	struct shim_spawn_file_actions** actions_ptr;
	struct shim_spawn_file_actions* actions;
	int result;

	if (!file_actions || !*file_actions)
		return EINVAL;

	actions_ptr = (struct shim_spawn_file_actions**)file_actions;
	actions = *actions_ptr;
	if (actions->count == actions->capacity) {
		result = grow_spawn_file_actions(actions_ptr);
		if (result)
			return result;
		actions = *actions_ptr;
	}

	actions->actions[actions->count++] = action;
	return 0;
}

int posix_spawn_file_actions_init(void** file_actions)
{
	struct shim_spawn_file_actions* actions;
	size_t capacity = 4;
	size_t size = sizeof(*actions) + capacity * sizeof(actions->actions[0]);

	if (!file_actions)
		return EINVAL;

	actions = calloc(1, size);
	if (!actions)
		return ENOMEM;
	actions->capacity = capacity;
	*file_actions = actions;
	return 0;
}

int posix_spawn_file_actions_destroy(void** file_actions)
{
	struct shim_spawn_file_actions* actions;

	if (!file_actions || !*file_actions)
		return EINVAL;

	actions = *file_actions;
	for (size_t index = 0; index < actions->count; index++)
		free(actions->actions[index].path);
	free(actions);
	*file_actions = NULL;
	return 0;
}

int posix_spawn_file_actions_adddup2(void** file_actions, int fd, int newfd)
{
	struct shim_spawn_file_action action;

	if (fd < 0 || newfd < 0)
		return EBADF;

	memset(&action, 0, sizeof(action));
	action.type = SHIM_SPAWN_ACTION_DUP2;
	action.fd = fd;
	action.newfd = newfd;
	return add_spawn_file_action(file_actions, action);
}

int posix_spawn_file_actions_addclose(void** file_actions, int fd)
{
	struct shim_spawn_file_action action;

	if (fd < 0)
		return EBADF;

	memset(&action, 0, sizeof(action));
	action.type = SHIM_SPAWN_ACTION_CLOSE;
	action.fd = fd;
	return add_spawn_file_action(file_actions, action);
}

int posix_spawn_file_actions_addopen(void** file_actions, int fd,
                                     const char* path, int flags, mode_t mode)
{
	struct shim_spawn_file_action action;

	if (fd < 0)
		return EBADF;
	if (!path)
		return EINVAL;

	memset(&action, 0, sizeof(action));
	action.type = SHIM_SPAWN_ACTION_OPEN;
	action.fd = fd;
	action.path = strdup(path);
	if (!action.path)
		return ENOMEM;
	action.flags = flags;
	action.mode = mode;
	int result = add_spawn_file_action(file_actions, action);
	if (result)
		free(action.path);
	return result;
}

int posix_spawnattr_init(void** attr)
{
	if (!attr)
		return EINVAL;
	*attr = calloc(1, sizeof(struct shim_spawn_attr));
	return *attr ? 0 : ENOMEM;
}

int posix_spawnattr_destroy(void** attr)
{
	if (!attr || !*attr)
		return EINVAL;
	free(*attr);
	*attr = NULL;
	return 0;
}

int posix_spawnattr_setflags(void** attr, short flags)
{
	struct shim_spawn_attr* spawn_attr;

	if (!attr || !*attr)
		return EINVAL;
	spawn_attr = *attr;
	spawn_attr->flags = flags;
	return 0;
}

int posix_spawnattr_setpgroup(void** attr, pid_t pgroup)
{
	struct shim_spawn_attr* spawn_attr;

	if (!attr || !*attr)
		return EINVAL;
	spawn_attr = *attr;
	spawn_attr->pgroup = pgroup;
	return 0;
}

int posix_spawnattr_setsigdefault(void** attr, const void* sigdefault)
{
	if (!attr || !*attr || !sigdefault)
		return EINVAL;
	return 0;
}

static void apply_spawn_file_actions(struct shim_spawn_file_actions* actions)
{
	if (!actions)
		return;

	for (size_t index = 0; index < actions->count; index++) {
		struct shim_spawn_file_action* action = &actions->actions[index];
		switch (action->type) {
		case SHIM_SPAWN_ACTION_CLOSE:
			close(action->fd);
			break;
		case SHIM_SPAWN_ACTION_DUP2:
			dup2(action->fd, action->newfd);
			break;
		case SHIM_SPAWN_ACTION_OPEN: {
			int fd = libc_open(action->path, translate_oflags(action->flags),
			              action->mode);
			if (fd >= 0) {
				if (fd != action->fd) {
					dup2(fd, action->fd);
					close(fd);
				}
			}
			break;
		}
		default:
			break;
		}
	}
}

static void spawn_exec_path(const char* path, char* const argv[], char* const envp[])
{
	int result = machgate_execve_macho_guest_forksafe(path, argv, envp);
	if (result != 45)
		errno = result;

	syscall(SYS_execve, path, argv, envp ? envp : environ);
}

static void spawn_exec_search_path(const char* file, char* const argv[],
                                   char* const envp[])
{
	const char* path_env;
	int saved_errno = ENOENT;

	if (strchr(file, '/')) {
		spawn_exec_path(file, argv, envp);
		return;
	}

	path_env = getenv("PATH");
	if (!path_env || !*path_env)
		path_env = "/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin";

	while (*path_env) {
		char candidate[4096];
		const char* end = strchr(path_env, ':');
		size_t directory_len = end ? (size_t)(end - path_env) : strlen(path_env);
		if (!directory_len) {
			if (snprintf(candidate, sizeof(candidate), "./%s", file) >=
			    (int)sizeof(candidate)) {
				errno = ENAMETOOLONG;
				return;
			}
		} else {
			if (directory_len + 1 + strlen(file) + 1 > sizeof(candidate)) {
				errno = ENAMETOOLONG;
				return;
			}
			memcpy(candidate, path_env, directory_len);
			candidate[directory_len] = '/';
			strcpy(candidate + directory_len + 1, file);
		}

		spawn_exec_path(candidate, argv, envp);
		if (errno != ENOENT && errno != ENOTDIR)
			saved_errno = errno;
		if (!end)
			break;
		path_env = end + 1;
	}

	errno = saved_errno;
}

static int shim_posix_spawn_common(pid_t* pid, const char* path, void* file_actions,
                                   void* attr, char* const argv[],
                                   char* const envp[], int search_path)
{
	(void)attr;

	if (!pid || !path || !argv)
		return EINVAL;

	pid_t child = fork();
	if (child < 0)
		return errno;

	if (child == 0) {
		apply_spawn_file_actions(file_actions);
		if (search_path)
			spawn_exec_search_path(path, argv, envp);
		else
			spawn_exec_path(path, argv, envp);
		_exit(errno == ENOENT ? 127 : 126);
	}

	*pid = child;
	return 0;
}

int posix_spawn(pid_t* pid, const char* path, void* file_actions,
                void* attr, char* const argv[], char* const envp[])
{
	return shim_posix_spawn_common(pid, path, file_actions, attr, argv, envp, 0);
}

int posix_spawnp(pid_t* pid, const char* file, void* file_actions,
                 void* attr, char* const argv[], char* const envp[])
{
	return shim_posix_spawn_common(pid, file, file_actions, attr, argv, envp, 1);
}

int execve(const char* path, char* const argv[], char* const envp[])
{
	int fork_child = machgate_shim_in_fork_child();
	int result = fork_child ?
	             machgate_execve_macho_guest_forksafe(path, argv, envp) :
	             machgate_execve_macho_guest(path, argv, envp);

	if (!fork_child && (getenv("MACHGATE_TRACE_SYSCALL") || getenv("MACHGATE_TRACE_SHIM"))) {
		fprintf(stderr, "libsystem_shim: execve('%s') -> -1 errno=%d\n",
		        path, result);
	}
	errno = result;
	return -1;
}

/* ===== popen / pclose ===== */

/*
 * A guest popen("<mach-o-guest> args...", "r") command must not reach the host
 * shell: /bin/sh cannot exec a Mach-O, so the child would die with ENOEXEC
 * (exit 126) before printing a byte. When the first token of the command is
 * a Mach-O image we spawn it directly through the MachGate loader re-exec
 * path; everything else falls back to the real host popen.
 */

struct shim_popen_record {
	FILE* stream;
	pid_t child;
	struct shim_popen_record* next;
};

static struct shim_popen_record* shim_popen_records;
static pthread_mutex_t shim_popen_records_mutex = PTHREAD_MUTEX_INITIALIZER;

static int shim_path_is_macho(const char* path)
{
	unsigned char magic[4];
	int fd = (int)syscall(SYS_openat, AT_FDCWD, path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return 0;
	ssize_t bytes_read = (ssize_t)syscall(SYS_read, fd, magic, sizeof(magic));
	syscall(SYS_close, fd);
	if (bytes_read != (ssize_t)sizeof(magic))
		return 0;
	return (magic[0] == 0xcf && magic[1] == 0xfa &&
	        magic[2] == 0xed && magic[3] == 0xfe) ||
	       (magic[0] == 0xca && magic[1] == 0xfe &&
	        magic[2] == 0xba && magic[3] == 0xbe);
}

static int shim_popen_token_is_plain(const char* token)
{
	return strpbrk(token, "\"'`\\|;&<>()*?$~") == NULL;
}

static void shim_popen_child_exec(const char* guest_path, char* guest_argv[],
                                  int pipe_end, int redirect_to,
                                  int merge_stderr)
{
	if (redirect_to == STDOUT_FILENO) {
		syscall(SYS_dup3, pipe_end, STDOUT_FILENO, 0);
		if (merge_stderr)
			syscall(SYS_dup3, STDOUT_FILENO, STDERR_FILENO, 0);
	} else {
		syscall(SYS_dup3, pipe_end, STDIN_FILENO, 0);
	}
	if (pipe_end > STDERR_FILENO)
		syscall(SYS_close, pipe_end);

	int exec_result = machgate_execve_macho_guest_forksafe(
	    guest_path, guest_argv, environ);
	(void)exec_result;
	syscall(SYS_exit_group, exec_result == 45 ? 126 : 126);
}

static FILE* shim_popen_macho(const char* guest_path, char* guest_argv[],
                              int guest_argc, int redirect_to,
                              int merge_stderr, int close_on_exec)
{
	int pipe_fds[2];
	if (syscall(SYS_pipe2, pipe_fds, close_on_exec ? O_CLOEXEC : 0) < 0)
		return NULL;

	int read_end = pipe_fds[0];
	int write_end = pipe_fds[1];
	int child_end = redirect_to == STDOUT_FILENO ? write_end : read_end;
	int parent_end = redirect_to == STDOUT_FILENO ? read_end : write_end;

	pid_t child = fork();
	if (child < 0) {
		syscall(SYS_close, read_end);
		syscall(SYS_close, write_end);
		return NULL;
	}
	if (child == 0) {
		syscall(SYS_close, parent_end);
		shim_popen_child_exec(guest_path, guest_argv, child_end,
		                      redirect_to, merge_stderr);
	}

	syscall(SYS_close, child_end);
	FILE* stream = fdopen(parent_end, redirect_to == STDOUT_FILENO ? "r" : "w");
	if (!stream) {
		int saved_errno = errno;
		syscall(SYS_close, parent_end);
		syscall(SYS_kill, child, SIGKILL);
		syscall(SYS_wait4, child, NULL, 0, NULL);
		errno = saved_errno;
		return NULL;
	}

	struct shim_popen_record* record = malloc(sizeof(*record));
	if (!record) {
		fclose(stream);
		errno = ENOMEM;
		return NULL;
	}
	record->stream = stream;
	record->child = child;

	pthread_mutex_lock(&shim_popen_records_mutex);
	record->next = shim_popen_records;
	shim_popen_records = record;
	pthread_mutex_unlock(&shim_popen_records_mutex);
	return stream;
}

static FILE* shim_real_popen(const char* command, const char* type)
{
	static FILE* (*real_popen)(const char*, const char*);
	if (!real_popen)
		real_popen = dlsym(RTLD_NEXT, "popen");
	return real_popen ? real_popen(command, type) : NULL;
}

static int shim_real_pclose(FILE* stream)
{
	static int (*real_pclose)(FILE*);
	if (!real_pclose)
		real_pclose = dlsym(RTLD_NEXT, "pclose");
	return real_pclose ? real_pclose(stream) : -1;
}

FILE* popen(const char* command, const char* type)
{
	if (!command || !type)
	{
		errno = EINVAL;
		return NULL;
	}

	int redirect_to = STDOUT_FILENO;
	int close_on_exec = 0;
	for (const char* mode_char = type; *mode_char; mode_char++) {
		if (*mode_char == 'w')
			redirect_to = STDIN_FILENO;
		else if (*mode_char == 'e')
			close_on_exec = 1;
	}

	char* command_copy = strdup(command);
	if (!command_copy) {
		errno = ENOMEM;
		return NULL;
	}

	char* guest_argv[4096];
	int guest_argc = 0;
	int merge_stderr = 0;
	int plain_command = 1;
	char* save_ptr = NULL;
	for (char* token = strtok_r(command_copy, " \t", &save_ptr); token;
	     token = strtok_r(NULL, " \t", &save_ptr)) {
		if (strcmp(token, "2>&1") == 0) {
			merge_stderr = 1;
			continue;
		}
		if (!shim_popen_token_is_plain(token)) {
			plain_command = 0;
			break;
		}
		if (guest_argc == 4095) {
			errno = E2BIG;
			free(command_copy);
			return NULL;
		}
		guest_argv[guest_argc++] = token;
	}

	FILE* result = NULL;
	if (plain_command && guest_argc > 0 && shim_path_is_macho(guest_argv[0])) {
		guest_argv[guest_argc] = NULL;
		result = shim_popen_macho(guest_argv[0], guest_argv, guest_argc,
		                          redirect_to, merge_stderr, close_on_exec);
		free(command_copy);
		return result;
	}

	result = shim_real_popen(command, type);
	free(command_copy);
	return result;
}

int pclose(FILE* stream)
{
	if (!stream) {
		errno = EINVAL;
		return -1;
	}

	struct shim_popen_record* record = NULL;
	struct shim_popen_record* previous = NULL;

	pthread_mutex_lock(&shim_popen_records_mutex);
	for (record = shim_popen_records; record; previous = record, record = record->next) {
		if (record->stream == stream) {
			if (previous)
				previous->next = record->next;
			else
				shim_popen_records = record->next;
			break;
		}
	}
	pthread_mutex_unlock(&shim_popen_records_mutex);

	if (!record)
		return shim_real_pclose(stream);

	if (fclose(stream) != 0)
		return -1;

	int status = 0;
	for (;;) {
		pid_t waited = (pid_t)syscall(SYS_wait4, record->child, &status, 0, NULL);
		if (waited == record->child)
			break;
		if (waited < 0 && errno != EINTR) {
			free(record);
			return -1;
		}
	}
	free(record);
	return status;
}

static ssize_t shim_write_with_sigpipe_guard(long syscall_number, int fd,
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
	ssize_t result = syscall(syscall_number, fd, buffer, size_or_count);
	int saved_errno = errno;
	if (result < 0)
		saved_errno = shim_errno_from_linux(saved_errno);
	shim_fd_trace_log("write caller=%p fd=%d size=%zu result=%zd errno=%d\n",
	                  SHIM_CALLER_RETURN_ADDRESS(), fd, size_or_count,
	                  result, result < 0 ? saved_errno : 0);

	if (changed && old_action.sa_handler != SIG_DFL)
		sigaction(SIGPIPE, &old_action, NULL);

	errno = saved_errno;
	return result;
}

ssize_t read(int fd, void* buffer, size_t size)
{
	errno = 0;
	ssize_t result = (ssize_t)syscall(SYS_read, fd, buffer, size);
	int saved_errno = errno;
	if (result < 0)
		saved_errno = shim_errno_from_linux(saved_errno);
	shim_fd_trace_log("read caller=%p fd=%d size=%zu result=%zd errno=%d\n",
	                  SHIM_CALLER_RETURN_ADDRESS(), fd, size, result,
	                  result < 0 ? saved_errno : 0);
	if (shim_trace_enabled())
		fprintf(stderr, "libsystem_shim: read(fd=%d size=%zu) -> %zd errno=%d\n",
		        fd, size, result, result < 0 ? saved_errno : 0);
	errno = saved_errno;
	return result;
}

ssize_t readv(int fd, const struct iovec* iov, int iovcnt)
{
	errno = 0;
	ssize_t result = (ssize_t)syscall(SYS_readv, fd, iov, (int)iovcnt);
	int saved_errno = errno;
	if (result < 0)
		saved_errno = shim_errno_from_linux(saved_errno);
	shim_fd_trace_log("readv caller=%p fd=%d iovcnt=%d result=%zd errno=%d\n",
	                  SHIM_CALLER_RETURN_ADDRESS(), fd, iovcnt, result,
	                  result < 0 ? saved_errno : 0);
	if (shim_trace_enabled())
		fprintf(stderr, "libsystem_shim: readv(fd=%d iovcnt=%d) -> %zd errno=%d\n",
		        fd, iovcnt, result, result < 0 ? saved_errno : 0);
	errno = saved_errno;
	return result;
}

ssize_t write(int fd, const void* buffer, size_t size)
{
	return shim_write_with_sigpipe_guard(SYS_write, fd, buffer, size);
}

ssize_t writev(int fd, const struct iovec* iov, int iovcnt)
{
	return shim_write_with_sigpipe_guard(SYS_writev, fd, iov, (size_t)iovcnt);
}

ssize_t recvmsg_x(int fd, void* msgp, unsigned int cnt, int flags)
{
	(void)fd;
	(void)msgp;
	(void)cnt;
	(void)flags;
	errno = ENOSYS;
	return -1;
}

ssize_t sendmsg_x(int fd, void* msgp, unsigned int cnt, int flags)
{
	(void)fd;
	(void)msgp;
	(void)cnt;
	(void)flags;
	errno = ENOSYS;
	return -1;
}

int sscanf_l(const char* string, locale_t locale, const char* format, ...)
{
	(void)locale;

	va_list args;
	va_start(args, format);
	int result = vsscanf(string, format, args);
	va_end(args);
	return result;
}

int __darwin_check_fd_set_overflow(int fd, const void* fd_set,
                                   int select_size)
{
	(void)fd_set;
	(void)select_size;
	return fd >= 0 && fd < FD_SETSIZE;
}

int sysctlnametomib(const char* name, int* mib, size_t* mib_length)
{
	if (!name || !mib_length) {
		errno = EINVAL;
		return -1;
	}

	struct mib_entry {
		const char* name;
		int values[2];
		size_t length;
	};

static const struct mib_entry entries[] = {
	{ "kern.ostype", { 1, 1 }, 2 },
	{ "kern.osrelease", { 1, 2 }, 2 },
	{ "kern.version", { 1, 4 }, 2 },
	{ "kern.argmax", { 1, 8 }, 2 },
	{ "kern.hostname", { 1, 10 }, 2 },
	{ "kern.osversion", { 1, 65 }, 2 },
	{ "kern.osproductversion", { 1, 140 }, 2 },
	{ "kern.osproductversioncompat", { 1, 141 }, 2 },
	{ "hw.machine", { 6, 1 }, 2 },
	{ "hw.model", { 6, 2 }, 2 },
	{ "hw.ncpu", { 6, 3 }, 2 },
	{ "hw.byteorder", { 6, 4 }, 2 },
		{ "hw.memsize", { 6, 24 }, 2 },
		{ "hw.pagesize", { 6, 7 }, 2 },
		{ "hw.logicalcpu", { 6, 103 }, 2 },
		{ "hw.physicalcpu", { 6, 101 }, 2 },
		{ "hw.activecpu", { 6, 25 }, 2 },
	};

	for (size_t index = 0; index < sizeof(entries) / sizeof(entries[0]);
	     index++) {
		if (strcmp(name, entries[index].name) != 0)
			continue;
		if (!mib || *mib_length < entries[index].length) {
			*mib_length = entries[index].length;
			errno = ENOMEM;
			return -1;
		}
		memcpy(mib, entries[index].values,
		       entries[index].length * sizeof(*mib));
		*mib_length = entries[index].length;
		return 0;
	}

	errno = ENOENT;
	return -1;
}

#define DARWIN_MNT_RDONLY 0x00000001u
#define DARWIN_MNT_NOEXEC 0x00000004u
#define DARWIN_MNT_NOSUID 0x00000008u
#define DARWIN_MNT_NODEV 0x00000010u
#define DARWIN_MNT_LOCAL 0x00001000u

struct shim_darwin_statfs {
	uint32_t f_bsize;
	int32_t f_iosize;
	uint64_t f_blocks;
	uint64_t f_bfree;
	uint64_t f_bavail;
	uint64_t f_files;
	uint64_t f_ffree;
	int32_t f_fsid[2];
	uint32_t f_owner;
	uint32_t f_type;
	uint32_t f_flags;
	uint32_t f_fssubtype;
	char f_fstypename[16];
	char f_mntonname[1024];
	char f_mntfromname[1024];
	uint32_t f_flags_ext;
	uint32_t f_reserved[7];
};

static uint32_t shim_linux_mount_flags_to_darwin(uint64_t linux_flags)
{
	uint32_t darwin_flags = DARWIN_MNT_LOCAL;

	if (linux_flags & MS_RDONLY)
		darwin_flags |= DARWIN_MNT_RDONLY;
	if (linux_flags & MS_NOEXEC)
		darwin_flags |= DARWIN_MNT_NOEXEC;
	if (linux_flags & MS_NOSUID)
		darwin_flags |= DARWIN_MNT_NOSUID;
	if (linux_flags & MS_NODEV)
		darwin_flags |= DARWIN_MNT_NODEV;

	return darwin_flags;
}

static void shim_linux_to_darwin_statfs(const struct statfs64* linux_statfs,
                                        struct shim_darwin_statfs* darwin_statfs)
{
	memset(darwin_statfs, 0, sizeof(*darwin_statfs));
	darwin_statfs->f_bsize = (uint32_t)linux_statfs->f_bsize;
	darwin_statfs->f_iosize = (int32_t)linux_statfs->f_frsize;
	darwin_statfs->f_blocks = linux_statfs->f_blocks;
	darwin_statfs->f_bfree = linux_statfs->f_bfree;
	darwin_statfs->f_bavail = linux_statfs->f_bavail;
	darwin_statfs->f_files = linux_statfs->f_files;
	darwin_statfs->f_ffree = linux_statfs->f_ffree;
	darwin_statfs->f_fsid[0] = linux_statfs->f_fsid.__val[0];
	darwin_statfs->f_fsid[1] = linux_statfs->f_fsid.__val[1];
	darwin_statfs->f_owner = (uint32_t)getuid();
	darwin_statfs->f_type = (uint32_t)linux_statfs->f_type;
	darwin_statfs->f_flags =
		shim_linux_mount_flags_to_darwin((uint64_t)linux_statfs->f_flags);
	strcpy(darwin_statfs->f_fstypename, "hfs");
	strcpy(darwin_statfs->f_mntonname, "/");
	strcpy(darwin_statfs->f_mntfromname, "/dev/disk0s1");
}

int shim_statfs(const char* path, struct shim_darwin_statfs* darwin_statfs)
	__asm__("statfs");
int shim_statfs(const char* path, struct shim_darwin_statfs* darwin_statfs)
{
	struct statfs64 linux_statfs;

	if (!path || !darwin_statfs) {
		errno = EFAULT;
		return -1;
	}
	if (syscall(SYS_statfs, path, &linux_statfs) != 0)
		return -1;
	shim_linux_to_darwin_statfs((const struct statfs*)&linux_statfs,
	                            darwin_statfs);
	return 0;
}

int shim_fstatfs(int fd, struct shim_darwin_statfs* darwin_statfs)
	__asm__("fstatfs");
int shim_fstatfs(int fd, struct shim_darwin_statfs* darwin_statfs)
{
	struct statfs64 linux_statfs;

	if (fd < 0 || !darwin_statfs) {
		errno = EFAULT;
		return -1;
	}
	if (syscall(SYS_fstatfs, fd, &linux_statfs) != 0)
		return -1;
	shim_linux_to_darwin_statfs((const struct statfs*)&linux_statfs,
	                            darwin_statfs);
	return 0;
}

int getfsstat(struct shim_darwin_statfs* buffer, int buffer_size, int flags)
{
	(void)buffer;
	(void)buffer_size;
	(void)flags;
	return 0;
}

int fsctl(const char* path, unsigned long request, void* data,
          unsigned int options)
{
	(void)path;
	(void)request;
	(void)data;
	(void)options;
	errno = ENOTSUP;
	return -1;
}

int gethostuuid(uint8_t* uuid, const struct timespec* wait)
{
	(void)wait;

	static const uint8_t machgate_uuid[16] = {
		0x6d, 0x61, 0x63, 0x68, 0x67, 0x61, 0x74, 0x65,
		0x2d, 0x6c, 0x69, 0x6e, 0x75, 0x78, 0x00, 0x01
	};

	if (!uuid) {
		errno = EINVAL;
		return -1;
	}

	memcpy(uuid, machgate_uuid, sizeof(machgate_uuid));
	return 0;
}

void srandomdev(void)
{
	unsigned int seed = 0;
	if (syscall(SYS_getrandom, &seed, sizeof(seed), 0) != sizeof(seed))
		seed = (unsigned int)time(NULL) ^ (unsigned int)getpid();
	srandom(seed);
}

struct darwin_arch_info {
	const char* name;
	int32_t cpu_type;
	int32_t cpu_subtype;
	int32_t byte_order;
	const char* description;
};

const struct darwin_arch_info* NXGetArchInfoFromCpuType(int32_t cpu_type,
                                                        int32_t cpu_subtype)
{
	(void)cpu_subtype;

	static const struct darwin_arch_info arm64_info = {
		"arm64", 0x0100000c, 0, 0, "arm64"
	};

	if (cpu_type == arm64_info.cpu_type)
		return &arm64_info;
	return NULL;
}

void* acl_dup(void* acl)
{
	(void)acl;
	errno = ENOTSUP;
	return NULL;
}

int acl_free(void* object)
{
	free(object);
	return 0;
}

void* acl_get_fd(int fd)
{
	(void)fd;
	errno = ENOTSUP;
	return NULL;
}

int acl_set_fd(int fd, void* acl)
{
	(void)fd;
	(void)acl;
	errno = ENOTSUP;
	return -1;
}

int copyfile(const char* from, const char* to, void* state, uint32_t flags)
{
	(void)state;
	(void)flags;

	if (!from || !to) {
		errno = EINVAL;
		return -1;
	}

	int input_fd = libc_open(from, O_RDONLY, 0);
	if (input_fd < 0)
		return -1;

	int output_fd = libc_open(to, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (output_fd < 0) {
		int saved_errno = errno;
		close(input_fd);
		errno = saved_errno;
		return -1;
	}

	char buffer[16384];
	for (;;) {
		ssize_t bytes_read = read(input_fd, buffer, sizeof(buffer));
		if (bytes_read == 0)
			break;
		if (bytes_read < 0) {
			int saved_errno = errno;
			close(input_fd);
			close(output_fd);
			errno = saved_errno;
			return -1;
		}

		char* cursor = buffer;
		while (bytes_read > 0) {
			ssize_t bytes_written = write(output_fd, cursor,
			                              (size_t)bytes_read);
			if (bytes_written < 0) {
				int saved_errno = errno;
				close(input_fd);
				close(output_fd);
				errno = saved_errno;
				return -1;
			}
			cursor += bytes_written;
			bytes_read -= bytes_written;
		}
	}

	int result = 0;
	if (close(output_fd) < 0)
		result = -1;
	if (close(input_fd) < 0)
		result = -1;
	return result;
}

struct copyfile_state {
	uint32_t flags;
};

void* copyfile_state_alloc(void)
{
	return calloc(1, sizeof(struct copyfile_state));
}

int copyfile_state_free(void* state)
{
	free(state);
	return 0;
}

int copyfile_state_get(void* state, uint32_t flag, void* value)
{
	(void)state;
	(void)flag;
	(void)value;
	errno = EINVAL;
	return -1;
}

int fcopyfile(int from_fd, int to_fd, void* state, uint32_t flags)
{
	(void)state;
	(void)flags;

	char buffer[16384];
	for (;;) {
		ssize_t bytes_read = read(from_fd, buffer, sizeof(buffer));
		if (bytes_read == 0)
			return 0;
		if (bytes_read < 0)
			return -1;

		char* cursor = buffer;
		while (bytes_read > 0) {
			ssize_t bytes_written = write(to_fd, cursor, (size_t)bytes_read);
			if (bytes_written < 0)
				return -1;
			cursor += bytes_written;
			bytes_read -= bytes_written;
		}
	}
}

int clonefile(const char* from, const char* to, uint32_t flags)
{
	return copyfile(from, to, NULL, flags);
}

int clonefileat(int from_dirfd, const char* from, int to_dirfd,
                const char* to, uint32_t flags)
{
	(void)flags;

	if (!from || !to) {
		errno = EINVAL;
		return -1;
	}

	int input_fd = openat(from_dirfd, from, O_RDONLY);
	if (input_fd < 0)
		return -1;
	int output_fd = openat(to_dirfd, to, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (output_fd < 0) {
		int saved_errno = errno;
		close(input_fd);
		errno = saved_errno;
		return -1;
	}

	int result = fcopyfile(input_fd, output_fd, NULL, 0);
	int saved_errno = errno;
	close(input_fd);
	close(output_fd);
	errno = saved_errno;
	return result;
}

int fclonefileat(int from_dirfd, const char* from, int to_dirfd,
                 const char* to, uint32_t flags)
{
	return clonefileat(from_dirfd, from, to_dirfd, to, flags);
}

int fsetattrlist(int fd, const void* attr_list, void* attr_buf,
                 size_t attr_buf_size, uint32_t options)
{
	(void)fd;
	(void)attr_list;
	(void)attr_buf;
	(void)attr_buf_size;
	(void)options;
	return 0;
}

int lchflags(const char* path, uint32_t flags)
{
	(void)path;
	(void)flags;
	return 0;
}

int atexit(void (*function)(void))
{
	(void)function;
	return 0;
}

int __cxa_atexit(void (*function)(void*), void* arg, void* dso_handle)
{
	(void)function;
	(void)arg;
	(void)dso_handle;
	return 0;
}

void __cxa_finalize(void* dso_handle)
{
	(void)dso_handle;
}

#define SHIM_CXA_GUARD_COMPLETE 0x01
#define SHIM_CXA_GUARD_PENDING 0x02
#define SHIM_CXA_GUARD_WAITING 0x04
#define SHIM_CXA_GUARD_STATE_SHIFT 8
#define SHIM_CXA_GUARD_STATE_MASK 0xff00U
#define SHIM_CXA_GUARD_STALL_LIMIT 30000

static uint32_t shim_cxa_guard_state_from_word(uint32_t word)
{
	return (word >> SHIM_CXA_GUARD_STATE_SHIFT) & 0xffU;
}

static uint32_t shim_cxa_guard_word_with_state(uint32_t word, uint32_t state)
{
	return (word & ~SHIM_CXA_GUARD_STATE_MASK) |
	       ((state & 0xffU) << SHIM_CXA_GUARD_STATE_SHIFT);
}

static int shim_cxa_guard_futex_wait(uint32_t* word, uint32_t expected)
{
	struct timespec timeout;

	timeout.tv_sec = 0;
	timeout.tv_nsec = 1000000L;
	return (int)syscall(SYS_futex, word, FUTEX_WAIT_PRIVATE, expected,
	                    &timeout, NULL, 0);
}

static void shim_cxa_guard_futex_wake(uint32_t* word)
{
	syscall(SYS_futex, word, FUTEX_WAKE_PRIVATE, INT32_MAX, NULL, NULL, 0);
}

int __cxa_guard_acquire(uint64_t* guard)
{
	uint32_t* words = (uint32_t*)guard;
	uint32_t self;
	int wait_count = 0;

	if (!guard)
		return 0;

	self = (uint32_t)syscall(SYS_gettid);

	for (;;) {
		uint32_t word = __atomic_load_n(&words[0], __ATOMIC_ACQUIRE);
		uint32_t state = shim_cxa_guard_state_from_word(word);

		if (shim_cxx_init_full_trace_enabled()) {
			fprintf(stderr,
			        "libsystem_shim: __cxa_guard_acquire guard=%p value=%#llx initialized=%u state=%#x owner=%u caller=%p\n",
			        guard, (unsigned long long)*guard, word & 0xffU, state,
			        __atomic_load_n(&words[1], __ATOMIC_ACQUIRE),
			        __builtin_return_address(0));
			trace_guest_address_context("__cxa_guard_acquire.caller",
			                            (uintptr_t)__builtin_return_address(0));
		}

		if ((word & SHIM_CXA_GUARD_COMPLETE) ||
		    state == SHIM_CXA_GUARD_COMPLETE)
			return 0;

		if (state & SHIM_CXA_GUARD_PENDING) {
			uint32_t owner = __atomic_load_n(&words[1], __ATOMIC_ACQUIRE);
			uint32_t expected;
			uint32_t desired;

			if (owner == self) {
				fprintf(stderr,
				        "libsystem_shim: recursive __cxa_guard_acquire(%p)\n",
				        guard);
				abort();
			}

			expected = word;
			desired = word | (SHIM_CXA_GUARD_WAITING <<
			                  SHIM_CXA_GUARD_STATE_SHIFT);
			if (!(state & SHIM_CXA_GUARD_WAITING))
				__atomic_compare_exchange_n(&words[0], &expected, desired, 0,
				                            __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);

			if (++wait_count >= SHIM_CXA_GUARD_STALL_LIMIT) {
				fprintf(stderr,
				        "libsystem_shim: stalled __cxa_guard_acquire(%p) owner=%u state=%#x\n",
				        guard, owner, state);
				abort();
			}

			shim_cxa_guard_futex_wait(&words[0], __atomic_load_n(&words[0], __ATOMIC_ACQUIRE));
			continue;
		}

		{
			uint32_t expected = word;
			uint32_t desired = shim_cxa_guard_word_with_state(
			    word, SHIM_CXA_GUARD_PENDING);
			if (__atomic_compare_exchange_n(&words[0], &expected, desired, 0,
			                                __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
				__atomic_store_n(&words[1], self, __ATOMIC_RELEASE);
				if (shim_cxx_init_full_trace_enabled())
					fprintf(stderr,
					        "libsystem_shim: __cxa_guard_acquire guard=%p -> run initializer owner=%u\n",
					        guard, self);
				return 1;
			}
		}
	}
}

void __cxa_guard_release(uint64_t* guard)
{
	uint32_t* words = (uint32_t*)guard;
	unsigned char* bytes = (unsigned char*)guard;
	unsigned char old_state;

	if (!guard)
		return;
	__atomic_store_n(&bytes[0], SHIM_CXA_GUARD_COMPLETE, __ATOMIC_RELEASE);
	old_state = __atomic_exchange_n(&bytes[1], SHIM_CXA_GUARD_COMPLETE,
	                                __ATOMIC_ACQ_REL);
	__atomic_store_n(&words[1], 0, __ATOMIC_RELEASE);
	if (old_state & SHIM_CXA_GUARD_WAITING)
		shim_cxa_guard_futex_wake(&words[0]);
	if (shim_cxx_init_full_trace_enabled())
		fprintf(stderr, "libsystem_shim: __cxa_guard_release guard=%p value=%#llx caller=%p\n",
		        guard, (unsigned long long)*guard,
		        __builtin_return_address(0));
}

void __cxa_guard_abort(uint64_t* guard)
{
	uint32_t* words = (uint32_t*)guard;
	unsigned char* bytes = (unsigned char*)guard;
	unsigned char old_state;

	if (!guard)
		return;
	__atomic_store_n(&words[1], 0, __ATOMIC_RELEASE);
	old_state = __atomic_exchange_n(&bytes[1], 0x00, __ATOMIC_ACQ_REL);
	if (old_state & SHIM_CXA_GUARD_WAITING)
		shim_cxa_guard_futex_wake(&words[0]);
	if (shim_cxx_init_full_trace_enabled())
		fprintf(stderr, "libsystem_shim: __cxa_guard_abort guard=%p value=%#llx caller=%p\n",
		        guard, (unsigned long long)*guard,
		        __builtin_return_address(0));
}

static void trace_process_exit_code(const char* name, int status, void* caller)
{
	FILE* trace_file = shim_open_trace_file();
	char* guest_cookie;
	const char* host_cookie;

	if (!trace_file)
		return;

	guest_cookie = getenv_from_guest_envp("PACKER_WRAP_COOKIE");
	host_cookie = getenv("PACKER_WRAP_COOKIE");
	fprintf(trace_file,
	        "libsystem_shim: %s pid=%d tid=%d ppid=%d fork_child=%d status=%d caller=%p guest_cookie=%d host_cookie=%d last_wait_valid=%d last_wait_owner=%d last_wait_result=%d last_wait_linux_status=%#x last_wait_darwin_status=%#x last_wait_status_ptr=%#lx\n",
	        name, (int)syscall(SYS_getpid), (int)shim_trace_tid(),
	        (int)syscall(SYS_getppid), machgate_shim_in_fork_child(), status,
	        caller, guest_cookie ? 1 : 0,
	        host_cookie && *host_cookie ? 1 : 0, shim_last_wait_valid,
	        (int)shim_last_wait_owner_pid,
	        (int)shim_last_wait_result_pid,
	        shim_last_wait_linux_status,
	        shim_last_wait_darwin_status,
	        (unsigned long)shim_last_wait_status_ptr);
	fclose(trace_file);
}

void exit(int status)
{
	trace_process_exit_code("exit", status, SHIM_CALLER_RETURN_ADDRESS());
	shim_run_tlv_term_funcs();
	shim_run_thread_tsd_destructors();
	syscall(SYS_exit_group, status);
	__builtin_unreachable();
}

void _exit(int status)
{
	trace_process_exit_code("_exit", status, SHIM_CALLER_RETURN_ADDRESS());
	syscall(SYS_exit_group, status);
	__builtin_unreachable();
}

static void tlv_print_exit_backtrace(void)
{
	void* frame_pointer;
	char line[512];
	int position = 0;

	__asm__ volatile("mov %0, x29" : "=r"(frame_pointer));

	position += snprintf(line + position, sizeof(line) - position,
	                    "libsystem_shim: exit backtrace");
	for (int depth = 0; depth < 16 && frame_pointer; depth++) {
		void** frame = frame_pointer;
		void* return_address = frame[1];
		if (!return_address)
			break;
		position += snprintf(line + position, sizeof(line) - position,
		                     " #%d=%p", depth, return_address);
		if (position >= (int)sizeof(line) - 32)
			break;
		frame_pointer = frame[0];
		if (!frame_pointer || (uintptr_t)frame_pointer <= (uintptr_t)frame)
			break;
	}
	position += snprintf(line + position, sizeof(line) - position, "\n");
	fwrite(line, 1, position, stderr);
}

void _Exit(int status)
{
	trace_process_exit_code("_Exit", status, SHIM_CALLER_RETURN_ADDRESS());
	if (shim_trace_enabled())
		tlv_print_exit_backtrace();
	syscall(SYS_exit_group, status);
	__builtin_unreachable();
}

void quick_exit(int status)
{
	trace_process_exit_code("quick_exit", status, SHIM_CALLER_RETURN_ADDRESS());
	syscall(SYS_exit_group, status);
	__builtin_unreachable();
}

static int shim_errno_from_linux(int linux_errno)
{
	switch (linux_errno) {
	case EPERM: return 1;
	case ENOENT: return 2;
	case ESRCH: return 3;
	case EINTR: return 4;
	case EIO: return 5;
	case ENXIO: return 6;
	case E2BIG: return 7;
	case ENOEXEC: return 8;
	case EBADF: return 9;
	case ECHILD: return 10;
	case EDEADLK: return 11;
	case ENOMEM: return 12;
	case EACCES: return 13;
	case EFAULT: return 14;
	case EBUSY: return 16;
	case EEXIST: return 17;
	case EXDEV: return 18;
	case ENODEV: return 19;
	case ENOTDIR: return 20;
	case EISDIR: return 21;
	case EINVAL: return 22;
	case ENFILE: return 23;
	case EMFILE: return 24;
	case ENOTTY: return 25;
	case EFBIG: return 27;
	case ENOSPC: return 28;
	case ESPIPE: return 29;
	case EROFS: return 30;
	case EMLINK: return 31;
	case EPIPE: return 32;
	case EDOM: return 33;
	case ERANGE: return 34;
	case EAGAIN: return 35;
	case EINPROGRESS: return 36;
	case EALREADY: return 37;
	case ENOTSOCK: return 38;
	case EDESTADDRREQ: return 39;
	case EMSGSIZE: return 40;
	case EPROTOTYPE: return 41;
	case ENOPROTOOPT: return 42;
	case EPROTONOSUPPORT: return 43;
	case ENOTSUP: return 45;
	case EAFNOSUPPORT: return 47;
	case EADDRINUSE: return 48;
	case EADDRNOTAVAIL: return 49;
	case ENETDOWN: return 50;
	case ENETUNREACH: return 51;
	case ENETRESET: return 52;
	case ECONNABORTED: return 53;
	case ECONNRESET: return 54;
	case ENOBUFS: return 55;
	case EISCONN: return 56;
	case ENOTCONN: return 57;
	case ESHUTDOWN: return 58;
	case ETIMEDOUT: return 60;
	case ECONNREFUSED: return 61;
	case ELOOP: return 62;
	case ENAMETOOLONG: return 63;
	case EHOSTUNREACH: return 65;
	case ENOTEMPTY: return 66;
	case EDQUOT: return 69;
	case ESTALE: return 70;
	case ENOLCK: return 77;
	case EOVERFLOW: return 84;
	case ECANCELED: return 89;
	case EILSEQ: return 92;
	default: return linux_errno;
	}
}

/* ===== Apple stdio globals ===== */

/* Apple's libSystem exports these as global FILE* pointers.
 * We define them as initialized globals pointing to glibc's streams.
 * Note: these are initialized at load time via __attribute__((constructor)). */
FILE* __stdinp;
FILE* __stdoutp;
FILE* __stderrp;

__attribute__((constructor))
static void init_stdio_globals(void)
{
	__stdinp  = stdin;
	__stdoutp = stdout;
	__stderrp = stderr;
}


/* ===== errno ===== */

/* Apple's __error() returns a pointer to errno (like __errno_location on Linux) */
int* __error(void)
{
	return __errno_location();
}

/* ===== Stack protector ===== */

/* __chkstk_darwin is a stack probing function. On Linux, the kernel handles
 * stack growth automatically via guard pages, so this is a no-op. */
void __chkstk_darwin(void)
{
	/* no-op */
}

/* __stack_chk_guard and __stack_chk_fail are provided by glibc.
 * We don't need to define them — they'll resolve through normal linking. */

/* ===== Math shims ===== */

/* Adapted from Darling's src/libm/Source/sincos.c */

struct __float2 { float __sinval; float __cosval; };
struct __double2 { double __sinval; double __cosval; };

struct __float2 __sincosf_stret(float v)
{
	struct __float2 rv = { sinf(v), cosf(v) };
	return rv;
}

struct __double2 __sincos_stret(double v)
{
	struct __double2 rv = { sin(v), cos(v) };
	return rv;
}

struct __float2 __sincospif_stret(float v)
{
	return __sincosf_stret(v * (float)M_PI);
}

struct __double2 __sincospi_stret(double v)
{
	return __sincos_stret(v * M_PI);
}

/* Adapted from Darling's src/libm/Source/exp10.c */

double __exp10(double x)
{
	return pow(10.0, x);
}

float __exp10f(float x)
{
	return powf(10.0f, x);
}

/* ===== HOME path rewrite ===== */

/*
 * macOS games use $HOME/Library/Preferences/ for save data.
 * Redirect $HOME to a "userdata" directory next to the game binary
 * so we don't pollute the user's Linux home directory with macOS
 * directory structures.
 *
 * The game calls getenv("HOME") and appends "/Library/Preferences/...".
 * By returning "./userdata" (relative to CWD, which machgate sets to the
 * binary's directory), saves go to <game_dir>/userdata/Library/Preferences/.
 */
static char fake_home[4096] = {0};
static pthread_once_t fake_home_once = PTHREAD_ONCE_INIT;

static void init_fake_home(void)
{
	/* MACHGATE_HOME overrides the default userdata location */
	const char *override = getenv("MACHGATE_HOME");
	if (override && override[0]) {
		snprintf(fake_home, sizeof(fake_home), "%.4095s", override);
	} else {
		/* Build path: <directory of machgate binary>/userdata */
		char exe[4096];
		ssize_t len = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
		if (len > 0) {
			exe[len] = '\0';
			char *slash = strrchr(exe, '/');
			if (slash) {
				*slash = '\0';
				snprintf(fake_home, sizeof(fake_home),
					 "%.4085s/userdata", exe);
			}
		}
		if (!fake_home[0])
			snprintf(fake_home, sizeof(fake_home), "./userdata");
	}

	if (shim_startup_log_enabled())
		fprintf(stderr, "libsystem_shim: HOME rewritten to %s\n", fake_home);
}

static const char* get_fake_home(void)
{
	int result = pthread_once(&fake_home_once, init_fake_home);
	if (result != 0 && !fake_home[0])
		snprintf(fake_home, sizeof(fake_home), "./userdata");
	return fake_home;
}

static char* getenv_from_guest_envp(const char* name)
{
	char*** machgate_envp = dlsym(RTLD_DEFAULT, "__machgate_guest_envp");
	size_t name_len;

	if (!name || !machgate_envp || !*machgate_envp)
		return NULL;

	name_len = strlen(name);
	for (size_t env_index = 0; (*machgate_envp)[env_index]; env_index++) {
		char* entry = (*machgate_envp)[env_index];
		if (strncmp(entry, name, name_len) == 0 && entry[name_len] == '=')
			return entry + name_len + 1;
	}

	return NULL;
}

char *shim_getenv(const char *name) __asm__("getenv");
char *shim_getenv(const char *name)
{
	if (name && strcmp(name, "HOME") == 0)
		return (char*)get_fake_home();

	char* guest_value = getenv_from_guest_envp(name);
	if (guest_value) {
		if (name && strcmp(name, "PACKER_WRAP_COOKIE") == 0 &&
		    (shim_trace_enabled() || shim_wait_trace_enabled())) {
			fprintf(stderr, "libsystem_shim: getenv PACKER_WRAP_COOKIE guest=1\n");
		}
		return guest_value;
	}

	static char *(*real_getenv)(const char*) = NULL;
	if (!real_getenv)
		real_getenv = dlsym(RTLD_NEXT, "getenv");
	char* result = real_getenv(name);
	if (name && strcmp(name, "PACKER_WRAP_COOKIE") == 0 &&
	    (shim_trace_enabled() || shim_wait_trace_enabled())) {
		fprintf(stderr, "libsystem_shim: getenv PACKER_WRAP_COOKIE guest=0 host=%d\n",
		        result ? 1 : 0);
	}
	return result;
}

/* ===== Darwin struct passwd marshaling ===== */

struct darwin_passwd {
	char *pw_name;
	char *pw_passwd;
	unsigned int pw_uid;
	unsigned int pw_gid;
	long long pw_change;
	char *pw_class;
	char *pw_gecos;
	char *pw_dir;
	char *pw_shell;
	long long pw_expire;
};

static struct darwin_passwd shim_passwd_storage;
static char shim_passwd_string_storage[2048];

static int (*shim_real_getpwuid_r)(uid_t, struct passwd*, char*, size_t, struct passwd**);
static int (*shim_real_getpwnam_r)(const char*, struct passwd*, char*, size_t, struct passwd**);

static int shim_resolve_passwd_lookups(void)
{
	if (!shim_real_getpwuid_r)
		shim_real_getpwuid_r = dlsym(RTLD_NEXT, "getpwuid_r");
	if (!shim_real_getpwnam_r)
		shim_real_getpwnam_r = dlsym(RTLD_NEXT, "getpwnam_r");
	return shim_real_getpwuid_r && shim_real_getpwnam_r;
}

static char *shim_copy_passwd_string(char **cursor, size_t *remaining, const char *value)
{
	if (!value)
		return NULL;
	size_t length = strlen(value) + 1;
	if (length > *remaining)
		return NULL;
	char *result = *cursor;
	memcpy(result, value, length);
	*cursor += length;
	*remaining -= length;
	return result;
}

static struct darwin_passwd *shim_marshal_passwd_static(const struct passwd *host)
{
	if (!host)
		return NULL;

	char *cursor = shim_passwd_string_storage;
	size_t remaining = sizeof(shim_passwd_string_storage);

	shim_passwd_storage.pw_name = shim_copy_passwd_string(&cursor, &remaining, host->pw_name);
	shim_passwd_storage.pw_passwd = shim_copy_passwd_string(&cursor, &remaining, host->pw_passwd);
	shim_passwd_storage.pw_uid = host->pw_uid;
	shim_passwd_storage.pw_gid = host->pw_gid;
	shim_passwd_storage.pw_change = 0;
	shim_passwd_storage.pw_class = shim_copy_passwd_string(&cursor, &remaining, "");
	shim_passwd_storage.pw_gecos = shim_copy_passwd_string(&cursor, &remaining, host->pw_gecos);
	shim_passwd_storage.pw_dir = shim_copy_passwd_string(&cursor, &remaining, host->pw_dir);
	shim_passwd_storage.pw_shell = shim_copy_passwd_string(&cursor, &remaining, host->pw_shell);
	shim_passwd_storage.pw_expire = 0;
	return &shim_passwd_storage;
}

static int shim_marshal_passwd_caller(const struct passwd *host,
                                      struct darwin_passwd *pwd,
                                      char *buffer,
                                      size_t buffer_size,
                                      struct darwin_passwd **result)
{
	if (!pwd || !buffer || !result || !host)
		return EINVAL;

	char *cursor = buffer;
	size_t remaining = buffer_size;
	size_t needed = 0;
	if (host->pw_name) needed += strlen(host->pw_name) + 1;
	if (host->pw_passwd) needed += strlen(host->pw_passwd) + 1;
	needed += 1;
	if (host->pw_gecos) needed += strlen(host->pw_gecos) + 1;
	if (host->pw_dir) needed += strlen(host->pw_dir) + 1;
	if (host->pw_shell) needed += strlen(host->pw_shell) + 1;
	if (needed > remaining)
		return ERANGE;

	pwd->pw_name = shim_copy_passwd_string(&cursor, &remaining, host->pw_name);
	pwd->pw_passwd = shim_copy_passwd_string(&cursor, &remaining, host->pw_passwd);
	pwd->pw_uid = host->pw_uid;
	pwd->pw_gid = host->pw_gid;
	pwd->pw_change = 0;
	pwd->pw_class = shim_copy_passwd_string(&cursor, &remaining, "");
	pwd->pw_gecos = shim_copy_passwd_string(&cursor, &remaining, host->pw_gecos);
	pwd->pw_dir = shim_copy_passwd_string(&cursor, &remaining, host->pw_dir);
	pwd->pw_shell = shim_copy_passwd_string(&cursor, &remaining, host->pw_shell);
	pwd->pw_expire = 0;
	*result = pwd;
	return 0;
}

struct darwin_passwd *shim_getpwuid(uid_t uid) __asm__("getpwuid");
struct darwin_passwd *shim_getpwuid(uid_t uid)
{
	if (!shim_resolve_passwd_lookups())
		return NULL;

	struct passwd host;
	struct passwd *host_result = NULL;
	char host_buffer[2048];
	int status = shim_real_getpwuid_r(uid, &host, host_buffer, sizeof(host_buffer), &host_result);
	if (status != 0) {
		errno = status;
		return NULL;
	}
	return shim_marshal_passwd_static(host_result);
}

struct darwin_passwd *shim_getpwnam(const char *name) __asm__("getpwnam");
struct darwin_passwd *shim_getpwnam(const char *name)
{
	if (!name || !shim_resolve_passwd_lookups())
		return NULL;

	struct passwd host;
	struct passwd *host_result = NULL;
	char host_buffer[2048];
	int status = shim_real_getpwnam_r(name, &host, host_buffer, sizeof(host_buffer), &host_result);
	if (status != 0) {
		errno = status;
		return NULL;
	}
	return shim_marshal_passwd_static(host_result);
}

int shim_getpwuid_r(uid_t uid,
                    struct darwin_passwd *pwd,
                    char *buffer,
                    size_t buffer_size,
                    struct darwin_passwd **result) __asm__("getpwuid_r");
int shim_getpwuid_r(uid_t uid,
                    struct darwin_passwd *pwd,
                    char *buffer,
                    size_t buffer_size,
                    struct darwin_passwd **result)
{
	if (!shim_resolve_passwd_lookups())
		return ENOENT;

	struct passwd host;
	struct passwd *host_result = NULL;
	char host_buffer[2048];
	int status = shim_real_getpwuid_r(uid, &host, host_buffer, sizeof(host_buffer), &host_result);
	if (status != 0)
		return status;
	if (!host_result) {
		if (result)
			*result = NULL;
		return ENOENT;
	}
	return shim_marshal_passwd_caller(host_result, pwd, buffer, buffer_size, result);
}

int shim_getpwnam_r(const char *name,
                    struct darwin_passwd *pwd,
                    char *buffer,
                    size_t buffer_size,
                    struct darwin_passwd **result) __asm__("getpwnam_r");
int shim_getpwnam_r(const char *name,
                    struct darwin_passwd *pwd,
                    char *buffer,
                    size_t buffer_size,
                    struct darwin_passwd **result)
{
	if (!name || !shim_resolve_passwd_lookups())
		return ENOENT;

	struct passwd host;
	struct passwd *host_result = NULL;
	char host_buffer[2048];
	int status = shim_real_getpwnam_r(name, &host, host_buffer, sizeof(host_buffer), &host_result);
	if (status != 0)
		return status;
	if (!host_result) {
		if (result)
			*result = NULL;
		return ENOENT;
	}
	return shim_marshal_passwd_caller(host_result, pwd, buffer, buffer_size, result);
}

/* ===== Darwin socket ABI translation ===== */

#define DARWIN_AF_UNIX   1
#define DARWIN_AF_INET   2
#define DARWIN_AF_INET6 30

#define DARWIN_SOL_SOCKET 0xffff
#define DARWIN_SO_DEBUG      0x0001
#define DARWIN_SO_ACCEPTCONN 0x0002
#define DARWIN_SO_REUSEADDR  0x0004
#define DARWIN_SO_KEEPALIVE  0x0008
#define DARWIN_SO_DONTROUTE  0x0010
#define DARWIN_SO_BROADCAST  0x0020
#define DARWIN_SO_LINGER     0x0080
#define DARWIN_SO_OOBINLINE  0x0100
#define DARWIN_SO_REUSEPORT  0x0200
#define DARWIN_SO_SNDTIMEO   0x1005
#define DARWIN_SO_RCVTIMEO  0x1006
#define DARWIN_SO_ERROR     0x1007
#define DARWIN_SO_TYPE       0x1008
#define DARWIN_SO_SNDBUF    0x1001
#define DARWIN_SO_RCVBUF    0x1002
#define DARWIN_SO_SNDLOWAT  0x1003
#define DARWIN_SO_RCVLOWAT  0x1004
#define DARWIN_SO_NOSIGPIPE 0x1022
#define DARWIN_IP_DONTFRAG          28
#define DARWIN_IP_PKTINFO           26
#define LINUX_IP_PKTINFO            8
#define DARWIN_IPPROTO_IP           0
#define DARWIN_IPPROTO_TCP          6
#define DARWIN_TCP_KEEPALIVE        0x10
#define DARWIN_TCP_KEEPINTVL        0x101
#define DARWIN_TCP_KEEPCNT          0x102
#define DARWIN_IPPROTO_IPV6         41
#define DARWIN_IPV6_UNICAST_HOPS     16
#define DARWIN_IPV6_V6ONLY           27
#define DARWIN_IPV6_MULTICAST_IF     9
#define DARWIN_IPV6_MULTICAST_HOPS  10
#define DARWIN_IPV6_MULTICAST_LOOP  11
#define DARWIN_IPV6_JOIN_GROUP      12
#define DARWIN_IPV6_LEAVE_GROUP     13
#define DARWIN_IPV6_CHECKSUM         26
#define DARWIN_IPV6_RECVPKTINFO      61
#define DARWIN_IPV6_PKTINFO          46
#define LINUX_IPV6_RECVPKTINFO       49
#define LINUX_IPV6_PKTINFO           50
#define LINUX_IPV6_V6ONLY            26
#define LINUX_IP_MTU_DISCOVER       10
#define LINUX_IP_PMTUDISC_DONT      0
#define LINUX_IP_PMTUDISC_DO        2

static int shim_map_socket_domain(int darwin_domain)
{
	switch (darwin_domain) {
	case DARWIN_AF_UNIX:
		return AF_UNIX;
	case DARWIN_AF_INET:
		return AF_INET;
	case DARWIN_AF_INET6:
		return AF_INET6;
	default:
		return darwin_domain;
	}
}

#define DARWIN_MSG_OOB        0x0001
#define DARWIN_MSG_PEEK       0x0002
#define DARWIN_MSG_DONTROUTE  0x0004
#define DARWIN_MSG_EOR        0x0008
#define DARWIN_MSG_TRUNC      0x0010
#define DARWIN_MSG_CTRUNC     0x0020
#define DARWIN_MSG_WAITALL    0x0040
#define DARWIN_MSG_DONTWAIT   0x0080
#define DARWIN_MSG_NOSIGNAL   0x80000

static int shim_send_flags_to_linux(int darwin_flags)
{
	int linux_flags = 0;

	if (darwin_flags & DARWIN_MSG_OOB)
		linux_flags |= MSG_OOB;
	if (darwin_flags & DARWIN_MSG_DONTROUTE)
		linux_flags |= MSG_DONTROUTE;
	if (darwin_flags & DARWIN_MSG_DONTWAIT)
		linux_flags |= MSG_DONTWAIT;
	if (darwin_flags & DARWIN_MSG_NOSIGNAL)
		linux_flags |= MSG_NOSIGNAL;
	return linux_flags;
}

static int shim_recv_flags_to_linux(int darwin_flags)
{
	int linux_flags = 0;

	if (darwin_flags & DARWIN_MSG_OOB)
		linux_flags |= MSG_OOB;
	if (darwin_flags & DARWIN_MSG_PEEK)
		linux_flags |= MSG_PEEK;
	if (darwin_flags & DARWIN_MSG_DONTROUTE)
		linux_flags |= MSG_DONTROUTE;
	if (darwin_flags & DARWIN_MSG_TRUNC)
		linux_flags |= MSG_TRUNC;
	if (darwin_flags & DARWIN_MSG_CTRUNC)
		linux_flags |= MSG_CTRUNC;
	if (darwin_flags & DARWIN_MSG_WAITALL)
		linux_flags |= MSG_WAITALL;
	if (darwin_flags & DARWIN_MSG_DONTWAIT)
		linux_flags |= MSG_DONTWAIT;
	return linux_flags;
}

static int shim_socket_domain_is_known(int domain)
{
	return domain == DARWIN_AF_UNIX || domain == DARWIN_AF_INET ||
	       domain == DARWIN_AF_INET6;
}

static int shim_translate_sockaddr_in(const void* darwin_addr,
                                      socklen_t darwin_len,
                                      struct sockaddr_storage* linux_addr,
                                      socklen_t* linux_len)
{
	const unsigned char* raw = (const unsigned char*)darwin_addr;
	int family = raw[0] & 0xff;

	if (!darwin_addr) {
		*linux_len = 0;
		return 1;
	}
	if (darwin_len < 2 || darwin_len > (socklen_t)sizeof(*linux_addr)) {
		errno = EINVAL;
		return 0;
	}
	if (!shim_socket_domain_is_known(raw[1])) {
		errno = EAFNOSUPPORT;
		return 0;
	}

	memset(linux_addr, 0, sizeof(*linux_addr));
	switch (raw[1]) {
	case DARWIN_AF_UNIX: {
		struct sockaddr_un* unix_addr = (struct sockaddr_un*)linux_addr;
		size_t path_len = darwin_len - 2;
		if (path_len > sizeof(unix_addr->sun_path)) {
			errno = ENAMETOOLONG;
			return 0;
		}
		unix_addr->sun_family = AF_UNIX;
		memcpy(unix_addr->sun_path, raw + 2, path_len);
		*linux_len = (socklen_t)(offsetof(struct sockaddr_un, sun_path) + path_len);
		return 1;
	}
	case DARWIN_AF_INET: {
		struct sockaddr_in* inet_addr = (struct sockaddr_in*)linux_addr;
		if (darwin_len < (socklen_t)sizeof(struct sockaddr_in)) {
			errno = EINVAL;
			return 0;
		}
		inet_addr->sin_family = AF_INET;
		memcpy(&inet_addr->sin_port, raw + 2, 2);
		memcpy(&inet_addr->sin_addr, raw + 4, 4);
		*linux_len = (socklen_t)sizeof(struct sockaddr_in);
		return 1;
	}
	case DARWIN_AF_INET6: {
		struct sockaddr_in6* inet6_addr = (struct sockaddr_in6*)linux_addr;
		if (darwin_len < 28) {
			errno = EINVAL;
			return 0;
		}
		inet6_addr->sin6_family = AF_INET6;
		memcpy(&inet6_addr->sin6_port, raw + 2, 2);
		memcpy(&inet6_addr->sin6_flowinfo, raw + 4, 4);
		memcpy(&inet6_addr->sin6_addr, raw + 8, 16);
		memcpy(&inet6_addr->sin6_scope_id, raw + 24, 4);
		*linux_len = (socklen_t)sizeof(struct sockaddr_in6);
		return 1;
	}
	default:
		errno = EAFNOSUPPORT;
		return 0;
	}
}

static socklen_t shim_fill_darwin_sockaddr(const struct sockaddr* linux_addr,
                                           socklen_t linux_len,
                                           void* darwin_addr,
                                           socklen_t darwin_capacity)
{
	unsigned char* raw = (unsigned char*)darwin_addr;

	if (!darwin_addr || darwin_capacity < 2)
		return 0;

	switch (linux_addr->sa_family) {
	case AF_UNIX: {
		const struct sockaddr_un* unix_addr = (const struct sockaddr_un*)linux_addr;
		size_t path_len = linux_len - offsetof(struct sockaddr_un, sun_path);
		if (path_len > (size_t)(darwin_capacity - 2))
			path_len = darwin_capacity - 2;
		raw[0] = (unsigned char)(2 + path_len);
		raw[1] = DARWIN_AF_UNIX;
		memcpy(raw + 2, unix_addr->sun_path, path_len);
		return (socklen_t)(2 + path_len);
	}
	case AF_INET: {
		const struct sockaddr_in* inet_addr = (const struct sockaddr_in*)linux_addr;
		if (darwin_capacity < (socklen_t)sizeof(struct sockaddr_in))
			return 0;
		raw[0] = (unsigned char)sizeof(struct sockaddr_in);
		raw[1] = DARWIN_AF_INET;
		memcpy(raw + 2, &inet_addr->sin_port, 2);
		memcpy(raw + 4, &inet_addr->sin_addr, 4);
		memset(raw + 8, 0, 8);
		return (socklen_t)sizeof(struct sockaddr_in);
	}
	case AF_INET6: {
		const struct sockaddr_in6* inet6_addr = (const struct sockaddr_in6*)linux_addr;
		if (darwin_capacity < 28)
			return 0;
		raw[0] = 28;
		raw[1] = DARWIN_AF_INET6;
		memcpy(raw + 2, &inet6_addr->sin6_port, 2);
		memcpy(raw + 4, &inet6_addr->sin6_flowinfo, 4);
		memcpy(raw + 8, &inet6_addr->sin6_addr, 16);
		memcpy(raw + 24, &inet6_addr->sin6_scope_id, 4);
		return 28;
	}
	default:
		return 0;
	}
}

static int shim_translate_socket_option(int darwin_level, int darwin_option,
                                        int* linux_level, int* linux_option)
{
	if (darwin_level == DARWIN_IPPROTO_TCP) {
		*linux_level = IPPROTO_TCP;
		switch (darwin_option) {
		case DARWIN_TCP_KEEPALIVE:
			*linux_option = TCP_KEEPIDLE;
			return 1;
		case DARWIN_TCP_KEEPINTVL:
			*linux_option = TCP_KEEPINTVL;
			return 1;
		case DARWIN_TCP_KEEPCNT:
			*linux_option = TCP_KEEPCNT;
			return 1;
		default:
			*linux_option = darwin_option;
			return 1;
		}
	}

	if (darwin_level == DARWIN_IPPROTO_IPV6) {
		*linux_level = IPPROTO_IPV6;
		switch (darwin_option) {
		case DARWIN_IPV6_UNICAST_HOPS:
			*linux_option = IPV6_UNICAST_HOPS;
			return 1;
		case DARWIN_IPV6_V6ONLY:
			*linux_option = LINUX_IPV6_V6ONLY;
			return 1;
		case DARWIN_IPV6_MULTICAST_IF:
			*linux_option = IPV6_MULTICAST_IF;
			return 1;
		case DARWIN_IPV6_MULTICAST_HOPS:
			*linux_option = IPV6_MULTICAST_HOPS;
			return 1;
		case DARWIN_IPV6_MULTICAST_LOOP:
			*linux_option = IPV6_MULTICAST_LOOP;
			return 1;
		case DARWIN_IPV6_JOIN_GROUP:
			*linux_option = IPV6_JOIN_GROUP;
			return 1;
		case DARWIN_IPV6_LEAVE_GROUP:
			*linux_option = IPV6_LEAVE_GROUP;
			return 1;
		case DARWIN_IPV6_CHECKSUM:
			*linux_option = IPV6_CHECKSUM;
			return 1;
		case DARWIN_IPV6_RECVPKTINFO:
			*linux_option = LINUX_IPV6_RECVPKTINFO;
			return 1;
		case DARWIN_IPV6_PKTINFO:
			*linux_option = LINUX_IPV6_PKTINFO;
			return 1;
		default:
			*linux_option = darwin_option;
			return 1;
		}
	}

	if (darwin_level == DARWIN_IPPROTO_IP) {
		*linux_level = IPPROTO_IP;
		switch (darwin_option) {
		case DARWIN_IP_PKTINFO:
			*linux_option = LINUX_IP_PKTINFO;
			return 1;
		default:
			*linux_option = darwin_option;
			return 1;
		}
	}

	if (darwin_level != DARWIN_SOL_SOCKET) {
		*linux_level = darwin_level;
		*linux_option = darwin_option;
		return 1;
	}

	*linux_level = SOL_SOCKET;
	switch (darwin_option) {
	case DARWIN_SO_DEBUG:      *linux_option = SO_DEBUG;      return 1;
	case DARWIN_SO_ACCEPTCONN: *linux_option = SO_ACCEPTCONN; return 1;
	case DARWIN_SO_REUSEADDR:  *linux_option = SO_REUSEADDR;  return 1;
	case DARWIN_SO_KEEPALIVE:  *linux_option = SO_KEEPALIVE;  return 1;
	case DARWIN_SO_DONTROUTE:  *linux_option = SO_DONTROUTE;  return 1;
	case DARWIN_SO_BROADCAST:  *linux_option = SO_BROADCAST;  return 1;
	case DARWIN_SO_LINGER:     *linux_option = SO_LINGER;     return 1;
	case DARWIN_SO_OOBINLINE:  *linux_option = SO_OOBINLINE;  return 1;
#ifdef SO_REUSEPORT
	case DARWIN_SO_REUSEPORT:  *linux_option = SO_REUSEPORT;  return 1;
#endif
#ifdef SO_SNDTIMEO
	case DARWIN_SO_SNDTIMEO:   *linux_option = SO_SNDTIMEO;   return 1;
#endif
#ifdef SO_RCVTIMEO
	case DARWIN_SO_RCVTIMEO:   *linux_option = SO_RCVTIMEO;   return 1;
#endif
	case DARWIN_SO_ERROR:      *linux_option = SO_ERROR;      return 1;
	case DARWIN_SO_TYPE:       *linux_option = SO_TYPE;       return 1;
	case DARWIN_SO_SNDBUF:     *linux_option = SO_SNDBUF;     return 1;
	case DARWIN_SO_RCVBUF:     *linux_option = SO_RCVBUF;     return 1;
#ifdef SO_SNDLOWAT
	case DARWIN_SO_SNDLOWAT:   *linux_option = SO_SNDLOWAT;   return 1;
#endif
#ifdef SO_RCVLOWAT
	case DARWIN_SO_RCVLOWAT:   *linux_option = SO_RCVLOWAT;   return 1;
#endif
	default:
		*linux_option = darwin_option;
		return 1;
	}
}

static int (*shim_real_socket)(int, int, int);
static int (*shim_real_bind)(int, const struct sockaddr*, socklen_t);
static int (*shim_real_connect)(int, const struct sockaddr*, socklen_t);
static int (*shim_real_listen)(int, int);
static int (*shim_real_accept)(int, struct sockaddr*, socklen_t*);
static int (*shim_real_getsockname)(int, struct sockaddr*, socklen_t*);
static int (*shim_real_getpeername)(int, struct sockaddr*, socklen_t*);
static ssize_t (*shim_real_sendto)(int, const void*, size_t, int,
                                  const struct sockaddr*, socklen_t);
static ssize_t (*shim_real_recvfrom)(int, void*, size_t, int,
                                     struct sockaddr*, socklen_t*);
static int (*shim_real_setsockopt)(int, int, int, const void*, socklen_t);
static int (*shim_real_getsockopt)(int, int, int, void*, socklen_t*);
static ssize_t (*shim_real_sendmsg)(int, const struct msghdr*, int);
static ssize_t (*shim_real_recvmsg)(int, struct msghdr*, int);

struct darwin_msghdr {
	void* msg_name;
	socklen_t msg_namelen;
	struct iovec* msg_iov;
	int msg_iovlen;
	void* msg_control;
	socklen_t msg_controllen;
	int msg_flags;
};

static void shim_resolve_real_socket_calls(void)
{
	if (!shim_real_socket)
		shim_real_socket = dlsym(RTLD_NEXT, "socket");
	if (!shim_real_bind)
		shim_real_bind = dlsym(RTLD_NEXT, "bind");
	if (!shim_real_connect)
		shim_real_connect = dlsym(RTLD_NEXT, "connect");
	if (!shim_real_listen)
		shim_real_listen = dlsym(RTLD_NEXT, "listen");
	if (!shim_real_accept)
		shim_real_accept = dlsym(RTLD_NEXT, "accept");
	if (!shim_real_getsockname)
		shim_real_getsockname = dlsym(RTLD_NEXT, "getsockname");
	if (!shim_real_getpeername)
		shim_real_getpeername = dlsym(RTLD_NEXT, "getpeername");
	if (!shim_real_sendto)
		shim_real_sendto = dlsym(RTLD_NEXT, "sendto");
	if (!shim_real_recvfrom)
		shim_real_recvfrom = dlsym(RTLD_NEXT, "recvfrom");
	if (!shim_real_setsockopt)
		shim_real_setsockopt = dlsym(RTLD_NEXT, "setsockopt");
	if (!shim_real_getsockopt)
		shim_real_getsockopt = dlsym(RTLD_NEXT, "getsockopt");
	if (!shim_real_sendmsg)
		shim_real_sendmsg = dlsym(RTLD_NEXT, "sendmsg");
	if (!shim_real_recvmsg)
		shim_real_recvmsg = dlsym(RTLD_NEXT, "recvmsg");
}

struct darwin_cmsghdr {
	uint32_t cmsg_len;
	int32_t cmsg_level;
	int32_t cmsg_type;
};

#define DARWIN_CMSG_LEN(data_len) \
	((uint32_t)sizeof(struct darwin_cmsghdr) + (uint32_t)(data_len))
#define DARWIN_CMSG_ALIGN(len) \
	(((uint32_t)(len) + 3u) & ~(uint32_t)3u)
#define DARWIN_CMSG_SPACE(data_len) \
	(DARWIN_CMSG_ALIGN(DARWIN_CMSG_LEN(data_len)))

static size_t shim_repack_linux_cmsgs_to_darwin(const void* linux_control,
                                                 size_t linux_length,
                                                 void* darwin_control,
                                                 size_t darwin_capacity)
{
	const struct cmsghdr* cmsg;
	size_t darwin_offset = 0;

	if (!linux_control || !darwin_control || linux_length < sizeof(struct cmsghdr))
		return 0;

	for (cmsg = (const struct cmsghdr*)linux_control;
	     (size_t)((const unsigned char*)cmsg -
	              (const unsigned char*)linux_control) +
	             sizeof(struct cmsghdr) <= linux_length;
	     cmsg = (const struct cmsghdr*)
	             ((const unsigned char*)cmsg + CMSG_ALIGN(cmsg->cmsg_len))) {
		size_t consumed = (size_t)((const unsigned char*)cmsg -
		                           (const unsigned char*)linux_control);
		size_t remaining = linux_length - consumed;
		struct darwin_cmsghdr* darwin_cmsg;
		size_t data_length;
		uint32_t darwin_len;

		if (cmsg->cmsg_len < sizeof(struct cmsghdr) ||
		    cmsg->cmsg_len > remaining)
			break;

		data_length = (size_t)cmsg->cmsg_len - sizeof(struct cmsghdr);
		darwin_len = DARWIN_CMSG_LEN(data_length);
		if (darwin_offset + DARWIN_CMSG_ALIGN(darwin_len) > darwin_capacity)
			break;

		darwin_cmsg = (struct darwin_cmsghdr*)
			((unsigned char*)darwin_control + darwin_offset);
		darwin_cmsg->cmsg_len = darwin_len;
		darwin_cmsg->cmsg_level = cmsg->cmsg_level;
		darwin_cmsg->cmsg_type = cmsg->cmsg_type;

		if (darwin_cmsg->cmsg_level == IPPROTO_IP &&
		    darwin_cmsg->cmsg_type == LINUX_IP_PKTINFO)
			darwin_cmsg->cmsg_type = DARWIN_IP_PKTINFO;
		else if (darwin_cmsg->cmsg_level == DARWIN_IPPROTO_IPV6 &&
		         darwin_cmsg->cmsg_type == LINUX_IPV6_PKTINFO)
			darwin_cmsg->cmsg_type = DARWIN_IPV6_PKTINFO;

		memcpy(((unsigned char*)darwin_cmsg) + sizeof(struct darwin_cmsghdr),
		       (const unsigned char*)cmsg + sizeof(struct cmsghdr),
		       data_length);

		darwin_offset += DARWIN_CMSG_ALIGN(darwin_len);
	}

	return darwin_offset;
}

static size_t shim_repack_darwin_cmsgs_to_linux(const void* darwin_control,
                                                size_t darwin_length,
                                                void* linux_control,
                                                size_t linux_capacity)
{
	const struct darwin_cmsghdr* cmsg;
	size_t linux_offset = 0;

	if (!darwin_control || !linux_control || darwin_length < sizeof(struct darwin_cmsghdr))
		return 0;

	for (cmsg = (const struct darwin_cmsghdr*)darwin_control;
	     (size_t)((const unsigned char*)cmsg -
	              (const unsigned char*)darwin_control) +
	             sizeof(struct darwin_cmsghdr) <= darwin_length;
	     cmsg = (const struct darwin_cmsghdr*)
	             ((const unsigned char*)cmsg +
	              DARWIN_CMSG_ALIGN(cmsg->cmsg_len))) {
		size_t consumed = (size_t)((const unsigned char*)cmsg -
		                           (const unsigned char*)darwin_control);
		size_t remaining = darwin_length - consumed;
		struct cmsghdr* linux_cmsg;
		size_t data_length;
		size_t linux_stride;

		if (cmsg->cmsg_len < sizeof(struct darwin_cmsghdr) ||
		    (size_t)cmsg->cmsg_len > remaining)
			break;

		data_length = (size_t)cmsg->cmsg_len - sizeof(struct darwin_cmsghdr);
		linux_stride = CMSG_SPACE(data_length);
		if (linux_offset + linux_stride > linux_capacity)
			break;

		linux_cmsg = (struct cmsghdr*)
			((unsigned char*)linux_control + linux_offset);
		linux_cmsg->cmsg_len = CMSG_LEN(data_length);
		linux_cmsg->cmsg_level = cmsg->cmsg_level;
		linux_cmsg->cmsg_type = cmsg->cmsg_type;

		if (linux_cmsg->cmsg_level == DARWIN_IPPROTO_IP &&
		    linux_cmsg->cmsg_type == DARWIN_IP_PKTINFO)
			linux_cmsg->cmsg_type = LINUX_IP_PKTINFO;
		else if (linux_cmsg->cmsg_level == DARWIN_IPPROTO_IPV6 &&
		         linux_cmsg->cmsg_type == DARWIN_IPV6_PKTINFO)
			linux_cmsg->cmsg_type = LINUX_IPV6_PKTINFO;

		memcpy((unsigned char*)linux_cmsg + sizeof(struct cmsghdr),
		       (const unsigned char*)cmsg + sizeof(struct darwin_cmsghdr),
		       data_length);

		linux_offset += linux_stride;
	}

	return linux_offset;
}

int shim_socket(int domain, int type, int protocol) __asm__("socket");
int shim_socket(int domain, int type, int protocol)
{
	int result;
	int saved_errno;

	shim_resolve_real_socket_calls();
	if (!shim_real_socket)
		return -1;
	result = shim_real_socket(shim_map_socket_domain(domain), type, protocol);
	saved_errno = errno;
	if (result < 0)
		saved_errno = shim_errno_from_linux(saved_errno);
	shim_fd_trace_log("socket caller=%p domain=%d type=%d protocol=%d -> %d errno=%d\n",
	                  SHIM_CALLER_RETURN_ADDRESS(), domain, type, protocol,
	                  result, result < 0 ? saved_errno : 0);
	errno = saved_errno;
	return result;
}

int shim_bind(int sockfd, const void* darwin_addr, socklen_t darwin_len) __asm__("bind");
int shim_bind(int sockfd, const void* darwin_addr, socklen_t darwin_len)
{
	struct sockaddr_storage linux_addr;
	socklen_t linux_len = 0;
	int result;
	int saved_errno;

	shim_resolve_real_socket_calls();
	if (!shim_real_bind)
		return -1;
	if (!shim_translate_sockaddr_in(darwin_addr, darwin_len,
	                                &linux_addr, &linux_len)) {
		shim_fd_trace_log("bind caller=%p fd=%d translate_failed len=%d\n",
		                  SHIM_CALLER_RETURN_ADDRESS(), sockfd, darwin_len);
		return -1;
	}
	errno = 0;
	result = shim_real_bind(sockfd, (const struct sockaddr*)&linux_addr, linux_len);
	saved_errno = errno;
	if (result < 0)
		saved_errno = shim_errno_from_linux(saved_errno);
	shim_fd_trace_log("bind caller=%p fd=%d family=%d port=%d -> %d errno=%d\n",
	                  SHIM_CALLER_RETURN_ADDRESS(), sockfd,
	                  linux_addr.ss_family,
	                  ntohs(((const struct sockaddr_in*)&linux_addr)->sin_port),
	                  result, result < 0 ? saved_errno : 0);
	errno = saved_errno;
	return result;
}

int shim_connect(int sockfd, const void* darwin_addr, socklen_t darwin_len) __asm__("connect");
int shim_connect(int sockfd, const void* darwin_addr, socklen_t darwin_len)
{
	struct sockaddr_storage linux_addr;
	socklen_t linux_len = 0;
	int result;
	int saved_errno;

	shim_resolve_real_socket_calls();
	if (!shim_real_connect)
		return -1;
	if (!shim_translate_sockaddr_in(darwin_addr, darwin_len,
	                                &linux_addr, &linux_len)) {
		errno = EINVAL;
		return -1;
	}
	errno = 0;
	result = shim_real_connect(sockfd, (const struct sockaddr*)&linux_addr, linux_len);
	saved_errno = errno;
	if (result < 0)
		saved_errno = shim_errno_from_linux(saved_errno);
	shim_fd_trace_log("connect caller=%p fd=%d family=%d port=%d -> %d errno=%d\n",
	                  SHIM_CALLER_RETURN_ADDRESS(), sockfd,
	                  linux_addr.ss_family,
	                  ntohs(((const struct sockaddr_in*)&linux_addr)->sin_port),
	                  result, result < 0 ? saved_errno : 0);
	errno = saved_errno;
	return result;
}

int shim_listen(int sockfd, int backlog) __asm__("listen");
int shim_listen(int sockfd, int backlog)
{
	int result;
	int saved_errno;

	shim_resolve_real_socket_calls();
	if (!shim_real_listen)
		return -1;
	errno = 0;
	result = shim_real_listen(sockfd, backlog);
	saved_errno = errno;
	if (result < 0)
		saved_errno = shim_errno_from_linux(saved_errno);
	shim_fd_trace_log("listen caller=%p fd=%d backlog=%d -> %d errno=%d\n",
	                  SHIM_CALLER_RETURN_ADDRESS(), sockfd, backlog,
	                  result, result < 0 ? saved_errno : 0);
	errno = saved_errno;
	return result;
}

int shim_accept(int sockfd, void* darwin_addr, socklen_t* darwin_len_ptr) __asm__("accept");
int shim_accept(int sockfd, void* darwin_addr, socklen_t* darwin_len_ptr)
{
	struct sockaddr_storage linux_addr;
	socklen_t linux_len = sizeof(linux_addr);
	int result;
	int saved_errno;

	shim_resolve_real_socket_calls();
	if (!shim_real_accept)
		return -1;

	errno = 0;
	result = shim_real_accept(sockfd, (struct sockaddr*)&linux_addr, &linux_len);
	saved_errno = errno;
	if (result < 0) {
		saved_errno = shim_errno_from_linux(saved_errno);
		shim_fd_trace_log("accept caller=%p fd=%d -> %d errno=%d\n",
		                  SHIM_CALLER_RETURN_ADDRESS(), sockfd, result,
		                  saved_errno);
		errno = saved_errno;
		return result;
	}

	if (darwin_addr && darwin_len_ptr) {
		socklen_t written = shim_fill_darwin_sockaddr(
			(const struct sockaddr*)&linux_addr, linux_len,
			darwin_addr, *darwin_len_ptr);
		if (written)
			*darwin_len_ptr = written;
	}
	return result;
}

int shim_getsockname(int sockfd, void* darwin_addr, socklen_t* darwin_len_ptr) __asm__("getsockname");
int shim_getsockname(int sockfd, void* darwin_addr, socklen_t* darwin_len_ptr)
{
	struct sockaddr_storage linux_addr;
	socklen_t linux_len = sizeof(linux_addr);
	int result;
	int saved_errno;

	shim_resolve_real_socket_calls();
	if (!shim_real_getsockname)
		return -1;

	errno = 0;
	result = shim_real_getsockname(sockfd, (struct sockaddr*)&linux_addr, &linux_len);
	if (result < 0) {
		saved_errno = shim_errno_from_linux(errno);
		errno = saved_errno;
		return result;
	}

	if (darwin_addr && darwin_len_ptr) {
		socklen_t written = shim_fill_darwin_sockaddr(
			(const struct sockaddr*)&linux_addr, linux_len,
			darwin_addr, *darwin_len_ptr);
		if (written)
			*darwin_len_ptr = written;
	}
	return result;
}

int shim_getpeername(int sockfd, void* darwin_addr, socklen_t* darwin_len_ptr) __asm__("getpeername");
int shim_getpeername(int sockfd, void* darwin_addr, socklen_t* darwin_len_ptr)
{
	struct sockaddr_storage linux_addr;
	socklen_t linux_len = sizeof(linux_addr);
	int result;
	int saved_errno;

	shim_resolve_real_socket_calls();
	if (!shim_real_getpeername)
		return -1;

	errno = 0;
	result = shim_real_getpeername(sockfd, (struct sockaddr*)&linux_addr, &linux_len);
	if (result < 0) {
		saved_errno = shim_errno_from_linux(errno);
		errno = saved_errno;
		return result;
	}

	if (darwin_addr && darwin_len_ptr) {
		socklen_t written = shim_fill_darwin_sockaddr(
			(const struct sockaddr*)&linux_addr, linux_len,
			darwin_addr, *darwin_len_ptr);
		if (written)
			*darwin_len_ptr = written;
	}
	return result;
}

ssize_t shim_sendto(int sockfd, const void* buffer, size_t length, int flags,
                    const void* darwin_addr, socklen_t darwin_len) __asm__("sendto");
ssize_t shim_sendto(int sockfd, const void* buffer, size_t length, int flags,
                    const void* darwin_addr, socklen_t darwin_len)
{
	struct sockaddr_storage linux_addr;
	socklen_t linux_len = 0;
	ssize_t result;
	int saved_errno;

	shim_resolve_real_socket_calls();
	if (!shim_real_sendto)
		return -1;
	if (!shim_translate_sockaddr_in(darwin_addr, darwin_len,
	                                &linux_addr, &linux_len)) {
		errno = EINVAL;
		return -1;
	}
	errno = 0;
	result = shim_real_sendto(sockfd, buffer, length,
	                         shim_send_flags_to_linux(flags),
	                         (const struct sockaddr*)&linux_addr, linux_len);
	saved_errno = errno;
	if (result < 0)
		saved_errno = shim_errno_from_linux(saved_errno);
	shim_fd_trace_log("sendto caller=%p fd=%d len=%zu -> %zd errno=%d\n",
	                  SHIM_CALLER_RETURN_ADDRESS(), sockfd, length,
	                  result, result < 0 ? saved_errno : 0);
	errno = saved_errno;
	return result;
}

ssize_t shim_sendmsg(int sockfd, const void* darwin_msg_hdr, int flags) __asm__("sendmsg");
ssize_t shim_sendmsg(int sockfd, const void* darwin_msg_hdr, int flags)
{
	const struct darwin_msghdr* darwin_msg =
		(const struct darwin_msghdr*)darwin_msg_hdr;
	struct sockaddr_storage linux_addr;
	socklen_t linux_len = 0;
	struct msghdr linux_msg;
	ssize_t result;
	int saved_errno;

	shim_resolve_real_socket_calls();
	if (!shim_real_sendmsg)
		return -1;
	memset(&linux_msg, 0, sizeof(linux_msg));
	if (darwin_msg->msg_name) {
		if (!shim_translate_sockaddr_in(darwin_msg->msg_name,
		                                darwin_msg->msg_namelen,
		                                &linux_addr, &linux_len)) {
			errno = EFAULT;
			return -1;
		}
		linux_msg.msg_name = &linux_addr;
		linux_msg.msg_namelen = linux_len;
	}
	linux_msg.msg_iov = darwin_msg->msg_iov;
	linux_msg.msg_iovlen = (size_t)darwin_msg->msg_iovlen;
	linux_msg.msg_flags = darwin_msg->msg_flags;
	if (darwin_msg->msg_control && darwin_msg->msg_controllen) {
		linux_msg.msg_control = malloc(darwin_msg->msg_controllen);
		if (!linux_msg.msg_control) {
			errno = ENOMEM;
			return -1;
		}
		linux_msg.msg_controllen = shim_repack_darwin_cmsgs_to_linux(
			darwin_msg->msg_control,
			(size_t)darwin_msg->msg_controllen,
			linux_msg.msg_control,
			(size_t)darwin_msg->msg_controllen);
	}
	errno = 0;
	result = shim_real_sendmsg(sockfd, &linux_msg,
	                          shim_send_flags_to_linux(flags));
	saved_errno = errno;
	if (linux_msg.msg_control)
		free(linux_msg.msg_control);
	if (result < 0)
		saved_errno = shim_errno_from_linux(saved_errno);
	shim_fd_trace_log("sendmsg caller=%p fd=%d iovlen=%d namelen=%d -> %zd errno=%d\n",
	                  SHIM_CALLER_RETURN_ADDRESS(), sockfd,
	                  darwin_msg->msg_iovlen, darwin_msg->msg_namelen,
	                  result, result < 0 ? saved_errno : 0);
	errno = saved_errno;
	return result;
}

ssize_t shim_recvmsg(int sockfd, void* darwin_msg_hdr, int flags) __asm__("recvmsg");
ssize_t shim_recvmsg(int sockfd, void* darwin_msg_hdr, int flags)
{
	struct darwin_msghdr* darwin_msg = (struct darwin_msghdr*)darwin_msg_hdr;
	struct sockaddr_storage linux_addr;
	socklen_t linux_len = sizeof(linux_addr);
	struct msghdr linux_msg;
	socklen_t darwin_capacity = darwin_msg->msg_namelen;
	char linux_control[512];
	ssize_t result;
	int saved_errno;

	shim_resolve_real_socket_calls();
	if (!shim_real_recvmsg)
		return -1;
	memset(&linux_addr, 0, sizeof(linux_addr));
	memset(&linux_msg, 0, sizeof(linux_msg));
	linux_msg.msg_name = darwin_msg->msg_name ? &linux_addr : NULL;
	linux_msg.msg_namelen = sizeof(linux_addr);
	linux_msg.msg_iov = darwin_msg->msg_iov;
	linux_msg.msg_iovlen = (size_t)darwin_msg->msg_iovlen;
	if (darwin_msg->msg_control && darwin_msg->msg_controllen) {
		linux_msg.msg_control = linux_control;
		linux_msg.msg_controllen = sizeof(linux_control);
	} else {
		linux_msg.msg_control = NULL;
		linux_msg.msg_controllen = 0;
	}
	result = shim_real_recvmsg(sockfd, &linux_msg,
	                           shim_recv_flags_to_linux(flags));
	if (result < 0) {
		saved_errno = shim_errno_from_linux(errno);
		errno = saved_errno;
		return result;
	}
	if (result >= 0) {
		darwin_msg->msg_flags = linux_msg.msg_flags;
		if (darwin_msg->msg_name && darwin_capacity >= 2) {
			socklen_t written = shim_fill_darwin_sockaddr(
				(const struct sockaddr*)&linux_addr, linux_msg.msg_namelen,
				darwin_msg->msg_name, darwin_capacity);
			darwin_msg->msg_namelen = written ? written : darwin_capacity;
		} else if (darwin_msg->msg_name) {
			darwin_msg->msg_namelen = 0;
		}
		if (darwin_msg->msg_control && darwin_msg->msg_controllen) {
			size_t repacked = shim_repack_linux_cmsgs_to_darwin(
				linux_control, (size_t)linux_msg.msg_controllen,
				darwin_msg->msg_control,
				(size_t)darwin_msg->msg_controllen);
			darwin_msg->msg_controllen = (socklen_t)repacked;
		} else {
			darwin_msg->msg_controllen = 0;
		}
	}
	shim_fd_trace_log("recvmsg caller=%p fd=%d len_iov=%d -> %zd errno=%d\n",
	                  SHIM_CALLER_RETURN_ADDRESS(), sockfd,
	                  darwin_msg->msg_iovlen,
	                  result, result < 0 ? errno : 0);
	return result;
}

ssize_t shim_recvfrom(int sockfd, void* buffer, size_t length, int flags,
                      void* darwin_addr, socklen_t* darwin_len_ptr) __asm__("recvfrom");
ssize_t shim_recvfrom(int sockfd, void* buffer, size_t length, int flags,
                      void* darwin_addr, socklen_t* darwin_len_ptr)
{
	struct sockaddr_storage linux_addr;
	socklen_t linux_len = sizeof(linux_addr);
	ssize_t result;
	int saved_errno;

	shim_resolve_real_socket_calls();
	if (!shim_real_recvfrom)
		return -1;

	errno = 0;
	result = shim_real_recvfrom(sockfd, buffer, length,
	                            shim_recv_flags_to_linux(flags),
	                            (struct sockaddr*)&linux_addr, &linux_len);
	if (result < 0) {
		saved_errno = shim_errno_from_linux(errno);
		errno = saved_errno;
		return result;
	}

	if (darwin_addr && darwin_len_ptr) {
		socklen_t written = shim_fill_darwin_sockaddr(
			(const struct sockaddr*)&linux_addr, linux_len,
			darwin_addr, *darwin_len_ptr);
		if (written)
			*darwin_len_ptr = written;
	}
	return result;
}

ssize_t shim_recv(int sockfd, void* buffer, size_t length, int flags) __asm__("recv");
ssize_t shim_recv(int sockfd, void* buffer, size_t length, int flags)
{
	ssize_t result;
	int saved_errno;

	errno = 0;
	result = (ssize_t)syscall(SYS_recvfrom, sockfd, buffer, length,
	                         shim_recv_flags_to_linux(flags),
	                         NULL, NULL);
	saved_errno = errno;
	if (result < 0)
		saved_errno = shim_errno_from_linux(saved_errno);
	shim_fd_trace_log("recv caller=%p fd=%d len=%zu -> %zd errno=%d\n",
	                  SHIM_CALLER_RETURN_ADDRESS(), sockfd, length,
	                  result, result < 0 ? saved_errno : 0);
	errno = saved_errno;
	return result;
}

ssize_t shim_send(int sockfd, const void* buffer, size_t length, int flags) __asm__("send");
ssize_t shim_send(int sockfd, const void* buffer, size_t length, int flags)
{
	ssize_t result;
	int saved_errno;

	errno = 0;
	result = (ssize_t)syscall(SYS_sendto, sockfd, buffer, length,
	                         shim_send_flags_to_linux(flags),
	                         NULL, 0);
	saved_errno = errno;
	if (result < 0)
		saved_errno = shim_errno_from_linux(saved_errno);
	shim_fd_trace_log("send caller=%p fd=%d len=%zu -> %zd errno=%d\n",
	                  SHIM_CALLER_RETURN_ADDRESS(), sockfd, length,
	                  result, result < 0 ? saved_errno : 0);
	errno = saved_errno;
	return result;
}

int inet_pton(int af, const char* src, void* dst)
{
	static int (*real_inet_pton)(int, const char*, void*);

	if (!real_inet_pton)
		real_inet_pton = dlsym(RTLD_NEXT, "inet_pton");
	if (!real_inet_pton)
		return -1;
	if (af == DARWIN_AF_INET)
		af = AF_INET;
	else if (af == DARWIN_AF_INET6)
		af = AF_INET6;
	return real_inet_pton(af, src, dst);
}

const char* inet_ntop(int af, const void* src, char* dst, socklen_t size)
{
	static const char* (*real_inet_ntop)(int, const void*, char*, socklen_t);

	if (!real_inet_ntop)
		real_inet_ntop = dlsym(RTLD_NEXT, "inet_ntop");
	if (!real_inet_ntop)
		return NULL;
	if (af == DARWIN_AF_INET)
		af = AF_INET;
	else if (af == DARWIN_AF_INET6)
		af = AF_INET6;
	return real_inet_ntop(af, src, dst, size);
}

int shim_setsockopt(int sockfd, int level, int option,
                   const void* value, socklen_t value_len) __asm__("setsockopt");
int shim_setsockopt(int sockfd, int level, int option,
                   const void* value, socklen_t value_len)
{
	int linux_level;
	int linux_option;
	int result;
	int saved_errno;

	shim_resolve_real_socket_calls();
	if (!shim_real_setsockopt)
		return -1;
	if (level == DARWIN_SOL_SOCKET && option == DARWIN_SO_NOSIGPIPE)
		return 0;
	if (level == DARWIN_IPPROTO_IP && option == DARWIN_IP_DONTFRAG) {
		int mtu_discovery = LINUX_IP_PMTUDISC_DONT;
		if (value && value_len >= (socklen_t)sizeof(int) && *(const int*)value > 0)
			mtu_discovery = LINUX_IP_PMTUDISC_DO;
		result = shim_real_setsockopt(sockfd, IPPROTO_IP, LINUX_IP_MTU_DISCOVER,
		                             &mtu_discovery, sizeof(mtu_discovery));
		shim_fd_trace_log("setsockopt caller=%p fd=%d level=0 option=IP_DONTFRAG translated=IP_MTU_DISCOVER -> %d errno=%d\n",
		                  SHIM_CALLER_RETURN_ADDRESS(), sockfd, result, result < 0 ? errno : 0);
		return result;
	}
	if (!shim_translate_socket_option(level, option, &linux_level, &linux_option))
		result = -1;
	else {
		errno = 0;
		result = shim_real_setsockopt(sockfd, linux_level, linux_option, value, value_len);
	}
	saved_errno = errno;
	if (result < 0)
		saved_errno = shim_errno_from_linux(saved_errno);
	shim_fd_trace_log("setsockopt caller=%p fd=%d level=%d option=%d linux_level=%d linux_option=%d -> %d errno=%d\n",
	                  SHIM_CALLER_RETURN_ADDRESS(), sockfd, level, option,
	                  linux_level, linux_option, result, result < 0 ? saved_errno : 0);
	errno = saved_errno;
	return result;
}

int shim_getsockopt(int sockfd, int level, int option,
                    void* value, socklen_t* value_len_ptr) __asm__("getsockopt");
int shim_getsockopt(int sockfd, int level, int option,
                    void* value, socklen_t* value_len_ptr)
{
	int linux_level;
	int linux_option;
	int result;
	int saved_errno;

	shim_resolve_real_socket_calls();
	if (!shim_real_getsockopt)
		return -1;
	if (level == DARWIN_IPPROTO_IP && option == DARWIN_IP_DONTFRAG) {
		int mtu_discovery = 0;
		socklen_t mtu_len = sizeof(mtu_discovery);
		errno = 0;
		result = shim_real_getsockopt(sockfd, IPPROTO_IP,
		                              LINUX_IP_MTU_DISCOVER,
		                              &mtu_discovery, &mtu_len);
		saved_errno = errno;
		if (result == 0 && value && value_len_ptr &&
		    *value_len_ptr >= (socklen_t)sizeof(int)) {
			*(int*)value = mtu_discovery == LINUX_IP_PMTUDISC_DO ? 1 : 0;
			*value_len_ptr = sizeof(int);
		}
		return result;
	}
	if (!shim_translate_socket_option(level, option, &linux_level, &linux_option))
		return -1;
	errno = 0;
	result = shim_real_getsockopt(sockfd, linux_level, linux_option,
	                              value, value_len_ptr);
	saved_errno = errno;
	if (result < 0) {
		saved_errno = shim_errno_from_linux(saved_errno);
		errno = saved_errno;
		return result;
	}
	if (level == DARWIN_SOL_SOCKET && option == DARWIN_SO_ERROR &&
	    value && value_len_ptr && *value_len_ptr >= (socklen_t)sizeof(int))
		*(int*)value = shim_errno_from_linux(*(int*)value);
	return result;
}

/* ===== _NSGetExecutablePath ===== */

int _NSGetExecutablePath(char *buf, uint32_t *bufsize)
{
	if (!buf || !bufsize) return -1;

	const char** guest_executable_path =
		dlsym(RTLD_DEFAULT, "__machgate_guest_executable_path");
	const char* path = (guest_executable_path && *guest_executable_path)
		? *guest_executable_path
		: NULL;
	char proc_path[4096];

	if (!path) {
		ssize_t proc_len = readlink("/proc/self/exe", proc_path,
		                            sizeof(proc_path) - 1);
		if (proc_len < 0) return -1;
		proc_path[proc_len] = '\0';
		path = proc_path;
	}

	size_t len = strlen(path);

	if ((uint32_t)len >= *bufsize) {
		*bufsize = (uint32_t)len + 1;
		return -1; /* buffer too small */
	}

	memcpy(buf, path, len + 1);
	*bufsize = (uint32_t)len;
	return 0;
}

int proc_pidpath(int pid, void* buffer, uint32_t buffersize)
{
	if (!buffer || buffersize == 0) {
		errno = EINVAL;
		return 0;
	}

	const char** guest_executable_path =
		dlsym(RTLD_DEFAULT, "__machgate_guest_executable_path");
	const char* path = (pid == getpid() && guest_executable_path &&
	                    *guest_executable_path)
		? *guest_executable_path
		: NULL;
	char proc_path[4096];
	char proc_link[64];

	if (!path) {
		snprintf(proc_link, sizeof(proc_link), "/proc/%d/exe", pid);
		ssize_t proc_len = readlink(proc_link, proc_path,
		                            sizeof(proc_path) - 1);
		if (proc_len < 0)
			return 0;
		proc_path[proc_len] = '\0';
		path = proc_path;
	}

	size_t len = strlen(path);
	if (len >= buffersize) {
		errno = ENOBUFS;
		return 0;
	}

	memcpy(buffer, path, len + 1);
	return (int)len;
}

ssize_t __getdirentries64(int fd, char* buffer, size_t buffer_size,
                          int64_t* basep)
{
	(void)fd;
	(void)buffer;
	(void)buffer_size;
	if (basep)
		*basep = 0;
	return 0;
}

int __pthread_fchdir(int fd)
{
	return fchdir(fd);
}

const char* getprogname(void)
{
	const char** guest_executable_path =
		dlsym(RTLD_DEFAULT, "__machgate_guest_executable_path");
	const char* path = (guest_executable_path && *guest_executable_path)
		? *guest_executable_path
		: "machgate";
	const char* slash = strrchr(path, '/');
	return slash ? slash + 1 : path;
}

int getpeereid(int fd, uid_t* uid, gid_t* gid)
{
	if (!uid || !gid) {
		errno = EINVAL;
		return -1;
	}

	struct ucred cred;
	socklen_t cred_len = sizeof(cred);
	int result = getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &cred_len);
	if (result != 0)
		return -1;

	*uid = cred.uid;
	*gid = cred.gid;
	return 0;
}

void* getsegmentdata(const void* header, const char* segment_name,
                     unsigned long* size)
{
	(void)header;
	(void)segment_name;
	if (size)
		*size = 0;
	return NULL;
}

void* getsectdata(const char* segment_name, const char* section_name,
                  unsigned long* size)
{
	(void)section_name;
	return getsegmentdata(NULL, segment_name, size);
}

void* getsectiondata(const void* header, const char* segment_name,
                     const char* section_name, unsigned long* size)
{
	(void)section_name;
	return getsegmentdata(header, segment_name, size);
}

static void* shim_objc_make_single_element_array(const char* text);

/* ===== NSSearchPathEnumeration =====
 *
 * Apple's API for enumerating well-known user directories. Sugar's
 * sugar::file::desktop_path uses it to locate ~/Library/Application Support.
 * We redirect to the rewritten HOME so saves land under MACHGATE_HOME.
 *
 * Real semantics:
 *   state = NSStartSearchPathEnumeration(key, domainMask);
 *   while ((state = NSGetNextSearchPathEnumeration(state, path))) { ... }
 * Callers check the return value — a zero return means "don't use buffer".
 */
unsigned int NSStartSearchPathEnumeration(unsigned int key, unsigned int domain_mask)
{
	(void)key; (void)domain_mask;
	return 1;
}

unsigned int NSGetNextSearchPathEnumeration(unsigned int state, char *path)
{
	if (!state || !path) return 0;
	/* One entry, then done. State isn't tracked across calls — the only
	 * in-game caller (sugar::file::desktop_path) invokes this once.
	 * Buffer is a macOS PATH_MAX (1024) stack allocation. Truncate the
	 * fake_home prefix so "/Library/Application Support" always fits. */
	const char *home = get_fake_home();
	snprintf(path, 1024, "%.980s/Library/Application Support", home);
	return 0x80000000u; /* nonzero so caller uses the buffer */
}

/* ===== NSSearchPathForDirectoriesInDomains =====
 *
 * Foundation's well-known-directory lookup. RBX::FileSystem::getLogsDirectory
 * (Darwin FileSystem.mm) asks for NSLibraryDirectory in the user domain, then
 * takes objectAtIndex:0 and calls cStringUsingEncoding:. Without a real array
 * the shim's objc layer returned NULL and std::string(NULL) crashed in the
 * CSG DCD dump worker (SelfUnionTest SIGSEGV). We return a one-element NSArray
 * of NSStrings under the rewritten HOME, mirroring the enumeration variant.
 *
 * Apple directory keys (NSSearchPathDirectory):
 *   9  NSDocumentDirectory             ~/Documents
 *   5  NSLibraryDirectory              ~/Library
 *   13 NSCachesDirectory               ~/Library/Caches
 *   14 NSApplicationSupportDirectory   ~/Library/Application Support
 *   12 NSDesktopDirectory              ~/Desktop
 *   15 NSDownloadsDirectory            ~/Downloads
 *   17 NSMoviesDirectory               ~/Movies
 *   18 NSMusicDirectory                ~/Music
 *   19 NSPicturesDirectory             ~/Pictures
 */
void* NSSearchPathForDirectoriesInDomains(unsigned int directory,
                                          unsigned int domain_mask,
                                          unsigned int expand_tilde)
{
	static const struct {
		unsigned int key;
		const char* suffix;
	} directory_suffixes[] = {
		{ 5,  "/Library" },
		{ 9,  "/Documents" },
		{ 12, "/Desktop" },
		{ 13, "/Library/Caches" },
		{ 14, "/Library/Application Support" },
		{ 15, "/Downloads" },
		{ 17, "/Movies" },
		{ 18, "/Music" },
		{ 19, "/Pictures" },
	};
	const char* suffix = NULL;
	char path[1100];
	void* result;

	(void)domain_mask;
	(void)expand_tilde;

	for (size_t index = 0; index < sizeof(directory_suffixes) / sizeof(directory_suffixes[0]); index++) {
		if (directory_suffixes[index].key == directory) {
			suffix = directory_suffixes[index].suffix;
			break;
		}
	}
	if (!suffix)
		suffix = "/Library";

	snprintf(path, sizeof(path), "%.980s%s", get_fake_home(), suffix);
	result = shim_objc_make_single_element_array(path);
	if (!result && shim_trace_enabled())
		fprintf(stderr, "libsystem_shim: NSSearchPathForDirectoriesInDomains allocation failed key=%u\n",
		        directory);
	return result;
}

/* ===== Apple locale / ctype ===== */

/* _DefaultRuneLocale — Apple's locale data structure.
 * The __runetype[] table is indexed by character value; each entry is a
 * bitmask of character class flags.
 *
 * Apple _CTYPE bit definitions (from <ctype.h>):
 *   0x00000100 _CTYPE_A  alpha        0x00000200 _CTYPE_C  control
 *   0x00000400 _CTYPE_D  digit        0x00000800 _CTYPE_G  graph
 *   0x00001000 _CTYPE_L  lowercase    0x00002000 _CTYPE_P  punctuation
 *   0x00004000 _CTYPE_S  space        0x00008000 _CTYPE_U  uppercase
 *   0x00010000 _CTYPE_X  hex digit    0x00020000 _CTYPE_B  blank
 *   0x00040000 _CTYPE_R  print
 *
 * On macOS, _DefaultRuneLocale is the struct itself (not a pointer).
 * Game code accesses it as:  *(uint32_t*)(_DefaultRuneLocale + ch*4 + 0x3c)
 * where 0x3c is the byte offset from the struct start to __runetype[]. */

struct _RuneLocale {
	char __pad[0x3c];        /* magic + encoding + other fields */
	uint32_t __runetype[256];
	int32_t  __maplower[256];
	int32_t  __mapupper[256];
};

struct _RuneLocale _DefaultRuneLocale;

/* Apple ctype flag bits */
#define _CTYPE_A 0x00000100
#define _CTYPE_C 0x00000200
#define _CTYPE_D 0x00000400
#define _CTYPE_G 0x00000800
#define _CTYPE_L 0x00001000
#define _CTYPE_P 0x00002000
#define _CTYPE_S 0x00004000
#define _CTYPE_U 0x00008000
#define _CTYPE_X 0x00010000
#define _CTYPE_B 0x00020000
#define _CTYPE_R 0x00040000

__attribute__((constructor))
static void init_rune_locale(void)
{
	memcpy(_DefaultRuneLocale.__pad, "RuneMagi", 8);

	for (int c = 0; c <= 0x1F; c++)
		_DefaultRuneLocale.__runetype[c] = _CTYPE_C;
	_DefaultRuneLocale.__runetype[0x7F] = _CTYPE_C;

	_DefaultRuneLocale.__runetype[' ']  = _CTYPE_S | _CTYPE_B;
	_DefaultRuneLocale.__runetype['\t'] = _CTYPE_S | _CTYPE_B;
	_DefaultRuneLocale.__runetype['\n'] = _CTYPE_S;
	_DefaultRuneLocale.__runetype['\r'] = _CTYPE_S;
	_DefaultRuneLocale.__runetype['\f'] = _CTYPE_S;
	_DefaultRuneLocale.__runetype['\v'] = _CTYPE_S;

	for (int c = '0'; c <= '9'; c++)
		_DefaultRuneLocale.__runetype[c] = _CTYPE_D | _CTYPE_G | _CTYPE_R | _CTYPE_X;

	for (int c = 'A'; c <= 'Z'; c++)
		_DefaultRuneLocale.__runetype[c] = _CTYPE_A | _CTYPE_U | _CTYPE_G | _CTYPE_R;
	for (int c = 'A'; c <= 'F'; c++)
		_DefaultRuneLocale.__runetype[c] |= _CTYPE_X;

	for (int c = 'a'; c <= 'z'; c++)
		_DefaultRuneLocale.__runetype[c] = _CTYPE_A | _CTYPE_L | _CTYPE_G | _CTYPE_R;
	for (int c = 'a'; c <= 'f'; c++)
		_DefaultRuneLocale.__runetype[c] |= _CTYPE_X;

	const char* punct = "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";
	for (int i = 0; punct[i]; i++)
		_DefaultRuneLocale.__runetype[(unsigned char)punct[i]] = _CTYPE_P | _CTYPE_G | _CTYPE_R;

	for (int c = 0; c < 256; c++) {
		_DefaultRuneLocale.__maplower[c] = (c >= 'A' && c <= 'Z') ? c + 32 : c;
		_DefaultRuneLocale.__mapupper[c] = (c >= 'a' && c <= 'z') ? c - 32 : c;
	}
}

/* ___maskrune — Apple's character classification */
int __maskrune(int c, unsigned long mask)
{
	if (c < 0 || c > 255) return 0;
	return (int)(_DefaultRuneLocale.__runetype[c] & mask);
}

/* ___tolower / ___toupper — Apple exports these as function pointers */
int __tolower(int c)
{
	return tolower(c);
}

int __toupper(int c)
{
	return toupper(c);
}

/* ===== BSD string functions ===== */

/* strlcpy/strlcat — available in glibc 2.38+, but provide fallbacks
 * for older systems */
#if !defined(__GLIBC__) || (__GLIBC__ < 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ < 38)

size_t strlcpy(char *dst, const char *src, size_t size)
{
	size_t srclen = strlen(src);
	if (size > 0) {
		size_t copylen = srclen < size - 1 ? srclen : size - 1;
		memcpy(dst, src, copylen);
		dst[copylen] = '\0';
	}
	return srclen;
}

size_t strlcat(char *dst, const char *src, size_t size)
{
	size_t dstlen = strnlen(dst, size);
	if (dstlen == size) return size + strlen(src);
	return dstlen + strlcpy(dst + dstlen, src, size - dstlen);
}

#endif

/* ===== memset_pattern ===== */

void memset_pattern4(void *dst, const void *pattern, size_t len)
{
	const uint8_t *p = (const uint8_t *)pattern;
	uint8_t *d = (uint8_t *)dst;
	while (len >= 4) {
		memcpy(d, p, 4);
		d += 4;
		len -= 4;
	}
	if (len > 0) memcpy(d, p, len);
}

void memset_pattern8(void *dst, const void *pattern, size_t len)
{
	const uint8_t *p = (const uint8_t *)pattern;
	uint8_t *d = (uint8_t *)dst;
	while (len >= 8) {
		memcpy(d, p, 8);
		d += 8;
		len -= 8;
	}
	if (len > 0) memcpy(d, p, len);
}

void memset_pattern16(void *dst, const void *pattern, size_t len)
{
	const uint8_t *p = (const uint8_t *)pattern;
	uint8_t *d = (uint8_t *)dst;
	while (len >= 16) {
		memcpy(d, p, 16);
		d += 16;
		len -= 16;
	}
	if (len > 0) memcpy(d, p, len);
}

/* ===== pthread mutex ABI translation ===== */

/*
 * macOS pthread_mutex_t is 64 bytes: { long __sig; char __opaque[56]; }
 * where __sig = 0x32AAABA7 for PTHREAD_MUTEX_INITIALIZER.
 *
 * Linux/glibc aarch64 pthread_mutex_t is 48 bytes with PTHREAD_MUTEX_INITIALIZER = {0}.
 *
 * Since Linux's struct fits within macOS's, we can re-initialize in-place.
 * We detect macOS-format by checking for the signature at offset 0.
 */

#define DARWIN_PTHREAD_MUTEX_SIG 0x32AAABA7L
#define DARWIN_PTHREAD_RMUTEX_SIG 0x32AAABA1L
/*
 * All Apple pthread_mutex static initializers share the prefix 0x32AAABA0 with
 * the mutex type encoded in the low nibble: A7=normal, A1=errorcheck,
 * A2=recursive, A3=firstfit. Detect by mask (as Apple's own
 * _PTHREAD_MUTEX_SIG_CMP does) rather than enumerating exact values — missing a
 * type (e.g. A2 = PTHREAD_RECURSIVE_MUTEX_INITIALIZER, written by Mina's
 * ycTempString.cpp ctor) leaves the signature in place, and glibc then reads it
 * as a locked __lock and deadlocks in __lll_lock_wait on the first lock.
 */
#define DARWIN_PTHREAD_MUTEX_SIG_MASK 0xFFFFFFF0L
#define DARWIN_PTHREAD_MUTEX_SIG_CMP  0x32AAABA0L
#define DARWIN_PTHREAD_COND_SIG 0x3CB0B1BBL

/*
 * We need wrappers that detect macOS-format pthread objects and re-init
 * them for Linux before use. The wrappers are exported with the standard
 * names (pthread_mutex_lock, etc.) so the resolver's dlsym finds them.
 * Inside, we call the real glibc functions via RTLD_NEXT.
 */

#include <dlfcn.h>

/* Real glibc pthread function pointers, resolved at load time */
static int (*real_pthread_mutex_lock)(pthread_mutex_t *);
static int (*real_pthread_mutex_unlock)(pthread_mutex_t *);
static int (*real_pthread_mutex_trylock)(pthread_mutex_t *);
static int (*real_pthread_mutex_init)(pthread_mutex_t *, const pthread_mutexattr_t *);
static int (*real_pthread_mutex_destroy)(pthread_mutex_t *);
static int (*real_pthread_cond_wait)(pthread_cond_t *, pthread_mutex_t *);
static int (*real_pthread_cond_timedwait)(pthread_cond_t *, pthread_mutex_t *, const struct timespec *);
static int (*real_pthread_cond_signal)(pthread_cond_t *);
static int (*real_pthread_cond_broadcast)(pthread_cond_t *);
static int (*real_pthread_cond_init)(pthread_cond_t *, const pthread_condattr_t *);
static int (*real_pthread_cond_destroy)(pthread_cond_t *);
static int (*real_pthread_rwlock_rdlock)(pthread_rwlock_t *);
static int (*real_pthread_rwlock_wrlock)(pthread_rwlock_t *);
static int (*real_pthread_rwlock_unlock)(pthread_rwlock_t *);

static int shim_trace_enabled(void);
static int darwin_signal_to_linux(int darwin_signal);

#define DARWIN_TSD_FIRST_KEY 128
#define DARWIN_TSD_LAST_KEY 511
#define DARWIN_TSD_KEY_COUNT (DARWIN_TSD_LAST_KEY - DARWIN_TSD_FIRST_KEY + 1)

static unsigned int next_darwin_tsd_key = DARWIN_TSD_FIRST_KEY;
static void (*darwin_tsd_destructors[DARWIN_TSD_KEY_COUNT])(void *);
static __thread void *darwin_tsd_values[DARWIN_TSD_KEY_COUNT];
static pthread_t machgate_main_pthread;
static int machgate_main_pthread_set;

#define PTHREAD_ID_SLOT_COUNT 2048

struct pthread_id_slot {
	int used;
	pthread_t thread;
	uint32_t tid;
	uint64_t identity;
};

struct shim_thread_start_context {
	void* (*start_routine)(void*);
	void* arg;
	size_t map_size;
};

static struct pthread_id_slot pthread_id_slots[PTHREAD_ID_SLOT_COUNT];
static volatile int pthread_id_slots_lock;
static uint64_t pthread_next_identity = 1;

static void lock_pthread_id_slots(void)
{
	while (__sync_lock_test_and_set(&pthread_id_slots_lock, 1))
		sched_yield();
}

static void unlock_pthread_id_slots(void)
{
	__sync_lock_release(&pthread_id_slots_lock);
}

static uint64_t next_pthread_identity_unlocked(void)
{
	uint64_t result = pthread_next_identity++;
	if (!result)
		result = pthread_next_identity++;
	return result;
}

static struct pthread_id_slot* find_pthread_id_slot_unlocked(pthread_t thread)
{
	for (int i = 0; i < PTHREAD_ID_SLOT_COUNT; i++) {
		if (pthread_id_slots[i].used &&
		    pthread_equal(pthread_id_slots[i].thread, thread))
			return &pthread_id_slots[i];
	}
	return NULL;
}

static struct pthread_id_slot* alloc_pthread_id_slot_unlocked(pthread_t thread)
{
	struct pthread_id_slot* slot = find_pthread_id_slot_unlocked(thread);
	if (slot)
		return slot;

	for (int i = 0; i < PTHREAD_ID_SLOT_COUNT; i++) {
		if (!pthread_id_slots[i].used) {
			pthread_id_slots[i].used = 1;
			pthread_id_slots[i].thread = thread;
			pthread_id_slots[i].identity = next_pthread_identity_unlocked();
			return &pthread_id_slots[i];
		}
	}
	return NULL;
}

static uint64_t register_current_pthread_identity(void)
{
	pthread_t thread = pthread_self();
	uint32_t tid = (uint32_t)syscall(SYS_gettid);
	uint64_t result;

	lock_pthread_id_slots();
	struct pthread_id_slot* slot = alloc_pthread_id_slot_unlocked(thread);
	if (slot) {
		slot->tid = tid;
		result = slot->identity;
	} else
		result = tid;
	unlock_pthread_id_slots();
	return result;
}

static uint64_t pthread_identity_for_thread(pthread_t thread)
{
	uint64_t result;

	if (!thread || pthread_equal(thread, pthread_self()))
		return register_current_pthread_identity();

	lock_pthread_id_slots();
	struct pthread_id_slot* slot = alloc_pthread_id_slot_unlocked(thread);
	if (slot)
		result = slot->identity;
	else
		result = (uint64_t)(uintptr_t)thread;
	unlock_pthread_id_slots();
	return result;
}

static void forget_pthread_identity(pthread_t thread)
{
	lock_pthread_id_slots();
	struct pthread_id_slot* slot = find_pthread_id_slot_unlocked(thread);
	if (slot)
		memset(slot, 0, sizeof(*slot));
	unlock_pthread_id_slots();
}

static inline int darwin_tsd_key_index(pthread_key_t key)
{
	unsigned int value = (unsigned int)key;
	if (value < DARWIN_TSD_FIRST_KEY || value > DARWIN_TSD_LAST_KEY)
		return -1;
	return (int)(value - DARWIN_TSD_FIRST_KEY);
}

static int shim_tsd_allocate_key(pthread_key_t *key, void (*destructor)(void *))
{
	unsigned int value;
	int index;

	value = __sync_fetch_and_add(&next_darwin_tsd_key, 1);
	if (value > DARWIN_TSD_LAST_KEY)
		return EAGAIN;

	index = (int)(value - DARWIN_TSD_FIRST_KEY);
	darwin_tsd_destructors[index] = destructor;
	darwin_tsd_values[index] = NULL;
	*key = (pthread_key_t)value;

	return 0;
}

static int shim_tsd_set(pthread_key_t key, void *value)
{
	int index = darwin_tsd_key_index(key);

	if (index < 0)
		return EINVAL;

	darwin_tsd_values[index] = value;

	return 0;
}

static void *shim_tsd_get(pthread_key_t key)
{
	int index = darwin_tsd_key_index(key);

	if (index < 0)
		return NULL;

	return darwin_tsd_values[index];
}

__attribute__((constructor(101)))
static void init_pthread_wrappers(void)
{
	/* Resolve real glibc pthread functions via RTLD_NEXT.
	 * This skips our own definitions and finds glibc's. */
	real_pthread_mutex_lock = dlsym(RTLD_NEXT, "pthread_mutex_lock");
	real_pthread_mutex_unlock = dlsym(RTLD_NEXT, "pthread_mutex_unlock");
	real_pthread_mutex_trylock = dlsym(RTLD_NEXT, "pthread_mutex_trylock");
	real_pthread_mutex_init = dlsym(RTLD_NEXT, "pthread_mutex_init");
	real_pthread_mutex_destroy = dlsym(RTLD_NEXT, "pthread_mutex_destroy");
	real_pthread_cond_wait = dlsym(RTLD_NEXT, "pthread_cond_wait");
	real_pthread_cond_timedwait = dlsym(RTLD_NEXT, "pthread_cond_timedwait");
	real_pthread_cond_signal = dlsym(RTLD_NEXT, "pthread_cond_signal");
	real_pthread_cond_broadcast = dlsym(RTLD_NEXT, "pthread_cond_broadcast");
	real_pthread_cond_init = dlsym(RTLD_NEXT, "pthread_cond_init");
	real_pthread_cond_destroy = dlsym(RTLD_NEXT, "pthread_cond_destroy");
	real_pthread_rwlock_rdlock = dlsym(RTLD_NEXT, "pthread_rwlock_rdlock");
	real_pthread_rwlock_wrlock = dlsym(RTLD_NEXT, "pthread_rwlock_wrlock");
	real_pthread_rwlock_unlock = dlsym(RTLD_NEXT, "pthread_rwlock_unlock");
	machgate_main_pthread = pthread_self();
	machgate_main_pthread_set = 1;
	register_current_pthread_identity();
	get_fake_home();

	if (!real_pthread_mutex_lock)
		fprintf(stderr, "libsystem_shim: WARNING: could not resolve real pthread_mutex_lock\n");
	else if (shim_startup_log_enabled())
		fprintf(stderr, "libsystem_shim: pthread wrappers initialized (real=%p)\n", real_pthread_mutex_lock);
}

/* Check if this looks like a macOS-initialized mutex and reinit for Linux */
static inline void fixup_mutex(pthread_mutex_t *mutex)
{
	long *sig = (long *)mutex;
	if ((*sig & DARWIN_PTHREAD_MUTEX_SIG_MASK) == DARWIN_PTHREAD_MUTEX_SIG_CMP) {
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		memset(mutex, 0, sizeof(pthread_mutex_t));
		real_pthread_mutex_init(mutex, &attr);
		pthread_mutexattr_destroy(&attr);
	}
}

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
	fixup_mutex(mutex);  /* macOS signature detection */
	/* Upgrade zero-initialized (PTHREAD_MUTEX_INITIALIZER) mutexes to
	 * recursive. macOS game code may re-lock from the same thread
	 * (works on macOS, deadlocks on Linux). Without LD_PRELOAD,
	 * only Mach-O code reaches these wrappers (via GOT patching). */
	{
		static const char zeros[sizeof(pthread_mutex_t)] = {0};
		if (memcmp(mutex, zeros, sizeof(pthread_mutex_t)) == 0) {
			pthread_mutexattr_t attr;
			pthread_mutexattr_init(&attr);
			pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
			real_pthread_mutex_init(mutex, &attr);
			pthread_mutexattr_destroy(&attr);
		}
	}
	return real_pthread_mutex_lock(mutex);
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
	return real_pthread_mutex_unlock(mutex);
}

int pthread_mutex_trylock(pthread_mutex_t *mutex)
{
	fixup_mutex(mutex);
	return real_pthread_mutex_trylock(mutex);
}

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
	memset(mutex, 0, sizeof(pthread_mutex_t));
	/* On macOS, PTHREAD_MUTEX_DEFAULT allows same-thread re-locking without
	 * deadlock in some configurations. Make mutexes recursive to match. */
	if (!attr) {
		pthread_mutexattr_t rattr;
		pthread_mutexattr_init(&rattr);
		pthread_mutexattr_settype(&rattr, PTHREAD_MUTEX_RECURSIVE);
		int ret = real_pthread_mutex_init(mutex, &rattr);
		pthread_mutexattr_destroy(&rattr);
		return ret;
	}
	return real_pthread_mutex_init(mutex, attr);
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	return real_pthread_mutex_destroy(mutex);
}

int pthread_key_create(pthread_key_t *key, void (*destructor)(void *))
{
	int result = shim_tsd_allocate_key(key, destructor);

	if (shim_trace_enabled())
		fprintf(stderr, "libsystem_shim: pthread_key_create -> %u\n",
		        (unsigned int)*key);

	return result;
}

int pthread_setspecific(pthread_key_t key, const void *value)
{
	int result = shim_tsd_set(key, (void *)value);

	if (shim_trace_enabled())
		fprintf(stderr, "libsystem_shim: pthread_setspecific(%u, %p) caller=%p\n",
		        (unsigned int)key, value, __builtin_return_address(0));

	return result;
}

void *pthread_getspecific(pthread_key_t key)
{
	void *result = shim_tsd_get(key);

	if (shim_trace_enabled())
		fprintf(stderr, "libsystem_shim: pthread_getspecific(%u) -> %p caller=%p\n",
		        (unsigned int)key, result, __builtin_return_address(0));

	return result;
}

int pthread_once(pthread_once_t *once_control, void (*init_routine)(void))
{
	int *sig = (int *)once_control;
	int val = *sig;

	if (val == 2 || val == ~0) {
		return 0;
	}

	if (val == 0 || val == (int)0x30B1BCBA) {
		if (__sync_bool_compare_and_swap(sig, val, 1)) {
			init_routine();
			__sync_synchronize();
			*sig = 2;
			return 0;
		}
	}

	while (*sig == 1) {
		sched_yield();
	}
	return 0;
}


static int shim_trace_enabled(void)
{
	const char* value = getenv("MACHGATE_TRACE_SHIM");
	return value && value[0] && strcmp(value, "0") != 0;
}

static int shim_cxx_init_full_trace_enabled(void)
{
	const char* value = getenv("MACHGATE_TRACE_CXX_INIT");
	return value && (strcmp(value, "full") == 0 ||
	                 strcmp(value, "verbose") == 0 ||
	                 strcmp(value, "all") == 0 ||
	                 strcmp(value, "2") == 0);
}

static int shim_delta_vm_trace_enabled(void)
{
	const char* value = getenv("MACHGATE_TRACE_DELTA_VM");
	if (value && value[0] && strcmp(value, "0") != 0)
		return 1;
	return shim_trace_enabled();
}

static pid_t shim_trace_tid(void)
{
	return (pid_t)syscall(SYS_gettid);
}

static unsigned long shim_trace_pthread_self(void)
{
	return (unsigned long)pthread_self();
}

static const char* shim_trace_path(const char* path)
{
	return path ? path : "(null)";
}

static int shim_signal_trace_enabled(void)
{
	const char* value = getenv("MACHGATE_TRACE_SIGNALS");
	return value && value[0] && strcmp(value, "0") != 0;
}

void machgate_shim_note_init_context(const char* kind, int index, int total,
                                     uintptr_t address)
{
	shim_init_kind = kind;
	shim_init_index = index;
	shim_init_total = total;
	shim_init_address = address;
}

void machgate_shim_clear_init_context(void)
{
	shim_init_kind = NULL;
	shim_init_index = -1;
	shim_init_total = 0;
	shim_init_address = 0;
}

static void trace_init_context(void)
{
	if (!shim_init_kind)
		return;

	fprintf(stderr,
	        "libsystem_shim: current initializer kind=%s progress=%d/%d index=%d address=%p\n",
	        shim_init_kind, shim_init_index + 1, shim_init_total,
	        shim_init_index, (void*)shim_init_address);
}

static void trace_guest_address_context(const char* label, uintptr_t address)
{
	typedef void (*trace_guest_address_fn)(const char*, uintptr_t);
	static trace_guest_address_fn trace_guest_address;
	static int looked_up;

	if (!looked_up) {
		trace_guest_address = (trace_guest_address_fn)dlsym(
			RTLD_DEFAULT, "machgate_trace_guest_address");
		looked_up = 1;
	}

	if (trace_guest_address)
		trace_guest_address(label, address);
}

static void trace_signal_indirect_branch(uintptr_t call_site, void* ucontext)
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
	        "libsystem_shim: signal callsite %p insn=0x%08x %s x%d target=%p\n",
	        (void*)call_site, insn, mnemonic, reg,
	        (void*)trace_ucontext_reg(ucontext, reg));
}

static int trace_read_u64(uintptr_t address, uint64_t* value)
{
	if (!address)
		return 0;
	memcpy(value, (void*)address, sizeof(*value));
	return 1;
}

static int trace_read_u32(uintptr_t address, uint32_t* value)
{
	if (!address)
		return 0;
	memcpy(value, (void*)address, sizeof(*value));
	return 1;
}

static void trace_copy_string_preview(char* out, size_t out_size,
                                      uintptr_t address, size_t length)
{
	size_t copied = 0;

	if (!out_size)
		return;
	out[0] = '\0';
	if (!address)
		return;
	if (length >= out_size)
		length = out_size - 1;
	for (; copied < length; copied++) {
		unsigned char ch = ((const unsigned char*)address)[copied];
		if (!ch)
			break;
		out[copied] = isprint(ch) ? (char)ch : '.';
	}
	out[copied] = '\0';
}

static void trace_copy_c_string_preview(char* out, size_t out_size,
                                        uintptr_t address)
{
	trace_copy_string_preview(out, out_size, address, out_size - 1);
}

static void trace_signal_pointer_words(const char* label, uintptr_t address)
{
	uint64_t words[4] = {0};

	if (!address)
		return;
	if (!trace_read_u64(address, &words[0]) ||
	    !trace_read_u64(address + 8, &words[1]) ||
	    !trace_read_u64(address + 16, &words[2]) ||
	    !trace_read_u64(address + 24, &words[3]))
		return;

	fprintf(stderr,
	        "libsystem_shim: signal memory %s=%p [0]=%p [8]=%p [16]=%p [24]=%p\n",
	        label, (void*)address, (void*)(uintptr_t)words[0],
	        (void*)(uintptr_t)words[1], (void*)(uintptr_t)words[2],
	        (void*)(uintptr_t)words[3]);
}

static void trace_signal_pointer_word_contexts(const char* label,
                                               const uint64_t* words,
                                               size_t count)
{
	for (size_t i = 0; i < count; i++) {
		uintptr_t value = (uintptr_t)words[i];
		char word_label[96];

		if (!value)
			continue;
		snprintf(word_label, sizeof(word_label), "%s[%zu]", label,
		         i * sizeof(uint64_t));
		trace_guest_address_context(word_label, value);
	}
}

static void trace_signal_pointer_words_wide(const char* label, uintptr_t address)
{
	uint64_t words[8] = {0};

	if (!address)
		return;
	for (size_t i = 0; i < 8; i++) {
		if (!trace_read_u64(address + i * 8, &words[i]))
			return;
	}

	fprintf(stderr,
	        "libsystem_shim: signal memory %s=%p [0]=%p [8]=%p [16]=%p [24]=%p [32]=%p [40]=%p [48]=%p [56]=%p\n",
	        label, (void*)address,
	        (void*)(uintptr_t)words[0], (void*)(uintptr_t)words[1],
	        (void*)(uintptr_t)words[2], (void*)(uintptr_t)words[3],
	        (void*)(uintptr_t)words[4], (void*)(uintptr_t)words[5],
	        (void*)(uintptr_t)words[6], (void*)(uintptr_t)words[7]);
	trace_signal_pointer_word_contexts(label, words,
	                                   sizeof(words) / sizeof(words[0]));
}

static void trace_signal_register_context(void* ucontext)
{
	for (int reg = 0; reg <= 2; reg++) {
		char label[32];
		uintptr_t value = trace_ucontext_reg(ucontext, reg);

		if (!value)
			continue;
		snprintf(label, sizeof(label), "signal.x%d", reg);
		trace_guest_address_context(label, value);
	}
	if (trace_ucontext_reg(ucontext, 8))
		trace_guest_address_context("signal.x8", trace_ucontext_reg(ucontext, 8));
	for (int reg = 19; reg <= 28; reg++) {
		char label[32];
		uintptr_t value = trace_ucontext_reg(ucontext, reg);

		if (!value)
			continue;
		snprintf(label, sizeof(label), "signal.x%d", reg);
		trace_guest_address_context(label, value);
	}
}

static int trace_decode_ldr_unsigned_64(uint32_t insn, int* rt, int* rn,
                                        unsigned* offset)
{
	if ((insn & 0xffc00000u) != 0xf9400000u)
		return 0;
	*rt = (int)(insn & 0x1fu);
	*rn = (int)((insn >> 5) & 0x1fu);
	*offset = (unsigned)(((insn >> 10) & 0xfffu) * 8u);
	return 1;
}

static void trace_signal_catch2_assertion_result(const char* label,
                                                 uintptr_t result)
{
	uint64_t macro_data = 0;
	uint64_t macro_size = 0;
	uint64_t file_data = 0;
	uint64_t line = 0;
	uint64_t expr_data = 0;
	uint64_t expr_size = 0;
	uint32_t disposition = 0;
	uint32_t result_type = 0;
	char macro[96];
	char file[192];
	char expr[192];

	if (!result)
		return;
	if (!trace_read_u64(result, &macro_data) ||
	    !trace_read_u64(result + 8, &macro_size) ||
	    !trace_read_u64(result + 16, &file_data) ||
	    !trace_read_u64(result + 24, &line) ||
	    !trace_read_u64(result + 32, &expr_data) ||
	    !trace_read_u64(result + 40, &expr_size))
		return;
	trace_read_u32(result + 48, &disposition);
	trace_read_u32(result + 120, &result_type);

	fprintf(stderr,
	        "libsystem_shim: catch2 %s assertion-result-raw=%p macro-data=%p macro-size=%llu file-data=%p line=%llu expr-data=%p expr-size=%llu disposition=0x%x result-type-candidate=0x%x\n",
	        label, (void*)result, (void*)(uintptr_t)macro_data,
	        (unsigned long long)macro_size, (void*)(uintptr_t)file_data,
	        (unsigned long long)line, (void*)(uintptr_t)expr_data,
	        (unsigned long long)expr_size, disposition, result_type);

	trace_copy_string_preview(macro, sizeof(macro), (uintptr_t)macro_data,
	                          (size_t)macro_size);
	trace_copy_c_string_preview(file, sizeof(file), (uintptr_t)file_data);
	trace_copy_string_preview(expr, sizeof(expr), (uintptr_t)expr_data,
	                          (size_t)expr_size);

	fprintf(stderr,
	        "libsystem_shim: catch2 %s assertion-result=%p macro='%s' file='%s' line=%llu expr='%s' disposition=0x%x result-type-candidate=0x%x\n",
	        label, (void*)result, macro, file,
	        (unsigned long long)line, expr, disposition, result_type);
	trace_guest_address_context("catch2.assertion-result", result);
	trace_guest_address_context("catch2.assertion.macro", (uintptr_t)macro_data);
	trace_guest_address_context("catch2.assertion.file", (uintptr_t)file_data);
	trace_guest_address_context("catch2.assertion.expr", (uintptr_t)expr_data);
}

static void trace_signal_catch2_null_active_testcase(void* ucontext)
{
	uintptr_t result = trace_ucontext_reg(ucontext, 1);
	uintptr_t saved_result = trace_ucontext_reg(ucontext, 20);

	fprintf(stderr,
	        "libsystem_shim: catch2 RunContext m_activeTestCase is null while handling assertion\n");
	shim_dump_recent_alloc_events("catch2-null-active-testcase", NULL);
	trace_signal_catch2_assertion_result("x1", result);
	if (saved_result && saved_result != result)
		trace_signal_catch2_assertion_result("x20", saved_result);
}

static void trace_signal_faulting_load_context(uintptr_t pc, void* ucontext)
{
	uint32_t insn = 0;
	int rt = 0;
	int rn = 0;
	unsigned offset = 0;
	uintptr_t base = 0;
	uintptr_t effective = 0;

	if (!pc)
		return;
	memcpy(&insn, (void*)pc, sizeof(insn));
	if (!trace_decode_ldr_unsigned_64(insn, &rt, &rn, &offset))
		return;

	base = trace_ucontext_reg(ucontext, rn);
	effective = base + offset;
	fprintf(stderr,
	        "libsystem_shim: signal faulting-load pc=%p insn=0x%08x ldr x%d,[x%d,#%u] base=%p effective=%p\n",
	        (void*)pc, insn, rt, rn, offset, (void*)base,
	        (void*)effective);
	if (base)
		trace_signal_pointer_words_wide("faulting-load.base", base);
	if (effective && effective != base)
		trace_guest_address_context("faulting-load.effective", effective);

	if (pc >= 4) {
		uint32_t previous = 0;
		int previous_rt = 0;
		int previous_rn = 0;
		unsigned previous_offset = 0;
		uintptr_t previous_base = 0;
		uintptr_t previous_effective = 0;
		uint64_t loaded = 0;

		memcpy(&previous, (void*)(pc - 4), sizeof(previous));
		if (trace_decode_ldr_unsigned_64(previous, &previous_rt,
		                                 &previous_rn, &previous_offset) &&
		    previous_rt == rn) {
			previous_base = trace_ucontext_reg(ucontext, previous_rn);
			previous_effective = previous_base + previous_offset;
			if (previous_effective)
				trace_read_u64(previous_effective, &loaded);
			fprintf(stderr,
			        "libsystem_shim: signal faulting-load source pc=%p insn=0x%08x ldr x%d,[x%d,#%u] base=%p effective=%p loaded=%p\n",
			        (void*)(pc - 4), previous, previous_rt, previous_rn,
			        previous_offset, (void*)previous_base,
			        (void*)previous_effective, (void*)(uintptr_t)loaded);
			if (!base && rn == 8 && rt == 0 && previous_rn == 19 &&
			    previous_offset == 32)
				trace_signal_catch2_null_active_testcase(ucontext);
			if (previous_base)
				trace_signal_pointer_words_wide("faulting-load.source-base",
				                                previous_base);
			if (previous_effective)
				trace_guest_address_context("faulting-load.source-effective",
				                            previous_effective);
		}
	}
}

static void trace_signal_tree_insert_context(uintptr_t pc, void* ucontext)
{
	uint32_t insn = 0;

	if (!pc)
		return;
	memcpy(&insn, (void*)pc, sizeof(insn));
	if (insn != 0xf9400108u)
		return;

	fprintf(stderr,
	        "libsystem_shim: signal libcxx-tree-insert probe pc=%p\n",
	        (void*)pc);
	trace_signal_pointer_words("x0.tree", trace_ucontext_reg(ucontext, 0));
	trace_signal_pointer_words("x1.child-slot", trace_ucontext_reg(ucontext, 1));
	trace_signal_pointer_words("x2.child-slot", trace_ucontext_reg(ucontext, 2));
	fprintf(stderr, "libsystem_shim: signal memory x3.new-node=%p (not dereferenced)\n",
	        (void*)trace_ucontext_reg(ucontext, 3));
	trace_guest_address_context("signal.tree", trace_ucontext_reg(ucontext, 0));
	trace_guest_address_context("signal.tree+8", trace_ucontext_reg(ucontext, 0) + 8);
}

static int shim_wait_trace_enabled(void)
{
	const char* value = getenv("MACHGATE_TRACE_WAIT");
	return value && value[0] && strcmp(value, "0") != 0;
}

static int shim_alloc_trace_cached(int* cache, const char* name)
{
	if (*cache == 0) {
		const char* value = getenv(name);
		*cache = value && value[0] && strcmp(value, "0") != 0 ? 1 : -1;
	}
	return *cache > 0;
}

static int shim_alloc_trace_enabled(void)
{
	static int cache;
	return shim_alloc_trace_cached(&cache, "MACHGATE_TRACE_ALLOC");
}

static int shim_alloc_trace_full_enabled(void)
{
	static int cache;
	if (cache == 0) {
		const char* value = getenv("MACHGATE_TRACE_ALLOC");
		cache = value && (strcmp(value, "2") == 0 ||
		                 strcmp(value, "all") == 0 ||
		                 strcmp(value, "full") == 0 ||
		                 strcmp(value, "verbose") == 0) ? 1 : -1;
	}
	return cache > 0;
}

static int shim_alloc_mismatch_trace_enabled(void)
{
	static int cache;
	if (cache == 0) {
		const char* value = getenv("MACHGATE_TRACE_ALLOC_MISMATCH");
		if (value && value[0] && strcmp(value, "0") != 0)
			cache = 1;
		else
			cache = shim_alloc_trace_enabled() ? 1 : -1;
	}
	return cache > 0;
}

static int shim_alloc_signal_dump_enabled(void)
{
	static int cache;
	if (cache == 0) {
		const char* value = getenv("MACHGATE_TRACE_ALLOC_MISMATCH");
		if (value && strcmp(value, "0") == 0)
			cache = -1;
		else
			cache = 1;
	}
	return cache > 0;
}

static size_t shim_alloc_trace_size(void)
{
	const char* value = getenv("MACHGATE_TRACE_ALLOC_SIZE");
	char* end = NULL;
	unsigned long long size;

	if (!value || !*value)
		return 0;
	errno = 0;
	size = strtoull(value, &end, 0);
	if (errno || end == value)
		return 0;
	return (size_t)size;
}

#define MACHGATE_ALLOC_EVENT_RING_SIZE 128
#define MACHGATE_ALLOC_EVENT_DEFAULT_DUMP_LIMIT 16

static int shim_alloc_recent_full_enabled(void)
{
	const char* value = getenv("MACHGATE_TRACE_ALLOC_RECENT_FULL");
	return value && value[0] && strcmp(value, "0") != 0;
}

static size_t shim_alloc_recent_limit(void)
{
	const char* value = getenv("MACHGATE_TRACE_ALLOC_RECENT_LIMIT");
	char* end = NULL;
	unsigned long long limit;

	if (!value || !*value)
		return MACHGATE_ALLOC_EVENT_DEFAULT_DUMP_LIMIT;
	errno = 0;
	limit = strtoull(value, &end, 0);
	if (errno || end == value)
		return MACHGATE_ALLOC_EVENT_DEFAULT_DUMP_LIMIT;
	if (limit > MACHGATE_ALLOC_EVENT_RING_SIZE)
		return MACHGATE_ALLOC_EVENT_RING_SIZE;
	return (size_t)limit;
}

struct machgate_alloc_event {
	const char* op;
	const void* ptr;
	size_t size;
	size_t old_size;
	void* zone;
	void* caller;
	int known;
};

static struct machgate_alloc_event alloc_event_ring[MACHGATE_ALLOC_EVENT_RING_SIZE];
static unsigned long alloc_event_seq;

static void shim_record_alloc_event(const char* op, const void* ptr,
                                    size_t size, size_t old_size, void* zone,
                                    void* caller, int known)
{
	struct machgate_alloc_event* event =
	    &alloc_event_ring[alloc_event_seq % MACHGATE_ALLOC_EVENT_RING_SIZE];

	event->op = op;
	event->ptr = ptr;
	event->size = size;
	event->old_size = old_size;
	event->zone = zone;
	event->caller = caller;
	event->known = known;
	alloc_event_seq++;
}

static size_t shim_alloc_event_extent(const struct machgate_alloc_event* event)
{
	return event->size > event->old_size ? event->size : event->old_size;
}

static void shim_dump_related_alloc_event(unsigned long seq,
                                          const struct machgate_alloc_event* event,
                                          const void* ptr)
{
	uintptr_t target;
	uintptr_t base;
	size_t extent;
	long long offset;

	if (!ptr || !event->ptr)
		return;
	extent = shim_alloc_event_extent(event);
	if (!extent)
		return;
	target = (uintptr_t)ptr;
	base = (uintptr_t)event->ptr;
	if (target < base) {
		if (base - target > 64)
			return;
		offset = -(long long)(base - target);
	} else {
		if (target - base > extent + 64)
			return;
		offset = (long long)(target - base);
	}
	fprintf(stderr,
	        "libsystem_shim: alloc related[%lu] op=%s base=%p offset=%lld extent=%zu caller=%p\n",
	        seq, event->op, event->ptr, offset, extent,
	        event->caller);
	if (event->caller)
		trace_guest_address_context("alloc.related-caller",
		                            (uintptr_t)event->caller);
}

static void shim_dump_recent_alloc_events(const char* reason, const void* ptr)
{
	unsigned long start;
	size_t limit = shim_alloc_recent_limit();
	int full_dump = shim_alloc_recent_full_enabled() ||
	    shim_alloc_trace_full_enabled();
	int symbolize_callers = full_dump;

	if (!shim_alloc_signal_dump_enabled() &&
	    !shim_alloc_mismatch_trace_enabled())
		return;
	if (alloc_event_seq == 0)
		return;
	if (limit == 0 && !full_dump)
		return;

	fprintf(stderr,
	        "libsystem_shim: alloc recent-events reason=%s ptr=%p seq=%lu\n",
	        reason ? reason : "(none)", ptr, alloc_event_seq);
	if (full_dump || limit > MACHGATE_ALLOC_EVENT_RING_SIZE)
		limit = MACHGATE_ALLOC_EVENT_RING_SIZE;
	start = alloc_event_seq > limit ? alloc_event_seq - limit : 0;
	for (unsigned long seq = start; seq < alloc_event_seq; seq++) {
		const struct machgate_alloc_event* event =
		    &alloc_event_ring[seq % MACHGATE_ALLOC_EVENT_RING_SIZE];
		if (!event->op)
			continue;
		fprintf(stderr,
		        "libsystem_shim: alloc recent[%lu] op=%s ptr=%p size=%zu old_size=%zu zone=%p caller=%p known=%d\n",
		        seq, event->op, event->ptr, event->size, event->old_size,
		        event->zone, event->caller, event->known);
		shim_dump_related_alloc_event(seq, event, ptr);
		if (symbolize_callers && event->caller)
			trace_guest_address_context("alloc.caller", (uintptr_t)event->caller);
	}
}

static void shim_trace_alloc_event_at(const char* op, const void* ptr,
                                      size_t size, size_t old_size,
                                      void* caller, int known)
{
	size_t trace_size;

	shim_record_alloc_event(op, ptr, size, old_size, NULL, caller, known);

	if (!shim_alloc_trace_enabled())
		return;
	trace_size = shim_alloc_trace_size();
	if (!shim_alloc_trace_full_enabled() &&
	    (!trace_size || (size != trace_size && old_size != trace_size)))
		return;
	fprintf(stderr,
	        "libsystem_shim: alloc %s ptr=%p size=%zu old_size=%zu caller=%p\n",
	        op, ptr, size, old_size, caller);
	if (caller)
		trace_guest_address_context("alloc.caller", (uintptr_t)caller);
}

static int shim_host_sigchld_handler_enabled(void)
{
	const char* value = getenv("MACHGATE_ENABLE_HOST_SIGCHLD_HANDLER");
	return value && value[0] && strcmp(value, "0") != 0;
}

static int shim_fd_trace_fd(void)
{
	static int trace_fd = -2;
	int current_fd = __atomic_load_n(&trace_fd, __ATOMIC_ACQUIRE);

	if (current_fd != -2)
		return current_fd;

	const char* trace_path = getenv("MACHGATE_FD_TRACE_FILE");
	if (!trace_path || !*trace_path) {
		__atomic_store_n(&trace_fd, -1, __ATOMIC_RELEASE);
		return -1;
	}

	int opened_fd = (int)syscall(SYS_openat, AT_FDCWD, trace_path,
	                             O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC,
	                             0644);
	if (opened_fd < 0) {
		__atomic_store_n(&trace_fd, -1, __ATOMIC_RELEASE);
		return -1;
	}

	int high_fd = (int)syscall(SYS_fcntl, opened_fd, F_DUPFD_CLOEXEC, 1000);
	int saved_errno = errno;
	syscall(SYS_close, opened_fd);
	errno = saved_errno;
	if (high_fd < 0) {
		__atomic_store_n(&trace_fd, -1, __ATOMIC_RELEASE);
		return -1;
	}

	if (__sync_bool_compare_and_swap(&trace_fd, -2, high_fd))
		return high_fd;

	syscall(SYS_close, high_fd);
	return __atomic_load_n(&trace_fd, __ATOMIC_ACQUIRE);
}

static void shim_fd_trace_log(const char* format, ...)
{
	int trace_fd = shim_fd_trace_fd();
	char buffer[1536];
	int offset;
	int length;
	va_list args;
	struct timespec now;

	if (trace_fd < 0)
		return;

	clock_gettime(CLOCK_MONOTONIC, &now);
	offset = snprintf(buffer, sizeof(buffer), "fdtrace t=%lld.%06ld pid=%d tid=%d ",
	                  (long long)now.tv_sec, now.tv_nsec / 1000,
	                  (int)syscall(SYS_getpid), (int)syscall(SYS_gettid));
	if (offset < 0 || (size_t)offset >= sizeof(buffer))
		return;

	va_start(args, format);
	length = vsnprintf(buffer + offset, sizeof(buffer) - (size_t)offset,
	                   format, args);
	va_end(args);
	if (length < 0)
		return;

	length += offset;
	if ((size_t)length > sizeof(buffer))
		length = (int)sizeof(buffer);
	syscall(SYS_write, trace_fd, buffer, (size_t)length);
}

static FILE* shim_open_trace_file(void)
{
	const char* trace_file = getenv("MACHGATE_EXECVE_TRACE_FILE");

	if (!trace_file || !*trace_file)
		return NULL;
	return fopen(trace_file, "a");
}

/* Same issue for pthread_cond_t */
static inline void fixup_cond(pthread_cond_t *cond)
{
	long *sig = (long *)cond;
	if (*sig == DARWIN_PTHREAD_COND_SIG) {
		memset(cond, 0, sizeof(pthread_cond_t));
		real_pthread_cond_init(cond, NULL);
	}
}

struct pthread_attr_slot {
	const void* key;
	pthread_attr_t native;
	int used;
};

#define ATTR_SLOT_CHUNK_SLOTS 256
#define ATTR_SLOT_CHUNK_COUNT 128

struct pthread_attr_chunk {
	struct pthread_attr_slot slots[ATTR_SLOT_CHUNK_SLOTS];
};

static struct pthread_attr_chunk* pthread_attr_chunks[ATTR_SLOT_CHUNK_COUNT];
static volatile int pthread_attr_slot_lock;

static int real_pthread_attr_init_for_create(pthread_attr_t* attr)
{
	static int (*real_pthread_attr_init)(pthread_attr_t*) = NULL;

	if (!real_pthread_attr_init)
		real_pthread_attr_init = dlsym(RTLD_NEXT, "pthread_attr_init");
	if (!real_pthread_attr_init)
		return ENOSYS;
	return real_pthread_attr_init(attr);
}

static int real_pthread_attr_destroy_for_create(pthread_attr_t* attr)
{
	static int (*real_pthread_attr_destroy)(pthread_attr_t*) = NULL;

	if (!real_pthread_attr_destroy)
		real_pthread_attr_destroy = dlsym(RTLD_NEXT, "pthread_attr_destroy");
	if (!real_pthread_attr_destroy)
		return ENOSYS;
	return real_pthread_attr_destroy(attr);
}

static void lock_pthread_attr_slots(void)
{
	while (__sync_lock_test_and_set(&pthread_attr_slot_lock, 1))
		sched_yield();
}

static void unlock_pthread_attr_slots(void)
{
	__sync_lock_release(&pthread_attr_slot_lock);
}

static struct pthread_attr_slot* pthread_attr_slot_find(const void* key)
{
	for (int chunk_index = 0; chunk_index < ATTR_SLOT_CHUNK_COUNT; chunk_index++) {
		struct pthread_attr_chunk* chunk = pthread_attr_chunks[chunk_index];
		if (!chunk)
			continue;
		for (int slot_index = 0; slot_index < ATTR_SLOT_CHUNK_SLOTS; slot_index++) {
			struct pthread_attr_slot* slot = &chunk->slots[slot_index];
			if (slot->used && slot->key == key)
				return slot;
		}
	}
	return NULL;
}

static struct pthread_attr_slot* pthread_attr_slot_alloc(const void* key)
{
	struct pthread_attr_slot* slot = pthread_attr_slot_find(key);
	if (slot)
		return slot;

	for (int chunk_index = 0; chunk_index < ATTR_SLOT_CHUNK_COUNT; chunk_index++) {
		struct pthread_attr_chunk* chunk = pthread_attr_chunks[chunk_index];
		if (chunk) {
			for (int slot_index = 0; slot_index < ATTR_SLOT_CHUNK_SLOTS; slot_index++) {
				if (!chunk->slots[slot_index].used) {
					chunk->slots[slot_index].used = 1;
					chunk->slots[slot_index].key = key;
					return &chunk->slots[slot_index];
				}
			}
			continue;
		}
		chunk = calloc(1, sizeof(*chunk));
		if (!chunk)
			return NULL;
		pthread_attr_chunks[chunk_index] = chunk;
		chunk->slots[0].used = 1;
		chunk->slots[0].key = key;
		return &chunk->slots[0];
	}
	return NULL;
}

int pthread_attr_init(pthread_attr_t* attr)
{
	static int (*real_pthread_attr_init)(pthread_attr_t*) = NULL;
	struct pthread_attr_slot* slot;

	if (!real_pthread_attr_init)
		real_pthread_attr_init = dlsym(RTLD_NEXT, "pthread_attr_init");
	if (!real_pthread_attr_init)
		return ENOSYS;

	lock_pthread_attr_slots();
	slot = pthread_attr_slot_alloc(attr);
	unlock_pthread_attr_slots();
	if (!slot)
		return ENOMEM;

	int result = real_pthread_attr_init(&slot->native);
	if (result == 0)
		memset(attr, 0, sizeof(uint64_t));
	return result;
}

int pthread_attr_destroy(pthread_attr_t* attr)
{
	static int (*real_pthread_attr_destroy)(pthread_attr_t*) = NULL;

	if (!real_pthread_attr_destroy)
		real_pthread_attr_destroy = dlsym(RTLD_NEXT, "pthread_attr_destroy");
	if (!real_pthread_attr_destroy)
		return ENOSYS;

	lock_pthread_attr_slots();
	struct pthread_attr_slot* slot = pthread_attr_slot_find(attr);
	if (!slot) {
		unlock_pthread_attr_slots();
		return real_pthread_attr_destroy(attr);
	}

	int result = real_pthread_attr_destroy(&slot->native);
	slot->used = 0;
	slot->key = NULL;
	unlock_pthread_attr_slots();
	return result;
}

static pthread_attr_t* pthread_attr_target(pthread_attr_t* attr)
{
	struct pthread_attr_slot* slot;

	lock_pthread_attr_slots();
	slot = pthread_attr_slot_find(attr);
	unlock_pthread_attr_slots();
	return slot ? &slot->native : attr;
}

int pthread_attr_setstacksize(pthread_attr_t* attr, size_t stack_size)
{
	static int (*real_pthread_attr_setstacksize)(pthread_attr_t*, size_t) = NULL;

	if (!real_pthread_attr_setstacksize)
		real_pthread_attr_setstacksize = dlsym(RTLD_NEXT, "pthread_attr_setstacksize");
	if (!real_pthread_attr_setstacksize)
		return ENOSYS;
	return real_pthread_attr_setstacksize(pthread_attr_target(attr), stack_size);
}

int pthread_attr_getstacksize(const pthread_attr_t* attr, size_t* stack_size)
{
	static int (*real_pthread_attr_getstacksize)(const pthread_attr_t*, size_t*) = NULL;

	if (!real_pthread_attr_getstacksize)
		real_pthread_attr_getstacksize = dlsym(RTLD_NEXT, "pthread_attr_getstacksize");
	if (!real_pthread_attr_getstacksize)
		return ENOSYS;
	return real_pthread_attr_getstacksize(
		pthread_attr_target((pthread_attr_t*)attr), stack_size);
}

int pthread_attr_setdetachstate(pthread_attr_t* attr, int detach_state)
{
	static int (*real_pthread_attr_setdetachstate)(pthread_attr_t*, int) = NULL;
	int linux_state = detach_state;

	if (!real_pthread_attr_setdetachstate)
		real_pthread_attr_setdetachstate = dlsym(RTLD_NEXT, "pthread_attr_setdetachstate");
	if (!real_pthread_attr_setdetachstate)
		return ENOSYS;

	if (detach_state == 1)
		linux_state = PTHREAD_CREATE_JOINABLE;
	else if (detach_state == 2)
		linux_state = PTHREAD_CREATE_DETACHED;
	return real_pthread_attr_setdetachstate(pthread_attr_target(attr), linux_state);
}

int pthread_attr_getdetachstate(const pthread_attr_t* attr, int* detach_state)
{
	static int (*real_pthread_attr_getdetachstate)(const pthread_attr_t*, int*) = NULL;
	int result;
	int linux_state = 0;

	if (!real_pthread_attr_getdetachstate)
		real_pthread_attr_getdetachstate = dlsym(RTLD_NEXT, "pthread_attr_getdetachstate");
	if (!real_pthread_attr_getdetachstate)
		return ENOSYS;

	result = real_pthread_attr_getdetachstate(
		pthread_attr_target((pthread_attr_t*)attr), &linux_state);
	if (result == 0 && detach_state) {
		if (linux_state == PTHREAD_CREATE_DETACHED)
			*detach_state = 2;
		else
			*detach_state = 1;
	}
	return result;
}

int pthread_attr_setschedparam(pthread_attr_t* attr,
                               const struct sched_param* sched_param)
{
	static int (*real_pthread_attr_setschedparam)(pthread_attr_t*,
		const struct sched_param*) = NULL;
	struct sched_param linux_param;

	if (!real_pthread_attr_setschedparam)
		real_pthread_attr_setschedparam = dlsym(RTLD_NEXT, "pthread_attr_setschedparam");
	if (!real_pthread_attr_setschedparam || !sched_param)
		return EINVAL;

	memset(&linux_param, 0, sizeof(linux_param));
	linux_param.sched_priority = sched_param->sched_priority;
	return real_pthread_attr_setschedparam(pthread_attr_target(attr), &linux_param);
}

int pthread_attr_getschedparam(const pthread_attr_t* attr,
                               struct sched_param* sched_param)
{
	static int (*real_pthread_attr_getschedparam)(const pthread_attr_t*,
		struct sched_param*) = NULL;
	int result;

	if (!real_pthread_attr_getschedparam)
		real_pthread_attr_getschedparam = dlsym(RTLD_NEXT, "pthread_attr_getschedparam");
	if (!real_pthread_attr_getschedparam || !sched_param)
		return EINVAL;

	result = real_pthread_attr_getschedparam(
		pthread_attr_target((pthread_attr_t*)attr), sched_param);
	return result;
}

int pthread_attr_setschedpolicy(pthread_attr_t* attr, int policy)
{
	static int (*real_pthread_attr_setschedpolicy)(pthread_attr_t*, int) = NULL;
	int linux_policy;

	if (!real_pthread_attr_setschedpolicy)
		real_pthread_attr_setschedpolicy = dlsym(RTLD_NEXT, "pthread_attr_setschedpolicy");
	if (!real_pthread_attr_setschedpolicy)
		return ENOSYS;

	switch (policy) {
	case 1:
		linux_policy = SCHED_OTHER;
		break;
	case 2:
		linux_policy = SCHED_FIFO;
		break;
	case 3:
		linux_policy = SCHED_RR;
		break;
	default:
		return EINVAL;
	}
	return real_pthread_attr_setschedpolicy(pthread_attr_target(attr), linux_policy);
}

int pthread_attr_setstack(pthread_attr_t* attr, void* stack_base,
                          size_t stack_size)
{
	static int (*real_pthread_attr_setstack)(pthread_attr_t*, void*, size_t) = NULL;

	if (!real_pthread_attr_setstack)
		real_pthread_attr_setstack = dlsym(RTLD_NEXT, "pthread_attr_setstack");
	if (!real_pthread_attr_setstack)
		return ENOSYS;
	return real_pthread_attr_setstack(pthread_attr_target(attr), stack_base,
	                                   stack_size);
}

static struct shim_thread_start_context* alloc_thread_start_context(
	void* (*start_routine)(void*), void* arg)
{
	size_t map_size = 4096;
	struct shim_thread_start_context* context =
		(struct shim_thread_start_context*)syscall(
			SYS_mmap, NULL, map_size, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	if (context == MAP_FAILED)
		return NULL;
	context->start_routine = start_routine;
	context->arg = arg;
	context->map_size = map_size;
	return context;
}

static void free_thread_start_context(struct shim_thread_start_context* context)
{
	if (context)
		syscall(SYS_munmap, context, context->map_size);
}

static void* shim_pthread_start(void* raw_context)
{
	struct shim_thread_start_context* context =
		(struct shim_thread_start_context*)raw_context;
	void* (*start_routine)(void*) = context->start_routine;
	void* arg = context->arg;
	void* result;

	register_current_pthread_identity();
	free_thread_start_context(context);
	result = start_routine(arg);
	shim_run_tlv_term_funcs();
	shim_run_thread_tsd_destructors();
	return result;
}

int pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                   void* (*start_routine)(void*), void* arg)
{
	static int (*real_pthread_create)(pthread_t*, const pthread_attr_t*,
	                                  void* (*)(void*), void*) = NULL;
	struct shim_thread_start_context* context;
	pthread_attr_t native_attr;
	int result;

	if (!real_pthread_create)
		real_pthread_create = dlsym(RTLD_NEXT, "pthread_create");
	if (!real_pthread_create)
		return ENOSYS;
	context = alloc_thread_start_context(start_routine, arg);
	if (!context)
		return EAGAIN;

	lock_pthread_attr_slots();
	struct pthread_attr_slot* slot = pthread_attr_slot_find(attr);
	unlock_pthread_attr_slots();

	pthread_attr_t* native = slot ? &slot->native : NULL;
	if (!native && attr) {
		if (shim_trace_enabled())
			fprintf(stderr, "libsystem_shim: pthread_create unregistered attr=%p replaced with default\n",
			        attr);
		if (real_pthread_attr_init_for_create(&native_attr) == 0)
			native = &native_attr;
		else
			native = NULL;
	}

	if (shim_trace_enabled())
		fprintf(stderr, "libsystem_shim: pthread_create(start=%p arg=%p attr=%p native=%p)\n",
		        start_routine, arg, attr, (void*)native);
	result = real_pthread_create(thread, native, shim_pthread_start, context);
	if (result != 0) {
		if (shim_trace_enabled())
			fprintf(stderr, "libsystem_shim: pthread_create FAILED result=%d attr=%p slot=%p\n",
			        result, attr, (void*)slot);
		free_thread_start_context(context);
		return result;
	}
	if (native == &native_attr)
		real_pthread_attr_destroy_for_create(&native_attr);
	pthread_identity_for_thread(*thread);
	return 0;
}

int pthread_join(pthread_t thread, void** value_ptr)
{
	static int (*real_pthread_join)(pthread_t, void**) = NULL;

	if (!real_pthread_join)
		real_pthread_join = dlsym(RTLD_NEXT, "pthread_join");
	if (!real_pthread_join)
		return ENOSYS;
	int result = real_pthread_join(thread, value_ptr);
	if (result == 0)
		forget_pthread_identity(thread);
	return result;
}

int pthread_kill(pthread_t thread, int signum)
{
	static int (*real_pthread_kill)(pthread_t, int) = NULL;
	int linux_signal = darwin_signal_to_linux(signum);

	if (linux_signal <= 0 || signum == 16) {
		if (shim_trace_enabled())
			fprintf(stderr, "libsystem_shim: pthread_kill(%lu, %d->%d) ignored\n",
			        (unsigned long)thread, signum, linux_signal);
		return 0;
	}

	if (!real_pthread_kill)
		real_pthread_kill = dlsym(RTLD_NEXT, "pthread_kill");
	if (!real_pthread_kill)
		return ENOSYS;
	return real_pthread_kill(thread, linux_signal);
}

void* pthread_get_stackaddr_np(pthread_t thread)
{
	pthread_attr_t attr;
	void* stack_addr = NULL;
	size_t stack_size = 0;
	void** machgate_stack_top;

	machgate_stack_top = dlsym(RTLD_DEFAULT, "__machgate_main_stack_top");
	if (machgate_stack_top && *machgate_stack_top &&
	    machgate_main_pthread_set &&
	    pthread_equal(thread, pthread_self()) &&
	    pthread_equal(thread, machgate_main_pthread)) {
		if (shim_trace_enabled())
			fprintf(stderr, "libsystem_shim: pthread_get_stackaddr_np main -> %p\n", *machgate_stack_top);
		return *machgate_stack_top;
	}
	if (pthread_getattr_np(thread, &attr) != 0)
		return NULL;
	pthread_attr_getstack(&attr, &stack_addr, &stack_size);
	pthread_attr_destroy(&attr);
	if (shim_trace_enabled())
		fprintf(stderr, "libsystem_shim: pthread_get_stackaddr_np host -> %p\n", (char*)stack_addr + stack_size);
	return (char*)stack_addr + stack_size;
}

size_t pthread_get_stacksize_np(pthread_t thread)
{
	pthread_attr_t attr;
	void* stack_addr = NULL;
	size_t stack_size = 0;
	size_t* machgate_stack_size;

	machgate_stack_size = dlsym(RTLD_DEFAULT, "__machgate_main_stack_size");
	if (machgate_stack_size && *machgate_stack_size &&
	    machgate_main_pthread_set &&
	    pthread_equal(thread, pthread_self()) &&
	    pthread_equal(thread, machgate_main_pthread)) {
		if (shim_trace_enabled())
			fprintf(stderr, "libsystem_shim: pthread_get_stacksize_np main -> %zu\n", *machgate_stack_size);
		return *machgate_stack_size;
	}
	if (pthread_getattr_np(thread, &attr) != 0)
		return 0;
	pthread_attr_getstack(&attr, &stack_addr, &stack_size);
	pthread_attr_destroy(&attr);
	if (shim_trace_enabled())
		fprintf(stderr, "libsystem_shim: pthread_get_stacksize_np host -> %zu\n", stack_size);
	return stack_size;
}

void pthread_jit_write_protect_np(int enabled)
{
	(void)enabled;
}

int pthread_jit_write_protect_supported_np(void)
{
	return 0;
}

int pthread_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void))
{
	static int (*real_pthread_atfork)(void (*)(void), void (*)(void), void (*)(void));

	if (!real_pthread_atfork)
		real_pthread_atfork = dlsym(RTLD_NEXT, "pthread_atfork");
	if (!real_pthread_atfork)
		return 0;
	return real_pthread_atfork(prepare, parent, child);
}

#define DARWIN_SC_PAGESIZE 29
#define DARWIN_SC_PAGE_SIZE 29
#define DARWIN_SC_NPROCESSORS_CONF 57
#define DARWIN_SC_NPROCESSORS_ONLN 58

long sysconf(int name)
{
	static long (*real_sysconf)(int) = NULL;

	if (!real_sysconf)
		real_sysconf = dlsym(RTLD_NEXT, "sysconf");

	switch (name) {
	case DARWIN_SC_PAGESIZE:
		return real_sysconf(_SC_PAGESIZE);
	case DARWIN_SC_NPROCESSORS_CONF:
	case DARWIN_SC_NPROCESSORS_ONLN:
		return shim_hw_ncpu();
	default:
		return real_sysconf(name);
	}
}

int pthread_cond_timedwait_relative_np(pthread_cond_t* cond,
                                       pthread_mutex_t* mutex,
                                       const struct timespec* relative)
{
	if (!relative)
		return pthread_cond_wait(cond, mutex);

	struct timespec deadline;
	clock_gettime(CLOCK_REALTIME, &deadline);
	deadline.tv_sec += relative->tv_sec;
	deadline.tv_nsec += relative->tv_nsec;
	if (deadline.tv_nsec >= 1000000000L) {
		deadline.tv_sec += deadline.tv_nsec / 1000000000L;
		deadline.tv_nsec %= 1000000000L;
	}
	return pthread_cond_timedwait(cond, mutex, &deadline);
}

static void remember_kqueue_fd(int fd, int canonical_fd);

int kqueue(void)
{
	int result = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
	if (result >= 0)
		remember_kqueue_fd(result, result);
	shim_fd_trace_log("kqueue caller=%p result=%d errno=%d\n",
	                  SHIM_CALLER_RETURN_ADDRESS(), result,
	                  result < 0 ? errno : 0);
	if (shim_trace_enabled())
		fprintf(stderr, "libsystem_shim: kqueue() -> %d errno=%d\n",
		        result, result < 0 ? errno : 0);
	return result;
}

#pragma pack(4)
struct darwin_kevent_placeholder {
	uint64_t ident;
	int16_t filter;
	uint16_t flags;
	uint32_t fflags;
	int64_t data;
	void* udata;
};
#pragma pack()

#define DARWIN_EVFILT_READ (-1)
#define DARWIN_EVFILT_WRITE (-2)
#define DARWIN_EVFILT_USER (-10)
#define DARWIN_EV_DELETE 0x0002
#define DARWIN_EV_DISABLE 0x0008
#define DARWIN_EV_CLEAR 0x0020
#define DARWIN_EV_RECEIPT 0x0040
#define DARWIN_EV_ERROR 0x4000
#define DARWIN_EV_EOF 0x8000
#define DARWIN_NOTE_TRIGGER 0x01000000
#define KQUEUE_REGISTRATION_COUNT 1024
#define KQUEUE_ALIAS_COUNT 1024

struct shim_kqueue_registration {
	int used;
	int kq;
	uint64_t ident;
	int16_t filter;
	uint16_t flags;
	uint32_t fflags;
	int64_t data;
	void* udata;
	int triggered;
	int ready;
};

struct shim_kqueue_poll_registration {
	struct shim_kqueue_registration* registration;
	struct shim_kqueue_registration snapshot;
};

static pthread_mutex_t kqueue_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct shim_kqueue_registration kqueue_registrations[KQUEUE_REGISTRATION_COUNT];
static int kqueue_aliases[KQUEUE_ALIAS_COUNT];

__attribute__((constructor))
static void init_kqueue_aliases(void)
{
	for (int i = 0; i < KQUEUE_ALIAS_COUNT; i++)
		kqueue_aliases[i] = -1;
}

static int resolve_kqueue_fd_unlocked(int kq)
{
	if (kq >= 0 && kq < KQUEUE_ALIAS_COUNT && kqueue_aliases[kq] >= 0)
		return kqueue_aliases[kq];
	return kq;
}

static void remember_kqueue_fd_unlocked(int fd, int canonical_fd)
{
	if (fd >= 0 && fd < KQUEUE_ALIAS_COUNT)
		kqueue_aliases[fd] = canonical_fd;
}

static void remember_kqueue_fd(int fd, int canonical_fd)
{
	pthread_mutex_lock(&kqueue_mutex);
	remember_kqueue_fd_unlocked(fd, canonical_fd);
	pthread_mutex_unlock(&kqueue_mutex);
}

static void remember_kqueue_dup(int from_fd, int to_fd)
{
	pthread_mutex_lock(&kqueue_mutex);
	int canonical_fd = resolve_kqueue_fd_unlocked(from_fd);
	if (canonical_fd != from_fd ||
	    (from_fd >= 0 && from_fd < KQUEUE_ALIAS_COUNT &&
	     kqueue_aliases[from_fd] >= 0))
		remember_kqueue_fd_unlocked(to_fd, canonical_fd);
	pthread_mutex_unlock(&kqueue_mutex);
}

static void forget_kqueue_fd_unlocked(int fd)
{
	int canonical_fd = resolve_kqueue_fd_unlocked(fd);
	int closing_kqueue = fd >= 0 && fd < KQUEUE_ALIAS_COUNT &&
	    kqueue_aliases[fd] >= 0;

	for (int i = 0; i < KQUEUE_REGISTRATION_COUNT; i++) {
		struct shim_kqueue_registration* registration = &kqueue_registrations[i];
		if (!registration->used)
			continue;
		if (closing_kqueue && registration->kq == canonical_fd) {
			memset(registration, 0, sizeof(*registration));
			continue;
		}
		if ((registration->filter == DARWIN_EVFILT_READ ||
		     registration->filter == DARWIN_EVFILT_WRITE) &&
		    (int)registration->ident == fd)
			memset(registration, 0, sizeof(*registration));
	}

	if (fd >= 0 && fd < KQUEUE_ALIAS_COUNT)
		kqueue_aliases[fd] = -1;
	if (closing_kqueue && fd == canonical_fd) {
		for (int i = 0; i < KQUEUE_ALIAS_COUNT; i++) {
			if (kqueue_aliases[i] == canonical_fd)
				kqueue_aliases[i] = -1;
		}
	}
}

static void forget_kqueue_fd(int fd)
{
	pthread_mutex_lock(&kqueue_mutex);
	forget_kqueue_fd_unlocked(fd);
	pthread_mutex_unlock(&kqueue_mutex);
}

static struct shim_kqueue_registration* find_kqueue_registration(int kq,
                                                                 uint64_t ident,
                                                                 int16_t filter)
{
	for (int i = 0; i < KQUEUE_REGISTRATION_COUNT; i++) {
		struct shim_kqueue_registration* registration = &kqueue_registrations[i];
		if (registration->used &&
		    registration->kq == resolve_kqueue_fd_unlocked(kq) &&
		    registration->ident == ident && registration->filter == filter)
			return registration;
	}
	return NULL;
}

static struct shim_kqueue_registration* alloc_kqueue_registration(void)
{
	for (int i = 0; i < KQUEUE_REGISTRATION_COUNT; i++) {
		if (!kqueue_registrations[i].used)
			return &kqueue_registrations[i];
	}
	return NULL;
}

static void ensure_kqueue_poll_fd_nonblocking(int fd)
{
	int flags;

	if (fd < 0)
		return;

	flags = (int)syscall(SYS_fcntl, fd, F_GETFL, 0);
	if (flags < 0 || (flags & O_NONBLOCK))
		return;

	syscall(SYS_fcntl, fd, F_SETFL, (unsigned long)(flags | O_NONBLOCK));
}

static int update_kqueue_registration(int kq,
                                      const struct darwin_kevent_placeholder* change)
{
	struct shim_kqueue_registration* registration =
		find_kqueue_registration(kq, change->ident, change->filter);

	if (shim_trace_enabled())
		fprintf(stderr,
		        "libsystem_shim: kevent change kq=%d ident=%llu filter=%d flags=%#x fflags=%#x data=%lld udata=%p\n",
		        kq, (unsigned long long)change->ident, change->filter,
		        change->flags, change->fflags, (long long)change->data,
		        change->udata);
	shim_fd_trace_log("kevent_change caller=%p kq=%d ident=%llu filter=%d flags=%#x fflags=%#x data=%lld udata=%p\n",
	                  SHIM_CALLER_RETURN_ADDRESS(), kq,
	                  (unsigned long long)change->ident, change->filter,
	                  change->flags, change->fflags, (long long)change->data,
	                  change->udata);

	if (change->flags & DARWIN_EV_DELETE) {
		if (registration)
			memset(registration, 0, sizeof(*registration));
		return 0;
	}

	if (change->filter != DARWIN_EVFILT_READ &&
	    change->filter != DARWIN_EVFILT_WRITE &&
	    change->filter != DARWIN_EVFILT_USER)
		return 0;

	if (change->filter == DARWIN_EVFILT_READ ||
	    change->filter == DARWIN_EVFILT_WRITE)
		ensure_kqueue_poll_fd_nonblocking((int)change->ident);

	if (!registration) {
		registration = alloc_kqueue_registration();
		if (!registration)
			return -1;
	}

	registration->used = 1;
	registration->kq = resolve_kqueue_fd_unlocked(kq);
	registration->ident = change->ident;
	registration->filter = change->filter;
	registration->flags = change->flags;
	registration->fflags = change->fflags;
	registration->data = change->data;
	registration->udata = change->udata;
	registration->ready = 0;
	if (change->fflags & DARWIN_NOTE_TRIGGER)
		registration->triggered = 1;
	if (change->filter == DARWIN_EVFILT_USER &&
	    (change->fflags & DARWIN_NOTE_TRIGGER)) {
		uint64_t value = 1;
		ssize_t write_result = write(registration->kq, &value, sizeof(value));
		(void)write_result;
	}

	return 0;
}

static int apply_kqueue_changes(int kq,
                                const struct darwin_kevent_placeholder* changelist,
                                int nchanges)
{
	for (int i = 0; i < nchanges; i++) {
		if (update_kqueue_registration(kq, &changelist[i]) < 0) {
			errno = ENOMEM;
			return -1;
		}
	}
	return 0;
}

static int emit_kqueue_receipts(const struct darwin_kevent_placeholder* changelist,
                                int nchanges,
                                struct darwin_kevent_placeholder* eventlist,
                                int nevents)
{
	int result = 0;

	for (int index = 0; index < nchanges && result < nevents; index++) {
		const struct darwin_kevent_placeholder* change = &changelist[index];
		if (!(change->flags & DARWIN_EV_RECEIPT))
			continue;

		eventlist[result].ident = change->ident;
		eventlist[result].filter = change->filter;
		eventlist[result].flags = change->flags | DARWIN_EV_ERROR;
		eventlist[result].fflags = change->fflags;
		eventlist[result].data = 0;
		eventlist[result].udata = change->udata;
		result++;
	}

	return result;
}

static int kevent_timeout_ms(const struct timespec* timeout)
{
	if (!timeout)
		return -1;
	if (timeout->tv_sec < 0 || timeout->tv_nsec < 0 ||
	    timeout->tv_nsec >= 1000000000L)
		return -1;
	if (timeout->tv_sec > INT32_MAX / 1000)
		return INT32_MAX;
	int result = (int)(timeout->tv_sec * 1000);
	result += (int)((timeout->tv_nsec + 999999L) / 1000000L);
	return result;
}

static short kqueue_poll_events(int16_t filter)
{
	if (filter == DARWIN_EVFILT_READ || filter == DARWIN_EVFILT_USER)
		return POLLIN;
	if (filter == DARWIN_EVFILT_WRITE)
		return POLLOUT;
	return 0;
}

static int collect_kqueue_pollfds(int kq, struct pollfd* pollfds,
                                  struct shim_kqueue_poll_registration* registrations,
                                  int max_events)
{
	int count = 0;

	for (int i = 0; i < KQUEUE_REGISTRATION_COUNT && count < max_events; i++) {
		struct shim_kqueue_registration* registration = &kqueue_registrations[i];
		if (!registration->used ||
		    registration->kq != resolve_kqueue_fd_unlocked(kq))
			continue;
		if (registration->flags & DARWIN_EV_DISABLE)
			continue;
		if (registration->filter == DARWIN_EVFILT_USER) {
			pollfds[count].fd = kq;
			pollfds[count].events = POLLIN;
		} else {
			pollfds[count].fd = (int)registration->ident;
		pollfds[count].events = kqueue_poll_events(registration->filter);
		}
		pollfds[count].revents = 0;
		registrations[count].registration = registration;
		registrations[count].snapshot = *registration;
		count++;
	}

	return count;
}

static int kqueue_registration_matches_snapshot(
	const struct shim_kqueue_registration* registration,
	const struct shim_kqueue_registration* snapshot)
{
	return registration && registration->used &&
	    registration->kq == snapshot->kq &&
	    registration->ident == snapshot->ident &&
	    registration->filter == snapshot->filter;
}

static int64_t kqueue_event_data(const struct shim_kqueue_registration* registration,
                                 short revents)
{
	if (registration->filter == DARWIN_EVFILT_READ) {
		int available = 0;
		if (syscall(SYS_ioctl, (int)registration->ident, FIONREAD,
		            &available) == 0 && available > 0)
			return available;
		if (revents & POLLIN) {
			int listening = 0;
			socklen_t listening_len = sizeof(listening);
			if (syscall(SYS_getsockopt, (int)registration->ident,
			            SOL_SOCKET, SO_ACCEPTCONN, &listening,
			            &listening_len) == 0 && listening)
				return 1;
		}
		return 0;
	}
	if (registration->filter == DARWIN_EVFILT_WRITE) {
		if (revents & (POLLERR | POLLHUP | POLLNVAL))
			return 0;
		return 1;
	}
	return 0;
}

static int emit_kqueue_events(struct pollfd* pollfds,
                              struct shim_kqueue_poll_registration* registrations,
                              int pollfd_count,
                              struct darwin_kevent_placeholder* eventlist,
                              int nevents)
{
	int result = 0;

	for (int i = 0; i < pollfd_count && result < nevents; i++) {
		struct shim_kqueue_registration* live_registration =
			registrations[i].registration;
		struct shim_kqueue_registration* registration =
			&registrations[i].snapshot;
		int live_matches = kqueue_registration_matches_snapshot(
			live_registration, registration);
		short revents = pollfds[i].revents;
		int ready = revents != 0;

		if (!live_matches ||
		    live_registration->udata != registration->udata) {
			if (registration->filter == DARWIN_EVFILT_USER) {
				uint64_t value;
				while (read(registration->kq, &value,
				            sizeof(value)) > 0) {
				}
			}
			continue;
		}
		if (!ready && registration->filter != DARWIN_EVFILT_USER) {
			if (live_matches)
				live_registration->ready = 0;
			continue;
		}
		if (!ready && !registration->triggered)
			continue;
		if ((registration->flags & DARWIN_EV_CLEAR) &&
		    registration->filter == DARWIN_EVFILT_WRITE) {
			if (live_matches && live_registration->ready)
				continue;
			if (live_matches)
				live_registration->ready = 1;
		}

		eventlist[result].ident = registration->ident;
		eventlist[result].filter = registration->filter;
		eventlist[result].flags = registration->flags;
		eventlist[result].fflags = registration->fflags;
		eventlist[result].data = kqueue_event_data(registration, revents);
		eventlist[result].udata = registration->udata;
		if (revents & (POLLHUP | POLLERR | POLLNVAL))
			eventlist[result].flags |= DARWIN_EV_EOF;

		if (shim_trace_enabled())
			fprintf(stderr,
			        "libsystem_shim: kevent event ident=%llu filter=%d flags=%#x fflags=%#x data=%lld udata=%p revents=%#x\n",
			        (unsigned long long)eventlist[result].ident,
			        eventlist[result].filter, eventlist[result].flags,
			        eventlist[result].fflags,
			        (long long)eventlist[result].data,
			        eventlist[result].udata, revents);
		shim_fd_trace_log("kevent_event caller=%p ident=%llu filter=%d flags=%#x fflags=%#x data=%lld udata=%p revents=%#x\n",
		                  SHIM_CALLER_RETURN_ADDRESS(),
		                  (unsigned long long)eventlist[result].ident,
		                  eventlist[result].filter, eventlist[result].flags,
		                  eventlist[result].fflags,
		                  (long long)eventlist[result].data,
		                  eventlist[result].udata, revents);

		if (registration->filter == DARWIN_EVFILT_USER) {
			uint64_t value;
			while (read(registration->kq, &value, sizeof(value)) > 0) {
			}
			if (live_matches)
				live_registration->triggered = 0;
		}

		result++;
	}

	return result;
}

static void sleep_for_kevent_timeout(const struct timespec* timeout)
{
	if (!timeout)
		return;
	if (timeout->tv_sec == 0 && timeout->tv_nsec == 0)
		return;
	if (timeout->tv_sec < 0 || timeout->tv_nsec < 0 ||
	    timeout->tv_nsec >= 1000000000L)
		return;

	struct timespec sleep_time = *timeout;
	if (sleep_time.tv_sec > 0 || sleep_time.tv_nsec > 1000000L) {
		sleep_time.tv_sec = 0;
		sleep_time.tv_nsec = 1000000L;
	}
	syscall(SYS_nanosleep, &sleep_time, NULL);
}

int kevent(int kq, const struct darwin_kevent_placeholder* changelist,
           int nchanges, struct darwin_kevent_placeholder* eventlist,
           int nevents, const struct timespec* timeout)
{
	int result = 0;

	if (kq < 0) {
		errno = EBADF;
		return -1;
	}
	if (nevents < 0) {
		errno = EINVAL;
		return -1;
	}

	pthread_mutex_lock(&kqueue_mutex);
	if (changelist && nchanges > 0)
		result = apply_kqueue_changes(kq, changelist, nchanges);
	pthread_mutex_unlock(&kqueue_mutex);
	if (result < 0)
		return result;

	if (changelist && nchanges > 0) {
		int receipts = 0;
		if (eventlist && nevents > 0)
			receipts = emit_kqueue_receipts(changelist, nchanges,
			                                eventlist, nevents);
		if (shim_trace_enabled())
			fprintf(stderr, "libsystem_shim: kevent(kq=%d nchanges=%d nevents=%d receipts=%d) applied\n",
			        kq, nchanges, nevents, receipts);
		shim_fd_trace_log("kevent caller=%p kq=%d nchanges=%d nevents=%d receipts=%d result=%d errno=0\n",
		                  SHIM_CALLER_RETURN_ADDRESS(), kq, nchanges,
		                  nevents, receipts, receipts);
		/*
		 * Real kqueue applies the changes AND waits for events when
		 * nevents > 0. Returning here made every register+wait caller
		 * (curl connect loops, dispatch sources) see zero events
		 * immediately: connect-completion was never observed, requests
		 * were aborted, and each such test burned its full timeout.
		 * Fall through to the poll path below; the receipt entries
		 * occupy the leading eventlist slots, and real events follow.
		 */
		if (!(eventlist && nevents > 0))
			return receipts;
		if (receipts >= nevents)
			return receipts;
		nevents -= receipts;
		eventlist += receipts;
	}

	if (eventlist && nevents > 0) {
		struct pollfd pollfds[64];
		struct shim_kqueue_poll_registration registrations[64];
		int max_events = nevents < 64 ? nevents : 64;
		int pollfd_count;
		int poll_result;

		pthread_mutex_lock(&kqueue_mutex);
		pollfd_count = collect_kqueue_pollfds(kq, pollfds, registrations,
		                                      max_events);
		pthread_mutex_unlock(&kqueue_mutex);

		if (pollfd_count == 0) {
			sleep_for_kevent_timeout(timeout);
			result = 0;
		} else {
			poll_result = poll(pollfds, (nfds_t)pollfd_count,
			                   kevent_timeout_ms(timeout));
			if (poll_result < 0)
				result = -1;
			else {
				pthread_mutex_lock(&kqueue_mutex);
				result = emit_kqueue_events(pollfds, registrations,
				                            pollfd_count, eventlist,
				                            nevents);
				pthread_mutex_unlock(&kqueue_mutex);
			}
		}
	}
	if (shim_trace_enabled())
		fprintf(stderr, "libsystem_shim: kevent(kq=%d nchanges=%d nevents=%d timeout=%p timeout_value=%lld.%09ld) -> %d errno=%d\n",
		        kq, nchanges, nevents, timeout,
		        timeout ? (long long)timeout->tv_sec : -1,
		        timeout ? timeout->tv_nsec : -1L,
		        result, result < 0 ? errno : 0);
	shim_fd_trace_log("kevent caller=%p kq=%d nchanges=%d nevents=%d timeout=%p timeout_value=%lld.%09ld result=%d errno=%d\n",
	                  SHIM_CALLER_RETURN_ADDRESS(), kq, nchanges, nevents,
	                  timeout, timeout ? (long long)timeout->tv_sec : -1,
	                  timeout ? timeout->tv_nsec : -1L, result,
	                  result < 0 ? errno : 0);
	return result;
}

int kevent64(int kq, const struct darwin_kevent_placeholder* changelist,
             int nchanges, struct darwin_kevent_placeholder* eventlist,
             int nevents, uint32_t flags, const struct timespec* timeout)
{
	(void)flags;
	return kevent(kq, changelist, nchanges, eventlist, nevents, timeout);
}

int notify_is_valid_token(int token)
{
	(void)token;
	return 0;
}

int notify_cancel(int token)
{
	(void)token;
	return 0;
}

int notify_register_file_descriptor(const char* name, int* notify_fd,
                                    int flags, int* token)
{
	(void)name;
	(void)flags;

	if (!notify_fd || !token)
		return EINVAL;

	int fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
	if (fd < 0)
		return errno;

	*notify_fd = fd;
	*token = fd;
	return 0;
}

void* xpc_date_create_from_current(void)
{
	return NULL;
}

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
	fixup_cond(cond);
	fixup_mutex(mutex);
	return real_pthread_cond_wait(cond, mutex);
}

int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
                           const struct timespec *abstime)
{
	int result;

	fixup_cond(cond);
	fixup_mutex(mutex);
	result = real_pthread_cond_timedwait(cond, mutex, abstime);
	return shim_errno_from_linux(result);
}

int pthread_cond_signal(pthread_cond_t *cond)
{
	fixup_cond(cond);
	return real_pthread_cond_signal(cond);
}

int pthread_cond_broadcast(pthread_cond_t *cond)
{
	fixup_cond(cond);
	return real_pthread_cond_broadcast(cond);
}

int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr)
{
	memset(cond, 0, sizeof(pthread_cond_t));
	return real_pthread_cond_init(cond, attr);
}

int pthread_cond_destroy(pthread_cond_t *cond)
{
	return real_pthread_cond_destroy(cond);
}

/* pthread_rwlock_t also has macOS signature: 0x2DA8B3B4 */
#define DARWIN_PTHREAD_RWLOCK_SIG 0x2DA8B3B4L

static inline void fixup_rwlock(pthread_rwlock_t *rwlock)
{
	long *sig = (long *)rwlock;
	if (*sig == DARWIN_PTHREAD_RWLOCK_SIG) {
		memset(rwlock, 0, sizeof(pthread_rwlock_t));
		pthread_rwlock_init(rwlock, NULL);
	}
}

int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock)
{
	fixup_rwlock(rwlock);
	return real_pthread_rwlock_rdlock(rwlock);
}

int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock)
{
	fixup_rwlock(rwlock);
	return real_pthread_rwlock_wrlock(rwlock);
}

int pthread_rwlock_unlock(pthread_rwlock_t *rwlock)
{
	return real_pthread_rwlock_unlock(rwlock);
}

/* ===== pthread Apple extensions ===== */

/* pthread_mach_thread_np — returns the "Mach thread port" for a pthread.
 * We return the Linux TID as a reasonable approximation. */
uint32_t pthread_mach_thread_np(pthread_t thread)
{
	return (uint32_t)pthread_identity_for_thread(thread);
}

/* pthread_threadid_np — get a unique 64-bit thread ID */
int pthread_threadid_np(pthread_t thread, uint64_t *thread_id)
{
	if (!thread_id) return EINVAL;
	*thread_id = pthread_identity_for_thread(thread);
	if (shim_trace_enabled())
		fprintf(stderr, "libsystem_shim: pthread_threadid_np -> %llu\n",
		        (unsigned long long)*thread_id);
	return 0;
}

int pthread_set_qos_class_self_np(int qos_class, int relative_priority)
{
	(void)qos_class;
	(void)relative_priority;
	return 0;
}

int pthread_main_np(void)
{
	return syscall(SYS_gettid) == getpid();
}

int pthread_cpu_number_np(unsigned int* cpu_number)
{
	int result;

	if (!cpu_number)
		return EINVAL;

	result = sched_getcpu();
	*cpu_number = result >= 0 ? (unsigned int)result : 0;
	return 0;
}

int pthread_mutexattr_setpolicy_np(pthread_mutexattr_t* attr, int policy)
{
	(void)attr;
	(void)policy;
	return 0;
}

int pthread_self_is_exiting_np(void)
{
	return 0;
}

int pthread_setname_np(pthread_t thread, const char *name)
{
	static int (*real_pthread_setname_np)(pthread_t, const char*) = NULL;
	char truncated_name[16];
	const char* darwin_name = (const char*)thread;

	(void)name;

	if (!real_pthread_setname_np)
		real_pthread_setname_np = dlsym(RTLD_NEXT, "pthread_setname_np");
	if (!real_pthread_setname_np)
		return 0;
	if (!darwin_name)
		return EINVAL;

	snprintf(truncated_name, sizeof(truncated_name), "%s", darwin_name);
	if (shim_trace_enabled())
		fprintf(stderr, "libsystem_shim: pthread_setname_np('%s')\n",
		        truncated_name);
	return real_pthread_setname_np(pthread_self(), truncated_name);
}

#define DARWIN_SS_ONSTACK 1
#define DARWIN_SS_DISABLE 4
#define DARWIN_SA_ONSTACK   0x0001
#define DARWIN_SA_RESTART   0x0002
#define DARWIN_SA_RESETHAND 0x0004
#define DARWIN_SA_NOCLDSTOP 0x0008
#define DARWIN_SA_NODEFER   0x0010
#define DARWIN_SA_NOCLDWAIT 0x0020
#define DARWIN_SA_SIGINFO   0x0040
#define DARWIN_SA_USERSPACE_MASK \
	(DARWIN_SA_ONSTACK | DARWIN_SA_RESTART | DARWIN_SA_RESETHAND | \
	 DARWIN_SA_NOCLDSTOP | DARWIN_SA_NODEFER | DARWIN_SA_NOCLDWAIT | \
	 DARWIN_SA_SIGINFO)

#ifdef SA_RESTORER
#define LINUX_SA_RESTORER SA_RESTORER
#else
#define LINUX_SA_RESTORER 0x04000000
#endif

static int darwin_sigaltstack_flags_to_linux(uint32_t darwin_flags,
                                             int* linux_flags)
{
	*linux_flags = 0;
	if (darwin_flags & DARWIN_SS_ONSTACK)
		*linux_flags |= SS_ONSTACK;
	if (darwin_flags & DARWIN_SS_DISABLE)
		*linux_flags |= SS_DISABLE;
	if (darwin_flags & ~(DARWIN_SS_ONSTACK | DARWIN_SS_DISABLE))
		return 0;
	return 1;
}

static uint32_t linux_sigaltstack_flags_to_darwin(int linux_flags)
{
	uint32_t result = 0;
	if (linux_flags & SS_ONSTACK)
		result |= DARWIN_SS_ONSTACK;
	if (linux_flags & SS_DISABLE)
		result |= DARWIN_SS_DISABLE;
	return result;
}

static int darwin_sigaction_flags_to_linux(uint32_t darwin_flags,
                                           int* linux_flags)
{
	int result = 0;

	if (darwin_flags & ~DARWIN_SA_USERSPACE_MASK)
		return 0;
	if (darwin_flags & DARWIN_SA_ONSTACK)
		result |= SA_ONSTACK;
	if (darwin_flags & DARWIN_SA_RESTART)
		result |= SA_RESTART;
	if (darwin_flags & DARWIN_SA_RESETHAND)
		result |= SA_RESETHAND;
	if (darwin_flags & DARWIN_SA_NOCLDSTOP)
		result |= SA_NOCLDSTOP;
	if (darwin_flags & DARWIN_SA_NODEFER)
		result |= SA_NODEFER;
	if (darwin_flags & DARWIN_SA_NOCLDWAIT)
		result |= SA_NOCLDWAIT;
	if (darwin_flags & DARWIN_SA_SIGINFO)
		result |= SA_SIGINFO;
	*linux_flags = result;
	return 1;
}

static uint32_t linux_sigaction_flags_to_darwin(int linux_flags)
{
	uint32_t result = 0;
	int supported_flags = SA_ONSTACK | SA_RESTART | SA_RESETHAND |
	                      SA_NOCLDSTOP | SA_NODEFER | SA_NOCLDWAIT |
	                      SA_SIGINFO | LINUX_SA_RESTORER;

	linux_flags &= supported_flags;
	if (linux_flags & SA_ONSTACK)
		result |= DARWIN_SA_ONSTACK;
	if (linux_flags & SA_RESTART)
		result |= DARWIN_SA_RESTART;
	if (linux_flags & SA_RESETHAND)
		result |= DARWIN_SA_RESETHAND;
	if (linux_flags & SA_NOCLDSTOP)
		result |= DARWIN_SA_NOCLDSTOP;
	if (linux_flags & SA_NODEFER)
		result |= DARWIN_SA_NODEFER;
	if (linux_flags & SA_NOCLDWAIT)
		result |= DARWIN_SA_NOCLDWAIT;
	if (linux_flags & SA_SIGINFO)
		result |= DARWIN_SA_SIGINFO;
	return result;
}

static int darwin_signal_to_linux(int darwin_signal)
{
	switch (darwin_signal) {
	case 1: return SIGHUP;
	case 2: return SIGINT;
	case 3: return SIGQUIT;
	case 4: return SIGILL;
	case 5: return SIGTRAP;
	case 6: return SIGABRT;
	case 7: return 7;
	case 8: return SIGFPE;
	case 9: return SIGKILL;
	case 10: return SIGBUS;
	case 11: return SIGSEGV;
	case 12: return SIGSYS;
	case 13: return SIGPIPE;
	case 14: return SIGALRM;
	case 15: return SIGTERM;
	case 16: return SIGURG;
	case 17: return SIGSTOP;
	case 18: return SIGTSTP;
	case 19: return SIGCONT;
	case 20: return SIGCHLD;
	case 21: return SIGTTIN;
	case 22: return SIGTTOU;
	case 23: return SIGIO;
	case 24: return SIGXCPU;
	case 25: return SIGXFSZ;
	case 26: return SIGVTALRM;
	case 27: return SIGPROF;
	case 28: return SIGWINCH;
	case 30: return SIGUSR1;
	case 31: return SIGUSR2;
	default: return darwin_signal;
	}
}

static int linux_signal_to_darwin(int linux_signal)
{
	switch (linux_signal) {
	case SIGHUP: return 1;
	case SIGINT: return 2;
	case SIGQUIT: return 3;
	case SIGILL: return 4;
	case SIGTRAP: return 5;
	case SIGABRT: return 6;
	case SIGBUS: return 10;
	case SIGSEGV: return 11;
	case SIGSYS: return 12;
	case SIGPIPE: return 13;
	case SIGALRM: return 14;
	case SIGTERM: return 15;
	case SIGURG: return 16;
	case SIGSTOP: return 17;
	case SIGTSTP: return 18;
	case SIGCONT: return 19;
	case SIGCHLD: return 20;
	case SIGTTIN: return 21;
	case SIGTTOU: return 22;
	case SIGIO: return 23;
	case SIGXCPU: return 24;
	case SIGXFSZ: return 25;
	case SIGVTALRM: return 26;
	case SIGPROF: return 27;
	case SIGWINCH: return 28;
	case SIGUSR1: return 30;
	case SIGUSR2: return 31;
	default: return linux_signal;
	}
}

static int darwin_wait_options_to_linux(int darwin_options, int* linux_options)
{
	int result = 0;
	int supported = WNOHANG | WUNTRACED;

#ifdef WCONTINUED
	supported |= 0x10;
#endif

	if (darwin_options & ~supported)
		return 0;

	if (darwin_options & WNOHANG)
		result |= WNOHANG;
	if (darwin_options & WUNTRACED)
		result |= WUNTRACED;
#ifdef WCONTINUED
	if (darwin_options & 0x10)
		result |= WCONTINUED;
#endif

	*linux_options = result;
	return 1;
}

static int linux_wait_status_to_darwin(int linux_status)
{
	int darwin_signal;

	if (WIFSIGNALED(linux_status)) {
		darwin_signal = linux_signal_to_darwin(WTERMSIG(linux_status));
		return (linux_status & ~0x7f) | (darwin_signal & 0x7f);
	}
	if (WIFSTOPPED(linux_status)) {
		darwin_signal = linux_signal_to_darwin(WSTOPSIG(linux_status));
		return (linux_status & ~0xff00) | ((darwin_signal & 0xff) << 8);
	}
	return linux_status;
}

static void linux_rusage_to_darwin(const struct rusage* linux_usage,
                                   struct darwin_rusage* darwin_usage)
{
	darwin_usage->ru_utime.tv_sec = linux_usage->ru_utime.tv_sec;
	darwin_usage->ru_utime.tv_usec = (int32_t)linux_usage->ru_utime.tv_usec;
	darwin_usage->ru_utime.pad = 0;
	darwin_usage->ru_stime.tv_sec = linux_usage->ru_stime.tv_sec;
	darwin_usage->ru_stime.tv_usec = (int32_t)linux_usage->ru_stime.tv_usec;
	darwin_usage->ru_stime.pad = 0;
	darwin_usage->ru_maxrss = linux_usage->ru_maxrss;
	darwin_usage->ru_ixrss = linux_usage->ru_ixrss;
	darwin_usage->ru_idrss = linux_usage->ru_idrss;
	darwin_usage->ru_isrss = linux_usage->ru_isrss;
	darwin_usage->ru_minflt = linux_usage->ru_minflt;
	darwin_usage->ru_majflt = linux_usage->ru_majflt;
	darwin_usage->ru_nswap = linux_usage->ru_nswap;
	darwin_usage->ru_inblock = linux_usage->ru_inblock;
	darwin_usage->ru_oublock = linux_usage->ru_oublock;
	darwin_usage->ru_msgsnd = linux_usage->ru_msgsnd;
	darwin_usage->ru_msgrcv = linux_usage->ru_msgrcv;
	darwin_usage->ru_nsignals = linux_usage->ru_nsignals;
	darwin_usage->ru_nvcsw = linux_usage->ru_nvcsw;
	darwin_usage->ru_nivcsw = linux_usage->ru_nivcsw;
}

pid_t wait4(pid_t pid, int* status, int options, struct rusage* usage)
{
	int linux_options;
	int linux_status = 0;
	struct rusage linux_usage;
	struct rusage* linux_usage_ptr = usage ? &linux_usage : NULL;
	struct darwin_rusage* darwin_usage = (struct darwin_rusage*)usage;
	unsigned char status_before[sizeof(uint32_t)] = {0};
	unsigned char status_after[sizeof(uint32_t)] = {0};
	int have_status_bytes = 0;

	if (!darwin_wait_options_to_linux(options, &linux_options)) {
		errno = EINVAL;
		return -1;
	}

	errno = 0;
	pid_t result = (pid_t)syscall(SYS_wait4, pid,
	                              status ? &linux_status : NULL,
	                              linux_options, linux_usage_ptr);
	int saved_errno = errno;
	int darwin_status = linux_status;
	if (result > 0) {
		if (status) {
			memcpy(status_before, status, sizeof(status_before));
			darwin_status = linux_wait_status_to_darwin(linux_status);
			*status = darwin_status;
			memcpy(status_after, status, sizeof(status_after));
			have_status_bytes = 1;
			shim_last_wait_valid = 1;
			shim_last_wait_owner_pid = (pid_t)syscall(SYS_getpid);
			shim_last_wait_result_pid = result;
			shim_last_wait_linux_status = linux_status;
			shim_last_wait_darwin_status = darwin_status;
			shim_last_wait_status_ptr = (uintptr_t)status;
		}
		if (usage)
			linux_rusage_to_darwin(&linux_usage, darwin_usage);
		if (pid > 0)
			result = pid;
	}

	if (shim_trace_enabled() || shim_wait_trace_enabled()) {
		if (result > 0 && status) {
			fprintf(stderr,
			        "libsystem_shim: wait4(%d status=%p options=%#x usage=%p) -> %d linux_status=%#x darwin_status=%#x exited=%d exit=%d signaled=%d signal=%d core=%d errno=0\n",
			        pid, status, options, usage, result, linux_status,
			        darwin_status, WIFEXITED(linux_status),
			        WIFEXITED(linux_status) ? WEXITSTATUS(linux_status) : -1,
			        WIFSIGNALED(linux_status),
			        WIFSIGNALED(linux_status) ? WTERMSIG(linux_status) : -1,
#ifdef WCOREDUMP
			        WIFSIGNALED(linux_status) ? WCOREDUMP(linux_status) : 0
#else
			        0
#endif
			);
		} else {
			fprintf(stderr,
			        "libsystem_shim: wait4(%d status=%p options=%#x usage=%p) -> %d errno=%d\n",
			        pid, status, options, usage, result,
			        result < 0 ? saved_errno : 0);
		}
	}

	FILE* trace_file = shim_open_trace_file();
	if (trace_file) {
		if (result > 0 && status) {
			fprintf(trace_file,
			        "libsystem_shim: wait4 self=%d tid=%d ppid=%d fork_child=%d pid=%d result=%d options=%#x status_ptr=%p linux_status=%#x darwin_status=%#x status_bytes_valid=%d before=%02x%02x%02x%02x after=%02x%02x%02x%02x exited=%d exit=%d signaled=%d signal=%d\n",
			        (int)syscall(SYS_getpid), (int)shim_trace_tid(),
			        (int)syscall(SYS_getppid), machgate_shim_in_fork_child(),
			        pid, result, options, status, linux_status, darwin_status,
			        have_status_bytes, status_before[0], status_before[1],
			        status_before[2], status_before[3], status_after[0],
			        status_after[1], status_after[2], status_after[3],
			        WIFEXITED(linux_status),
			        WIFEXITED(linux_status) ? WEXITSTATUS(linux_status) : -1,
			        WIFSIGNALED(linux_status),
			        WIFSIGNALED(linux_status) ? WTERMSIG(linux_status) : -1);
		} else {
			fprintf(trace_file,
			        "libsystem_shim: wait4 self=%d tid=%d ppid=%d fork_child=%d pid=%d result=%d options=%#x errno=%d\n",
			        (int)syscall(SYS_getpid), (int)shim_trace_tid(),
			        (int)syscall(SYS_getppid), machgate_shim_in_fork_child(),
			        pid, result, options, result < 0 ? saved_errno : 0);
		}
		fclose(trace_file);
	}

	errno = saved_errno;
	return result;
}

pid_t waitpid(pid_t pid, int* status, int options)
{
	return wait4(pid, status, options, NULL);
}

static int darwin_sigprocmask_how_to_linux(int darwin_how)
{
	switch (darwin_how) {
	case 1: return SIG_BLOCK;
	case 2: return SIG_UNBLOCK;
	case 3: return SIG_SETMASK;
	default: return -1;
	}
}

static int linux_sigemptyset(sigset_t* set)
{
	static int (*real_sigemptyset)(sigset_t*) = NULL;

	if (!real_sigemptyset)
		real_sigemptyset = dlsym(RTLD_NEXT, "sigemptyset");
	return real_sigemptyset(set);
}

static int linux_sigaddset(sigset_t* set, int signum)
{
	static int (*real_sigaddset)(sigset_t*, int) = NULL;

	if (!real_sigaddset)
		real_sigaddset = dlsym(RTLD_NEXT, "sigaddset");
	return real_sigaddset(set, signum);
}

static void darwin_sigset_to_linux(uint32_t darwin_set, sigset_t* linux_set)
{
	linux_sigemptyset(linux_set);
	for (int bit = 1; bit < 32; bit++) {
		if (darwin_set & (1u << (bit - 1))) {
			int linux_signal = darwin_signal_to_linux(bit);
			if (linux_signal > 0)
				linux_sigaddset(linux_set, linux_signal);
		}
	}
}

static uint32_t linux_sigset_to_darwin(const sigset_t* linux_set)
{
	uint32_t result = 0;
	for (int linux_bit = 1; linux_bit < 32; linux_bit++) {
		if (sigismember(linux_set, linux_bit) == 1) {
			int darwin_bit = linux_signal_to_darwin(linux_bit);
			if (darwin_bit > 0 && darwin_bit < 32)
				result |= 1u << (darwin_bit - 1);
		}
	}
	return result;
}

static void (*trace_signal_actions[_NSIG])(int, siginfo_t*, void*);
static void (*trace_signal_handlers[_NSIG])(int);
static int trace_signal_uses_siginfo[_NSIG];

static uintptr_t trace_ucontext_pc(void* ucontext)
{
#if defined(__aarch64__)
	return ((ucontext_t*)ucontext)->uc_mcontext.pc;
#else
	(void)ucontext;
	return 0;
#endif
}

static uintptr_t trace_ucontext_sp(void* ucontext)
{
#if defined(__aarch64__)
	return ((ucontext_t*)ucontext)->uc_mcontext.sp;
#else
	(void)ucontext;
	return 0;
#endif
}

static uintptr_t trace_ucontext_reg(void* ucontext, int reg)
{
#if defined(__aarch64__)
	if (reg == 31)
		return ((ucontext_t*)ucontext)->uc_mcontext.sp;
	return ((ucontext_t*)ucontext)->uc_mcontext.regs[reg];
#else
	(void)ucontext;
	(void)reg;
	return 0;
#endif
}

static void trace_signal_dispatcher(int signum, siginfo_t* info, void* ucontext)
{
	if (signum == SIGABRT)
		shim_dump_recent_alloc_events("signal-sigabrt", NULL);

	if (shim_signal_trace_enabled()) {
		uintptr_t pc = trace_ucontext_pc(ucontext);
		uintptr_t lr = trace_ucontext_reg(ucontext, 30);
		fprintf(stderr,
		        "libsystem_shim: guest signal signum=%d darwin=%d code=%d addr=%p pc=%p lr=%p sp=%p fp=%p\n",
		        signum, linux_signal_to_darwin(signum), info ? info->si_code : 0,
		        info ? info->si_addr : NULL,
		        (void*)pc,
		        (void*)lr,
		        (void*)trace_ucontext_sp(ucontext),
		        (void*)trace_ucontext_reg(ucontext, 29));
		trace_init_context();
		if (lr >= 4)
			trace_signal_indirect_branch(lr - 4, ucontext);
		fprintf(stderr,
		        "libsystem_shim: guest regs x0=%p x1=%p x2=%p x3=%p x4=%p x5=%p x6=%p x7=%p x8=%p x16=%p\n",
		        (void*)trace_ucontext_reg(ucontext, 0),
		        (void*)trace_ucontext_reg(ucontext, 1),
		        (void*)trace_ucontext_reg(ucontext, 2),
		        (void*)trace_ucontext_reg(ucontext, 3),
		        (void*)trace_ucontext_reg(ucontext, 4),
		        (void*)trace_ucontext_reg(ucontext, 5),
		        (void*)trace_ucontext_reg(ucontext, 6),
		        (void*)trace_ucontext_reg(ucontext, 7),
		        (void*)trace_ucontext_reg(ucontext, 8),
		        (void*)trace_ucontext_reg(ucontext, 16));
		fprintf(stderr,
		        "libsystem_shim: guest regs x19=%p x20=%p x21=%p x22=%p x23=%p x24=%p x25=%p x26=%p x27=%p x28=%p\n",
		        (void*)trace_ucontext_reg(ucontext, 19),
		        (void*)trace_ucontext_reg(ucontext, 20),
		        (void*)trace_ucontext_reg(ucontext, 21),
		        (void*)trace_ucontext_reg(ucontext, 22),
		        (void*)trace_ucontext_reg(ucontext, 23),
		        (void*)trace_ucontext_reg(ucontext, 24),
		        (void*)trace_ucontext_reg(ucontext, 25),
		        (void*)trace_ucontext_reg(ucontext, 26),
		        (void*)trace_ucontext_reg(ucontext, 27),
		        (void*)trace_ucontext_reg(ucontext, 28));
		trace_signal_faulting_load_context(pc, ucontext);
		trace_signal_tree_insert_context(pc, ucontext);
		trace_guest_address_context("signal.pc", pc);
		trace_guest_address_context("signal.lr", lr);
		if (lr >= 4)
			trace_guest_address_context("signal.lr-4", lr - 4);
		trace_signal_register_context(ucontext);
	}

	if (signum > 0 && signum < _NSIG) {
		if (trace_signal_uses_siginfo[signum] && trace_signal_actions[signum]) {
			trace_signal_actions[signum](signum, info, ucontext);
			return;
		}
		if (trace_signal_handlers[signum]) {
			trace_signal_handlers[signum](signum);
			return;
		}
	}

	signal(signum, SIG_DFL);
	raise(signum);
}

static int should_trace_guest_signal(int linux_signal)
{
	if (!shim_signal_trace_enabled())
		return 0;
	return linux_signal == SIGSEGV || linux_signal == SIGBUS ||
	       linux_signal == SIGILL || linux_signal == SIGABRT;
}

int sigaltstack(const stack_t *new_stack, stack_t *old_stack)
{
	static int (*real_sigaltstack)(const stack_t*, stack_t*) = NULL;
	void* caller = SHIM_CALLER_RETURN_ADDRESS();
	const struct darwin_sigaltstack* darwin_new_stack =
		(const struct darwin_sigaltstack*)new_stack;
	struct darwin_sigaltstack* darwin_old_stack =
		(struct darwin_sigaltstack*)old_stack;
	stack_t linux_new_stack;
	stack_t linux_old_stack;
	stack_t* linux_new_stack_ptr = NULL;
	stack_t* linux_old_stack_ptr = darwin_old_stack ? &linux_old_stack : NULL;

	if (!real_sigaltstack)
		real_sigaltstack = dlsym(RTLD_NEXT, "sigaltstack");
	if (!real_sigaltstack) {
		int saved_errno = errno;
		if (shim_delta_vm_trace_enabled()) {
			fprintf(stderr,
			        "libsystem_shim: sigaltstack tid=%d pthread=%#lx caller=%p new=%p old=%p -> -1 errno=%d\n",
			        (int)shim_trace_tid(), shim_trace_pthread_self(), caller,
			        new_stack, old_stack, saved_errno);
			errno = saved_errno;
		}
		return -1;
	}

	if (darwin_new_stack) {
		int linux_flags;
		if (!darwin_sigaltstack_flags_to_linux(darwin_new_stack->flags,
		                                       &linux_flags)) {
			errno = EINVAL;
			if (shim_delta_vm_trace_enabled()) {
				int saved_errno = errno;
				fprintf(stderr,
				        "libsystem_shim: sigaltstack tid=%d pthread=%#lx caller=%p new=%p new_sp=%p new_size=%llu new_flags=%#x old=%p -> -1 errno=%d\n",
				        (int)shim_trace_tid(), shim_trace_pthread_self(), caller,
				        new_stack, (void*)(uintptr_t)darwin_new_stack->sp,
				        (unsigned long long)darwin_new_stack->size,
				        darwin_new_stack->flags, old_stack, saved_errno);
				errno = saved_errno;
			}
			return -1;
		}
		linux_new_stack.ss_sp = (void*)darwin_new_stack->sp;
		linux_new_stack.ss_size = darwin_new_stack->size;
		linux_new_stack.ss_flags = linux_flags;
		linux_new_stack_ptr = &linux_new_stack;
	}

	int result = real_sigaltstack(linux_new_stack_ptr, linux_old_stack_ptr);
	int saved_errno = errno;
	if (shim_delta_vm_trace_enabled()) {
		fprintf(stderr,
		        "libsystem_shim: sigaltstack tid=%d pthread=%#lx caller=%p new=%p new_sp=%p new_size=%llu new_flags=%#x old=%p -> %d errno=%d\n",
		        (int)shim_trace_tid(), shim_trace_pthread_self(), caller,
		        new_stack,
		        darwin_new_stack ? (void*)(uintptr_t)darwin_new_stack->sp : NULL,
		        darwin_new_stack ? (unsigned long long)darwin_new_stack->size : 0,
		        darwin_new_stack ? darwin_new_stack->flags : 0,
		        old_stack, result, result < 0 ? saved_errno : 0);
		errno = saved_errno;
	}
	if (result < 0)
		return result;

	if (darwin_old_stack) {
		darwin_old_stack->sp = (uint64_t)linux_old_stack.ss_sp;
		darwin_old_stack->size = linux_old_stack.ss_size;
		darwin_old_stack->flags =
			linux_sigaltstack_flags_to_darwin(linux_old_stack.ss_flags);
		darwin_old_stack->pad = 0;
	}

	errno = saved_errno;
	return result;
}

int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
	static int (*real_sigaction)(int, const struct sigaction*, struct sigaction*) = NULL;
	const struct darwin_sigaction* darwin_act =
		(const struct darwin_sigaction*)act;
	struct darwin_sigaction* darwin_oldact =
		(struct darwin_sigaction*)oldact;
	struct sigaction linux_act;
	struct sigaction linux_oldact;
	struct sigaction* linux_act_ptr = NULL;
	struct sigaction* linux_oldact_ptr = darwin_oldact ? &linux_oldact : NULL;
	int linux_signal = darwin_signal_to_linux(signum);

	if (linux_signal <= 0) {
		if (darwin_oldact) {
			darwin_oldact->handler = 0;
			darwin_oldact->mask = 0;
			darwin_oldact->flags = 0;
		}
		if (shim_trace_enabled())
			fprintf(stderr, "libsystem_shim: sigaction(%d->%d act=%p old=%p) -> 0 errno=0\n",
			        signum, linux_signal, act, oldact);
		return 0;
	}

	if ((signum == 9 || signum == 17) &&
	    (!darwin_act || darwin_act->handler == 0)) {
		if (darwin_oldact) {
			darwin_oldact->handler = 0;
			darwin_oldact->mask = 0;
			darwin_oldact->flags = 0;
		}
		if (shim_trace_enabled())
			fprintf(stderr, "libsystem_shim: sigaction(%d->%d act=%p old=%p) -> 0 errno=0\n",
			        signum, linux_signal, act, oldact);
		return 0;
	}

	if ((signum == 4 || signum == 7 || signum == 11) && darwin_act &&
	    darwin_act->handler != 0 && getenv("MACHGATE_KEEP_CRASH_HANDLER")) {
		if (darwin_oldact) {
			darwin_oldact->handler = 0;
			darwin_oldact->mask = 0;
			darwin_oldact->flags = 0;
		}
		if (shim_trace_enabled())
			fprintf(stderr, "libsystem_shim: sigaction(%d->%d act=%p old=%p) kept machgate handler\n",
			        signum, linux_signal, act, oldact);
		return 0;
	}

	if ((signum == 9 || signum == 17) && darwin_act) {
		errno = EINVAL;
		if (shim_trace_enabled())
			fprintf(stderr, "libsystem_shim: sigaction(%d->%d act=%p old=%p) -> -1 errno=%d\n",
			        signum, linux_signal, act, oldact, errno);
		return -1;
	}

	if (!real_sigaction)
		real_sigaction = dlsym(RTLD_NEXT, "sigaction");
	if (!real_sigaction)
		return -1;

	if (darwin_act) {
		int linux_flags;
		if (!darwin_sigaction_flags_to_linux(darwin_act->flags,
		                                     &linux_flags)) {
			errno = EINVAL;
			return -1;
		}
		memset(&linux_act, 0, sizeof(linux_act));
		linux_act.sa_flags = linux_flags;
		linux_sigemptyset(&linux_act.sa_mask);
		for (int bit = 1; bit < 32; bit++) {
			if (darwin_act->mask & (1u << (bit - 1))) {
				int mask_signal = darwin_signal_to_linux(bit);
				if (mask_signal > 0)
					linux_sigaddset(&linux_act.sa_mask, mask_signal);
			}
		}
			if (should_trace_guest_signal(linux_signal) &&
			    darwin_act->handler != (uint64_t)(uintptr_t)SIG_DFL &&
			    darwin_act->handler != (uint64_t)(uintptr_t)SIG_IGN) {
			trace_signal_uses_siginfo[linux_signal] =
				(linux_flags & SA_SIGINFO) != 0;
			if (trace_signal_uses_siginfo[linux_signal]) {
				trace_signal_actions[linux_signal] =
					(void (*)(int, siginfo_t*, void*))(uintptr_t)darwin_act->handler;
				trace_signal_handlers[linux_signal] = NULL;
			} else {
				trace_signal_handlers[linux_signal] =
					(void (*)(int))(uintptr_t)darwin_act->handler;
				trace_signal_actions[linux_signal] = NULL;
			}
			linux_act.sa_sigaction = trace_signal_dispatcher;
			linux_act.sa_flags = linux_flags | SA_SIGINFO;
			} else {
				linux_act.sa_handler = (void (*)(int))(uintptr_t)darwin_act->handler;
			}
			if (machgate_shim_in_fork_child() && signum == 13) {
				linux_act.sa_handler = SIG_IGN;
				linux_act.sa_flags = 0;
				linux_sigemptyset(&linux_act.sa_mask);
			}
			if (signum == 20 && !shim_host_sigchld_handler_enabled()) {
				linux_act.sa_handler = SIG_DFL;
				linux_act.sa_flags = 0;
				linux_sigemptyset(&linux_act.sa_mask);
			}
			if ((shim_wait_trace_enabled() || shim_signal_trace_enabled()) &&
			    signum == 13) {
				fprintf(stderr,
				        "libsystem_shim: sigaction SIGPIPE fork_child=%d handler=%p effective=%p flags=%#x\n",
				        machgate_shim_in_fork_child(),
				        (void*)(uintptr_t)darwin_act->handler,
				        (void*)linux_act.sa_handler,
				        linux_act.sa_flags);
			}
			if ((shim_wait_trace_enabled() || shim_signal_trace_enabled()) &&
			    signum == 20) {
				fprintf(stderr,
				        "libsystem_shim: sigaction SIGCHLD host_handler=%d handler=%p effective=%p flags=%#x\n",
				        shim_host_sigchld_handler_enabled(),
				        (void*)(uintptr_t)darwin_act->handler,
				        (void*)linux_act.sa_handler,
				        linux_act.sa_flags);
			}
		linux_act_ptr = &linux_act;
	}

	int result = real_sigaction(linux_signal, linux_act_ptr, linux_oldact_ptr);
	if (shim_trace_enabled())
		fprintf(stderr, "libsystem_shim: sigaction(%d->%d act=%p old=%p) -> %d errno=%d\n",
		        signum, linux_signal, act, oldact, result, result < 0 ? errno : 0);
	if (result < 0)
		return result;

	if (darwin_oldact) {
		darwin_oldact->handler = (uint64_t)(uintptr_t)linux_oldact.sa_handler;
		if (darwin_oldact->handler ==
		    (uint64_t)(uintptr_t)trace_signal_dispatcher &&
		    linux_signal > 0 && linux_signal < _NSIG) {
			if (trace_signal_uses_siginfo[linux_signal])
				darwin_oldact->handler =
					(uint64_t)(uintptr_t)trace_signal_actions[linux_signal];
			else
					darwin_oldact->handler =
						(uint64_t)(uintptr_t)trace_signal_handlers[linux_signal];
			}
			darwin_oldact->mask = 0;
		for (int linux_bit = 1; linux_bit < 32; linux_bit++) {
			if (sigismember(&linux_oldact.sa_mask, linux_bit) == 1) {
				int darwin_bit = linux_signal_to_darwin(linux_bit);
				if (darwin_bit > 0 && darwin_bit < 32)
					darwin_oldact->mask |= 1u << (darwin_bit - 1);
			}
		}
		darwin_oldact->flags =
			linux_sigaction_flags_to_darwin(linux_oldact.sa_flags);
	}

	return result;
}

int pthread_sigmask(int how, const sigset_t *set, sigset_t *oldset)
{
	static int (*real_pthread_sigmask)(int, const sigset_t*, sigset_t*) = NULL;
	sigset_t linux_set;
	sigset_t linux_oldset;
	const sigset_t* linux_set_ptr = NULL;
	sigset_t* linux_oldset_ptr = oldset ? &linux_oldset : NULL;
	int linux_how = darwin_sigprocmask_how_to_linux(how);

	if (!real_pthread_sigmask)
		real_pthread_sigmask = dlsym(RTLD_NEXT, "pthread_sigmask");
	if (!real_pthread_sigmask)
		return ENOSYS;
	if (linux_how < 0)
		return EINVAL;
	if (set) {
		darwin_sigset_to_linux(*(const uint32_t*)set, &linux_set);
		linux_set_ptr = &linux_set;
	}
	int result = real_pthread_sigmask(linux_how, linux_set_ptr, linux_oldset_ptr);
	if (shim_trace_enabled())
		fprintf(stderr, "libsystem_shim: pthread_sigmask(%d->%d set=%p old=%p) -> %d\n",
		        how, linux_how, set, oldset, result);
	if (result == 0 && oldset)
		*(uint32_t*)oldset = linux_sigset_to_darwin(&linux_oldset);
	return result;
}

int sigemptyset(sigset_t *set)
{
	*(uint32_t*)set = 0;
	if (shim_trace_enabled())
		fprintf(stderr, "libsystem_shim: sigemptyset(%p)\n", set);
	return 0;
}

int sigfillset(sigset_t *set)
{
	*(uint32_t*)set = 0xffffffffu;
	if (shim_trace_enabled())
		fprintf(stderr, "libsystem_shim: sigfillset(%p)\n", set);
	return 0;
}

int sigaddset(sigset_t *set, int signum)
{
	if (signum <= 0 || signum >= 32) {
		errno = EINVAL;
		return -1;
	}
	*(uint32_t*)set |= 1u << (signum - 1);
	if (shim_trace_enabled())
		fprintf(stderr, "libsystem_shim: sigaddset(%p, %d)\n", set, signum);
	return 0;
}

int sigwait(const sigset_t *set, int *sig)
{
	static int (*real_sigwait)(const sigset_t*, int*) = NULL;
	sigset_t linux_set;
	int linux_signal;

	if (!real_sigwait)
		real_sigwait = dlsym(RTLD_NEXT, "sigwait");
	if (!real_sigwait)
		return ENOSYS;
	darwin_sigset_to_linux(*(const uint32_t*)set, &linux_set);
	int result = real_sigwait(&linux_set, &linux_signal);
	if (result == 0)
		*sig = linux_signal_to_darwin(linux_signal);
	return result;
}

/* ===== sysctl ===== */

/* Apple <sys/sysctl.h> MIB constants we answer. */
#define DARWIN_CTL_KERN     1
#define DARWIN_CTL_HW       6
#define DARWIN_KERN_OSTYPE  1
#define DARWIN_KERN_OSRELEASE 2
#define DARWIN_KERN_VERSION 4
#define DARWIN_KERN_ARGMAX  8
#define DARWIN_KERN_HOSTNAME 10
#define DARWIN_KERN_OSVERSION 65
#define DARWIN_KERN_OSPRODUCTVERSION 140
#define DARWIN_KERN_OSPRODUCTVERSION_COMPAT 141
#define DARWIN_HW_MACHINE   1
#define DARWIN_HW_MODEL     2
#define DARWIN_HW_NCPU      3
#define DARWIN_HW_BYTEORDER 4
#define DARWIN_HW_PHYSMEM   5
#define DARWIN_HW_PAGESIZE  7
#define DARWIN_HW_MEMSIZE   24
#define DARWIN_HW_AVAILCPU  25
#define DARWIN_HW_PHYSICALCPU 101
#define DARWIN_HW_LOGICALCPU 103

static const char darwin_ostype[] = "Darwin";
static const char darwin_osrelease[] = "24.6.0";
static const char darwin_version[] = "Darwin Kernel Version 24.6.0: MachGate";
static const char darwin_osversion[] = "24G84";
static const char darwin_osproductversion[] = "15.6";
static const char darwin_machine[] = "arm64";
static const char darwin_model[] = "VirtualMac2,1";

static int shim_hw_ncpu(void)
{
	const char* override = getenv("MACHGATE_GUEST_NCPU");
	if (override && *override) {
		int parsed = atoi(override);
		if (parsed >= 1 && parsed <= 255)
			return parsed;
	}
	long n = sysconf(_SC_NPROCESSORS_ONLN);
	return (n < 1) ? 1 : (int)n;
}

static uint64_t shim_hw_memsize(void)
{
	long pages = sysconf(_SC_PHYS_PAGES);
	long psize = sysconf(_SC_PAGESIZE);
	if (pages < 1 || psize < 1)
		return 0;
	return (uint64_t)pages * (uint64_t)psize;
}

static int shim_sysctl_copy(const void* value, size_t value_size, void* oldp,
                            size_t* oldlenp)
{
	if (!oldlenp) {
		errno = EINVAL;
		return -1;
	}

	if (!oldp) {
		*oldlenp = value_size;
		return 0;
	}

	if (*oldlenp < value_size) {
		*oldlenp = value_size;
		errno = ENOMEM;
		return -1;
	}

	memcpy(oldp, value, value_size);
	*oldlenp = value_size;
	return 0;
}

static int shim_sysctl_copy_int(int value, void* oldp, size_t* oldlenp)
{
	return shim_sysctl_copy(&value, sizeof(value), oldp, oldlenp);
}

static int shim_sysctl_copy_uint64(uint64_t value, void* oldp,
                                   size_t* oldlenp)
{
	return shim_sysctl_copy(&value, sizeof(value), oldp, oldlenp);
}

static int shim_sysctl_copy_string(const char* value, void* oldp,
                                   size_t* oldlenp)
{
	return shim_sysctl_copy(value, strlen(value) + 1, oldp, oldlenp);
}

static int shim_sysctl_copy_hostname(void* oldp, size_t* oldlenp)
{
	char hostname[256];

	if (gethostname(hostname, sizeof(hostname)) != 0)
		snprintf(hostname, sizeof(hostname), "%s", "machgate");
	hostname[sizeof(hostname) - 1] = '\0';
	return shim_sysctl_copy_string(hostname, oldp, oldlenp);
}

/* sysctl — answer the hardware MIBs games actually read.
 *
 * CRITICAL: returning -1 without writing *oldp leaves the caller reading
 * uninitialized stack. Mina's startup does exactly this: it asks for
 * {CTL_HW, HW_AVAILCPU} (then {CTL_HW, HW_NCPU}) to size its job-queue worker
 * pool and does NOT check the return value — a stub that skips *oldp yields a
 * garbage core count and an unbounded thread-spawn loop. So we write real values
 * for the CPU/memory/page MIBs and only fall through to ENOTSUP for the rest. */
int sysctl(const int *name, unsigned int namelen,
           void *oldp, size_t *oldlenp,
           const void *newp, size_t newlen)
{
	(void)newp; (void)newlen;

	if (name && namelen >= 2 && name[0] == DARWIN_CTL_KERN) {
		switch (name[1]) {
		case DARWIN_KERN_OSTYPE:
			return shim_sysctl_copy_string(darwin_ostype, oldp,
			                               oldlenp);
		case DARWIN_KERN_OSRELEASE:
			return shim_sysctl_copy_string(darwin_osrelease, oldp,
			                               oldlenp);
		case DARWIN_KERN_VERSION:
			return shim_sysctl_copy_string(darwin_version, oldp,
			                               oldlenp);
		case DARWIN_KERN_ARGMAX:
			return shim_sysctl_copy_int(262144, oldp, oldlenp);
		case DARWIN_KERN_HOSTNAME:
			return shim_sysctl_copy_hostname(oldp, oldlenp);
		case DARWIN_KERN_OSVERSION:
			return shim_sysctl_copy_string(darwin_osversion, oldp,
			                               oldlenp);
		case DARWIN_KERN_OSPRODUCTVERSION:
		case DARWIN_KERN_OSPRODUCTVERSION_COMPAT:
			return shim_sysctl_copy_string(darwin_osproductversion,
			                               oldp, oldlenp);
		default:
			break;
		}
	}

	if (name && namelen >= 2 && name[0] == DARWIN_CTL_HW) {
		switch (name[1]) {
		case DARWIN_HW_MACHINE:
			return shim_sysctl_copy_string(darwin_machine, oldp,
			                               oldlenp);
		case DARWIN_HW_MODEL:
			return shim_sysctl_copy_string(darwin_model, oldp,
			                               oldlenp);
		case DARWIN_HW_NCPU:
		case DARWIN_HW_AVAILCPU:
		case DARWIN_HW_PHYSICALCPU:
		case DARWIN_HW_LOGICALCPU:
			return shim_sysctl_copy_int(shim_hw_ncpu(), oldp,
			                            oldlenp);
		case DARWIN_HW_BYTEORDER:
			return shim_sysctl_copy_int(1234, oldp, oldlenp);
		case DARWIN_HW_PAGESIZE:
			return shim_sysctl_copy_int((int)sysconf(_SC_PAGESIZE),
			                            oldp, oldlenp);
		case DARWIN_HW_MEMSIZE:
		case DARWIN_HW_PHYSMEM:
			return shim_sysctl_copy_uint64(shim_hw_memsize(), oldp,
			                               oldlenp);
		default:
			break;
		}
	}
	errno = ENOENT;
	return -1;
}

int sysctlbyname(const char *name, void *oldp, size_t *oldlenp,
                 const void *newp, size_t newlen)
{
	(void)newp; (void)newlen;

	if (name) {
		if (!strcmp(name, "hw.ncpu")        || !strcmp(name, "hw.activecpu")  ||
		    !strcmp(name, "hw.logicalcpu")  || !strcmp(name, "hw.logicalcpu_max") ||
		    !strcmp(name, "hw.physicalcpu") || !strcmp(name, "hw.physicalcpu_max") ||
		    !strcmp(name, "hw.availcpu")    ||
		    !strcmp(name, "machdep.cpu.core_count") ||
		    !strcmp(name, "machdep.cpu.thread_count")) {
			return shim_sysctl_copy_int(shim_hw_ncpu(), oldp,
			                            oldlenp);
		} else if (!strcmp(name, "hw.memsize")) {
			return shim_sysctl_copy_uint64(shim_hw_memsize(), oldp,
			                               oldlenp);
		} else if (!strcmp(name, "hw.pagesize")) {
			return shim_sysctl_copy_int((int)sysconf(_SC_PAGESIZE),
			                            oldp, oldlenp);
		} else if (!strcmp(name, "hw.byteorder")) {
			return shim_sysctl_copy_int(1234, oldp, oldlenp);
		} else if (!strcmp(name, "hw.optional.arm64")) {
			return shim_sysctl_copy_int(1, oldp, oldlenp);
		} else if (!strcmp(name, "hw.machine")) {
			return shim_sysctl_copy_string(darwin_machine, oldp,
			                               oldlenp);
		} else if (!strcmp(name, "hw.model")) {
			return shim_sysctl_copy_string(darwin_model, oldp,
			                               oldlenp);
		} else if (!strcmp(name, "kern.ostype")) {
			return shim_sysctl_copy_string(darwin_ostype, oldp,
			                               oldlenp);
		} else if (!strcmp(name, "kern.osrelease")) {
			return shim_sysctl_copy_string(darwin_osrelease, oldp,
			                               oldlenp);
		} else if (!strcmp(name, "kern.version")) {
			return shim_sysctl_copy_string(darwin_version, oldp,
			                               oldlenp);
		} else if (!strcmp(name, "kern.hostname")) {
			return shim_sysctl_copy_hostname(oldp, oldlenp);
		} else if (!strcmp(name, "kern.argmax")) {
			return shim_sysctl_copy_int(262144, oldp, oldlenp);
		} else if (!strcmp(name, "kern.osversion")) {
			return shim_sysctl_copy_string(darwin_osversion, oldp,
			                               oldlenp);
		} else if (!strcmp(name, "kern.osproductversion") ||
		           !strcmp(name, "kern.osproductversioncompat")) {
			return shim_sysctl_copy_string(darwin_osproductversion,
			                               oldp, oldlenp);
		}
	}
	errno = ENOENT;
	return -1;
}

int uname(void* buffer)
{
	char* bytes = buffer;

	if (!bytes) {
		errno = EFAULT;
		return -1;
	}

	memset(bytes, 0, 256 * 5);
	snprintf(bytes, 256, "%s", darwin_ostype);
	if (gethostname(bytes + 256, 256) != 0)
		snprintf(bytes + 256, 256, "%s", "machgate");
	snprintf(bytes + 512, 256, "%s", darwin_osrelease);
	snprintf(bytes + 768, 256, "%s", darwin_version);
	snprintf(bytes + 1024, 256, "%s", darwin_machine);
	return 0;
}

/* ===== Blocks runtime ===== */

#define BLOCK_REFCOUNT_MASK   0xffffu
#define BLOCK_NEEDS_FREE      (1u << 24)
#define BLOCK_HAS_COPY_DISPOSE (1u << 25)
#define BLOCK_IS_GLOBAL       (1u << 28)

#define BLOCK_FIELD_IS_OBJECT 3
#define BLOCK_FIELD_IS_BLOCK  7
#define BLOCK_FIELD_IS_BYREF  8
#define BLOCK_FIELD_IS_WEAK   16
#define BLOCK_BYREF_CALLER    128

struct block_descriptor {
	uint64_t reserved;
	uint64_t size;
	void (*copy)(void* destination, const void* source);
	void (*dispose)(const void* block);
};

struct block_layout {
	void* isa;
	uint32_t flags;
	uint32_t reserved;
	void (*invoke)(void* block, ...);
	const struct block_descriptor* descriptor;
};

struct block_byref {
	void* isa;
	struct block_byref* forwarding;
	uint32_t flags;
	uint32_t size;
	void (*byref_keep)(struct block_byref* destination, struct block_byref* source);
	void (*byref_destroy)(struct block_byref* source);
};

void* _NSConcreteGlobalBlock = NULL;
void* _NSConcreteStackBlock = NULL;
void* _NSConcreteMallocBlock = NULL;

static int block_flags_latch_increment(uint32_t* flags)
{
	uint32_t previous = __sync_fetch_and_add(flags, 1);
	if ((previous & BLOCK_REFCOUNT_MASK) == BLOCK_REFCOUNT_MASK) {
		__sync_fetch_and_sub(flags, 1);
		return 1;
	}
	return 0;
}

static void* block_copy_internal(const void* block)
{
	const struct block_layout* source = block;
	struct block_layout* result;

	if (!source)
		return NULL;
	if (source->flags & BLOCK_NEEDS_FREE) {
		block_flags_latch_increment(&((struct block_layout*)source)->flags);
		return (void*)source;
	}
	if (source->flags & BLOCK_IS_GLOBAL)
		return (void*)source;

	if (!source->descriptor || !source->invoke)
		return NULL;

	result = malloc(source->descriptor->size);
	if (!result)
		return NULL;
	memmove(result, source, source->descriptor->size);
	result->flags &= ~BLOCK_REFCOUNT_MASK;
	result->flags |= BLOCK_NEEDS_FREE | 1;
	result->isa = &_NSConcreteMallocBlock;
	if (result->flags & BLOCK_HAS_COPY_DISPOSE) {
		if (source->descriptor->copy)
			source->descriptor->copy(result, source);
	}
	return result;
}

void* _Block_copy(const void* block)
{
	return block_copy_internal(block);
}

static void block_byref_release(const void* object);

void _Block_release(void* block)
{
	struct block_layout* source = block;
	uint32_t new_count;

	if (!source)
		return;
	if (source->isa == &_NSConcreteStackBlock && !(source->flags & BLOCK_NEEDS_FREE))
		return;
	if (!(source->flags & BLOCK_NEEDS_FREE))
		return;
	new_count = __sync_fetch_and_sub(&source->flags, 1) & BLOCK_REFCOUNT_MASK;
	if (new_count > 1)
		return;
	if (source->flags & BLOCK_HAS_COPY_DISPOSE) {
		if (source->descriptor->dispose)
			source->descriptor->dispose(source);
	}
	free(source);
}

static void block_byref_assign_copy(void* destination, const void* object)
{
	struct block_byref** destination_slot = destination;
	struct block_byref* source = (struct block_byref*)object;
	struct block_byref* copy;

	if (!source || !source->forwarding)
		return;
	if (source->forwarding->flags & BLOCK_NEEDS_FREE) {
		block_flags_latch_increment(&source->forwarding->flags);
		*destination_slot = source->forwarding;
		return;
	}

	copy = malloc(source->size);
	if (!copy)
		return;
	memmove(copy, source->forwarding, source->size);
	copy->flags = source->forwarding->flags | BLOCK_NEEDS_FREE | 2;
	copy->forwarding = copy;
	source->forwarding = copy;
	if (source->flags & BLOCK_HAS_COPY_DISPOSE) {
		copy->byref_keep = source->byref_keep;
		copy->byref_destroy = source->byref_destroy;
		if (source->byref_keep)
			source->byref_keep(copy, source);
	}
	*destination_slot = copy;
}

static void block_byref_release(const void* object)
{
	struct block_byref* shared = (struct block_byref*)object;

	if (!shared || !shared->forwarding)
		return;
	shared = shared->forwarding;
	if (!(shared->flags & BLOCK_NEEDS_FREE))
		return;
	if ((__sync_fetch_and_sub(&shared->flags, 1) & BLOCK_REFCOUNT_MASK) == 1) {
		if ((shared->flags & BLOCK_HAS_COPY_DISPOSE) && shared->byref_destroy)
			shared->byref_destroy(shared);
		free(shared);
	}
}

void _Block_object_assign(void* destination, const void* object, int flags)
{
	if (!destination || !object)
		return;

	if (flags & BLOCK_FIELD_IS_BYREF) {
		if (!(flags & BLOCK_BYREF_CALLER))
			block_byref_assign_copy(destination, object);
		else
			*(void**)destination = ((struct block_byref*)object)->forwarding;
		return;
	}
	if ((flags & BLOCK_FIELD_IS_BLOCK) && !(flags & BLOCK_BYREF_CALLER)) {
		*(void**)destination = block_copy_internal(object);
		return;
	}
	*(void**)destination = (void*)object;
}

void _Block_object_dispose(const void* object, int flags)
{
	if (!object)
		return;

	if (flags & BLOCK_FIELD_IS_BYREF) {
		block_byref_release(object);
		return;
	}
	if ((flags & (BLOCK_FIELD_IS_BLOCK | BLOCK_BYREF_CALLER)) == BLOCK_FIELD_IS_BLOCK) {
		_Block_release((void*)object);
		return;
	}
}

void* objc_retainBlock(void* block)
{
	return block_copy_internal(block);
}


/* ===== Grand Central Dispatch ===== */

/*
 * Minimal dispatch implementation for bx/bgfx threading.
 *
 * Imported symbols (from llvm-nm -U):
 *   __dispatch_main_q          — global DATA object (queue)
 *   _dispatch_async            — function
 *   _dispatch_data_create      — function
 *   _dispatch_get_global_queue — function
 *   _dispatch_release          — function
 *   _dispatch_retain           — function (implicit)
 *   _dispatch_semaphore_create — function
 *   _dispatch_semaphore_signal — function
 *   _dispatch_semaphore_wait   — function
 *   _dispatch_sync             — function
 *   _dispatch_time             — function
 *
 * Note: macOS dispatch_get_main_queue() is a macro for &_dispatch_main_q,
 * so the binary imports the global, not the function.
 */
#include <semaphore.h>

#define DISPATCH_OBJ_SEMAPHORE 1
#define DISPATCH_OBJ_QUEUE     2
#define DISPATCH_OBJ_DATA      3

struct dispatch_object {
	int type;
	int refcount;
};

struct dispatch_semaphore {
	int type;  /* DISPATCH_OBJ_SEMAPHORE */
	int refcount;
	sem_t sem;
};

struct dispatch_queue {
	int type;  /* DISPATCH_OBJ_QUEUE */
	int refcount;
	char label[64];
	struct dispatch_work_item* head;
	struct dispatch_work_item* tail;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int worker_started;
	int serial;
};

struct dispatch_work_item {
	void* block;
	struct dispatch_work_item* next;
	void* context;
	void (*function)(void* context);
};

typedef void (*dispatch_block_t)(void);
typedef void (*dispatch_function_t)(void*);

static void dispatch_run_block(void* block);


/* Global queues — the binary imports _dispatch_main_q as a DATA symbol.
 * After Mach-O underscore stripping, resolver looks for "_dispatch_main_q". */
struct dispatch_queue _dispatch_main_q = { DISPATCH_OBJ_QUEUE, 100, "com.apple.main-thread" };
static struct dispatch_queue _dispatch_global_default = { DISPATCH_OBJ_QUEUE, 100, "com.apple.root.default" };

/* ---- Semaphores (real, backed by POSIX sem_t) ---- */

void* dispatch_semaphore_create(long value)
{
	struct dispatch_semaphore *ds = (struct dispatch_semaphore *)calloc(1, sizeof(*ds));
	if (!ds) return NULL;
	ds->type = DISPATCH_OBJ_SEMAPHORE;
	ds->refcount = 1;
	sem_init(&ds->sem, 0, (unsigned int)value);
	return ds;
}

long dispatch_semaphore_wait(void *dsema, uint64_t timeout)
{
	struct dispatch_semaphore *ds = (struct dispatch_semaphore *)dsema;
	if (!ds) return -1;
	if (timeout == ~(uint64_t)0) {
		/* DISPATCH_TIME_FOREVER */
		return sem_wait(&ds->sem) == 0 ? 0 : -1;
	} else if (timeout == 0) {
		/* DISPATCH_TIME_NOW */
		return sem_trywait(&ds->sem) == 0 ? 0 : -1;
	} else {
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		uint64_t ns = timeout;
		ts.tv_sec += ns / 1000000000ULL;
		ts.tv_nsec += ns % 1000000000ULL;
		if (ts.tv_nsec >= 1000000000L) {
			ts.tv_sec++;
			ts.tv_nsec -= 1000000000L;
		}
		return sem_timedwait(&ds->sem, &ts) == 0 ? 0 : -1;
	}
}

long dispatch_semaphore_signal(void *dsema)
{
	struct dispatch_semaphore *ds = (struct dispatch_semaphore *)dsema;
	if (!ds) return 0;
	sem_post(&ds->sem);
	return 0;
}

/* ---- Queues (serial: dedicated worker per created queue) ---- */

void* dispatch_get_global_queue(long priority, unsigned long flags)
{
	(void)priority; (void)flags;
	return &_dispatch_global_default;
}

static void dispatch_run_item(struct dispatch_work_item* item)
{
	if (item->block) {
		dispatch_run_block(item->block);
		return;
	}
	if (item->function)
		item->function(item->context);
}

static void* dispatch_serial_worker(void* arg)
{
	struct dispatch_queue* queue = arg;

	for (;;) {
		struct dispatch_work_item* item;

		pthread_mutex_lock(&queue->mutex);
		while (!queue->head)
			pthread_cond_wait(&queue->cond, &queue->mutex);
		item = queue->head;
		queue->head = item->next;
		if (!queue->head)
			queue->tail = NULL;
		pthread_mutex_unlock(&queue->mutex);

		dispatch_run_item(item);
		free(item);
	}

	return NULL;
}

static int dispatch_start_serial_worker(struct dispatch_queue* queue)
{
	static int (*real_pthread_create_fn)(pthread_t*, const pthread_attr_t*,
	                                     void* (*)(void*), void*) = NULL;
	pthread_t worker;

	if (queue->worker_started)
		return 1;
	if (!real_pthread_create_fn)
		real_pthread_create_fn = dlsym(RTLD_NEXT, "pthread_create");
	if (!real_pthread_create_fn)
		return 0;
	if (real_pthread_create_fn(&worker, NULL, dispatch_serial_worker, queue) != 0)
		return 0;
	pthread_detach(worker);
	queue->worker_started = 1;
	return 1;
}

void* dispatch_queue_create(const char *label, void *attr)
{
	(void)attr;
	struct dispatch_queue *q = (struct dispatch_queue *)calloc(1, sizeof(*q));
	if (!q) return NULL;
	q->type = DISPATCH_OBJ_QUEUE;
	q->refcount = 1;
	q->serial = 1;
	if (label) strncpy(q->label, label, sizeof(q->label) - 1);
	pthread_mutex_init(&q->mutex, NULL);
	pthread_cond_init(&q->cond, NULL);
	if (shim_objc_msgsend_trace_enabled())
		fprintf(stderr, "libsystem_shim: dispatch_queue_create(%s) -> %p\n",
		        label ? label : "(nil)", q);
	return q;
}

/* ---- Async dispatch (worker thread pool) / sync (inline) ---- */

struct machgate_dispatch_source {
	void* type;
	uintptr_t handle;
	uintptr_t mask;
	void* queue;
	void* event_handler;
	void* cancel_handler;
	int resumed;
	int cancelled;
};

const char _dispatch_source_type_mach_recv[] = "mach_recv";

static void dispatch_invoke_block(void* block)
{
	if (!block)
		return;

	void** block_words = (void**)block;
	void (*invoke)(void*) = (void (*)(void*))block_words[2];
	if (invoke)
		invoke(block);
}

static void dispatch_run_block(void* block)
{
	struct block_layout* layout = block;

	if (!block)
		return;
	if (shim_trace_enabled())
		fprintf(stderr, "libsystem_shim: dispatch run block=%p invoke=%p\n",
		        block, (void*)(uintptr_t)layout->invoke);
	if (layout->invoke)
		layout->invoke(block);
	_Block_release(block);
}

static pthread_mutex_t dispatch_pool_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t dispatch_pool_cond = PTHREAD_COND_INITIALIZER;
static struct dispatch_work_item* dispatch_pool_head;
static struct dispatch_work_item* dispatch_pool_tail;
static int dispatch_pool_started;

#define DISPATCH_POOL_WORKER_COUNT 4

static void* dispatch_pool_worker(void* arg)
{
	(void)arg;
	for (;;) {
		struct dispatch_work_item* item;

		pthread_mutex_lock(&dispatch_pool_mutex);
		while (!dispatch_pool_head)
			pthread_cond_wait(&dispatch_pool_cond, &dispatch_pool_mutex);
		item = dispatch_pool_head;
		dispatch_pool_head = item->next;
		if (!dispatch_pool_head)
			dispatch_pool_tail = NULL;
		pthread_mutex_unlock(&dispatch_pool_mutex);

		dispatch_run_item(item);
		free(item);
	}

	return NULL;
}

static void dispatch_pool_start_workers(void)
{
	static int (*real_pthread_create_fn)(pthread_t*, const pthread_attr_t*,
	                                     void* (*)(void*), void*) = NULL;
	int index;

	if (dispatch_pool_started)
		return;
	if (!real_pthread_create_fn)
		real_pthread_create_fn = dlsym(RTLD_NEXT, "pthread_create");
	if (!real_pthread_create_fn)
		return;

	pthread_mutex_lock(&dispatch_pool_mutex);
	if (dispatch_pool_started) {
		pthread_mutex_unlock(&dispatch_pool_mutex);
		return;
	}
	dispatch_pool_started = 1;
	for (index = 0; index < DISPATCH_POOL_WORKER_COUNT; index++) {
		pthread_t worker;
		real_pthread_create_fn(&worker, NULL, dispatch_pool_worker, NULL);
		pthread_detach(worker);
	}
	pthread_mutex_unlock(&dispatch_pool_mutex);
}

static void dispatch_pool_post(struct dispatch_work_item* item)
{
	pthread_mutex_lock(&dispatch_pool_mutex);
	if (dispatch_pool_tail)
		dispatch_pool_tail->next = item;
	else
		dispatch_pool_head = item;
	dispatch_pool_tail = item;
	pthread_cond_signal(&dispatch_pool_cond);
	pthread_mutex_unlock(&dispatch_pool_mutex);
}

void dispatch_async(void *queue, void *block)
{
	struct dispatch_queue* serial = queue;
	struct dispatch_work_item* item;
	void* owned;

	if (shim_objc_msgsend_trace_enabled())
		fprintf(stderr,
		        "libsystem_shim: dispatch_async(queue=%p, block=%p) caller=%p\n",
		        queue, block, MACHGATE_SHIM_CALLER());

	if (shim_trace_enabled() && serial && serial->type == DISPATCH_OBJ_QUEUE)
		fprintf(stderr, "libsystem_shim: dispatch_async queue=%s block=%p\n",
		        serial->label, block);

	if (queue == &_dispatch_main_q || queue == &_dispatch_global_default)
		serial = NULL;

	owned = block_copy_internal(block);
	if (!owned) {
		dispatch_invoke_block(block);
		return;
	}

	if (serial && serial->type == DISPATCH_OBJ_QUEUE && serial->serial &&
	    dispatch_start_serial_worker(serial)) {
		item = malloc(sizeof(*item));
		if (!item) {
			dispatch_run_block(owned);
			return;
		}
		item->block = owned;
		item->context = NULL;
		item->function = NULL;
		item->next = NULL;
		pthread_mutex_lock(&serial->mutex);
		if (serial->tail)
			serial->tail->next = item;
		else
			serial->head = item;
		serial->tail = item;
		pthread_cond_signal(&serial->cond);
		pthread_mutex_unlock(&serial->mutex);
		return;
	}

	dispatch_pool_start_workers();
	if (!dispatch_pool_started) {
		dispatch_run_block(owned);
		return;
	}

	item = malloc(sizeof(*item));
	if (!item) {
		dispatch_run_block(owned);
		return;
	}
	item->block = owned;
	item->context = NULL;
	item->function = NULL;
	item->next = NULL;
	dispatch_pool_post(item);
}

void dispatch_sync(void *queue, void *block)
{
	(void)queue;
	dispatch_invoke_block(block);
}

void dispatch_async_f(void *queue, void *context, dispatch_function_t function)
{
	struct dispatch_queue* serial = queue;

	if (serial && serial->type == DISPATCH_OBJ_QUEUE && serial->serial &&
	    dispatch_start_serial_worker(serial)) {
		struct dispatch_work_item* item = malloc(sizeof(*item));
		if (!item) {
			if (function)
				function(context);
			return;
		}
		item->block = NULL;
		item->context = context;
		item->function = function;
		item->next = NULL;
		pthread_mutex_lock(&serial->mutex);
		if (serial->tail)
			serial->tail->next = item;
		else
			serial->head = item;
		serial->tail = item;
		pthread_cond_signal(&serial->cond);
		pthread_mutex_unlock(&serial->mutex);
		return;
	}

	if (function)
		function(context);
}

void dispatch_after_f(uint64_t when, void *queue, void *context,
                     dispatch_function_t function)
{
	(void)when;
	(void)queue;
	if (function)
		function(context);
}

void dispatch_sync_f(void *queue, void *context, dispatch_function_t function)
{
	(void)queue;
	if (function)
		function(context);
}

struct machgate_dispatch_group {
	int refcount;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	long entered;
	long left;
	struct dispatch_work_item* head;
	struct dispatch_work_item* tail;
	int notify_armed;
	void* notify_queue;
	void* notify_block;
};

void* dispatch_group_create(void)
{
	struct machgate_dispatch_group* group = calloc(1, sizeof(*group));
	if (!group)
		return NULL;
	pthread_mutex_init(&group->mutex, NULL);
	pthread_cond_init(&group->cond, NULL);
	return group;
}

void dispatch_group_enter(void* group_ref)
{
	struct machgate_dispatch_group* group = group_ref;
	if (!group)
		return;
	pthread_mutex_lock(&group->mutex);
	group->entered++;
	pthread_mutex_unlock(&group->mutex);
}

static void dispatch_group_check_notify(struct machgate_dispatch_group* group)
{
	void* block;
	void* queue;

	if (group->notify_armed && group->entered == group->left) {
		block = group->notify_block;
		queue = group->notify_queue;
		group->notify_armed = 0;
		group->notify_block = NULL;
		group->notify_queue = NULL;
		if (block)
			dispatch_async(queue, block);
	}
}

void dispatch_group_leave(void* group_ref)
{
	struct machgate_dispatch_group* group = group_ref;
	if (!group)
		return;
	pthread_mutex_lock(&group->mutex);
	group->left++;
	pthread_cond_broadcast(&group->cond);
	dispatch_group_check_notify(group);
	pthread_mutex_unlock(&group->mutex);
}

void dispatch_group_notify(void* group_ref, void* queue, void* block)
{
	struct machgate_dispatch_group* group = group_ref;
	if (!group || !block)
		return;
	pthread_mutex_lock(&group->mutex);
	group->notify_queue = queue;
	group->notify_block = block_copy_internal(block);
	group->notify_armed = 1;
	dispatch_group_check_notify(group);
	pthread_mutex_unlock(&group->mutex);
}

const char* dispatch_queue_get_label(void* queue)
{
	struct dispatch_queue* serial = queue;
	if (serial && serial->type == DISPATCH_OBJ_QUEUE)
		return serial->label;
	return NULL;
}

void* dispatch_queue_create_with_target(const char* label, void* attr,
                                         void* target)
{
	(void)target;
	return dispatch_queue_create(label, attr);
}

void dispatch_set_context(void* object, void* context)
{
	struct dispatch_queue* serial = object;
	if (serial && serial->type == DISPATCH_OBJ_QUEUE)
		;
	(void)context;
}

void dispatch_set_finalizer_f(void* object, dispatch_function_t finalizer)
{
	(void)object;
	(void)finalizer;
}

void* dispatch_get_context(void* queue)
{
	(void)queue;
	return NULL;
}

void dispatch_once(long *predicate, void *block)
{
	if (__sync_bool_compare_and_swap(predicate, 0, 1)) {
		dispatch_invoke_block(block);
		__sync_synchronize();
		*predicate = ~0L;
	} else {
		while (*predicate != ~0L)
			sched_yield();
	}
}

void dispatch_once_f(long* predicate, void* context,
                     dispatch_function_t function)
{
	if (__sync_bool_compare_and_swap(predicate, 0, 1)) {
		if (function)
			function(context);
		__sync_synchronize();
		*predicate = ~0L;
		return;
	}

	while (*predicate != ~0L)
		sched_yield();
}

void* dispatch_source_create(const void* type, uintptr_t handle,
                             uintptr_t mask, void* queue)
{
	struct machgate_dispatch_source* source = calloc(1, sizeof(*source));
	if (!source)
		return NULL;
	source->type = (void*)type;
	source->handle = handle;
	source->mask = mask;
	source->queue = queue;
	return source;
}

void dispatch_source_set_event_handler(void* source_ref, void* handler)
{
	struct machgate_dispatch_source* source = source_ref;
	if (source)
		source->event_handler = block_copy_internal(handler);
}

void dispatch_source_set_timer(void* source_ref, uint64_t start,
                              uint64_t interval, uint64_t leeway)
{
	(void)source_ref;
	(void)start;
	(void)interval;
	(void)leeway;
}

void dispatch_source_set_cancel_handler(void* source_ref, void* handler)
{
	struct machgate_dispatch_source* source = source_ref;
	if (!source)
		return;
	source->cancel_handler = block_copy_internal(handler);
	if (source->cancelled)
		dispatch_invoke_block(source->cancel_handler);
}

void dispatch_source_cancel(void* source_ref)
{
	struct machgate_dispatch_source* source = source_ref;
	if (!source)
		return;
	if (source->cancelled)
		return;
	source->cancelled = 1;
	if (source->cancel_handler)
		dispatch_invoke_block(source->cancel_handler);
}

void dispatch_after(uint64_t when, void* queue, void* block)
{
	(void)when;
	dispatch_async(queue, block);
}

void dispatch_resume(void* object)
{
	struct machgate_dispatch_source* source = object;
	if (source && source->event_handler)
		dispatch_invoke_block(source->event_handler);
}

void _os_signpost_emit_with_name_impl(void* dso, void* log, uint32_t type,
                                      uint64_t signpost_id, const char* name,
                                      const char* format, ...)
{
	(void)dso;
	(void)log;
	(void)type;
	(void)signpost_id;
	(void)name;
	(void)format;
}

void* os_log_create(const char* subsystem, const char* category)
{
	(void)subsystem;
	(void)category;
	return (void*)"MachGate/os_log";
}

uint8_t os_signpost_enabled(void* log)
{
	(void)log;
	return 0;
}

uint8_t os_log_type_enabled(void* log, uint8_t type)
{
	(void)log;
	(void)type;
	return 0;
}

void os_unfair_lock_lock(uint32_t* lock)
{
	if (!lock)
		return;
	while (!__sync_bool_compare_and_swap(lock, 0, 1))
		sched_yield();
}

uint8_t os_unfair_lock_trylock(uint32_t* lock)
{
	if (!lock)
		return 0;
	return __sync_bool_compare_and_swap(lock, 0, 1) ? 1 : 0;
}

void os_unfair_lock_unlock(uint32_t* lock)
{
	if (lock)
		__sync_lock_release(lock);
}

void os_unfair_lock_assert_owner(uint32_t* lock)
{
	(void)lock;
}

int OSAtomicCompareAndSwap64(int64_t old_value, int64_t new_value,
                             volatile int64_t* address)
{
	if (!address)
		return 0;
	return __sync_bool_compare_and_swap(address, old_value, new_value) ? 1 : 0;
}

int64_t OSAtomicAdd64(int64_t amount, volatile int64_t* address)
{
	if (!address)
		return 0;
	return __sync_fetch_and_add(address, amount) + amount;
}

int64_t OSAtomicIncrement64(volatile int64_t* address)
{
	return OSAtomicAdd64(1, address);
}

int64_t OSAtomicDecrement64(volatile int64_t* address)
{
	return OSAtomicAdd64(-1, address);
}

#define DARWIN_UL_OPCODE_MASK 0x000000ffU
#define DARWIN_ULF_NO_ERRNO 0x01000000U
#define DARWIN_ULF_WAKE_ALL 0x00000100U

static int ulock_result(int result, uint32_t operation)
{
	if (result == 0)
		return 0;
	if (operation & DARWIN_ULF_NO_ERRNO)
		return -shim_errno_from_linux(errno ? errno : EIO);
	return -1;
}

int __ulock_wait2(uint32_t operation, void* address, uint64_t value,
                  uint64_t timeout, uint64_t value2)
{
	(void)value2;

	if (!address) {
		errno = EFAULT;
		return ulock_result(-1, operation);
	}

	uint32_t opcode = operation & DARWIN_UL_OPCODE_MASK;
	int futex_op = FUTEX_WAIT_PRIVATE;
	const struct timespec* timeout_ptr = NULL;
	struct timespec timeout_value;

	if (timeout) {
		timeout_value.tv_sec = (time_t)(timeout / 1000000000ULL);
		timeout_value.tv_nsec = (long)(timeout % 1000000000ULL);
		timeout_ptr = &timeout_value;
	}

	long result;
	if (opcode == 5 || opcode == 6) {
		result = syscall(SYS_futex, address, futex_op, value,
		                 timeout_ptr, NULL, 0);
	} else {
		result = syscall(SYS_futex, address, futex_op, (uint32_t)value,
		                 timeout_ptr, NULL, 0);
	}

	if (result == 0)
		return 0;
	return ulock_result(-1, operation);
}

int __ulock_wait(uint32_t operation, void* address, uint64_t value,
                 uint32_t timeout)
{
	uint64_t timeout_ns = (uint64_t)timeout * 1000ULL;
	return __ulock_wait2(operation, address, value, timeout_ns, 0);
}

int __ulock_wake(uint32_t operation, void* address, uint64_t wake_value)
{
	if (!address) {
		errno = EFAULT;
		return ulock_result(-1, operation);
	}

	int wake_count = 1;
	if ((operation & DARWIN_ULF_WAKE_ALL) || wake_value > INT32_MAX)
		wake_count = INT32_MAX;
	else if (wake_value > 0)
		wake_count = (int)wake_value;

	long result = syscall(SYS_futex, address, FUTEX_WAKE_PRIVATE, wake_count,
	                      NULL, NULL, 0);
	if (result >= 0)
		return 0;
	return ulock_result(-1, operation);
}

/* ---- Dispatch time ---- */

#define DISPATCH_TIME_NOW     0ULL
#define DISPATCH_TIME_FOREVER (~0ULL)

uint64_t dispatch_time(uint64_t when, int64_t delta)
{
	if (when == DISPATCH_TIME_FOREVER)
		return DISPATCH_TIME_FOREVER;
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	uint64_t now = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
	if (when == DISPATCH_TIME_NOW)
		when = now;
	return when + (uint64_t)delta;
}

uint64_t dispatch_walltime(const struct timespec *when, int64_t delta)
{
	uint64_t t;
	if (when) {
		t = (uint64_t)when->tv_sec * 1000000000ULL + (uint64_t)when->tv_nsec;
	} else {
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		t = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
	}
	return t + (uint64_t)delta;
}

/* ---- Dispatch data (stub) ---- */

struct dispatch_data_obj {
	int          type;       /* DISPATCH_OBJ_DATA */
	int          refcount;
	const void  *buf;
	size_t       size;
};

void* dispatch_data_create(const void *buffer, size_t size,
                           void *queue, void *destructor)
{
	(void)queue; (void)destructor;
	/* Carry the (buffer,size) so consumers (e.g. the Gothic objc shim's
	 * newLibraryWithData:) can recover the bytes; the old stub returned NULL,
	 * which discarded them. The buffer is borrowed (engine-owned), not copied. */
	struct dispatch_data_obj *o = (struct dispatch_data_obj *)calloc(1, sizeof(*o));
	if (!o) return NULL;
	o->type = DISPATCH_OBJ_DATA;
	o->refcount = 1;
	o->buf = buffer;
	o->size = size;
	return o;
}

/* Recover the bytes from a dispatch_data created above; NULL if `dd` is not one
 * of ours. Exported (loader is -rdynamic) for the Gothic shim to dlsym. */
const void *machgate_dispatch_data_bytes(void *dd, size_t *out_size)
{
	if (!dd) return NULL;
	struct dispatch_data_obj *o = (struct dispatch_data_obj *)dd;
	if (o->type != DISPATCH_OBJ_DATA) return NULL;
	if (out_size) *out_size = o->size;
	return o->buf;
}

/* ---- Retain/release ---- */

void dispatch_release(void *object)
{
	if (!object) return;
	struct dispatch_object *obj = (struct dispatch_object *)object;
	if (obj->refcount > 99) return; /* global queues, never free */
	if (__sync_sub_and_fetch(&obj->refcount, 1) <= 0) {
		if (obj->type == DISPATCH_OBJ_SEMAPHORE) {
			struct dispatch_semaphore *ds = (struct dispatch_semaphore *)object;
			sem_destroy(&ds->sem);
		}
		free(object);
	}
}

void dispatch_retain(void *object)
{
	if (!object) return;
	struct dispatch_object *obj = (struct dispatch_object *)object;
	__sync_add_and_fetch(&obj->refcount, 1);
}

/* ===== Thread-local variable support ===== */

#define SHIM_TLV_TERM_MAX 512
#define SHIM_TSD_DESTRUCTOR_PASSES 4

struct shim_tlv_term_entry {
	void (*func)(void*);
	void* obj;
};

static __thread struct shim_tlv_term_entry shim_tlv_term_entries[SHIM_TLV_TERM_MAX];
static __thread int shim_tlv_term_count;

void _tlv_atexit(void (*func)(void *), void *arg)
{
	if (shim_tlv_term_count >= SHIM_TLV_TERM_MAX)
		return;
	shim_tlv_term_entries[shim_tlv_term_count].func = func;
	shim_tlv_term_entries[shim_tlv_term_count].obj = arg;
	shim_tlv_term_count++;
}

static void shim_run_tlv_term_funcs(void)
{
	while (shim_tlv_term_count > 0) {
		shim_tlv_term_count--;
		shim_tlv_term_entries[shim_tlv_term_count].func(
			shim_tlv_term_entries[shim_tlv_term_count].obj);
	}
}

static void shim_run_thread_tsd_destructors(void)
{
	for (int pass = 0; pass < SHIM_TSD_DESTRUCTOR_PASSES; pass++) {
		int rerun = 0;
		for (int index = 0; index < DARWIN_TSD_KEY_COUNT; index++) {
			void* value = darwin_tsd_values[index];
			if (!value)
				continue;
			darwin_tsd_values[index] = NULL;
			if (darwin_tsd_destructors[index]) {
				darwin_tsd_destructors[index](value);
				if (darwin_tsd_values[index])
					rerun = 1;
			}
		}
		if (!rerun)
			break;
	}
}

/*
 * Mach-O thread-local variable (TLV) bootstrap.
 *
 * Mach-O TLV descriptors have the layout:
 *   struct TLVDescriptor {
 *       void* (*thunk)(TLVDescriptor*);  // this function
 *       unsigned long key;                // pthread_key_t (0 = uninitialized)
 *       unsigned long offset;             // offset within TLV image
 *   };
 *
 * The thunk is called when a TLV is first accessed. It must return
 * a pointer to per-thread storage for the variable at `offset`.
 * We allocate a per-thread block using pthread keys and copy the
 * initial TLV image from __thread_data on first use.
 */

/* TLV image info — set by machgate before execution */
void* __tlv_image_base = NULL;   /* base of __DATA,__thread_data */
size_t __tlv_image_size = 0;     /* size of __thread_data */
size_t __tlv_bss_size = 0;       /* size of __thread_bss (after thread_data) */

struct tlv_descriptor {
	void* (*thunk)(struct tlv_descriptor*);
	unsigned long key;
	unsigned long offset;
};

static pthread_mutex_t tlv_mutex = PTHREAD_MUTEX_INITIALIZER;

#define TLV_BLOCK_PREFIX 16

static void tlv_destructor(void* block)
{
	free((char*)block - TLV_BLOCK_PREFIX);
}

static char* tlv_allocate_block(size_t capacity)
{
	char* storage = calloc(1, TLV_BLOCK_PREFIX + capacity);
	if (!storage)
		return NULL;
	*(size_t*)storage = capacity;
	if (__tlv_image_base && __tlv_image_size > 0)
		memcpy(storage + TLV_BLOCK_PREFIX, __tlv_image_base, __tlv_image_size);
	return storage + TLV_BLOCK_PREFIX;
}

void* _tlv_bootstrap_impl(struct tlv_descriptor* desc)
{
	if (desc->key == 0) {
		pthread_mutex_lock(&tlv_mutex);
		if (desc->key == 0) {
			pthread_key_t key;
			shim_tsd_allocate_key(&key, tlv_destructor);
			desc->key = (unsigned long)key + 1;
		}
		pthread_mutex_unlock(&tlv_mutex);
	}

	pthread_key_t key = (pthread_key_t)(desc->key - 1);
	char* block = shim_tsd_get(key);

	size_t needed = __tlv_image_size + __tlv_bss_size;
	if (needed < desc->offset + 16)
		needed = desc->offset + 16;
	if (needed < 4096) needed = 4096;

	if (!block) {
		block = tlv_allocate_block(needed);
		if (!block)
			return NULL;
		shim_tsd_set(key, block);
	} else {
		size_t capacity = *(size_t*)(block - TLV_BLOCK_PREFIX);
		if (capacity < needed) {
			char* grown = tlv_allocate_block(needed);
			if (!grown)
				return NULL;
			memcpy(grown, block, capacity);
			free(block - TLV_BLOCK_PREFIX);
			shim_tsd_set(key, grown);
			block = grown;
		}
	}

	return block + desc->offset;
}

#if defined(__aarch64__)
__asm__(
	".text\n"
	".global _tlv_bootstrap\n"
	".type _tlv_bootstrap, %function\n"
	"_tlv_bootstrap:\n"
	"sub sp, sp, #160\n"
	"stp x1, x2, [sp]\n"
	"stp x3, x4, [sp, #16]\n"
	"stp x5, x6, [sp, #32]\n"
	"stp x7, x8, [sp, #48]\n"
	"stp x9, x10, [sp, #64]\n"
	"stp x11, x12, [sp, #80]\n"
	"stp x13, x14, [sp, #96]\n"
	"stp x15, x16, [sp, #112]\n"
	"stp x17, x18, [sp, #128]\n"
	"str x30, [sp, #144]\n"
	"bl _tlv_bootstrap_impl\n"
	"ldr x30, [sp, #144]\n"
	"ldp x17, x18, [sp, #128]\n"
	"ldp x15, x16, [sp, #112]\n"
	"ldp x13, x14, [sp, #96]\n"
	"ldp x11, x12, [sp, #80]\n"
	"ldp x9, x10, [sp, #64]\n"
	"ldp x7, x8, [sp, #48]\n"
	"ldp x5, x6, [sp, #32]\n"
	"ldp x3, x4, [sp, #16]\n"
	"ldp x1, x2, [sp]\n"
	"add sp, sp, #160\n"
	"ret\n"
);
#else
void* _tlv_bootstrap(struct tlv_descriptor* desc)
{
	return _tlv_bootstrap_impl(desc);
}
#endif

/* ===== Darwin-suffixed symbol variants ===== */

/* Some symbols in the binary have $DARWIN_EXTSN suffix. The resolver strips
 * these suffixes during lookup, so fopen$DARWIN_EXTSN maps to fopen.
 * No action needed here — they resolve through normal glibc. */

/* ===== vfork ===== */

/* vfork is deprecated but available on Linux. We redirect to fork for safety. */
pid_t vfork(void)
{
	return fork();
}

/* ===== setjmp ===== */

#if defined(__aarch64__)
__asm__(
".text\n"
".global sigsetjmp\n"
".type sigsetjmp, %function\n"
"sigsetjmp:\n"
"	b __sigsetjmp\n"
".size sigsetjmp, .-sigsetjmp\n"
);
#endif

/* ===== 128-bit division ===== */

/* __udivti3 is provided by libgcc / compiler-rt.
 * It should resolve through normal linking. No stub needed. */

/* ===== C++ exception unwinding ===== */

/* _Unwind_Resume is imported by the Mach-O from libSystem.B.dylib.
 * On macOS, libSystem re-exports it from libunwind. On Linux, it's
 * in libgcc_s.so.1. We forward to the real implementation via dlsym
 * so the resolver can find it when resolving libSystem symbols. */
void _Unwind_Resume(void* exception_object)
{
	static void (*real_unwind_resume)(void*) = NULL;
	if (!real_unwind_resume) {
		void* libgcc = dlopen("libgcc_s.so.1", RTLD_NOW | RTLD_GLOBAL);
		if (libgcc)
			real_unwind_resume = dlsym(libgcc, "_Unwind_Resume");
		if (!real_unwind_resume)
			real_unwind_resume = dlsym(RTLD_NEXT, "_Unwind_Resume");
	}
	if (real_unwind_resume && real_unwind_resume != _Unwind_Resume)
		real_unwind_resume(exception_object);
	abort();
}

/* ===== File operation ABI translation ===== */

/*
 * macOS and Linux assign entirely different bit values to O_* flags,
 * AT_* flags, and some fcntl() command numbers. Without translation:
 *   - Darwin O_CREAT (0x0200) becomes Linux O_TRUNC — corrupting files
 *   - Darwin O_NONBLOCK (0x0004) is meaningless on Linux (unused bit)
 *   - Darwin AT_FDCWD (-2) != Linux AT_FDCWD (-100)
 *   - Darwin AT_SYMLINK_NOFOLLOW (0x0020) != Linux (0x0100)
 *
 * We translate at every entry point where Mach-O code passes these values.
 */

/* ---- Darwin O_* flag values (XNU bsd/sys/fcntl.h) ---- */
#define DARWIN_O_NONBLOCK    0x00000004
#define DARWIN_O_APPEND      0x00000008
#define DARWIN_O_SHLOCK      0x00000010  /* no Linux equivalent */
#define DARWIN_O_EXLOCK      0x00000020  /* no Linux equivalent */
#define DARWIN_O_ASYNC       0x00000040
#define DARWIN_O_NOFOLLOW    0x00000100
#define DARWIN_O_CREAT       0x00000200
#define DARWIN_O_TRUNC       0x00000400
#define DARWIN_O_EXCL        0x00000800
#define DARWIN_O_EVTONLY     0x00008000  /* no Linux equivalent */
#define DARWIN_O_NOCTTY      0x00020000
#define DARWIN_O_DIRECTORY   0x00100000
#define DARWIN_O_SYMLINK     0x00200000  /* no Linux equivalent */
#define DARWIN_O_DSYNC       0x00400000
#define DARWIN_O_CLOEXEC     0x01000000

/* ---- Darwin AT_* flag values (XNU bsd/sys/fcntl.h) ---- */
#define DARWIN_AT_FDCWD              (-2)
#define DARWIN_AT_EACCESS            0x0010
#define DARWIN_AT_SYMLINK_NOFOLLOW   0x0020
#define DARWIN_AT_SYMLINK_FOLLOW     0x0040
#define DARWIN_AT_REMOVEDIR          0x0080

/* ---- Darwin fcntl commands that differ from Linux ----
 * F_DUPFD(0), F_GETFD(1), F_SETFD(2), F_GETFL(3), F_SETFL(4) are the
 * same on both platforms. The rest are renumbered or macOS-only. */
#define DARWIN_F_GETOWN     5   /* Linux: 9 */
#define DARWIN_F_SETOWN     6   /* Linux: 8 */
#define DARWIN_F_GETLK      7   /* Linux: 5 */
#define DARWIN_F_SETLK      8   /* Linux: 6 */
#define DARWIN_F_SETLKW     9   /* Linux: 7 */
#define DARWIN_F_RDADVISE   44  /* no Linux equivalent */
#define DARWIN_F_RDAHEAD    45  /* no Linux equivalent */
#define DARWIN_F_NOCACHE    48  /* no Linux equivalent */
#define DARWIN_F_GETPATH    50  /* implement via /proc */
#define DARWIN_F_FULLFSYNC  51  /* implement via fsync */
#define DARWIN_F_DUPFD_CLOEXEC 67
#define DARWIN_F_PREALLOCATE 42  /* implement via fallocate/ftruncate */

#define DARWIN_FIOCLEX      0x20006601u
#define DARWIN_FIONCLEX     0x20006602u
#define DARWIN_FIONREAD     0x4004667fu
#define DARWIN_FIONBIO      0x8004667eu
#define DARWIN_TIOCOUTQ     0x40047473u
#define DARWIN_TIOCSWINSZ   0x80087467u
#define DARWIN_TIOCGWINSZ   0x40087468u

#define DARWIN_RENAME_SWAP 0x00000002u
#define DARWIN_RENAME_EXCL 0x00000004u

#ifndef RENAME_NOREPLACE
#define RENAME_NOREPLACE (1U << 0)
#endif

#ifndef RENAME_EXCHANGE
#define RENAME_EXCHANGE (1U << 1)
#endif

/* ---- Translation helpers ---- */

struct flag_pair { int darwin; int linux_val; };

static const struct flag_pair oflags_map[] = {
	{ DARWIN_O_NONBLOCK,  O_NONBLOCK  },
	{ DARWIN_O_APPEND,    O_APPEND    },
	{ DARWIN_O_ASYNC,     FASYNC      },
	{ DARWIN_O_NOFOLLOW,  O_NOFOLLOW  },
	{ DARWIN_O_CREAT,     O_CREAT     },
	{ DARWIN_O_TRUNC,     O_TRUNC     },
	{ DARWIN_O_EXCL,      O_EXCL      },
	{ DARWIN_O_NOCTTY,    O_NOCTTY    },
	{ DARWIN_O_DIRECTORY, O_DIRECTORY },
	{ DARWIN_O_DSYNC,     O_DSYNC     },
	{ DARWIN_O_CLOEXEC,   O_CLOEXEC   },
	{ 0, 0 }
};

/* Darwin O_flags → Linux O_flags */
static int translate_oflags(int darwin_flags)
{
	/* Access mode bits (O_RDONLY=0, O_WRONLY=1, O_RDWR=2) are identical */
	int linux_flags = darwin_flags & O_ACCMODE;

	for (const struct flag_pair *p = oflags_map; p->darwin; p++) {
		if (darwin_flags & p->darwin)
			linux_flags |= p->linux_val;
	}

	/* Silently dropped Darwin-only flags:
	 *   O_SHLOCK/O_EXLOCK — BSD advisory lock at open time
	 *   O_EVTONLY — kqueue event-only descriptor
	 *   O_SYMLINK — open symlink itself, not target */
	return linux_flags;
}

/* Linux O_flags → Darwin O_flags (for fcntl F_GETFL) */
static int translate_oflags_to_darwin(int linux_flags)
{
	int darwin_flags = linux_flags & O_ACCMODE;

	for (const struct flag_pair *p = oflags_map; p->darwin; p++) {
		if (linux_flags & p->linux_val)
			darwin_flags |= p->darwin;
	}

	return darwin_flags;
}

static const struct flag_pair atflags_map[] = {
	{ DARWIN_AT_EACCESS,          AT_EACCESS          },
	{ DARWIN_AT_SYMLINK_NOFOLLOW, AT_SYMLINK_NOFOLLOW },
	{ DARWIN_AT_SYMLINK_FOLLOW,   AT_SYMLINK_FOLLOW   },
	{ DARWIN_AT_REMOVEDIR,        AT_REMOVEDIR        },
	{ 0, 0 }
};

/* Darwin AT_flags → Linux AT_flags */
static int translate_atflags(int darwin_flags)
{
	int linux_flags = 0;

	for (const struct flag_pair *p = atflags_map; p->darwin; p++) {
		if (darwin_flags & p->darwin)
			linux_flags |= p->linux_val;
	}

	return linux_flags;
}

/* Darwin AT_FDCWD (-2) → Linux AT_FDCWD (-100) */
static int translate_dirfd(int darwin_dirfd)
{
	return (darwin_dirfd == DARWIN_AT_FDCWD) ? AT_FDCWD : darwin_dirfd;
}

/* ---- open() / openat() ---- */

/*
 * Darwin passes a variadic function's unnamed arguments on the caller stack,
 * starting at the incoming [sp]. Linux AAPCS64 also passes unnamed arguments
 * in registers x2..x7 (saved by va_start) before spilling to the stack, so a
 * Linux-compiled va_arg reads the register save area first and never sees the
 * Darwin caller's stack slot — open(path, O_CREAT, 0644) arrived with mode=002
 * (stale x2) and created 0000-mode files, breaking NamedMutex and every guest
 * that re-opened one (EACCES -> throw).
 *
 * Naming x2..x7 as dummy parameters consumes the register save area, so the
 * first va_arg lands exactly on the Darwin caller's first stack argument.
 * Internal shim callers use real_open/real_openat, which take the mode as a
 * fixed parameter under the Linux convention.
 */

static int (*real_open_libc)(const char*, int, ...) = NULL;
static int (*real_openat_libc)(int, const char*, int, ...) = NULL;

static int libc_open(const char* pathname, int linux_flags, mode_t mode)
{
	if (!real_open_libc)
		real_open_libc = dlsym(RTLD_NEXT, "open");
	if (!real_open_libc)
		return -1;
	return real_open_libc(pathname, linux_flags, mode);
}

static int libc_openat(int dirfd, const char* pathname, int linux_flags, mode_t mode)
{
	if (!real_openat_libc)
		real_openat_libc = dlsym(RTLD_NEXT, "openat");
	if (!real_openat_libc)
		return -1;
	return real_openat_libc(dirfd, pathname, linux_flags, mode);
}

static long shim_open_common(const char* pathname, int flags, mode_t mode)
{
	int linux_flags = translate_oflags(flags);
	/* aarch64 Linux has no SYS_open; everything goes through SYS_openat */
	return syscall(SYS_openat, AT_FDCWD, pathname, linux_flags, mode);
}

static long shim_openat_common(int dirfd, const char* pathname, int flags, mode_t mode)
{
	int linux_flags = translate_oflags(flags);
	return syscall(SYS_openat, translate_dirfd(dirfd), pathname,
	               linux_flags, mode);
}

int shim_open(const char *pathname, int flags,
              long darwin_stack_area_do_not_use_0,
              long darwin_stack_area_do_not_use_1,
              long darwin_stack_area_do_not_use_2,
              long darwin_stack_area_do_not_use_3,
              long darwin_stack_area_do_not_use_4,
              long darwin_stack_area_do_not_use_5,
              ...) __asm__("open");
int shim_open(const char *pathname, int flags,
              long darwin_stack_area_do_not_use_0,
              long darwin_stack_area_do_not_use_1,
              long darwin_stack_area_do_not_use_2,
              long darwin_stack_area_do_not_use_3,
              long darwin_stack_area_do_not_use_4,
              long darwin_stack_area_do_not_use_5,
              ...)
{
	mode_t mode = 0;

	if (flags & DARWIN_O_CREAT) {
		va_list args;
		va_start(args, darwin_stack_area_do_not_use_5);
		mode = va_arg(args, mode_t);
		va_end(args);
	}

	return shim_open_common(pathname, flags, mode);
}

int shim_openat(int dirfd, const char *pathname, int flags,
               long darwin_stack_area_do_not_use_0,
               long darwin_stack_area_do_not_use_1,
               long darwin_stack_area_do_not_use_2,
               long darwin_stack_area_do_not_use_3,
               long darwin_stack_area_do_not_use_4,
               long darwin_stack_area_do_not_use_5,
               ...) __asm__("openat");
int shim_openat(int dirfd, const char *pathname, int flags,
               long darwin_stack_area_do_not_use_0,
               long darwin_stack_area_do_not_use_1,
               long darwin_stack_area_do_not_use_2,
               long darwin_stack_area_do_not_use_3,
               long darwin_stack_area_do_not_use_4,
               long darwin_stack_area_do_not_use_5,
               ...)
{
	mode_t mode = 0;

	if (flags & DARWIN_O_CREAT) {
		va_list args;
		va_start(args, darwin_stack_area_do_not_use_5);
		mode = va_arg(args, mode_t);
		va_end(args);
	}

	return shim_openat_common(dirfd, pathname, flags, mode);
}

int __renameatx_np(int oldfd, const char* old_path, int newfd,
                   const char* new_path, unsigned int flags)
{
	unsigned int linux_flags = 0;

	if (flags & DARWIN_RENAME_SWAP)
		linux_flags |= RENAME_EXCHANGE;
	if (flags & DARWIN_RENAME_EXCL)
		linux_flags |= RENAME_NOREPLACE;

	if (flags & ~(DARWIN_RENAME_SWAP | DARWIN_RENAME_EXCL)) {
		errno = ENOTSUP;
		return -1;
	}

	if (linux_flags)
		return syscall(SYS_renameat2, translate_dirfd(oldfd), old_path,
		               translate_dirfd(newfd), new_path, linux_flags);

	return syscall(SYS_renameat, translate_dirfd(oldfd), old_path,
	               translate_dirfd(newfd), new_path);
}

int renameatx_np(int oldfd, const char* old_path, int newfd,
                 const char* new_path, unsigned int flags)
{
	return __renameatx_np(oldfd, old_path, newfd, new_path, flags);
}

/* ---- fcntl() ---- */

/* ---- struct flock layout translation (Darwin vs Linux) ----
 * Darwin (XNU bsd/sys/fcntl.h, LP64):            Linux (aarch64 glibc):
 *   off_t  l_start;   +0                           short  l_type;    +0
 *   off_t  l_len;     +8                           short  l_whence;  +2
 *   pid_t  l_pid;     +16                          off_t  l_start;    +8
 *   short  l_type;    +20                          off_t  l_len;     +16
 *   short  l_whence;  +22                          pid_t  l_pid;     +24
 * Lock type values (XNU bsd/sys/fcntl.h):
 *   Darwin F_RDLCK=1 F_UNLCK=2 F_WRLCK=3;
 *   Linux  F_RDLCK=0 F_WRLCK=1 F_UNLCK=2. */
#define DARWIN_F_RDLCK 1
#define DARWIN_F_UNLCK 2
#define DARWIN_F_WRLCK 3

static short darwin_lock_type_to_linux(short darwin_type)
{
	switch (darwin_type) {
	case DARWIN_F_RDLCK: return F_RDLCK;
	case DARWIN_F_WRLCK: return F_WRLCK;
	case DARWIN_F_UNLCK: return F_UNLCK;
	default: return -1;
	}
}

static short linux_lock_type_to_darwin(short linux_type)
{
	switch (linux_type) {
	case F_RDLCK: return DARWIN_F_RDLCK;
	case F_WRLCK: return DARWIN_F_WRLCK;
	case F_UNLCK: return DARWIN_F_UNLCK;
	default: return -1;
	}
}

static int darwin_flock_to_linux(const void* darwin_flock, struct flock* linux_flock)
{
	const unsigned char* d = (const unsigned char*)darwin_flock;
	off_t l_start, l_len;
	int32_t l_pid;
	short l_type, l_whence;
	memcpy(&l_start, d + 0, 8);
	memcpy(&l_len, d + 8, 8);
	memcpy(&l_pid, d + 16, 4);
	memcpy(&l_type, d + 20, 2);
	memcpy(&l_whence, d + 22, 2);

	short linux_type = darwin_lock_type_to_linux(l_type);
	if (linux_type < 0) {
		errno = EINVAL;
		return -1;
	}

	memset(linux_flock, 0, sizeof(*linux_flock));
	linux_flock->l_type = linux_type;
	linux_flock->l_whence = l_whence;
	linux_flock->l_start = l_start;
	linux_flock->l_len = l_len;
	linux_flock->l_pid = l_pid;
	return 0;
}

static void linux_flock_to_darwin(const struct flock* linux_flock, void* darwin_flock)
{
	unsigned char* d = (unsigned char*)darwin_flock;
	short darwin_type = linux_lock_type_to_darwin(linux_flock->l_type);
	off_t l_start = linux_flock->l_start;
	off_t l_len = linux_flock->l_len;
	int32_t l_pid = (int32_t)linux_flock->l_pid;
	short l_whence = linux_flock->l_whence;

	memcpy(d + 0, &l_start, 8);
	memcpy(d + 8, &l_len, 8);
	memcpy(d + 16, &l_pid, 4);
	memcpy(d + 20, &darwin_type, 2);
	memcpy(d + 22, &l_whence, 2);
}

static int shim_fcntl_fixed(int fd, int cmd, unsigned long arg)
{
	int linux_cmd;
	switch (cmd) {
	/* Commands 0–4 (F_DUPFD through F_SETFL) are identical */
	case 0: case 1: case 2: case 3: case 4:
		linux_cmd = cmd;
		break;
	case DARWIN_F_GETOWN:   linux_cmd = F_GETOWN;  break;
	case DARWIN_F_SETOWN:   linux_cmd = F_SETOWN;  break;
	case DARWIN_F_GETLK:    linux_cmd = F_GETLK;   break;
	case DARWIN_F_SETLK:    linux_cmd = F_SETLK;   break;
	case DARWIN_F_SETLKW:   linux_cmd = F_SETLKW;  break;
	case DARWIN_F_DUPFD_CLOEXEC:
		linux_cmd = F_DUPFD_CLOEXEC;
		break;
 	case DARWIN_F_FULLFSYNC:
 		/* Best-effort: Linux fsync flushes data + metadata */
 		return fsync(fd);
	case DARWIN_F_PREALLOCATE: {
		struct darwin_fstore_shim {
			uint32_t fst_flags;
			int32_t fst_posmode;
			int64_t fst_offset;
			int64_t fst_length;
			int32_t fst_bytesalloc;
			int32_t fst_pad;
		};
		const struct darwin_fstore_shim* store =
		    (const struct darwin_fstore_shim*)arg;
		if (!store)
			return -1;
		struct stat file_stat;
		if (fstat(fd, &file_stat) != 0)
			return -1;
		if (store->fst_posmode != 3 && store->fst_posmode != 0)
			return -1;
		off_t allocate_from = store->fst_posmode == 3
		                          ? file_stat.st_size
		                          : store->fst_offset;
		off_t allocate_end = allocate_from + store->fst_length;
		if (store->fst_flags & 0x04) {
			if (allocate_end > file_stat.st_size)
				return ftruncate(fd, allocate_end);
			return 0;
		}
		return posix_fallocate(fd, allocate_from, store->fst_length);
	}
 	case DARWIN_F_NOCACHE:
	case DARWIN_F_RDADVISE:
	case DARWIN_F_RDAHEAD:
		/* No Linux equivalent; succeed silently */
		return 0;
	case DARWIN_F_GETPATH: {
		/* Implement via /proc/self/fd readlink */
		char proc_path[32];
		snprintf(proc_path, sizeof(proc_path), "/proc/self/fd/%d", fd);
		ssize_t len = readlink(proc_path, (char *)arg, 1024 - 1);
		if (len < 0) return -1;
		((char *)arg)[len] = '\0';
		return 0;
	}
	default:
		linux_cmd = cmd;
		break;
	}

	/* Translate O_flags for F_SETFL */
	if (cmd == F_SETFL)
		arg = (unsigned long)translate_oflags((int)arg);

	/* Translate struct flock for the record-lock commands. The Darwin
	 * caller's flock layout differs from Linux (see the table above), so
	 * convert in both directions; F_GETLK writes the result back into the
	 * caller's Darwin-layout struct. */
	struct flock converted_flock;
	void* darwin_flock_ptr = NULL;
	int flock_is_garbage = 0;
	if (cmd == DARWIN_F_GETLK || cmd == DARWIN_F_SETLK || cmd == DARWIN_F_SETLKW) {
		darwin_flock_ptr = (void*)arg;
		if (darwin_flock_to_linux(darwin_flock_ptr, &converted_flock) == 0) {
			arg = (unsigned long)&converted_flock;
		} else {
			flock_is_garbage = 1;
		}
	}

	int ret;
	if (flock_is_garbage)
		ret = -1;
	else
		ret = syscall(SYS_fcntl, fd, linux_cmd, arg);
	int saved_errno = errno;

	if (ret >= 0 && cmd == DARWIN_F_GETLK && darwin_flock_ptr)
		linux_flock_to_darwin(&converted_flock, darwin_flock_ptr);
	if (ret >= 0 && (cmd == F_DUPFD || cmd == DARWIN_F_DUPFD_CLOEXEC))
		remember_kqueue_dup(fd, ret);

	/* Translate O_flags back to Darwin for F_GETFL */
	if (cmd == F_GETFL && ret >= 0)
		ret = translate_oflags_to_darwin(ret);

	shim_fd_trace_log("fcntl caller=%p fd=%d cmd=%d linux_cmd=%d arg=%#lx result=%d errno=%d\n",
	                  SHIM_CALLER_RETURN_ADDRESS(), fd, cmd, linux_cmd, arg,
	                  ret, ret < 0 ? saved_errno : 0);
	if (shim_trace_enabled())
		fprintf(stderr, "libsystem_shim: fcntl(fd=%d cmd=%d->%d arg=%#lx) -> %d errno=%d\n",
		        fd, cmd, linux_cmd, arg, ret, ret < 0 ? saved_errno : 0);

	return ret;
}

/*
 * Darwin passes the variadic third argument on the caller stack (see the
 * open() note above); the dummy named parameters consume the register save
 * area so va_arg lands on the Darwin stack slot carrying the flock* / int.
 */
int shim_fcntl(int fd, int cmd,
               long darwin_stack_area_do_not_use_0,
               long darwin_stack_area_do_not_use_1,
               long darwin_stack_area_do_not_use_2,
               long darwin_stack_area_do_not_use_3,
               long darwin_stack_area_do_not_use_4,
               long darwin_stack_area_do_not_use_5,
               ...) __asm__("fcntl");
int shim_fcntl(int fd, int cmd,
               long darwin_stack_area_do_not_use_0,
               long darwin_stack_area_do_not_use_1,
               long darwin_stack_area_do_not_use_2,
               long darwin_stack_area_do_not_use_3,
               long darwin_stack_area_do_not_use_4,
               long darwin_stack_area_do_not_use_5,
               ...)
{
	va_list args;
	va_start(args, darwin_stack_area_do_not_use_5);
	unsigned long arg = va_arg(args, unsigned long);
	va_end(args);

	return shim_fcntl_fixed(fd, cmd, arg);
}

int machgate_darwin_fcntl_fixed(int fd, int cmd, unsigned long arg)
{
	return shim_fcntl_fixed(fd, cmd, arg);
}

static int translate_ioctl_request(unsigned long darwin_request,
                                   unsigned long* linux_request)
{
	switch (darwin_request) {
	case DARWIN_FIONREAD:
		*linux_request = FIONREAD;
		return 1;
	case DARWIN_FIONBIO:
		*linux_request = FIONBIO;
		return 1;
#ifdef TIOCOUTQ
	case DARWIN_TIOCOUTQ:
		*linux_request = TIOCOUTQ;
		return 1;
#endif
	case DARWIN_TIOCSWINSZ:
		*linux_request = TIOCSWINSZ;
		return 1;
	case DARWIN_TIOCGWINSZ:
		*linux_request = TIOCGWINSZ;
		return 1;
	default:
		return 0;
	}
}

int machgate_darwin_ioctl_fixed(int fd, unsigned long request,
                                unsigned long arg)
{
	unsigned long linux_request;
	int result;
	int saved_errno;

	linux_request = request;

	if (request == DARWIN_FIOCLEX) {
		result = syscall(SYS_fcntl, fd, F_SETFD, FD_CLOEXEC);
		saved_errno = errno;
	} else if (request == DARWIN_FIONCLEX) {
		result = syscall(SYS_fcntl, fd, F_SETFD, 0);
		saved_errno = errno;
	} else if (translate_ioctl_request(request, &linux_request)) {
		result = syscall(SYS_ioctl, fd, linux_request, arg);
		saved_errno = errno;
	} else {
		errno = ENOSYS;
		result = -1;
		saved_errno = errno;
		linux_request = request;
	}

	if (shim_trace_enabled())
		fprintf(stderr, "libsystem_shim: ioctl(fd=%d request=%#lx->%#lx arg=%#lx) -> %d errno=%d\n",
		        fd, request, linux_request, arg, result,
		        result < 0 ? saved_errno : 0);

	return result;
}

/* ---- shm_open() ---- */

int shim_shm_open(const char *name, int oflag, mode_t mode) __asm__("shm_open");
int shim_shm_open(const char *name, int oflag, mode_t mode)
{
	int linux_flags = translate_oflags(oflag);
	/* Forward to real shm_open via dlsym to avoid recursion */
	static int (*real_shm_open)(const char *, int, mode_t) = NULL;
	if (!real_shm_open) {
		real_shm_open = dlsym(RTLD_NEXT, "shm_open");
		if (!real_shm_open) {
			errno = ENOSYS;
			return -1;
		}
	}
	return real_shm_open(name, linux_flags, mode);
}

/* ---- dup() / dup2() / dup3() / pipe2() ---- */

int shim_dup(int fd) __asm__("dup");
int shim_dup(int fd)
{
	int result = syscall(SYS_dup, fd);
	if (result >= 0)
		remember_kqueue_dup(fd, result);
	shim_fd_trace_log("dup caller=%p fd=%d result=%d errno=%d\n",
	                  SHIM_CALLER_RETURN_ADDRESS(), fd, result,
	                  result < 0 ? errno : 0);
	if (shim_trace_enabled())
		fprintf(stderr, "libsystem_shim: dup(fd=%d) -> %d errno=%d\n",
		        fd, result, result < 0 ? errno : 0);
	return result;
}

int shim_dup2(int oldfd, int newfd) __asm__("dup2");
int shim_dup2(int oldfd, int newfd)
{
	int result;

	if (oldfd == newfd) {
		result = syscall(SYS_fcntl, oldfd, F_GETFD, 0);
		if (result >= 0)
			result = newfd;
	} else {
		result = syscall(SYS_dup3, oldfd, newfd, 0);
		if (result >= 0)
			forget_kqueue_fd(newfd);
	}
	if (result >= 0)
		remember_kqueue_dup(oldfd, result);

	shim_fd_trace_log("dup2 caller=%p oldfd=%d newfd=%d result=%d errno=%d\n",
	                  SHIM_CALLER_RETURN_ADDRESS(), oldfd, newfd, result,
	                  result < 0 ? errno : 0);
	if (shim_trace_enabled())
		fprintf(stderr, "libsystem_shim: dup2(oldfd=%d newfd=%d) -> %d errno=%d\n",
		        oldfd, newfd, result, result < 0 ? errno : 0);
	return result;
}

int shim_dup3(int oldfd, int newfd, int flags) __asm__("dup3");
int shim_dup3(int oldfd, int newfd, int flags)
{
	int result = syscall(SYS_dup3, oldfd, newfd, translate_oflags(flags));
	if (result >= 0) {
		forget_kqueue_fd(newfd);
		remember_kqueue_dup(oldfd, result);
	}
	shim_fd_trace_log("dup3 caller=%p oldfd=%d newfd=%d flags=%#x result=%d errno=%d\n",
	                  SHIM_CALLER_RETURN_ADDRESS(), oldfd, newfd, flags,
	                  result, result < 0 ? errno : 0);
	return result;
}

int shim_close(int fd) __asm__("close");
int shim_close(int fd)
{
	int result = syscall(SYS_close, fd);
	int saved_errno = errno;
	if (result == 0)
		forget_kqueue_fd(fd);
	shim_fd_trace_log("close caller=%p fd=%d result=%d errno=%d\n",
	                  SHIM_CALLER_RETURN_ADDRESS(), fd, result,
	                  result < 0 ? saved_errno : 0);
	if (shim_trace_enabled())
		fprintf(stderr, "libsystem_shim: close(fd=%d) -> %d errno=%d\n",
		        fd, result, result < 0 ? saved_errno : 0);
	errno = saved_errno;
	return result;
}

int shim_pipe(int pipefd[2]) __asm__("pipe");
int shim_pipe(int pipefd[2])
{
	int result = syscall(SYS_pipe2, pipefd, 0);
	shim_fd_trace_log("pipe caller=%p result=%d read_fd=%d write_fd=%d errno=%d\n",
	                  SHIM_CALLER_RETURN_ADDRESS(), result,
	                  result == 0 ? pipefd[0] : -1,
	                  result == 0 ? pipefd[1] : -1,
	                  result < 0 ? errno : 0);
	if (shim_trace_enabled())
		fprintf(stderr, "libsystem_shim: pipe() -> %d fds=[%d,%d] errno=%d\n",
		        result, result == 0 ? pipefd[0] : -1,
		        result == 0 ? pipefd[1] : -1, result < 0 ? errno : 0);
	return result;
}

int shim_pipe2(int pipefd[2], int flags) __asm__("pipe2");
int shim_pipe2(int pipefd[2], int flags)
{
	int result = syscall(SYS_pipe2, pipefd, translate_oflags(flags));
	shim_fd_trace_log("pipe2 caller=%p flags=%#x result=%d read_fd=%d write_fd=%d errno=%d\n",
	                  SHIM_CALLER_RETURN_ADDRESS(), flags, result,
	                  result == 0 ? pipefd[0] : -1,
	                  result == 0 ? pipefd[1] : -1,
	                  result < 0 ? errno : 0);
	if (shim_trace_enabled())
		fprintf(stderr, "libsystem_shim: pipe2(flags=%#x) -> %d fds=[%d,%d] errno=%d\n",
		        flags, result, result == 0 ? pipefd[0] : -1,
		        result == 0 ? pipefd[1] : -1, result < 0 ? errno : 0);
	return result;
}

/* ===== stat() ABI translation ===== */

/*
 * macOS aarch64 and Linux aarch64 have different struct stat layouts.
 * macOS st_mode is uint16_t at offset 4; Linux st_mode is uint32_t at
 * offset 16. The game allocates a Darwin-sized buffer (144 bytes) and
 * reads fields at Darwin offsets, so we call the real Linux stat via
 * syscall and convert the result to Darwin layout.
 */

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

static void linux_to_darwin_stat(const struct stat *ls, struct darwin_stat *ds)
{
	memset(ds, 0, sizeof(*ds));
	ds->st_dev        = (int32_t)ls->st_dev;
	ds->st_mode       = (uint16_t)ls->st_mode;
	ds->st_nlink      = (uint16_t)ls->st_nlink;
	ds->st_ino        = ls->st_ino;
	ds->st_uid        = ls->st_uid;
	ds->st_gid        = ls->st_gid;
	ds->st_rdev       = (int32_t)ls->st_rdev;
	ds->st_atimespec  = ls->st_atim;
	ds->st_mtimespec  = ls->st_mtim;
	ds->st_ctimespec  = ls->st_ctim;
	ds->st_birthtimespec = ls->st_ctim; /* no birth time on Linux */
	ds->st_size       = ls->st_size;
	ds->st_blocks     = ls->st_blocks;
	ds->st_blksize    = ls->st_blksize;
}

/*
 * Use syscall(SYS_newfstatat) directly to avoid recursion — our
 * wrappers redefine stat/fstat/lstat, so calling glibc's stat()
 * would call ourselves.
 */

/*
 * Hide files that must not exist from the Mach-O binary's point of view.
 *
 * A macOS libsteam_api.dylib can never function under machgate on Linux (it
 * talks to the macOS Steam client, which isn't here), and machgate already
 * STUBs the Steamworks API to return 0. Games decide "am I a Steam build?" by
 * stat()'ing the dylib — e.g. Mina the Hollower (_global.c) does
 * `s_isSteamBuild = stat("libsteam_api.dylib")==0`, then refuses to boot
 * (MessagePromptManager::CheckSystemError → never calls Game::PostLoad) once
 * SteamAPI_Init fails. Returning ENOENT makes such games take their normal
 * non-Steam path. Matched by basename so the "../MacOS/libsteam_api.dylib"
 * fallback probe is covered too.
 */
static int path_is_hidden(const char *path)
{
	if (!path)
		return 0;
	const char *base = strrchr(path, '/');
	base = base ? base + 1 : path;
	return strcmp(base, "libsteam_api.dylib") == 0;
}

int stat_darwin(const char *path, void *buf)
{
	if (path_is_hidden(path)) { errno = ENOENT; return -1; }
	struct stat ls;
	int ret = syscall(SYS_newfstatat, AT_FDCWD, path, &ls, 0);
	if (ret == 0)
		linux_to_darwin_stat(&ls, (struct darwin_stat *)buf);
	else
		ret = -1;  /* syscall returns -errno on error */
	return ret;
}

int fstat_darwin(int fd, void *buf)
{
	struct stat ls;
	int ret = syscall(SYS_fstat, fd, &ls);
	if (ret == 0)
		linux_to_darwin_stat(&ls, (struct darwin_stat *)buf);
	else
		ret = -1;
	return ret;
}

int lstat_darwin(const char *path, void *buf)
{
	if (path_is_hidden(path)) { errno = ENOENT; return -1; }
	struct stat ls;
	int ret = syscall(SYS_newfstatat, AT_FDCWD, path, &ls, AT_SYMLINK_NOFOLLOW);
	if (ret == 0)
		linux_to_darwin_stat(&ls, (struct darwin_stat *)buf);
	else
		ret = -1;
	return ret;
}

int fstatat_darwin(int dirfd, const char *path, void *buf, int flags)
{
	if (path_is_hidden(path)) { errno = ENOENT; return -1; }
	struct stat ls;
	int ret = syscall(SYS_newfstatat, dirfd, path, &ls, flags);
	if (ret == 0)
		linux_to_darwin_stat(&ls, (struct darwin_stat *)buf);
	else
		ret = -1;
	return ret;
}

/* Export under standard names so the resolver finds them when the game
 * imports stat/fstat/lstat from libSystem.B.dylib.
 * Use __asm__ labels to avoid conflicting with glibc's prototypes. */
/* Export under standard names so the resolver finds them when the game
 * imports stat/fstat/lstat from libSystem.B.dylib. Only Mach-O code
 * reaches these (via GOT patching), so unconditionally translate. */
int shim_stat(const char *path, void *buf) __asm__("stat");
int shim_stat(const char *path, void *buf)
{
	return stat_darwin(path, buf);
}

int shim_fstat(int fd, void *buf) __asm__("fstat");
int shim_fstat(int fd, void *buf)
{
	return fstat_darwin(fd, buf);
}

int shim_lstat(const char *path, void *buf) __asm__("lstat");
int shim_lstat(const char *path, void *buf)
{
	return lstat_darwin(path, buf);
}

int shim_fstatat(int dirfd, const char *path, void *buf, int flags) __asm__("fstatat");
int shim_fstatat(int dirfd, const char *path, void *buf, int flags)
{
	return fstatat_darwin(translate_dirfd(dirfd), path, buf,
	                      translate_atflags(flags));
}

/* ===== readdir() ABI translation ===== */

/*
 * macOS struct dirent (64-bit):
 *   uint64_t  d_ino;       // offset 0
 *   uint64_t  d_seekoff;   // offset 8  (macOS-only)
 *   uint16_t  d_reclen;    // offset 16
 *   uint16_t  d_namlen;    // offset 18 (macOS-only)
 *   uint8_t   d_type;      // offset 20
 *   char      d_name[1024];// offset 21
 *   [padding to 1048]
 *
 * Linux struct dirent (aarch64):
 *   ino_t     d_ino;       // offset 0  (8 bytes)
 *   off_t     d_off;       // offset 8  (8 bytes)
 *   uint16_t  d_reclen;    // offset 16
 *   uint8_t   d_type;      // offset 18
 *   char      d_name[256]; // offset 19
 *
 * The game reads d_type at offset 20 and d_name at offset 21 (macOS).
 * We convert the Linux result into a static Darwin-layout buffer.
 */

struct darwin_dirent {
	uint64_t  d_ino;
	uint64_t  d_seekoff;
	uint16_t  d_reclen;
	uint16_t  d_namlen;
	uint8_t   d_type;
	char      d_name[1024];
	/* padding to 8-byte alignment */
	char      __pad[3];
};

_Static_assert(sizeof(struct darwin_dirent) == 1048,
	"darwin_dirent must be 1048 bytes");

/*
 * Thread-local buffer for the converted dirent, since readdir returns
 * a pointer to an internal buffer (not caller-allocated).
 */
static __thread struct darwin_dirent tls_darwin_dirent;

struct dirent *shim_readdir(DIR *dirp) __asm__("readdir");
struct dirent *shim_readdir(DIR *dirp)
{
	struct dirent *le = readdir(dirp);
	if (!le)
		return NULL;

	/* Convert to darwin dirent layout. Only Mach-O code reaches
	 * this wrapper (via GOT patching), so always convert. */
	struct darwin_dirent *de = &tls_darwin_dirent;
	memset(de, 0, sizeof(*de));
	de->d_ino     = le->d_ino;
	de->d_seekoff = 0;  /* not available on Linux */
	de->d_reclen  = sizeof(struct darwin_dirent);
	de->d_type    = le->d_type;

	size_t namelen = strlen(le->d_name);
	if (namelen >= sizeof(de->d_name))
		namelen = sizeof(de->d_name) - 1;
	memcpy(de->d_name, le->d_name, namelen);
	de->d_name[namelen] = '\0';
	de->d_namlen = (uint16_t)namelen;

	return (struct dirent *)(void *)de;
}

struct dirent *shim_readdir_r(DIR *dirp, void *entry, void **result) __asm__("readdir_r");
struct dirent *shim_readdir_r(DIR *dirp, void *entry, void **result)
{
	/* readdir_r is deprecated but some Mach-O code uses it.
	 * Convert into the caller's darwin_dirent buffer. */
	struct dirent linux_entry;
	struct dirent *linux_result = NULL;

	int ret = readdir_r(dirp, &linux_entry, &linux_result);
	if (ret != 0 || !linux_result) {
		if (result) *(void**)result = NULL;
		return (struct dirent *)(intptr_t)ret;
	}

	struct darwin_dirent *de = (struct darwin_dirent *)entry;
	memset(de, 0, sizeof(*de));
	de->d_ino     = linux_result->d_ino;
	de->d_seekoff = 0;
	de->d_reclen  = sizeof(struct darwin_dirent);
	de->d_type    = linux_result->d_type;

	size_t namelen = strlen(linux_result->d_name);
	if (namelen >= 1024) namelen = 1023;
	memcpy(de->d_name, linux_result->d_name, namelen);
	de->d_name[namelen] = '\0';
	de->d_namlen = (uint16_t)namelen;

	if (result) *(void**)result = de;
	return (struct dirent *)(intptr_t)ret;
}

/* ===== mmap flag translation ===== */

/* macOS and Linux use different values for mmap flags.
 * macOS MAP_ANON = 0x1000, Linux MAP_ANONYMOUS = 0x0020.
 * Without translation, LuaJIT's allocator (and any other code using
 * mmap with MAP_ANON) silently fails on Linux. */

#include <sys/mman.h>

#define DARWIN_MAP_ANON  0x1000
#define DARWIN_MAP_JIT   0x0800

typedef long (*machgate_darwin_mmap_fn)(void*, size_t, int, int, int, off_t);
typedef long (*machgate_darwin_munmap_fn)(void*, size_t);

static machgate_darwin_mmap_fn shim_guest_mmap_fn;
static machgate_darwin_munmap_fn shim_guest_munmap_fn;
static void* (*shim_real_mmap)(void*, size_t, int, int, int, off_t);
static int (*shim_real_munmap)(void*, size_t);

static void shim_resolve_guest_vm_wrappers(void)
{
	if (!shim_real_mmap)
		shim_real_mmap = dlsym(RTLD_NEXT, "mmap");
	if (!shim_real_munmap)
		shim_real_munmap = dlsym(RTLD_NEXT, "munmap");
	if (!shim_guest_mmap_fn)
		shim_guest_mmap_fn = (machgate_darwin_mmap_fn)dlsym(RTLD_DEFAULT,
		                                                    "machgate_darwin_mmap");
	if (!shim_guest_munmap_fn)
		shim_guest_munmap_fn = (machgate_darwin_munmap_fn)dlsym(RTLD_DEFAULT,
		                                                        "machgate_darwin_munmap");
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
	void* caller = SHIM_CALLER_RETURN_ADDRESS();
	int darwin_flags = flags;
	int linux_flags = flags;
	void* result;

	shim_resolve_guest_vm_wrappers();
	if (flags & DARWIN_MAP_ANON) {
		linux_flags &= ~DARWIN_MAP_ANON;
		linux_flags |= MAP_ANONYMOUS;
	}
	linux_flags &= ~DARWIN_MAP_JIT;

	if (shim_guest_mmap_fn) {
		long mapped = shim_guest_mmap_fn(addr, length, prot, linux_flags, fd, offset);
		result = mapped < 0 ? MAP_FAILED : (void*)mapped;
	} else if (shim_real_mmap) {
		result = shim_real_mmap(addr, length, prot, linux_flags, fd, offset);
	} else {
		result = MAP_FAILED;
		errno = ENOSYS;
	}
	int saved_errno = errno;
	if (shim_delta_vm_trace_enabled()) {
		fprintf(stderr,
		        "libsystem_shim: mmap tid=%d pthread=%#lx caller=%p addr=%p length=%zu prot=%#x flags=%#x linux_flags=%#x fd=%d offset=%jd -> %p errno=%d\n",
		        (int)shim_trace_tid(), shim_trace_pthread_self(), caller,
		        addr, length, prot, darwin_flags, linux_flags, fd,
		        (intmax_t)offset, result,
		        result == MAP_FAILED ? saved_errno : 0);
		errno = saved_errno;
	}
	return result;
}

int munmap(void *addr, size_t length)
{
	void* caller = SHIM_CALLER_RETURN_ADDRESS();
	long result;

	shim_resolve_guest_vm_wrappers();
	if (shim_guest_munmap_fn)
		result = shim_guest_munmap_fn(addr, length);
	else if (shim_real_munmap)
		result = shim_real_munmap(addr, length);
	else {
		errno = ENOSYS;
		result = -1;
	}
	int saved_errno = errno;
	if (shim_delta_vm_trace_enabled()) {
		fprintf(stderr,
		        "libsystem_shim: munmap tid=%d pthread=%#lx caller=%p addr=%p length=%zu -> %ld errno=%d\n",
		        (int)shim_trace_tid(), shim_trace_pthread_self(), caller,
		        addr, length, result, result < 0 ? saved_errno : 0);
		errno = saved_errno;
	}
	return (int)result;
}

ssize_t readlink(const char *path, char *buffer, size_t buffer_size)
{
	static ssize_t (*real_readlink)(const char*, char*, size_t) = NULL;
	void* caller = SHIM_CALLER_RETURN_ADDRESS();

	if (!real_readlink)
		real_readlink = dlsym(RTLD_NEXT, "readlink");
	if (!real_readlink) {
		errno = ENOSYS;
		if (shim_delta_vm_trace_enabled()) {
			int saved_errno = errno;
			fprintf(stderr,
			        "libsystem_shim: readlink tid=%d pthread=%#lx caller=%p path=\"%s\" buffer=%p size=%zu -> -1 errno=%d\n",
			        (int)shim_trace_tid(), shim_trace_pthread_self(), caller,
			        shim_trace_path(path), buffer, buffer_size, saved_errno);
			errno = saved_errno;
		}
		return -1;
	}

	ssize_t result = real_readlink(path, buffer, buffer_size);
	int saved_errno = errno;
	if (shim_delta_vm_trace_enabled()) {
		fprintf(stderr,
		        "libsystem_shim: readlink tid=%d pthread=%#lx caller=%p path=\"%s\" buffer=%p size=%zu -> %zd errno=%d\n",
		        (int)shim_trace_tid(), shim_trace_pthread_self(), caller,
		        shim_trace_path(path), buffer, buffer_size, result,
		        result < 0 ? saved_errno : 0);
		errno = saved_errno;
	}
	return result;
}

ssize_t readlinkat(int dirfd, const char *path, char *buffer,
                   size_t buffer_size)
{
	static ssize_t (*real_readlinkat)(int, const char*, char*, size_t) = NULL;
	void* caller = SHIM_CALLER_RETURN_ADDRESS();

	if (!real_readlinkat)
		real_readlinkat = dlsym(RTLD_NEXT, "readlinkat");
	if (!real_readlinkat) {
		errno = ENOSYS;
		if (shim_delta_vm_trace_enabled()) {
			int saved_errno = errno;
			fprintf(stderr,
			        "libsystem_shim: readlinkat tid=%d pthread=%#lx caller=%p dirfd=%d path=\"%s\" buffer=%p size=%zu -> -1 errno=%d\n",
			        (int)shim_trace_tid(), shim_trace_pthread_self(), caller,
			        dirfd, shim_trace_path(path), buffer, buffer_size,
			        saved_errno);
			errno = saved_errno;
		}
		return -1;
	}

	ssize_t result = real_readlinkat(dirfd, path, buffer, buffer_size);
	int saved_errno = errno;
	if (shim_delta_vm_trace_enabled()) {
		fprintf(stderr,
		        "libsystem_shim: readlinkat tid=%d pthread=%#lx caller=%p dirfd=%d path=\"%s\" buffer=%p size=%zu -> %zd errno=%d\n",
		        (int)shim_trace_tid(), shim_trace_pthread_self(), caller,
		        dirfd, shim_trace_path(path), buffer, buffer_size, result,
		        result < 0 ? saved_errno : 0);
		errno = saved_errno;
	}
	return result;
}

int mprotect(void *addr, size_t len, int prot)
{
	static int (*real_mprotect)(void*, size_t, int) = NULL;
	if (!real_mprotect)
		real_mprotect = dlsym(RTLD_NEXT, "mprotect");

	int result = real_mprotect(addr, len, prot);
	if (shim_trace_enabled())
		fprintf(stderr, "libsystem_shim: mprotect(%p, %zu, %#x) -> %d errno=%d\n",
		        addr, len, prot, result, result < 0 ? errno : 0);
	return result;
}

/* ===== mmap registry ===== */

/* Track mmap'd regions so shim_free can munmap instead of free.
 * Used by game function replacements (e.g., DataStream::preloadFile → mmap). */

#define MMAP_REGISTRY_MAX 512

static struct {
	void *addr;
	size_t size;
} mmap_registry[MMAP_REGISTRY_MAX];
static int mmap_registry_count = 0;

void mmap_registry_add(void *addr, size_t size)
{
	if (mmap_registry_count >= MMAP_REGISTRY_MAX) {
		fprintf(stderr, "mmap_registry: full, cannot register %p\n", addr);
		return;
	}
	mmap_registry[mmap_registry_count].addr = addr;
	mmap_registry[mmap_registry_count].size = size;
	mmap_registry_count++;
	fprintf(stderr, "mmap_registry: registered %p (%zu bytes)\n", addr, size);
}

/* Returns size if found and removed, 0 if not found */
static size_t mmap_registry_remove(void *addr)
{
	for (int i = 0; i < mmap_registry_count; i++) {
		if (mmap_registry[i].addr == addr) {
			size_t size = mmap_registry[i].size;
			mmap_registry[i] = mmap_registry[--mmap_registry_count];
			return size;
		}
	}
	return 0;
}

/* ===== dlopen interception ===== */

/* Redirect Apple framework dlopen calls to native Linux libraries.
 * Game code (e.g., GLEW's NSGLGetProcAddress) does:
 *   dlopen("/System/Library/Frameworks/OpenGL.framework/...", RTLD_LAZY)
 * We intercept and redirect to the native GL library (gl4es or Mesa). */

static void* (*real_dlopen)(const char*, int) = NULL;
static void* (*real_dlsym)(void*, const char*) = NULL;

#define DARWIN_RTLD_DEFAULT ((void*)-2L)
#define DARWIN_RTLD_SELF ((void*)-3L)
#define DARWIN_RTLD_MAIN_ONLY ((void*)-5L)

static void* translate_dlsym_handle(void* handle)
{
	if (handle == DARWIN_RTLD_DEFAULT ||
	    handle == DARWIN_RTLD_SELF ||
	    handle == DARWIN_RTLD_MAIN_ONLY)
		return RTLD_DEFAULT;
	return handle;
}

void* machgate_shim_malloc(size_t size);
void* machgate_shim_calloc(size_t count, size_t size);
void* machgate_shim_realloc(void* ptr, size_t size);
void machgate_shim_free(void* ptr);

static void* shim_dlsym_allocator_symbol(const char* symbol)
{
	if (strcmp(symbol, "malloc") == 0)
		return (void*)machgate_shim_malloc;
	if (strcmp(symbol, "free") == 0)
		return (void*)machgate_shim_free;
	if (strcmp(symbol, "calloc") == 0)
		return (void*)machgate_shim_calloc;
	if (strcmp(symbol, "realloc") == 0)
		return (void*)machgate_shim_realloc;
	return NULL;
}

static void* (*resolve_real_dlsym(void))(void*, const char*)
{
	static const char* versions[] = {
		"GLIBC_2.17",
		"GLIBC_2.34",
		"GLIBC_2.2.5"
	};

	for (size_t index = 0; index < sizeof(versions) / sizeof(versions[0]);
	     index++) {
		void* result = dlvsym(RTLD_NEXT, "dlsym", versions[index]);
		if (result)
			return (void* (*)(void*, const char*))result;
	}
	return NULL;
}

void* dlsym(void* handle, const char* symbol)
{
	if (!real_dlsym)
		real_dlsym = resolve_real_dlsym();

	if (!real_dlsym)
		return NULL;
	if (handle == (void*)RTLD_NEXT) {
		void* allocator_symbol = shim_dlsym_allocator_symbol(symbol);
		if (allocator_symbol)
			return allocator_symbol;
	}
	return real_dlsym(translate_dlsym_handle(handle), symbol);
}

static int translate_dlopen_flags(int flags)
{
	int result = 0;

	if (flags & RTLD_NOW)
		result |= RTLD_NOW;
	else
		result |= RTLD_LAZY;

	if (flags & RTLD_GLOBAL)
		result |= RTLD_GLOBAL;
	else
		result |= RTLD_LOCAL;

	return result;
}

static int is_shim_framework_path(const char* path)
{
	return path &&
	       (strstr(path, "CoreFoundation.framework") ||
	        strstr(path, "CoreServices.framework") ||
	        strstr(path, "ApplicationServices.framework") ||
	        strstr(path, "IOKit.framework"));
}

void* dlopen(const char* path, int flags)
{
	if (!real_dlopen)
		real_dlopen = dlsym(RTLD_NEXT, "dlopen");

	int linux_flags = translate_dlopen_flags(flags);

	if (is_shim_framework_path(path)) {
		void* handle = real_dlopen("libsystem_shim.so", linux_flags);
		if (shim_trace_enabled())
			fprintf(stderr, "libsystem_shim: redirected dlopen('%s') -> libsystem_shim.so handle=%p\n",
			        path, handle);
		return handle;
	}

	if (path && strstr(path, "OpenGL.framework")) {
		/* Redirect to gl4es (or native Mesa GL) — search LD_LIBRARY_PATH */
		void* handle = real_dlopen("libGL.so.1", linux_flags);
		if (handle) {
			fprintf(stderr, "libsystem_shim: redirected dlopen('%s') → libGL.so.1\n", path);
			return handle;
		}
		/* Fallback to GLES if no desktop GL */
		handle = real_dlopen("libGLESv2.so.2", linux_flags);
		if (handle) {
			fprintf(stderr, "libsystem_shim: redirected dlopen('%s') → libGLESv2.so.2\n", path);
			return handle;
		}
		fprintf(stderr, "libsystem_shim: WARNING: failed to redirect dlopen('%s')\n", path);
	}

	return real_dlopen(path, linux_flags);
}

/* ===== Darwin assert ===== */

/* macOS __assert_rtn(func, file, line, expr) → Linux __assert_fail(expr, file, line, func) */
extern void __assert_fail(const char *, const char *, unsigned int, const char *)
	__attribute__((noreturn));

__attribute__((noreturn))
void __assert_rtn(const char *func, const char *file, int line, const char *expr)
{
	__assert_fail(expr, file, (unsigned)line, func);
}

__attribute__((noreturn))
void abort(void)
{
	shim_dump_recent_alloc_events("abort", NULL);
	raise(SIGABRT);
	_exit(134);
}

int timingsafe_bcmp(const void* buffer_a, const void* buffer_b, size_t length)
{
	const unsigned char* bytes_a = buffer_a;
	const unsigned char* bytes_b = buffer_b;
	unsigned char result = 0;

	while (length--)
		result |= bytes_a[length] ^ bytes_b[length];

	return (result + 0xff) >> 8;
}

/* ===== Heap allocation wrappers ===== */

/* Intercept malloc/free/calloc/realloc for:
 * 1. Zero-initialization (macOS malloc returns zeroed pages)
 * 2. mmap registry (munmap instead of free for mmap'd regions)
 *
 * Uses dlsym(RTLD_NEXT) to find the real glibc functions.
 * Only Mach-O code reaches these (via GOT patching). */

typedef void *(*real_malloc_fn)(size_t);
typedef void  (*real_free_fn)(void *);
typedef void *(*real_calloc_fn)(size_t, size_t);
typedef void *(*real_realloc_fn)(void *, size_t);
typedef int   (*real_posix_memalign_fn)(void **, size_t, size_t);

static real_malloc_fn  real_malloc  = NULL;
static real_free_fn real_free = NULL;

#define MACHGATE_FREE_QUARANTINE_SLOTS 262144
#define MACHGATE_FREE_QUARANTINE_MAX_CHUNK (16u << 20)
#define MACHGATE_FREE_QUARANTINE_DEFAULT_BUDGET (8u << 30)

struct machgate_free_quarantine_slot {
	void* ptr;
	size_t bytes;
};

static struct machgate_free_quarantine_slot
    free_quarantine_ring[MACHGATE_FREE_QUARANTINE_SLOTS];
static size_t free_quarantine_head;
static size_t free_quarantine_tail;
static size_t quarantine_pending_bytes;

static size_t machgate_free_quarantine_budget(void)
{
	static long configured = -1;

	if (configured < 0) {
		const char* budget_env = getenv("MACHGATE_FREE_QUARANTINE_MB");
		long parsed = 0;

		if (budget_env)
			parsed = strtol(budget_env, NULL, 10);
		if (parsed > 0)
			configured = parsed << 20;
		else
			configured = 0;
	}
	if (configured == 0)
		return MACHGATE_FREE_QUARANTINE_DEFAULT_BUDGET;
	return (size_t)configured;
}

static real_calloc_fn real_calloc = NULL;
static real_realloc_fn real_realloc = NULL;
static real_posix_memalign_fn real_posix_memalign = NULL;

static char bootstrap_buf[4096];
static int bootstrap_pos = 0;

#define MACHGATE_ALLOCATION_SHARD_COUNT 32
#define MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE 8192
#define MACHGATE_ALLOCATION_MAX_TABLE_SIZE 4194304
#define MACHGATE_ALLOCATION_LOCK_SPIN_LIMIT 64

struct machgate_allocation_record {
	void* ptr;
	size_t size;
	void* zone;
	unsigned flags;
};

struct machgate_allocation_shard {
	struct machgate_allocation_record* records;
	size_t capacity;
	size_t live_count;
	size_t tombstone_count;
	int drop_warned;
	unsigned lock_state;
} __attribute__((aligned(64)));

static struct machgate_allocation_record allocation_records_static[
    MACHGATE_ALLOCATION_SHARD_COUNT]
    [MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE];

static struct machgate_allocation_shard allocation_shards[MACHGATE_ALLOCATION_SHARD_COUNT] = {
	{ allocation_records_static[0], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[1], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[2], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[3], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[4], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[5], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[6], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[7], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[8], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[9], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[10], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[11], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[12], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[13], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[14], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[15], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[16], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[17], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[18], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[19], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[20], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[21], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[22], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[23], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[24], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[25], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[26], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[27], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[28], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[29], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[30], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
	{ allocation_records_static[31], MACHGATE_ALLOCATION_SHARD_INITIAL_TABLE_SIZE, 0, 0, 0, 0 },
};

#define MACHGATE_ALLOCATION_GUEST_CXX 1u

#if defined(__aarch64__)
#define MACHGATE_ALLOCATION_SPIN_HINT() __asm__ volatile("yield" ::: "memory")
#else
#define MACHGATE_ALLOCATION_SPIN_HINT() do { } while (0)
#endif

static void allocation_shard_lock(struct machgate_allocation_shard* shard)
{
	unsigned previous;
	unsigned expected = 0;

	if (__atomic_compare_exchange_n(&shard->lock_state, &expected,
	                                1, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
		return;
	for (unsigned spin = 0; spin < MACHGATE_ALLOCATION_LOCK_SPIN_LIMIT; spin++) {
		previous = __atomic_load_n(&shard->lock_state, __ATOMIC_RELAXED);
		if (previous == 0) {
			unsigned retry_expected = 0;
			if (__atomic_compare_exchange_n(&shard->lock_state, &retry_expected,
			                                1, 0, __ATOMIC_ACQUIRE,
			                                __ATOMIC_RELAXED))
				return;
		}
		MACHGATE_ALLOCATION_SPIN_HINT();
	}
	previous = __atomic_exchange_n(&shard->lock_state, 2, __ATOMIC_ACQUIRE);
	while (previous != 0) {
		syscall(SYS_futex, &shard->lock_state, FUTEX_WAIT_PRIVATE, 2, NULL,
		        NULL, 0);
		previous = __atomic_exchange_n(&shard->lock_state, 2,
		                               __ATOMIC_ACQUIRE);
	}
}

static void allocation_shard_unlock(struct machgate_allocation_shard* shard)
{
	if (__atomic_fetch_sub(&shard->lock_state, 1, __ATOMIC_RELEASE) == 1)
		return;
	__atomic_store_n(&shard->lock_state, 0, __ATOMIC_RELEASE);
	syscall(SYS_futex, &shard->lock_state, FUTEX_WAKE_PRIVATE, 1, NULL, NULL,
	        0);
}

static uintptr_t allocation_record_hash(const void* ptr)
{
	uintptr_t value = (uintptr_t)ptr >> 4;

	value *= 0x9E3779B97F4A7C15ULL;
	value ^= value >> 29;
	return value;
}

static struct machgate_allocation_shard* allocation_shard_for_hash(uintptr_t hash)
{
	return &allocation_shards[hash & (MACHGATE_ALLOCATION_SHARD_COUNT - 1)];
}

static struct machgate_allocation_record* allocation_shard_static_base(
    const struct machgate_allocation_shard* shard)
{
	size_t shard_index = (size_t)(shard - allocation_shards);

	return allocation_records_static[shard_index];
}

static size_t allocation_record_index(uintptr_t hash, size_t capacity)
{
	return (hash >> 4) & (capacity - 1);
}

static int insert_allocation_record(
    struct machgate_allocation_record* records, size_t capacity,
    uintptr_t hash, void* ptr, size_t size, void* zone, unsigned flags,
    int* inserted_out, int* reused_tombstone_out)
{
	size_t index = allocation_record_index(hash, capacity);
	struct machgate_allocation_record* reusable_record = NULL;

	if (inserted_out)
		*inserted_out = 0;
	if (reused_tombstone_out)
		*reused_tombstone_out = 0;

	for (size_t probe = 0; probe < capacity; probe++) {
		struct machgate_allocation_record* record =
		    &records[(index + probe) & (capacity - 1)];

		if (record->ptr == ptr) {
			if (record->size == 0) {
				if (inserted_out)
					*inserted_out = 1;
				if (reused_tombstone_out)
					*reused_tombstone_out = 1;
			}
			record->ptr = ptr;
			record->size = size;
			record->zone = zone;
			record->flags = flags;
			return 1;
		}
		if (record->ptr && record->size == 0 && !reusable_record)
			reusable_record = record;
		if (!record->ptr) {
			if (reusable_record)
				record = reusable_record;
			if (inserted_out)
				*inserted_out = 1;
			if (reused_tombstone_out && reusable_record)
				*reused_tombstone_out = 1;
			record->ptr = ptr;
			record->size = size;
			record->zone = zone;
			record->flags = flags;
			return 1;
		}
	}
	if (reusable_record) {
		if (inserted_out)
			*inserted_out = 1;
		if (reused_tombstone_out)
			*reused_tombstone_out = 1;
		reusable_record->ptr = ptr;
		reusable_record->size = size;
		reusable_record->zone = zone;
		reusable_record->flags = flags;
		return 1;
	}
	return 0;
}

static void note_inserted_allocation_record(
    struct machgate_allocation_shard* shard, int inserted, int reused_tombstone)
{
	if (!inserted)
		return;
	shard->live_count++;
	if (reused_tombstone && shard->tombstone_count > 0)
		shard->tombstone_count--;
}

static int compact_allocation_shard(struct machgate_allocation_shard* shard)
{
	size_t capacity = shard->capacity;
	size_t bytes = capacity * sizeof(*shard->records);
	struct machgate_allocation_record* old_records = shard->records;
	struct machgate_allocation_record* new_records;
	size_t live_count = 0;

	new_records = (struct machgate_allocation_record*)syscall(
	    SYS_mmap, NULL, bytes, PROT_READ | PROT_WRITE,
	    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (new_records == MAP_FAILED)
		return 0;

	for (size_t index = 0; index < capacity; index++) {
		struct machgate_allocation_record* record = &old_records[index];
		if (!record->ptr || record->size == 0)
			continue;
		if (!insert_allocation_record(new_records, capacity,
		                              allocation_record_hash(record->ptr),
		                              record->ptr, record->size,
		                              record->zone, record->flags,
		                              NULL, NULL)) {
			syscall(SYS_munmap, new_records, bytes);
			return 0;
		}
		live_count++;
	}

	shard->records = new_records;
	shard->live_count = live_count;
	shard->tombstone_count = 0;
	if (old_records != allocation_shard_static_base(shard))
		syscall(SYS_munmap, old_records, bytes);
	return 1;
}

static int allocation_shard_should_compact(struct machgate_allocation_shard* shard)
{
	return shard->tombstone_count > 4096 &&
	       (shard->tombstone_count > shard->live_count ||
	        shard->tombstone_count > shard->capacity / 4);
}

static void warn_allocation_record_drop_once(struct machgate_allocation_shard* shard,
                                             void* ptr, size_t size, void* zone,
                                             unsigned flags)
{
	if (shard->drop_warned)
		return;
	shard->drop_warned = 1;
	fprintf(stderr,
	        "libsystem_shim: WARNING: allocation ledger full after %zu slots; first dropped ptr=%p size=%zu zone=%p flags=%#x\n",
	        shard->capacity, ptr, size, zone, flags);
}

static int grow_allocation_shard(struct machgate_allocation_shard* shard)
{
	size_t old_capacity = shard->capacity;
	size_t new_capacity = old_capacity * 2;
	size_t new_bytes;
	struct machgate_allocation_record* old_records = shard->records;
	struct machgate_allocation_record* new_records;

	if (old_capacity >= MACHGATE_ALLOCATION_MAX_TABLE_SIZE)
		return 0;
	if (new_capacity > MACHGATE_ALLOCATION_MAX_TABLE_SIZE)
		new_capacity = MACHGATE_ALLOCATION_MAX_TABLE_SIZE;
	if (new_capacity <= old_capacity)
		return 0;

	new_bytes = new_capacity * sizeof(*new_records);
	new_records = (struct machgate_allocation_record*)syscall(
	    SYS_mmap, NULL, new_bytes, PROT_READ | PROT_WRITE,
	    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (new_records == MAP_FAILED)
		return 0;

	for (size_t index = 0; index < old_capacity; index++) {
		struct machgate_allocation_record* record = &old_records[index];
		if (!record->ptr || record->size == 0)
			continue;
		if (!insert_allocation_record(new_records, new_capacity,
		                              allocation_record_hash(record->ptr),
		                              record->ptr, record->size,
		                              record->zone, record->flags,
		                              NULL, NULL)) {
			syscall(SYS_munmap, new_records, new_bytes);
			return 0;
		}
	}

	shard->records = new_records;
	shard->capacity = new_capacity;
	shard->tombstone_count = 0;
	if (old_records != allocation_shard_static_base(shard))
		syscall(SYS_munmap, old_records, old_capacity * sizeof(*old_records));
	return 1;
}

static void record_allocation_with_zone(void* ptr, size_t size, void* zone)
{
	int inserted = 0;
	int reused_tombstone = 0;
	uintptr_t hash = allocation_record_hash(ptr);
	struct machgate_allocation_shard* shard = allocation_shard_for_hash(hash);

	if (!ptr)
		return;

	allocation_shard_lock(shard);
	if (allocation_shard_should_compact(shard))
		compact_allocation_shard(shard);
	while (!insert_allocation_record(shard->records, shard->capacity, hash,
	                                 ptr, size, zone, 0, &inserted,
	                                 &reused_tombstone)) {
		if (grow_allocation_shard(shard))
			continue;
		warn_allocation_record_drop_once(shard, ptr, size, zone, 0);
		break;
	}
	note_inserted_allocation_record(shard, inserted, reused_tombstone);
	allocation_shard_unlock(shard);
}

static void record_allocation(void* ptr, size_t size)
{
	record_allocation_with_zone(ptr, size, NULL);
}

static int lookup_allocation(const void* ptr, size_t* size_out, void** zone_out,
                             unsigned* flags_out)
{
	uintptr_t hash = allocation_record_hash(ptr);
	struct machgate_allocation_shard* shard = allocation_shard_for_hash(hash);
	size_t index;

	if (!ptr)
		return 0;

	allocation_shard_lock(shard);
	index = allocation_record_index(hash, shard->capacity);
	for (size_t probe = 0; probe < shard->capacity; probe++) {
		const struct machgate_allocation_record* record =
		    &shard->records[(index + probe) & (shard->capacity - 1)];

		if (!record->ptr) {
			allocation_shard_unlock(shard);
			return 0;
		}
		if (record->ptr == ptr) {
			if (size_out)
				*size_out = record->size;
			if (zone_out)
				*zone_out = record->zone;
			if (flags_out)
				*flags_out = record->flags;
			allocation_shard_unlock(shard);
			return 1;
		}
	}

	allocation_shard_unlock(shard);
	return 0;
}

static int lookup_allocation_size(const void* ptr, size_t* size_out)
{
	return lookup_allocation(ptr, size_out, NULL, NULL);
}

static size_t recorded_allocation_size(void* ptr, size_t requested_size)
{
	(void)ptr;
	if (requested_size > 0)
		return requested_size;
	return 1;
}

static void forget_allocation(const void* ptr)
{
	uintptr_t hash = allocation_record_hash(ptr);
	struct machgate_allocation_shard* shard = allocation_shard_for_hash(hash);
	size_t index;

	if (!ptr)
		return;

	allocation_shard_lock(shard);
	index = allocation_record_index(hash, shard->capacity);
	for (size_t probe = 0; probe < shard->capacity; probe++) {
		struct machgate_allocation_record* record =
		    &shard->records[(index + probe) & (shard->capacity - 1)];

		if (!record->ptr) {
			allocation_shard_unlock(shard);
			return;
		}
		if (record->ptr == ptr) {
			if (record->size != 0) {
				shard->live_count--;
				shard->tombstone_count++;
			}
			record->size = 0;
			record->zone = NULL;
			record->flags = 0;
			allocation_shard_unlock(shard);
			return;
		}
	}
	allocation_shard_unlock(shard);
}

static void mark_guest_cxx_allocation(void* ptr, size_t size)
{
	uintptr_t hash = allocation_record_hash(ptr);
	struct machgate_allocation_shard* shard = allocation_shard_for_hash(hash);
	size_t index;
	struct machgate_allocation_record* reusable_record = NULL;

	if (!ptr)
		return;

	allocation_shard_lock(shard);
	if (allocation_shard_should_compact(shard))
		compact_allocation_shard(shard);
retry:
	index = allocation_record_index(hash, shard->capacity);
	reusable_record = NULL;
	for (size_t probe = 0; probe < shard->capacity; probe++) {
		struct machgate_allocation_record* record =
		    &shard->records[(index + probe) & (shard->capacity - 1)];

		if (!record->ptr) {
			if (reusable_record)
				record = reusable_record;
			shard->live_count++;
			if (reusable_record && shard->tombstone_count > 0)
				shard->tombstone_count--;
			record->ptr = ptr;
			record->size = recorded_allocation_size(ptr, size);
			record->zone = NULL;
			record->flags = MACHGATE_ALLOCATION_GUEST_CXX;
			allocation_shard_unlock(shard);
			return;
		}
		if (record->ptr && record->size == 0 && !reusable_record)
			reusable_record = record;
		if (record->ptr == ptr) {
			if (record->size == 0) {
				shard->live_count++;
				if (shard->tombstone_count > 0)
					shard->tombstone_count--;
				record->size = recorded_allocation_size(ptr, size);
			}
			record->flags |= MACHGATE_ALLOCATION_GUEST_CXX;
			allocation_shard_unlock(shard);
			return;
		}
	}
	if (reusable_record) {
		shard->live_count++;
		if (shard->tombstone_count > 0)
			shard->tombstone_count--;
		reusable_record->ptr = ptr;
		reusable_record->size = recorded_allocation_size(ptr, size);
		reusable_record->zone = NULL;
		reusable_record->flags = MACHGATE_ALLOCATION_GUEST_CXX;
		allocation_shard_unlock(shard);
		return;
	}
	if (grow_allocation_shard(shard))
		goto retry;
	warn_allocation_record_drop_once(shard, ptr, size, NULL,
	                                 MACHGATE_ALLOCATION_GUEST_CXX);
	allocation_shard_unlock(shard);
}

static int should_forward_guest_cxx_delete(const void* ptr)
{
	size_t size = 0;
	unsigned flags = 0;

	if (!ptr)
		return 0;
	if (lookup_allocation(ptr, &size, NULL, &flags))
		return size != 0 && (flags & MACHGATE_ALLOCATION_GUEST_CXX);
	return 1;
}

static void resolve_real_funcs(void)
{
	if (real_malloc) return;
	if (!real_dlsym)
		real_dlsym = resolve_real_dlsym();
	if (!real_dlsym)
		return;
	real_malloc  = (real_malloc_fn)real_dlsym(RTLD_NEXT, "malloc");
	real_free    = (real_free_fn)real_dlsym(RTLD_NEXT, "free");
	real_calloc  = (real_calloc_fn)real_dlsym(RTLD_NEXT, "calloc");
	real_realloc = (real_realloc_fn)real_dlsym(RTLD_NEXT, "realloc");
	real_posix_memalign = (real_posix_memalign_fn)real_dlsym(RTLD_NEXT,
	                                                        "posix_memalign");
}

static void *shim_malloc_impl_at(size_t size, void* caller)
{
	if (!real_malloc) {
		resolve_real_funcs();
		if (!real_malloc) {
			/* Bootstrap: return zeroed memory from static buffer */
			int aligned = (bootstrap_pos + 15) & ~15;
			if (aligned + (int)size <= (int)sizeof(bootstrap_buf)) {
				void *p = bootstrap_buf + aligned;
				memset(p, 0, size);
				bootstrap_pos = aligned + size;
				record_allocation(p, size ? size : 1);
				return p;
			}
			return NULL;
		}
	}
	/* macOS malloc returns zeroed pages — zero-init for compatibility. */
	void *p = real_malloc(size);
	if (p) {
		memset(p, 0, size);
		record_allocation(p, recorded_allocation_size(p, size));
		shim_trace_alloc_event_at("malloc", p, size, 0, caller, 1);
	}
	return p;
}

void *shim_malloc(size_t size) __asm__("malloc");
void *shim_malloc(size_t size)
{
	return shim_malloc_impl_at(size, MACHGATE_SHIM_CALLER());
}

static int take_allocation_record(const void* ptr, size_t* size_out,
                                  void** zone_out, unsigned* flags_out)
{
	uintptr_t hash = allocation_record_hash(ptr);
	struct machgate_allocation_shard* shard = allocation_shard_for_hash(hash);
	size_t index;

	if (!ptr)
		return 0;

	allocation_shard_lock(shard);
	index = allocation_record_index(hash, shard->capacity);
	for (size_t probe = 0; probe < shard->capacity; probe++) {
		struct machgate_allocation_record* record =
		    &shard->records[(index + probe) & (shard->capacity - 1)];

		if (!record->ptr)
			break;
		if (record->ptr != ptr)
			continue;
		if (size_out)
			*size_out = record->size;
		if (zone_out)
			*zone_out = record->zone;
		if (flags_out)
			*flags_out = record->flags;
		if (record->size != 0) {
			shard->live_count--;
			shard->tombstone_count++;
		}
		record->size = 0;
		record->zone = NULL;
		record->flags = 0;
		allocation_shard_unlock(shard);
		return 1;
	}
	allocation_shard_unlock(shard);
	return 0;
}

static void shim_free_release_oldest_locked(void)
{
	while (quarantine_pending_bytes > machgate_free_quarantine_budget()) {
		struct machgate_free_quarantine_slot* slot =
		    &free_quarantine_ring[free_quarantine_head];
		free_quarantine_head =
		    (free_quarantine_head + 1) % MACHGATE_FREE_QUARANTINE_SLOTS;
		quarantine_pending_bytes -= slot->bytes;
		real_free(slot->ptr);
		slot->ptr = NULL;
		slot->bytes = 0;
	}
}

static void shim_free_quarantine_push(void* ptr, size_t bytes)
{
	static pthread_mutex_t quarantine_mutex = PTHREAD_MUTEX_INITIALIZER;

	if (!ptr || !bytes)
		return;
	if (!real_free)
		return;
	if (machgate_free_quarantine_budget() == 0) {
		real_free(ptr);
		return;
	}

	size_t slot_size = bytes > PTRDIFF_MAX ? PTRDIFF_MAX : bytes;
	if (pthread_mutex_lock(&quarantine_mutex) != 0) {
		real_free(ptr);
		return;
	}
	struct machgate_free_quarantine_slot* slot =
	    &free_quarantine_ring[free_quarantine_tail];
	if (slot->ptr) {
		quarantine_pending_bytes -= slot->bytes;
		real_free(slot->ptr);
	}
	slot->ptr = ptr;
	slot->bytes = slot_size;
	quarantine_pending_bytes += slot_size;
	free_quarantine_tail =
	    (free_quarantine_tail + 1) % MACHGATE_FREE_QUARANTINE_SLOTS;
	shim_free_release_oldest_locked();
	pthread_mutex_unlock(&quarantine_mutex);
}

static void shim_free_impl_at(void *ptr, void* caller)
{
	size_t old_size = 0;
	int known;

	if (!ptr) return;
	known = take_allocation_record(ptr, &old_size, NULL, NULL);
	shim_trace_alloc_event_at("free", ptr, old_size, old_size, caller, known);
	if (!known && shim_alloc_mismatch_trace_enabled())
		shim_dump_recent_alloc_events("free-unknown-pointer", ptr);
	/* Don't free bootstrap allocations */
	if ((char *)ptr >= bootstrap_buf &&
	    (char *)ptr < bootstrap_buf + sizeof(bootstrap_buf))
		return;
	/* Check mmap registry: munmap instead of free for mmap'd regions */
	size_t mmap_size = mmap_registry_remove(ptr);
	if (mmap_size) {
		munmap(ptr, mmap_size);
		return;
	}
	if (!real_free) resolve_real_funcs();
	if (!real_free) return;
	size_t quarantine_size = known ? old_size : 0;
	if (quarantine_size > MACHGATE_FREE_QUARANTINE_MAX_CHUNK)
		quarantine_size = 0;
	if (!quarantine_size) {
		quarantine_size = malloc_usable_size(ptr);
		if ((char*)ptr >= bootstrap_buf &&
		    (char*)ptr < bootstrap_buf + sizeof(bootstrap_buf))
			quarantine_size = 0;
	}
	if (!quarantine_size || quarantine_size > MACHGATE_FREE_QUARANTINE_MAX_CHUNK) {
		real_free(ptr);
		return;
	}
	shim_free_quarantine_push(ptr, quarantine_size);
}

void shim_free(void *ptr) __asm__("free");
void shim_free(void *ptr)
{
	shim_free_impl_at(ptr, MACHGATE_SHIM_CALLER());
}

static void *shim_calloc_impl_at(size_t nmemb, size_t size, void* caller)
{
	if (!real_calloc) {
		resolve_real_funcs();
		if (!real_calloc) {
			size_t total = nmemb * size;
			return shim_malloc_impl_at(total, caller);
		}
	}
	void *p = real_calloc(nmemb, size);
	if (p) {
		record_allocation(p, recorded_allocation_size(p, nmemb * size));
		shim_trace_alloc_event_at("calloc", p, nmemb * size, 0, caller, 1);
	}
	return p;
}

void *shim_calloc(size_t nmemb, size_t size) __asm__("calloc");
void *shim_calloc(size_t nmemb, size_t size)
{
	return shim_calloc_impl_at(nmemb, size, MACHGATE_SHIM_CALLER());
}

static void *shim_realloc_impl_at(void *ptr, size_t size, void* caller)
{
	size_t old_size = 0;
	int known = 0;

	if (ptr && (char *)ptr >= bootstrap_buf &&
	    (char *)ptr < bootstrap_buf + sizeof(bootstrap_buf)) {
		lookup_allocation_size(ptr, &old_size);
		void* new_ptr = shim_malloc_impl_at(size, caller);
		if (!new_ptr && size != 0)
			return NULL;
		if (new_ptr && old_size)
			memcpy(new_ptr, ptr, old_size < size ? old_size : size);
		forget_allocation(ptr);
		return new_ptr;
	}

	if (ptr) {
		known = lookup_allocation_size(ptr, &old_size);
		if (!known && shim_alloc_mismatch_trace_enabled())
			shim_dump_recent_alloc_events("realloc-unknown-pointer", ptr);
	}
	if (!real_realloc) resolve_real_funcs();
	if (!real_realloc) return NULL;
	void *new_ptr = real_realloc(ptr, size);
	if (new_ptr || size == 0)
		forget_allocation(ptr);
	if (new_ptr) {
		record_allocation(new_ptr, recorded_allocation_size(new_ptr, size));
		shim_trace_alloc_event_at("realloc", new_ptr, size, old_size, caller, known);
	}
	return new_ptr;
}

void *shim_realloc(void *ptr, size_t size) __asm__("realloc");
void *shim_realloc(void *ptr, size_t size)
{
	return shim_realloc_impl_at(ptr, size, MACHGATE_SHIM_CALLER());
}

static int shim_posix_memalign_impl_at(void **memptr, size_t alignment,
                                       size_t size, void* caller)
{
	if (!memptr)
		return EINVAL;
	if (alignment < sizeof(void*) || (alignment & (alignment - 1)) != 0)
		return EINVAL;
	if (!real_posix_memalign)
		resolve_real_funcs();
	if (!real_posix_memalign)
		return ENOMEM;
	int result = real_posix_memalign(memptr, alignment, size);
	if (result == 0) {
		record_allocation(*memptr, recorded_allocation_size(*memptr, size));
		shim_trace_alloc_event_at("posix_memalign", *memptr, size, alignment,
		                          caller, 1);
	}
	return result;
}

int shim_posix_memalign(void **memptr, size_t alignment, size_t size) __asm__("posix_memalign");
int shim_posix_memalign(void **memptr, size_t alignment, size_t size)
{
	return shim_posix_memalign_impl_at(memptr, alignment, size,
	                                   MACHGATE_SHIM_CALLER());
}

size_t malloc_size(const void* ptr);
size_t malloc_good_size(size_t size);

void* machgate_shim_malloc(size_t size)
{
	return shim_malloc_impl_at(size, MACHGATE_SHIM_CALLER());
}

void* machgate_shim_calloc(size_t count, size_t size)
{
	return shim_calloc_impl_at(count, size, MACHGATE_SHIM_CALLER());
}

void* machgate_shim_realloc(void* ptr, size_t size)
{
	return shim_realloc_impl_at(ptr, size, MACHGATE_SHIM_CALLER());
}

void machgate_shim_free(void* ptr)
{
	shim_free_impl_at(ptr, MACHGATE_SHIM_CALLER());
}

int machgate_shim_posix_memalign(void** memptr, size_t alignment, size_t size)
{
	return shim_posix_memalign_impl_at(memptr, alignment, size,
	                                   MACHGATE_SHIM_CALLER());
}

static void* machgate_shim_memalign_at(size_t alignment, size_t size, void* caller)
{
	void* result = NULL;

	if (shim_posix_memalign_impl_at(&result, alignment, size, caller) != 0)
		return NULL;
	return result;
}

void* machgate_shim_memalign(size_t alignment, size_t size)
{
	return machgate_shim_memalign_at(alignment, size, MACHGATE_SHIM_CALLER());
}

void* machgate_shim_valloc(size_t size)
{
	size_t page_size = (size_t)sysconf(_SC_PAGESIZE);

	if (page_size == 0)
		page_size = 4096;
	return machgate_shim_memalign_at(page_size, size, MACHGATE_SHIM_CALLER());
}

void* memalign(size_t alignment, size_t size)
{
	return machgate_shim_memalign_at(alignment, size, MACHGATE_SHIM_CALLER());
}

void* aligned_alloc(size_t alignment, size_t size)
{
	return machgate_shim_memalign_at(alignment, size, MACHGATE_SHIM_CALLER());
}

void* valloc(size_t size)
{
	size_t page_size = (size_t)sysconf(_SC_PAGESIZE);

	if (page_size == 0)
		page_size = 4096;
	return machgate_shim_memalign_at(page_size, size, MACHGATE_SHIM_CALLER());
}

size_t machgate_shim_malloc_size(const void* ptr)
{
	return malloc_size(ptr);
}

size_t machgate_shim_malloc_good_size(size_t size)
{
	return malloc_good_size(size);
}

typedef void* (*machgate_guest_operator_new_fn)(size_t);
typedef void* (*machgate_guest_operator_new_nothrow_fn)(size_t, const void*);
typedef void* (*machgate_guest_operator_new_aligned_fn)(size_t, size_t);
typedef void* (*machgate_guest_operator_new_aligned_nothrow_fn)(size_t, size_t,
                                                               const void*);
typedef void (*machgate_guest_operator_delete_fn)(void*);
typedef void (*machgate_guest_operator_delete_sized_fn)(void*, size_t);
typedef void (*machgate_guest_operator_delete_aligned_fn)(void*, size_t);
typedef void (*machgate_guest_operator_delete_sized_aligned_fn)(void*, size_t, size_t);
typedef void (*machgate_guest_operator_delete_nothrow_fn)(void*, const void*);
typedef void (*machgate_guest_operator_delete_aligned_nothrow_fn)(void*, size_t,
                                                                 const void*);

static machgate_guest_operator_new_fn guest_operator_new;
static machgate_guest_operator_new_fn guest_operator_new_array;
static machgate_guest_operator_new_aligned_fn guest_operator_new_aligned;
static machgate_guest_operator_new_aligned_fn guest_operator_new_array_aligned;
static machgate_guest_operator_new_nothrow_fn guest_operator_new_nothrow;
static machgate_guest_operator_new_nothrow_fn guest_operator_new_array_nothrow;
static machgate_guest_operator_new_aligned_nothrow_fn guest_operator_new_aligned_nothrow;
static machgate_guest_operator_new_aligned_nothrow_fn guest_operator_new_array_aligned_nothrow;
static machgate_guest_operator_delete_fn guest_operator_delete;
static machgate_guest_operator_delete_fn guest_operator_delete_array;
static machgate_guest_operator_delete_sized_fn guest_operator_delete_sized;
static machgate_guest_operator_delete_sized_fn guest_operator_delete_array_sized;
static machgate_guest_operator_delete_aligned_fn guest_operator_delete_aligned;
static machgate_guest_operator_delete_aligned_fn guest_operator_delete_array_aligned;
static machgate_guest_operator_delete_sized_aligned_fn guest_operator_delete_sized_aligned;
static machgate_guest_operator_delete_sized_aligned_fn guest_operator_delete_array_sized_aligned;
static machgate_guest_operator_delete_nothrow_fn guest_operator_delete_nothrow;
static machgate_guest_operator_delete_nothrow_fn guest_operator_delete_array_nothrow;
static machgate_guest_operator_delete_aligned_nothrow_fn guest_operator_delete_aligned_nothrow;
static machgate_guest_operator_delete_aligned_nothrow_fn guest_operator_delete_array_aligned_nothrow;
static __thread int guest_cxx_allocator_depth;

void machgate_shim_set_guest_cxx_allocators(void* operator_new_fn,
                                            void* operator_new_array_fn,
                                            void* operator_new_aligned_fn,
                                            void* operator_new_array_aligned_fn,
                                            void* operator_delete_fn,
                                            void* operator_delete_array_fn,
                                            void* operator_delete_sized_fn,
                                            void* operator_delete_array_sized_fn,
                                            void* operator_delete_aligned_fn,
                                            void* operator_delete_array_aligned_fn,
                                            void* operator_delete_sized_aligned_fn,
                                            void* operator_delete_array_sized_aligned_fn,
                                            void* operator_new_nothrow_fn,
                                            void* operator_new_array_nothrow_fn,
                                            void* operator_new_aligned_nothrow_fn,
                                            void* operator_new_array_aligned_nothrow_fn,
                                            void* operator_delete_nothrow_fn,
                                            void* operator_delete_array_nothrow_fn,
                                            void* operator_delete_aligned_nothrow_fn,
                                            void* operator_delete_array_aligned_nothrow_fn)
{
	guest_operator_new = (machgate_guest_operator_new_fn)operator_new_fn;
	guest_operator_new_array = (machgate_guest_operator_new_fn)operator_new_array_fn;
	guest_operator_new_aligned =
	    (machgate_guest_operator_new_aligned_fn)operator_new_aligned_fn;
	guest_operator_new_array_aligned =
	    (machgate_guest_operator_new_aligned_fn)operator_new_array_aligned_fn;
	guest_operator_new_nothrow =
	    (machgate_guest_operator_new_nothrow_fn)operator_new_nothrow_fn;
	guest_operator_new_array_nothrow =
	    (machgate_guest_operator_new_nothrow_fn)operator_new_array_nothrow_fn;
	guest_operator_new_aligned_nothrow =
	    (machgate_guest_operator_new_aligned_nothrow_fn)operator_new_aligned_nothrow_fn;
	guest_operator_new_array_aligned_nothrow =
	    (machgate_guest_operator_new_aligned_nothrow_fn)operator_new_array_aligned_nothrow_fn;
	guest_operator_delete = (machgate_guest_operator_delete_fn)operator_delete_fn;
	guest_operator_delete_array =
	    (machgate_guest_operator_delete_fn)operator_delete_array_fn;
	guest_operator_delete_sized =
	    (machgate_guest_operator_delete_sized_fn)operator_delete_sized_fn;
	guest_operator_delete_array_sized =
	    (machgate_guest_operator_delete_sized_fn)operator_delete_array_sized_fn;
	guest_operator_delete_aligned =
	    (machgate_guest_operator_delete_aligned_fn)operator_delete_aligned_fn;
	guest_operator_delete_array_aligned =
	    (machgate_guest_operator_delete_aligned_fn)operator_delete_array_aligned_fn;
	guest_operator_delete_sized_aligned =
	    (machgate_guest_operator_delete_sized_aligned_fn)operator_delete_sized_aligned_fn;
	guest_operator_delete_array_sized_aligned =
	    (machgate_guest_operator_delete_sized_aligned_fn)operator_delete_array_sized_aligned_fn;
	guest_operator_delete_nothrow =
	    (machgate_guest_operator_delete_nothrow_fn)operator_delete_nothrow_fn;
	guest_operator_delete_array_nothrow =
	    (machgate_guest_operator_delete_nothrow_fn)operator_delete_array_nothrow_fn;
	guest_operator_delete_aligned_nothrow =
	    (machgate_guest_operator_delete_aligned_nothrow_fn)operator_delete_aligned_nothrow_fn;
	guest_operator_delete_array_aligned_nothrow =
	    (machgate_guest_operator_delete_aligned_nothrow_fn)operator_delete_array_aligned_nothrow_fn;
}

void* machgate_shim_guest_operator_new(size_t size)
{
	if (guest_operator_new && !guest_cxx_allocator_depth) {
		guest_cxx_allocator_depth++;
		void* result = guest_operator_new(size);
		guest_cxx_allocator_depth--;
		mark_guest_cxx_allocation(result, size);
		return result;
	}
	return shim_malloc_impl_at(size, MACHGATE_SHIM_CALLER());
}

void* machgate_shim_guest_operator_new_array(size_t size)
{
	if (guest_operator_new_array && !guest_cxx_allocator_depth) {
		guest_cxx_allocator_depth++;
		void* result = guest_operator_new_array(size);
		guest_cxx_allocator_depth--;
		mark_guest_cxx_allocation(result, size);
		return result;
	}
	return shim_malloc_impl_at(size, MACHGATE_SHIM_CALLER());
}

void* machgate_shim_guest_operator_new_aligned(size_t size, size_t alignment)
{
	if (guest_operator_new_aligned && !guest_cxx_allocator_depth) {
		guest_cxx_allocator_depth++;
		void* result = guest_operator_new_aligned(size, alignment);
		guest_cxx_allocator_depth--;
		mark_guest_cxx_allocation(result, size);
		return result;
	}
	return machgate_shim_memalign_at(alignment, size, MACHGATE_SHIM_CALLER());
}

void* machgate_shim_guest_operator_new_array_aligned(size_t size, size_t alignment)
{
	if (guest_operator_new_array_aligned && !guest_cxx_allocator_depth) {
		guest_cxx_allocator_depth++;
		void* result = guest_operator_new_array_aligned(size, alignment);
		guest_cxx_allocator_depth--;
		mark_guest_cxx_allocation(result, size);
		return result;
	}
	return machgate_shim_memalign_at(alignment, size, MACHGATE_SHIM_CALLER());
}

void* machgate_shim_guest_operator_new_nothrow(size_t size, const void* nothrow_arg)
{
	if (guest_operator_new_nothrow && !guest_cxx_allocator_depth) {
		guest_cxx_allocator_depth++;
		void* result = guest_operator_new_nothrow(size, nothrow_arg);
		guest_cxx_allocator_depth--;
		mark_guest_cxx_allocation(result, size);
		return result;
	}
	return machgate_shim_guest_operator_new(size);
}

void* machgate_shim_guest_operator_new_array_nothrow(size_t size,
                                                     const void* nothrow_arg)
{
	if (guest_operator_new_array_nothrow && !guest_cxx_allocator_depth) {
		guest_cxx_allocator_depth++;
		void* result = guest_operator_new_array_nothrow(size, nothrow_arg);
		guest_cxx_allocator_depth--;
		mark_guest_cxx_allocation(result, size);
		return result;
	}
	return machgate_shim_guest_operator_new_array(size);
}

void* machgate_shim_guest_operator_new_aligned_nothrow(size_t size,
                                                       size_t alignment,
                                                       const void* nothrow_arg)
{
	if (guest_operator_new_aligned_nothrow && !guest_cxx_allocator_depth) {
		guest_cxx_allocator_depth++;
		void* result =
		    guest_operator_new_aligned_nothrow(size, alignment, nothrow_arg);
		guest_cxx_allocator_depth--;
		mark_guest_cxx_allocation(result, size);
		return result;
	}
	return machgate_shim_guest_operator_new_aligned(size, alignment);
}

void* machgate_shim_guest_operator_new_array_aligned_nothrow(size_t size,
                                                             size_t alignment,
                                                             const void* nothrow_arg)
{
	if (guest_operator_new_array_aligned_nothrow && !guest_cxx_allocator_depth) {
		guest_cxx_allocator_depth++;
		void* result =
		    guest_operator_new_array_aligned_nothrow(size, alignment, nothrow_arg);
		guest_cxx_allocator_depth--;
		mark_guest_cxx_allocation(result, size);
		return result;
	}
	return machgate_shim_guest_operator_new_array_aligned(size, alignment);
}

void machgate_shim_guest_operator_delete(void* ptr)
{
	if (ptr && guest_operator_delete && should_forward_guest_cxx_delete(ptr) &&
	    !guest_cxx_allocator_depth) {
		guest_cxx_allocator_depth++;
		guest_operator_delete(ptr);
		guest_cxx_allocator_depth--;
		return;
	}
	shim_free_impl_at(ptr, MACHGATE_SHIM_CALLER());
}

void machgate_shim_guest_operator_delete_array(void* ptr)
{
	if (ptr && should_forward_guest_cxx_delete(ptr) &&
	    !guest_cxx_allocator_depth) {
		guest_cxx_allocator_depth++;
		if (guest_operator_delete_array)
			guest_operator_delete_array(ptr);
		else if (guest_operator_delete)
			guest_operator_delete(ptr);
		else
			shim_free_impl_at(ptr, MACHGATE_SHIM_CALLER());
		guest_cxx_allocator_depth--;
		return;
	}
	shim_free_impl_at(ptr, MACHGATE_SHIM_CALLER());
}

void machgate_shim_guest_operator_delete_sized(void* ptr, size_t size)
{
	if (ptr && should_forward_guest_cxx_delete(ptr) &&
	    !guest_cxx_allocator_depth) {
		guest_cxx_allocator_depth++;
		if (guest_operator_delete_sized)
			guest_operator_delete_sized(ptr, size);
		else if (guest_operator_delete)
			guest_operator_delete(ptr);
		else
			shim_free_impl_at(ptr, MACHGATE_SHIM_CALLER());
		guest_cxx_allocator_depth--;
		return;
	}
	shim_free_impl_at(ptr, MACHGATE_SHIM_CALLER());
}

void machgate_shim_guest_operator_delete_array_sized(void* ptr, size_t size)
{
	if (ptr && should_forward_guest_cxx_delete(ptr) &&
	    !guest_cxx_allocator_depth) {
		guest_cxx_allocator_depth++;
		if (guest_operator_delete_array_sized)
			guest_operator_delete_array_sized(ptr, size);
		else if (guest_operator_delete_array)
			guest_operator_delete_array(ptr);
		else if (guest_operator_delete)
			guest_operator_delete(ptr);
		else
			shim_free_impl_at(ptr, MACHGATE_SHIM_CALLER());
		guest_cxx_allocator_depth--;
		return;
	}
	shim_free_impl_at(ptr, MACHGATE_SHIM_CALLER());
}

void machgate_shim_guest_operator_delete_aligned(void* ptr, size_t alignment)
{
	if (ptr && should_forward_guest_cxx_delete(ptr) &&
	    !guest_cxx_allocator_depth) {
		guest_cxx_allocator_depth++;
		if (guest_operator_delete_aligned)
			guest_operator_delete_aligned(ptr, alignment);
		else if (guest_operator_delete)
			guest_operator_delete(ptr);
		else
			shim_free_impl_at(ptr, MACHGATE_SHIM_CALLER());
		guest_cxx_allocator_depth--;
		return;
	}
	shim_free_impl_at(ptr, MACHGATE_SHIM_CALLER());
}

void machgate_shim_guest_operator_delete_array_aligned(void* ptr, size_t alignment)
{
	if (ptr && should_forward_guest_cxx_delete(ptr) &&
	    !guest_cxx_allocator_depth) {
		guest_cxx_allocator_depth++;
		if (guest_operator_delete_array_aligned)
			guest_operator_delete_array_aligned(ptr, alignment);
		else if (guest_operator_delete_array)
			guest_operator_delete_array(ptr);
		else if (guest_operator_delete)
			guest_operator_delete(ptr);
		else
			shim_free_impl_at(ptr, MACHGATE_SHIM_CALLER());
		guest_cxx_allocator_depth--;
		return;
	}
	shim_free_impl_at(ptr, MACHGATE_SHIM_CALLER());
}

void machgate_shim_guest_operator_delete_sized_aligned(void* ptr, size_t size,
                                                       size_t alignment)
{
	if (ptr && should_forward_guest_cxx_delete(ptr) &&
	    !guest_cxx_allocator_depth) {
		guest_cxx_allocator_depth++;
		if (guest_operator_delete_sized_aligned)
			guest_operator_delete_sized_aligned(ptr, size, alignment);
		else if (guest_operator_delete_aligned)
			guest_operator_delete_aligned(ptr, alignment);
		else if (guest_operator_delete_sized)
			guest_operator_delete_sized(ptr, size);
		else if (guest_operator_delete)
			guest_operator_delete(ptr);
		else
			shim_free_impl_at(ptr, MACHGATE_SHIM_CALLER());
		guest_cxx_allocator_depth--;
		return;
	}
	shim_free_impl_at(ptr, MACHGATE_SHIM_CALLER());
}

void machgate_shim_guest_operator_delete_array_sized_aligned(void* ptr,
                                                             size_t size,
                                                             size_t alignment)
{
	if (ptr && should_forward_guest_cxx_delete(ptr) &&
	    !guest_cxx_allocator_depth) {
		guest_cxx_allocator_depth++;
		if (guest_operator_delete_array_sized_aligned)
			guest_operator_delete_array_sized_aligned(ptr, size, alignment);
		else if (guest_operator_delete_array_aligned)
			guest_operator_delete_array_aligned(ptr, alignment);
		else if (guest_operator_delete_array_sized)
			guest_operator_delete_array_sized(ptr, size);
		else if (guest_operator_delete_array)
			guest_operator_delete_array(ptr);
		else if (guest_operator_delete)
			guest_operator_delete(ptr);
		else
			shim_free_impl_at(ptr, MACHGATE_SHIM_CALLER());
		guest_cxx_allocator_depth--;
		return;
	}
	shim_free_impl_at(ptr, MACHGATE_SHIM_CALLER());
}

void machgate_shim_guest_operator_delete_nothrow(void* ptr, const void* nothrow_arg)
{
	if (ptr && should_forward_guest_cxx_delete(ptr) &&
	    !guest_cxx_allocator_depth) {
		guest_cxx_allocator_depth++;
		if (guest_operator_delete_nothrow)
			guest_operator_delete_nothrow(ptr, nothrow_arg);
		else if (guest_operator_delete)
			guest_operator_delete(ptr);
		else
			shim_free_impl_at(ptr, MACHGATE_SHIM_CALLER());
		guest_cxx_allocator_depth--;
		return;
	}
	shim_free_impl_at(ptr, MACHGATE_SHIM_CALLER());
}

void machgate_shim_guest_operator_delete_array_nothrow(void* ptr,
                                                       const void* nothrow_arg)
{
	if (ptr && should_forward_guest_cxx_delete(ptr) &&
	    !guest_cxx_allocator_depth) {
		guest_cxx_allocator_depth++;
		if (guest_operator_delete_array_nothrow)
			guest_operator_delete_array_nothrow(ptr, nothrow_arg);
		else if (guest_operator_delete_array)
			guest_operator_delete_array(ptr);
		else if (guest_operator_delete)
			guest_operator_delete(ptr);
		else
			shim_free_impl_at(ptr, MACHGATE_SHIM_CALLER());
		guest_cxx_allocator_depth--;
		return;
	}
	shim_free_impl_at(ptr, MACHGATE_SHIM_CALLER());
}

void machgate_shim_guest_operator_delete_aligned_nothrow(void* ptr,
                                                         size_t alignment,
                                                         const void* nothrow_arg)
{
	if (ptr && should_forward_guest_cxx_delete(ptr) &&
	    !guest_cxx_allocator_depth) {
		guest_cxx_allocator_depth++;
		if (guest_operator_delete_aligned_nothrow)
			guest_operator_delete_aligned_nothrow(ptr, alignment, nothrow_arg);
		else if (guest_operator_delete_aligned)
			guest_operator_delete_aligned(ptr, alignment);
		else if (guest_operator_delete)
			guest_operator_delete(ptr);
		else
			shim_free_impl_at(ptr, MACHGATE_SHIM_CALLER());
		guest_cxx_allocator_depth--;
		return;
	}
	shim_free_impl_at(ptr, MACHGATE_SHIM_CALLER());
}

void machgate_shim_guest_operator_delete_array_aligned_nothrow(
    void* ptr, size_t alignment, const void* nothrow_arg)
{
	if (ptr && should_forward_guest_cxx_delete(ptr) &&
	    !guest_cxx_allocator_depth) {
		guest_cxx_allocator_depth++;
		if (guest_operator_delete_array_aligned_nothrow)
			guest_operator_delete_array_aligned_nothrow(ptr, alignment,
			                                            nothrow_arg);
		else if (guest_operator_delete_array_aligned)
			guest_operator_delete_array_aligned(ptr, alignment);
		else if (guest_operator_delete_array)
			guest_operator_delete_array(ptr);
		else if (guest_operator_delete)
			guest_operator_delete(ptr);
		else
			shim_free_impl_at(ptr, MACHGATE_SHIM_CALLER());
		guest_cxx_allocator_depth--;
		return;
	}
	shim_free_impl_at(ptr, MACHGATE_SHIM_CALLER());
}

void* machgate_operator_new(size_t size) __asm__("_Znwm");
void* machgate_operator_new(size_t size)
{
	return shim_malloc_impl_at(size, MACHGATE_SHIM_CALLER());
}

void* machgate_operator_new_array(size_t size) __asm__("_Znam");
void* machgate_operator_new_array(size_t size)
{
	return shim_malloc_impl_at(size, MACHGATE_SHIM_CALLER());
}

void* machgate_operator_new_nothrow(size_t size, const void* nothrow_arg) __asm__("_ZnwmRKSt9nothrow_t");
void* machgate_operator_new_nothrow(size_t size, const void* nothrow_arg)
{
	(void)nothrow_arg;
	return shim_malloc_impl_at(size, MACHGATE_SHIM_CALLER());
}

void* machgate_operator_new_array_nothrow(size_t size, const void* nothrow_arg) __asm__("_ZnamRKSt9nothrow_t");
void* machgate_operator_new_array_nothrow(size_t size, const void* nothrow_arg)
{
	(void)nothrow_arg;
	return shim_malloc_impl_at(size, MACHGATE_SHIM_CALLER());
}

void* machgate_operator_new_aligned(size_t size, size_t alignment) __asm__("_ZnwmSt11align_val_t");
void* machgate_operator_new_aligned(size_t size, size_t alignment)
{
	return machgate_shim_memalign_at(alignment, size, MACHGATE_SHIM_CALLER());
}

void* machgate_operator_new_array_aligned(size_t size, size_t alignment) __asm__("_ZnamSt11align_val_t");
void* machgate_operator_new_array_aligned(size_t size, size_t alignment)
{
	return machgate_shim_memalign_at(alignment, size, MACHGATE_SHIM_CALLER());
}

void* machgate_operator_new_aligned_nothrow(size_t size, size_t alignment, const void* nothrow_arg) __asm__("_ZnwmSt11align_val_tRKSt9nothrow_t");
void* machgate_operator_new_aligned_nothrow(size_t size, size_t alignment, const void* nothrow_arg)
{
	(void)nothrow_arg;
	return machgate_shim_memalign_at(alignment, size, MACHGATE_SHIM_CALLER());
}

void* machgate_operator_new_array_aligned_nothrow(size_t size, size_t alignment, const void* nothrow_arg) __asm__("_ZnamSt11align_val_tRKSt9nothrow_t");
void* machgate_operator_new_array_aligned_nothrow(size_t size, size_t alignment, const void* nothrow_arg)
{
	(void)nothrow_arg;
	return machgate_shim_memalign_at(alignment, size, MACHGATE_SHIM_CALLER());
}

void machgate_operator_delete(void* ptr) __asm__("_ZdlPv");
void machgate_operator_delete(void* ptr)
{
	shim_free_impl_at(ptr, MACHGATE_SHIM_CALLER());
}

void machgate_operator_delete_array(void* ptr) __asm__("_ZdaPv");
void machgate_operator_delete_array(void* ptr)
{
	shim_free_impl_at(ptr, MACHGATE_SHIM_CALLER());
}

void machgate_operator_delete_sized(void* ptr, size_t size) __asm__("_ZdlPvm");
void machgate_operator_delete_sized(void* ptr, size_t size)
{
	void* caller = MACHGATE_SHIM_CALLER();
	shim_trace_alloc_event_at("operator_delete_sized", ptr, size, size,
	                          caller, 1);
	shim_free_impl_at(ptr, caller);
}

void machgate_operator_delete_array_sized(void* ptr, size_t size) __asm__("_ZdaPvm");
void machgate_operator_delete_array_sized(void* ptr, size_t size)
{
	void* caller = MACHGATE_SHIM_CALLER();
	shim_trace_alloc_event_at("operator_delete_array_sized", ptr, size, size,
	                          caller, 1);
	shim_free_impl_at(ptr, caller);
}

void machgate_operator_delete_nothrow(void* ptr, const void* nothrow_arg) __asm__("_ZdlPvRKSt9nothrow_t");
void machgate_operator_delete_nothrow(void* ptr, const void* nothrow_arg)
{
	(void)nothrow_arg;
	shim_free_impl_at(ptr, MACHGATE_SHIM_CALLER());
}

void machgate_operator_delete_array_nothrow(void* ptr, const void* nothrow_arg) __asm__("_ZdaPvRKSt9nothrow_t");
void machgate_operator_delete_array_nothrow(void* ptr, const void* nothrow_arg)
{
	(void)nothrow_arg;
	shim_free_impl_at(ptr, MACHGATE_SHIM_CALLER());
}

void machgate_operator_delete_aligned(void* ptr, size_t alignment) __asm__("_ZdlPvSt11align_val_t");
void machgate_operator_delete_aligned(void* ptr, size_t alignment)
{
	(void)alignment;
	shim_free_impl_at(ptr, MACHGATE_SHIM_CALLER());
}

void machgate_operator_delete_array_aligned(void* ptr, size_t alignment) __asm__("_ZdaPvSt11align_val_t");
void machgate_operator_delete_array_aligned(void* ptr, size_t alignment)
{
	(void)alignment;
	shim_free_impl_at(ptr, MACHGATE_SHIM_CALLER());
}

void machgate_operator_delete_sized_aligned(void* ptr, size_t size, size_t alignment) __asm__("_ZdlPvmSt11align_val_t");
void machgate_operator_delete_sized_aligned(void* ptr, size_t size, size_t alignment)
{
	(void)alignment;
	void* caller = MACHGATE_SHIM_CALLER();
	shim_trace_alloc_event_at("operator_delete_sized_aligned", ptr, size, size,
	                          caller, 1);
	shim_free_impl_at(ptr, caller);
}

void machgate_operator_delete_array_sized_aligned(void* ptr, size_t size, size_t alignment) __asm__("_ZdaPvmSt11align_val_t");
void machgate_operator_delete_array_sized_aligned(void* ptr, size_t size, size_t alignment)
{
	(void)alignment;
	void* caller = MACHGATE_SHIM_CALLER();
	shim_trace_alloc_event_at("operator_delete_array_sized_aligned", ptr, size, size,
	                          caller, 1);
	shim_free_impl_at(ptr, caller);
}

void machgate_operator_delete_aligned_nothrow(void* ptr, size_t alignment, const void* nothrow_arg) __asm__("_ZdlPvSt11align_val_tRKSt9nothrow_t");
void machgate_operator_delete_aligned_nothrow(void* ptr, size_t alignment, const void* nothrow_arg)
{
	(void)alignment;
	(void)nothrow_arg;
	shim_free_impl_at(ptr, MACHGATE_SHIM_CALLER());
}

void machgate_operator_delete_array_aligned_nothrow(void* ptr, size_t alignment, const void* nothrow_arg) __asm__("_ZdaPvSt11align_val_tRKSt9nothrow_t");
void machgate_operator_delete_array_aligned_nothrow(void* ptr, size_t alignment, const void* nothrow_arg)
{
	(void)alignment;
	(void)nothrow_arg;
	shim_free_impl_at(ptr, MACHGATE_SHIM_CALLER());
}

struct machgate_malloc_zone {
	void* reserved1;
	void* reserved2;
	size_t (*size)(void* zone, const void* ptr);
	void* (*malloc)(void* zone, size_t size);
	void* (*calloc)(void* zone, size_t count, size_t size);
	void* (*valloc)(void* zone, size_t size);
	void (*free)(void* zone, void* ptr);
	void* (*realloc)(void* zone, void* ptr, size_t size);
	void (*destroy)(void* zone);
	const char* zone_name;
	unsigned (*batch_malloc)(void* zone, size_t size, void** results,
	                         unsigned count);
	void (*batch_free)(void* zone, void** pointers, unsigned count);
	void* introspect;
	unsigned version;
	void* (*memalign)(void* zone, size_t alignment, size_t size);
	void (*free_definite_size)(void* zone, void* ptr, size_t size);
	size_t (*pressure_relief)(void* zone, size_t goal);
	int (*claimed_address)(void* zone, void* ptr);
	void (*try_free_default)(void* zone, void* ptr);
	void* (*malloc_with_options)(void* zone, size_t alignment, size_t size,
	                             unsigned options);
	void* (*aligned_malloc)(void* zone, size_t alignment, size_t size);
};

static struct machgate_malloc_zone default_malloc_zone;

#define MACHGATE_MALLOC_ZONE_CAPACITY 64

unsigned malloc_num_zones = 1;
void* malloc_zones[MACHGATE_MALLOC_ZONE_CAPACITY] = { &default_malloc_zone };

static int is_default_malloc_zone(void* zone)
{
	return !zone || zone == &default_malloc_zone;
}

static struct machgate_malloc_zone* custom_malloc_zone(void* zone)
{
	if (is_default_malloc_zone(zone))
		return NULL;
	return (struct machgate_malloc_zone*)zone;
}

static void* allocation_recorded_zone(const void* ptr)
{
	size_t size = 0;
	void* zone = NULL;

	if (lookup_allocation(ptr, &size, &zone, NULL) && size != 0)
		return zone;
	return NULL;
}

static void* effective_malloc_zone(void* zone, const void* ptr)
{
	void* recorded_zone;

	if (!is_default_malloc_zone(zone))
		return zone;
	recorded_zone = allocation_recorded_zone(ptr);
	if (recorded_zone)
		return recorded_zone;
	return zone;
}

static size_t zone_recorded_size(struct machgate_malloc_zone* zone, void* zone_arg,
                                  const void* ptr, size_t fallback_size)
{
	size_t result = 0;

	if (zone && zone->size)
		result = zone->size(zone_arg, ptr);
	if (result)
		return result;
	if (fallback_size)
		return fallback_size;
	return 1;
}

size_t malloc_zone_size(void* zone, const void* ptr);
void malloc_zone_free(void* zone, void* ptr);

void* malloc_zone_malloc(void* zone, size_t size)
{
	void* caller = MACHGATE_SHIM_CALLER();
	struct machgate_malloc_zone* machgate_zone = custom_malloc_zone(zone);
	if (machgate_zone && machgate_zone->malloc) {
		void* result = machgate_zone->malloc(zone, size);
		if (result) {
			record_allocation_with_zone(result,
			                            zone_recorded_size(machgate_zone, zone, result, size),
			                            zone);
			shim_trace_alloc_event_at("malloc_zone_malloc", result, size, 0,
			                          caller, 1);
		}
		return result;
	}
	return shim_malloc_impl_at(size, caller);
}

void* malloc_zone_realloc(void* zone, void* ptr, size_t size)
{
	void* caller = MACHGATE_SHIM_CALLER();
	void* effective_zone = effective_malloc_zone(zone, ptr);
	struct machgate_malloc_zone* machgate_zone = custom_malloc_zone(effective_zone);
	if (machgate_zone && machgate_zone->realloc) {
		size_t old_size = malloc_zone_size(effective_zone, ptr);
		void* result = machgate_zone->realloc(effective_zone, ptr, size);
		if (result || size == 0)
			forget_allocation(ptr);
		if (result) {
			record_allocation_with_zone(result,
			                            zone_recorded_size(machgate_zone, effective_zone, result, size),
			                            effective_zone);
			shim_trace_alloc_event_at("malloc_zone_realloc", result, size, old_size,
			                          caller, old_size != 0);
		}
		return result;
	}
	return shim_realloc_impl_at(ptr, size, caller);
}

void malloc_zone_free(void* zone, void* ptr)
{
	void* caller = MACHGATE_SHIM_CALLER();
	void* effective_zone = effective_malloc_zone(zone, ptr);
	struct machgate_malloc_zone* machgate_zone = custom_malloc_zone(effective_zone);
	if (machgate_zone && machgate_zone->free) {
		size_t old_size = malloc_zone_size(effective_zone, ptr);
		shim_trace_alloc_event_at("malloc_zone_free", ptr, old_size, old_size,
		                          caller, old_size != 0);
		machgate_zone->free(effective_zone, ptr);
		forget_allocation(ptr);
		return;
	}
	shim_free_impl_at(ptr, caller);
}

size_t malloc_zone_size(void* zone, const void* ptr)
{
	void* effective_zone = effective_malloc_zone(zone, ptr);
	struct machgate_malloc_zone* machgate_zone = custom_malloc_zone(effective_zone);
	if (machgate_zone && machgate_zone->size)
		return machgate_zone->size(effective_zone, ptr);
	return malloc_size(ptr);
}

void* malloc_zone_calloc(void* zone, size_t count, size_t size)
{
	void* caller = MACHGATE_SHIM_CALLER();
	struct machgate_malloc_zone* machgate_zone = custom_malloc_zone(zone);
	if (machgate_zone && machgate_zone->calloc) {
		size_t total = count * size;
		void* result = machgate_zone->calloc(zone, count, size);
		if (result) {
			record_allocation_with_zone(result,
			                            zone_recorded_size(machgate_zone, zone, result, total),
			                            zone);
			shim_trace_alloc_event_at("malloc_zone_calloc", result, total, 0,
			                          caller, 1);
		}
		return result;
	}
	return shim_calloc_impl_at(count, size, caller);
}

void* malloc_zone_valloc(void* zone, size_t size)
{
	void* caller = MACHGATE_SHIM_CALLER();
	struct machgate_malloc_zone* machgate_zone = custom_malloc_zone(zone);
	if (machgate_zone && machgate_zone->valloc) {
		void* result = machgate_zone->valloc(zone, size);
		if (result) {
			record_allocation_with_zone(result,
			                            zone_recorded_size(machgate_zone, zone, result, size),
			                            zone);
			shim_trace_alloc_event_at("malloc_zone_valloc", result, size, 0,
			                          caller, 1);
		}
		return result;
	}

	size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
	void* result = NULL;

	if (page_size == 0)
		page_size = 4096;
	if (shim_posix_memalign_impl_at(&result, page_size, size, caller) != 0)
		return NULL;
	return result;
}

void malloc_zone_destroy(void* zone)
{
	struct machgate_malloc_zone* machgate_zone = custom_malloc_zone(zone);
	if (machgate_zone && machgate_zone->destroy)
		machgate_zone->destroy(zone);
}

unsigned malloc_zone_batch_malloc(void* zone, size_t size,
                                  void** results, unsigned count)
{
	void* caller = MACHGATE_SHIM_CALLER();
	struct machgate_malloc_zone* machgate_zone = custom_malloc_zone(zone);
	unsigned index;

	if (machgate_zone && machgate_zone->batch_malloc) {
		unsigned result = machgate_zone->batch_malloc(zone, size, results, count);
		for (index = 0; index < result; index++) {
			record_allocation_with_zone(results[index],
			                            zone_recorded_size(machgate_zone, zone, results[index], size),
			                            zone);
			shim_trace_alloc_event_at("malloc_zone_batch_malloc", results[index],
			                          size, 0, caller, 1);
		}
		return result;
	}

	for (index = 0; index < count; index++) {
		results[index] = shim_malloc_impl_at(size, caller);
		if (!results[index])
			break;
	}
	return index;
}

void malloc_zone_batch_free(void* zone, void** pointers,
                            unsigned count)
{
	struct machgate_malloc_zone* machgate_zone = custom_malloc_zone(zone);

	if (machgate_zone && machgate_zone->batch_free) {
		void* caller = MACHGATE_SHIM_CALLER();
		for (unsigned index = 0; index < count; index++) {
			size_t old_size = malloc_zone_size(zone, pointers[index]);
			shim_trace_alloc_event_at("malloc_zone_batch_free",
			                          pointers[index], old_size, old_size,
			                          caller, old_size != 0);
		}
		machgate_zone->batch_free(zone, pointers, count);
		for (unsigned index = 0; index < count; index++)
			forget_allocation(pointers[index]);
		return;
	}

	for (unsigned index = 0; index < count; index++)
		malloc_zone_free(zone, pointers[index]);
}

void* malloc_zone_memalign(void* zone, size_t alignment, size_t size)
{
	void* caller = MACHGATE_SHIM_CALLER();
	struct machgate_malloc_zone* machgate_zone = custom_malloc_zone(zone);
	void* result = NULL;

	if (alignment < sizeof(void*))
		alignment = sizeof(void*);
	if (machgate_zone && machgate_zone->memalign) {
		result = machgate_zone->memalign(zone, alignment, size);
		if (result) {
			record_allocation_with_zone(result,
			                            zone_recorded_size(machgate_zone, zone, result, size),
			                            zone);
			shim_trace_alloc_event_at("malloc_zone_memalign", result, size, alignment,
			                          caller, 1);
		}
		return result;
	}
	if (shim_posix_memalign_impl_at(&result, alignment, size, caller) != 0)
		return NULL;
	return result;
}

void malloc_zone_free_definite_size(void* zone, void* ptr, size_t size)
{
	void* caller = MACHGATE_SHIM_CALLER();
	void* effective_zone = effective_malloc_zone(zone, ptr);
	struct machgate_malloc_zone* machgate_zone = custom_malloc_zone(effective_zone);
	if (machgate_zone && machgate_zone->free_definite_size) {
		shim_trace_alloc_event_at("malloc_zone_free_definite_size", ptr, size, size,
		                          caller, 1);
		machgate_zone->free_definite_size(effective_zone, ptr, size);
		forget_allocation(ptr);
		return;
	}
	malloc_zone_free(effective_zone, ptr);
}

size_t malloc_zone_pressure_relief(void* zone, size_t goal)
{
	struct machgate_malloc_zone* machgate_zone = custom_malloc_zone(zone);
	if (machgate_zone && machgate_zone->pressure_relief)
		return machgate_zone->pressure_relief(zone, goal);
	(void)goal;
	return 0;
}

int malloc_zone_claimed_address(void* zone, void* ptr)
{
	void* recorded_zone = allocation_recorded_zone(ptr);
	struct machgate_malloc_zone* machgate_zone = custom_malloc_zone(zone);

	if (recorded_zone)
		return is_default_malloc_zone(zone) || recorded_zone == zone;
	if (machgate_zone && machgate_zone->claimed_address)
		return machgate_zone->claimed_address(zone, ptr);
	return is_default_malloc_zone(zone) && malloc_usable_size(ptr) > 0;
}

void* malloc_zone_malloc_with_options(void* zone, size_t alignment,
                                      size_t size, unsigned options)
{
	void* caller = MACHGATE_SHIM_CALLER();
	struct machgate_malloc_zone* machgate_zone = custom_malloc_zone(zone);
	if (machgate_zone && machgate_zone->malloc_with_options) {
		void* result = machgate_zone->malloc_with_options(zone, alignment, size, options);
		if (result) {
			record_allocation_with_zone(result,
			                            zone_recorded_size(machgate_zone, zone, result, size),
			                            zone);
			shim_trace_alloc_event_at("malloc_zone_malloc_with_options", result,
			                          size, alignment, caller, 1);
		}
		return result;
	}
	(void)options;
	if (alignment)
		return malloc_zone_memalign(zone, alignment, size);
	return malloc_zone_malloc(zone, size);
}

static struct machgate_malloc_zone default_malloc_zone = {
	NULL,
	NULL,
	malloc_zone_size,
	malloc_zone_malloc,
	malloc_zone_calloc,
	malloc_zone_valloc,
	malloc_zone_free,
	malloc_zone_realloc,
	malloc_zone_destroy,
	"MachGate default malloc zone",
	malloc_zone_batch_malloc,
	malloc_zone_batch_free,
	NULL,
	9,
	malloc_zone_memalign,
	malloc_zone_free_definite_size,
	malloc_zone_pressure_relief,
	malloc_zone_claimed_address,
	malloc_zone_free,
	malloc_zone_malloc_with_options,
	malloc_zone_memalign
};

void* malloc_default_zone(void)
{
	return &default_malloc_zone;
}

void* malloc_create_zone(size_t start_size, unsigned int flags)
{
	(void)start_size;
	(void)flags;
	return &default_malloc_zone;
}

void malloc_destroy_zone(void* zone)
{
	malloc_zone_destroy(zone);
}

size_t malloc_good_size(size_t size)
{
	return size;
}

const char* malloc_get_zone_name(void* zone)
{
	struct machgate_malloc_zone* machgate_zone = zone;

	if (!machgate_zone)
		machgate_zone = &default_malloc_zone;
	return machgate_zone->zone_name;
}

void* malloc_logger = NULL;

void malloc_zone_register(void* zone)
{
	if (!zone)
		return;
	for (unsigned index = 0; index < malloc_num_zones; index++) {
		if (malloc_zones[index] == zone)
			return;
	}
	if (malloc_num_zones < MACHGATE_MALLOC_ZONE_CAPACITY)
		malloc_zones[malloc_num_zones++] = zone;
}

void malloc_zone_unregister(void* zone)
{
	if (!zone || zone == &default_malloc_zone)
		return;
	for (unsigned index = 0; index < malloc_num_zones; index++) {
		if (malloc_zones[index] != zone)
			continue;
		for (unsigned next = index + 1; next < malloc_num_zones; next++)
			malloc_zones[next - 1] = malloc_zones[next];
		malloc_zones[--malloc_num_zones] = NULL;
		return;
	}
}

void malloc_set_zone_name(void* zone, const char* name)
{
	struct machgate_malloc_zone* machgate_zone = zone;
	if (!machgate_zone)
		return;
	machgate_zone->zone_name = name;
}

void* malloc_zone_from_ptr(const void* ptr)
{
	size_t size;
	void* zone = NULL;

	if (!ptr)
		return NULL;
	if (lookup_allocation(ptr, &size, &zone, NULL) && size != 0)
		return zone ? zone : &default_malloc_zone;
	if (malloc_usable_size((void*)ptr) > 0)
		return &default_malloc_zone;
	return NULL;
}

int malloc_get_all_zones(void* task, void* reader, void*** zones, unsigned* count)
{
	(void)task;
	(void)reader;
	if (zones)
		*zones = malloc_zones;
	if (count)
		*count = malloc_num_zones;
	return 0;
}

size_t malloc_size(const void* ptr)
{
	size_t size = 0;
	size_t result;

	if (!ptr)
		return 0;
	if (lookup_allocation_size(ptr, &size) && size != 0)
		return size;
	result = malloc_usable_size((void*)ptr);
	if (result == 0 && shim_alloc_trace_enabled())
		fprintf(stderr, "libsystem_shim: malloc_size zero foreign ptr=%p\n", ptr);
	return result;
}

/* ===== Guest Objective-C class dispatch =====
 *
 * Guest images define their own ObjC classes (RBMessageBus,
 * RBContactsProtocol, mock implementations). objc_msgSend receives their
 * class objects or instances but the shim object model cannot answer; the
 * real runtime would walk the class metadata. machgate core registers each
 * __objc_classlist entry (and its metaclass) here, and msgSend falls back
 * to a method-list lookup that tail-calls the guest IMP with the original
 * argument registers restored.
 */

#define GUEST_OBJC_MAX_CLASSES 4096
#define GUEST_OBJC_MAX_STACK_ARGS 16

struct guest_objc_method {
	const char* selector;
	void* imp;
};

struct guest_objc_class_entry {
	void* class_ptr;
	void* metaclass_ptr;
	struct guest_objc_method* instance_methods;
	int instance_method_count;
	struct guest_objc_method* class_methods;
	int class_method_count;
	void** cached_superclass;
};

static struct guest_objc_class_entry guest_objc_classes[GUEST_OBJC_MAX_CLASSES];
static int guest_objc_class_count;
static pthread_mutex_t guest_objc_mutex = PTHREAD_MUTEX_INITIALIZER;

struct guest_objc_layout {
	void* isa;
	void* superclass;
	void* cache[2];
	uint64_t data_bits;
};

static struct guest_objc_class_entry* guest_objc_find_class(void* class_ptr)
{
	for (int index = 0; index < guest_objc_class_count; index++) {
		if (guest_objc_classes[index].class_ptr == class_ptr)
			return &guest_objc_classes[index];
	}
	return NULL;
}

static int guest_objc_readable(const void* address, size_t size)
{
	static pthread_mutex_t maps_mutex = PTHREAD_MUTEX_INITIALIZER;
	static uintptr_t* map_ranges;
	static size_t map_range_count;
	static size_t map_range_capacity;
	static int maps_loaded;

	if (!address)
		return 0;

	int attempts;

	if (!address)
		return 0;

	for (attempts = 0; attempts < 2; attempts++) {
		pthread_mutex_lock(&maps_mutex);
		if (!maps_loaded) {
			FILE* maps = fopen("/proc/self/maps", "r");
			if (maps) {
				char line[512];
				while (fgets(line, sizeof(line), maps)) {
					uintptr_t start;
					uintptr_t end;
					if (sscanf(line, "%lx-%lx", &start, &end) != 2)
						continue;
					if (map_range_count == map_range_capacity) {
						size_t capacity = map_range_capacity ? map_range_capacity * 2 : 512;
						uintptr_t* ranges = realloc(map_ranges, capacity * 2 * sizeof(uintptr_t));
						if (!ranges)
							break;
						map_ranges = ranges;
						map_range_capacity = capacity;
					}
					map_ranges[map_range_count * 2] = start;
					map_ranges[map_range_count * 2 + 1] = end;
					map_range_count++;
				}
				fclose(maps);
			}
			maps_loaded = 1;
		}
		pthread_mutex_unlock(&maps_mutex);

		uintptr_t address_start = (uintptr_t)address;
		uintptr_t address_end = address_start + size;
		for (size_t index = 0; index < map_range_count; index++) {
			if (address_start >= map_ranges[index * 2] &&
			    address_end <= map_ranges[index * 2 + 1])
				return 1;
		}

		pthread_mutex_lock(&maps_mutex);
		map_range_count = 0;
		maps_loaded = 0;
		pthread_mutex_unlock(&maps_mutex);
	}
	return 0;
}

static struct guest_objc_method* guest_objc_parse_methods(void* class_ptr,
                                                          int* method_count)
{
#define SHIM_OBJC_MAGIC_FORWARD 0x4F424A43
	struct shim_objc_header_forward {
		uint32_t magic;
	};
	struct shim_objc_header_forward* shim_header = class_ptr;
	struct guest_objc_layout* layout = class_ptr;

	if (shim_header && shim_header->magic == SHIM_OBJC_MAGIC_FORWARD)
		return NULL;
	uint64_t data_bits;
	uint32_t* class_ro;
	uint64_t* base_methods_slot;
	uint32_t* method_list;
	struct guest_objc_method* methods;
	int count;
	int entsize;
	int relative;

	*method_count = 0;
	if (!guest_objc_readable(class_ptr, sizeof(*layout)))
		return NULL;

	data_bits = layout->data_bits & ~7ULL;
	if (data_bits < 0x1000 || !guest_objc_readable((void*)data_bits, 48))
		return NULL;

	class_ro = (uint32_t*)data_bits;
	base_methods_slot = (uint64_t*)((char*)class_ro + 32);
	if (!*base_methods_slot || !guest_objc_readable((void*)*base_methods_slot, 8))
		return NULL;

	method_list = (uint32_t*)*base_methods_slot;
	entsize = method_list[0] & 0xffffu;
	relative = (method_list[0] & 0x80000000u) != 0;
	count = (int)method_list[1];
	if (count <= 0 || count > 4096 || entsize < 12)
		return NULL;
	if (!guest_objc_readable(method_list, 8 + count * entsize))
		return NULL;

	methods = calloc(count, sizeof(*methods));
	if (!methods)
		return NULL;

	for (int index = 0; index < count; index++) {
		char* entry = (char*)method_list + 8 + index * entsize;
		const char* selector;
		void* imp;

		if (relative) {
			int32_t selector_offset = *(int32_t*)entry;
			int32_t imp_offset = *(int32_t*)(entry + 8);
			void** selector_ref = (void**)(entry + selector_offset);
			if (!guest_objc_readable(selector_ref, sizeof(void*))) {
				methods[index].selector = NULL;
				methods[index].imp = NULL;
				continue;
			}
			selector = (const char*)*selector_ref;
			if (selector && !guest_objc_readable(selector, 1))
				selector = NULL;
			imp = entry + 8 + imp_offset;
		} else {
			selector = *(const char**)entry;
			imp = *(void**)(entry + 16);
			if (selector && !guest_objc_readable(selector, 1)) {
				methods[index].selector = NULL;
				methods[index].imp = NULL;
				continue;
			}
		}
		methods[index].selector = selector;
		methods[index].imp = imp;
	}

	*method_count = count;
	return methods;
}

static void guest_objc_register_one(void* class_ptr)
{
	struct guest_objc_layout* layout = class_ptr;
	struct guest_objc_class_entry* entry;

	if (!class_ptr || guest_objc_class_count >= GUEST_OBJC_MAX_CLASSES)
		return;
	if (!guest_objc_readable(class_ptr, sizeof(*layout)))
		return;

	pthread_mutex_lock(&guest_objc_mutex);
	if (guest_objc_find_class(class_ptr)) {
		pthread_mutex_unlock(&guest_objc_mutex);
		return;
	}

	entry = &guest_objc_classes[guest_objc_class_count++];
	entry->class_ptr = class_ptr;
	entry->metaclass_ptr = layout->isa;
	entry->instance_methods = guest_objc_parse_methods(
		class_ptr, &entry->instance_method_count);
	entry->class_methods = guest_objc_parse_methods(
		layout->isa, &entry->class_method_count);
	pthread_mutex_unlock(&guest_objc_mutex);
}

void machgate_shim_register_objc_class(void* class_ptr)
{
	struct guest_objc_layout* layout = class_ptr;

	if (!class_ptr)
		return;
	guest_objc_register_one(class_ptr);
	guest_objc_register_one(layout->isa);
}

static void* guest_objc_lookup_imp(struct guest_objc_class_entry* entry,
                                   int use_class_methods, const char* selector)
{
	struct guest_objc_method* methods;
	int method_count;

	if (!entry || !selector)
		return NULL;

	methods = use_class_methods ? entry->class_methods
	                            : entry->instance_methods;
	method_count = use_class_methods ? entry->class_method_count
	                                  : entry->instance_method_count;
	for (int index = 0; index < method_count; index++) {
		if (methods[index].selector &&
		    strcmp(methods[index].selector, selector) == 0)
			return methods[index].imp;
	}
	return NULL;
}

struct shim_objc_forwarder_args {
	void* imp;
	uint64_t stack_arg_count;
	void* receiver;
	void* sel;
	void* sret;
	uintptr_t a2;
	uintptr_t a3;
	uintptr_t a4;
	uintptr_t a5;
	uintptr_t a6;
	uintptr_t a7;
	const uintptr_t* guest_sp;
};

#if defined(__aarch64__)
void* shim_objc_forwarder(void* args);
__asm__(
	".text\n"
	".global shim_objc_forwarder\n"
	".type shim_objc_forwarder, %function\n"
	"shim_objc_forwarder:\n"
	"stp x29, x30, [sp, #-208]!\n"
	"mov x29, sp\n"
	"stp x19, x20, [sp, #16]\n"
	"stp x21, x22, [sp, #32]\n"
	"mov x20, x0\n"
	"sub sp, sp, #160\n"
	"ldr x9, [x20, #8]\n"
	"mov x11, #0\n"
	"cmp x9, #16\n"
	"bhi 1f\n"
	"ldr x10, [x20, #88]\n"
	"2:\n"
	"cmp x11, x9\n"
	"b.eq 1f\n"
	"ldr x12, [x10, x11, lsl #3]\n"
	"str x12, [sp, x11, lsl #3]\n"
	"add x11, x11, #1\n"
	"b 2b\n"
	"1:\n"
	"ldr x0, [x20, #16]\n"
	"ldr x1, [x20, #24]\n"
	"ldr x2, [x20, #40]\n"
	"ldr x3, [x20, #48]\n"
	"ldr x4, [x20, #56]\n"
	"ldr x5, [x20, #64]\n"
	"ldr x6, [x20, #72]\n"
	"ldr x7, [x20, #80]\n"
	"ldr x8, [x20, #32]\n"
	"ldr x9, [x20, #0]\n"
	"blr x9\n"
	"mov sp, x29\n"
	"ldp x19, x20, [sp, #16]\n"
	"ldp x21, x22, [sp, #32]\n"
	"ldp x29, x30, [sp], #208\n"
	"ret\n"
);
#endif

void* shim_objc_msgSend_call_guest_imp(void* imp, void* receiver, void* sel,
                                       void* sret,
                                       const uintptr_t* register_args,
                                       const uintptr_t* guest_stack_args)
{
#if defined(__aarch64__)
	struct shim_objc_forwarder_args forwarder_args;

	forwarder_args.imp = imp;
	forwarder_args.stack_arg_count = 0;
	forwarder_args.receiver = receiver;
	forwarder_args.sel = sel;
	forwarder_args.sret = sret;
	forwarder_args.a2 = register_args[0];
	forwarder_args.a3 = register_args[1];
	forwarder_args.a4 = register_args[2];
	forwarder_args.a5 = register_args[3];
	forwarder_args.a6 = register_args[4];
	forwarder_args.a7 = register_args[5];
	forwarder_args.guest_sp = guest_stack_args;
	if (guest_stack_args)
		forwarder_args.stack_arg_count = GUEST_OBJC_MAX_STACK_ARGS;
	return shim_objc_forwarder(&forwarder_args);
#else
	typedef void* (*imp_fn)(void*, void*, ...);
	imp_fn guest_imp = (imp_fn)imp;
	return guest_imp(receiver, sel, register_args[0], register_args[1],
	                 register_args[2], register_args[3], register_args[4]);
#endif
}

static void* guest_objc_alloc_instance(void* class_object)
{
	struct guest_objc_layout* layout = class_object;
	uint64_t data_bits = layout->data_bits & ~7ULL;
	uint32_t* class_ro = (uint32_t*)data_bits;
	uint32_t instance_size = class_ro[2];
	void* instance;

	if (instance_size < 64)
		instance_size = 64;
	instance = calloc(1, instance_size);
	if (!instance)
		return NULL;
	*(void**)instance = class_object;
	return instance;
}

static int shim_objc_msgsend_trace_enabled(void)
{
	static int cached_result = -1;

	if (cached_result < 0) {
		const char* value = getenv("MACHGATE_TRACE_OBJC_MSGSEND");
		cached_result = value && value[0] && strcmp(value, "0") != 0;
	}
	return cached_result;
}

static int shim_objc_guest_handled(void* receiver)
{
	void* class_ptr;

	if (!receiver)
		return 0;
	if (guest_objc_find_class(receiver))
		return 1;
	class_ptr = *(void**)receiver;
	return guest_objc_find_class(class_ptr) != NULL;
}

static void* guest_objc_lookup_imp_chained(void* start_class,
                                           int use_class_methods,
                                           const char* selector)
{
	struct guest_objc_layout* layout = start_class;
	void* class_ptr = start_class;

	for (int depth = 0; depth < 16 && class_ptr; depth++) {
		struct guest_objc_class_entry* entry =
			guest_objc_find_class(class_ptr);
		if (entry) {
			void* imp = guest_objc_lookup_imp(entry, use_class_methods,
			                                   selector);
			if (imp)
				return imp;
		}
		if (!guest_objc_readable((char*)class_ptr + 8, sizeof(void*)))
			return NULL;
		class_ptr = *(void**)((char*)class_ptr + 8);
	}
	(void)layout;
	return NULL;
}

static void* shim_objc_guest_root_fallback(void* receiver, void* class_ptr,
                                          const char* selector)
{
	if (strcmp(selector, "class") == 0)
		return class_ptr ? class_ptr : receiver;
	if (strcmp(selector, "init") == 0 ||
	    strcmp(selector, "retain") == 0 ||
	    strcmp(selector, "autorelease") == 0)
		return receiver;
	if (strcmp(selector, "hash") == 0)
		return (void*)(uintptr_t)(uintptr_t)receiver;
	if (strcmp(selector, "alloc") == 0 ||
	    strcmp(selector, "allocWithZone:") == 0 ||
	    strcmp(selector, "new") == 0)
		return guest_objc_alloc_instance(class_ptr ? class_ptr : receiver);
	return NULL;
}

static void* shim_objc_try_guest_dispatch(void* receiver, const char* selector,
                                          void* sret,
                                          const uintptr_t* register_args,
                                          const uintptr_t* guest_stack_args)
{
	struct guest_objc_class_entry* entry;
	void* imp;
	void* class_ptr;

	if (!receiver || !selector)
		return NULL;

	entry = guest_objc_find_class(receiver);
	if (entry) {
		imp = guest_objc_lookup_imp_chained(receiver, 1, selector);
		if (imp)
			return shim_objc_msgSend_call_guest_imp(
				imp, receiver, (void*)selector, sret,
				register_args, guest_stack_args);
		return shim_objc_guest_root_fallback(receiver, receiver, selector);
	}

	if (!guest_objc_readable(receiver, sizeof(void*)))
		return NULL;
	class_ptr = *(void**)receiver;
	entry = guest_objc_find_class(class_ptr);
	if (entry) {
		imp = guest_objc_lookup_imp_chained(class_ptr, 0, selector);
		if (imp)
			return shim_objc_msgSend_call_guest_imp(
				imp, receiver, (void*)selector, sret,
				register_args, guest_stack_args);
		return shim_objc_guest_root_fallback(receiver, class_ptr,
		                                     selector);
	}
	return NULL;
}

#define SHIM_OBJC_MAGIC 0x4F424A43

enum shim_objc_kind {
	SHIM_OBJC_CLASS_NSPROCESSINFO,
	SHIM_OBJC_INSTANCE_NSPROCESSINFO,
	SHIM_OBJC_CLASS_NSSTRING,
	SHIM_OBJC_STRING,
	SHIM_OBJC_ARRAY,
	SHIM_OBJC_CLASS_NSHTTPCOOKIE,
	SHIM_OBJC_INSTANCE_NSHTTPCOOKIE,
	SHIM_OBJC_CLASS_NSHTTPCOOKIESTORAGE,
	SHIM_OBJC_INSTANCE_NSHTTPCOOKIESTORAGE,
	SHIM_OBJC_CLASS_NSURL,
	SHIM_OBJC_INSTANCE_NSURL,
	SHIM_OBJC_CLASS_NSMUTABLEDICTIONARY,
	SHIM_OBJC_INSTANCE_NSMUTABLEDICTIONARY,
	SHIM_OBJC_CLASS_NSCHARACTERSET,
	SHIM_OBJC_INSTANCE_NSCHARACTERSET,
	SHIM_OBJC_CLASS_NSURLCOMPONENTS,
	SHIM_OBJC_INSTANCE_NSURLCOMPONENTS,
	SHIM_OBJC_CLASS_NSFILEMANAGER,
	SHIM_OBJC_INSTANCE_NSFILEMANAGER,
	SHIM_OBJC_CLASS_NSFILEHANDLE,
	SHIM_OBJC_INSTANCE_NSFILEHANDLE,
	SHIM_OBJC_CLASS_NSDATA,
	SHIM_OBJC_INSTANCE_NSDATA,
	SHIM_OBJC_CLASS_NSDATE,
	SHIM_OBJC_INSTANCE_NSERROR,
	SHIM_OBJC_CLASS_NSUSERDEFAULTS,
	SHIM_OBJC_INSTANCE_NSUSERDEFAULTS,
	SHIM_OBJC_CLASS_NSNUMBER,
	SHIM_OBJC_INSTANCE_NSNUMBER,
	SHIM_OBJC_CLASS_NSARRAY,
	SHIM_OBJC_CLASS_NSMUTABLEARRAY,
	SHIM_OBJC_INSTANCE_NSMUTABLEARRAY,
	SHIM_OBJC_CLASS_NSDICTIONARY,
	SHIM_OBJC_CLASS_NSJSONSERIALIZATION,
	SHIM_OBJC_CLASS_NSMUTABLESTRING,
};

struct shim_objc_header {
	uint32_t magic;
	uint32_t kind;
};

struct shim_objc_string {
	struct shim_objc_header header;
	char utf8[];
};

struct shim_objc_array {
	struct shim_objc_header header;
	uint32_t count;
	struct shim_objc_string* elements[];
};

struct shim_cookie {
	char name[256];
	char value[4096];
	char domain[256];
	char path[1024];
	int secure;
	int http_only;
};

struct shim_objc_cookie {
	struct shim_objc_header header;
	struct shim_cookie cookie;
};

struct shim_objc_dictionary {
	struct shim_objc_header header;
	uint32_t pair_count;
	uint32_t pair_capacity;
	struct shim_objc_string* keys[];
};

struct shim_objc_url_components {
	struct shim_objc_header header;
	struct shim_objc_string* scheme;
	struct shim_objc_string* host;
	struct shim_objc_string* path;
};

struct shim_objc_header OBJC_CLASS_$_NSProcessInfo = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSPROCESSINFO,
};

struct shim_objc_header OBJC_CLASS_$_NSString = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSSTRING,
};

#define SHIM_COOKIE_PROPERTY(variable, text) \
	static const struct shim_objc_string variable = { \
		{ SHIM_OBJC_MAGIC, SHIM_OBJC_STRING }, text }

SHIM_COOKIE_PROPERTY(shim_cookie_domain_string, "Domain");
SHIM_COOKIE_PROPERTY(shim_cookie_expires_string, "Expires");
SHIM_COOKIE_PROPERTY(shim_cookie_name_string, "Name");
SHIM_COOKIE_PROPERTY(shim_cookie_originurl_string, "OriginURL");
SHIM_COOKIE_PROPERTY(shim_cookie_path_string, "Path");
SHIM_COOKIE_PROPERTY(shim_cookie_value_string, "Value");

const void* const NSHTTPCookieDomain = &shim_cookie_domain_string;
const void* const NSHTTPCookieExpires = &shim_cookie_expires_string;
const void* const NSHTTPCookieName = &shim_cookie_name_string;
const void* const NSHTTPCookieOriginURL = &shim_cookie_originurl_string;
const void* const NSHTTPCookiePath = &shim_cookie_path_string;
const void* const NSHTTPCookieValue = &shim_cookie_value_string;

struct shim_objc_header OBJC_CLASS_$_NSFileManager = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSFILEMANAGER,
};

struct shim_objc_header OBJC_CLASS_$_NSFileHandle = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSFILEHANDLE,
};

struct shim_objc_header OBJC_CLASS_$_NSData = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSDATA,
};

struct shim_objc_data {
	struct shim_objc_header header;
	size_t length;
	char bytes[];
};

struct shim_objc_filehandle {
	struct shim_objc_header header;
	int fd;
};

static struct shim_objc_header shim_processinfo_singleton = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_INSTANCE_NSPROCESSINFO,
};

static struct shim_objc_header shim_cookie_storage_singleton = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_INSTANCE_NSHTTPCOOKIESTORAGE,
};

static struct shim_objc_header shim_filemanager_singleton = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_INSTANCE_NSFILEMANAGER,
};

static struct shim_objc_header shim_nserror_singleton = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_INSTANCE_NSERROR,
};

static struct shim_objc_header shim_characterset_singleton = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_INSTANCE_NSCHARACTERSET,
};

static pthread_mutex_t shim_cookie_jar_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct shim_cookie shim_cookie_jar[256];
static uint32_t shim_cookie_jar_count;

static const uint32_t shim_objc_os_version[3] = {15, 0, 0};

static int shim_objc_is_object(const void* receiver, uint32_t kind)
{
	const struct shim_objc_header* header = receiver;

	if (!header)
		return 0;
	return header->magic == SHIM_OBJC_MAGIC && header->kind == kind;
}

static void* shim_objc_make_string(const char* text)
{
	size_t length = strlen(text);
	struct shim_objc_string* result = malloc(sizeof(*result) + length + 1);
	if (!result)
		return NULL;
	result->header.magic = SHIM_OBJC_MAGIC;
	result->header.kind = SHIM_OBJC_STRING;
	memcpy(result->utf8, text, length + 1);
	return result;
}

static void* shim_objc_make_array(struct shim_objc_string** elements, uint32_t count)
{
	struct shim_objc_array* result = malloc(sizeof(*result) + sizeof(*elements) * (count ? count : 1));
	if (!result)
		return NULL;
	result->header.magic = SHIM_OBJC_MAGIC;
	result->header.kind = SHIM_OBJC_ARRAY;
	result->count = count;
	for (uint32_t element_index = 0; element_index < count; element_index++)
		result->elements[element_index] = elements[element_index];
	return result;
}

static void* shim_objc_make_single_element_array(const char* text)
{
	struct shim_objc_string* element = shim_objc_make_string(text);
	if (!element)
		return NULL;
	return shim_objc_make_array(&element, 1);
}

static const char* shim_objc_string_utf8(void* object);

static char* shim_objc_format_values(const char* format,
                                     const uintptr_t* values,
                                     int value_count)
{
	size_t capacity = strlen(format) * 4 + 256;
	char* result = malloc(capacity);
	size_t position = 0;
	int value_index = 0;

	if (!result)
		return NULL;

	for (const char* cursor = format; *cursor && position + 64 < capacity; cursor++) {
		if (*cursor != '%') {
			result[position++] = *cursor;
			continue;
		}
		const char* spec = cursor + 1;
		if (*spec == '%') {
			result[position++] = '%';
			cursor = spec;
			continue;
		}
		while (*spec == 'l' || *spec == 'z')
			spec++;
		if (*spec == '@' && value_index < value_count) {
			const char* text = shim_objc_string_utf8((void*)values[value_index++]);
			if (!text)
				text = "(null)";
			size_t length = strlen(text);
			if (position + length >= capacity) {
				length = capacity - position - 1;
			}
			memcpy(result + position, text, length);
			position += length;
			cursor = spec;
		} else if ((*spec == 'd' || *spec == 'u') && value_index < value_count) {
			uintptr_t value = values[value_index++];
			int written;
			if (*spec == 'd')
				written = snprintf(result + position, capacity - position, "%ld", (long)value);
			else
				written = snprintf(result + position, capacity - position, "%lu", (unsigned long)value);
			if (written > 0)
				position += (size_t)written;
			cursor = spec;
		} else {
			result[position++] = *cursor;
		}
	}
	result[position] = '\0';
	return result;
}


static const char* shim_objc_string_utf8(void* object)
{
	struct shim_objc_string* shim_string;
	const char* chars;
	size_t length;

	if (!object)
		return NULL;
	if (shim_objc_is_object(object, SHIM_OBJC_STRING))
		return ((struct shim_objc_string*)object)->utf8;

	chars = *(const char**)((const char*)object + 16);
	length = *(const size_t*)((const char*)object + 24);
	if ((uintptr_t)chars < 0x1000 || length == 0 || length > (1 << 20))
		return NULL;
	if (chars[length] != '\0')
		return NULL;
	return chars;
}

static void shim_parse_url_host(const char* url, char* host, size_t host_size)
{
	const char* scheme_end = strstr(url, "://");
	const char* host_start = scheme_end ? scheme_end + 3 : url;
	const char* host_end = strchr(host_start, '/');

	if (!host_end)
		host_end = host_start + strlen(host_start);
	if (!host_end)
		host_end = host_start;
	size_t length = (size_t)(host_end - host_start);
	if (length >= host_size)
		length = host_size - 1;
	memcpy(host, host_start, length);
	host[length] = '\0';
}

static int shim_domain_matches_host(const char* domain, const char* host)
{
	size_t domain_length = strlen(domain);
	size_t host_length = strlen(host);

	if (domain_length == 0 || host_length == 0)
		return 0;

	if (strcmp(domain, host) == 0)
		return 1;

	if (domain[0] != '.')
		return 0;

	if (host_length >= domain_length &&
	    strcmp(host + host_length - domain_length, domain) == 0)
		return 1;

	return host_length + 1 == domain_length &&
	       strncmp(domain + 1, host, host_length) == 0;
}

static void shim_copy_cstring(char* destination, size_t destination_size,
                              const char* source)
{
	size_t length = strlen(source);

	if (length >= destination_size)
		length = destination_size - 1;
	memcpy(destination, source, length);
	destination[length] = '\0';
}

static int shim_cookie_equals(const struct shim_cookie* left,
                              const struct shim_cookie* right)
{
	return strcmp(left->name, right->name) == 0 &&
	       strcmp(left->domain, right->domain) == 0 &&
	       strcmp(left->path, right->path) == 0;
}

static void shim_cookie_jar_remove(const struct shim_cookie* cookie)
{
	for (uint32_t index = 0; index < shim_cookie_jar_count; index++) {
		if (!shim_cookie_equals(&shim_cookie_jar[index], cookie))
			continue;
		for (uint32_t shift = index; shift + 1 < shim_cookie_jar_count; shift++)
			shim_cookie_jar[shift] = shim_cookie_jar[shift + 1];
		shim_cookie_jar_count--;
		return;
	}
}

static void shim_cookie_jar_store(const struct shim_cookie* cookie)
{
	shim_cookie_jar_remove(cookie);
	if (shim_cookie_jar_count >= 256)
		return;
	shim_cookie_jar[shim_cookie_jar_count++] = *cookie;
}

static void* shim_objc_make_cookie(const struct shim_cookie* cookie)
{
	struct shim_objc_cookie* result = malloc(sizeof(*result));

	if (!result)
		return NULL;
	result->header.magic = SHIM_OBJC_MAGIC;
	result->header.kind = SHIM_OBJC_INSTANCE_NSHTTPCOOKIE;
	result->cookie = *cookie;
	return result;
}

static void* shim_objc_make_url(const char* url)
{
	struct shim_objc_string* result =
		shim_objc_make_string(url);

	if (!result)
		return NULL;
	result->header.kind = SHIM_OBJC_INSTANCE_NSURL;
	return result;
}

static void* shim_objc_make_url_components(void)
{
	struct shim_objc_url_components* result = calloc(1, sizeof(*result));

	if (!result)
		return NULL;
	result->header.magic = SHIM_OBJC_MAGIC;
	result->header.kind = SHIM_OBJC_INSTANCE_NSURLCOMPONENTS;
	return result;
}

static int shim_is_url_path_allowed_character(unsigned char character)
{
	if (isalnum(character))
		return 1;
	return strchr("-._~!$&'()*+,;=:@/", character) != NULL;
}

static const char* shim_objc_url_string(void* url)
{
	if (!shim_objc_is_object(url, SHIM_OBJC_INSTANCE_NSURL))
		return NULL;
	return ((struct shim_objc_string*)url)->utf8;
}

static void* shim_objc_make_dictionary(struct shim_objc_string** keys,
                                       struct shim_objc_string** values,
                                       uint32_t pair_count)
{
	uint32_t capacity = pair_count < 8 ? 8 : pair_count;
	struct shim_objc_dictionary* result =
		malloc(sizeof(*result) + sizeof(*keys) * 2 * capacity);

	if (!result)
		return NULL;
	result->header.magic = SHIM_OBJC_MAGIC;
	result->header.kind = SHIM_OBJC_INSTANCE_NSMUTABLEDICTIONARY;
	result->pair_count = pair_count;
	result->pair_capacity = capacity;
	for (uint32_t pair_index = 0; pair_index < pair_count; pair_index++) {
		result->keys[pair_index * 2] = keys[pair_index];
		result->keys[pair_index * 2 + 1] = values[pair_index];
	}
	return result;
}

struct shim_objc_userdefaults {
	struct shim_objc_header header;
	struct shim_objc_dictionary* values;
};

struct shim_objc_number {
	struct shim_objc_header header;
	union {
		long long integer_value;
		double double_value;
	} value;
	int is_floating;
};

struct shim_objc_header OBJC_CLASS_$_NSNumber = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSNUMBER,
};

struct shim_objc_header OBJC_CLASS_$_NSArray = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSARRAY,
};

struct shim_objc_header OBJC_CLASS_$_NSMutableArray = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSMUTABLEARRAY,
};

struct shim_objc_header OBJC_CLASS_$_NSDictionary = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSDICTIONARY,
};

struct shim_objc_header OBJC_CLASS_$_NSJSONSerialization = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSJSONSERIALIZATION,
};

void* __NSDictionary0__;
void* __NSArray0__;

__attribute__((constructor))
static void shim_init_empty_collection_singletons(void)
{
	if (!__NSDictionary0__)
		__NSDictionary0__ = shim_objc_make_dictionary(NULL, NULL, 0);
	if (!__NSArray0__)
		__NSArray0__ = shim_objc_make_array(NULL, 0);
}

static void* shim_objc_make_number(long long integer_value)
{
	struct shim_objc_number* result = malloc(sizeof(*result));
	if (!result)
		return NULL;
	result->header.magic = SHIM_OBJC_MAGIC;
	result->header.kind = SHIM_OBJC_INSTANCE_NSNUMBER;
	result->value.integer_value = integer_value;
	result->is_floating = 0;
	return result;
}

static void shim_objc_dictionary_set(struct shim_objc_dictionary* dictionary,
                                     void* key, void* value)
{
	for (uint32_t pair_index = 0; pair_index < dictionary->pair_count;
	     pair_index++) {
		if (dictionary->keys[pair_index * 2] == key) {
			dictionary->keys[pair_index * 2 + 1] = value;
			return;
		}
	}
	if (dictionary->pair_count >= dictionary->pair_capacity) {
		uint32_t capacity = dictionary->pair_capacity * 2;
		struct shim_objc_dictionary* grown =
			realloc(dictionary,
			        sizeof(*dictionary) + sizeof(void*) * 2 * capacity);
		if (!grown)
			return;
		grown->pair_capacity = capacity;
		dictionary = grown;
	}
	dictionary->keys[dictionary->pair_count * 2] = key;
	dictionary->keys[dictionary->pair_count * 2 + 1] = value;
	dictionary->pair_count++;
}

static void* shim_objc_dictionary_get(
	const struct shim_objc_dictionary* dictionary, void* key)
{
	for (uint32_t pair_index = 0; pair_index < dictionary->pair_count;
	     pair_index++) {
		if (dictionary->keys[pair_index * 2] == key)
			return dictionary->keys[pair_index * 2 + 1];
	}
	return NULL;
}

static void* shim_objc_array_append(struct shim_objc_array* array, void* value)
{
	struct shim_objc_array* grown;
	uint32_t new_count;

	if (!array)
		return NULL;
	grown = realloc(array, sizeof(*array) + sizeof(void*) * (array->count + 1));
	if (!grown)
		return array;
	grown->count++;
	grown->elements[grown->count - 1] = value;
	return grown;
}

static void shim_json_escape_append(const char* text, char* out,
                                    size_t out_size, size_t* position)
{
	for (const char* cursor = text; *cursor; cursor++) {
		char ch = *cursor;
		if (ch == '"' || ch == '\\') {
			if (*position + 2 >= out_size)
				return;
			out[(*position)++] = '\\';
			out[(*position)++] = ch;
		} else if ((unsigned char)ch < 0x20) {
			if (*position + 6 >= out_size)
				return;
			*position += (size_t)snprintf(out + *position,
			                              out_size - *position, "\\u%04x", ch);
		} else {
			if (*position + 1 >= out_size)
				return;
			out[(*position)++] = ch;
		}
	}
	out[*position] = '\0';
}

static char* shim_json_serialize_object(void* object, int depth);

static int shim_json_serializable(const void* object)
{
	if (!object)
		return 1;
	if (!guest_objc_readable(object, sizeof(struct shim_objc_header)))
		return 0;
	if (shim_objc_is_object((void*)object, SHIM_OBJC_STRING) ||
	    shim_objc_is_object((void*)object, SHIM_OBJC_INSTANCE_NSNUMBER) ||
	    shim_objc_is_object((void*)object, SHIM_OBJC_ARRAY) ||
	    shim_objc_is_object((void*)object,
	                        SHIM_OBJC_INSTANCE_NSMUTABLEDICTIONARY))
		return 1;
	const char* chars = *(const char**)((const char*)object + 16);
	size_t length = *(const size_t*)((const char*)object + 24);
	if ((uintptr_t)chars > 0x1000 && length > 0 && length <= (1 << 20) &&
	    guest_objc_readable(chars, length + 1) && chars[length] == '\0')
		return 1;
	return 0;
}

static char* shim_json_serialize_object(void* object, int depth)
{
	size_t capacity = 4096;
	char* out;
	size_t position = 0;

	if (depth > 8)
		return NULL;
	if (!shim_json_serializable(object)) {
		if (object && guest_objc_readable(object, sizeof(void*))) {
			void* class_ptr = *(void**)object;
			if (guest_objc_find_class(class_ptr))
				return NULL;
		}
		if (!object)
			object = NULL;
		else
			return NULL;
	}
	out = malloc(capacity);
	if (!out)
		return NULL;
	out[0] = '\0';

	if (shim_objc_is_object(object, SHIM_OBJC_STRING)) {
		const char* text = ((struct shim_objc_string*)object)->utf8;
		out[position++] = '"';
		shim_json_escape_append(text, out, capacity, &position);
		out[position++] = '"';
		out[position] = '\0';
		return out;
	}
	if (shim_objc_is_object(object, SHIM_OBJC_INSTANCE_NSNUMBER)) {
		const struct shim_objc_number* number = object;
		if (number->is_floating)
			snprintf(out, capacity, "%g", number->value.double_value);
		else
			snprintf(out, capacity, "%lld", number->value.integer_value);
		return out;
	}
	if (shim_objc_is_object(object, SHIM_OBJC_ARRAY)) {
		const struct shim_objc_array* array = object;
		out[position++] = '[';
		for (uint32_t index = 0; index < array->count; index++) {
			char* element = shim_json_serialize_object(
				array->elements[index], depth + 1);
			if (!element) {
				free(out);
				return NULL;
			}
			if (index > 0)
				out[position++] = ',';
			size_t length = strlen(element);
			if (position + length + 2 >= capacity) {
				free(element);
				free(out);
				return NULL;
			}
			memcpy(out + position, element, length);
			position += length;
			free(element);
		}
		out[position++] = ']';
		out[position] = '\0';
		return out;
	}
	if (shim_objc_is_object(object, SHIM_OBJC_INSTANCE_NSMUTABLEDICTIONARY)) {
		const struct shim_objc_dictionary* dictionary = object;
		out[position++] = '{';
		for (uint32_t pair_index = 0;
		     pair_index < dictionary->pair_count; pair_index++) {
			if (pair_index > 0)
				out[position++] = ',';
			char* key_text = shim_json_serialize_object(
				dictionary->keys[pair_index * 2], depth + 1);
			char* value_text = shim_json_serialize_object(
				dictionary->keys[pair_index * 2 + 1], depth + 1);
			if (!key_text || !value_text) {
				free(key_text);
				free(value_text);
				free(out);
				return NULL;
			}
			size_t length = strlen(key_text) + strlen(value_text);
			if (position + length + 3 >= capacity) {
				free(key_text);
				free(value_text);
				free(out);
				return NULL;
			}
			memcpy(out + position, key_text, strlen(key_text));
			position += strlen(key_text);
			out[position++] = ':';
			memcpy(out + position, value_text, strlen(value_text));
			position += strlen(value_text);
			free(key_text);
			free(value_text);
		}
		out[position++] = '}';
		out[position] = '\0';
		return out;
	}
	if (!object) {
		snprintf(out, capacity, "null");
		return out;
	}
	free(out);
	return NULL;
}

static int shim_json_match(const char** cursor, char expected)
{
	while (**cursor == ' ' || **cursor == '\t' || **cursor == '\n' ||
	       **cursor == '\r')
		(*cursor)++;
	if (**cursor != expected)
		return 0;
	(*cursor)++;
	return 1;
}

static char* shim_json_read_string(const char** cursor)
{
	const char* start;
	size_t length;
	char* out;
	size_t position = 0;

	while (**cursor == ' ' || **cursor == '\t')
		(*cursor)++;
	if (**cursor != '"')
		return NULL;
	(*cursor)++;
	start = *cursor;
	while (**cursor && **cursor != '"') {
		if (**cursor == '\\' && *(*cursor + 1))
			(*cursor)++;
		(*cursor)++;
	}
	length = (size_t)(*cursor - start);
	out = malloc(length + 1);
	if (!out)
		return NULL;
	for (size_t index = 0; index < length; index++) {
		char ch = start[index];
		if (ch == '\\' && index + 1 < length)
			ch = start[++index];
		out[position++] = ch;
	}
	out[position] = '\0';
	if (**cursor == '"')
		(*cursor)++;
	return out;
}

static void* shim_json_parse_value(const char** cursor, int depth);

static void* shim_json_parse_value(const char** cursor, int depth)
{
	while (**cursor == ' ' || **cursor == '\t' || **cursor == '\n' ||
	       **cursor == '\r')
		(*cursor)++;

	if (depth > 8)
		return NULL;
	if (**cursor == '"') {
		char* text = shim_json_read_string(cursor);
		if (!text)
			return NULL;
		void* result = shim_objc_make_string(text);
		free(text);
		return result;
	}
	if (strncmp(*cursor, "true", 4) == 0) {
		*cursor += 4;
		return shim_objc_make_number(1);
	}
	if (strncmp(*cursor, "false", 5) == 0) {
		*cursor += 5;
		return shim_objc_make_number(0);
	}
	if (strncmp(*cursor, "null", 4) == 0) {
		*cursor += 4;
		return NULL;
	}
	if (**cursor == '[') {
		(*cursor)++;
		struct shim_objc_array* array =
			shim_objc_make_array(NULL, 0);
		if (!array)
			return NULL;
		while (**cursor && **cursor != ']') {
			void* value = shim_json_parse_value(cursor, depth + 1);
			if (value)
				array = shim_objc_array_append(array, value);
			while (**cursor == ' ' || **cursor == ',')
				(*cursor)++;
		}
		if (**cursor == ']')
			(*cursor)++;
		return array;
	}
	if (**cursor == '{') {
		(*cursor)++;
		struct shim_objc_dictionary* dictionary =
			shim_objc_make_dictionary(NULL, NULL, 0);
		if (!dictionary)
			return NULL;
		while (**cursor && **cursor != '}') {
			char* key_text = shim_json_read_string(cursor);
			if (!key_text)
				break;
			shim_json_match(cursor, ':');
			void* value = shim_json_parse_value(cursor, depth + 1);
			void* key = shim_objc_make_string(key_text);
			free(key_text);
			if (key)
				shim_objc_dictionary_set(dictionary, key, value);
			while (**cursor == ' ' || **cursor == ',')
				(*cursor)++;
		}
		if (**cursor == '}')
			(*cursor)++;
		return dictionary;
	}
	if (**cursor == '-' || (**cursor >= '0' && **cursor <= '9')) {
		long long value = strtoll(*cursor, (char**)cursor, 10);
		return shim_objc_make_number(value);
	}
	return NULL;
}

struct shim_objc_header OBJC_CLASS_$_NSUserDefaults = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSUSERDEFAULTS,
};

#define SHIM_USERDEFAULTS_MAX_SUITES 64

struct shim_userdefaults_suite {
	char name[256];
	struct shim_objc_userdefaults* instance;
	int in_use;
};

static struct shim_userdefaults_suite shim_userdefaults_suites[SHIM_USERDEFAULTS_MAX_SUITES];
static pthread_mutex_t shim_userdefaults_mutex = PTHREAD_MUTEX_INITIALIZER;

static struct shim_objc_userdefaults* shim_userdefaults_for_suite(const char* suite_name)
{
	struct shim_objc_userdefaults* instance;
	int index;

	if (!suite_name)
		suite_name = "";

	for (index = 0; index < SHIM_USERDEFAULTS_MAX_SUITES; index++) {
		if (shim_userdefaults_suites[index].in_use &&
		    strcmp(shim_userdefaults_suites[index].name, suite_name) == 0)
			return shim_userdefaults_suites[index].instance;
	}

	for (index = 0; index < SHIM_USERDEFAULTS_MAX_SUITES; index++) {
		if (!shim_userdefaults_suites[index].in_use)
			break;
	}
	if (index == SHIM_USERDEFAULTS_MAX_SUITES)
		return NULL;

	instance = malloc(sizeof(*instance));
	if (!instance)
		return NULL;
	instance->header.magic = SHIM_OBJC_MAGIC;
	instance->header.kind = SHIM_OBJC_INSTANCE_NSUSERDEFAULTS;
	instance->values = shim_objc_make_dictionary(NULL, NULL, 0);
	if (!instance->values) {
		free(instance);
		return NULL;
	}

	shim_userdefaults_suites[index].in_use = 1;
	snprintf(shim_userdefaults_suites[index].name,
	         sizeof(shim_userdefaults_suites[index].name), "%s",
	         suite_name);
	shim_userdefaults_suites[index].instance = instance;
	return instance;
}

static void shim_userdefaults_set(struct shim_objc_userdefaults* defaults,
                                  struct shim_objc_string* key,
                                  struct shim_objc_string* value)
{
	struct shim_objc_dictionary* dict = defaults->values;
	const char* key_text = shim_objc_string_utf8(key);
	uint32_t pair_index;

	if (!key_text)
		return;

	for (pair_index = 0; pair_index < dict->pair_count; pair_index++) {
		const char* existing = shim_objc_string_utf8(dict->keys[pair_index * 2]);
		if (existing && strcmp(existing, key_text) == 0) {
			dict->keys[pair_index * 2 + 1] = value;
			return;
		}
	}
	if (dict->pair_count < dict->pair_capacity) {
		dict->keys[dict->pair_count * 2] = key;
		dict->keys[dict->pair_count * 2 + 1] = value;
		dict->pair_count++;
		return;
	}

	struct shim_objc_dictionary* grown =
		malloc(sizeof(*grown) +
		       sizeof(*dict->keys) * 2 * dict->pair_capacity * 2);
	if (!grown)
		return;
	memcpy(grown, dict,
	       sizeof(*dict) + sizeof(*dict->keys) * 2 * dict->pair_count);
	grown->pair_capacity = dict->pair_capacity * 2;
	grown->keys[grown->pair_count * 2] = key;
	grown->keys[grown->pair_count * 2 + 1] = value;
	grown->pair_count++;
	free(dict);
	defaults->values = grown;
}

static struct shim_objc_string* shim_userdefaults_get(
	struct shim_objc_userdefaults* defaults, struct shim_objc_string* key)
{
	const struct shim_objc_dictionary* dict = defaults->values;
	const char* key_text = shim_objc_string_utf8(key);
	uint32_t pair_index;

	if (!key_text)
		return NULL;

	for (pair_index = 0; pair_index < dict->pair_count; pair_index++) {
		const char* existing = shim_objc_string_utf8(dict->keys[pair_index * 2]);
		if (existing && strcmp(existing, key_text) == 0)
			return dict->keys[pair_index * 2 + 1];
	}
	return NULL;
}

static void shim_userdefaults_remove(struct shim_objc_userdefaults* defaults,
                                     struct shim_objc_string* key)
{
	struct shim_objc_dictionary* dict = defaults->values;
	const char* key_text = shim_objc_string_utf8(key);
	uint32_t pair_index;

	if (!key_text)
		return;

	for (pair_index = 0; pair_index < dict->pair_count; pair_index++) {
		const char* existing = shim_objc_string_utf8(dict->keys[pair_index * 2]);
		if (existing && strcmp(existing, key_text) == 0) {
			uint32_t last = dict->pair_count - 1;
			dict->keys[pair_index * 2] = dict->keys[last * 2];
			dict->keys[pair_index * 2 + 1] = dict->keys[last * 2 + 1];
			dict->pair_count--;
			return;
		}
	}
}

struct shim_objc_header OBJC_CLASS_$_NSHTTPCookie = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSHTTPCOOKIE,
};

struct shim_objc_header OBJC_CLASS_$_NSHTTPCookieStorage = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSHTTPCOOKIESTORAGE,
};

struct shim_objc_header OBJC_CLASS_$_NSURL = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSURL,
};

struct shim_objc_header OBJC_CLASS_$_NSMutableDictionary = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSMUTABLEDICTIONARY,
};

struct shim_objc_header OBJC_CLASS_$_NSCharacterSet = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSCHARACTERSET,
};

struct shim_objc_header OBJC_CLASS_$_NSURLComponents = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSURLCOMPONENTS,
};

struct shim_objc_header OBJC_CLASS_$_NSDate = {
	SHIM_OBJC_MAGIC, SHIM_OBJC_CLASS_NSDATE,
};

static void* shim_cookie_jar_matching_array(const char* url)
{
	char host[256];
	struct shim_objc_cookie** cookies;
	uint32_t match_count = 0;

	shim_parse_url_host(url ? url : "", host, sizeof(host));
	cookies = malloc(sizeof(*cookies) * (shim_cookie_jar_count ? shim_cookie_jar_count : 1));
	if (!cookies)
		return NULL;

	for (uint32_t index = 0; index < shim_cookie_jar_count; index++) {
		if (!shim_domain_matches_host(shim_cookie_jar[index].domain, host))
			continue;
		void* cookie = shim_objc_make_cookie(&shim_cookie_jar[index]);
		if (!cookie)
			continue;
		cookies[match_count++] = cookie;
	}

	struct shim_objc_array* result =
		malloc(sizeof(*result) + sizeof(*cookies) * (match_count ? match_count : 1));
	if (!result) {
		free(cookies);
		return NULL;
	}
	result->header.magic = SHIM_OBJC_MAGIC;
	result->header.kind = SHIM_OBJC_ARRAY;
	result->count = match_count;
	for (uint32_t index = 0; index < match_count; index++)
		((struct shim_objc_string**)result->elements)[index] =
			(struct shim_objc_string*)cookies[index];
	free(cookies);
	return result;
}

static void* shim_cookie_jar_all_array(void)
{
	struct shim_objc_array* result =
		malloc(sizeof(*result) +
		       sizeof(struct shim_objc_string*) * (shim_cookie_jar_count ? shim_cookie_jar_count : 1));

	if (!result)
		return NULL;
	result->header.magic = SHIM_OBJC_MAGIC;
	result->header.kind = SHIM_OBJC_ARRAY;
	result->count = shim_cookie_jar_count;
	for (uint32_t index = 0; index < shim_cookie_jar_count; index++) {
		void* cookie = shim_objc_make_cookie(&shim_cookie_jar[index]);
		if (!cookie)
			cookie = shim_objc_make_string("");
		((struct shim_objc_string**)result->elements)[index] = cookie;
	}
	return result;
}

static void shim_parse_set_cookie_header(const char* header, struct shim_cookie* cookie)
{
	const char* separator = strchr(header, ';');
	size_t name_value_length = separator ? (size_t)(separator - header) : strlen(header);
	char name_value[2304];
	const char* equals_sign;

	if (name_value_length >= sizeof(name_value))
		name_value_length = sizeof(name_value) - 1;
	memcpy(name_value, header, name_value_length);
	name_value[name_value_length] = '\0';

	memset(cookie, 0, sizeof(*cookie));
	shim_copy_cstring(cookie->path, sizeof(cookie->path), "/");

	equals_sign = strchr(name_value, '=');
	if (!equals_sign) {
		shim_copy_cstring(cookie->name, sizeof(cookie->name), name_value);
		return;
	}
	size_t name_length = (size_t)(equals_sign - name_value);
	if (name_length >= sizeof(cookie->name))
		name_length = sizeof(cookie->name) - 1;
	memcpy(cookie->name, name_value, name_length);
	cookie->name[name_length] = '\0';
	shim_copy_cstring(cookie->value, sizeof(cookie->value), equals_sign + 1);

	const char* attribute = separator;
	while (attribute) {
		attribute++;
		while (*attribute == ' ')
			attribute++;
		if (strncasecmp(attribute, "domain=", 7) == 0) {
			const char* domain = attribute + 7;
			char domain_buffer[254];
			size_t domain_length = 0;
			while (domain[domain_length] && domain[domain_length] != ';' &&
			       domain_length < sizeof(domain_buffer) - 1) {
				domain_buffer[domain_length] = domain[domain_length];
				domain_length++;
			}
			domain_buffer[domain_length] = '\0';
			if (domain_length > 0) {
				if (domain_buffer[0] == '.')
					shim_copy_cstring(cookie->domain, sizeof(cookie->domain),
					                  domain_buffer);
				else
					snprintf(cookie->domain, sizeof(cookie->domain), ".%s",
					         domain_buffer);
			}
		} else if (strncasecmp(attribute, "path=", 5) == 0) {
			const char* path = attribute + 5;
			size_t path_length = 0;
			while (path[path_length] && path[path_length] != ';' &&
			       path_length < sizeof(cookie->path) - 1) {
				cookie->path[path_length] = path[path_length];
				path_length++;
			}
			cookie->path[path_length] = '\0';
		} else if (strncasecmp(attribute, "secure", 6) == 0) {
			cookie->secure = 1;
		} else if (strncasecmp(attribute, "httponly", 8) == 0) {
			cookie->http_only = 1;
		}
		attribute = strchr(attribute, ';');
	}
}

static void* shim_parse_response_header_cookies(void* header_fields, void* url)
{
	const struct shim_objc_dictionary* fields = header_fields;
	const char* set_cookie_value = NULL;
	const char* url_text = shim_objc_url_string(url);
	struct shim_cookie cookie;
	char host[256];

	if (!fields || !shim_objc_is_object(fields, SHIM_OBJC_INSTANCE_NSMUTABLEDICTIONARY))
		return NULL;

	for (uint32_t pair_index = 0; pair_index < fields->pair_count; pair_index++) {
		const char* key = shim_objc_string_utf8(fields->keys[pair_index * 2]);
		if (key && strcasecmp(key, "Set-Cookie") == 0) {
			set_cookie_value = shim_objc_string_utf8(fields->keys[pair_index * 2 + 1]);
			break;
		}
	}
	if (!set_cookie_value)
		return NULL;

	shim_parse_set_cookie_header(set_cookie_value, &cookie);
	if (cookie.domain[0] == '\0') {
		shim_parse_url_host(url_text ? url_text : "", host, sizeof(host));
		shim_copy_cstring(cookie.domain, sizeof(cookie.domain), host);
	}

	struct shim_objc_array* result =
		malloc(sizeof(*result) + sizeof(struct shim_objc_string*));
	if (!result)
		return NULL;
	result->header.magic = SHIM_OBJC_MAGIC;
	result->header.kind = SHIM_OBJC_ARRAY;
	result->count = 1;
	result->elements[0] = shim_objc_make_cookie(&cookie);
	if (!result->elements[0]) {
		free(result);
		return NULL;
	}
	return result;
}

static void* shim_cookie_request_header_fields(void* cookies_array)
{
	struct shim_objc_array* array = cookies_array;
	char* joined = NULL;
	size_t joined_length = 0;
	void* result;

	if (!array || !shim_objc_is_object(array, SHIM_OBJC_ARRAY) || array->count == 0)
		return NULL;

	for (uint32_t index = 0; index < array->count; index++) {
		struct shim_objc_cookie* cookie = (struct shim_objc_cookie*)array->elements[index];
		if (!cookie || !shim_objc_is_object(cookie, SHIM_OBJC_INSTANCE_NSHTTPCOOKIE))
			continue;
		size_t entry_length = strlen(cookie->cookie.name) + 1 +
		                      strlen(cookie->cookie.value) + 3;
		size_t separator_length = joined_length > 0 ? 2 : 0;
		size_t needed = joined_length + separator_length + entry_length;
		char* reallocated = realloc(joined, needed);
		if (!reallocated) {
			free(joined);
			return NULL;
		}
		joined = reallocated;
		if (separator_length == 2) {
			joined[joined_length++] = ';';
			joined[joined_length++] = ' ';
		}
		int written = snprintf(joined + joined_length, needed - joined_length,
		                       "%s=%s", cookie->cookie.name, cookie->cookie.value);
		if (written < 0) {
			free(joined);
			return NULL;
		}
		joined_length += (size_t)written;
	}
	if (!joined)
		return NULL;

	struct shim_objc_string* header_value = shim_objc_make_string(joined);
	free(joined);
	if (!header_value)
		return NULL;

	struct shim_objc_string* header_key = shim_objc_make_string("Cookie");
	if (!header_key) {
		free(header_value);
		return NULL;
	}

	result = shim_objc_make_dictionary(&header_key, &header_value, 1);
	if (!result) {
		free(header_key);
		free(header_value);
	}
	return result;
}

void* shim_objc_msgSend_impl(void* receiver, void* sel, uintptr_t a2, uintptr_t a3,
                             uintptr_t a4, uintptr_t a5, uintptr_t a6, uintptr_t a7,
                             void* sret, const uintptr_t* guest_stack_args)
{
	const char* selector = sel;
	uintptr_t register_args[6];
	void* guest_result;

	if (!receiver || !selector)
		return NULL;

	register_args[0] = a2;
	register_args[1] = a3;
	register_args[2] = a4;
	register_args[3] = a5;
	register_args[4] = a6;
	register_args[5] = a7;

	if (getenv("MACHGATE_TRACE_OBJC_MSGSEND")) {
		void* trace_isa = guest_objc_readable(receiver, sizeof(void*))
			? *(void**)receiver : NULL;
		fprintf(stderr,
		        "libsystem_shim: objc_msgSend [%s] recv=%p isa=%p a2=%p a3=%p a4=%p a5=%p a6=%p a7=%p\n",
		        selector, receiver, trace_isa, (void*)a2, (void*)a3,
		        (void*)a4, (void*)a5, (void*)a6, (void*)a7);
	}

	guest_result = shim_objc_try_guest_dispatch(receiver, selector, sret,
	                                            register_args,
	                                            guest_stack_args);
	if (shim_objc_msgsend_trace_enabled() && shim_objc_guest_handled(receiver))
		fprintf(stderr,
		        "libsystem_shim:   guest dispatch [%s] recv=%p -> %p\n",
		        selector, receiver, guest_result);
	if (guest_result || shim_objc_guest_handled(receiver))
		return guest_result;

	if (strcmp(selector, "processInfo") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSPROCESSINFO))
			return &shim_processinfo_singleton;
	} else if (strcmp(selector, "operatingSystemVersion") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSPROCESSINFO) && sret) {
			memcpy(sret, shim_objc_os_version, sizeof(shim_objc_os_version));
			return sret;
		}
	} else if (strcmp(selector, "stringWithFormat:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSSTRING)) {
			const char* format = shim_objc_string_utf8((void*)a2);
			char* text;
			void* result;
			if (!format)
				return NULL;
			uintptr_t call_values[9] = {0};
			if (guest_stack_args) {
				for (int value_index = 0; value_index < 8; value_index++)
					call_values[value_index] =
						guest_stack_args[value_index];
			}
			text = shim_objc_format_values(format, call_values, 8);
			if (!text)
				return NULL;
			result = shim_objc_make_string(text);
			free(text);
			return result;
		}
	} else if (strcmp(selector, "UTF8String") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_STRING))
			return ((struct shim_objc_string*)receiver)->utf8;
		return (void*)shim_objc_string_utf8(receiver);
	} else if (strcmp(selector, "cStringUsingEncoding:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_STRING))
			return ((struct shim_objc_string*)receiver)->utf8;
		return (void*)shim_objc_string_utf8(receiver);
	} else if (strcmp(selector, "length") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_STRING))
			return (void*)(uintptr_t)strlen(((struct shim_objc_string*)receiver)->utf8);
		if (shim_objc_is_object(receiver, SHIM_OBJC_ARRAY))
			return (void*)(uintptr_t)((struct shim_objc_array*)receiver)->count;
	} else if (strcmp(selector, "count") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_ARRAY))
			return (void*)(uintptr_t)((struct shim_objc_array*)receiver)->count;
	} else if (strcmp(selector, "countByEnumeratingWithState:objects:count:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_ARRAY)) {
			struct shim_objc_array* array = receiver;
			struct {
				unsigned long state;
				void** items_ptr;
				unsigned long* mutations_ptr;
				unsigned long extra[5];
			}* enumeration_state = (void*)a2;
			void** buffer = (void**)a3;
			unsigned long buffer_capacity = (unsigned long)a4;
			unsigned long copied = 0;

			if (!enumeration_state || !buffer || buffer_capacity == 0)
				return NULL;
			if (array->count == 0)
				return NULL;

			if (enumeration_state->state == 0) {
				enumeration_state->state = 1;
				enumeration_state->extra[0] = 0;
				enumeration_state->mutations_ptr = &enumeration_state->extra[0];
				for (uint32_t index = 0; index < array->count && index < buffer_capacity; index++)
					buffer[index] = array->elements[index];
				copied = array->count < buffer_capacity ? array->count : buffer_capacity;
				enumeration_state->items_ptr = buffer;
			} else {
				enumeration_state->state = 0;
				copied = 0;
			}
			return (void*)(uintptr_t)copied;
		}
	} else if (strcmp(selector, "objectAtIndex:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_ARRAY)) {
			const struct shim_objc_array* array = receiver;
			if (a2 < array->count)
				return array->elements[a2];
		}
	} else if (strcmp(selector, "firstObject") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_ARRAY)) {
			const struct shim_objc_array* array = receiver;
			if (array->count > 0)
				return array->elements[0];
		}
	} else if (strcmp(selector, "lastObject") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_ARRAY)) {
			const struct shim_objc_array* array = receiver;
			if (array->count > 0)
				return array->elements[array->count - 1];
		}
	} else if (strcmp(selector, "cookies") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSHTTPCOOKIESTORAGE)) {
			pthread_mutex_lock(&shim_cookie_jar_mutex);
			void* result = shim_cookie_jar_all_array();
			pthread_mutex_unlock(&shim_cookie_jar_mutex);
			return result;
		}
	} else if (strcmp(selector, "cookiesForURL:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSHTTPCOOKIESTORAGE)) {
			const char* url_text = shim_objc_url_string((void*)a2);
			pthread_mutex_lock(&shim_cookie_jar_mutex);
			void* result = shim_cookie_jar_matching_array(url_text);
			pthread_mutex_unlock(&shim_cookie_jar_mutex);
			return result;
		}
	} else if (strcmp(selector, "setCookies:forURL:mainDocumentURL:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSHTTPCOOKIESTORAGE)) {
			const struct shim_objc_array* array = (const struct shim_objc_array*)a2;
			const char* url_text = shim_objc_url_string((void*)a3);
			if (array && shim_objc_is_object(array, SHIM_OBJC_ARRAY)) {
				pthread_mutex_lock(&shim_cookie_jar_mutex);
				for (uint32_t index = 0; index < array->count; index++) {
					const struct shim_objc_cookie* cookie =
						(const struct shim_objc_cookie*)array->elements[index];
					if (!cookie ||
					    !shim_objc_is_object(cookie, SHIM_OBJC_INSTANCE_NSHTTPCOOKIE))
						continue;
					struct shim_cookie stored = cookie->cookie;
					if (stored.domain[0] == '\0' && url_text) {
						char host[256];
						shim_parse_url_host(url_text, host, sizeof(host));
						shim_copy_cstring(stored.domain, sizeof(stored.domain), host);
					}
					shim_cookie_jar_store(&stored);
				}
				pthread_mutex_unlock(&shim_cookie_jar_mutex);
			}
			return NULL;
		}
	} else if (strcmp(selector, "deleteCookie:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSHTTPCOOKIESTORAGE)) {
			const struct shim_objc_cookie* cookie =
				(const struct shim_objc_cookie*)a2;
			if (cookie && shim_objc_is_object(cookie, SHIM_OBJC_INSTANCE_NSHTTPCOOKIE)) {
				pthread_mutex_lock(&shim_cookie_jar_mutex);
				shim_cookie_jar_remove(&cookie->cookie);
				pthread_mutex_unlock(&shim_cookie_jar_mutex);
			}
			return NULL;
		}
	} else if (strcmp(selector, "cookiesWithResponseHeaderFields:forURL:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSHTTPCOOKIE))
			return shim_parse_response_header_cookies((void*)a2, (void*)a3);
	} else if (strcmp(selector, "requestHeaderFieldsWithCookies:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSHTTPCOOKIE))
			return shim_cookie_request_header_fields((void*)a2);
	} else if (strcmp(selector, "name") == 0) {
		const struct shim_objc_cookie* cookie = receiver;
		if (shim_objc_is_object(cookie, SHIM_OBJC_INSTANCE_NSHTTPCOOKIE))
			return shim_objc_make_string(cookie->cookie.name);
	} else if (strcmp(selector, "value") == 0) {
		const struct shim_objc_cookie* cookie = receiver;
		if (shim_objc_is_object(cookie, SHIM_OBJC_INSTANCE_NSHTTPCOOKIE))
			return shim_objc_make_string(cookie->cookie.value);
	} else if (strcmp(selector, "fileURLWithPath:") == 0 ||
	           strcmp(selector, "fileURLWithPath:isDirectory:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSURL)) {
			const char* url_text = shim_objc_string_utf8((void*)a2);
			if (!url_text)
				return NULL;
			return shim_objc_make_url(url_text);
		}
	} else if (strcmp(selector, "fileURLWithPath:relativeToURL:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSURL)) {
			const char* relative = shim_objc_string_utf8((void*)a2);
			const char* base_text = shim_objc_url_string((void*)a3);
			char joined[PATH_MAX * 2];
			if (!relative || !base_text)
				return NULL;
			if (snprintf(joined, sizeof(joined), "%s/%s", base_text,
			             relative) >= (int)sizeof(joined))
				return NULL;
			return shim_objc_make_url(joined);
		}
	} else if (strcmp(selector, "stringByExpandingTildeInPath") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_STRING)) {
			const char* text = ((struct shim_objc_string*)receiver)->utf8;
			char expanded[PATH_MAX];
			if (text[0] == '~') {
				const char* home = getenv("HOME");
				const char* rest = text[1] == '/' ? text + 2 : text + 1;
				if (snprintf(expanded, sizeof(expanded), "%s/%s",
				             home ? home : "", rest) >= (int)sizeof(expanded))
					return NULL;
				return shim_objc_make_string(expanded);
			}
			return shim_objc_make_string(text);
		}
	} else if (strcmp(selector, "stringByAppendingPathComponent:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_STRING)) {
			const char* base_text = ((struct shim_objc_string*)receiver)->utf8;
			const char* component = shim_objc_string_utf8((void*)a2);
			char joined[PATH_MAX * 2];
			if (!component)
				return NULL;
			if (snprintf(joined, sizeof(joined), "%s%s%s", base_text,
			             base_text[strlen(base_text) - 1] == '/' ? "" : "/",
			             component) >= (int)sizeof(joined))
				return NULL;
			return shim_objc_make_string(joined);
		}
	} else if (strcmp(selector, "defaultManager") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSFILEMANAGER))
			return &shim_filemanager_singleton;
	} else if (strcmp(selector, "createDirectoryAtURL:withIntermediateDirectories:attributes:error:") == 0 ||
	           strcmp(selector, "createDirectoryAtPath:withIntermediateDirectories:attributes:error:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSFILEMANAGER)) {
			const char* path_text = shim_objc_url_string((void*)a2);
			if (!path_text)
				path_text = shim_objc_string_utf8((void*)a2);
			if (!path_text)
				return NULL;
			char mutable_path[PATH_MAX];
			snprintf(mutable_path, sizeof(mutable_path), "%s", path_text);
			for (char* cursor = mutable_path + 1; *cursor; cursor++) {
				if (*cursor != '/')
					continue;
				*cursor = '\0';
				mkdir(mutable_path, 0755);
				*cursor = '/';
			}
			if (mkdir(mutable_path, 0755) != 0 && errno != EEXIST) {
				if (a5)
					*(void**)a5 = NULL;
				return NULL;
			}
			if (a5)
				*(void**)a5 = NULL;
			return (void*)(uintptr_t)1;
		}
	} else if (strcmp(selector, "createFileAtPath:contents:attributes:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSFILEMANAGER)) {
			const char* path_text = shim_objc_string_utf8((void*)a2);
			const struct shim_objc_data* data =
				(const struct shim_objc_data*)a3;
			if (!path_text)
				return NULL;
			FILE* output = fopen(path_text, "wb");
			if (!output)
				return NULL;
			if (data && data->length)
				fwrite(data->bytes, 1, data->length, output);
			fclose(output);
			return (void*)(uintptr_t)1;
		}
	} else if (strcmp(selector, "fileExistsAtPath:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSFILEMANAGER)) {
			const char* path_text = shim_objc_string_utf8((void*)a2);
			if (!path_text)
				return NULL;
			return (void*)(uintptr_t)(access(path_text, F_OK) == 0);
		}
	} else if (strcmp(selector, "removeItemAtPath:error:") == 0 ||
	           strcmp(selector, "removeItemAtURL:error:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSFILEMANAGER)) {
			const char* path_text = shim_objc_string_utf8((void*)a2);
			if (!path_text)
				path_text = shim_objc_url_string((void*)a2);
			if (!path_text)
				return NULL;
			if (a3)
				*(void**)a3 = NULL;
			return (void*)(uintptr_t)(unlink(path_text) == 0);
		}
	} else if (strcmp(selector, "fileHandleForWritingAtPath:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSFILEHANDLE)) {
			const char* path_text = shim_objc_string_utf8((void*)a2);
			struct shim_objc_filehandle* handle;
			if (!path_text)
				return NULL;
			int fd = libc_open(path_text, O_WRONLY | O_CREAT | O_APPEND, 0644);
			if (fd < 0)
				return NULL;
			handle = malloc(sizeof(*handle));
			if (!handle) {
				close(fd);
				return NULL;
			}
			handle->header.magic = SHIM_OBJC_MAGIC;
			handle->header.kind = SHIM_OBJC_INSTANCE_NSFILEHANDLE;
			handle->fd = fd;
			return handle;
		}
	} else if (strcmp(selector, "writeData:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSFILEHANDLE)) {
			const struct shim_objc_filehandle* handle = receiver;
			const struct shim_objc_data* data =
				(const struct shim_objc_data*)a2;
			if (data && data->length)
				write(handle->fd, data->bytes, data->length);
			return NULL;
		}
	} else if (strcmp(selector, "seekToEndOfFile") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSFILEHANDLE)) {
			const struct shim_objc_filehandle* handle = receiver;
			lseek(handle->fd, 0, SEEK_END);
			return NULL;
		}
	} else if (strcmp(selector, "closeFile") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSFILEHANDLE)) {
			struct shim_objc_filehandle* handle = receiver;
			close(handle->fd);
			handle->fd = -1;
			return NULL;
		}
	} else if (strcmp(selector, "dataUsingEncoding:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_STRING)) {
			const char* text = ((struct shim_objc_string*)receiver)->utf8;
			size_t text_length = strlen(text);
			struct shim_objc_data* data =
				malloc(sizeof(*data) + text_length);
			if (!data)
				return NULL;
			data->header.magic = SHIM_OBJC_MAGIC;
			data->header.kind = SHIM_OBJC_INSTANCE_NSDATA;
			data->length = text_length;
			memcpy(data->bytes, text, text_length);
			return data;
		}
	} else if (strcmp(selector, "stringWithContentsOfFile:") == 0 ||
	           strcmp(selector, "stringWithContentsOfFile:encoding:error:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSSTRING)) {
			const char* path_text = shim_objc_string_utf8((void*)a2);
			void** error_out =
				strcmp(selector, "stringWithContentsOfFile:encoding:error:") == 0
					? (void**)a4 : NULL;
			FILE* input;
			long file_size;
			char* contents;
			struct shim_objc_string* result;
			if (!path_text)
				return NULL;
			input = fopen(path_text, "rb");
			if (!input) {
				if (error_out)
					*error_out = &shim_nserror_singleton;
				return NULL;
			}
			if (error_out)
				*error_out = NULL;
			fseek(input, 0, SEEK_END);
			file_size = ftell(input);
			fseek(input, 0, SEEK_SET);
			contents = malloc((size_t)file_size + 1);
			if (!contents) {
				fclose(input);
				return NULL;
			}
			size_t read_length = fread(contents, 1, (size_t)file_size, input);
			fclose(input);
			contents[read_length] = '\0';
			result = shim_objc_make_string(contents);
			free(contents);
			return result;
		}
	} else if (strcmp(selector, "newlineCharacterSet") == 0 ||
	           strcmp(selector, "whitespaceCharacterSet") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSCHARACTERSET)) {
			if (strcmp(selector, "newlineCharacterSet") == 0)
				return (void*)(uintptr_t)'\n';
			return (void*)(uintptr_t)' ';
		}
	} else if (strcmp(selector, "componentsSeparatedByCharactersInSet:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_STRING)) {
			const char* text = ((struct shim_objc_string*)receiver)->utf8;
			char separator = (char)(uintptr_t)a2;
			struct shim_objc_array* array;
			char** parts = NULL;
			uint32_t component_count = 0;
			char* copy;
			char* scan;
			char* component_start;
			if (!text)
				return NULL;
			copy = strdup(text);
			if (!copy)
				return NULL;
			component_start = copy;
			for (scan = copy; ; scan++) {
				if (*scan == separator || *scan == '\0') {
					char terminator = *scan;
					parts = realloc(parts,
					                (component_count + 1) * sizeof(*parts));
					parts[component_count++] = component_start;
					*scan = '\0';
					if (terminator == '\0')
						break;
					component_start = scan + 1;
				}
			}
			array = malloc(sizeof(*array) +
				       sizeof(struct shim_objc_string*) * component_count);
			if (!array) {
				free(parts);
				free(copy);
				return NULL;
			}
			array->header.magic = SHIM_OBJC_MAGIC;
			array->header.kind = SHIM_OBJC_ARRAY;
			array->count = component_count;
			for (uint32_t index = 0; index < component_count; index++) {
				struct shim_objc_string* element =
					shim_objc_make_string(parts[index]);
				((struct shim_objc_string**)array->elements)[index] = element;
			}
			free(parts);
			free(copy);
			return array;
		}
	} else if (strcmp(selector, "componentsJoinedByString:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_ARRAY)) {
			const struct shim_objc_array* array = receiver;
			const char* joiner = shim_objc_string_utf8((void*)a2);
			size_t total_length = 1;
			char* joined;
			size_t position = 0;
			if (!joiner)
				return NULL;
			for (uint32_t index = 0; index < array->count; index++) {
				const char* element = shim_objc_string_utf8(array->elements[index]);
				if (element)
					total_length += strlen(element);
				if (index + 1 < array->count)
					total_length += strlen(joiner);
			}
			joined = malloc(total_length);
			if (!joined)
				return NULL;
			for (uint32_t index = 0; index < array->count; index++) {
				const char* element = shim_objc_string_utf8(array->elements[index]);
				if (element)
					strcpy(joined + position, element);
				position += element ? strlen(element) : 0;
				if (index + 1 < array->count) {
					strcpy(joined + position, joiner);
					position += strlen(joiner);
				}
			}
			joined[position] = '\0';
			struct shim_objc_string* result = shim_objc_make_string(joined);
			free(joined);
			return result;
		}
	} else if (strcmp(selector, "subarrayWithRange:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_ARRAY)) {
			const struct shim_objc_array* array = receiver;
			uint32_t start = (uint32_t)a2;
			uint32_t length = (uint32_t)a3;
			if (start + length > array->count)
				return NULL;
			struct shim_objc_array* result =
				malloc(sizeof(*result) +
				       sizeof(struct shim_objc_string*) * (length ? length : 1));
			if (!result)
				return NULL;
			result->header.magic = SHIM_OBJC_MAGIC;
			result->header.kind = SHIM_OBJC_ARRAY;
			result->count = length;
			for (uint32_t index = 0; index < length; index++)
				((struct shim_objc_string**)result->elements)[index] =
					array->elements[start + index];
			return result;
		}
	} else if (strcmp(selector, "localizedDescription") == 0) {
		return shim_objc_make_string("operation failed");
	} else if (strcmp(selector, "reason") == 0) {
		return shim_objc_make_string("exception");
	} else if (strcmp(selector, "objectAtIndexedSubscript:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_ARRAY)) {
			const struct shim_objc_array* array = receiver;
			uintptr_t index = a2;
			if (index < array->count)
				return array->elements[index];
			return NULL;
		}
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSMUTABLEDICTIONARY)) {
			const struct shim_objc_dictionary* dict = receiver;
			const char* wanted_key = shim_objc_string_utf8((void*)a2);
			if (!wanted_key)
				return NULL;
			for (uint32_t pair_index = 0; pair_index < dict->pair_count; pair_index++) {
				const char* key = shim_objc_string_utf8(dict->keys[pair_index * 2]);
				if (key && strcmp(key, wanted_key) == 0)
					return dict->keys[pair_index * 2 + 1];
			}
			return NULL;
		}
	} else if (strcmp(selector, "intValue") == 0) {
		const char* text = shim_objc_string_utf8(receiver);
		if (text)
			return (void*)(uintptr_t)(int)strtol(text, NULL, 10);
		return NULL;
	} else if (strcmp(selector, "integerValue") == 0) {
		const char* text = shim_objc_string_utf8(receiver);
		if (text)
			return (void*)(uintptr_t)strtoll(text, NULL, 10);
		return NULL;
	} else if (strcmp(selector, "URLWithString:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSURL)) {
			const char* url_text = shim_objc_string_utf8((void*)a2);
			if (!url_text)
				return NULL;
			return shim_objc_make_url(url_text);
		}
	} else if (strcmp(selector, "dictionary") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSMUTABLEDICTIONARY)) {
			struct shim_objc_string** no_keys = NULL;
			struct shim_objc_string** no_values = NULL;
			return shim_objc_make_dictionary(no_keys, no_values, 0);
		}
	} else if (strcmp(selector, "setObject:forKey:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSUSERDEFAULTS)) {
			pthread_mutex_lock(&shim_userdefaults_mutex);
			shim_userdefaults_set(receiver, (struct shim_objc_string*)a3,
			                      (struct shim_objc_string*)a2);
			pthread_mutex_unlock(&shim_userdefaults_mutex);
			return NULL;
		}
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSMUTABLEDICTIONARY)) {
			struct shim_objc_dictionary* dict = receiver;
			uint32_t pair_index;
			for (pair_index = 0; pair_index < dict->pair_count; pair_index++) {
				const char* key = shim_objc_string_utf8(dict->keys[pair_index * 2]);
				const char* new_key = shim_objc_string_utf8((void*)a3);
				if (key && new_key && strcmp(key, new_key) == 0) {
					dict->keys[pair_index * 2 + 1] = (struct shim_objc_string*)a2;
					return NULL;
				}
			}
			if (dict->pair_count < dict->pair_capacity) {
				dict->keys[dict->pair_count * 2] = (struct shim_objc_string*)a3;
				dict->keys[dict->pair_count * 2 + 1] = (struct shim_objc_string*)a2;
				dict->pair_count++;
			}
			return NULL;
		}
	} else if (strcmp(selector, "objectForKeyedSubscript:") == 0 ||
	           strcmp(selector, "objectForKey:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSUSERDEFAULTS)) {
			pthread_mutex_lock(&shim_userdefaults_mutex);
			void* stored = shim_userdefaults_get(
				receiver, (struct shim_objc_string*)a2);
			pthread_mutex_unlock(&shim_userdefaults_mutex);
			return stored;
		}
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSMUTABLEDICTIONARY)) {
			const struct shim_objc_dictionary* dict = receiver;
			const char* wanted_key = shim_objc_string_utf8((void*)a2);
			if (!wanted_key)
				return NULL;
			for (uint32_t pair_index = 0; pair_index < dict->pair_count; pair_index++) {
				const char* key = shim_objc_string_utf8(dict->keys[pair_index * 2]);
				if (key && strcmp(key, wanted_key) == 0)
					return dict->keys[pair_index * 2 + 1];
			}
			return NULL;
		}
	} else if (strcmp(selector, "path") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSURLCOMPONENTS)) {
			struct shim_objc_url_components* components = receiver;
			if (!components->path)
				return NULL;
			return shim_objc_make_string(components->path->utf8);
		}
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSURL)) {
			const char* text = shim_objc_url_string(receiver);
			const char* host_start;
			const char* path_start;
			if (!text)
				return NULL;
			host_start = strstr(text, "://");
			host_start = host_start ? host_start + 3 : text;
			path_start = strchr(host_start, '/');
			if (!path_start)
				return shim_objc_make_string("/");
			return shim_objc_make_string(path_start);
		}
	} else if (strcmp(selector, "dateWithTimeIntervalSinceNow:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSDATE))
			return shim_objc_make_string("NSDate");
	} else if (strcmp(selector, "componentsWithURL:resolvingAgainstBaseURL:") == 0) {
		const char* url_text = shim_objc_url_string((void*)a2);
		if (!url_text)
			return NULL;
		struct shim_objc_url_components* components =
			shim_objc_make_url_components();
		if (!components)
			return NULL;
		const char* scheme_end = strstr(url_text, "://");
		size_t scheme_length = scheme_end ?
			(size_t)(scheme_end - url_text) : 0;
		const char* host_start = scheme_end ? scheme_end + 3 : url_text;
		const char* path_start = strchr(host_start, '/');
		size_t host_length = path_start ?
			(size_t)(path_start - host_start) : strlen(host_start);
		const char* path_text = path_start ? path_start : "";
		char buffer[2048];
		if (scheme_length >= sizeof(buffer) || host_length >= sizeof(buffer))
			return components;
		memcpy(buffer, url_text, scheme_length);
		buffer[scheme_length] = '\0';
		components->scheme = shim_objc_make_string(buffer);
		memcpy(buffer, host_start, host_length);
		buffer[host_length] = '\0';
		components->host = shim_objc_make_string(buffer);
		components->path = shim_objc_make_string(path_text);
		return components;
	} else if (strcmp(selector, "URLPathAllowedCharacterSet") == 0) {
		return &shim_characterset_singleton;
	} else if (strcmp(selector, "stringByTrimmingCharactersInSet:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_STRING)) {
			const char* text = ((struct shim_objc_string*)receiver)->utf8;
			size_t start = 0;
			size_t end = strlen(text);
			while (end > start &&
			       shim_is_url_path_allowed_character((unsigned char)text[end - 1]))
				end--;
			while (start < end &&
			       shim_is_url_path_allowed_character((unsigned char)text[start]))
				start++;
			size_t length = end - start;
			struct shim_objc_string* result =
				malloc(sizeof(*result) + length + 1);
			if (!result)
				return NULL;
			result->header.magic = SHIM_OBJC_MAGIC;
			result->header.kind = SHIM_OBJC_STRING;
			memcpy(result->utf8, text + start, length);
			result->utf8[length] = '\0';
			return result;
		}
	} else if (strcmp(selector, "setScheme:") == 0 ||
	           strcmp(selector, "setHost:") == 0 ||
	           strcmp(selector, "setPath:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSURLCOMPONENTS)) {
			struct shim_objc_url_components* components = receiver;
			struct shim_objc_string** slot = NULL;
			if (selector[3] == 'S')
				slot = &components->scheme;
			else if (selector[3] == 'H')
				slot = &components->host;
			else
				slot = &components->path;
			const char* text = shim_objc_string_utf8((void*)a2);
			if (!text) {
				*slot = NULL;
				return NULL;
			}
			struct shim_objc_string* value = shim_objc_make_string(text);
			*slot = value;
			return NULL;
		}
	} else if (strcmp(selector, "URL") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSURLCOMPONENTS)) {
			struct shim_objc_url_components* components = receiver;
			const char* scheme = components->scheme ?
				components->scheme->utf8 : "";
			const char* host = components->host ?
				components->host->utf8 : "";
			const char* path = components->path ?
				components->path->utf8 : "";
			if (!scheme[0] && !host[0] && !path[0])
				return NULL;
			size_t capacity = strlen(scheme) + strlen(host) + strlen(path) + 16;
			char* joined = malloc(capacity);
			if (!joined)
				return NULL;
			snprintf(joined, capacity, "%s://%s%s", scheme, host, path);
			void* result = shim_objc_make_url(joined);
			free(joined);
			return result;
		}
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSURLCOMPONENTS))
			return NULL;
	} else if (strcmp(selector, "absoluteString") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSURL)) {
			const char* text = shim_objc_url_string(receiver);
			if (!text)
				return NULL;
			return shim_objc_make_string(text);
		}
	} else if (strcmp(selector, "sharedCookieStorageForGroupContainerIdentifier:") == 0 ||
	           strcmp(selector, "sharedHTTPCookieStorage") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSHTTPCOOKIESTORAGE))
			return &shim_cookie_storage_singleton;
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSHTTPCOOKIESTORAGE))
			return &shim_cookie_storage_singleton;
	} else if (strcmp(selector, "cookieAcceptPolicy") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSHTTPCOOKIESTORAGE))
			return NULL;
	} else if (strcmp(selector, "dictionaryWithObjectsAndKeys:") == 0) {
		uintptr_t values[32];
		uint32_t value_count = 0;
		values[value_count++] = a2;
		for (uint32_t stack_index = 0; stack_index < 16; stack_index++) {
			if (!guest_stack_args)
				break;
			uintptr_t next_value = guest_stack_args[stack_index];
			if (next_value == 0)
				break;
			if (value_count >= 32)
				break;
			values[value_count++] = next_value;
		}
		uint32_t pair_count = value_count / 2;
		if (pair_count == 0)
			return NULL;
		struct shim_objc_string** keys = malloc(sizeof(*keys) * pair_count);
		struct shim_objc_string** pair_values = malloc(sizeof(*pair_values) * pair_count);
		if (!keys || !pair_values) {
			free(keys);
			free(pair_values);
			return NULL;
		}
		for (uint32_t pair_index = 0; pair_index < pair_count; pair_index++) {
			keys[pair_index] = (struct shim_objc_string*)values[pair_index * 2 + 1];
			pair_values[pair_index] = (struct shim_objc_string*)values[pair_index * 2];
		}
		void* result = shim_objc_make_dictionary(keys, pair_values, pair_count);
		free(keys);
		free(pair_values);
		return result;
	} else if (strcmp(selector, "cookieWithProperties:") == 0) {
		const struct shim_objc_dictionary* properties = (const void*)a2;
		struct shim_cookie cookie;
		const char* origin_url = NULL;
		memset(&cookie, 0, sizeof(cookie));
		if (properties &&
		    shim_objc_is_object(properties, SHIM_OBJC_INSTANCE_NSMUTABLEDICTIONARY)) {
			for (uint32_t pair_index = 0;
			     pair_index < properties->pair_count; pair_index++) {
				const char* key =
					shim_objc_string_utf8(properties->keys[pair_index * 2]);
				void* value_object = properties->keys[pair_index * 2 + 1];
				const char* value =
					shim_objc_string_utf8(value_object);
				if (!key)
					continue;
				if (strcmp(key, "OriginURL") == 0) {
					origin_url = shim_objc_url_string(value_object);
					if (!origin_url)
						origin_url = value;
				} else if (!value) {
					continue;
				} else if (strcmp(key, "Name") == 0) {
					shim_copy_cstring(cookie.name, sizeof(cookie.name), value);
				} else if (strcmp(key, "Value") == 0) {
					shim_copy_cstring(cookie.value, sizeof(cookie.value), value);
				} else if (strcmp(key, "Path") == 0) {
					shim_copy_cstring(cookie.path, sizeof(cookie.path), value);
				} else if (strcmp(key, "Domain") == 0) {
					shim_copy_cstring(cookie.domain, sizeof(cookie.domain), value);
				}
			}
		}
		if (cookie.domain[0] == '\0' && origin_url)
			shim_parse_url_host(origin_url, cookie.domain, sizeof(cookie.domain));
		return shim_objc_make_cookie(&cookie);
	} else if (strcmp(selector, "setCookie:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSHTTPCOOKIESTORAGE)) {
			const struct shim_objc_cookie* cookie = (const struct shim_objc_cookie*)a2;
			if (cookie &&
			    shim_objc_is_object(cookie, SHIM_OBJC_INSTANCE_NSHTTPCOOKIE)) {
				pthread_mutex_lock(&shim_cookie_jar_mutex);
				shim_cookie_jar_store(&cookie->cookie);
				pthread_mutex_unlock(&shim_cookie_jar_mutex);
			}
			return NULL;
		}
	} else if (strcmp(selector, "alloc") == 0 ||
	           strcmp(selector, "allocWithZone:") == 0 ||
	           strcmp(selector, "new") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSURLCOMPONENTS))
			return shim_objc_make_url_components();
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSMUTABLEDICTIONARY))
			return shim_objc_make_dictionary(NULL, NULL, 0);
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSUSERDEFAULTS)) {
			struct shim_objc_userdefaults* defaults = malloc(sizeof(*defaults));
			if (!defaults)
				return NULL;
			defaults->header.magic = SHIM_OBJC_MAGIC;
			defaults->header.kind = SHIM_OBJC_INSTANCE_NSUSERDEFAULTS;
			defaults->values = shim_objc_make_dictionary(NULL, NULL, 0);
			if (!defaults->values) {
				free(defaults);
				return NULL;
			}
			return defaults;
		}
		return shim_objc_make_string("");
	} else if (strcmp(selector, "init") == 0) {
		return receiver;
	} else if (strcmp(selector, "standardUserDefaults") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSUSERDEFAULTS)) {
			pthread_mutex_lock(&shim_userdefaults_mutex);
			void* result = shim_userdefaults_for_suite("");
			pthread_mutex_unlock(&shim_userdefaults_mutex);
			return result;
		}
	} else if (strcmp(selector, "initWithSuiteName:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSUSERDEFAULTS)) {
			const char* suite_name = shim_objc_string_utf8((void*)a2);
			struct shim_objc_userdefaults* allocated = receiver;
			pthread_mutex_lock(&shim_userdefaults_mutex);
			struct shim_objc_userdefaults* shared =
				shim_userdefaults_for_suite(suite_name ? suite_name : "");
			if (!shared) {
				pthread_mutex_unlock(&shim_userdefaults_mutex);
				return NULL;
			}
			free(allocated->values);
			free(allocated);
			pthread_mutex_unlock(&shim_userdefaults_mutex);
			return shared;
		}
	} else if (strcmp(selector, "setObject:forKey:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSUSERDEFAULTS)) {
			pthread_mutex_lock(&shim_userdefaults_mutex);
			shim_userdefaults_set(receiver, (struct shim_objc_string*)a3,
			                      (struct shim_objc_string*)a2);
			pthread_mutex_unlock(&shim_userdefaults_mutex);
			return NULL;
		}
	} else if (strcmp(selector, "stringForKey:") == 0 ||
	           strcmp(selector, "objectForKey:") == 0 ||
	           strcmp(selector, "stringArrayForKey:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSUSERDEFAULTS)) {
			pthread_mutex_lock(&shim_userdefaults_mutex);
			void* result = shim_userdefaults_get(
				receiver, (struct shim_objc_string*)a2);
			pthread_mutex_unlock(&shim_userdefaults_mutex);
			return result;
		}
	} else if (strcmp(selector, "removeObjectForKey:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSUSERDEFAULTS)) {
			pthread_mutex_lock(&shim_userdefaults_mutex);
			shim_userdefaults_remove(receiver, (struct shim_objc_string*)a2);
			pthread_mutex_unlock(&shim_userdefaults_mutex);
			return NULL;
		}
	} else if (strcmp(selector, "initWithBytes:length:encoding:") == 0) {
		const char* bytes = (const char*)a2;
		size_t byte_length = (size_t)a3;
		struct shim_objc_string* result;
		if (!bytes || byte_length > (1 << 20))
			return NULL;
		result = malloc(sizeof(*result) + byte_length + 1);
		if (!result)
			return NULL;
		result->header.magic = SHIM_OBJC_MAGIC;
		result->header.kind = SHIM_OBJC_STRING;
		memcpy(result->utf8, bytes, byte_length);
		result->utf8[byte_length] = '\0';
		return result;
	} else if (strcmp(selector, "initWithUTF8String:") == 0) {
		if (!a2)
			return NULL;
		return shim_objc_make_string((const char*)a2);
	} else if (strcmp(selector, "stringWithUTF8String:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSSTRING)) {
			if (!a2)
				return NULL;
			return shim_objc_make_string((const char*)a2);
		}
	} else if (strcmp(selector, "hasPrefix:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_STRING)) {
			const char* prefix = shim_objc_string_utf8((void*)a2);
			const char* text = ((struct shim_objc_string*)receiver)->utf8;
			if (!prefix)
				return NULL;
			return (void*)(uintptr_t)(strncmp(text, prefix, strlen(prefix)) == 0);
		}
	} else if (strcmp(selector, "substringFromIndex:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_STRING)) {
			const char* text = ((struct shim_objc_string*)receiver)->utf8;
			size_t length = strlen(text);
			if (a2 > length)
				return NULL;
			return shim_objc_make_string(text + a2);
		}
	} else if (strcmp(selector, "isEqualToString:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_STRING)) {
			const char* other = shim_objc_string_utf8((void*)a2);
			const char* text = ((struct shim_objc_string*)receiver)->utf8;
			return (void*)(uintptr_t)(other && strcmp(text, other) == 0);
		}
	} else if (strcmp(selector, "numberWithBool:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSNUMBER))
			return shim_objc_make_number(a2 ? 1 : 0);
	} else if (strcmp(selector, "numberWithInteger:") == 0 ||
	           strcmp(selector, "numberWithInt:") == 0 ||
	           strcmp(selector, "numberWithLong:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSNUMBER))
			return shim_objc_make_number((long long)(intptr_t)a2);
	} else if (strcmp(selector, "numberWithDouble:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSNUMBER)) {
			struct shim_objc_number* result = malloc(sizeof(*result));
			if (!result)
				return NULL;
			result->header.magic = SHIM_OBJC_MAGIC;
			result->header.kind = SHIM_OBJC_INSTANCE_NSNUMBER;
			memcpy(&result->value, &a2, sizeof(double));
			result->is_floating = 1;
			return result;
		}
	} else if (strcmp(selector, "boolValue") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSNUMBER))
			return (void*)(uintptr_t)(
				((struct shim_objc_number*)receiver)->value.integer_value != 0);
	} else if (strcmp(selector, "integerValue") == 0 ||
	           strcmp(selector, "intValue") == 0 ||
	           strcmp(selector, "longLongValue") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSNUMBER))
			return (void*)(intptr_t)(
				((struct shim_objc_number*)receiver)->value.integer_value);
	} else if (strcmp(selector, "arrayWithObjects:count:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSARRAY)) {
			void** elements = (void**)a2;
			uintptr_t count = a3;
			struct shim_objc_array* result =
				shim_objc_make_array(NULL, 0);
			if (!result)
				return NULL;
			for (uintptr_t index = 0; index < count; index++)
				result = shim_objc_array_append(result, elements[index]);
			return result;
		}
	} else if (strcmp(selector, "array") == 0 ||
	           strcmp(selector, "arrayWithCapacity:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSARRAY)) {
			struct shim_objc_array* result =
				shim_objc_make_array(NULL, 0);
			if (result)
				result->header.kind = SHIM_OBJC_ARRAY;
			return result;
		}
	} else if (strcmp(selector, "dictionaryWithObjects:forKeys:count:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSMUTABLEDICTIONARY) ||
		    shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSDICTIONARY)) {
			void** objects = (void**)a2;
			void** keys = (void**)a3;
			uintptr_t count = a4;
			struct shim_objc_dictionary* result =
				shim_objc_make_dictionary(NULL, NULL, 0);
			if (!result)
				return NULL;
			for (uintptr_t index = 0; index < count; index++)
				shim_objc_dictionary_set(result, keys[index],
				                         objects[index]);
			return result;
		}
	} else if (strcmp(selector, "dictionary") == 0 ||
	           strcmp(selector, "dictionaryWithCapacity:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSMUTABLEDICTIONARY) ||
		    shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSDICTIONARY))
			return shim_objc_make_dictionary(NULL, NULL, 0);
	} else if (strcmp(selector, "addObject:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_ARRAY)) {
			struct shim_objc_array* grown =
				shim_objc_array_append(receiver, (void*)a2);
			if (grown != receiver) {
				memcpy(receiver, grown, sizeof(struct shim_objc_header));
				((struct shim_objc_array*)receiver)->count = grown->count;
				((struct shim_objc_array*)receiver)->elements[0] =
					grown->elements[grown->count - 1];
			}
			return NULL;
		}
	} else if (strcmp(selector, "setObject:forKeyedSubscript:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSMUTABLEDICTIONARY)) {
			shim_objc_dictionary_set(receiver, (void*)a3, (void*)a2);
			return NULL;
		}
	} else if (strcmp(selector, "objectForKey:") == 0 ||
	           strcmp(selector, "objectForKeyedSubscript:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSMUTABLEDICTIONARY))
			return shim_objc_dictionary_get(receiver, (void*)a2);
	} else if (strcmp(selector, "objectAtIndexedSubscript:") == 0 ||
	           strcmp(selector, "objectAtIndex:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_ARRAY)) {
			const struct shim_objc_array* array = receiver;
			if (a2 < array->count)
				return array->elements[a2];
		}
	} else if (strcmp(selector, "count") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_INSTANCE_NSMUTABLEDICTIONARY))
			return (void*)(uintptr_t)(
				((struct shim_objc_dictionary*)receiver)->pair_count);
	} else if (strcmp(selector, "dataWithJSONObject:options:error:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSJSONSERIALIZATION)) {
			if (a4)
				*(void**)a4 = NULL;
			char* json = shim_json_serialize_object((void*)a2, 0);
			if (!json)
				return NULL;
			struct shim_objc_data* data =
				malloc(sizeof(*data) + strlen(json) + 1);
			if (!data) {
				free(json);
				return NULL;
			}
			data->header.magic = SHIM_OBJC_MAGIC;
			data->header.kind = SHIM_OBJC_INSTANCE_NSDATA;
			data->length = strlen(json);
			memcpy(data->bytes, json, data->length + 1);
			free(json);
			return data;
		}
	} else if (strcmp(selector, "JSONObjectWithData:options:error:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_CLASS_NSJSONSERIALIZATION)) {
			if (a4)
				*(void**)a4 = NULL;
			if (!shim_objc_is_object((void*)a2, SHIM_OBJC_INSTANCE_NSDATA))
				return NULL;
			const struct shim_objc_data* data = (const void*)a2;
			const char* json = (const char*)data->bytes;
			void* result = shim_json_parse_value(&json, 0);
			if (!result)
				return shim_objc_make_dictionary(NULL, NULL, 0);
			return result;
		}
	} else if (strcmp(selector, "initWithData:encoding:") == 0) {
		if (shim_objc_is_object((void*)a2, SHIM_OBJC_INSTANCE_NSDATA)) {
			const struct shim_objc_data* data = (const void*)a2;
			struct shim_objc_string* result =
				malloc(sizeof(*result) + data->length + 1);
			if (!result)
				return NULL;
			result->header.magic = SHIM_OBJC_MAGIC;
			result->header.kind = SHIM_OBJC_STRING;
			memcpy(result->utf8, data->bytes, data->length);
			result->utf8[data->length] = '\0';
			return result;
		}
	} else if (strcmp(selector, "copy") == 0 ||
	           strcmp(selector, "mutableCopy") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_ARRAY)) {
			const struct shim_objc_array* array = receiver;
			return shim_objc_make_array((struct shim_objc_string**)array->elements,
			                            array->count);
		}
		if (shim_objc_is_object(receiver, SHIM_OBJC_STRING))
			return shim_objc_make_string(
				((struct shim_objc_string*)receiver)->utf8);
	} else if (strcmp(selector, "characterAtIndex:") == 0) {
		if (shim_objc_is_object(receiver, SHIM_OBJC_STRING)) {
			const char* text = ((struct shim_objc_string*)receiver)->utf8;
			if (a2 < strlen(text))
				return (void*)(uintptr_t)(unsigned char)text[a2];
		}
	}

	if (shim_trace_enabled())
		fprintf(stderr, "libsystem_shim: objc_msgSend unhandled selector '%s' receiver=%p\n",
		        selector, receiver);
	return NULL;
}

struct shim_objc_msgsend_result {
	void* pointer;
	double floating;
};

struct shim_objc_msgsend_result shim_objc_msgSend_struct_impl(void* receiver, void* sel, uintptr_t a2, uintptr_t a3,
                                                              uintptr_t a4, uintptr_t a5, uintptr_t a6, uintptr_t a7,
                                                              void* sret, const uintptr_t* guest_stack_args)
{
	struct shim_objc_msgsend_result result = {NULL, 0.0};
	const char* selector = sel;

	if (!receiver || !selector)
		return result;

	if (strcmp(selector, "doubleValue") == 0) {
		const char* text = shim_objc_string_utf8(receiver);
		if (text)
			result.floating = strtod(text, NULL);
		return result;
	}

	result.pointer = shim_objc_msgSend_impl(receiver, sel, a2, a3, a4, a5,
	                                       a6, a7, sret, guest_stack_args);
	return result;
}

#if defined(__aarch64__)
struct shim_objc_msgsend_args {
	uintptr_t a2;
	uintptr_t a3;
	uintptr_t a4;
	uintptr_t a5;
	uintptr_t a6;
	uintptr_t a7;
	uintptr_t receiver;
	uintptr_t sel;
	uintptr_t guest_sp;
	uintptr_t sret;
};

static struct shim_objc_msgsend_result shim_objc_msgSend_dispatch(
	const struct shim_objc_msgsend_args* args) __attribute__((used));
static struct shim_objc_msgsend_result shim_objc_msgSend_dispatch(
	const struct shim_objc_msgsend_args* args)
{
	return shim_objc_msgSend_struct_impl((void*)args->receiver,
	                                     (void*)args->sel,
	                                     args->a2, args->a3, args->a4,
	                                     args->a5, args->a6, args->a7,
	                                     (void*)args->sret,
	                                     (const uintptr_t*)args->guest_sp);
}

__asm__(
	".text\n"
	".global objc_msgSend\n"
	".type objc_msgSend, %function\n"
	"objc_msgSend:\n"
	"stp x29, x30, [sp, #-0x70]!\n"
	"stp x2, x3, [sp, #0x10]\n"
	"stp x4, x5, [sp, #0x20]\n"
	"stp x6, x7, [sp, #0x30]\n"
	"stp x0, x1, [sp, #0x40]\n"
	"add x9, sp, #0x70\n"
	"str x9, [sp, #0x50]\n"
	"str x8, [sp, #0x58]\n"
	"add x0, sp, #0x10\n"
	"bl shim_objc_msgSend_dispatch\n"
	".p2align 4\n"
	"ldp x29, x30, [sp], #0x70\n"
	"ret\n"
);
#else
void* objc_msgSend(void* receiver, void* sel, ...)
{
	void* sret = NULL;
	return shim_objc_msgSend_impl(receiver, sel, 0, 0, 0, 0, 0, sret, NULL);
}
#endif

void* objc_getClass(const char* name)
{
	if (!name)
		return NULL;
	if (strcmp(name, "NSProcessInfo") == 0)
		return &OBJC_CLASS_$_NSProcessInfo;
	if (strcmp(name, "NSUserDefaults") == 0)
		return &OBJC_CLASS_$_NSUserDefaults;
	if (strcmp(name, "NSString") == 0)
		return &OBJC_CLASS_$_NSString;
	if (strcmp(name, "NSHTTPCookie") == 0)
		return &OBJC_CLASS_$_NSHTTPCookie;
	if (strcmp(name, "NSHTTPCookieStorage") == 0)
		return &OBJC_CLASS_$_NSHTTPCookieStorage;
	if (strcmp(name, "NSURL") == 0)
		return &OBJC_CLASS_$_NSURL;
	if (strcmp(name, "NSMutableDictionary") == 0 ||
	    strcmp(name, "NSDictionary") == 0)
		return &OBJC_CLASS_$_NSMutableDictionary;
	if (strcmp(name, "NSCharacterSet") == 0)
		return &OBJC_CLASS_$_NSCharacterSet;
	if (strcmp(name, "NSURLComponents") == 0)
		return &OBJC_CLASS_$_NSURLComponents;
	if (strcmp(name, "NSDate") == 0)
		return &OBJC_CLASS_$_NSDate;
	if (strcmp(name, "NSFileManager") == 0)
		return &OBJC_CLASS_$_NSFileManager;
	if (strcmp(name, "NSFileHandle") == 0)
		return &OBJC_CLASS_$_NSFileHandle;
	if (strcmp(name, "NSData") == 0 ||
	    strcmp(name, "NSMutableData") == 0)
		return &OBJC_CLASS_$_NSData;
	if (strcmp(name, "NSNumber") == 0)
		return &OBJC_CLASS_$_NSNumber;
	if (strcmp(name, "NSArray") == 0 ||
	    strcmp(name, "NSMutableArray") == 0)
		return &OBJC_CLASS_$_NSArray;
	if (strcmp(name, "NSDictionary") == 0 ||
	    strcmp(name, "NSMutableDictionary") == 0)
		return &OBJC_CLASS_$_NSMutableDictionary;
	if (strcmp(name, "NSJSONSerialization") == 0)
		return &OBJC_CLASS_$_NSJSONSerialization;
	if (shim_trace_enabled())
		fprintf(stderr, "libsystem_shim: objc_getClass unknown '%s'\n", name);
	return NULL;
}

void* objc_lookUpClass(const char* name)
{
	return objc_getClass(name);
}

void* objc_alloc(void* class_object)
{
	if (shim_objc_is_object(class_object, SHIM_OBJC_CLASS_NSURLCOMPONENTS))
		return shim_objc_make_url_components();
	if (shim_objc_is_object(class_object, SHIM_OBJC_CLASS_NSMUTABLEDICTIONARY))
		return shim_objc_make_dictionary(NULL, NULL, 0);
	if (shim_objc_is_object(class_object, SHIM_OBJC_CLASS_NSUSERDEFAULTS)) {
		struct shim_objc_userdefaults* defaults = malloc(sizeof(*defaults));
		if (!defaults)
			return NULL;
		defaults->header.magic = SHIM_OBJC_MAGIC;
		defaults->header.kind = SHIM_OBJC_INSTANCE_NSUSERDEFAULTS;
		defaults->values = shim_objc_make_dictionary(NULL, NULL, 0);
		if (!defaults->values) {
			free(defaults);
			return NULL;
		}
		return defaults;
	}
	if (guest_objc_find_class(class_object)) {
		void* instance = guest_objc_alloc_instance(class_object);
		if (shim_objc_msgsend_trace_enabled())
			fprintf(stderr,
			        "libsystem_shim: objc_alloc guest class=%p -> %p isa=%p\n",
			        class_object, instance,
			        instance ? *(void**)instance : NULL);
		return instance;
	}
	return shim_objc_make_string("");
}

void* objc_allocWithZone(void* class_object, void* zone)
{
	(void)zone;
	return objc_alloc(class_object);
}

void* objc_retain(void* object)
{
	return object;
}

void objc_release(void* object)
{
	(void)object;
}

void* objc_autorelease(void* object)
{
	return object;
}

void* objc_retainAutoreleasedReturnValue(void* object)
{
	return object;
}

void* objc_retainAutorelease(void* object)
{
	return object;
}

void* objc_retainAutoreleaseReturnValue(void* object)
{
	return object;
}

void* objc_autoreleasePoolPush(void)
{
	return (void*)(uintptr_t)1;
}

void objc_autoreleasePoolPop(void* pool)
{
	(void)pool;
}

void objc_enumerationMutation(void* object)
{
	(void)object;
}

void* objc_autoreleaseReturnValue(void* object)
{
	return object;
}

void objc_storeStrong(void** location, void* object)
{
	if (location)
		*location = object;
}

struct guest_objc_super {
	void* receiver;
	void* class_ptr;
};

void* objc_msgSendSuper2(struct guest_objc_super* super_data, void* sel, ...)
{
	struct guest_objc_class_entry* entry;
	const char* selector;
	void* imp;
	void* superclass;

	if (!super_data || !sel)
		return NULL;

	selector = sel;
	superclass = *(void**)((char*)super_data->class_ptr + 8);
	if (shim_objc_msgsend_trace_enabled())
		fprintf(stderr,
		        "libsystem_shim: msgSendSuper2 [%s] recv=%p class=%p super=%p\n",
		        selector, super_data->receiver, super_data->class_ptr,
		        superclass);

	entry = guest_objc_find_class(superclass);
	if (entry) {
		imp = guest_objc_lookup_imp(entry, 0, selector);
		if (!imp)
			return NULL;
		uintptr_t register_args[6] = {0, 0, 0, 0, 0, 0};
		return shim_objc_msgSend_call_guest_imp(imp, super_data->receiver,
		                                        sel, NULL, register_args,
		                                        NULL);
	}

	if (strcmp(selector, "init") == 0)
		return super_data->receiver;
	return NULL;
}

void* objc_getProperty(void* receiver, void* sel, void* value,
                       uintptr_t offset, int atomic_flag)
{
	(void)sel;
	(void)value;
	(void)atomic_flag;
	if (!receiver)
		return NULL;
	return *(void**)((char*)receiver + offset);
}

void objc_setProperty_atomic(void* receiver, void* sel, void* value,
                             uintptr_t offset)
{
	(void)sel;
	if (shim_objc_msgsend_trace_enabled())
		fprintf(stderr,
		        "libsystem_shim: setProperty self=%p value=%p offset=%#lx\n",
		        receiver, value, (unsigned long)offset);
	if (!receiver)
		return;
	*(void**)((char*)receiver + offset) = value;
}

void objc_setProperty_nonatomic(void* receiver, void* sel, void* value,
                                uintptr_t offset)
{
	objc_setProperty_atomic(receiver, sel, value, offset);
}

void objc_setProperty_nonatomic_copy(void* receiver, void* sel, void* value,
                                     uintptr_t offset)
{
	objc_setProperty_atomic(receiver, sel, value, offset);
}
