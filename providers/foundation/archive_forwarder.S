.text
.p2align 2
.global shim_objc_forwarder
.type shim_objc_forwarder, %function
shim_objc_forwarder:
    cbz x0, .Lempty
    sub sp, sp, #160
    stp x29, x30, [sp, #128]
    str x18, [sp, #144]
    add x29, sp, #128
    mov x16, x0
    ldr x9, [x16, #8]
    cmp x9, #16
    b.hi .Linvalid
    ldr x10, [x16, #88]
    mov x11, #0
.Lcopy:
    cmp x11, x9
    b.eq .Linvoke
    cbz x10, .Linvalid
    ldr x12, [x10, x11, lsl #3]
    str x12, [sp, x11, lsl #3]
    add x11, x11, #1
    b .Lcopy
.Linvoke:
    ldp x0, x1, [x16, #16]
    ldr x8, [x16, #32]
    ldp x2, x3, [x16, #40]
    ldp x4, x5, [x16, #56]
    ldp x6, x7, [x16, #72]
    ldr x16, [x16]
    cbz x16, .Linvalid
    blr x16
    b .Ldone
.Linvalid:
    mov x0, #0
.Ldone:
    ldr x18, [sp, #144]
    ldp x29, x30, [sp, #128]
    add sp, sp, #160
    ret
.Lempty:
    ret
.size shim_objc_forwarder, .-shim_objc_forwarder
.section .note.GNU-stack,"",%progbits
