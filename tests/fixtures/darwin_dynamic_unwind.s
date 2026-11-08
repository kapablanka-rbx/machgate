.section __TEXT,__text
.globl _main
.p2align 2
_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    adrp x0, dynamic_fde@PAGE
    add x0, x0, dynamic_fde@PAGEOFF
    adrp x8, ___register_frame@GOTPAGE
    ldr x8, [x8, ___register_frame@GOTPAGEOFF]
    blr x8
    adrp x0, dynamic_fde@PAGE
    add x0, x0, dynamic_fde@PAGEOFF
    adrp x8, ___deregister_frame@GOTPAGE
    ldr x8, [x8, ___deregister_frame@GOTPAGEOFF]
    blr x8
    adrp x0, dynamic_fde@PAGE
    add x0, x0, dynamic_fde@PAGEOFF
    adrp x8, ___unw_add_dynamic_fde@GOTPAGE
    ldr x8, [x8, ___unw_add_dynamic_fde@GOTPAGEOFF]
    blr x8
    adrp x0, dynamic_fde@PAGE
    add x0, x0, dynamic_fde@PAGEOFF
    adrp x8, ___unw_remove_dynamic_fde@GOTPAGE
    ldr x8, [x8, ___unw_remove_dynamic_fde@GOTPAGEOFF]
    blr x8
    mov w0, #0
    ldp x29, x30, [sp], #16
    ret

dynamic_code:
    ret
dynamic_code_end:

.section __DATA,__data
.p2align 3
dynamic_cie:
    .long dynamic_cie_end - dynamic_cie - 4
    .long 0
    .byte 1
    .asciz "zR"
    .byte 1, 0x78, 30, 1, 0
    .byte 0x0c, 31, 0
    .p2align 3
dynamic_cie_end:
dynamic_fde:
    .long dynamic_fde_end - dynamic_fde - 4
    .long . - dynamic_cie
    .quad dynamic_code
    .quad dynamic_code_end - dynamic_code
    .byte 0
    .p2align 3
dynamic_fde_end:
    .long 0
