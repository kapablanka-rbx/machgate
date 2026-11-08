.text
.p2align 2
.global printf
.type printf, %function
printf:
	mov x9, sp
	stp x18, x30, [sp, #-16]!
	mov x1, x9
	bl machgate_darwin_printf_from_stack
	ldp x18, x30, [sp], #16
	ret
.size printf, .-printf

.p2align 2
.global vfprintf
.type vfprintf, %function
vfprintf:
	stp x18, x30, [sp, #-16]!
	bl machgate_darwin_vfprintf_from_stack
	ldp x18, x30, [sp], #16
	ret
.size vfprintf, .-vfprintf

.p2align 2
.global vsnprintf
.type vsnprintf, %function
vsnprintf:
	stp x18, x30, [sp, #-16]!
	bl machgate_darwin_vsnprintf_from_stack
	ldp x18, x30, [sp], #16
	ret
.size vsnprintf, .-vsnprintf

.p2align 2
.global snprintf
.type snprintf, %function
snprintf:
	mov x9, sp
	stp x18, x30, [sp, #-16]!
	mov x3, x9
	bl machgate_darwin_snprintf_from_stack
	ldp x18, x30, [sp], #16
	ret
.size snprintf, .-snprintf

.p2align 2
.global fprintf
.type fprintf, %function
fprintf:
	mov x9, sp
	stp x18, x30, [sp, #-16]!
	mov x2, x9
	bl machgate_darwin_fprintf_from_stack
	ldp x18, x30, [sp], #16
	ret
.size fprintf, .-fprintf

.p2align 2
.global sprintf
.type sprintf, %function
sprintf:
	mov x9, sp
	stp x18, x30, [sp, #-16]!
	mov x2, x9
	bl machgate_darwin_sprintf_from_stack
	ldp x18, x30, [sp], #16
	ret
.size sprintf, .-sprintf

.p2align 2
.global sscanf
.type sscanf, %function
sscanf:
	mov x9, sp
	stp x18, x30, [sp, #-16]!
	mov x2, x9
	bl machgate_darwin_sscanf_from_stack
	ldp x18, x30, [sp], #16
	ret
.size sscanf, .-sscanf

.p2align 2
.global fscanf
.type fscanf, %function
fscanf:
	mov x9, sp
	stp x18, x30, [sp, #-16]!
	mov x2, x9
	bl machgate_darwin_fscanf_from_stack
	ldp x18, x30, [sp], #16
	ret
.size fscanf, .-fscanf

.p2align 2
.global __sprintf_chk
.type __sprintf_chk, %function
__sprintf_chk:
	mov x9, sp
	stp x18, x30, [sp, #-16]!
	mov x4, x9
	bl machgate_darwin_sprintf_chk_from_stack
	ldp x18, x30, [sp], #16
	ret
.size __sprintf_chk, .-__sprintf_chk

.p2align 2
.global machgate_darwin_snprintf_chk
.hidden machgate_darwin_snprintf_chk
.type machgate_darwin_snprintf_chk, %function
machgate_darwin_snprintf_chk:
	mov x9, sp
	stp x18, x30, [sp, #-16]!
	mov x5, x9
	bl machgate_darwin_vsnprintf_chk
	ldp x18, x30, [sp], #16
	ret
.size machgate_darwin_snprintf_chk, .-machgate_darwin_snprintf_chk

.p2align 2
.global vsprintf
.type vsprintf, %function
vsprintf:
	stp x18, x30, [sp, #-16]!
	bl machgate_darwin_vsprintf
	ldp x18, x30, [sp], #16
	ret
.size vsprintf, .-vsprintf

.p2align 2
.global vprintf
.type vprintf, %function
vprintf:
	stp x18, x30, [sp, #-16]!
	bl machgate_darwin_vprintf
	ldp x18, x30, [sp], #16
	ret
.size vprintf, .-vprintf

.p2align 2
.global vsscanf
.type vsscanf, %function
vsscanf:
	stp x18, x30, [sp, #-16]!
	bl machgate_darwin_vsscanf
	ldp x18, x30, [sp], #16
	ret
.size vsscanf, .-vsscanf

.p2align 2
.global vfscanf
.type vfscanf, %function
vfscanf:
	stp x18, x30, [sp, #-16]!
	bl machgate_darwin_vfscanf
	ldp x18, x30, [sp], #16
	ret
.size vfscanf, .-vfscanf

.p2align 2
.global vasprintf
.type vasprintf, %function
vasprintf:
	stp x18, x30, [sp, #-16]!
	bl machgate_darwin_vasprintf
	ldp x18, x30, [sp], #16
	ret
.size vasprintf, .-vasprintf

.p2align 2
.global __vsnprintf_chk
.type __vsnprintf_chk, %function
__vsnprintf_chk:
	stp x18, x30, [sp, #-16]!
	bl machgate_darwin_vsnprintf_chk
	ldp x18, x30, [sp], #16
	ret
.size __vsnprintf_chk, .-__vsnprintf_chk

.p2align 2
.global __vsprintf_chk
.type __vsprintf_chk, %function
__vsprintf_chk:
	stp x18, x30, [sp, #-16]!
	bl machgate_darwin_vsprintf_chk
	ldp x18, x30, [sp], #16
	ret
.size __vsprintf_chk, .-__vsprintf_chk

.section .note.GNU-stack,"",%progbits
