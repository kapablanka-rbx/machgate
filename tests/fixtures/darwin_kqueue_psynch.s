.section __TEXT,__text
.globl _main
_main:
    adrp x0, listen_path@PAGE
    add x0, x0, listen_path@PAGEOFF
    mov x16, #10
    svc #0x80

    mov x0, #1
    mov x1, #1
    mov x2, #0
    mov x16, #97
    svc #0x80
    b.cs fail_10
    mov x19, x0

    mov x0, x19
    adrp x1, listen_address@PAGE
    add x1, x1, listen_address@PAGEOFF
    mov x2, #28
    mov x16, #104
    svc #0x80
    b.cs fail_11

    mov x0, x19
    mov x1, #4
    mov x16, #106
    svc #0x80
    b.cs fail_12

    mov x16, #362
    svc #0x80
    b.cs fail_13
    mov x20, x0

    mov x0, #1
    mov x1, #1
    mov x2, #0
    mov x16, #97
    svc #0x80
    b.cs fail_14
    mov x21, x0

    mov x0, x21
    adrp x1, listen_address@PAGE
    add x1, x1, listen_address@PAGEOFF
    mov x2, #28
    mov x16, #98
    svc #0x80
    b.cs fail_15

    adrp x9, read_registration@PAGE
    add x9, x9, read_registration@PAGEOFF
    str x19, [x9]
    mov x0, x20
    mov x1, x9
    mov x2, #1
    adrp x3, returned_event@PAGE
    add x3, x3, returned_event@PAGEOFF
    mov x4, #1
    adrp x5, zero_timeout@PAGE
    add x5, x5, zero_timeout@PAGEOFF
    mov x16, #363
    svc #0x80
    b.cs fail_16
    cmp x0, #1
    b.ne fail_17
    adrp x9, returned_event@PAGE
    add x9, x9, returned_event@PAGEOFF
    ldr x10, [x9]
    cmp x10, x19
    b.ne fail_18
    ldrsh w10, [x9, #8]
    cmn w10, #1
    b.ne fail_19
    ldr x10, [x9, #16]
    cmp x10, #1
    b.lt fail_20

    mov x0, x19
    mov x1, #0
    mov x2, #0
    mov x16, #30
    svc #0x80
    b.cs fail_21
    mov x22, x0

    mov x0, x22
    mov x16, #6
    svc #0x80
    b.cs fail_22
    mov x0, x21
    mov x16, #6
    svc #0x80
    b.cs fail_23
    mov x0, x19
    mov x16, #6
    svc #0x80
    b.cs fail_24

    adrp x9, read_registration@PAGE
    add x9, x9, read_registration@PAGEOFF
    adrp x10, delete_registration@PAGE
    add x10, x10, delete_registration@PAGEOFF
    mov x11, #32
copy_delete:
    ldr x12, [x9], #8
    str x12, [x10], #8
    subs x11, x11, #8
    b.ne copy_delete
    adrp x9, delete_registration@PAGE
    add x9, x9, delete_registration@PAGEOFF
    mov w10, #2
    strh w10, [x9, #10]

    mov x0, x20
    mov x1, x9
    mov x2, #1
    mov x3, #0
    mov x4, #0
    adrp x5, zero_timeout@PAGE
    add x5, x5, zero_timeout@PAGEOFF
    mov x16, #363
    svc #0x80
    b.cs fail_25
    cbnz x0, fail_26

    mov x0, x20
    mov x1, #0
    mov x2, #0
    adrp x3, returned_event@PAGE
    add x3, x3, returned_event@PAGEOFF
    mov x4, #1
    adrp x5, zero_timeout@PAGE
    add x5, x5, zero_timeout@PAGEOFF
    mov x16, #363
    svc #0x80
    b.cs fail_27
    cbnz x0, fail_28

    mov x0, x20
    mov x16, #6
    svc #0x80
    b.cs fail_29

    mov x16, #362
    svc #0x80
    b.cs fail_30
    mov x20, x0
    mov x0, #1
    mov x1, #1
    mov x2, #0
    adrp x3, pipe_descriptors@PAGE
    add x3, x3, pipe_descriptors@PAGEOFF
    mov x16, #42
    svc #0x80
    b.cs fail_31
    mov x19, x0
    mov x21, x1

    mov x0, x21
    adrp x1, payload@PAGE
    add x1, x1, payload@PAGEOFF
    mov x2, #4
    mov x16, #4
    svc #0x80
    b.cs fail_32
    cmp x0, #4
    b.ne fail_33

    adrp x9, read_registration@PAGE
    add x9, x9, read_registration@PAGEOFF
    mov w10, w19
    str x10, [x9]
    adrp x11, returned_event64@PAGE
    add x11, x11, returned_event64@PAGEOFF
    mov x0, x20
    mov x1, x9
    mov x2, #1
    mov x3, x11
    mov x4, #1
    mov x16, #369
    mov x5, #0
    adrp x6, zero_timeout@PAGE
    add x6, x6, zero_timeout@PAGEOFF
    svc #0x80
    b.cs fail_34
    cmp x0, #1
    b.ne fail_35
    adrp x9, returned_event64@PAGE
    add x9, x9, returned_event64@PAGEOFF
    ldr x10, [x9]
    mov w11, w19
    cmp x10, x11
    b.ne fail_36
    ldr x10, [x9, #16]
    cmp x10, #4
    b.ne fail_37
    ldr x10, [x9, #24]
    adrp x11, user_cookie@PAGE
    add x11, x11, user_cookie@PAGEOFF
    cmp x10, x11
    b.ne fail_38
    ldr x10, [x9, #32]
    movz x11, #0x5678
    movk x11, #0x1234, lsl #16
    cmp x10, x11
    b.ne fail_49
    ldr x10, [x9, #40]
    movz x11, #0xcdef
    movk x11, #0x89ab, lsl #16
    cmp x10, x11
    b.ne fail_50

    mov x0, x19
    mov x16, #6
    svc #0x80
    b.cs fail_39
    mov x0, x20
    mov x1, #0
    mov x2, #0
    adrp x3, returned_event@PAGE
    add x3, x3, returned_event@PAGEOFF
    mov x4, #1
    adrp x5, zero_timeout@PAGE
    add x5, x5, zero_timeout@PAGEOFF
    mov x16, #363
    svc #0x80
    b.cs fail_40
    cbnz x0, fail_41

    mov x0, x21
    mov x16, #6
    svc #0x80
    b.cs fail_42
    mov x0, x20
    mov x16, #6
    svc #0x80
    b.cs fail_43

    mov x16, #362
    svc #0x80
    b.cs fail_51
    mov x20, x0
    mov x0, #1
    mov x1, #1
    mov x2, #0
    mov x16, #42
    svc #0x80
    b.cs fail_52
    mov x19, x0
    mov x21, x1
    mov x0, x21
    adrp x1, payload@PAGE
    add x1, x1, payload@PAGEOFF
    mov x2, #4
    mov x16, #4
    svc #0x80
    b.cs fail_53

    adrp x9, read_registration@PAGE
    add x9, x9, read_registration@PAGEOFF
    mov w10, w19
    str x10, [x9]
    mov x0, x20
    mov x1, x9
    mov x2, #1
    mov x3, #0
    mov x4, #0
    mov x16, #363
    svc #0x80
    b.cs fail_54

    mov x0, x20
    mov x16, #41
    svc #0x80
    b.cs fail_55
    mov x22, x0
    mov x0, x22
    mov x16, #6
    svc #0x80
    b.cs fail_56
    mov x0, x20
    mov x1, #0
    mov x2, #0
    adrp x3, returned_event@PAGE
    add x3, x3, returned_event@PAGEOFF
    mov x4, #1
    adrp x5, zero_timeout@PAGE
    add x5, x5, zero_timeout@PAGEOFF
    mov x16, #363
    svc #0x80
    b.cs fail_57
    cmp x0, #1
    b.ne fail_58

    mov x0, x20
    mov x1, #67
    mov x2, #1100
    mov x16, #92
    svc #0x80
    b.cs fail_59
    cmp x0, #1100
    b.lt fail_60
    mov x22, x0
    mov x0, x20
    mov x16, #6
    svc #0x80
    b.cs fail_61
    mov x0, x22
    mov x1, #0
    mov x2, #0
    adrp x3, returned_event@PAGE
    add x3, x3, returned_event@PAGEOFF
    mov x4, #1
    adrp x5, zero_timeout@PAGE
    add x5, x5, zero_timeout@PAGEOFF
    mov x16, #363
    svc #0x80
    b.cs fail_62
    cmp x0, #1
    b.ne fail_63

    mov x0, x22
    mov x1, #1200
    mov x16, #90
    svc #0x80
    b.cs fail_64
    mov x0, x22
    mov x16, #6
    svc #0x80
    b.cs fail_65
    mov x0, #1200
    mov x1, #0
    mov x2, #0
    adrp x3, returned_event@PAGE
    add x3, x3, returned_event@PAGEOFF
    mov x4, #1
    adrp x5, zero_timeout@PAGE
    add x5, x5, zero_timeout@PAGEOFF
    mov x16, #363
    svc #0x80
    b.cs fail_66
    cmp x0, #1
    b.ne fail_67

    mov x0, #1200
    mov x16, #6
    svc #0x80
    b.cs fail_68
    mov x0, #1200
    mov x1, #0
    mov x2, #0
    mov x3, #0
    mov x4, #0
    adrp x5, zero_timeout@PAGE
    add x5, x5, zero_timeout@PAGEOFF
    mov x16, #363
    svc #0x80
    b.cc fail_69
    cmp x0, #9
    b.ne fail_70

    mov x0, x19
    mov x16, #6
    svc #0x80
    b.cs fail_71
    mov x0, x21
    mov x16, #6
    svc #0x80
    b.cs fail_72

    mov x16, #362
    svc #0x80
    b.cs fail_73
    mov x20, x0
    mov x0, #1
    mov x1, #1
    mov x2, #0
    mov x16, #42
    svc #0x80
    b.cs fail_74
    mov x19, x0
    mov x21, x1
    mov x0, #1
    mov x1, #1
    mov x2, #0
    mov x16, #42
    svc #0x80
    b.cs fail_75
    mov x22, x0
    mov x23, x1
    mov x0, x21
    adrp x1, payload@PAGE
    add x1, x1, payload@PAGEOFF
    mov x2, #4
    mov x16, #4
    svc #0x80
    b.cs fail_76
    mov x0, x23
    adrp x1, payload@PAGE
    add x1, x1, payload@PAGEOFF
    mov x2, #4
    mov x16, #4
    svc #0x80
    b.cs fail_77

    adrp x9, change_array64@PAGE
    add x9, x9, change_array64@PAGEOFF
    str x19, [x9]
    str x22, [x9, #48]
    mov x0, x20
    mov x1, x9
    mov x2, #2
    adrp x3, event_array64@PAGE
    add x3, x3, event_array64@PAGEOFF
    mov x4, #2
    mov x5, #0
    adrp x6, zero_timeout@PAGE
    add x6, x6, zero_timeout@PAGEOFF
    mov x16, #369
    svc #0x80
    b.cs fail_78
    cmp x0, #2
    b.ne fail_79
    adrp x9, event_array64@PAGE
    add x9, x9, event_array64@PAGEOFF
    ldr x10, [x9]
    cmp x10, x19
    b.ne fail_80
    ldr x10, [x9, #16]
    cmp x10, #4
    b.ne fail_81
    ldr x10, [x9, #32]
    movz x11, #0xaaaa
    cmp x10, x11
    b.ne fail_82
    ldr x10, [x9, #40]
    movz x11, #0xbbbb
    cmp x10, x11
    b.ne fail_83
    ldr x10, [x9, #48]
    cmp x10, x22
    b.ne fail_84
    ldr x10, [x9, #64]
    cmp x10, #4
    b.ne fail_85
    ldr x10, [x9, #80]
    movz x11, #0xcccc
    cmp x10, x11
    b.ne fail_86
    ldr x10, [x9, #88]
    movz x11, #0xdddd
    cmp x10, x11
    b.ne fail_87

    mov x0, x19
    mov x16, #6
    svc #0x80
    b.cs fail_88
    mov x0, x21
    mov x16, #6
    svc #0x80
    b.cs fail_89
    mov x0, x22
    mov x16, #6
    svc #0x80
    b.cs fail_90
    mov x0, x23
    mov x16, #6
    svc #0x80
    b.cs fail_91
    mov x0, x20
    mov x16, #6
    svc #0x80
    b.cs fail_92

    adrp x0, psynch_condition@PAGE
    add x0, x0, psynch_condition@PAGEOFF
    mov x1, #0x1111
    mov x2, #0x2222
    adrp x3, psynch_mutex@PAGE
    add x3, x3, psynch_mutex@PAGEOFF
    mov x4, #0x3333
    mov x5, #0
    mov x6, #0
    mov x7, #0
    mov x16, #305
    svc #0x80
    b.cc fail_44
    cmp x0, #60
    b.ne fail_45

    adrp x0, psynch_condition@PAGE
    add x0, x0, psynch_condition@PAGEOFF
    mov x1, #0x1111
    mov x2, #0x2222
    adrp x3, psynch_mutex@PAGE
    add x3, x3, psynch_mutex@PAGEOFF
    mov x4, #0x3333
    mov x5, #0
    movz x6, #0x2d00
    movk x6, #0x0131, lsl #16
    mov x7, #0
    mov x16, #305
    svc #0x80
    b.cc fail_46
    cmp x0, #60
    b.ne fail_47

    adrp x0, listen_path@PAGE
    add x0, x0, listen_path@PAGEOFF
    mov x16, #10
    svc #0x80

    mov x0, #1
    adrp x1, success@PAGE
    add x1, x1, success@PAGEOFF
    mov x2, #17
    mov x16, #4
    svc #0x80
    b.cs fail_48
    mov x0, #0
    mov x16, #1
    svc #0x80

fail_10: mov x0, #10
    b fail
fail_11: mov x0, #11
    b fail
fail_12: mov x0, #12
    b fail
fail_13: mov x0, #13
    b fail
fail_14: mov x0, #14
    b fail
fail_15: mov x0, #15
    b fail
fail_16: mov x0, #16
    b fail
fail_17: mov x0, #17
    b fail
fail_18: mov x0, #18
    b fail
fail_19: mov x0, #19
    b fail
fail_20: mov x0, #20
    b fail
fail_21: mov x0, #21
    b fail
fail_22: mov x0, #22
    b fail
fail_23: mov x0, #23
    b fail
fail_24: mov x0, #24
    b fail
fail_25: mov x0, #25
    b fail
fail_26: mov x0, #26
    b fail
fail_27: mov x0, #27
    b fail
fail_28: mov x0, #28
    b fail
fail_29: mov x0, #29
    b fail
fail_30: mov x0, #30
    b fail
fail_31: mov x0, #31
    b fail
fail_32: mov x0, #32
    b fail
fail_33: mov x0, #33
    b fail
fail_34: mov x0, #34
    b fail
fail_35: mov x0, #35
    b fail
fail_36: mov x0, #36
    b fail
fail_37: mov x0, #37
    b fail
fail_38: mov x0, #38
    b fail
fail_39: mov x0, #39
    b fail
fail_40: mov x0, #40
    b fail
fail_41: mov x0, #41
    b fail
fail_42: mov x0, #42
    b fail
fail_43: mov x0, #43
    b fail
fail_44: mov x0, #44
    b fail
fail_45: mov x0, #45
    b fail
fail_46: mov x0, #46
    b fail
fail_47: mov x0, #47
    b fail
fail_48: mov x0, #48
    b fail
fail_49: mov x0, #49
    b fail
fail_50: mov x0, #50
    b fail
fail_51: mov x0, #51
    b fail
fail_52: mov x0, #52
    b fail
fail_53: mov x0, #53
    b fail
fail_54: mov x0, #54
    b fail
fail_55: mov x0, #55
    b fail
fail_56: mov x0, #56
    b fail
fail_57: mov x0, #57
    b fail
fail_58: mov x0, #58
    b fail
fail_59: mov x0, #59
    b fail
fail_60: mov x0, #60
    b fail
fail_61: mov x0, #61
    b fail
fail_62: mov x0, #62
    b fail
fail_63: mov x0, #63
    b fail
fail_64: mov x0, #64
    b fail
fail_65: mov x0, #65
    b fail
fail_66: mov x0, #66
    b fail
fail_67: mov x0, #67
    b fail
fail_68: mov x0, #68
    b fail
fail_69: mov x0, #69
    b fail
fail_70: mov x0, #70
    b fail
fail_71: mov x0, #71
    b fail
fail_72: mov x0, #72
    b fail
fail_73: mov x0, #73
    b fail
fail_74: mov x0, #74
    b fail
fail_75: mov x0, #75
    b fail
fail_76: mov x0, #76
    b fail
fail_77: mov x0, #77
    b fail
fail_78: mov x0, #78
    b fail
fail_79: mov x0, #79
    b fail
fail_80: mov x0, #80
    b fail
fail_81: mov x0, #81
    b fail
fail_82: mov x0, #82
    b fail
fail_83: mov x0, #83
    b fail
fail_84: mov x0, #84
    b fail
fail_85: mov x0, #85
    b fail
fail_86: mov x0, #86
    b fail
fail_87: mov x0, #87
    b fail
fail_88: mov x0, #88
    b fail
fail_89: mov x0, #89
    b fail
fail_90: mov x0, #90
    b fail
fail_91: mov x0, #91
    b fail
fail_92: mov x0, #92
fail:
    mov x16, #1
    svc #0x80

.section __TEXT,__cstring
listen_path:
    .asciz "darwin_kqueue_psynch.sock"
success:
    .asciz "kqueue-psynch-ok\n"

.section __DATA,__data
.p2align 3
listen_address:
    .byte 28
    .byte 1
    .asciz "darwin_kqueue_psynch.sock"
    .space 2
zero_timeout:
    .quad 0
    .quad 0
read_registration:
    .quad 0
    .hword -1
    .hword 1
    .long 0
    .quad 0
    .quad user_cookie
    .quad 0x12345678
    .quad 0x89abcdef
delete_registration:
    .space 32
returned_event:
    .space 32
returned_event64:
    .space 48
change_array64:
    .quad 0
    .hword -1
    .hword 1
    .long 0
    .quad 0
    .quad user_cookie
    .quad 0xaaaa
    .quad 0xbbbb
    .quad 0
    .hword -1
    .hword 1
    .long 0
    .quad 0
    .quad user_cookie
    .quad 0xcccc
    .quad 0xdddd
event_array64:
    .space 96
pipe_descriptors:
    .space 8
payload:
    .ascii "data"
user_cookie:
    .quad 0x1234
.p2align 2
psynch_condition:
    .long 0x1111
    .space 60
psynch_mutex:
    .space 64
