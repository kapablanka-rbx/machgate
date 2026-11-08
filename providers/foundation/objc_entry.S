.text
.p2align 2
.global objc_msgSend
.type objc_msgSend, %function
objc_msgSend:
    mov x16, #0
    b .Lentry
.size objc_msgSend, .-objc_msgSend
.global objc_msgSendSuper2
.type objc_msgSendSuper2, %function
objc_msgSendSuper2:
    mov x16, #1
.Lentry:
    sub sp, sp, #256
    stp x29, x30, [sp, #240]
    add x29, sp, #240
    stp x0, x1, [sp]
    stp x2, x3, [sp, #16]
    stp x4, x5, [sp, #32]
    stp x6, x7, [sp, #48]
    add x9, sp, #256
    stp x8, x9, [sp, #64]
    stp q0, q1, [sp, #80]
    stp q2, q3, [sp, #112]
    stp q4, q5, [sp, #144]
    stp q6, q7, [sp, #176]
    str x18, [sp, #232]
    mov x0, sp
    cbnz x16, .Lsuper
    bl foundation_dispatch
    b .Lreturn
.Lsuper:
    bl foundation_dispatch_super
.Lreturn:
    ldr x18, [sp, #232]
    ldr x16, [sp, #224]
    cbnz x16, .Lfallback
    ldr x0, [sp, #208]
    ldr d0, [sp, #216]
    ldp x29, x30, [sp, #240]
    add sp, sp, #256
    ret
.Lfallback:
    ldp x0, x1, [sp]
    ldp x2, x3, [sp, #16]
    ldp x4, x5, [sp, #32]
    ldp x6, x7, [sp, #48]
    ldr x8, [sp, #64]
    ldp q0, q1, [sp, #80]
    ldp q2, q3, [sp, #112]
    ldp q4, q5, [sp, #144]
    ldp q6, q7, [sp, #176]
    ldp x29, x30, [sp, #240]
    add sp, sp, #256
    br x16
.size objc_msgSendSuper2, .-objc_msgSendSuper2
.section .note.GNU-stack,"",%progbits
