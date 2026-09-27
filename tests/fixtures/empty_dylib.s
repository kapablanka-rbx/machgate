; empty_dylib.s - stub libSystem.dylib exporting popen/pclose for fixture linking
.section __TEXT,__text
.globl _popen
.p2align 2
_popen:
	ret

.globl _pclose
.p2align 2
_pclose:
	ret
