#define _GNU_SOURCE
#include "ports/socket_state.h"
#include "../syscall/execve_reexec.h"
#include "allocation_ledger.h"
#include "darwin_errno.h"
#include "fd_lifecycle.h"
#include "file_abi.h"
#include "host_facts.h"
#include "guest_hardware_compat.h"
#include "initializer_context.h"
#include "registered_memory_compat.h"
#include "pthread_attr_compat.h"
#include "thread_local_compat.h"
#include "kqueue_compat.h"
#include "variadic_abi_compat.h"
#include "guest_process_runtime.h"
#include "native_provider_guest_import.h"
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>
#include <wctype.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/resource.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/uio.h>

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
static void trace_guest_address_context(const char* label, uintptr_t address);
static uintptr_t trace_ucontext_reg(void* ucontext, int reg);
static void trace_signal_indirect_branch(uintptr_t call_site, void* ucontext);
static void shim_dump_recent_alloc_events(const char* reason, const void* ptr);
static pid_t machgate_shim_process_pid;
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
uint64_t vm_page_size = 4096;
uint64_t vm_kernel_page_size = 4096;
uint32_t kIOMasterPortDefault = 0;
uint32_t kIOMainPortDefault = 0;

enum {
	MACHGATE_KERN_NOT_SUPPORTED = 46,
	MACHGATE_MACH_SEND_MSG = 0x00000001,
	MACHGATE_MACH_RCV_MSG = 0x00000002,
	MACHGATE_MACH_SEND_INVALID_DEST = 0x10000003,
	MACHGATE_MACH_RCV_INVALID_NAME = 0x10004002,
};

uint32_t mach_host_self(void)
{
	return 0x104;
}

uint32_t mach_thread_self(void)
{
	return (uint32_t)syscall(SYS_gettid);
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

int mach_msg(void* msg, uint32_t option, uint32_t send_size,
             uint32_t rcv_size, uint32_t rcv_name,
             uint32_t timeout, uint32_t notify)
{
	(void)msg;
	(void)send_size;
	(void)rcv_size;
	(void)rcv_name;
	(void)timeout;
	(void)notify;

	if (option & MACHGATE_MACH_SEND_MSG)
		return MACHGATE_MACH_SEND_INVALID_DEST;
	if (option & MACHGATE_MACH_RCV_MSG)
		return MACHGATE_MACH_RCV_INVALID_NAME;
	return MACHGATE_KERN_NOT_SUPPORTED;
}

int mach_port_deallocate(uint32_t task, uint32_t name)
{
	(void)task;
	(void)name;
	return MACHGATE_KERN_NOT_SUPPORTED;
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
		basic.max_cpus = machgate_guest_cpu_count();
		basic.avail_cpus = basic.max_cpus;
		basic.memory_size = 0x80000000U;
		basic.cpu_type = 0x0100000c;
		basic.physical_cpu = basic.max_cpus;
		basic.physical_cpu_max = basic.max_cpus;
		basic.logical_cpu = basic.max_cpus;
		basic.logical_cpu_max = basic.max_cpus;
		basic.max_mem = machgate_host_memory_size();

		size_t available = (size_t)*info_count * sizeof(int32_t);
		size_t copy_size = available < sizeof(basic) ? available : sizeof(basic);
		memcpy(info, &basic, copy_size);
		*info_count = (uint32_t)(sizeof(basic) / sizeof(int32_t));
		return 0;
	}

	memset(info, 0, (size_t)*info_count * sizeof(int32_t));
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

	uint32_t processor_count = (uint32_t)machgate_guest_cpu_count();
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
	uint8_t uuid[16];
	char uuid_string[37];

	if (CCRandomGenerateBytes(uuid, sizeof(uuid)) != 0)
		return NULL;
	uuid[6] = (uuid[6] & 0x0f) | 0x40;
	uuid[8] = (uuid[8] & 0x3f) | 0x80;
	snprintf(uuid_string, sizeof(uuid_string),
	         "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-"
	         "%02x%02x%02x%02x%02x%02x",
	         uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5],
	         uuid[6], uuid[7], uuid[8], uuid[9], uuid[10], uuid[11],
	         uuid[12], uuid[13], uuid[14], uuid[15]);
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

uint32_t IOServiceGetMatchingService(uint32_t main_port,
				     CFDictionaryRef matching)
{
	(void)main_port;
	(void)matching;
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
	    type_id == CF_TYPE_ID_DICTIONARY || type_id == CF_TYPE_ID_BOOLEAN ||
	    type_id == CF_TYPE_ID_NUMBER)
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
			int fd = syscall(SYS_openat, AT_FDCWD, action->path,
			                 machgate_linux_open_flags(action->flags),
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

static ssize_t shim_write_result(long syscall_number, int fd,
                                  const void* buffer, size_t size_or_count)
{
	errno = 0;
	ssize_t result = syscall_number == SYS_write ?
		machgate_socket_write(fd, buffer, size_or_count) :
		machgate_socket_writev(fd, buffer, (int)size_or_count);
	int saved_errno = result < 0 ? machgate_darwin_errno_from_linux(errno) : errno;
	shim_fd_trace_log("write caller=%p fd=%d size=%zu result=%zd errno=%d\n",
	                  SHIM_CALLER_RETURN_ADDRESS(), fd, size_or_count,
	                  result, result < 0 ? saved_errno : 0);
	errno = saved_errno;
	return result;
}

ssize_t read(int fd, void* buffer, size_t size)
{
	errno = 0;
	ssize_t result = (ssize_t)syscall(SYS_read, fd, buffer, size);
	int saved_errno = errno;
	if (result < 0)
		saved_errno = machgate_darwin_errno_from_linux(saved_errno);
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
		saved_errno = machgate_darwin_errno_from_linux(saved_errno);
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
	return shim_write_result(SYS_write, fd, buffer, size);
}

ssize_t writev(int fd, const struct iovec* iov, int iovcnt)
{
	return shim_write_result(SYS_writev, fd, iov, (size_t)iovcnt);
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

__attribute__((visibility("hidden")))
void machgate_trace_process_exit_code(const char* name, int status,
	                              void* caller)
{
	FILE* trace_file = shim_open_trace_file();

	if (!trace_file)
		return;

	fprintf(trace_file,
	        "libsystem_shim: %s pid=%d tid=%d ppid=%d fork_child=%d status=%d caller=%p last_wait_valid=%d last_wait_owner=%d last_wait_result=%d last_wait_linux_status=%#x last_wait_darwin_status=%#x last_wait_status_ptr=%#lx\n",
	        name, (int)syscall(SYS_getpid), (int)shim_trace_tid(),
	        (int)syscall(SYS_getppid), machgate_shim_in_fork_child(), status,
	        caller, shim_last_wait_valid,
	        (int)shim_last_wait_owner_pid,
	        (int)shim_last_wait_result_pid,
	        shim_last_wait_linux_status,
	        shim_last_wait_darwin_status,
	        (unsigned long)shim_last_wait_status_ptr);
	fclose(trace_file);
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

static int shim_trace_enabled(void);
static int darwin_signal_to_linux(int darwin_signal);

#define DARWIN_TSD_FIRST_KEY 128
#define DARWIN_TSD_LAST_KEY 511
#define DARWIN_TSD_KEY_COUNT (DARWIN_TSD_LAST_KEY - DARWIN_TSD_FIRST_KEY + 1)

static pthread_mutex_t darwin_tsd_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint64_t darwin_tsd_generations[DARWIN_TSD_KEY_COUNT];
static __thread uint64_t darwin_tsd_value_generations[DARWIN_TSD_KEY_COUNT];
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
	char name[64];
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

extern void* machgate_guest_thread_context(void) __attribute__((weak));

static inline uintptr_t* darwin_tsd_mirror_base(void)
{
	if (machgate_guest_thread_context)
		return machgate_guest_thread_context();
	return NULL;
}

static inline int darwin_tsd_key_index(pthread_key_t key)
{
	unsigned int value = (unsigned int)key;
	if (value < DARWIN_TSD_FIRST_KEY || value > DARWIN_TSD_LAST_KEY)
		return -1;
	return (int)(value - DARWIN_TSD_FIRST_KEY);
}

__attribute__((constructor(101)))
static void init_pthread_identity(void)
{
	machgate_main_pthread = pthread_self();
	machgate_main_pthread_set = 1;
	register_current_pthread_identity();
}

static int create_darwin_tsd_key(pthread_key_t* key, void (*destructor)(void*))
{
	pthread_mutex_lock(&darwin_tsd_mutex);
	for (int index = 0; index < DARWIN_TSD_KEY_COUNT; index++) {
		uint64_t generation = __atomic_load_n(&darwin_tsd_generations[index], __ATOMIC_RELAXED);
		if (generation & 1)
			continue;
		darwin_tsd_destructors[index] = destructor;
		darwin_tsd_values[index] = NULL;
		darwin_tsd_value_generations[index] = generation + 1;
		__atomic_store_n(&darwin_tsd_generations[index], generation + 1, __ATOMIC_RELEASE);
		*key = (pthread_key_t)(index + DARWIN_TSD_FIRST_KEY);
		uintptr_t* mirror_base = darwin_tsd_mirror_base();
		if (mirror_base)
			mirror_base[*key] = 0;
		pthread_mutex_unlock(&darwin_tsd_mutex);
		return 0;
	}
	pthread_mutex_unlock(&darwin_tsd_mutex);
	return EAGAIN;
}

static int delete_darwin_tsd_key(pthread_key_t key)
{
	int index = darwin_tsd_key_index(key);
	if (index < 0)
		return EINVAL;
	pthread_mutex_lock(&darwin_tsd_mutex);
	uint64_t generation = __atomic_load_n(&darwin_tsd_generations[index], __ATOMIC_RELAXED);
	if (!(generation & 1)) {
		pthread_mutex_unlock(&darwin_tsd_mutex);
		return EINVAL;
	}
	__atomic_store_n(&darwin_tsd_generations[index], generation + 1, __ATOMIC_RELEASE);
	darwin_tsd_destructors[index] = NULL;
	darwin_tsd_values[index] = NULL;
	uintptr_t* mirror_base = darwin_tsd_mirror_base();
	if (mirror_base)
		mirror_base[key] = 0;
	pthread_mutex_unlock(&darwin_tsd_mutex);
	return 0;
}

int pthread_key_delete(pthread_key_t key)
{
	return delete_darwin_tsd_key(key);
}

int machgate_darwin_pthread_key_delete(uint64_t key)
{
	if (key > DARWIN_TSD_LAST_KEY)
		return 22;
	return machgate_darwin_errno_from_linux(delete_darwin_tsd_key((pthread_key_t)key));
}

static int set_darwin_tsd_value(pthread_key_t key, const void *value)
{
	uintptr_t *mirror_base;
	int index = darwin_tsd_key_index(key);

	if (index < 0)
		return EINVAL;
	uint64_t generation = __atomic_load_n(&darwin_tsd_generations[index], __ATOMIC_ACQUIRE);
	if (!(generation & 1))
		return EINVAL;
	darwin_tsd_value_generations[index] = generation;
	darwin_tsd_values[index] = (void *)value;
	mirror_base = darwin_tsd_mirror_base();
	if (mirror_base)
		mirror_base[(unsigned int)key] = (uintptr_t)value;

	if (shim_trace_enabled())
		fprintf(stderr, "libsystem_shim: pthread_setspecific(%u, %p)\n",
		        (unsigned int)key, value);

	return 0;
}

static void* get_darwin_tsd_value(pthread_key_t key)
{
	int index = darwin_tsd_key_index(key);

	if (index < 0)
		return NULL;
	uint64_t generation = __atomic_load_n(&darwin_tsd_generations[index], __ATOMIC_ACQUIRE);
	if (!(generation & 1) || darwin_tsd_value_generations[index] != generation)
		return NULL;
	if (shim_trace_enabled())
		fprintf(stderr, "libsystem_shim: pthread_getspecific(%u) -> %p\n",
		        (unsigned int)key, darwin_tsd_values[index]);
	return darwin_tsd_values[index];
}

int pthread_key_create(pthread_key_t* key, void (*destructor)(void*))
{
	return create_darwin_tsd_key(key, destructor);
}

int pthread_setspecific(pthread_key_t key, const void* value)
{
	return set_darwin_tsd_value(key, value);
}

void* pthread_getspecific(pthread_key_t key)
{
	return get_darwin_tsd_value(key);
}

int machgate_darwin_pthread_key_create(uint64_t* key, void (*destructor)(void*))
{
	pthread_key_t native_key;
	int result = create_darwin_tsd_key(&native_key, destructor);
	if (result == 0)
		*key = native_key;
	return machgate_darwin_errno_from_linux(result);
}

int machgate_darwin_pthread_setspecific(uint64_t key, const void* value)
{
	if (key > DARWIN_TSD_LAST_KEY)
		return 22;
	return machgate_darwin_errno_from_linux(
		set_darwin_tsd_value((pthread_key_t)key, value));
}

void* machgate_darwin_pthread_getspecific(uint64_t key)
{
	if (key > DARWIN_TSD_LAST_KEY)
		return NULL;
	return get_darwin_tsd_value((pthread_key_t)key);
}

void machgate_run_darwin_tsd_destructors(void)
{
	for (int pass = 0; pass < PTHREAD_DESTRUCTOR_ITERATIONS; pass++) {
		int values_remain = 0;

		for (int index = 0; index < DARWIN_TSD_KEY_COUNT; index++) {
			void* value = get_darwin_tsd_value((pthread_key_t)(index + DARWIN_TSD_FIRST_KEY));
			pthread_mutex_lock(&darwin_tsd_mutex);
			void (*destructor)(void*) = darwin_tsd_destructors[index];
			pthread_mutex_unlock(&darwin_tsd_mutex);

			if (!value || !destructor)
				continue;
			darwin_tsd_values[index] = NULL;
			uintptr_t* mirror_base = darwin_tsd_mirror_base();
			if (mirror_base)
				mirror_base[index + DARWIN_TSD_FIRST_KEY] = 0;
			destructor(value);
			if (darwin_tsd_values[index])
				values_remain = 1;
		}
		if (!values_remain)
			break;
	}
}

void machgate_run_thread_local_termination_callbacks(void)
{
	machgate_run_tlv_exit_callbacks();
	machgate_run_darwin_tsd_destructors();
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
	static int configured = -1;
	int result = __atomic_load_n(&configured, __ATOMIC_ACQUIRE);
	if (result >= 0)
		return result;
	const char* value = getenv("MACHGATE_TRACE_SHIM");
	result = value && value[0] && strcmp(value, "0") != 0;
	__atomic_store_n(&configured, result, __ATOMIC_RELEASE);
	return result;
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

static const char* shim_host_environment_value(const char* name)
{
	size_t name_length = strlen(name);
	for (char** entry = environ; entry && *entry; entry++) {
		if ((*entry)[0] == name[0] &&
		    strncmp(*entry, name, name_length) == 0 &&
		    (*entry)[name_length] == '=')
			return *entry + name_length + 1;
	}
	return NULL;
}

static int shim_alloc_trace_enabled(void)
{
	const char* value = shim_host_environment_value("MACHGATE_TRACE_ALLOC");
	return value && value[0] && strcmp(value, "0") != 0;
}

static int shim_alloc_trace_full_enabled(void)
{
	const char* value = shim_host_environment_value("MACHGATE_TRACE_ALLOC");
	return value && (strcmp(value, "2") == 0 ||
	                 strcmp(value, "all") == 0 ||
	                 strcmp(value, "full") == 0 ||
	                 strcmp(value, "verbose") == 0);
}

static int shim_alloc_mismatch_trace_enabled(void)
{
	const char* value =
		shim_host_environment_value("MACHGATE_TRACE_ALLOC_MISMATCH");
	if (value && value[0] && strcmp(value, "0") != 0)
		return 1;
	return shim_alloc_trace_enabled();
}

static int shim_alloc_signal_dump_enabled(void)
{
	const char* value =
		shim_host_environment_value("MACHGATE_TRACE_ALLOC_MISMATCH");
	if (value && strcmp(value, "0") == 0)
		return 0;
	return 1;
}

static size_t shim_alloc_trace_size(void)
{
	const char* value =
		shim_host_environment_value("MACHGATE_TRACE_ALLOC_SIZE");
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
	const char* value =
		shim_host_environment_value("MACHGATE_TRACE_ALLOC_RECENT_FULL");
	return value && value[0] && strcmp(value, "0") != 0;
}

static size_t shim_alloc_recent_limit(void)
{
	const char* value =
		shim_host_environment_value("MACHGATE_TRACE_ALLOC_RECENT_LIMIT");
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
	length = machgate_host_vsnprintf(
		buffer + offset, sizeof(buffer) - (size_t)offset, format, args);
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

	register_current_pthread_identity();
	free_thread_start_context(context);
	void* result = start_routine(arg);
	machgate_run_thread_local_termination_callbacks();
	return result;
}

int pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                   void* (*start_routine)(void*), void* arg)
{
	struct shim_thread_start_context* context;
	int result;

	context = alloc_thread_start_context(start_routine, arg);
	if (!context)
		return EAGAIN;
	if (shim_trace_enabled())
		fprintf(stderr, "libsystem_shim: pthread_create(start=%p arg=%p attr=%p)\n",
		        start_routine, arg, attr);
	result = machgate_native_pthread_create(thread, attr, shim_pthread_start, context);
	if (result != 0) {
		free_thread_start_context(context);
		return result;
	}
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
	const struct machgate_guest_process_runtime_state* runtime;

	runtime = machgate_current_guest_process_runtime();
	if (runtime && runtime->main_stack_top &&
	    machgate_main_pthread_set &&
	    pthread_equal(thread, pthread_self()) &&
	    pthread_equal(thread, machgate_main_pthread)) {
		if (shim_trace_enabled())
			fprintf(stderr, "libsystem_shim: pthread_get_stackaddr_np main -> %p\n", runtime->main_stack_top);
		return runtime->main_stack_top;
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
	const struct machgate_guest_process_runtime_state* runtime;

	runtime = machgate_current_guest_process_runtime();
	if (runtime && runtime->main_stack_size &&
	    machgate_main_pthread_set &&
	    pthread_equal(thread, pthread_self()) &&
	    pthread_equal(thread, machgate_main_pthread)) {
		if (shim_trace_enabled())
			fprintf(stderr, "libsystem_shim: pthread_get_stacksize_np main -> %zu\n", runtime->main_stack_size);
		return runtime->main_stack_size;
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
		return machgate_guest_cpu_count();
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

int kqueue(void)
{
	return machgate_kqueue_open();
}

int kevent(int kq, const struct machgate_darwin_kevent* changelist,
           int nchanges, struct machgate_darwin_kevent* eventlist,
           int nevents, const struct timespec* timeout)
{
	return machgate_kqueue_event(kq, changelist, nchanges, eventlist, nevents,
	                             timeout);
}

int kevent64(int kq, const struct machgate_darwin_kevent64* changelist,
             int nchanges, struct machgate_darwin_kevent64* eventlist,
             int nevents, unsigned int flags, const struct timespec* timeout)
{
	if (flags) {
		errno = ENOSYS;
		return -1;
	}
	return machgate_kqueue_event64(kq, changelist, nchanges, eventlist,
	                               nevents, timeout);
}

void machgate_fd_did_close(int fd)
{
	{
		machgate_socket_state_close(fd);
		machgate_kqueue_fd_did_close(fd);
	}
}

void machgate_fd_did_duplicate(int from_fd, int to_fd)
{
	{
		machgate_socket_state_duplicate(from_fd, to_fd);
		machgate_kqueue_fd_did_duplicate(from_fd, to_fd);
	}
}

void* xpc_date_create_from_current(void)
{
	return NULL;
}

uint32_t pthread_mach_thread_np(pthread_t thread)
{
	uint64_t identity = pthread_identity_for_thread(thread);
	return (uint32_t)identity;
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

int pthread_self_is_exiting_np(void)
{
	return 0;
}

int pthread_setname_np(pthread_t thread, const char *name)
{
	const char* darwin_name = (const char*)thread;
	(void)name;
	size_t length = darwin_name ? strlen(darwin_name) : 0;
	if (length >= sizeof(pthread_id_slots[0].name)) {
		errno = 63;
		return -1;
	}

	char host_name[16] = {0};
	if (length)
		memcpy(host_name, darwin_name, length < 15 ? length : 15);
	int result = syscall(SYS_prctl, PR_SET_NAME, host_name, 0, 0, 0);
	if (result < 0) {
		errno = machgate_darwin_errno_from_linux(errno);
		return -1;
	}

	lock_pthread_id_slots();
	struct pthread_id_slot* slot = alloc_pthread_id_slot_unlocked(pthread_self());
	if (slot) {
		if (length)
			memcpy(slot->name, darwin_name, length);
		slot->name[length] = 0;
	}
	unlock_pthread_id_slots();
	if (!slot) {
		errno = 12;
		return -1;
	}
	return 0;
}

int pthread_getname_np(pthread_t thread, char* name, size_t capacity)
{
	lock_pthread_id_slots();
	struct pthread_id_slot* slot = find_pthread_id_slot_unlocked(thread);
	if (slot && capacity) {
		size_t length = strlen(slot->name);
		if (length >= capacity)
			length = capacity - 1;
		memcpy(name, slot->name, length);
		name[length] = 0;
	}
	unlock_pthread_id_slots();
	return slot ? 0 : 3;
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
		machgate_trace_initializer_context();
		fprintf(stderr,
		        "libsystem_shim: guest signal signum=%d darwin=%d code=%d addr=%p pc=%p lr=%p sp=%p fp=%p\n",
		        signum, linux_signal_to_darwin(signum), info ? info->si_code : 0,
		        info ? info->si_addr : NULL,
		        (void*)pc,
		        (void*)lr,
		        (void*)trace_ucontext_sp(ucontext),
		        (void*)trace_ucontext_reg(ucontext, 29));
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

pid_t vfork(void)
{
	pid_t child = fork();
	if (child < 0)
		return -1;
	return child;
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

#define DARWIN_MAP_ANON 0x1000
#define DARWIN_MAP_JIT 0x0800

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

/* ===== dlopen interception ===== */

static void* (*real_dlopen)(const char*, int) = NULL;
static void* (*real_dlsym)(void*, const char*) = NULL;

#define DARWIN_RTLD_NEXT ((void*)-1L)
#define DARWIN_RTLD_DEFAULT ((void*)-2L)
#define DARWIN_RTLD_SELF ((void*)-3L)
#define DARWIN_RTLD_MAIN_ONLY ((void*)-5L)

static void* translate_dlsym_handle(void* handle)
{
	if (handle == DARWIN_RTLD_NEXT ||
	    handle == DARWIN_RTLD_DEFAULT ||
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

__attribute__((visibility("default")))
void* machgate_darwin_dlsym_impl(void* handle, const char* symbol)
{
	static machgate_native_symbol_resolver_function native_resolver;
	if (!real_dlsym)
		real_dlsym = resolve_real_dlsym();
	if (!real_dlsym)
		return NULL;
	if (!native_resolver)
		native_resolver = (machgate_native_symbol_resolver_function)
			real_dlsym(RTLD_DEFAULT, MACHGATE_NATIVE_SYMBOL_RESOLVER_SYMBOL);
	void* native_handle = translate_dlsym_handle(handle);
	void* result = NULL;
	if (native_resolver && native_resolver(native_handle, symbol, &result))
		return result;
	return real_dlsym(native_handle, symbol);
}

void* dlsym(void* handle, const char* symbol) __asm__("dlsym");
void* dlsym(void* handle, const char* symbol)
{
	if (!real_dlsym)
		real_dlsym = resolve_real_dlsym();
	return real_dlsym ? real_dlsym(handle, symbol) : NULL;
}

__attribute__((visibility("hidden")))
void* machgate_guest_dlsym_address(void)
{
	return machgate_darwin_dlsym_impl;
}

static int translate_dlopen_flags(int flags)
{
	const int darwin_rtld_now = 0x2;
	const int darwin_rtld_global = 0x8;
	unsigned int mode_flags = (unsigned int)flags;
	int result = (mode_flags & darwin_rtld_now) ? RTLD_NOW : RTLD_LAZY;
	result |= (mode_flags & darwin_rtld_global) ? RTLD_GLOBAL : RTLD_LOCAL;
	return result;
}

static int is_shim_framework_path(const char* path)
{
	return path &&
	       (strstr(path, "CoreFoundation.framework") ||
	        strstr(path, "CoreServices.framework"));
}

static int initialize_real_dlopen(void)
{
	if (!real_dlsym)
		real_dlsym = resolve_real_dlsym();
	if (!real_dlsym)
		return 0;
	if (!real_dlopen)
		real_dlopen = real_dlsym(RTLD_NEXT, "dlopen");
	return real_dlopen != NULL;
}

__attribute__((visibility("default")))
void* machgate_darwin_dlopen_impl(const char* path, int flags)
{
	if (!initialize_real_dlopen())
		return NULL;

	int linux_flags = translate_dlopen_flags(flags);

	if (is_shim_framework_path(path)) {
		void* handle = real_dlopen("libsystem_shim.so", linux_flags);
		if (shim_trace_enabled())
			fprintf(stderr, "libsystem_shim: redirected dlopen('%s') -> libsystem_shim.so handle=%p\n",
			        path, handle);
		return handle;
	}

	return real_dlopen(path, linux_flags);
}

void* dlopen(const char* path, int flags) __asm__("dlopen");
void* dlopen(const char* path, int flags)
{
	if (!initialize_real_dlopen())
		return NULL;
	return real_dlopen(path, flags);
}

__attribute__((visibility("hidden")))
void* machgate_guest_dlopen_address(void)
{
	return machgate_darwin_dlopen_impl;
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

typedef void* (*host_malloc_function)(size_t);
typedef void (*host_free_function)(void*);
typedef void* (*host_calloc_function)(size_t, size_t);
typedef void* (*host_realloc_function)(void*, size_t);
typedef int (*host_posix_memalign_function)(void**, size_t, size_t);

static host_malloc_function real_malloc;
static host_free_function real_free;
static host_calloc_function real_calloc;
static host_realloc_function real_realloc;
static host_posix_memalign_function real_posix_memalign;

static unsigned char bootstrap_storage[4096];
static size_t bootstrap_used;

static int should_forward_guest_cxx_delete(const void* ptr)
{
	size_t size = 0;
	unsigned flags = 0;

	if (!ptr)
		return 0;
	if (machgate_lookup_allocation(ptr, &size, NULL, &flags))
		return size != 0 && (flags & MACHGATE_ALLOCATION_GUEST_CXX);
	return 1;
}

static void resolve_host_allocator(void)
{
	if (real_malloc)
		return;
	real_malloc = (host_malloc_function)dlsym(RTLD_NEXT, "malloc");
	real_free = (host_free_function)dlsym(RTLD_NEXT, "free");
	real_calloc = (host_calloc_function)dlsym(RTLD_NEXT, "calloc");
	real_realloc = (host_realloc_function)dlsym(RTLD_NEXT, "realloc");
	real_posix_memalign = (host_posix_memalign_function)dlsym(
		RTLD_NEXT, "posix_memalign");
}

static void* allocate_from_bootstrap_storage(size_t size)
{
	size_t start = (bootstrap_used + 15u) & ~(size_t)15u;
	if (start > sizeof(bootstrap_storage) ||
	    size > sizeof(bootstrap_storage) - start)
		return NULL;

	void* result = bootstrap_storage + start;
	memset(result, 0, size);
	bootstrap_used = start + size;
	machgate_record_allocation(result, size ? size : 1);
	return result;
}

static void *shim_malloc_impl_at(size_t size, void* caller)
{
	if (!real_malloc) {
		resolve_host_allocator();
		if (!real_malloc)
			return allocate_from_bootstrap_storage(size);
	}
	void *p = real_malloc(size);
	if (p) {
		memset(p, 0, size);
		machgate_record_allocation(p, machgate_recorded_allocation_size(p, size));
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
	if (machgate_release_registered_mapping(ptr))
		return;
	known = machgate_take_allocation(ptr, &old_size);
	shim_trace_alloc_event_at("free", ptr, old_size, old_size, caller, known);
	if (!known && shim_alloc_mismatch_trace_enabled())
		shim_dump_recent_alloc_events("free-unknown-pointer", ptr);
	if ((unsigned char*)ptr >= bootstrap_storage &&
	    (unsigned char*)ptr < bootstrap_storage + sizeof(bootstrap_storage))
		return;
	if (!real_free)
		resolve_host_allocator();
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
		resolve_host_allocator();
		if (!real_calloc) {
			size_t total = nmemb * size;
			return shim_malloc_impl_at(total, caller);
		}
	}
	void *p = real_calloc(nmemb, size);
	if (p) {
		machgate_record_allocation(p, machgate_recorded_allocation_size(p, nmemb * size));
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

	if (ptr && (unsigned char*)ptr >= bootstrap_storage &&
	    (unsigned char*)ptr < bootstrap_storage + sizeof(bootstrap_storage)) {
		machgate_lookup_allocation_size(ptr, &old_size);
		void* new_ptr = shim_malloc_impl_at(size, caller);
		if (!new_ptr && size != 0)
			return NULL;
		if (new_ptr && old_size)
			memcpy(new_ptr, ptr, old_size < size ? old_size : size);
		machgate_forget_allocation(ptr);
		return new_ptr;
	}

	if (ptr) {
		known = machgate_lookup_allocation_size(ptr, &old_size);
		if (!known && shim_alloc_mismatch_trace_enabled())
			shim_dump_recent_alloc_events("realloc-unknown-pointer", ptr);
	}
	if (!real_realloc)
		resolve_host_allocator();
	if (!real_realloc) return NULL;
	void *new_ptr = real_realloc(ptr, size);
	if (new_ptr || size == 0)
		machgate_forget_allocation(ptr);
	if (new_ptr) {
		machgate_record_allocation(new_ptr, machgate_recorded_allocation_size(new_ptr, size));
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
		resolve_host_allocator();
	if (!real_posix_memalign)
		return ENOMEM;
	int result = real_posix_memalign(memptr, alignment, size);
	if (result == 0) {
		machgate_record_allocation(*memptr, machgate_recorded_allocation_size(*memptr, size));
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

void machgate_shim_record_external_allocation(void* ptr, size_t size)
{
	if (ptr)
		machgate_record_allocation(ptr, size ? size : 1);
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

static void* machgate_shim_operator_new_aligned_at(size_t size,
                                                   size_t alignment,
                                                   void* caller)
{
	if (!alignment || (alignment & (alignment - 1)) != 0)
		return NULL;
	if (alignment < sizeof(void*))
		alignment = sizeof(void*);
	return machgate_shim_memalign_at(alignment, size, caller);
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
		machgate_mark_guest_cxx_allocation(result, size);
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
		machgate_mark_guest_cxx_allocation(result, size);
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
		machgate_mark_guest_cxx_allocation(result, size);
		return result;
	}
	return machgate_shim_operator_new_aligned_at(
		size, alignment, MACHGATE_SHIM_CALLER());
}

void* machgate_shim_guest_operator_new_array_aligned(size_t size, size_t alignment)
{
	if (guest_operator_new_array_aligned && !guest_cxx_allocator_depth) {
		guest_cxx_allocator_depth++;
		void* result = guest_operator_new_array_aligned(size, alignment);
		guest_cxx_allocator_depth--;
		machgate_mark_guest_cxx_allocation(result, size);
		return result;
	}
	return machgate_shim_operator_new_aligned_at(
		size, alignment, MACHGATE_SHIM_CALLER());
}

void* machgate_shim_guest_operator_new_nothrow(size_t size, const void* nothrow_arg)
{
	if (guest_operator_new_nothrow && !guest_cxx_allocator_depth) {
		guest_cxx_allocator_depth++;
		void* result = guest_operator_new_nothrow(size, nothrow_arg);
		guest_cxx_allocator_depth--;
		machgate_mark_guest_cxx_allocation(result, size);
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
		machgate_mark_guest_cxx_allocation(result, size);
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
		machgate_mark_guest_cxx_allocation(result, size);
		return result;
	}
	return machgate_shim_operator_new_aligned_at(
		size, alignment, MACHGATE_SHIM_CALLER());
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
		machgate_mark_guest_cxx_allocation(result, size);
		return result;
	}
	return machgate_shim_operator_new_aligned_at(
		size, alignment, MACHGATE_SHIM_CALLER());
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
	return machgate_shim_operator_new_aligned_at(
		size, alignment, MACHGATE_SHIM_CALLER());
}

void* machgate_operator_new_array_aligned(size_t size, size_t alignment) __asm__("_ZnamSt11align_val_t");
void* machgate_operator_new_array_aligned(size_t size, size_t alignment)
{
	return machgate_shim_operator_new_aligned_at(
		size, alignment, MACHGATE_SHIM_CALLER());
}

void* machgate_operator_new_aligned_nothrow(size_t size, size_t alignment, const void* nothrow_arg) __asm__("_ZnwmSt11align_val_tRKSt9nothrow_t");
void* machgate_operator_new_aligned_nothrow(size_t size, size_t alignment, const void* nothrow_arg)
{
	(void)nothrow_arg;
	return machgate_shim_operator_new_aligned_at(
		size, alignment, MACHGATE_SHIM_CALLER());
}

void* machgate_operator_new_array_aligned_nothrow(size_t size, size_t alignment, const void* nothrow_arg) __asm__("_ZnamSt11align_val_tRKSt9nothrow_t");
void* machgate_operator_new_array_aligned_nothrow(size_t size, size_t alignment, const void* nothrow_arg)
{
	(void)nothrow_arg;
	return machgate_shim_operator_new_aligned_at(
		size, alignment, MACHGATE_SHIM_CALLER());
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

	if (machgate_lookup_allocation(ptr, &size, &zone, NULL) && size != 0)
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
			machgate_record_allocation_with_zone(result,
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
			machgate_forget_allocation(ptr);
		if (result) {
			machgate_record_allocation_with_zone(result,
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
		machgate_forget_allocation(ptr);
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
			machgate_record_allocation_with_zone(result,
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
			machgate_record_allocation_with_zone(result,
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
			machgate_record_allocation_with_zone(results[index],
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
			machgate_forget_allocation(pointers[index]);
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
			machgate_record_allocation_with_zone(result,
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
		machgate_forget_allocation(ptr);
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
			machgate_record_allocation_with_zone(result,
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
	if (machgate_lookup_allocation(ptr, &size, &zone, NULL) && size != 0)
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
	if (machgate_lookup_allocation_size(ptr, &size) && size != 0)
		return size;
	result = malloc_usable_size((void*)ptr);
	if (result == 0 && shim_alloc_trace_enabled())
		fprintf(stderr, "libsystem_shim: malloc_size zero foreign ptr=%p\n", ptr);
	return result;
}
