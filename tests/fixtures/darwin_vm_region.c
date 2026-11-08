typedef unsigned long address_t;
typedef unsigned int count_t;

#pragma pack(push, 4)
struct basic_info {
	int protection;
	int maximum_protection;
	int inheritance;
	int shared;
	int reserved;
	unsigned long offset;
	int behavior;
	unsigned short wired_count;
};
#pragma pack(pop)

_Static_assert(sizeof(struct basic_info) == 36, "XNU basic info size");
_Static_assert(__builtin_offsetof(struct basic_info, offset) == 20, "XNU offset field");

extern unsigned int mach_task_self_;
#define mach_task_self() mach_task_self_
extern int vm_region_64(unsigned int, address_t*, address_t*, int, void*, count_t*, unsigned int*);
extern int mach_vm_region(unsigned int, address_t*, address_t*, int, void*, count_t*, unsigned int*);
extern void* mmap(void*, unsigned long, int, int, int, long);
extern int mprotect(void*, unsigned long, int);
extern int munmap(void*, unsigned long);

int main(void)
{
	char* mapping = mmap((void*)0, 49152, 3, 0x1002, -1, 0);
	if (mapping == (void*)-1)
		return 41;
	if (mprotect(mapping + 16384, 16384, 0) != 0)
		return 42;
	struct {
		struct basic_info info;
		unsigned int sentinel;
	} output = {{0}, 0x12345678};
	address_t address = (address_t)mapping + 16385;
	address_t size = 0;
	count_t count = 9;
	unsigned int object = 123;
	if (vm_region_64(mach_task_self(), &address, &size, 9, &output.info, &count, &object) ||
	    address != (address_t)mapping + 16384 || size != 16384 ||
	    output.info.protection != 0 || (output.info.maximum_protection & 3) != 3 ||
	    output.info.inheritance != 1 || output.info.shared || object || count != 9 ||
	    output.sentinel != 0x12345678)
		return 43;
	if (mprotect(mapping + 16384, 16384, 1) != 0)
		return 44;
	address = (address_t)mapping + 16385;
	if (mach_vm_region(mach_task_self(), &address, &size, 9, &output.info, &count, &object) ||
	    output.info.protection != 1)
		return 45;
	if (munmap(mapping + 16384, 16384))
		return 46;
	address = (address_t)mapping + 16384;
	if (vm_region_64(mach_task_self(), &address, &size, 9, &output.info, &count, &object) ||
	    address != (address_t)mapping + 32768 || output.info.protection != 3)
		return 47;
	count = 8;
	if (vm_region_64(mach_task_self(), &address, &size, 9, &output.info, &count, &object) != 4)
		return 48;
	count = 9;
	address = ~0UL;
	if (vm_region_64(mach_task_self(), &address, &size, 9, &output.info, &count, &object) != 1)
		return 49;
	munmap(mapping, 16384);
	munmap(mapping + 32768, 16384);
	return 0;
}
