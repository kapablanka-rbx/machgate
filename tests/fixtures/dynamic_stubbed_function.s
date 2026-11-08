.section __TEXT,__text
.globl _main
.p2align 2
_main:
    mov x0, #57
    adrp x9, _machgate_absent_probe@GOTPAGE
    ldr x9, [x9, _machgate_absent_probe@GOTPAGEOFF]
    blr x9
    cbnz x0, failed
    mov x0, #93
    b exit
failed:
    mov x0, #94
exit:
    mov x16, #1
    svc #0x80
