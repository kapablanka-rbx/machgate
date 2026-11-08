typedef __builtin_va_list darwin_va_list;

extern void* stdout;
extern int fflush(void* stream);
extern void free(void* pointer);
extern void* fmemopen(void* buffer, unsigned long size, const char* mode);
extern int printf(const char* format, ...);
extern int fprintf(void* stream, const char* format, ...);
extern int sprintf(char* buffer, const char* format, ...);
extern int sscanf(const char* input, const char* format, ...);
extern int fscanf(void* stream, const char* format, ...);
extern int snprintf(char* buffer, unsigned long buffer_size,
	            const char* format, ...);
extern int __sprintf_chk(char* buffer, int flag, unsigned long destination_size,
	                 const char* format, ...);
extern int vfprintf(void* stream, const char* format, darwin_va_list arguments);
extern int vsnprintf(char* buffer, unsigned long buffer_size,
	             const char* format, darwin_va_list arguments);
extern int vsprintf(char* buffer, const char* format, darwin_va_list arguments);
extern int vprintf(const char* format, darwin_va_list arguments);
extern int vsscanf(const char* input, const char* format,
	           darwin_va_list arguments);
extern int vfscanf(void* stream, const char* format, darwin_va_list arguments);
extern int vasprintf(char** output, const char* format,
	             darwin_va_list arguments);
extern int __vsnprintf_chk(char* buffer, unsigned long buffer_size, int flag,
	                   unsigned long destination_size, const char* format,
	                   darwin_va_list arguments);
extern int __vsprintf_chk(char* buffer, int flag,
	                  unsigned long destination_size, const char* format,
	                  darwin_va_list arguments);

static int print_with_darwin_list(const char* format, ...)
{
	darwin_va_list arguments;
	__builtin_va_start(arguments, format);
	int result = vfprintf(stdout, format, arguments);
	__builtin_va_end(arguments);
	return result;
}

static int format_with_darwin_list(char* buffer, unsigned long buffer_size,
	                           const char* format, ...)
{
	darwin_va_list arguments;
	__builtin_va_start(arguments, format);
	int result = vsnprintf(buffer, buffer_size, format, arguments);
	__builtin_va_end(arguments);
	return result;
}

static int sprintf_with_darwin_list(char* buffer, const char* format, ...)
{
	darwin_va_list arguments;
	__builtin_va_start(arguments, format);
	int result = vsprintf(buffer, format, arguments);
	__builtin_va_end(arguments);
	return result;
}

static int vprintf_with_darwin_list(const char* format, ...)
{
	darwin_va_list arguments;
	__builtin_va_start(arguments, format);
	int result = vprintf(format, arguments);
	__builtin_va_end(arguments);
	return result;
}

static int vsscanf_with_darwin_list(const char* input, const char* format, ...)
{
	darwin_va_list arguments;
	__builtin_va_start(arguments, format);
	int result = vsscanf(input, format, arguments);
	__builtin_va_end(arguments);
	return result;
}

static int vfscanf_with_darwin_list(void* stream, const char* format, ...)
{
	darwin_va_list arguments;
	__builtin_va_start(arguments, format);
	int result = vfscanf(stream, format, arguments);
	__builtin_va_end(arguments);
	return result;
}

static int vasprintf_with_darwin_list(char** output, const char* format, ...)
{
	darwin_va_list arguments;
	__builtin_va_start(arguments, format);
	int result = vasprintf(output, format, arguments);
	__builtin_va_end(arguments);
	return result;
}

static int vsnprintf_chk_with_darwin_list(
	char* buffer, unsigned long buffer_size, unsigned long destination_size,
	const char* format, ...)
{
	darwin_va_list arguments;
	__builtin_va_start(arguments, format);
	int result = __vsnprintf_chk(buffer, buffer_size, 1, destination_size,
	                            format, arguments);
	__builtin_va_end(arguments);
	return result;
}

static int vsprintf_chk_with_darwin_list(
	char* buffer, unsigned long destination_size, const char* format, ...)
{
	darwin_va_list arguments;
	__builtin_va_start(arguments, format);
	int result = __vsprintf_chk(buffer, 1, destination_size, format, arguments);
	__builtin_va_end(arguments);
	return result;
}

static unsigned long string_length(const char* string)
{
	unsigned long result = 0;
	while (string[result])
		result++;
	return result;
}

static int string_equal(const char* left, const char* right)
{
	while (*left && *left == *right) {
		left++;
		right++;
	}
	return *left == *right;
}

static int check_string_result(const char* actual, const char* expected,
	                       int result)
{
	return string_equal(actual, expected) &&
	       result == (int)string_length(expected);
}

static void exit_guest(int status)
{
	register unsigned long result __asm__("x0") = (unsigned long)status;
	register unsigned long number __asm__("x16") = 1;
	__asm__ volatile("svc #0x80" : : "r"(result), "r"(number) : "memory");
	__builtin_unreachable();
}

int main(void)
{
	char buffer[256];
	char direct_buffer[256];
	char first_word[32];
	char second_word[32];
	char* allocated = 0;
	int first_integer = 0;
	int second_integer = 0;
	double first_real = 0;
	double second_real = 0;

	int fprintf_result = fprintf(
		stdout, "fprintf:%d:%s:%.1f:%d:%s:%.1f\n",
		11, "stream", 1.25, 12, "stack", 2.75);
	if (fprintf_result != 35)
		exit_guest(73);
	int printf_result = printf("printf:%d:%s:%.1f\n", 7, "clean", 3.5);
	int snprintf_result = snprintf(
		direct_buffer, sizeof(direct_buffer),
		"snprintf:%d:%s:%.1f", 8, "direct", 0.5);
	int vfprintf_result = print_with_darwin_list(
		"vfprintf:%lld:%s:%.2f\n", 9000000001LL, "room", 2.25);
	int vsnprintf_result = format_with_darwin_list(
		buffer, sizeof(buffer), "vsnprintf:%u:%s:%.1f", 42, "safe", 1.5);
	if (!check_string_result(direct_buffer, "snprintf:8:direct:0.5",
	                         snprintf_result) ||
	    !check_string_result(buffer, "vsnprintf:42:safe:1.5",
	                         vsnprintf_result) ||
	    printf_result != 19 || vfprintf_result != 30)
		exit_guest(71);
	if (printf("%s\n%s\n", direct_buffer, buffer) != 44)
		exit_guest(72);

	int sprintf_result = sprintf(
		direct_buffer, "sprintf:%d:%s:%.1f:%d:%s:%.1f",
		17, "stack", 2.5, 18, "tail", 3.5);
	if (!check_string_result(direct_buffer, "sprintf:17:stack:2.5:18:tail:3.5",
	                         sprintf_result))
		exit_guest(74);
	if (printf("%s\n", direct_buffer) != sprintf_result + 1)
		exit_guest(75);

	int sscanf_result = sscanf(
		"31 direct 4.25 32 tail 5.75", "%d %31s %lf %d %31s %lf",
		&first_integer, first_word, &first_real,
		&second_integer, second_word, &second_real);
	if (sscanf_result != 6 || first_integer != 31 || second_integer != 32 ||
	    first_real != 4.25 || second_real != 5.75 ||
	    !string_equal(first_word, "direct") || !string_equal(second_word, "tail"))
		exit_guest(76);
	if (printf("sscanf:%d:%s:%.2f:%d:%s:%.2f\n",
	           first_integer, first_word, first_real,
	           second_integer, second_word, second_real) != 35)
		exit_guest(77);

	char fscanf_input[] = "41 stream 6.25 42 file 7.75";
	void* stream = fmemopen(fscanf_input, sizeof(fscanf_input) - 1, "r");
	if (!stream)
		exit_guest(78);
	first_integer = second_integer = 0;
	first_real = second_real = 0;
	int fscanf_result = fscanf(
		stream, "%d %31s %lf %d %31s %lf",
		&first_integer, first_word, &first_real,
		&second_integer, second_word, &second_real);
	if (fscanf_result != 6 || first_integer != 41 || second_integer != 42 ||
	    first_real != 6.25 || second_real != 7.75 ||
	    !string_equal(first_word, "stream") || !string_equal(second_word, "file"))
		exit_guest(79);
	if (printf("fscanf:%d:%s:%.2f:%d:%s:%.2f\n",
	           first_integer, first_word, first_real,
	           second_integer, second_word, second_real) != 35)
		exit_guest(80);

	int sprintf_chk_result = __sprintf_chk(
		direct_buffer, 1, sizeof(direct_buffer),
		"sprintf_chk:%d:%s:%.1f:%d:%s:%.1f",
		51, "checked", 8.5, 52, "stack", 9.5);
	if (!check_string_result(
		direct_buffer, "sprintf_chk:51:checked:8.5:52:stack:9.5",
		sprintf_chk_result))
		exit_guest(81);
	if (printf("%s\n", direct_buffer) != sprintf_chk_result + 1)
		exit_guest(82);

	int vsprintf_result = sprintf_with_darwin_list(
		direct_buffer, "vsprintf:%d:%s:%.1f:%d:%s:%.1f",
		61, "list", 1.5, 62, "stack", 2.5);
	if (!check_string_result(direct_buffer, "vsprintf:61:list:1.5:62:stack:2.5",
	                         vsprintf_result))
		exit_guest(83);
	if (printf("%s\n", direct_buffer) != vsprintf_result + 1)
		exit_guest(84);

	int vprintf_result = vprintf_with_darwin_list(
		"vprintf:%d:%s:%.1f:%d:%s:%.1f\n",
		71, "list", 3.5, 72, "stack", 4.5);
	if (vprintf_result != 33)
		exit_guest(85);

	int vsscanf_result = vsscanf_with_darwin_list(
		"81 list 9.25 82 stack 10.75", "%d %31s %lf %d %31s %lf",
		&first_integer, first_word, &first_real,
		&second_integer, second_word, &second_real);
	if (vsscanf_result != 6 || first_integer != 81 || second_integer != 82 ||
	    first_real != 9.25 || second_real != 10.75 ||
	    !string_equal(first_word, "list") || !string_equal(second_word, "stack"))
		exit_guest(86);
	if (printf("vsscanf:%d:%s:%.2f:%d:%s:%.2f\n",
	           first_integer, first_word, first_real,
	           second_integer, second_word, second_real) != 36)
		exit_guest(87);

	char vfscanf_input[] = "91 list 11.25 92 stack 12.75";
	stream = fmemopen(vfscanf_input, sizeof(vfscanf_input) - 1, "r");
	if (!stream)
		exit_guest(88);
	int vfscanf_result = vfscanf_with_darwin_list(
		stream, "%d %31s %lf %d %31s %lf",
		&first_integer, first_word, &first_real,
		&second_integer, second_word, &second_real);
	if (vfscanf_result != 6 || first_integer != 91 || second_integer != 92 ||
	    first_real != 11.25 || second_real != 12.75 ||
	    !string_equal(first_word, "list") || !string_equal(second_word, "stack"))
		exit_guest(89);
	if (printf("vfscanf:%d:%s:%.2f:%d:%s:%.2f\n",
	           first_integer, first_word, first_real,
	           second_integer, second_word, second_real) != 37)
		exit_guest(90);

	int vasprintf_result = vasprintf_with_darwin_list(
		&allocated, "vasprintf:%d:%s:%.1f:%d:%s:%.1f",
		101, "allocated", 13.5, 102, "stack", 14.5);
	if (!allocated || !check_string_result(
		allocated, "vasprintf:101:allocated:13.5:102:stack:14.5",
		vasprintf_result))
		exit_guest(91);
	if (printf("%s\n", allocated) != vasprintf_result + 1)
		exit_guest(92);
	free(allocated);

	int vsnprintf_chk_result = vsnprintf_chk_with_darwin_list(
		direct_buffer, sizeof(direct_buffer), sizeof(direct_buffer),
		"vsnprintf_chk:%d:%s:%.1f:%d:%s:%.1f",
		111, "checked", 15.5, 112, "stack", 16.5);
	if (!check_string_result(
		direct_buffer, "vsnprintf_chk:111:checked:15.5:112:stack:16.5",
		vsnprintf_chk_result))
		exit_guest(93);
	if (printf("%s\n", direct_buffer) != vsnprintf_chk_result + 1)
		exit_guest(94);

	int vsprintf_chk_result = vsprintf_chk_with_darwin_list(
		direct_buffer, sizeof(direct_buffer),
		"vsprintf_chk:%d:%s:%.1f:%d:%s:%.1f",
		121, "checked", 17.5, 122, "stack", 18.5);
	if (!check_string_result(
		direct_buffer, "vsprintf_chk:121:checked:17.5:122:stack:18.5",
		vsprintf_chk_result))
		exit_guest(95);
	if (printf("%s\n", direct_buffer) != vsprintf_chk_result + 1)
		exit_guest(96);

	if (fflush(stdout) != 0)
		exit_guest(97);
	exit_guest(0);
}
