.section __TEXT,__text,regular,pure_instructions
.globl _main
.p2align 2
_main:
	sub sp, sp, #2208
	mov x18, #0x5a17
	adrp x0, filesystem_path@PAGE
	add x0, x0, filesystem_path@PAGEOFF
	mov x1, sp
	bl _statfs
	cbnz w0, failure
	mov x9, #0x5a17
	cmp x18, x9
	b.ne failure
	ldr w9, [sp]
	cbz w9, failure

	adrp x0, filesystem_path@PAGE
	add x0, x0, filesystem_path@PAGEOFF
	mov x1, #0
	mov x2, #0
	mov x16, #5
	svc #0x80
	b.cs failure
	mov x19, x0
	mov x1, sp
	bl _fstatfs
	cbnz w0, failure
	mov x9, #0x5a17
	cmp x18, x9
	b.ne failure
	mov x0, x19
	mov x16, #6
	svc #0x80
	b.cs failure

	mov x9, #-1
	str x9, [sp, #2176]
	add x0, sp, #2176
	bl _pthread_cpu_number_np
	cbnz w0, failure
	ldr x9, [sp, #2176]
	lsr x9, x9, #32
	cbnz x9, failure
	mov x9, #0x5a17
	cmp x18, x9
	b.ne failure

	mov x9, #0x100000000
	str x9, [sp, #2184]
	mov x0, #7
	add x1, sp, #2184
	bl _OSAtomicAdd64
	mov x9, #0x100000000
	add x9, x9, #7
	cmp x0, x9
	b.ne failure
	ldr x10, [sp, #2184]
	cmp x10, x9
	b.ne failure
	mov x9, #0x5a17
	cmp x18, x9
	b.ne failure
	mov x0, #0
	bl __Exit

failure:
	mov x0, #61
	mov x16, #1
	svc #0x80

.section __TEXT,__cstring,cstring_literals
filesystem_path:
	.asciz "/"
