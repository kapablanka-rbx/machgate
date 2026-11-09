typedef unsigned long size_t;
extern int sysctlbyname(const char*, void*, size_t*, const void*, size_t);
int main(void)
{
	size_t size = 0;
	if (sysctlbyname("machdep.cpu.brand_string", 0, &size, 0, 0) || size < 2 || size > 128)
		return 1;
	char output[129];
	for (size_t index = 0; index < sizeof(output); index++) output[index] = '!';
	size_t length = size;
	if (sysctlbyname("machdep.cpu.brand_string", output, &length, 0, 0) || length != size)
		return 2;
	if (output[0] == 0 || output[size - 1] != 0 || output[size] != '!')
		return 3;
	length = 1;
	if (sysctlbyname("machdep.cpu.brand_string", output, &length, 0, 0) != -1)
		return 4;
	if (sysctlbyname("machdep.cpu.brand_string", 0, &length, output, size) != -1)
		return 5;
	return 0;
}
