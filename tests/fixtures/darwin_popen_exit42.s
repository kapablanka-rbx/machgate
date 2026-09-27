; darwin_popen_exit42.s - popen() of a Mach-O guest re-execs through the loader.
; The guest builds a command line referencing darwin_exit42 (a Mach-O fixture)
; and calls popen + pclose through imported libSystem symbols. pclose must
; report the child's exit status 42.

.section __TEXT,__text
.globl _main
.p2align 2
_main:
	stp x29, x30, [sp, #-16]!
	mov x29, sp

	; popen(command, "r")
	adrp x0, command@PAGE
	add x0, x0, command@PAGEOFF
	adrp x1, read_mode@PAGE
	add x1, x1, read_mode@PAGEOFF
	bl _popen
	cbnz x0, have_file
	mov w0, #98
	b exit_path

have_file:
	; pclose(file)
	mov x19, x0
	mov x0, x19
	bl _pclose
	lsr w0, w0, #8
	and w0, w0, #0xff
	cmp w0, #42
	b.eq exit_path
	mov w0, #99
	b exit_path

exit_path:
	; exit(w0)
	uxtb w0, w0
	mov x0, x0
	mov x16, #1
	svc #0x80

.section __TEXT,__cstring
command:
	.asciz "./darwin_exit42"
read_mode:
	.asciz "r"

.section __DATA,__data
.p2align 3
