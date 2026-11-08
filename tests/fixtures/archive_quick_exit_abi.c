#include <dlfcn.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

static void verify_callback_register(void)
{
	uintptr_t result;
	__asm__ volatile("mov %0, x18" : "=r"(result));
	char output = result == 0x5a17 ? '1' : '0';
	syscall(SYS_write, STDOUT_FILENO, &output, 1);
}

int main(int argument_count, char** arguments)
{
	if (argument_count != 2)
		return 1;
	void* library = dlopen(arguments[1], RTLD_NOW | RTLD_LOCAL);
	if (!library)
		return 2;
	void (*terminate)(int) = dlsym(library, "quick_exit");
	if (!terminate || at_quick_exit(verify_callback_register) != 0)
		return 3;
	__asm__ volatile("mov x18, #0x5a17" ::: "x18");
	terminate(42);
	return 4;
}
