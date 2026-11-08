#define _GNU_SOURCE

#include "../syscall/execve_reexec.h"

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#define MACHGATE_POPEN_MAX_ARGS 4096

struct popen_record {
	FILE* stream;
	pid_t child;
	struct popen_record* next;
};

typedef FILE* (*native_popen_function)(const char*, const char*);
typedef int (*native_pclose_function)(FILE*);
typedef FILE* (*native_fdopen_function)(int, const char*);
typedef int (*native_fclose_function)(FILE*);

static native_popen_function native_popen;
static native_pclose_function native_pclose;
static native_fdopen_function native_fdopen;
static native_fclose_function native_fclose;
static struct popen_record* popen_records;
static atomic_flag popen_records_lock = ATOMIC_FLAG_INIT;

extern char** environ;

static int resolve_native_stdio(void)
{
	if (!native_popen)
		native_popen = (native_popen_function)dlsym(RTLD_NEXT, "popen");
	if (!native_pclose)
		native_pclose = (native_pclose_function)dlsym(RTLD_NEXT, "pclose");
	if (!native_fdopen)
		native_fdopen = (native_fdopen_function)dlsym(RTLD_NEXT, "fdopen");
	if (!native_fclose)
		native_fclose = (native_fclose_function)dlsym(RTLD_NEXT, "fclose");
	if (!native_popen || !native_pclose || !native_fdopen || !native_fclose) {
		errno = ENOSYS;
		return -1;
	}
	return 0;
}

static void lock_popen_records(void)
{
	while (atomic_flag_test_and_set_explicit(&popen_records_lock,
	                                         memory_order_acquire))
		sched_yield();
}

static void unlock_popen_records(void)
{
	atomic_flag_clear_explicit(&popen_records_lock, memory_order_release);
}

static int token_is_plain(const char* token)
{
	return strpbrk(token, "\"'`\\|;&<>()*?$~") == NULL;
}

static int parse_macho_command(char* command, char* guest_argv[],
                               int* merge_stderr)
{
	char* save_pointer = NULL;
	char* token = strtok_r(command, " \t", &save_pointer);
	size_t count = 0;

	*merge_stderr = 0;
	while (token) {
		if (strcmp(token, "2>&1") == 0) {
			*merge_stderr = 1;
		} else {
			if (!token_is_plain(token) || count == MACHGATE_POPEN_MAX_ARGS - 1)
				return 0;
			guest_argv[count++] = token;
		}
		token = strtok_r(NULL, " \t", &save_pointer);
	}
	guest_argv[count] = NULL;
	return count > 0;
}

static void popen_child_exec(const char* guest_path, char* const guest_argv[],
                             int child_end, int parent_end, int redirect_to,
                             int merge_stderr)
{
	syscall(SYS_close, parent_end);
	if (child_end != redirect_to &&
	    syscall(SYS_dup3, child_end, redirect_to, 0) < 0)
		syscall(SYS_exit_group, 126);
	if (redirect_to == STDOUT_FILENO && merge_stderr &&
	    syscall(SYS_dup3, STDOUT_FILENO, STDERR_FILENO, 0) < 0)
		syscall(SYS_exit_group, 126);
	if (child_end > STDERR_FILENO)
		syscall(SYS_close, child_end);

	machgate_execve_macho_guest_forksafe(guest_path, guest_argv, environ);
	syscall(SYS_exit_group, 126);
	__builtin_unreachable();
}

static FILE* popen_macho(char* guest_argv[], const char* type, int merge_stderr)
{
	int redirect_to = type[0] == 'w' ? STDIN_FILENO : STDOUT_FILENO;
	int close_on_exec = strchr(type, 'e') != NULL;
	int pipe_fds[2];
	struct popen_record* record = calloc(1, sizeof(*record));
	FILE* stream;

	if (!record)
		return NULL;
	if (syscall(SYS_pipe2, pipe_fds, close_on_exec ? O_CLOEXEC : 0) < 0) {
		free(record);
		return NULL;
	}

	int child_end = redirect_to == STDOUT_FILENO ? pipe_fds[1] : pipe_fds[0];
	int parent_end = redirect_to == STDOUT_FILENO ? pipe_fds[0] : pipe_fds[1];
	pid_t child = fork();
	if (child < 0) {
		int saved_errno = errno;
		syscall(SYS_close, pipe_fds[0]);
		syscall(SYS_close, pipe_fds[1]);
		free(record);
		errno = saved_errno;
		return NULL;
	}
	if (child == 0)
		popen_child_exec(guest_argv[0], guest_argv, child_end, parent_end,
		                 redirect_to, merge_stderr);

	syscall(SYS_close, child_end);
	stream = native_fdopen(parent_end, type[0] == 'w' ? "w" : "r");
	if (!stream) {
		int saved_errno = errno;
		syscall(SYS_close, parent_end);
		syscall(SYS_kill, child, SIGKILL);
		syscall(SYS_wait4, child, NULL, 0, NULL);
		free(record);
		errno = saved_errno;
		return NULL;
	}

	record->stream = stream;
	record->child = child;
	lock_popen_records();
	record->next = popen_records;
	popen_records = record;
	unlock_popen_records();
	return stream;
}

FILE* popen(const char* command, const char* type)
{
	char* command_copy;
	char* guest_argv[MACHGATE_POPEN_MAX_ARGS];
	int merge_stderr;
	FILE* result;

	if (!command || !type) {
		errno = EINVAL;
		return NULL;
	}
	if (resolve_native_stdio() != 0)
		return NULL;
	if ((type[0] != 'r' && type[0] != 'w') ||
	    (type[1] && (type[1] != 'e' || type[2])))
		return native_popen(command, type);

	command_copy = strdup(command);
	if (!command_copy)
		return NULL;
	if (!parse_macho_command(command_copy, guest_argv, &merge_stderr) ||
	    !machgate_path_is_macho(guest_argv[0])) {
		free(command_copy);
		return native_popen(command, type);
	}

	result = popen_macho(guest_argv, type, merge_stderr);
	free(command_copy);
	return result;
}

int pclose(FILE* stream)
{
	struct popen_record* record = NULL;
	struct popen_record** link;
	int close_result;
	int status = 0;

	if (resolve_native_stdio() != 0)
		return -1;

	lock_popen_records();
	link = &popen_records;
	while (*link) {
		if ((*link)->stream == stream) {
			record = *link;
			*link = record->next;
			break;
		}
		link = &(*link)->next;
	}
	unlock_popen_records();
	if (!record)
		return native_pclose(stream);

	close_result = native_fclose(stream);
	for (;;) {
		pid_t result = (pid_t)syscall(SYS_wait4, record->child, &status, 0, NULL);
		if (result == record->child)
			break;
		if (result < 0 && errno != EINTR) {
			free(record);
			return -1;
		}
	}
	free(record);
	return close_result == 0 ? status : -1;
}
