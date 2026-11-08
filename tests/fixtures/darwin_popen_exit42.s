.section __TEXT,__text
.globl _main
.p2align 2
_main:
	stp x29, x30, [sp, #-32]!
	stp x19, x20, [sp, #16]
	mov x29, sp

	adrp x0, command@PAGE
	add x0, x0, command@PAGEOFF
	adrp x1, read_mode@PAGE
	add x1, x1, read_mode@PAGEOFF
	bl _popen
	cbnz x0, have_file
	mov w0, #98
	b exit_path

have_file:
	mov x19, x0
	mov x0, x19
	bl _pclose
	lsr w0, w0, #8
	and w0, w0, #0xff
	cmp w0, #42
	b.eq exit_path
	mov w0, #99

exit_path:
	uxtb w0, w0
	mov x16, #1
	svc #0x80

.section __TEXT,__cstring
command:
	.asciz "./darwin_exit42 2>&1"
read_mode:
	.asciz "r"
