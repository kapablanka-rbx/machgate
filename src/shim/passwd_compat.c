#define _GNU_SOURCE

#include <dlfcn.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

struct darwin_passwd {
	char* pw_name;
	char* pw_passwd;
	uint32_t pw_uid;
	uint32_t pw_gid;
	int64_t pw_change;
	char* pw_class;
	char* pw_gecos;
	char* pw_dir;
	char* pw_shell;
	int64_t pw_expire;
};

_Static_assert(sizeof(struct darwin_passwd) == 72,
	       "Darwin passwd layout changed");
_Static_assert(offsetof(struct darwin_passwd, pw_expire) == 64,
	       "Darwin passwd pw_expire offset changed");

struct passwd {
	char* pw_name;
	char* pw_passwd;
	uint32_t pw_uid;
	uint32_t pw_gid;
	char* pw_gecos;
	char* pw_dir;
	char* pw_shell;
};

typedef struct passwd* (*host_getpwuid_function)(uint32_t);
typedef struct passwd* (*host_getpwnam_function)(const char*);
typedef int (*host_getpwuid_r_function)(uint32_t, struct passwd*, char*, size_t,
                                        struct passwd**);
typedef int (*host_getpwnam_r_function)(const char*, struct passwd*, char*,
                                        size_t, struct passwd**);

static void* next_symbol(const char* name)
{
	return dlsym(RTLD_NEXT, name);
}

static void marshal_passwd(const struct passwd* source,
                           struct darwin_passwd* destination)
{
	memset(destination, 0, sizeof(*destination));
	destination->pw_name = source->pw_name;
	destination->pw_passwd = source->pw_passwd;
	destination->pw_uid = source->pw_uid;
	destination->pw_gid = source->pw_gid;
	destination->pw_gecos = source->pw_gecos;
	destination->pw_dir = source->pw_dir;
	destination->pw_shell = source->pw_shell;
}

struct darwin_passwd* getpwuid(uint32_t uid)
{
	static host_getpwuid_function host_getpwuid;
	static struct darwin_passwd result;
	struct passwd* host_result;

	if (!host_getpwuid)
		host_getpwuid = (host_getpwuid_function)next_symbol("getpwuid");
	if (!host_getpwuid) {
		errno = ENOSYS;
		return NULL;
	}

	host_result = host_getpwuid(uid);
	if (!host_result)
		return NULL;
	marshal_passwd(host_result, &result);
	return &result;
}

struct darwin_passwd* getpwnam(const char* name)
{
	static host_getpwnam_function host_getpwnam;
	static struct darwin_passwd result;
	struct passwd* host_result;

	if (!host_getpwnam)
		host_getpwnam = (host_getpwnam_function)next_symbol("getpwnam");
	if (!host_getpwnam) {
		errno = ENOSYS;
		return NULL;
	}

	host_result = host_getpwnam(name);
	if (!host_result)
		return NULL;
	marshal_passwd(host_result, &result);
	return &result;
}

static int copy_reentrant_string(char* source, char** cursor, size_t* remaining,
	                         char** destination)
{
	if (!source) {
		*destination = NULL;
		return 0;
	}

	size_t length = strlen(source) + 1;
	if (length > *remaining)
		return ERANGE;
	memcpy(*cursor, source, length);
	*destination = *cursor;
	*cursor += length;
	*remaining -= length;
	return 0;
}

static int marshal_reentrant_passwd(const struct passwd* source,
                                    struct darwin_passwd* destination,
                                    char* buffer, size_t buffer_size)
{
	char* cursor = buffer;
	size_t remaining = buffer_size;
	int result;

	memset(destination, 0, sizeof(*destination));
	destination->pw_uid = source->pw_uid;
	destination->pw_gid = source->pw_gid;

	result = copy_reentrant_string(source->pw_name, &cursor, &remaining,
	                               &destination->pw_name);
	if (result)
		return result;
	result = copy_reentrant_string(source->pw_passwd, &cursor, &remaining,
	                               &destination->pw_passwd);
	if (result)
		return result;
	result = copy_reentrant_string(source->pw_gecos, &cursor, &remaining,
	                               &destination->pw_gecos);
	if (result)
		return result;
	result = copy_reentrant_string(source->pw_dir, &cursor, &remaining,
	                               &destination->pw_dir);
	if (result)
		return result;
	return copy_reentrant_string(source->pw_shell, &cursor, &remaining,
	                             &destination->pw_shell);
}

int getpwuid_r(uint32_t uid, struct darwin_passwd* pwd, char* buffer,
               size_t buffer_size, struct darwin_passwd** result)
{
	static host_getpwuid_r_function host_getpwuid_r;
	struct passwd host_pwd;
	struct passwd* host_result = NULL;
	int host_status;

	if (!pwd || !result)
		return EINVAL;
	*result = NULL;
	if (!host_getpwuid_r)
		host_getpwuid_r =
			(host_getpwuid_r_function)next_symbol("getpwuid_r");
	if (!host_getpwuid_r)
		return ENOSYS;

	host_status = host_getpwuid_r(uid, &host_pwd, buffer, buffer_size,
	                              &host_result);
	if (host_status || !host_result)
		return host_status;

	host_status = marshal_reentrant_passwd(host_result, pwd, buffer,
	                                       buffer_size);
	if (!host_status)
		*result = pwd;
	return host_status;
}

int getpwnam_r(const char* name, struct darwin_passwd* pwd, char* buffer,
               size_t buffer_size, struct darwin_passwd** result)
{
	static host_getpwnam_r_function host_getpwnam_r;
	struct passwd host_pwd;
	struct passwd* host_result = NULL;
	int host_status;

	if (!name || !pwd || !result)
		return EINVAL;
	*result = NULL;
	if (!host_getpwnam_r)
		host_getpwnam_r =
			(host_getpwnam_r_function)next_symbol("getpwnam_r");
	if (!host_getpwnam_r)
		return ENOSYS;

	host_status = host_getpwnam_r(name, &host_pwd, buffer, buffer_size,
	                              &host_result);
	if (host_status || !host_result)
		return host_status;

	host_status = marshal_reentrant_passwd(host_result, pwd, buffer,
	                                       buffer_size);
	if (!host_status)
		*result = pwd;
	return host_status;
}
