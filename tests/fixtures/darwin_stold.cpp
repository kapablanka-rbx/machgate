extern "C" void string_init(void*, const char*, unsigned long)
    __asm__("__ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6__initEPKcm");
extern "C" void string_destroy(void*)
    __asm__("__ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEED1Ev");
extern "C" long double string_stold(const void*, unsigned long*)
    __asm__("__ZNSt3__15stoldERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEEPm");
extern "C" void wide_string_init(void*, const int*, unsigned long)
    __asm__("__ZNSt3__112basic_stringIwNS_11char_traitsIwEENS_9allocatorIwEEE6__initEPKwm");
extern "C" void wide_string_destroy(void*)
    __asm__("__ZNSt3__112basic_stringIwNS_11char_traitsIwEENS_9allocatorIwEEED1Ev");
extern "C" long double wide_string_stold(const void*, unsigned long*)
    __asm__("__ZNSt3__15stoldERKNS_12basic_stringIwNS_11char_traitsIwEENS_9allocatorIwEEEEPm");

static int check_narrow(const char* text, unsigned long length,
                        long double expected, unsigned long consumed,
                        bool throws)
{
    alignas(16) unsigned char storage[64] = {};
    string_init(storage, text, length);
    unsigned long index = 777;
    int result = 0;
    try {
        long double value = string_stold(storage, &index);
        result = !throws && value == expected && index == consumed;
    } catch (...) {
        result = throws && index == 777;
    }
    string_destroy(storage);
    return result;
}

int main()
{
    static_assert(sizeof(long double) == sizeof(double), "Darwin ARM64 floating ABI");
    if (!check_narrow("12", 2, 12.0L, 2, false))
        return 1;
    if (!check_narrow(" -12.5tail", 10, -12.5L, 6, false))
        return 2;
    if (!check_narrow("1e309", 5, 0, 0, true))
        return 3;
    if (!check_narrow("invalid", 7, 0, 0, true))
        return 4;
    alignas(16) unsigned char storage[64] = {};
    const int text[] = {'2', '6', '.', '5', 'x', 0};
    wide_string_init(storage, text, 5);
    unsigned long index = 777;
    long double value = wide_string_stold(storage, &index);
    wide_string_destroy(storage);
    if (value != 26.5L || index != 4)
        return 5;
    return 0;
}
