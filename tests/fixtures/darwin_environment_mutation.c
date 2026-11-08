extern char* getenv(const char*);
extern int setenv(const char*, const char*, int);
extern int unsetenv(const char*);
extern int putenv(char*);
extern int strcmp(const char*, const char*);
extern char*** _NSGetEnviron(void);

int main(void)
{
	const char* name = "MACHGATE_ENV_FIXTURE";
	char* value = getenv(name);
	if (!value || strcmp(value, "initial"))
		return 1;
	if (setenv(name, "replacement", 1) || !(value = getenv(name)) || strcmp(value, "replacement"))
		return 2;
	if (setenv(name, "ignored", 0) || strcmp(getenv(name), "replacement"))
		return 3;
	if (unsetenv(name) || getenv(name))
		return 4;
	char assignment[] = "MACHGATE_ENV_FIXTURE=borrowed";
	if (putenv(assignment) || !(value = getenv(name)) || strcmp(value, "borrowed"))
		return 5;
	assignment[21] = 'B';
	if (getenv(name) != assignment + 21)
		return 6;
	for (char** entry = *_NSGetEnviron(); entry && *entry; entry++)
		if (*entry == assignment)
			return unsetenv(name) || getenv(name) != 0;
	return 7;
}
