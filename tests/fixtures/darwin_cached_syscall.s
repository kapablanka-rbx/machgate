.section __TEXT,__text
.globl _machgate_override_probe
.p2align 2
_machgate_override_probe:
    svc #0x80
    ret

.globl _main
.p2align 2
_main:
    mov x0, #-1
    mov x16, #6
    bl _machgate_override_probe
    mov x16, #1
    svc #0x80
