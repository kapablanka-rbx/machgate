#include <stddef.h>
#include <stdint.h>

extern int* __error(void);
extern int open(const char*, int, ...);
extern int close(int);
extern long getxattr(const char*, const char*, void*, size_t, uint32_t, int);
extern long fgetxattr(int, const char*, void*, size_t, uint32_t, int);
extern int setxattr(const char*, const char*, const void*, size_t, uint32_t, int);
extern int fsetxattr(int, const char*, const void*, size_t, uint32_t, int);
extern int removexattr(const char*, const char*, int);
extern int fremovexattr(int, const char*, int);
extern long listxattr(const char*, char*, size_t, int);
extern long flistxattr(int, char*, size_t, int);
extern int strcmp(const char*, const char*);

static long request(int direct, unsigned operation, uintptr_t target,
	uintptr_t name, uintptr_t value, uintptr_t size, uintptr_t position, uintptr_t options)
{
	if (!direct) {
		switch (operation) {
		case 234: return getxattr((void*)target, (void*)name, (void*)value, size, position, options);
		case 235: return fgetxattr(target, (void*)name, (void*)value, size, position, options);
		case 236: return setxattr((void*)target, (void*)name, (void*)value, size, position, options);
		case 237: return fsetxattr(target, (void*)name, (void*)value, size, position, options);
		case 238: return removexattr((void*)target, (void*)name, value);
		case 239: return fremovexattr(target, (void*)name, value);
		case 240: return listxattr((void*)target, (void*)name, value, size);
		case 241: return flistxattr(target, (void*)name, value, size);
		}
	}
	register uintptr_t argument0 __asm__("x0") = target;
	register uintptr_t argument1 __asm__("x1") = name;
	register uintptr_t argument2 __asm__("x2") = value;
	register uintptr_t argument3 __asm__("x3") = size;
	register uintptr_t argument4 __asm__("x4") = position;
	register uintptr_t argument5 __asm__("x5") = options;
	register uintptr_t number __asm__("x16") = operation;
	unsigned failed;
	__asm__ volatile("svc #0x80\n\tcset %w1, cs"
		: "+r"(argument0), "=r"(failed)
		: "r"(argument1), "r"(argument2), "r"(argument3), "r"(argument4),
		  "r"(argument5), "r"(number) : "cc", "memory");
	if (!failed)
		return argument0;
	*__error() = argument0;
	return -1;
}

int main(int count, char** arguments)
{
	if (count != 3)
		return 1;
	int direct = arguments[2][0] == '1';
	uintptr_t path = (uintptr_t)arguments[1];
	uintptr_t name = (uintptr_t)"org.example.attribute";
	uintptr_t prefixed = (uintptr_t)"user.other";
	char buffer[128];
	int descriptor = open(arguments[1], 2);
	if (descriptor < 0)
		return 2;
	for (unsigned operation = 234; operation <= 240; operation += 2) {
		if (request(direct, operation, 0, name, 0, 0, 0, 0) != -1 || *__error() != 14)
			return 21;
	}
	if (request(direct, 235, (uintptr_t)-1, name, 0, 0, 0, 0) != -1 || *__error() != 9)
		return 22;
	if (request(direct, 234, path, name, 0, 0, 0, 0) != -1 || *__error() != 93)
		return 3;
	if (request(direct, 237, descriptor, name, (uintptr_t)"value", 6, 0, 4) != -1 || *__error() != 93)
		return 4;
	if (request(direct, 236, path, name, (uintptr_t)"value", 6, 0, 2) != 0)
		return 5;
	if (request(direct, 236, path, name, (uintptr_t)"other", 6, 0, 2) != -1 || *__error() != 17)
		return 6;
	if (request(direct, 235, descriptor, name, (uintptr_t)buffer, sizeof(buffer), 0, 0) != 6 ||
	    strcmp(buffer, "value") != 0)
		return 7;
	if (request(direct, 234, path, name, 0, 100, 0, 0) != 6)
		return 8;
	if (request(direct, 237, descriptor, name, (uintptr_t)"replacement", 12, 0, 4) != 0)
		return 9;
	if (request(direct, 234, path, name, (uintptr_t)buffer, 1, 0, 0) != -1 || *__error() != 34)
		return 10;
	if (request(direct, 240, path, 0, 0, 0, 0, 0) != 22)
		return 11;
	if (request(direct, 241, descriptor, (uintptr_t)buffer, sizeof(buffer), 0, 0, 0) != 22 ||
	    strcmp(buffer, (const char*)name) != 0)
		return 12;
	if (request(direct, 240, path, (uintptr_t)buffer, 1, 0, 0, 0) != -1 || *__error() != 34)
		return 13;
	if (request(direct, 236, path, prefixed, (uintptr_t)"x", 2, 0, 0) != 0 ||
	    request(direct, 235, descriptor, prefixed, (uintptr_t)buffer, sizeof(buffer), 0, 0) != 2)
		return 14;
	if (request(direct, 239, descriptor, prefixed, 0, 0, 0, 0) != 0)
		return 15;
	if (request(direct, 235, descriptor, name, 0, 0, 0, 1) != -1 || *__error() != 22)
		return 16;
	if (request(direct, 236, path, name, 0, 0, 0, 6) != -1 || *__error() != 22)
		return 17;
	if (request(direct, 238, path, name, 0, 0, 0, 0) != 0)
		return 18;
	if (request(direct, 239, descriptor, name, 0, 0, 0, 0) != -1 || *__error() != 93)
		return 19;
	if (request(direct, 241, descriptor, 0, 0, 0, 0, 0) != 0)
		return 20;
	return close(descriptor) != 0;
}
