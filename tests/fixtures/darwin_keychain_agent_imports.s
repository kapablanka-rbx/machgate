.text
.globl _printf
_printf:
 ret
.globl _free
_free:
 ret
.globl _SecItemAdd
_SecItemAdd:
 ret
.globl _SecItemCopyMatching
_SecItemCopyMatching:
 ret
.globl _SecItemUpdate
_SecItemUpdate:
 ret
.globl _SecItemDelete
_SecItemDelete:
 ret
.globl _CFDictionaryCreate
_CFDictionaryCreate:
 ret
.globl _CFDataCreate
_CFDataCreate:
 ret
.globl _CFDataGetBytePtr
_CFDataGetBytePtr:
 ret
.globl _CFDataGetLength
_CFDataGetLength:
 ret
.globl _CFRelease
_CFRelease:
 ret
.globl dyld_stub_binder
dyld_stub_binder:
 ret

.data
.globl _kCFAllocatorDefault
_kCFAllocatorDefault:
 .quad 0
.globl _kCFBooleanTrue
_kCFBooleanTrue:
 .quad 0
.globl _kCFTypeDictionaryKeyCallBacks
_kCFTypeDictionaryKeyCallBacks:
 .zero 48
.globl _kCFTypeDictionaryValueCallBacks
_kCFTypeDictionaryValueCallBacks:
 .zero 40
.globl _kSecClass
_kSecClass:
 .quad 0
.globl _kSecClassGenericPassword
_kSecClassGenericPassword:
 .quad 0
.globl _kSecAttrAccount
_kSecAttrAccount:
 .quad 0
.globl _kSecAttrService
_kSecAttrService:
 .quad 0
.globl _kSecAttrAccessible
_kSecAttrAccessible:
 .quad 0
.globl _kSecAttrAccessibleAfterFirstUnlock
_kSecAttrAccessibleAfterFirstUnlock:
 .quad 0
.globl _kSecValueData
_kSecValueData:
 .quad 0
.globl _kSecReturnData
_kSecReturnData:
 .quad 0
