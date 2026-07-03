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
#include <sys/socket.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <dirent.h>
#include <stdarg.h>
#include <malloc.h>
#include <locale.h>
#include <sched.h>

#define MACHGATE_SHIM_CALLER() __builtin_extract_return_addr(__builtin_return_address(0))

extern char **environ;

static int shim_hw_ncpu(void);
static int shim_trace_enabled(void);
static int shim_cxx_init_full_trace_enabled(void);
static int shim_delta_vm_trace_enabled(void);
static int shim_wait_trace_enabled(void);
static int shim_alloc_trace_enabled(void);
static int shim_alloc_mismatch_trace_enabled(void);
static int shim_alloc_signal_dump_enabled(void);
static int shim_host_sigchld_handler_enabled(void);
static FILE* shim_open_trace_file(void);
static void shim_fd_trace_log(const char* format, ...);
static pid_t shim_trace_tid(void);
static unsigned long shim_trace_pthread_self(void);
static const char* shim_trace_path(const char* path);
static char* getenv_from_guest_envp(const char* name);
static int shim_errno_from_linux(int linux_errno);
static int translate_oflags(int darwin_flags);
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

/* task_info — stub, returns KERN_FAILURE */
int task_info(uint32_t target_task, uint32_t flavor,
              void *task_info_out, uint32_t *task_info_count)
{
	(void)target_task; (void)flavor;
	(void)task_info_out; (void)task_info_count;
	return 5; /* KERN_FAILURE */
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

int host_statistics(uint32_t host_priv, int flavor, void* host_info_out,
                    uint32_t* host_info_count)
{
	(void)host_priv;
	(void)flavor;

	if (host_info_out && host_info_count)
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

int host_statistics64(uint32_t host, int flavor, void* info,
                      uint32_t* info_count)
{
	(void)host;
	(void)flavor;

	if (info && info_count)
		memset(info, 0, (size_t)*info_count * sizeof(uint64_t));
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
			int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
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

struct fsevent_stream {
	FSEventStreamCallback callback;
	struct fsevent_stream_context context;
	CFArrayRef paths;
	uint64_t device;
	FSEventStreamEventId latest_event_id;
	uint8_t started;
	uint8_t invalidated;
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

CFStringRef CFUUIDCreate(CFAllocatorRef allocator)
{
	return CFStringCreateWithCString(allocator,
	                                 "00000000-0000-0000-0000-000000000000",
	                                 0);
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
	(void)stream_ref;
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
	free(stream_ref);
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
			int fd = open(action->path, translate_oflags(action->flags),
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

int getfsstat(struct statfs* buffer, int buffer_size, int flags)
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

	int input_fd = open(from, O_RDONLY);
	if (input_fd < 0)
		return -1;

	int output_fd = open(to, O_WRONLY | O_CREAT | O_TRUNC, 0666);
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

int __cxa_guard_acquire(uint64_t* guard)
{
	unsigned char* bytes = (unsigned char*)guard;
	unsigned char state;

	if (!guard)
		return 0;

	state = __atomic_load_n(&bytes[1], __ATOMIC_ACQUIRE);
	if (shim_cxx_init_full_trace_enabled()) {
		fprintf(stderr,
		        "libsystem_shim: __cxa_guard_acquire guard=%p value=%#llx initialized=%d state=%#x caller=%p\n",
		        guard, (unsigned long long)*guard,
		        __atomic_load_n(&bytes[0], __ATOMIC_ACQUIRE), state,
		        __builtin_return_address(0));
		trace_guest_address_context("__cxa_guard_acquire.caller",
		                            (uintptr_t)__builtin_return_address(0));
	}
	if (__atomic_load_n(&bytes[0], __ATOMIC_ACQUIRE) ||
	    state == SHIM_CXA_GUARD_COMPLETE)
		return 0;
	if (state & SHIM_CXA_GUARD_PENDING)
		return 0;
	if (!__atomic_compare_exchange_n(&bytes[1], &state, SHIM_CXA_GUARD_PENDING,
	                                 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
		return 0;
	if (shim_cxx_init_full_trace_enabled())
		fprintf(stderr, "libsystem_shim: __cxa_guard_acquire guard=%p -> run initializer\n",
		        guard);
	return 1;
}

void __cxa_guard_release(uint64_t* guard)
{
	unsigned char* bytes = (unsigned char*)guard;

	if (!guard)
		return;
	__atomic_store_n(&bytes[0], SHIM_CXA_GUARD_COMPLETE, __ATOMIC_RELEASE);
	__atomic_store_n(&bytes[1], SHIM_CXA_GUARD_COMPLETE, __ATOMIC_RELEASE);
	if (shim_cxx_init_full_trace_enabled())
		fprintf(stderr, "libsystem_shim: __cxa_guard_release guard=%p value=%#llx caller=%p\n",
		        guard, (unsigned long long)*guard,
		        __builtin_return_address(0));
}

void __cxa_guard_abort(uint64_t* guard)
{
	unsigned char* bytes = (unsigned char*)guard;

	if (!guard)
		return;
	__atomic_store_n(&bytes[1], 0x00, __ATOMIC_RELEASE);
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
	syscall(SYS_exit_group, status);
	__builtin_unreachable();
}

void _exit(int status)
{
	trace_process_exit_code("_exit", status, SHIM_CALLER_RETURN_ADDRESS());
	syscall(SYS_exit_group, status);
	__builtin_unreachable();
}

static int shim_errno_from_linux(int linux_errno)
{
	switch (linux_errno) {
	case EAGAIN:
		return 35;
	case ETIMEDOUT:
		return 60;
	default:
		return linux_errno;
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

static inline uintptr_t *darwin_tsd_mirror_base(void)
{
#if defined(__aarch64__)
	uintptr_t *result;
	__asm__ volatile("mrs %0, tpidr_el0" : "=r"(result));
	return result;
#else
	return NULL;
#endif
}

static inline int darwin_tsd_key_index(pthread_key_t key)
{
	unsigned int value = (unsigned int)key;
	if (value < DARWIN_TSD_FIRST_KEY || value > DARWIN_TSD_LAST_KEY)
		return -1;
	return (int)(value - DARWIN_TSD_FIRST_KEY);
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
	unsigned int value;
	int index;

	value = __sync_fetch_and_add(&next_darwin_tsd_key, 1);
	if (value > DARWIN_TSD_LAST_KEY)
		return EAGAIN;

	index = (int)(value - DARWIN_TSD_FIRST_KEY);
	darwin_tsd_destructors[index] = destructor;
	darwin_tsd_values[index] = NULL;
	*key = (pthread_key_t)value;

	if (shim_trace_enabled())
		fprintf(stderr, "libsystem_shim: pthread_key_create -> %u\n", value);

	return 0;
}

int pthread_setspecific(pthread_key_t key, const void *value)
{
	uintptr_t *mirror_base;
	int index = darwin_tsd_key_index(key);

	if (index < 0)
		return EINVAL;

	darwin_tsd_values[index] = (void *)value;
	mirror_base = darwin_tsd_mirror_base();
	if (mirror_base)
		mirror_base[(unsigned int)key] = (uintptr_t)value;

	if (shim_trace_enabled())
		fprintf(stderr, "libsystem_shim: pthread_setspecific(%u, %p)\n",
		        (unsigned int)key, value);

	return 0;
}

void *pthread_getspecific(pthread_key_t key)
{
	int index = darwin_tsd_key_index(key);

	if (index < 0)
		return NULL;
	if (shim_trace_enabled())
		fprintf(stderr, "libsystem_shim: pthread_getspecific(%u) -> %p\n",
		        (unsigned int)key, darwin_tsd_values[index]);
	return darwin_tsd_values[index];
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

static int shim_alloc_trace_enabled(void)
{
	const char* value = getenv("MACHGATE_TRACE_ALLOC");
	return value && value[0] && strcmp(value, "0") != 0;
}

static int shim_alloc_trace_full_enabled(void)
{
	const char* value = getenv("MACHGATE_TRACE_ALLOC");
	return value && (strcmp(value, "2") == 0 ||
	                 strcmp(value, "all") == 0 ||
	                 strcmp(value, "full") == 0 ||
	                 strcmp(value, "verbose") == 0);
}

static int shim_alloc_mismatch_trace_enabled(void)
{
	const char* value = getenv("MACHGATE_TRACE_ALLOC_MISMATCH");
	if (value && value[0] && strcmp(value, "0") != 0)
		return 1;
	return shim_alloc_trace_enabled();
}

static int shim_alloc_signal_dump_enabled(void)
{
	const char* value = getenv("MACHGATE_TRACE_ALLOC_MISMATCH");
	if (value && strcmp(value, "0") == 0)
		return 0;
	return 1;
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

	if (trace_fd < 0)
		return;

	offset = snprintf(buffer, sizeof(buffer), "fdtrace pid=%d tid=%d ",
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

#define ATTR_SLOT_COUNT 64

struct pthread_attr_slot {
	const void* key;
	pthread_attr_t native;
	int used;
};

static struct pthread_attr_slot pthread_attr_slots[ATTR_SLOT_COUNT];

static struct pthread_attr_slot* pthread_attr_slot_find(const void* key)
{
	for (int i = 0; i < ATTR_SLOT_COUNT; i++) {
		if (pthread_attr_slots[i].used && pthread_attr_slots[i].key == key)
			return &pthread_attr_slots[i];
	}
	return NULL;
}

static struct pthread_attr_slot* pthread_attr_slot_alloc(const void* key)
{
	struct pthread_attr_slot* slot = pthread_attr_slot_find(key);
	if (slot)
		return slot;

	for (int i = 0; i < ATTR_SLOT_COUNT; i++) {
		if (!pthread_attr_slots[i].used) {
			pthread_attr_slots[i].used = 1;
			pthread_attr_slots[i].key = key;
			return &pthread_attr_slots[i];
		}
	}
	return NULL;
}

int pthread_attr_init(pthread_attr_t* attr)
{
	static int (*real_pthread_attr_init)(pthread_attr_t*) = NULL;
	struct pthread_attr_slot* slot = pthread_attr_slot_alloc(attr);

	if (!real_pthread_attr_init)
		real_pthread_attr_init = dlsym(RTLD_NEXT, "pthread_attr_init");
	if (!real_pthread_attr_init)
		return ENOSYS;
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
	struct pthread_attr_slot* slot = pthread_attr_slot_find(attr);

	if (!real_pthread_attr_destroy)
		real_pthread_attr_destroy = dlsym(RTLD_NEXT, "pthread_attr_destroy");
	if (!real_pthread_attr_destroy)
		return ENOSYS;
	if (!slot)
		return real_pthread_attr_destroy(attr);

	int result = real_pthread_attr_destroy(&slot->native);
	slot->used = 0;
	slot->key = NULL;
	return result;
}

int pthread_attr_setstacksize(pthread_attr_t* attr, size_t stack_size)
{
	static int (*real_pthread_attr_setstacksize)(pthread_attr_t*, size_t) = NULL;
	struct pthread_attr_slot* slot = pthread_attr_slot_find(attr);

	if (!real_pthread_attr_setstacksize)
		real_pthread_attr_setstacksize = dlsym(RTLD_NEXT, "pthread_attr_setstacksize");
	if (!real_pthread_attr_setstacksize)
		return ENOSYS;
	return real_pthread_attr_setstacksize(slot ? &slot->native : attr, stack_size);
}

int pthread_attr_getstacksize(const pthread_attr_t* attr, size_t* stack_size)
{
	static int (*real_pthread_attr_getstacksize)(const pthread_attr_t*, size_t*) = NULL;
	struct pthread_attr_slot* slot = pthread_attr_slot_find(attr);

	if (!real_pthread_attr_getstacksize)
		real_pthread_attr_getstacksize = dlsym(RTLD_NEXT, "pthread_attr_getstacksize");
	if (!real_pthread_attr_getstacksize)
		return ENOSYS;
	return real_pthread_attr_getstacksize(slot ? &slot->native : attr, stack_size);
}

int pthread_attr_setdetachstate(pthread_attr_t* attr, int detach_state)
{
	static int (*real_pthread_attr_setdetachstate)(pthread_attr_t*, int) = NULL;
	struct pthread_attr_slot* slot = pthread_attr_slot_find(attr);
	int linux_state = detach_state;

	if (!real_pthread_attr_setdetachstate)
		real_pthread_attr_setdetachstate = dlsym(RTLD_NEXT, "pthread_attr_setdetachstate");
	if (!real_pthread_attr_setdetachstate)
		return ENOSYS;

	if (detach_state == 1)
		linux_state = PTHREAD_CREATE_JOINABLE;
	else if (detach_state == 2)
		linux_state = PTHREAD_CREATE_DETACHED;
	return real_pthread_attr_setdetachstate(slot ? &slot->native : attr, linux_state);
}

int pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                   void* (*start_routine)(void*), void* arg)
{
	static int (*real_pthread_create)(pthread_t*, const pthread_attr_t*,
	                                  void* (*)(void*), void*) = NULL;
	struct pthread_attr_slot* slot = pthread_attr_slot_find(attr);

	if (!real_pthread_create)
		real_pthread_create = dlsym(RTLD_NEXT, "pthread_create");
	if (!real_pthread_create)
		return ENOSYS;
	if (shim_trace_enabled())
		fprintf(stderr, "libsystem_shim: pthread_create(start=%p arg=%p attr=%p native=%p)\n",
		        start_routine, arg, attr, slot ? (void*)&slot->native : NULL);
	return real_pthread_create(thread, slot ? &slot->native : attr,
	                           start_routine, arg);
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
		return real_sysconf(_SC_NPROCESSORS_CONF);
	case DARWIN_SC_NPROCESSORS_ONLN:
		return real_sysconf(_SC_NPROCESSORS_ONLN);
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

struct darwin_kevent_placeholder {
	uint64_t ident;
	int16_t filter;
	uint16_t flags;
	uint32_t fflags;
	int64_t data;
	void* udata;
};

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
		if (eventlist && nevents > 0)
			result = emit_kqueue_receipts(changelist, nchanges, eventlist,
			                              nevents);
		if (shim_trace_enabled())
			fprintf(stderr, "libsystem_shim: kevent(kq=%d nchanges=%d nevents=%d receipts=%d) -> %d errno=0\n",
			        kq, nchanges, nevents, result, result);
		shim_fd_trace_log("kevent caller=%p kq=%d nchanges=%d nevents=%d receipts=%d result=%d errno=0\n",
		                  SHIM_CALLER_RETURN_ADDRESS(), kq, nchanges,
		                  nevents, result, result);
		return result;
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
	(void)thread;
	return (uint32_t)syscall(SYS_gettid);
}

/* pthread_threadid_np — get a unique 64-bit thread ID */
int pthread_threadid_np(pthread_t thread, uint64_t *thread_id)
{
	(void)thread;
	if (!thread_id) return EINVAL;
	*thread_id = (uint64_t)syscall(SYS_gettid);
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

/* ===== Blocks runtime (stubs) ===== */

/* These are only used by Objective-C code paths (Cocoa/AppKit SDL2 backend).
 * After SDL2 trampolining, they should never be called. Provide stubs that
 * warn if hit. */

void *_NSConcreteGlobalBlock = NULL;
void *_NSConcreteStackBlock = NULL;
void *_NSConcreteMallocBlock = NULL;

void* _Block_copy(const void* block)
{
	return (void*)block;
}

void _Block_release(const void* block)
{
	(void)block;
}

void _Block_object_assign(void *dst, const void *src, int flags)
{
	(void)dst; (void)src; (void)flags;
	fprintf(stderr, "libsystem_shim: WARNING: _Block_object_assign called (ObjC code path?)\n");
}

void _Block_object_dispose(const void *obj, int flags)
{
	(void)obj; (void)flags;
	fprintf(stderr, "libsystem_shim: WARNING: _Block_object_dispose called (ObjC code path?)\n");
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
};

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

/* ---- Queues (stubs — async/sync execute inline) ---- */

void* dispatch_get_global_queue(long priority, unsigned long flags)
{
	(void)priority; (void)flags;
	return &_dispatch_global_default;
}

void* dispatch_queue_create(const char *label, void *attr)
{
	(void)attr;
	struct dispatch_queue *q = (struct dispatch_queue *)calloc(1, sizeof(*q));
	if (!q) return NULL;
	q->type = DISPATCH_OBJ_QUEUE;
	q->refcount = 1;
	if (label) strncpy(q->label, label, sizeof(q->label) - 1);
	return q;
}

/* ---- Async/sync (execute blocks inline — no real concurrency) ---- */

typedef void (*dispatch_block_t)(void);
typedef void (*dispatch_function_t)(void*);

struct machgate_dispatch_source {
	void* type;
	uintptr_t handle;
	uintptr_t mask;
	void* queue;
	void* event_handler;
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

void dispatch_async(void *queue, void *block)
{
	(void)queue;
	dispatch_invoke_block(block);
}

void dispatch_sync(void *queue, void *block)
{
	(void)queue;
	dispatch_invoke_block(block);
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
		source->event_handler = handler;
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

#define DARWIN_UL_OPCODE_MASK 0x000000ffU
#define DARWIN_ULF_NO_ERRNO 0x01000000U
#define DARWIN_ULF_WAKE_ALL 0x00000100U

static int ulock_result(int result, uint32_t operation)
{
	if (result == 0)
		return 0;
	if (operation & DARWIN_ULF_NO_ERRNO)
		return -errno;
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

/* ===== Thread-local variable support (stubs) ===== */

void _tlv_atexit(void (*func)(void *), void *arg)
{
	(void)func; (void)arg;
	/* Best-effort: ignore TLV cleanup registration */
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

static void tlv_destructor(void* block)
{
	free(block);
}

void* _tlv_bootstrap_impl(struct tlv_descriptor* desc)
{
	if (desc->key == 0) {
		pthread_mutex_lock(&tlv_mutex);
		if (desc->key == 0) {
			pthread_key_t key;
			pthread_key_create(&key, tlv_destructor);
			desc->key = (unsigned long)key + 1;
		}
		pthread_mutex_unlock(&tlv_mutex);
	}

	pthread_key_t key = (pthread_key_t)(desc->key - 1);
	void* block = pthread_getspecific(key);

	if (!block) {
		/* Allocate per-thread TLV block */
		size_t total = __tlv_image_size + __tlv_bss_size;
		if (total < 4096) total = 4096; /* minimum size */
		block = calloc(1, total);

		/* Copy initial values from __thread_data */
		if (__tlv_image_base && __tlv_image_size > 0)
			memcpy(block, __tlv_image_base, __tlv_image_size);

		pthread_setspecific(key, block);
	}

	return (char*)block + desc->offset;
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

int shim_open(const char *pathname, int flags, ...) __asm__("open");
int shim_open(const char *pathname, int flags, ...)
{
	int linux_flags = translate_oflags(flags);
	mode_t mode = 0;

	if (flags & DARWIN_O_CREAT) {
		va_list args;
		va_start(args, flags);
		mode = va_arg(args, mode_t);
		va_end(args);
	}

	/* aarch64 Linux has no SYS_open; everything goes through SYS_openat */
	return syscall(SYS_openat, AT_FDCWD, pathname, linux_flags, mode);
}

int shim_openat(int dirfd, const char *pathname, int flags, ...) __asm__("openat");
int shim_openat(int dirfd, const char *pathname, int flags, ...)
{
	int linux_flags = translate_oflags(flags);
	mode_t mode = 0;

	if (flags & DARWIN_O_CREAT) {
		va_list args;
		va_start(args, flags);
		mode = va_arg(args, mode_t);
		va_end(args);
	}

	return syscall(SYS_openat, translate_dirfd(dirfd), pathname,
	               linux_flags, mode);
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

	int ret = syscall(SYS_fcntl, fd, linux_cmd, arg);
	int saved_errno = errno;
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

int shim_fcntl(int fd, int cmd, ...) __asm__("fcntl");
int shim_fcntl(int fd, int cmd, ...)
{
	va_list args;
	va_start(args, cmd);
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
static real_free_fn    real_free    = NULL;
static real_calloc_fn  real_calloc  = NULL;
static real_realloc_fn real_realloc = NULL;
static real_posix_memalign_fn real_posix_memalign = NULL;

static char bootstrap_buf[4096];
static int bootstrap_pos = 0;

#define MACHGATE_ALLOCATION_INITIAL_TABLE_SIZE 262144
#define MACHGATE_ALLOCATION_MAX_TABLE_SIZE 4194304

struct machgate_allocation_record {
	void* ptr;
	size_t size;
	void* zone;
	unsigned flags;
};

static struct machgate_allocation_record allocation_records_static[MACHGATE_ALLOCATION_INITIAL_TABLE_SIZE];
static struct machgate_allocation_record* allocation_records = allocation_records_static;
static size_t allocation_records_capacity = MACHGATE_ALLOCATION_INITIAL_TABLE_SIZE;
static volatile int allocation_records_lock;

#define MACHGATE_ALLOCATION_GUEST_CXX 1u

static void lock_allocation_records(void)
{
	while (__sync_lock_test_and_set(&allocation_records_lock, 1))
		sched_yield();
}

static void unlock_allocation_records(void)
{
	__sync_lock_release(&allocation_records_lock);
}

static size_t allocation_record_index(const void* ptr, size_t capacity)
{
	uintptr_t value = (uintptr_t)ptr;

	value >>= 4;
	value ^= value >> 17;
	value ^= value >> 31;
	return value & (capacity - 1);
}

static int insert_allocation_record(struct machgate_allocation_record* records,
                                    size_t capacity, void* ptr, size_t size,
                                    void* zone, unsigned flags)
{
	size_t index = allocation_record_index(ptr, capacity);
	struct machgate_allocation_record* reusable_record = NULL;

	for (size_t probe = 0; probe < capacity; probe++) {
		struct machgate_allocation_record* record =
		    &records[(index + probe) & (capacity - 1)];

		if (record->ptr == ptr) {
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
			record->ptr = ptr;
			record->size = size;
			record->zone = zone;
			record->flags = flags;
			return 1;
		}
	}
	if (reusable_record) {
		reusable_record->ptr = ptr;
		reusable_record->size = size;
		reusable_record->zone = zone;
		reusable_record->flags = flags;
		return 1;
	}
	return 0;
}

static int grow_allocation_records(void)
{
	size_t old_capacity = allocation_records_capacity;
	size_t new_capacity = old_capacity * 2;
	size_t new_bytes;
	struct machgate_allocation_record* old_records = allocation_records;
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
		if (!insert_allocation_record(new_records, new_capacity, record->ptr,
		                              record->size, record->zone,
		                              record->flags)) {
			syscall(SYS_munmap, new_records, new_bytes);
			return 0;
		}
	}

	allocation_records = new_records;
	allocation_records_capacity = new_capacity;
	if (old_records != allocation_records_static)
		syscall(SYS_munmap, old_records, old_capacity * sizeof(*old_records));
	return 1;
}

static void record_allocation_with_zone(void* ptr, size_t size, void* zone)
{
	if (!ptr)
		return;

	lock_allocation_records();
	while (!insert_allocation_record(allocation_records,
	                                 allocation_records_capacity, ptr, size,
	                                 zone, 0)) {
		if (grow_allocation_records())
			continue;
		break;
	}
	unlock_allocation_records();
}

static void record_allocation(void* ptr, size_t size)
{
	record_allocation_with_zone(ptr, size, NULL);
}

static int lookup_allocation(const void* ptr, size_t* size_out, void** zone_out,
                             unsigned* flags_out)
{
	size_t index;

	if (!ptr)
		return 0;

	lock_allocation_records();
	index = allocation_record_index(ptr, allocation_records_capacity);
	for (size_t probe = 0; probe < allocation_records_capacity; probe++) {
		const struct machgate_allocation_record* record =
		    &allocation_records[(index + probe) & (allocation_records_capacity - 1)];

		if (!record->ptr) {
			unlock_allocation_records();
			return 0;
		}
		if (record->ptr == ptr) {
			if (size_out)
				*size_out = record->size;
			if (zone_out)
				*zone_out = record->zone;
			if (flags_out)
				*flags_out = record->flags;
			unlock_allocation_records();
			return 1;
		}
	}

	unlock_allocation_records();
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
	size_t index;

	if (!ptr)
		return;

	lock_allocation_records();
	index = allocation_record_index(ptr, allocation_records_capacity);
	for (size_t probe = 0; probe < allocation_records_capacity; probe++) {
		struct machgate_allocation_record* record =
		    &allocation_records[(index + probe) & (allocation_records_capacity - 1)];

		if (!record->ptr) {
			unlock_allocation_records();
			return;
		}
		if (record->ptr == ptr) {
			record->size = 0;
			record->zone = NULL;
			record->flags = 0;
			unlock_allocation_records();
			return;
		}
	}
	unlock_allocation_records();
}

static void mark_guest_cxx_allocation(void* ptr, size_t size)
{
	size_t index;
	struct machgate_allocation_record* reusable_record = NULL;

	if (!ptr)
		return;

	lock_allocation_records();
retry:
	index = allocation_record_index(ptr, allocation_records_capacity);
	reusable_record = NULL;
	for (size_t probe = 0; probe < allocation_records_capacity; probe++) {
		struct machgate_allocation_record* record =
		    &allocation_records[(index + probe) & (allocation_records_capacity - 1)];

		if (!record->ptr) {
			if (reusable_record)
				record = reusable_record;
			record->ptr = ptr;
			record->size = recorded_allocation_size(ptr, size);
			record->zone = NULL;
			record->flags = MACHGATE_ALLOCATION_GUEST_CXX;
			unlock_allocation_records();
			return;
		}
		if (record->ptr && record->size == 0 && !reusable_record)
			reusable_record = record;
		if (record->ptr == ptr) {
			if (record->size == 0)
				record->size = recorded_allocation_size(ptr, size);
			record->flags |= MACHGATE_ALLOCATION_GUEST_CXX;
			unlock_allocation_records();
			return;
		}
	}
	if (reusable_record) {
		reusable_record->ptr = ptr;
		reusable_record->size = recorded_allocation_size(ptr, size);
		reusable_record->zone = NULL;
		reusable_record->flags = MACHGATE_ALLOCATION_GUEST_CXX;
		unlock_allocation_records();
		return;
	}
	if (grow_allocation_records())
		goto retry;
	unlock_allocation_records();
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
	real_malloc  = (real_malloc_fn)dlsym(RTLD_NEXT, "malloc");
	real_free    = (real_free_fn)dlsym(RTLD_NEXT, "free");
	real_calloc  = (real_calloc_fn)dlsym(RTLD_NEXT, "calloc");
	real_realloc = (real_realloc_fn)dlsym(RTLD_NEXT, "realloc");
	real_posix_memalign = (real_posix_memalign_fn)dlsym(RTLD_NEXT,
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

static void shim_free_impl_at(void *ptr, void* caller)
{
	size_t old_size = 0;
	int known;

	if (!ptr) return;
	known = lookup_allocation_size(ptr, &old_size);
	shim_trace_alloc_event_at("free", ptr, old_size, old_size, caller, known);
	if (!known && shim_alloc_mismatch_trace_enabled())
		shim_dump_recent_alloc_events("free-unknown-pointer", ptr);
	forget_allocation(ptr);
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
	if (real_free) real_free(ptr);
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
