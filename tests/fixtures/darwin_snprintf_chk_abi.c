extern int __snprintf_chk(char* buffer, unsigned long buffer_size, int flag,
	unsigned long destination_size, const char* format, ...);

static int string_equal(const char* actual, const char* expected)
{
	while (*actual && *actual == *expected) {
		++actual;
		++expected;
	}
	return *actual == *expected;
}

int main(void)
{
	char buffer[128];
	int result = __snprintf_chk(buffer, sizeof(buffer), 0, sizeof(buffer),
		"%s:%d:%.2f:%lld:%s", "checked", 37, 2.25, 9000000001LL, "tail");
	if (result != sizeof("checked:37:2.25:9000000001:tail") - 1 ||
	    !string_equal(buffer, "checked:37:2.25:9000000001:tail"))
		return 41;
	result = __snprintf_chk(buffer, 5, 0, sizeof(buffer), "%s:%d", "truncate", 17);
	if (result != 11 || !string_equal(buffer, "trun"))
		return 42;
	result = __snprintf_chk((void*)0, 0, 0, 0, "%s:%d", "length", 17);
	if (result != 9)
		return 43;
	return 0;
}
