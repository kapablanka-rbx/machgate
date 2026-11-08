.section __TEXT,__text
.p2align 2
.globl _pthread_key_create
_pthread_key_create:
    ret

.globl _pthread_setspecific
_pthread_setspecific:
    ret

.globl _pthread_getspecific
_pthread_getspecific:
    ret

.globl __tlv_bootstrap
__tlv_bootstrap:
    ret

.globl dyld_stub_binder
dyld_stub_binder:
    ret

.globl _pthread_key_delete
_pthread_key_delete:
    ret
