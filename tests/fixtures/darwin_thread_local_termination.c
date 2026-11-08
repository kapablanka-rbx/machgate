extern void _tlv_atexit(void (*function)(void*), void* argument);
extern int pthread_key_create(unsigned long* key, void (*destructor)(void*));
extern int pthread_setspecific(unsigned long key, const void* value);
static void print_message(void* argument)
{
	unsigned long length = 0;
	while (((const char*)argument)[length])
		length++;
	register unsigned long fd __asm__("x0") = 1;
	register const char* message __asm__("x1") = argument;
	register unsigned long size __asm__("x2") = length;
	register unsigned long syscall_number __asm__("x16") = 4;
	__asm__ volatile("svc #0x80" : : "r"(fd), "r"(message), "r"(size),
	                 "r"(syscall_number) : "memory");
	fd = 1;
	message = "\n";
	size = 1;
	__asm__ volatile("svc #0x80" : : "r"(fd), "r"(message), "r"(size),
	                 "r"(syscall_number) : "memory");
}

int main(int argument_count, char** arguments)
{
	unsigned long key;

	if (argument_count > 1 && arguments[1] && arguments[1][0] == 'x')
		return 71;
	if (pthread_key_create(&key, print_message) != 0)
		return 72;
	_tlv_atexit(print_message, "tlv first");
	if (pthread_setspecific(key, "tsd") != 0)
		return 73;
	_tlv_atexit(print_message, "tlv second");
	return 29;
}
