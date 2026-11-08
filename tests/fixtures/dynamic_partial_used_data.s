.section __TEXT,__text
.globl _main
.p2align 2
_main:
    adrp x9, _machgate_absent_data@GOTPAGE
    ldr x9, [x9, _machgate_absent_data@GOTPAGEOFF]
    ldr x0, [x9]
    cbnz x0, failed
    ldr x0, [x9, #8]
    cbnz x0, failed
    adrp x10, _machgate_optional_data@GOTPAGE
    ldr x10, [x10, _machgate_optional_data@GOTPAGEOFF]
    cmp x9, x10
    b.eq failed
    ldr x0, [x10]
    cbnz x0, failed
    mov x11, #0x1234
    str x11, [x9]
    mov x12, #0x5678
    str x12, [x10]
    ldr x0, [x9]
    cmp x0, x11
    b.ne failed
    ldr x0, [x10]
    cmp x0, x12
    b.ne failed
    mov x0, #0
    b exit
failed:
    mov x0, #95
exit:
    mov x16, #1
    svc #0x80
