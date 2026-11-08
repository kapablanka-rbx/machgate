#include <stddef.h>
#include <stdint.h>

typedef struct {
	const char* dli_fname;
	void* dli_fbase;
	const char* dli_sname;
	void* dli_saddr;
} Dl_info;

extern int dladdr(const void*, Dl_info*);
extern uint32_t _dyld_image_count(void);
extern const void* _dyld_get_image_header(uint32_t);
extern const char* _dyld_get_image_name(uint32_t);
extern intptr_t _dyld_get_image_vmaddr_slide(uint32_t);
extern char** backtrace_symbols(void* const*, int);
extern void backtrace_symbols_fd(void* const*, int, int);
extern int pipe(int*);
extern int close(int);
extern long read(int, void*, unsigned long);
extern void free(void*);
extern int strcmp(const char*, const char*);
extern char* strstr(const char*, const char*);
extern int dependency_value(void);

__attribute__((noinline)) int fixture_symbol(int value)
{
	return value + dependency_value();
}

int main(void)
{
	_Static_assert(sizeof(Dl_info) == 32, "Darwin arm64 Dl_info layout");
	Dl_info info;
	void* address = (void*)((uintptr_t)fixture_symbol + 4);
	if (!dladdr(address, &info) || !info.dli_fname || !info.dli_fbase || !info.dli_sname)
		return 1;
	if (strcmp(info.dli_sname, "fixture_symbol") || info.dli_saddr != (void*)fixture_symbol)
		return 2;
	if (_dyld_image_count() != 2 || _dyld_get_image_header(0) != info.dli_fbase ||
	    strcmp(_dyld_get_image_name(0), info.dli_fname))
		return 3;
	if (*(const uint32_t*)info.dli_fbase != 0xfeedfacf || _dyld_get_image_vmaddr_slide(0) != 0)
		return 4;
	if (!dladdr(info.dli_fbase, &info) || strcmp(info.dli_sname, "__dso_handle"))
		return 5;
	if (!dladdr((void*)dependency_value, &info) || !info.dli_sname ||
	    strcmp(info.dli_sname, "dependency_value") || info.dli_fbase != _dyld_get_image_header(1))
		return 6;
	if (strcmp(info.dli_fname, _dyld_get_image_name(1)) || !strstr(info.dli_fname, "dependency.dylib"))
		return 7;
	if (_dyld_get_image_header(2) || _dyld_get_image_name(2) || _dyld_get_image_vmaddr_slide(2))
		return 8;
	if (dladdr(NULL, &info) || dladdr(address, NULL))
		return 9;
	void* frames[] = {address, (void*)dependency_value, NULL};
	char** symbols = backtrace_symbols(frames, 3);
	if (!symbols || !strstr(symbols[0], " fixture_symbol + 4") ||
	    !strstr(symbols[1], " dependency_value + 0") || !strstr(symbols[2], "0x0 + 0"))
		return 10;
	free(symbols);
	int descriptors[2];
	if (pipe(descriptors))
		return 11;
	backtrace_symbols_fd(frames, 3, descriptors[1]);
	close(descriptors[1]);
	char output[1024];
	long length = read(descriptors[0], output, sizeof(output) - 1);
	close(descriptors[0]);
	if (length <= 0)
		return 12;
	output[length] = 0;
	if (!strstr(output, " fixture_symbol + 4\n") || !strstr(output, " dependency_value + 0\n"))
		return 13;
	return fixture_symbol(3) != 8;
}
