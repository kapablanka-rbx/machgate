.text
.p2align 2

.macro FILESYSTEM_ENTRY public_name, implementation
.global \public_name
.type \public_name, %function
.hidden \implementation
\public_name:
	.cfi_startproc
	stp x18, x30, [sp, #-16]!
	.cfi_def_cfa_offset 16
	.cfi_offset 18, -16
	.cfi_offset 30, -8
	bl \implementation
	ldp x18, x30, [sp], #16
	.cfi_restore 18
	.cfi_restore 30
	.cfi_def_cfa_offset 0
	ret
	.cfi_endproc
.size \public_name, .-\public_name
.endm

FILESYSTEM_ENTRY statfs, machgate_darwin_statfs_compat
FILESYSTEM_ENTRY fstatfs, machgate_darwin_fstatfs_compat

.global quick_exit
.type quick_exit, %function
.hidden machgate_host_quick_exit
quick_exit:
	.cfi_startproc
	stp x0, x18, [sp, #-32]!
	.cfi_def_cfa_offset 32
	.cfi_offset 18, -24
	stp x29, x30, [sp, #16]
	.cfi_offset 29, -16
	.cfi_offset 30, -8
	bl machgate_host_quick_exit
	mov x16, x0
	ldp x29, x30, [sp, #16]
	.cfi_restore 29
	.cfi_restore 30
	ldp x0, x18, [sp], #32
	.cfi_restore 18
	.cfi_def_cfa_offset 0
	br x16
	.cfi_endproc
.size quick_exit, .-quick_exit

.section .note.GNU-stack,"",%progbits
