typedef unsigned long size_t;

extern int memset_s(void* destination, size_t capacity, int value, size_t length);
extern int* __error(void);

static unsigned char storage[34];

static void reset_storage(void)
{
	for (size_t index = 0; index < sizeof(storage); index++)
		storage[index] = 0xa5;
}

static int storage_matches(size_t length, unsigned char value)
{
	if (storage[0] != 0xa5 || storage[33] != 0xa5)
		return 0;
	for (size_t index = 0; index < 32; index++) {
		unsigned char expected = index < length ? value : 0xa5;
		if (storage[index + 1] != expected)
			return 0;
	}
	return 1;
}

int main(void)
{
	const size_t maximum = (~(size_t)0) >> 1;
	reset_storage();
	*__error() = 123;
	if (memset_s(storage + 1, 32, 0, 32) != 0 || !storage_matches(32, 0))
		return 1;
	if (*__error() != 123)
		return 2;

	reset_storage();
	if (memset_s(storage + 1, 32, 0x17b, 7) != 0 || !storage_matches(7, 0x7b))
		return 3;
	reset_storage();
	if (memset_s(storage + 1, 8, 0, 12) != 84 || !storage_matches(8, 0))
		return 4;
	reset_storage();
	if (memset_s(storage + 1, 8, 0, maximum + 1) != 7 || !storage_matches(8, 0))
		return 5;
	reset_storage();
	if (memset_s(storage + 1, maximum + 1, 0, 1) != 7 || !storage_matches(0, 0))
		return 6;
	if (memset_s(0, 0, 0, 0) != 22 || memset_s(0, maximum + 1, 0, 1) != 22)
		return 7;
	if (memset_s(storage + 1, 0, 0, 0) != 0 || !storage_matches(0, 0))
		return 8;
	if (memset_s(storage + 1, 0, 0, 1) != 84 || !storage_matches(0, 0))
		return 9;
	return *__error() == 123 ? 0 : 10;
}
