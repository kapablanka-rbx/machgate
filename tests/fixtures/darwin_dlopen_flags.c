extern void* dlopen(const char* path, int flags);
extern void* dlsym(void* handle, const char* name);
extern int dlclose(void* handle);

int main(int argc, char** argv)
{
	if (argc != 2)
		return 2;
	void* handle = dlopen(argv[1], 0x2 | 0x4);
	if (!handle)
		return 10;
	int (*marker)(void) = dlsym(handle, "machgate_dlopen_marker");
	if (!marker || marker() != 71)
		return 11;
	if (dlsym((void*)-2L, "machgate_dlopen_marker"))
		return 12;
	if (dlclose(handle) != 0)
		return 13;
	handle = dlopen(argv[1], 0x1 | 0x8);
	if (!handle)
		return 20;
	marker = dlsym((void*)-2L, "machgate_dlopen_marker");
	if (!marker || marker() != 71)
		return 21;
	return dlclose(handle) != 0 ? 22 : 0;
}
