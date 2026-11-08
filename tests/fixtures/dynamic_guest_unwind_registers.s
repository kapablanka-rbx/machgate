.section __TEXT,__text,regular,pure_instructions

.globl _walk_frame_saved
.p2align 2
_walk_frame_saved:
	.cfi_startproc
	stp x29, x30, [sp, #-16]!
	mov x29, sp
	.cfi_def_cfa w29, 16
	.cfi_offset w30, -8
	.cfi_offset w29, -16
	stp x20, x19, [sp, #-16]!
	.cfi_offset w19, -24
	.cfi_offset w20, -32
	stp d9, d8, [sp, #-16]!
	.cfi_offset d8, -40
	.cfi_offset d9, -48
	blr x0
	ldp d9, d8, [sp], #16
	ldp x20, x19, [sp], #16
	ldp x29, x30, [sp], #16
	ret
	.cfi_endproc
.globl _walk_frame_saved_end
_walk_frame_saved_end:

.globl _walk_frameless_saved
.p2align 2
_walk_frameless_saved:
	.cfi_startproc
	sub sp, sp, #32
	.cfi_def_cfa_offset 32
	stp x20, x19, [sp, #16]
	.cfi_offset w19, -8
	.cfi_offset w20, -16
	stp d9, d8, [sp]
	.cfi_offset d8, -24
	.cfi_offset d9, -32
	mov w0, #42
	ldp d9, d8, [sp]
	ldp x20, x19, [sp, #16]
	add sp, sp, #32
	ret
	.cfi_endproc
.globl _walk_frameless_saved_end
_walk_frameless_saved_end:

.globl _walk_no_unwind
.p2align 2
_walk_no_unwind:
	ret
