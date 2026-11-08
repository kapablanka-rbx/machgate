extern int close(int descriptor);
extern int fcntl(int descriptor, int command, ...);
extern int ioctl(int descriptor, unsigned long request, ...);
extern int open(const char* path, int flags, ...);
extern int openat(int directory, const char* path, int flags, ...);
extern int pipe(int descriptors[2]);

enum {
	DARWIN_F_GETFD = 1,
	DARWIN_F_SETFD = 2,
	DARWIN_F_GETFL = 3,
	DARWIN_F_GETLK = 7,
	DARWIN_F_SETLK = 8,
	DARWIN_F_PREALLOCATE = 42,
	DARWIN_F_UNLCK = 2,
	DARWIN_F_WRLCK = 3,
	DARWIN_F_ALLOCATEALL = 4,
	DARWIN_F_PEOFPOSMODE = 3,
	DARWIN_FD_CLOEXEC = 1,
	DARWIN_O_NONBLOCK = 0x00000004,
	DARWIN_O_WRONLY = 1,
	DARWIN_O_RDWR = 2,
	DARWIN_O_CREAT = 0x00000200,
	DARWIN_O_EXCL = 0x00000800,
	DARWIN_AT_FDCWD = -2,
};

#define DARWIN_FIONBIO 0x8004667eul

struct darwin_flock {
	long long start;
	long long length;
	int process;
	short type;
	short whence;
};

struct darwin_fstore {
	unsigned int flags;
	int position_mode;
	long long offset;
	long long length;
	int allocated_bytes;
	int padding;
};

static void exit_guest(int status)
{
	register unsigned long result __asm__("x0") = (unsigned long)status;
	register unsigned long number __asm__("x16") = 1;
	__asm__ volatile("svc #0x80" : : "r"(result), "r"(number) : "memory");
	__builtin_unreachable();
}

static long raw_syscall3(unsigned long number, unsigned long argument0,
	                 unsigned long argument1, unsigned long argument2)
{
	register unsigned long result __asm__("x0") = argument0;
	register unsigned long value1 __asm__("x1") = argument1;
	register unsigned long value2 __asm__("x2") = argument2;
	register unsigned long syscall_number __asm__("x16") = number;
	__asm__ volatile("svc #0x80"
	                 : "+r"(result)
	                 : "r"(value1), "r"(value2), "r"(syscall_number)
	                 : "cc", "memory");
	return (long)result;
}

static void check_create_mode(int use_openat)
{
	const char* path = use_openat ? "darwin_file_openat_mode.tmp" :
	                            "darwin_file_open_mode.tmp";
	int descriptor;
	if (use_openat)
		descriptor = openat(DARWIN_AT_FDCWD, path,
		                    DARWIN_O_CREAT | DARWIN_O_EXCL | DARWIN_O_RDWR,
		                    0640);
	else
		descriptor = open(path, DARWIN_O_CREAT | DARWIN_O_EXCL | DARWIN_O_RDWR,
		                  0640);
	if (descriptor < 0)
		exit_guest(use_openat ? 88 : 87);
	if (close(descriptor) != 0)
		exit_guest(89);

	if (use_openat)
		descriptor = openat(DARWIN_AT_FDCWD, path, DARWIN_O_WRONLY);
	else
		descriptor = open(path, DARWIN_O_WRONLY);
	if (descriptor < 0)
		exit_guest(use_openat ? 91 : 90);
	if (close(descriptor) != 0)
		exit_guest(95);
}

static void check_lock_path(const char* path, int raw_command, int failure_base)
{
	int descriptor = open(path, DARWIN_O_CREAT | DARWIN_O_EXCL |
	                      DARWIN_O_WRONLY, 0600);
	if (descriptor < 0)
		exit_guest(failure_base);

	struct darwin_flock lock = {0, 0, 0, DARWIN_F_WRLCK, 0};
	long result;
	if (raw_command)
		result = raw_syscall3((unsigned long)raw_command,
		                      (unsigned long)descriptor, DARWIN_F_SETLK,
		                      (unsigned long)&lock);
	else
		result = fcntl(descriptor, DARWIN_F_SETLK, &lock);
	if (result != 0)
		exit_guest(failure_base + 1);

	if (raw_command)
		result = raw_syscall3((unsigned long)raw_command,
		                      (unsigned long)descriptor, DARWIN_F_GETLK,
		                      (unsigned long)&lock);
	else
		result = fcntl(descriptor, DARWIN_F_GETLK, &lock);
	if (result != 0 || lock.type != DARWIN_F_UNLCK)
		exit_guest(failure_base + 2);

	lock.type = DARWIN_F_UNLCK;
	if (raw_command)
		result = raw_syscall3((unsigned long)raw_command,
		                      (unsigned long)descriptor, DARWIN_F_SETLK,
		                      (unsigned long)&lock);
	else
		result = fcntl(descriptor, DARWIN_F_SETLK, &lock);
	if (result != 0)
		exit_guest(failure_base + 3);
	if (close(descriptor) != 0)
		exit_guest(failure_base + 4);
}

static void check_preallocate_path(const char* path, int raw_command,
	                           int failure_base)
{
	int descriptor = open(path, DARWIN_O_CREAT | DARWIN_O_EXCL |
	                      DARWIN_O_RDWR, 0600);
	if (descriptor < 0)
		exit_guest(failure_base);

	struct darwin_fstore store = {0, 0, 0, 4096, 0, 0};
	long result;
	if (raw_command)
		result = raw_syscall3((unsigned long)raw_command,
		                      (unsigned long)descriptor, DARWIN_F_PREALLOCATE,
		                      (unsigned long)&store);
	else
		result = fcntl(descriptor, DARWIN_F_PREALLOCATE, &store);
	if (result != 0)
		exit_guest(failure_base + 1);

	store.flags = DARWIN_F_ALLOCATEALL;
	store.position_mode = DARWIN_F_PEOFPOSMODE;
	store.length = 1024;
	if (raw_command)
		result = raw_syscall3((unsigned long)raw_command,
		                      (unsigned long)descriptor, DARWIN_F_PREALLOCATE,
		                      (unsigned long)&store);
	else
		result = fcntl(descriptor, DARWIN_F_PREALLOCATE, &store);
	if (result != 0)
		exit_guest(failure_base + 2);
	if (close(descriptor) != 0)
		exit_guest(failure_base + 3);
}

int main(void)
{
	int descriptors[2];
	if (pipe(descriptors) != 0)
		exit_guest(81);

	int enabled = 1;
	if (ioctl(descriptors[0], DARWIN_FIONBIO, &enabled) != 0)
		exit_guest(82);
	if ((fcntl(descriptors[0], DARWIN_F_GETFL) & DARWIN_O_NONBLOCK) == 0)
		exit_guest(83);
	if (fcntl(descriptors[0], DARWIN_F_SETFD, DARWIN_FD_CLOEXEC) != 0)
		exit_guest(84);
	if ((fcntl(descriptors[0], DARWIN_F_GETFD) & DARWIN_FD_CLOEXEC) == 0)
		exit_guest(85);

	if (close(descriptors[0]) != 0 || close(descriptors[1]) != 0)
		exit_guest(86);

	check_create_mode(0);
	check_create_mode(1);
	check_lock_path("darwin_file_import_lock.tmp", 0, 100);
	check_lock_path("darwin_file_raw_92_lock.tmp", 92, 110);
	check_lock_path("darwin_file_raw_406_lock.tmp", 406, 120);
	check_preallocate_path("darwin_file_import_preallocate.tmp", 0, 130);
	check_preallocate_path("darwin_file_raw_92_preallocate.tmp", 92, 140);
	check_preallocate_path("darwin_file_raw_406_preallocate.tmp", 406, 150);
	exit_guest(0);
}
