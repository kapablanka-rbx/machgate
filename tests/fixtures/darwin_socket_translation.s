.section __TEXT,__text
.globl _main
.p2align 2
_main:
    mov x0, #1
    mov x1, #1
    mov x2, #0
    adrp x3, socket_pair@PAGE
    add x3, x3, socket_pair@PAGEOFF
    mov x16, #135
    svc #0x80
    b.cs fail_135
    adrp x9, socket_pair@PAGE
    add x9, x9, socket_pair@PAGEOFF
    ldr w19, [x9]
    ldr w20, [x9, #4]
    adrp x10, socket_pair_value@PAGE
    add x10, x10, socket_pair_value@PAGEOFF
    str w19, [x10]

    mov x0, x19
    adrp x1, message@PAGE
    add x1, x1, message@PAGEOFF
    mov x2, #1
    movz x3, #0x80
    movz x4, #8, lsl #16
    orr x3, x3, x4
    mov x4, #0
    mov x5, #0
    mov x16, #133
    svc #0x80
    b.cs fail_133
    cmp x0, #1
    b.ne fail_133

    mov x0, x20
    adrp x1, receive_buffer@PAGE
    add x1, x1, receive_buffer@PAGEOFF
    mov x2, #1
    mov x3, #0xc2
    mov x4, #0
    mov x5, #0
    mov x16, #29
    svc #0x80
    b.cs fail_29
    cmp x0, #1
    b.ne fail_29
    adrp x9, receive_buffer@PAGE
    add x9, x9, receive_buffer@PAGEOFF
    ldrb w10, [x9]
    cmp w10, #'s'
    b.ne fail_29

    mov x0, x20
    adrp x1, receive_buffer@PAGE
    add x1, x1, receive_buffer@PAGEOFF
    mov x2, #1
    mov x3, #0x80
    mov x4, #0
    mov x5, #0
    mov x16, #403
    svc #0x80
    b.cs fail_29
    cmp x0, #1
    b.ne fail_29

    mov x0, x19
    adrp x1, send_message@PAGE
    add x1, x1, send_message@PAGEOFF
    movz x2, #8, lsl #16
    mov x16, #28
    svc #0x80
    b.cs fail_28
    cmp x0, #1
    b.ne fail_28

    mov x0, x20
    adrp x1, receive_message@PAGE
    add x1, x1, receive_message@PAGEOFF
    mov x2, #0x80
    mov x16, #401
    svc #0x80
    b.cs fail_401
    cmp x0, #1
    b.ne fail_401
    adrp x9, receive_control@PAGE
    add x9, x9, receive_control@PAGEOFF
    ldr w21, [x9, #12]
    mov x0, x21
    mov x1, #1
    mov x2, #0
    mov x16, #42
    svc #0x80
    b.cs fail_401
    mov x0, x21
    mov x16, #6
    svc #0x80
    b.cs fail_401

    mov x0, x20
    mov x16, #6
    svc #0x80
    b.cs fail_6
    mov x0, x19
    adrp x1, send_message@PAGE
    add x1, x1, send_message@PAGEOFF
    movz x2, #8, lsl #16
    mov x16, #402
    svc #0x80
    b.cc fail_402
    cmp x0, #32
    b.ne fail_402

    mov x0, #1
    mov x1, #1
    mov x2, #0
    adrp x3, socket_pair@PAGE
    add x3, x3, socket_pair@PAGEOFF
    mov x16, #135
    svc #0x80
    b.cs fail_135
    adrp x9, socket_pair@PAGE
    add x9, x9, socket_pair@PAGEOFF
    ldr w19, [x9]
    ldr w20, [x9, #4]
    mov x0, x20
    mov x16, #6
    svc #0x80
    b.cs fail_6
    mov x0, x19
    mov x16, #6
    svc #0x80
    b.cs fail_6

    mov x0, #1
    mov x1, #1
    mov x2, #0
    adrp x3, socket_pair@PAGE
    add x3, x3, socket_pair@PAGEOFF
    mov x16, #135
    svc #0x80
    b.cs fail_135
    adrp x9, socket_pair@PAGE
    add x9, x9, socket_pair@PAGEOFF
    ldr w19, [x9]
    ldr w20, [x9, #4]
    mov x0, x20
    mov x16, #6
    svc #0x80
    b.cs fail_6
    mov x0, x19
    adrp x1, message@PAGE
    add x1, x1, message@PAGEOFF
    mov x2, #1
    movz x3, #8, lsl #16
    mov x4, #0
    mov x5, #0
    mov x16, #413
    svc #0x80
    b.cc fail_413
    cmp x0, #32
    b.ne fail_413
    mov x0, x19
    mov x16, #6
    svc #0x80
    b.cs fail_6

    mov x0, #2
    mov x1, #1
    mov x2, #6
    mov x16, #97
    svc #0x80
    b.cs fail_97
    mov x19, x0
    mov x0, x19
    mov x1, #6
    mov x2, #0x10
    adrp x3, option_value@PAGE
    add x3, x3, option_value@PAGEOFF
    mov x4, #4
    mov x16, #105
    svc #0x80
    b.cs fail_105
    mov x0, x19
    mov x1, #6
    mov x2, #0x10
    adrp x3, option_result@PAGE
    add x3, x3, option_result@PAGEOFF
    adrp x4, option_length@PAGE
    add x4, x4, option_length@PAGEOFF
    mov x16, #118
    svc #0x80
    b.cs fail_118
    adrp x9, option_result@PAGE
    add x9, x9, option_result@PAGEOFF
    ldr w10, [x9]
    cmp w10, #30
    b.ne fail_118
    mov x0, x19
    mov x16, #6
    svc #0x80
    b.cs fail_6

    mov x0, #2
    mov x1, #2
    mov x2, #0
    mov x16, #97
    svc #0x80
    b.cs fail_97
    mov x19, x0
    mov x0, x19
    mov x1, #0
    mov x2, #26
    adrp x3, option_one@PAGE
    add x3, x3, option_one@PAGEOFF
    mov x4, #4
    mov x16, #105
    svc #0x80
    b.cs fail_105
    mov x0, x19
    mov x1, #0
    mov x2, #28
    adrp x3, option_one@PAGE
    add x3, x3, option_one@PAGEOFF
    mov x4, #4
    mov x16, #105
    svc #0x80
    b.cs fail_105
    mov x0, x19
    mov x16, #6
    svc #0x80
    b.cs fail_6

    mov x0, #30
    mov x1, #2
    mov x2, #0
    mov x16, #97
    svc #0x80
    b.cs fail_97
    mov x19, x0
    mov x0, x19
    mov x1, #41
    mov x2, #27
    adrp x3, option_one@PAGE
    add x3, x3, option_one@PAGEOFF
    mov x4, #4
    mov x16, #105
    svc #0x80
    b.cs fail_105
    mov x0, x19
    mov x1, #41
    mov x2, #61
    adrp x3, option_one@PAGE
    add x3, x3, option_one@PAGEOFF
    mov x4, #4
    mov x16, #105
    svc #0x80
    b.cs fail_105
    mov x0, x19
    mov x16, #6
    svc #0x80
    b.cs fail_6

    mov x0, #1
    adrp x1, success_message@PAGE
    add x1, x1, success_message@PAGEOFF
    mov x2, #18
    mov x16, #4
    svc #0x80
    b.cs fail_4
    mov x0, #0
    mov x16, #1
    svc #0x80

fail_4:
    mov x0, #4
    b fail
fail_6:
    mov x0, #6
    b fail
fail_28:
    mov x0, #28
    b fail
fail_29:
    mov x0, #29
    b fail
fail_97:
    mov x0, #97
    b fail
fail_105:
    mov x0, #105
    b fail
fail_118:
    mov x0, #118
    b fail
fail_133:
    mov x0, #133
    b fail
fail_135:
    mov x0, #135
    b fail
fail_401:
    mov x0, #201
    b fail
fail_402:
    mov x0, #202
    b fail
fail_413:
    mov x0, #203
fail:
    mov x16, #1
    svc #0x80

.section __TEXT,__cstring
message:
    .asciz "s"
success_message:
    .asciz "socket-translation"

.section __DATA,__data
.p2align 3
socket_pair:
    .space 8
receive_buffer:
    .space 1
option_value:
    .long 30
option_one:
    .long 1
option_result:
    .long 0
option_length:
    .long 4
send_iovec:
    .quad message
    .quad 1
receive_iovec:
    .quad receive_buffer
    .quad 1
send_control:
    .long 16
    .long 1
    .long 1
socket_pair_value:
    .long 0
send_message:
    .quad 0
    .long 0
    .long 0
    .quad send_iovec
    .long 1
    .long 0
    .quad send_control
    .long 16
    .long 0
    .long 0
    .long 0
receive_message:
    .quad 0
    .long 0
    .long 0
    .quad receive_iovec
    .long 1
    .long 0
    .quad receive_control
    .long 16
    .long 0
    .long 0
    .long 0
receive_control:
    .space 16
