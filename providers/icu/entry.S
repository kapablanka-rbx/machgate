.text

.macro icu_entry name, extend_signed_byte
.p2align 2
.globl \name
.type \name, %function
\name:
    .cfi_startproc
    stp x29, x30, [sp, #-32]!
    .cfi_def_cfa_offset 32
    .cfi_offset x29, -32
    .cfi_offset x30, -24
    str x18, [sp, #16]
    .cfi_offset x18, -16
    mov x29, sp
    adrp x16, machgate_icu_\name
    ldr x16, [x16, :lo12:machgate_icu_\name]
    blr x16
.if \extend_signed_byte
    sxtb w0, w0
.endif
    ldr x18, [sp, #16]
    ldp x29, x30, [sp], #32
    .cfi_def_cfa_offset 0
    .cfi_restore x18
    .cfi_restore x29
    .cfi_restore x30
    ret
    .cfi_endproc
.size \name, .-\name
.endm

#define ICU_EXTEND_direct 0
#define ICU_EXTEND_signed_byte 1
#define MACHGATE_ICU(name, result, parameters, bridge) icu_entry name, ICU_EXTEND_##bridge
#include "exports.def"
#undef MACHGATE_ICU

.section .note.GNU-stack,"",%progbits
