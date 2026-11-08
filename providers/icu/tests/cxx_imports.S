.text
.macro function_import name
.globl _\name
_\name:
    ret
.endm
function_import __cxa_allocate_exception
function_import __cxa_throw
function_import __cxa_begin_catch
function_import __cxa_end_catch
function_import __gxx_personality_v0
function_import _Unwind_Resume
function_import _ZNSt9exceptionD2Ev
function_import _ZNKSt9exception4whatEv
function_import _ZdlPv
.globl dyld_stub_binder
dyld_stub_binder:
    ret

.data
.p2align 3
.globl __ZTVN10__cxxabiv117__class_type_infoE
__ZTVN10__cxxabiv117__class_type_infoE:
    .space 128
.globl __ZTVN10__cxxabiv120__si_class_type_infoE
__ZTVN10__cxxabiv120__si_class_type_infoE:
    .space 128
.globl __ZTISt9exception
__ZTISt9exception:
    .space 128
