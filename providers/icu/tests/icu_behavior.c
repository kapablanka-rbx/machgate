#include "icu_contract.h"

static int equal_units(const UChar* left, const UChar* right, int length)
{
    for (int index = 0; index < length; index++) {
        if (left[index] != right[index])
            return 0;
    }
    return 1;
}

static int unicode_case_mapping(void)
{
    if (u_tolower(0x3a3) != 0x3c3 || u_toupper(0xe9) != 0xc9)
        return 11;
    if (u_charType(0x4e2d) != 5 || u_charDirection(0x5d0) != 1)
        return 12;
    if (!u_hasBinaryProperty(0x4e2d, 0))
        return 13;
    const UChar german[] = {'s', 't', 'r', 'a', 0xdf, 'e'};
    const UChar upper[] = {'S', 'T', 'R', 'A', 'S', 'S', 'E'};
    UChar output[32] = {0};
    UErrorCode status = 0;
    int result = u_strToUpper(output, 32, german, 6, "de", &status);
    if (status > 0 || result != 7 || !equal_units(output, upper, 7))
        return 14;
    status = 0;
    result = u_strToUpper((void*)0, 0, german, 6, "de", &status);
    if (status != 15 || result != 7)
        return 15;
    const UChar turkish[] = {'I', 0x130};
    const UChar lower[] = {0x131, 'i'};
    status = 0;
    result = u_strToLower(output, 32, turkish, 2, "tr", &status);
    return status > 0 || result != 2 || !equal_units(output, lower, 2) ? 16 : 0;
}

static int canonical_normalization(void)
{
    UErrorCode status = 0;
    const void* composed = unorm2_getNFCInstance(&status);
    const void* decomposed = unorm2_getNFDInstance(&status);
    const UChar input[] = {'e', 0x301};
    const UChar expected[] = {0xe9};
    UChar output[8] = {0};
    if (status > 0 || !composed || !decomposed)
        return 21;
    int result = unorm2_normalize(composed, input, 2, output, 8, &status);
    if (status > 0 || result != 1 || !equal_units(output, expected, 1))
        return 22;
    if (!unorm2_isNormalized(composed, output, 1, &status) ||
        unorm2_isNormalized(composed, input, 2, &status))
        return 23;
    result = unorm2_normalize(decomposed, expected, 1, output, 8, &status);
    return status > 0 || result != 2 || !equal_units(output, input, 2) ? 24 : 0;
}

static int international_domain_name(void)
{
    UErrorCode status = 0;
    void* idna = uidna_openUTS46(0, &status);
    const UChar input[] = {'b', 0xfc, 'c', 'h', 'e', 'r', '.', 'd', 'e'};
    const UChar expected[] = {'x', 'n', '-', '-', 'b', 'c', 'h', 'e', 'r', '-', 'k', 'v', 'a', '.', 'd', 'e'};
    UIDNAInfo information = {sizeof(UIDNAInfo), 0, 0, 0, 0, 0};
    UChar output[64] = {0};
    if (!idna || status > 0)
        return 31;
    int result = uidna_nameToASCII(idna, input, 9, output, 64, &information, &status);
    if (status > 0 || information.errors || result != 16 || !equal_units(output, expected, 16))
        return 32;
    result = uidna_nameToUnicode(idna, expected, 16, output, 64, &information, &status);
    uidna_close(idna);
    return status > 0 || information.errors || result != 9 || !equal_units(output, input, 9) ? 33 : 0;
}

static int grapheme_boundaries(void)
{
    UErrorCode status = 0;
    const UChar input[] = {'a', 0x301, 0xd83d, 0xde00, 'b'};
    void* iterator = ubrk_open(0, "en", input, 5, &status);
    if (status > 0 || !iterator)
        return 41;
    if (ubrk_first(iterator) != 0 || ubrk_next(iterator) != 2 ||
        ubrk_next(iterator) != 4 || ubrk_next(iterator) != 5 || ubrk_next(iterator) != -1)
        return 42;
    int result = !ubrk_isBoundary(iterator, 1) && ubrk_isBoundary(iterator, 2);
    ubrk_close(iterator);
    return result ? 0 : 43;
}

static int localized_number(void)
{
    const UChar skeleton[] = {'p', 'r', 'e', 'c', 'i', 's', 'i', 'o', 'n', '-', 'i', 'n', 't', 'e', 'g', 'e', 'r'};
    const UChar expected[] = {'1', ',', '2', '3', '5'};
    UErrorCode status = 0;
    void* formatter = unumf_openForSkeletonAndLocale(skeleton, 17, "en_US", &status);
    void* formatted = unumf_openResult(&status);
    if (status > 0 || !formatter || !formatted)
        return 51;
    unumf_formatDouble(formatter, 1234.6, formatted, &status);
    UChar output[64] = {0};
    int result = unumf_resultToString(formatted, output, 64, &status);
    unumf_closeResult(formatted);
    unumf_close(formatter);
    return status > 0 || result != 5 || !equal_units(output, expected, 5) ? 52 : 0;
}

static int patterned_date(void)
{
    UErrorCode status = 0;
    const UChar timezone[] = {'U', 'T', 'C'};
    const UChar pattern[] = {'y', 'y', 'y', 'y', '-', 'M', 'M', '-', 'd', 'd'};
    const UChar expected[] = {'1', '9', '7', '0', '-', '0', '1', '-', '0', '1'};
    void* formatter = udat_open(-2, -2, "en_US_POSIX", timezone, 3, pattern, 10, &status);
    if (status > 0 || !formatter)
        return 61;
    UChar output[32] = {0};
    int result = udat_format(formatter, 0.0, output, 32, (void*)0, &status);
    udat_close(formatter);
    return status > 0 || result != 10 || !equal_units(output, expected, 10) ? 62 : 0;
}

int icu_run_behavior(void)
{
    int (*checks[])(void) = {unicode_case_mapping, canonical_normalization,
        international_domain_name, grapheme_boundaries, localized_number, patterned_date};
    for (unsigned int index = 0; index < sizeof(checks) / sizeof(checks[0]); index++) {
        int result = checks[index]();
        if (result)
            return result;
    }
    unsigned char version[4] = {0};
    u_getVersion(version);
    return version[0] == 74 ? 0 : 71;
}
