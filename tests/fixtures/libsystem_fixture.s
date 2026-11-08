.section __TEXT,__text
.globl _mach_absolute_time
.p2align 2
_mach_absolute_time:
    mov x0, #1
    ret

.globl _printf
.p2align 2
_printf:
    ret

.globl _fprintf
.p2align 2
_fprintf:
    ret

.globl _sprintf
.p2align 2
_sprintf:
    ret

.globl _sscanf
.p2align 2
_sscanf:
    ret

.globl _fscanf
.p2align 2
_fscanf:
    ret

.globl _snprintf
.p2align 2
_snprintf:
    ret

.globl ___sprintf_chk
.p2align 2
___sprintf_chk:
    ret

.globl _fcntl
.p2align 2
_fcntl:
    ret

.globl _open
.p2align 2
_open:
    ret

.globl _openat
.p2align 2
_openat:
    ret

.globl _ioctl
.p2align 2
_ioctl:
    ret

.globl _pipe
.p2align 2
_pipe:
    ret

.globl _close
.p2align 2
_close:
    ret

.globl _socket
.p2align 2
_socket:
    ret

.globl _socketpair
.p2align 2
_socketpair:
    ret

.globl _bind
.p2align 2
_bind:
    ret

.globl _connect
.p2align 2
_connect:
    ret

.globl _listen
.p2align 2
_listen:
    ret

.globl _accept
.p2align 2
_accept:
    ret

.globl _send
.p2align 2
_send:
    ret

.globl _recv
.p2align 2
_recv:
    ret

.globl _sendto
.p2align 2
_sendto:
    ret

.globl _recvfrom
.p2align 2
_recvfrom:
    ret

.globl _sendmsg
.p2align 2
_sendmsg:
    ret

.globl _recvmsg
.p2align 2
_recvmsg:
    ret

.globl _sendmsg_x
.p2align 2
_sendmsg_x:
    ret

.globl _recvmsg_x
.p2align 2
_recvmsg_x:
    ret

.globl _setsockopt
.p2align 2
_setsockopt:
    ret

.globl _getsockopt
.p2align 2
_getsockopt:
    ret

.globl _getsockname
.p2align 2
_getsockname:
    ret

.globl _inet_pton
.p2align 2
_inet_pton:
    ret

.globl _inet_ntop
.p2align 2
_inet_ntop:
    ret

.globl ___error
.p2align 2
___error:
    ret

.globl _memset
.p2align 2
_memset:
    ret

.globl _vfprintf
.p2align 2
_vfprintf:
    ret

.globl _vsnprintf
.p2align 2
_vsnprintf:
    ret

.globl _vsprintf
.p2align 2
_vsprintf:
    ret

.globl _vprintf
.p2align 2
_vprintf:
    ret

.globl _vsscanf
.p2align 2
_vsscanf:
    ret

.globl _vfscanf
.p2align 2
_vfscanf:
    ret

.globl _vasprintf
.p2align 2
_vasprintf:
    ret

.globl ___vsnprintf_chk
.p2align 2
___vsnprintf_chk:
    ret

.globl ___vsprintf_chk
.p2align 2
___vsprintf_chk:
    ret

.globl _fflush
.p2align 2
_fflush:
    ret

.globl _free
.p2align 2
_free:
    ret

.globl _fmemopen
.p2align 2
_fmemopen:
    ret

.globl _getrlimit
.p2align 2
_getrlimit:
    ret

.globl _setrlimit
.p2align 2
_setrlimit:
    ret

.globl _task_policy_set
.p2align 2
_task_policy_set:
    ret

.globl __Unwind_Backtrace
.p2align 2
__Unwind_Backtrace:
    ret

.globl __Unwind_GetIP
.p2align 2
__Unwind_GetIP:
	ret

.globl _pthread_key_create
.p2align 2
_pthread_key_create:
    ret

.globl _pthread_setspecific
.p2align 2
_pthread_setspecific:
    ret

.globl __tlv_atexit
.p2align 2
__tlv_atexit:
    ret

.globl _popen
.p2align 2
_popen:
    ret

.globl _pclose
.p2align 2
_pclose:
    ret

.globl dyld_stub_binder
.p2align 2
dyld_stub_binder:
    ret

.section __DATA,__data
.globl _stdout
.p2align 3
_stdout:
    .quad 0

.globl _mach_task_self_
.p2align 2
_mach_task_self_:
    .long 0
