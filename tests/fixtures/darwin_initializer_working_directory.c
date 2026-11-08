static long raw_open(const char* path, unsigned long flags, unsigned long mode)
{
	register const char* argument0 __asm__("x0") = path;
	register unsigned long argument1 __asm__("x1") = flags;
	register unsigned long argument2 __asm__("x2") = mode;
	register unsigned long result __asm__("x0");
	register unsigned long syscall_number __asm__("x16") = 5;
	__asm__ volatile("svc #0x80"
	                 : "=r"(result)
	                 : "r"(argument0), "r"(argument1), "r"(argument2),
	                   "r"(syscall_number)
	                 : "cc", "memory");
	return (long)result;
}

__attribute__((constructor)) static void create_initializer_file(void)
{
	raw_open("initializer-output.txt", 0x201, 0600);
}

int main(void)
{
	register unsigned long status __asm__("x0") = 0;
	register unsigned long syscall_number __asm__("x16") = 1;
	__asm__ volatile("svc #0x80"
	                 :
	                 : "r"(status), "r"(syscall_number)
	                 : "memory");
	__builtin_unreachable();
}
