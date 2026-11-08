.section __TEXT,__text
.globl _dlsym
.globl _dlopen
.globl _dlclose
.globl _mach_absolute_time
.globl _pthread_key_create
.globl _getrlimit
.globl dyld_stub_binder
.p2align 2
_dlsym:
_dlopen:
_dlclose:
_mach_absolute_time:
_pthread_key_create:
_getrlimit:
dyld_stub_binder:
    ret
