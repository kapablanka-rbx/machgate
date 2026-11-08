.text
.globl _objc_msgSend
.globl _objc_autoreleasePoolPush
.globl _objc_autoreleasePoolPop
.globl _objc_alloc
.globl _objc_release
.globl dyld_stub_binder
_objc_msgSend:
_objc_autoreleasePoolPush:
_objc_autoreleasePoolPop:
_objc_alloc:
_objc_release:
dyld_stub_binder:
    ret
.data
.globl _OBJC_CLASS_$_NSObject
.globl _OBJC_CLASS_$_NSProcessInfo
.globl _OBJC_CLASS_$_NSString
.globl _OBJC_CLASS_$_NSMutableString
.globl ___CFConstantStringClassReference
_OBJC_CLASS_$_NSObject:
_OBJC_CLASS_$_NSProcessInfo:
_OBJC_CLASS_$_NSString:
_OBJC_CLASS_$_NSMutableString:
___CFConstantStringClassReference:
    .quad 0
