extern char* strerror(int number);
extern int strerror_r(int number, char* output, unsigned long capacity);
extern int* __error(void);

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
	*__error() = 71;
	if (strerror_r(49, buffer, sizeof(buffer)) != 0 ||
	    !string_equal(buffer, "Can't assign requested address") || *__error() != 71)
		return 41;
	if (!string_equal(strerror(35), "Resource temporarily unavailable") ||
	    !string_equal(strerror(60), "Operation timed out") ||
	    !string_equal(strerror(0), "Undefined error: 0"))
		return 42;
	if (strerror_r(49, buffer, 6) != 34 || !string_equal(buffer, "Can't"))
		return 43;
	if (strerror_r(999, buffer, sizeof(buffer)) != 22 ||
	    !string_equal(buffer, "Unknown error: 999") || *__error() != 71)
		return 44;
	if (!string_equal(strerror(-1), "Unknown error: -1") || *__error() != 22)
		return 45;
	if (strerror_r(49, (void*)0, 0) != 34)
		return 46;
	return 0;
}
