#include "execve_reexec.h"
#include "guest_process_runtime.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#define DARWIN_E2BIG 7
#define DARWIN_EFAULT 14
#define DARWIN_EIO 5
#define DARWIN_ENOEXEC 8
#define DARWIN_ENOTSUP 45

#define MACHGATE_EXECVE_MAX_ARGS 4096
#define MACHGATE_EXECVE_MAX_ENVS 4096
#define MACHGATE_EXECVE_PATH_MAX 4096

extern char** environ;

static pid_t machgate_execve_process_pid;
static char machgate_execve_loader_path[MACHGATE_EXECVE_PATH_MAX];
static char machgate_execve_dylib_map_env[MACHGATE_EXECVE_PATH_MAX];
static char machgate_execve_ld_library_path_env[MACHGATE_EXECVE_PATH_MAX];
static char machgate_execve_ld_preload_env[MACHGATE_EXECVE_PATH_MAX];
static char machgate_execve_trace_file[MACHGATE_EXECVE_PATH_MAX];
static int machgate_execve_trace_enabled_snapshot;

static int read_loader_path(char* loader_path, size_t loader_path_size);
static int env_name_matches(const char* env_value, const char* name);
static int env_has_name(char* const envp[], const char* name);

__attribute__((constructor))
static void init_execve_process_pid(void)
{
	machgate_execve_process_pid = (pid_t)syscall(SYS_getpid);

	read_loader_path(machgate_execve_loader_path,
	                 sizeof(machgate_execve_loader_path));

	for (size_t index = 0; environ && environ[index]; index++) {
		if (env_name_matches(environ[index], "MACHGATE_DYLIB_MAP")) {
			strncpy(machgate_execve_dylib_map_env, environ[index],
			        sizeof(machgate_execve_dylib_map_env) - 1);
			machgate_execve_dylib_map_env[
				sizeof(machgate_execve_dylib_map_env) - 1] = '\0';
		} else if (env_name_matches(environ[index], "LD_LIBRARY_PATH")) {
			strncpy(machgate_execve_ld_library_path_env, environ[index],
			        sizeof(machgate_execve_ld_library_path_env) - 1);
			machgate_execve_ld_library_path_env[
				sizeof(machgate_execve_ld_library_path_env) - 1] = '\0';
		} else if (env_name_matches(environ[index], "LD_PRELOAD")) {
			strncpy(machgate_execve_ld_preload_env, environ[index],
			        sizeof(machgate_execve_ld_preload_env) - 1);
			machgate_execve_ld_preload_env[
				sizeof(machgate_execve_ld_preload_env) - 1] = '\0';
		}
	}

	machgate_execve_trace_enabled_snapshot =
		(getenv("MACHGATE_TRACE_EXECVE") != NULL) ||
		(getenv("MACHGATE_TRACE_SYSCALL") != NULL) ||
		(getenv("MACHGATE_TRACE_SHIM") != NULL);

	const char* trace_file = getenv("MACHGATE_EXECVE_TRACE_FILE");
	if (trace_file && *trace_file) {
		strncpy(machgate_execve_trace_file, trace_file,
		        sizeof(machgate_execve_trace_file) - 1);
		machgate_execve_trace_file[
			sizeof(machgate_execve_trace_file) - 1] = '\0';
		machgate_execve_trace_enabled_snapshot = 1;
	}
}

static int execve_in_fork_child(void)
{
	return machgate_execve_process_pid &&
	       (pid_t)syscall(SYS_getpid) != machgate_execve_process_pid;
}

static int trace_execve_enabled(void)
{
	if (execve_in_fork_child())
		return machgate_execve_trace_enabled_snapshot;
	return getenv("MACHGATE_TRACE_EXECVE") ||
	       getenv("MACHGATE_TRACE_SYSCALL") ||
	       getenv("MACHGATE_TRACE_SHIM");
}

static int trace_execve_failure(const char* reason, const char* path, int error)
{
	if (trace_execve_enabled()) {
		fprintf(stderr, "machgate: execve reexec rejected path='%s' reason=%s darwin_errno=%d\n",
		        path ? path : "(null)", reason, error);
	}
	return error;
}

static void trace_execve_forksafe(const char* action, const char* p1, const char* p2)
{
	if (!machgate_execve_trace_enabled_snapshot)
		return;
	int fd = 2;
	int file_fd = -1;
	const char* s;
	if (machgate_execve_trace_file[0]) {
		file_fd = (int)syscall(SYS_openat, AT_FDCWD,
		                       machgate_execve_trace_file,
		                       O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC,
		                       0600);
		if (file_fd >= 0)
			fd = file_fd;
	}
	s = "machgate: execve reexec forksafe ";
	syscall(SYS_write, fd, s, strlen(s));
	if (action) {
		syscall(SYS_write, fd, action, strlen(action));
	}
	if (p1) {
		s = " '";
		syscall(SYS_write, fd, s, strlen(s));
		syscall(SYS_write, fd, p1, strlen(p1));
		s = "'";
		syscall(SYS_write, fd, s, strlen(s));
	}
	if (p2) {
		s = " '";
		syscall(SYS_write, fd, s, strlen(s));
		syscall(SYS_write, fd, p2, strlen(p2));
		s = "'";
		syscall(SYS_write, fd, s, strlen(s));
	}
	s = "\n";
	syscall(SYS_write, fd, s, 1);
	if (file_fd >= 0)
		syscall(SYS_close, file_fd);
}

static void trace_execve_forksafe_number(const char* action, long value)
{
	char buffer[32];
	size_t index = sizeof(buffer);
	unsigned long magnitude;

	if (!machgate_execve_trace_enabled_snapshot)
		return;

	buffer[--index] = '\0';
	if (value < 0)
		magnitude = (unsigned long)(-value);
	else
		magnitude = (unsigned long)value;

	do {
		buffer[--index] = (char)('0' + (magnitude % 10));
		magnitude /= 10;
	} while (magnitude && index > 0);

	if (value < 0 && index > 0)
		buffer[--index] = '-';

	trace_execve_forksafe(action, &buffer[index], NULL);
}

static int execve_errno_from_linux(int linux_errno)
{
	switch (linux_errno) {
	case 0:
		return DARWIN_EIO;
	case ENOTSUP:
		return DARWIN_ENOTSUP;
	default:
		return linux_errno;
	}
}

static int has_macho_magic(const unsigned char magic[4])
{
	return (magic[0] == 0xcf && magic[1] == 0xfa &&
	        magic[2] == 0xed && magic[3] == 0xfe) ||
	       (magic[0] == 0xfe && magic[1] == 0xed &&
	        magic[2] == 0xfa && magic[3] == 0xcf) ||
	       (magic[0] == 0xca && magic[1] == 0xfe &&
	        magic[2] == 0xba && magic[3] == 0xbe) ||
	       (magic[0] == 0xbe && magic[1] == 0xba &&
	        magic[2] == 0xfe && magic[3] == 0xca);
}

static int path_is_macho_common(const char* path, int* darwin_error, int trace)
{
	unsigned char magic[4];
	int fd;
	ssize_t bytes_read;

	errno = 0;
	fd = (int)syscall(SYS_openat, AT_FDCWD, path, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		*darwin_error = execve_errno_from_linux(errno);
		if (trace && trace_execve_enabled()) {
			fprintf(stderr, "machgate: execve reexec open failed path='%s' linux_errno=%d darwin_errno=%d\n",
			        path ? path : "(null)", errno, *darwin_error);
		}
		return 0;
	}

	errno = 0;
	bytes_read = (ssize_t)syscall(SYS_read, fd, magic, sizeof(magic));
	syscall(SYS_close, fd);

	if (bytes_read < 0) {
		*darwin_error = execve_errno_from_linux(errno);
		return 0;
	}
	if ((size_t)bytes_read != sizeof(magic)) {
		*darwin_error = DARWIN_ENOEXEC;
		if (trace && trace_execve_enabled()) {
			fprintf(stderr, "machgate: execve reexec short magic path='%s' bytes=%zd\n",
			        path ? path : "(null)", bytes_read);
		}
		return 0;
	}
	if (!has_macho_magic(magic)) {
		*darwin_error = DARWIN_ENOTSUP;
		if (trace && trace_execve_enabled()) {
			fprintf(stderr, "machgate: execve reexec non-macho path='%s' magic=%02x%02x%02x%02x\n",
			        path ? path : "(null)", magic[0], magic[1], magic[2], magic[3]);
		}
		return 0;
	}

	return 1;
}

static int path_is_macho_forksafe(const char* path, int* darwin_error)
{
	return path_is_macho_common(path, darwin_error, 0);
}

int machgate_path_is_macho(const char* path)
{
	int result;

	return path && path_is_macho_forksafe(path, &result);
}

static int read_loader_path(char* loader_path, size_t loader_path_size)
{
	ssize_t length;

	errno = 0;
	length = (ssize_t)syscall(SYS_readlinkat, AT_FDCWD, "/proc/self/exe",
	                          loader_path, loader_path_size - 1);
	if (length < 0)
		return execve_errno_from_linux(errno);
	if ((size_t)length >= loader_path_size - 1)
		return DARWIN_E2BIG;

	loader_path[length] = '\0';
	return 0;
}

static const char* machgate_guest_executable_path(void)
{
	const struct machgate_guest_process_runtime_state* runtime =
		machgate_current_guest_process_runtime();
	if (runtime && runtime->executable_path && *runtime->executable_path)
		return runtime->executable_path;
	return NULL;
}

static const char* resolve_guest_exec_path(const char* path)
{
	const char* guest_path = machgate_guest_executable_path();

	if (!path || !guest_path)
		return path;

	if (strcmp(path, "/proc/self/exe") == 0)
		return guest_path;
	if (machgate_execve_loader_path[0] &&
	    strcmp(path, machgate_execve_loader_path) == 0)
		return guest_path;

	return path;
}

static int count_guest_argv(char* const guest_argv[], size_t* guest_argc)
{
	size_t count = 0;

	if (!guest_argv) {
		*guest_argc = 0;
		return 0;
	}

	while (guest_argv[count]) {
		if (count == MACHGATE_EXECVE_MAX_ARGS)
			return DARWIN_E2BIG;
		count++;
	}

	*guest_argc = count;
	return 0;
}

static int build_loader_argv(const char* loader_path, const char* guest_path,
                             char* const guest_argv[], char* loader_argv[],
                             size_t loader_argv_size)
{
	size_t guest_argc;
	int result = count_guest_argv(guest_argv, &guest_argc);
	if (result)
		return result;

	if (guest_argc + 2 > loader_argv_size)
		return DARWIN_E2BIG;

	loader_argv[0] = (char*)loader_path;
	loader_argv[1] = (char*)guest_path;
	for (size_t index = 1; index < guest_argc; index++)
		loader_argv[index + 1] = guest_argv[index];
	loader_argv[guest_argc + 1] = NULL;

	return 0;
}

static int append_env_snapshot(char* loader_envp[], size_t* loader_envc,
                               const char* value)
{
	if (!value || !*value)
		return 0;
	if (*loader_envc == MACHGATE_EXECVE_MAX_ENVS)
		return DARWIN_E2BIG;

	loader_envp[*loader_envc] = (char*)value;
	(*loader_envc)++;
	return 0;
}

static int build_loader_envp_forksafe(char* const guest_envp[],
                                      char* loader_envp[])
{
	size_t loader_envc = 0;
	int result;

	if (guest_envp) {
		while (guest_envp[loader_envc]) {
			if (loader_envc == MACHGATE_EXECVE_MAX_ENVS)
				return DARWIN_E2BIG;
			loader_envp[loader_envc] = guest_envp[loader_envc];
			loader_envc++;
		}
	}

	loader_envp[loader_envc] = NULL;
	if (!env_has_name(loader_envp, "MACHGATE_DYLIB_MAP")) {
		result = append_env_snapshot(loader_envp, &loader_envc,
		                             machgate_execve_dylib_map_env);
		if (result)
			return result;
	}
	if (!env_has_name(loader_envp, "LD_LIBRARY_PATH")) {
		result = append_env_snapshot(loader_envp, &loader_envc,
		                             machgate_execve_ld_library_path_env);
		if (result)
			return result;
	}
	if (!env_has_name(loader_envp, "LD_PRELOAD")) {
		result = append_env_snapshot(loader_envp, &loader_envc,
		                             machgate_execve_ld_preload_env);
		if (result)
			return result;
	}
	loader_envp[loader_envc] = NULL;
	return 0;
}

static int env_name_matches(const char* env_value, const char* name)
{
	size_t name_length = strlen(name);
	return strncmp(env_value, name, name_length) == 0 &&
	       env_value[name_length] == '=';
}

static int env_has_name(char* const envp[], const char* name)
{
	if (!envp)
		return 0;

	for (size_t index = 0; envp[index]; index++) {
		if (env_name_matches(envp[index], name))
			return 1;
	}

	return 0;
}

static int append_env_value(char* loader_envp[], size_t* loader_envc,
                            const char* name, char* buffer,
                            size_t buffer_size)
{
	const char* value = getenv(name);
	int written;

	if (!value || !*value)
		return 0;

	written = snprintf(buffer, buffer_size, "%s=%s", name, value);
	if (written < 0 || (size_t)written >= buffer_size)
		return DARWIN_E2BIG;
	if (*loader_envc == MACHGATE_EXECVE_MAX_ENVS)
		return DARWIN_E2BIG;

	loader_envp[*loader_envc] = buffer;
	(*loader_envc)++;
	return 0;
}

static int build_loader_envp(char* const guest_envp[], char* loader_envp[],
			     char* dylib_map_env,
                             char* ld_library_path_env,
                             char* ld_preload_env)
{
	char* const* base_envp = guest_envp ? guest_envp : environ;
	size_t loader_envc = 0;
	int result;

	if (base_envp) {
		while (base_envp[loader_envc]) {
			if (loader_envc == MACHGATE_EXECVE_MAX_ENVS)
				return DARWIN_E2BIG;
			loader_envp[loader_envc] = base_envp[loader_envc];
			loader_envc++;
		}
	}
	loader_envp[loader_envc] = NULL;

	if (!env_has_name(loader_envp, "MACHGATE_DYLIB_MAP")) {
		result = append_env_value(loader_envp, &loader_envc,
		                          "MACHGATE_DYLIB_MAP", dylib_map_env,
		                          MACHGATE_EXECVE_PATH_MAX);
		if (result)
			return result;
	}

	if (!env_has_name(loader_envp, "LD_LIBRARY_PATH")) {
		result = append_env_value(loader_envp, &loader_envc, "LD_LIBRARY_PATH",
		                          ld_library_path_env,
		                          MACHGATE_EXECVE_PATH_MAX);
		if (result)
			return result;
	}

	if (!env_has_name(loader_envp, "LD_PRELOAD")) {
		result = append_env_value(loader_envp, &loader_envc, "LD_PRELOAD",
		                          ld_preload_env,
		                          MACHGATE_EXECVE_PATH_MAX);
		if (result)
			return result;
	}

	loader_envp[loader_envc] = NULL;
	return 0;
}

int machgate_execve_macho_guest(const char* path, char* const guest_argv[],
                                char* const guest_envp[])
{
	const char* guest_path;
	char loader_path[MACHGATE_EXECVE_PATH_MAX];
	char dylib_map_env[MACHGATE_EXECVE_PATH_MAX];
	char ld_library_path_env[MACHGATE_EXECVE_PATH_MAX];
	char ld_preload_env[MACHGATE_EXECVE_PATH_MAX];
	char* loader_argv[MACHGATE_EXECVE_MAX_ARGS + 3];
	char* loader_envp[MACHGATE_EXECVE_MAX_ENVS + 3];
	int result;

	if (execve_in_fork_child())
		return machgate_execve_macho_guest_forksafe(path, guest_argv, guest_envp);

	if (!path)
		return trace_execve_failure("null-path", path, DARWIN_EFAULT);

	guest_path = resolve_guest_exec_path(path);

	if (!path_is_macho_forksafe(guest_path, &result))
		return result;

	result = read_loader_path(loader_path, sizeof(loader_path));
	if (result)
		return result;

	result = build_loader_argv(loader_path, guest_path, guest_argv, loader_argv,
	                           sizeof(loader_argv) / sizeof(loader_argv[0]));
	if (result)
		return result;

	result = build_loader_envp(guest_envp, loader_envp, dylib_map_env,
	                           ld_library_path_env, ld_preload_env);
	if (result)
		return result;

	errno = 0;
	if (trace_execve_enabled()) {
		fprintf(stderr, "machgate: execve reexec loader='%s' guest='%s'\n",
		        loader_path, guest_path);
	}
	syscall(SYS_execve, loader_path, loader_argv, loader_envp);
	return trace_execve_failure("linux-execve-failed", guest_path,
	                            execve_errno_from_linux(errno));
}

int machgate_execve_macho_guest_forksafe(const char* path,
                                         char* const guest_argv[],
                                         char* const guest_envp[])
{
	const char* guest_path;
	char loader_path[MACHGATE_EXECVE_PATH_MAX];
	char* loader_argv[MACHGATE_EXECVE_MAX_ARGS + 3];
	char* loader_envp[MACHGATE_EXECVE_MAX_ENVS + 3];
	int result;

	trace_execve_forksafe("enter", path, NULL);

	if (!path) {
		trace_execve_forksafe("null-path", path, NULL);
		return DARWIN_EFAULT;
	}

	guest_path = resolve_guest_exec_path(path);
	trace_execve_forksafe("resolve", guest_path, NULL);

	if (!path_is_macho_forksafe(guest_path, &result)) {
		trace_execve_forksafe("non-macho", guest_path, NULL);
		return result;
	}

	result = read_loader_path(loader_path, sizeof(loader_path));
	if (result) {
		trace_execve_forksafe("read-loader-fail", NULL, NULL);
		return result;
	}

	result = build_loader_argv(loader_path, guest_path, guest_argv, loader_argv,
	                           sizeof(loader_argv) / sizeof(loader_argv[0]));
	if (result) {
		trace_execve_forksafe("argv-build-fail", NULL, NULL);
		return result;
	}

	result = build_loader_envp_forksafe(guest_envp, loader_envp);
	if (result) {
		trace_execve_forksafe("env-build-fail", NULL, NULL);
		return result;
	}

	errno = 0;
	trace_execve_forksafe("argv0", loader_argv[0], NULL);
	trace_execve_forksafe("argv1", loader_argv[1], NULL);
	trace_execve_forksafe("argv2", loader_argv[2], NULL);
	trace_execve_forksafe("argv3", loader_argv[3], NULL);
	trace_execve_forksafe_number("fd1-flags", syscall(SYS_fcntl, 1, F_GETFD));
	trace_execve_forksafe_number("fd2-flags", syscall(SYS_fcntl, 2, F_GETFD));
	trace_execve_forksafe_number("env-ld-preload",
	                             env_has_name(loader_envp, "LD_PRELOAD"));
	trace_execve_forksafe_number("env-ld-library-path",
	                             env_has_name(loader_envp, "LD_LIBRARY_PATH"));
	trace_execve_forksafe_number(
		"env-dylib-map", env_has_name(loader_envp, "MACHGATE_DYLIB_MAP"));
	trace_execve_forksafe("exec", loader_path, guest_path);
	syscall(SYS_execve, loader_path, loader_argv, loader_envp);
	trace_execve_forksafe_number("linux-execve-errno", errno);
	trace_execve_forksafe("post-exec-fail", NULL, NULL);
	return execve_errno_from_linux(errno);
}
