#include <stddef.h>
#include <unicode/uchar.h>
#include <unicode/ustring.h>
#include <unicode/uversion.h>
#include <unicode/ubrk.h>
#include <unicode/ucal.h>
#include <unicode/ucol.h>
#include <unicode/ucurr.h>
#include <unicode/udat.h>
#include <unicode/udatpg.h>
#include <unicode/udateintervalformat.h>
#include <unicode/uenum.h>
#include <unicode/ufieldpositer.h>
#include <unicode/uformattedvalue.h>
#include <unicode/uidna.h>
#include <unicode/uldnames.h>
#include <unicode/ulistformatter.h>
#include <unicode/uloc.h>
#include <unicode/unorm2.h>
#include <unicode/unum.h>
#include <unicode/unumberformatter.h>
#include <unicode/unumberrangeformatter.h>
#include <unicode/unumsys.h>
#include <unicode/upluralrules.h>
#include <unicode/ureldatefmt.h>
#include <unicode/ures.h>
#include <unicode/utext.h>

_Static_assert(U_ICU_VERSION_MAJOR_NUM == 74, "ICU provider requires the reviewed ICU 74 ABI");
_Static_assert(sizeof(void*) == 8, "ICU provider requires 64-bit pointers");
_Static_assert(sizeof(UChar) == 2, "UChar must be a UTF-16 code unit");
_Static_assert(sizeof(UChar32) == 4, "UChar32 must be 32 bits");
_Static_assert(sizeof(UErrorCode) == 4, "UErrorCode must be 32 bits");
_Static_assert(sizeof(UBool) == 1, "UBool must be a signed byte");
_Static_assert((UBool)-1 < 0, "UBool must be signed");
_Static_assert(sizeof(UIDNAInfo) == 16, "UIDNAInfo must match the guest layout");
_Static_assert(offsetof(UIDNAInfo, errors) == 4, "UIDNAInfo errors must match the guest layout");
_Static_assert(sizeof(UParseError) == 72, "UParseError must match the guest layout");
_Static_assert(sizeof(UFieldPosition) == 12, "UFieldPosition must match the guest layout");

#define MACHGATE_ICU(name, result, parameters, bridge) \
    _Static_assert(__builtin_types_compatible_p(__typeof__(&name), result (*) parameters), \
        "ICU prototype changed: " #name);
#include "exports.def"
#undef MACHGATE_ICU
