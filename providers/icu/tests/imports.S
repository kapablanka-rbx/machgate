.text
.macro icu_import name
.globl _\name
_\name:
    ret
.endm
#define MACHGATE_ICU(name, result, parameters, bridge) icu_import name
#include "../exports.def"
#undef MACHGATE_ICU
.globl dyld_stub_binder
dyld_stub_binder:
    ret
