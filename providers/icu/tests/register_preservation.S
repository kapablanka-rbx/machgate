#if defined(__APPLE__)
#define C_SYMBOL(name) _##name
#else
#define C_SYMBOL(name) name
#endif

.text
.p2align 2
.globl C_SYMBOL(main)
C_SYMBOL(main):
    stp x29, x30, [sp, #-32]!
    stp x18, x19, [sp, #16]
    mov x29, sp
    mov x19, #0x1357
    movk x19, #0x2468, lsl #16
    movk x19, #0x369c, lsl #32
    movk x19, #0x48ad, lsl #48
    mov x18, x19
    bl C_SYMBOL(icu_run_behavior)
    cmp x18, x19
    mov w1, #72
    csel w0, w0, w1, eq
    ldp x18, x19, [sp, #16]
    ldp x29, x30, [sp], #32
    ret

#if !defined(__APPLE__)
.section .note.GNU-stack,"",%progbits
#endif
