#ifndef MACHGATE_ICU_TEST_CONTRACT_H
#define MACHGATE_ICU_TEST_CONTRACT_H

typedef unsigned short UChar;
typedef int UChar32;
typedef int UErrorCode;
typedef signed char UBool;
typedef struct {
    short size;
    UBool is_transitional_different;
    UBool reserved_byte;
    unsigned int errors;
    int reserved_first;
    int reserved_second;
} UIDNAInfo;

extern void u_getVersion(unsigned char version[4]);
extern UChar32 u_tolower(UChar32 character);
extern UChar32 u_toupper(UChar32 character);
extern signed char u_charType(UChar32 character);
extern int u_charDirection(UChar32 character);
extern UBool u_hasBinaryProperty(UChar32 character, int property);
extern int u_strToUpper(UChar* output, int capacity, const UChar* input,
    int length, const char* locale, UErrorCode* status);
extern int u_strToLower(UChar* output, int capacity, const UChar* input,
    int length, const char* locale, UErrorCode* status);
extern const void* unorm2_getNFCInstance(UErrorCode* status);
extern const void* unorm2_getNFDInstance(UErrorCode* status);
extern int unorm2_normalize(const void* normalizer, const UChar* input,
    int length, UChar* output, int capacity, UErrorCode* status);
extern UBool unorm2_isNormalized(const void* normalizer, const UChar* input,
    int length, UErrorCode* status);
extern void* uidna_openUTS46(unsigned int options, UErrorCode* status);
extern int uidna_nameToASCII(const void* idna, const UChar* input, int length,
    UChar* output, int capacity, UIDNAInfo* information, UErrorCode* status);
extern int uidna_nameToUnicode(const void* idna, const UChar* input, int length,
    UChar* output, int capacity, UIDNAInfo* information, UErrorCode* status);
extern void uidna_close(void* idna);
extern void* ubrk_open(int type, const char* locale, const UChar* input,
    int length, UErrorCode* status);
extern int ubrk_first(void* iterator);
extern int ubrk_next(void* iterator);
extern UBool ubrk_isBoundary(void* iterator, int offset);
extern void ubrk_close(void* iterator);
extern void* unumf_openForSkeletonAndLocale(const UChar* skeleton, int length,
    const char* locale, UErrorCode* status);
extern void* unumf_openResult(UErrorCode* status);
extern void unumf_formatDouble(const void* formatter, double value,
    void* result, UErrorCode* status);
extern int unumf_resultToString(const void* result, UChar* output,
    int capacity, UErrorCode* status);
extern void unumf_close(void* formatter);
extern void unumf_closeResult(void* result);
extern void* udat_open(int time_style, int date_style, const char* locale,
    const UChar* timezone, int timezone_length, const UChar* pattern,
    int pattern_length, UErrorCode* status);
extern int udat_format(const void* formatter, double timestamp, UChar* output,
    int capacity, void* field_position, UErrorCode* status);
extern void udat_close(void* formatter);

#endif
