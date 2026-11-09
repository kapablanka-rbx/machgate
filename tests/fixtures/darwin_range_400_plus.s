; darwin_range_400_plus.s - representative Darwin syscalls 400+
.section __TEXT,__text
.globl _main
.p2align 2
_main:
    mov x0, #-1
    adrp x1, wait_status@PAGE
    add x1, x1, wait_status@PAGEOFF
    mov x2, #1
    mov x3, #0
    mov x16, #400
    svc #0x80
    b.cc fail
    cmp x0, #10
    b.ne fail

    mov x0, #0
    mov x1, #0
    adrp x2, waitid_info@PAGE
    add x2, x2, waitid_info@PAGEOFF
    mov x3, #1
    mov x16, #416
    svc #0x80
    b.cc fail
    cmp x0, #10
    b.ne fail

    mov x0, #-2
    adrp x1, path@PAGE
    add x1, x1, path@PAGEOFF
    mov x2, #0
    mov x3, #0
    mov x16, #463
    svc #0x80
    b.cs fail
    mov x19, x0

    mov x0, x19
    adrp x1, readbuf@PAGE
    add x1, x1, readbuf@PAGEOFF
    mov x2, #6
    mov x3, #0
    mov x16, #414
    svc #0x80
    b.cs fail
    cmp x0, #6
    b.ne fail

    adrp x9, aio_read_cb@PAGE
    add x9, x9, aio_read_cb@PAGEOFF
    str w19, [x9]
    str xzr, [x9, #8]
    adrp x10, aio_readbuf@PAGE
    add x10, x10, aio_readbuf@PAGEOFF
    str x10, [x9, #16]
    mov x10, #4
    str x10, [x9, #24]
    str wzr, [x9, #32]
    str wzr, [x9, #72]

    mov x0, x9
    mov x16, #318
    svc #0x80
    b.cs fail

    adrp x9, aio_read_cb@PAGE
    add x9, x9, aio_read_cb@PAGEOFF
    adrp x10, aio_list@PAGE
    add x10, x10, aio_list@PAGEOFF
    str x9, [x10]
    mov x0, x10
    mov x1, #1
    mov x2, #0
    mov x16, #421
    svc #0x80
    b.cs fail

    mov x0, #-2
    adrp x1, path@PAGE
    add x1, x1, path@PAGEOFF
    mov x2, #0
    mov x3, #0
    mov x16, #466
    svc #0x80
    b.cs fail

    mov x0, #-2
    adrp x1, path@PAGE
    add x1, x1, path@PAGEOFF
    adrp x2, statbuf@PAGE
    add x2, x2, statbuf@PAGEOFF
    mov x3, #0
    mov x16, #470
    svc #0x80
    b.cs fail

    mov x0, x19
    mov x1, #3
    mov x2, #0
    mov x16, #406
    svc #0x80
    b.cs fail

    mov x0, #0
    mov x1, #0
    mov x2, #0
    mov x3, #0
    adrp x4, timeout@PAGE
    add x4, x4, timeout@PAGEOFF
    mov x16, #407
    svc #0x80
    b.cs fail
    cmp x0, #0
    b.ne fail

    mov x0, #-2
    adrp x1, linkpath@PAGE
    add x1, x1, linkpath@PAGEOFF
    mov x2, #0
    mov x16, #472
    svc #0x80

    adrp x0, path@PAGE
    add x0, x0, path@PAGEOFF
    mov x1, #-2
    adrp x2, linkpath@PAGE
    add x2, x2, linkpath@PAGEOFF
    mov x16, #474
    svc #0x80
    b.cs fail

    mov x0, #-2
    adrp x1, linkpath@PAGE
    add x1, x1, linkpath@PAGEOFF
    movz x2, #0x20, lsl #16
    mov x3, #0
    mov x16, #463
    svc #0x80
    b.cs fail
    mov x20, x0

    mov x0, x20
    adrp x1, linkbuf@PAGE
    add x1, x1, linkbuf@PAGEOFF
    mov x2, #16
    mov x16, #551
    svc #0x80
    b.cs fail
    cmp x0, #16
    b.ne fail

    mov x0, x20
    mov x16, #6
    svc #0x80
    b.cs fail

    mov x0, #-2
    adrp x1, rename_src@PAGE
    add x1, x1, rename_src@PAGEOFF
    mov x2, #-2
    adrp x3, rename_dst@PAGE
    add x3, x3, rename_dst@PAGEOFF
    mov x4, #0
    mov x16, #488       ; Darwin SYS_renameatx_np
    svc #0x80
    b.cs fail

    mov x0, #-2
    adrp x1, rename_excl_src@PAGE
    add x1, x1, rename_excl_src@PAGEOFF
    mov x2, #-2
    adrp x3, rename_dst@PAGE
    add x3, x3, rename_dst@PAGEOFF
    mov x4, #4
    mov x16, #488       ; Darwin SYS_renameatx_np, RENAME_EXCL
    svc #0x80
    b.cc fail
    cmp x0, #17
    b.ne fail

    mov x16, #426       ; Darwin SYS___mac_getfsstat
    svc #0x80
    b.cc fail
    cmp x0, #45
    b.ne fail

    mov x0, #-2
    adrp x1, path@PAGE
    add x1, x1, path@PAGEOFF
    adrp x2, attrlist_file400@PAGE
    add x2, x2, attrlist_file400@PAGEOFF
    adrp x3, attrlist_buf400@PAGE
    add x3, x3, attrlist_buf400@PAGEOFF
    mov x4, #512
    mov x5, #0
    mov x16, #476
    svc #0x80
    b.cs fail
    adrp x9, attrlist_buf400@PAGE
    add x9, x9, attrlist_buf400@PAGEOFF
    ldr w10, [x9]
    cmp w10, #52
    b.lt fail
    ldr w10, [x9, #32]
    cmp w10, #1
    b.ne fail
    adrp x11, fsid_buf@PAGE
    add x11, x11, fsid_buf@PAGEOFF
    ldr x10, [x9, #24]
    str x10, [x11]
    adrp x11, fileid_buf400@PAGE
    add x11, x11, fileid_buf400@PAGEOFF
    ldr x10, [x9, #36]
    str x10, [x11]
    adrp x11, fsobj_id400@PAGE
    add x11, x11, fsobj_id400@PAGEOFF
    str w10, [x11]
    str wzr, [x11, #4]

    adrp x0, fsgetpath_buf@PAGE
    add x0, x0, fsgetpath_buf@PAGEOFF
    mov x1, #4096
    adrp x2, fsid_buf@PAGE
    add x2, x2, fsid_buf@PAGEOFF
    adrp x3, fileid_buf400@PAGE
    add x3, x3, fileid_buf400@PAGEOFF
    ldr x3, [x3]
    mov x16, #427
    svc #0x80
    b.cs fail
    cbz x0, fail
    adrp x9, fsgetpath_buf@PAGE
    add x9, x9, fsgetpath_buf@PAGEOFF
    ldrb w10, [x9]
    cmp w10, #'/'
    b.ne fail

    mov x0, #-2
    adrp x1, dotpath400@PAGE
    add x1, x1, dotpath400@PAGEOFF
    mov x2, #0
    mov x3, #0
    mov x16, #463
    svc #0x80
    b.cs fail
    mov x21, x0

    mov x0, x21
    adrp x1, attrlist_bulk400@PAGE
    add x1, x1, attrlist_bulk400@PAGEOFF
    adrp x2, attrlist_bulk_buf400@PAGE
    add x2, x2, attrlist_bulk_buf400@PAGEOFF
    mov x3, #1024
    mov x4, #0
    mov x16, #461
    svc #0x80
    b.cs fail
    cbz x0, fail

    mov x0, x21
    mov x16, #6
    svc #0x80
    b.cs fail

    mov x0, #-2
    adrp x1, path@PAGE
    add x1, x1, path@PAGEOFF
    mov x2, #-2
    adrp x3, clonefileat_dst400@PAGE
    add x3, x3, clonefileat_dst400@PAGEOFF
    mov x4, #0
    mov x16, #462
    svc #0x80
    b.cs fail

    adrp x0, fsid_buf@PAGE
    add x0, x0, fsid_buf@PAGEOFF
    adrp x1, fsobj_id400@PAGE
    add x1, x1, fsobj_id400@PAGEOFF
    mov x2, #0
    mov x16, #479
    svc #0x80
    b.cs fail
    mov x21, x0

    mov x0, x21
    adrp x1, openbyid_buf400@PAGE
    add x1, x1, openbyid_buf400@PAGEOFF
    mov x2, #6
    mov x3, #0
    mov x16, #414
    svc #0x80
    b.cs fail
    cmp x0, #6
    b.ne fail

    mov x0, x21
    mov x16, #6
    svc #0x80
    b.cs fail

    mov x0, #0
    mov x1, #0x1234
    mov x16, #477
    svc #0x80
    b.cs fail

    mov x0, #1
    adrp x1, csr_config400@PAGE
    add x1, x1, csr_config400@PAGEOFF
    mov x2, #4
    mov x16, #483
    svc #0x80
    b.cs fail
    adrp x9, csr_config400@PAGE
    add x9, x9, csr_config400@PAGEOFF
    ldr w10, [x9]
    cbz w10, fail

    adrp x0, guarded_path@PAGE
    add x0, x0, guarded_path@PAGEOFF
    adrp x1, guard_a@PAGE
    add x1, x1, guard_a@PAGEOFF
    mov x2, #19
    movz x3, #0x0602
    movk x3, #0x0100, lsl #16
    mov x4, #0600
    mov x16, #441      ; Darwin SYS_guarded_open_np
    svc #0x80
    b.cs fail
    mov x28, x0

    mov x0, x28
    adrp x1, guard_bad@PAGE
    add x1, x1, guard_bad@PAGEOFF
    mov x16, #442      ; Darwin SYS_guarded_close_np, wrong guard
    svc #0x80
    b.cc fail
    cmp x0, #1
    b.ne fail

    mov x0, x28
    adrp x1, guard_a@PAGE
    add x1, x1, guard_a@PAGEOFF
    adrp x2, guarded_msg1@PAGE
    add x2, x2, guarded_msg1@PAGEOFF
    mov x3, #4
    mov x16, #485      ; Darwin SYS_guarded_write_np
    svc #0x80
    b.cs fail
    cmp x0, #4
    b.ne fail

    mov x0, x28
    adrp x1, guard_a@PAGE
    add x1, x1, guard_a@PAGEOFF
    adrp x2, guarded_msg2@PAGE
    add x2, x2, guarded_msg2@PAGEOFF
    mov x3, #2
    mov x4, #1
    mov x16, #486      ; Darwin SYS_guarded_pwrite_np
    svc #0x80
    b.cs fail
    cmp x0, #2
    b.ne fail

    mov x0, x28
    adrp x1, guard_a@PAGE
    add x1, x1, guard_a@PAGEOFF
    adrp x2, guarded_iov@PAGE
    add x2, x2, guarded_iov@PAGEOFF
    mov x3, #2
    mov x16, #487      ; Darwin SYS_guarded_writev_np
    svc #0x80
    b.cs fail
    cmp x0, #4
    b.ne fail

    mov x0, x28
    adrp x1, guarded_readbuf@PAGE
    add x1, x1, guarded_readbuf@PAGEOFF
    mov x2, #8
    mov x3, #0
    mov x16, #414
    svc #0x80
    b.cs fail
    cmp x0, #8
    b.ne fail
    adrp x9, guarded_readbuf@PAGE
    add x9, x9, guarded_readbuf@PAGEOFF
    ldrb w10, [x9]
    cmp w10, #'A'
    b.ne fail
    ldrb w10, [x9, #1]
    cmp w10, #'x'
    b.ne fail
    ldrb w10, [x9, #2]
    cmp w10, #'y'
    b.ne fail
    ldrb w10, [x9, #3]
    cmp w10, #'D'
    b.ne fail
    ldrb w10, [x9, #4]
    cmp w10, #'E'
    b.ne fail
    ldrb w10, [x9, #7]
    cmp w10, #'H'
    b.ne fail

    mov x0, x28
    adrp x1, guard_a@PAGE
    add x1, x1, guard_a@PAGEOFF
    mov x2, #19
    adrp x3, guard_b@PAGE
    add x3, x3, guard_b@PAGEOFF
    mov x4, #19
    adrp x5, fdflags_save@PAGE
    add x5, x5, fdflags_save@PAGEOFF
    mov x16, #444      ; Darwin SYS_change_fdguard_np
    svc #0x80
    b.cs fail

    mov x0, x28
    adrp x1, guard_a@PAGE
    add x1, x1, guard_a@PAGEOFF
    adrp x2, guarded_msg3@PAGE
    add x2, x2, guarded_msg3@PAGEOFF
    mov x3, #1
    mov x16, #485      ; old guard should no longer authorize writes
    svc #0x80
    b.cc fail
    cmp x0, #1
    b.ne fail

    mov x0, x28
    adrp x1, guard_b@PAGE
    add x1, x1, guard_b@PAGEOFF
    adrp x2, guarded_msg3@PAGE
    add x2, x2, guarded_msg3@PAGEOFF
    mov x3, #1
    mov x4, #8
    mov x16, #486
    svc #0x80
    b.cs fail
    cmp x0, #1
    b.ne fail

    mov x0, x28
    adrp x1, guard_b@PAGE
    add x1, x1, guard_b@PAGEOFF
    mov x16, #442
    svc #0x80
    b.cs fail

    adrp x0, guard_a@PAGE
    add x0, x0, guard_a@PAGEOFF
    mov x1, #3
    mov x16, #443      ; Darwin SYS_guarded_kqueue_np
    svc #0x80
    b.cs fail
    mov x28, x0

    mov x0, x28
    adrp x1, guard_a@PAGE
    add x1, x1, guard_a@PAGEOFF
    mov x16, #442
    svc #0x80
    b.cs fail

    adrp x0, guarded_dprotected_path@PAGE
    add x0, x0, guarded_dprotected_path@PAGEOFF
    adrp x1, guard_a@PAGE
    add x1, x1, guard_a@PAGEOFF
    mov x2, #3
    movz x3, #0x0602
    movk x3, #0x0100, lsl #16
    mov x4, #1
    mov x5, #0
    mov x6, #0600
    mov x16, #484      ; Darwin SYS_guarded_open_dprotected_np
    svc #0x80
    b.cs fail
    mov x28, x0

    mov x0, x28
    adrp x1, guard_a@PAGE
    add x1, x1, guard_a@PAGEOFF
    mov x16, #442
    svc #0x80
    b.cs fail

    mov x0, #-2
    adrp x1, guarded_path@PAGE
    add x1, x1, guarded_path@PAGEOFF
    mov x2, #0
    mov x16, #472
    svc #0x80
    b.cs fail

    mov x0, #-2
    adrp x1, guarded_dprotected_path@PAGE
    add x1, x1, guarded_dprotected_path@PAGEOFF
    mov x2, #0
    mov x16, #472
    svc #0x80
    b.cs fail

    mov x0, #-1
    adrp x1, guard_a@PAGE
    add x1, x1, guard_a@PAGEOFF
    mov x16, #442
    svc #0x80
    b.cc fail
    cmp x0, #9
    b.ne fail

    mov x0, #0
    mov x16, #445
    svc #0x80
    b.cc fail
    cmp x0, #1
    b.ne fail

    mov x0, #-1
    mov x1, #1
    mov x2, #0
    mov x16, #446
    svc #0x80
    b.cs fail

    mov x0, #-1
    mov x1, #3
    mov x2, #0xa32
    mov x16, #446
    svc #0x80
    b.cs fail

    mov x0, #-1
    mov x1, #4
    mov x2, #1
    mov x16, #446
    svc #0x80
    b.cs fail

    mov x0, #-1
    mov x1, #99
    mov x2, #0
    mov x16, #446
    svc #0x80
    b.cc fail
    cmp x0, #22
    b.ne fail

    mov x0, #-1
    mov x1, #3
    mov x2, #0
    mov x16, #446
    svc #0x80
    b.cc fail
    cmp x0, #22
    b.ne fail

    mov x0, #0
    mov x1, #0
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x5, #0
    mov x16, #451
    svc #0x80
    b.cs fail

    mov x0, #0
    adrp x1, uuid400@PAGE
    add x1, x1, uuid400@PAGEOFF
    mov x2, #16
    mov x3, #0
    mov x16, #452
    svc #0x80
    b.cs fail

    mov x0, #0
    mov x1, #0
    mov x16, #454
    svc #0x80
    b.cs fail

    mov x16, #455
    svc #0x80
    b.cs fail

    mov x0, #0
    mov x1, #0
    mov x2, #123
    adrp x3, sfi_time400@PAGE
    add x3, x3, sfi_time400@PAGEOFF
    mov x16, #456
    svc #0x80
    b.cs fail
    adrp x9, sfi_time400@PAGE
    add x9, x9, sfi_time400@PAGEOFF
    ldr x10, [x9]
    cmp x10, #123
    b.ne fail

    mov x0, #0
    mov x1, #0
    mov x2, #0
    adrp x3, sfi_flags400@PAGE
    add x3, x3, sfi_flags400@PAGEOFF
    mov x16, #457
    svc #0x80
    b.cs fail
    adrp x9, sfi_flags400@PAGE
    add x9, x9, sfi_flags400@PAGEOFF
    ldr w10, [x9]
    cbnz w10, fail

    mov x0, #1
    adrp x1, coalition_id400@PAGE
    add x1, x1, coalition_id400@PAGEOFF
    mov x2, #0
    mov x16, #458
    svc #0x80
    b.cs fail
    adrp x9, coalition_id400@PAGE
    add x9, x9, coalition_id400@PAGEOFF
    ldr x10, [x9]
    cbz x10, fail

    mov x0, #1
    adrp x1, coalition_id400@PAGE
    add x1, x1, coalition_id400@PAGEOFF
    adrp x2, coalition_buf400@PAGE
    add x2, x2, coalition_buf400@PAGEOFF
    adrp x3, coalition_bufsize400@PAGE
    add x3, x3, coalition_bufsize400@PAGEOFF
    mov x16, #459
    svc #0x80
    b.cs fail

    mov x0, #0
    mov x1, #0
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x16, #489
    svc #0x80
    b.cs fail

    adrp x0, uuid400@PAGE
    add x0, x0, uuid400@PAGEOFF
    mov x1, #16
    mov x16, #490
    svc #0x80
    b.cs fail

    mov x0, #0
    adrp x1, stackshot_config400@PAGE
    add x1, x1, stackshot_config400@PAGEOFF
    mov x2, #0
    mov x16, #491
    svc #0x80
    b.cs fail

    adrp x0, microstackshot_buf400@PAGE
    add x0, x0, microstackshot_buf400@PAGEOFF
    mov x1, #64
    mov x2, #0
    mov x16, #492
    svc #0x80
    b.cs fail

    adrp x0, uuid400@PAGE
    add x0, x0, uuid400@PAGEOFF
    mov x1, #0
    adrp x2, pgo_buf400@PAGE
    add x2, x2, pgo_buf400@PAGEOFF
    mov x3, #64
    mov x16, #493
    svc #0x80
    b.cs fail
    cbnz x0, fail

    mov x0, #4
    mov x1, #0
    mov x2, #0
    adrp x3, persona_id400@PAGE
    add x3, x3, persona_id400@PAGEOFF
    mov x4, #0
    mov x5, #0
    mov x16, #494
    svc #0x80
    b.cs fail

    mov x25, #51
    mov x0, x19
    mov x1, #-2
    adrp x2, fclonefileat_dst400@PAGE
    add x2, x2, fclonefileat_dst400@PAGEOFF
    mov x3, #0
    mov x16, #517
    svc #0x80
    b.cs fail

    mov x25, #52
    mov x0, #-2
    adrp x1, clonefileat_dst400@PAGE
    add x1, x1, clonefileat_dst400@PAGEOFF
    adrp x2, attrlist_set_mode400@PAGE
    add x2, x2, attrlist_set_mode400@PAGEOFF
    adrp x3, mode_0600_400@PAGE
    add x3, x3, mode_0600_400@PAGEOFF
    mov x4, #4
    mov x5, #0
    mov x16, #524
    svc #0x80
    b.cs fail

    mov x25, #53
    mov x0, #-2
    adrp x1, clonefileat_dst400@PAGE
    add x1, x1, clonefileat_dst400@PAGEOFF
    adrp x2, statbuf@PAGE
    add x2, x2, statbuf@PAGEOFF
    mov x3, #0
    mov x16, #470
    svc #0x80
    b.cs fail
    adrp x9, statbuf@PAGE
    add x9, x9, statbuf@PAGEOFF
    ldrh w10, [x9, #4]
    and w10, w10, #0777
    cmp w10, #0600
    b.ne fail

    mov x0, #-2
    adrp x1, linkpath@PAGE
    add x1, x1, linkpath@PAGEOFF
    mov x2, #0
    mov x16, #472
    svc #0x80
    b.cs fail

    adrp x0, entropy@PAGE
    add x0, x0, entropy@PAGEOFF
    mov x1, #16
    mov x16, #500
    svc #0x80
    b.cs fail

    adrp x0, ntptimebuf@PAGE
    add x0, x0, ntptimebuf@PAGEOFF
    mov x16, #528
    svc #0x80
    b.cs fail

    adrp x0, timexbuf@PAGE
    add x0, x0, timexbuf@PAGEOFF
    mov x16, #527
    svc #0x80
    b.cs fail

    adrp x0, memlevel@PAGE
    add x0, x0, memlevel@PAGEOFF
    mov x16, #453
    svc #0x80
    b.cs fail

    mov x16, #534
    svc #0x80
    b.cs fail
    cbz x0, fail

    mov x0, #4
    mov x1, #0
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x16, #440
    svc #0x80
    b.cs fail

    mov x0, #24
    mov x1, #0
    mov x2, #0
    adrp x3, memlevel@PAGE
    add x3, x3, memlevel@PAGEOFF
    mov x4, #4
    mov x16, #440
    svc #0x80
    b.cs fail

    adrp x0, sem_name@PAGE
    add x0, x0, sem_name@PAGEOFF
    mov x16, #270
    svc #0x80

    adrp x0, sem_name@PAGE
    add x0, x0, sem_name@PAGEOFF
    mov x1, #0xA00
    mov x2, #0600
    mov x3, #1
    mov x16, #268
    svc #0x80
    b.cs fail
    mov x21, x0

    mov x0, x21
    mov x16, #420
    svc #0x80
    b.cs fail

    adrp x0, sem_name@PAGE
    add x0, x0, sem_name@PAGEOFF
    mov x16, #270
    svc #0x80
    b.cs fail

    mov x0, x21
    mov x16, #269
    svc #0x80
    b.cs fail

    mov x0, #0
    mov x1, #0
    mov x2, #1
    mov x3, #1
    mov x4, #0
    mov x5, #0
    mov x16, #423
    svc #0x80
    b.cc fail
    cmp x0, #35
    b.ne fail

    mov x0, #0
    mov x1, #896
    mov x16, #259
    svc #0x80
    b.cs fail
    mov x27, x0

    mov x0, x27
    adrp x1, sysv_send_msg@PAGE
    add x1, x1, sysv_send_msg@PAGEOFF
    mov x2, #4
    mov x3, #0
    mov x16, #418
    svc #0x80
    b.cs fail

    mov x0, x27
    adrp x1, sysv_recv_msg@PAGE
    add x1, x1, sysv_recv_msg@PAGEOFF
    mov x2, #4
    mov x3, #0
    mov x4, #0
    mov x16, #419
    svc #0x80
    b.cs fail
    cmp x0, #4
    b.ne fail
    adrp x9, sysv_recv_msg@PAGE
    add x9, x9, sysv_recv_msg@PAGEOFF
    ldrb w10, [x9, #8]
    cmp w10, #'n'
    b.ne fail
    ldrb w10, [x9, #11]
    cmp w10, #'g'
    b.ne fail

    mov x0, x27
    mov x1, #0
    mov x2, #0
    mov x16, #258
    svc #0x80
    b.cs fail

    mov x0, #1
    adrp x1, ulock_word@PAGE
    add x1, x1, ulock_word@PAGEOFF
    mov x2, #0
    mov x16, #516
    svc #0x80
    b.cc fail
    cmp x0, #2
    b.ne fail

    mov x0, #1
    adrp x1, ulock_word@PAGE
    add x1, x1, ulock_word@PAGEOFF
    mov x2, #7
    mov x3, #1
    mov x16, #515
    svc #0x80
    b.cs fail
    cmp x0, #0
    b.ne fail

    mov x0, #1
    adrp x1, ulock_word@PAGE
    add x1, x1, ulock_word@PAGEOFF
    mov x2, #7
    mov x3, #1
    mov x4, #0
    mov x16, #544
    svc #0x80
    b.cs fail
    cmp x0, #0
    b.ne fail

    mov x0, #0
    mov x1, #0
    mov x2, #0
    mov x3, #0
    mov x16, #536
    svc #0x80
    b.cs fail

    mov x0, #0
    mov x1, #0
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x5, #0
    mov x16, #529
    svc #0x80
    b.cs fail

    mov x16, #482
    svc #0x80
    b.cs fail
    cbnz x0, fail

    mov x16, #20
    svc #0x80
    b.cs fail
    mov x21, x0

    mov x0, #2
    mov x1, x21
    mov x2, #13
    mov x3, #0
    mov x4, #0
    mov x5, #0
    adrp x6, proc_shortinfo_ext@PAGE
    add x6, x6, proc_shortinfo_ext@PAGEOFF
    mov x7, #64
    mov x16, #545
    svc #0x80
    b.cs fail
    cmp x0, #64
    b.ne fail
    adrp x9, proc_shortinfo_ext@PAGE
    add x9, x9, proc_shortinfo_ext@PAGEOFF
    ldr w10, [x9]
    cmp x10, x21
    b.ne fail

    mov x0, #0
    mov x1, #0
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x5, #0
    mov x6, #0
    mov x16, #520
    svc #0x80
    b.cc fail
    cmp x0, #22
    b.ne fail

    movz x0, #0xffff
    movk x0, #0x7fff, lsl #16
    mov x1, #0
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x5, #0
    mov x6, #0
    mov x16, #520
    svc #0x80
    b.cc fail
    cmp x0, #3
    b.ne fail

    adrp x0, mount_type400@PAGE
    add x0, x0, mount_type400@PAGEOFF
    adrp x1, dotpath400@PAGE
    add x1, x1, dotpath400@PAGEOFF
    mov x2, #0
    mov x3, #0
    adrp x4, mac_label400@PAGE
    add x4, x4, mac_label400@PAGEOFF
    mov x16, #424       ; Darwin SYS___mac_mount
	svc #0x80
	b.cc fail
	cmp x0, #45
	b.ne fail

    adrp x0, path@PAGE
    add x0, x0, path@PAGEOFF
    adrp x1, mac_label400@PAGE
    add x1, x1, mac_label400@PAGEOFF
    mov x16, #425       ; Darwin SYS___mac_get_mount
    svc #0x80
    b.cs fail
    adrp x9, mac_label400_buf@PAGE
    add x9, x9, mac_label400_buf@PAGEOFF
    ldrb w10, [x9]
    cbnz w10, fail

    mov x0, #0
    mov x1, #0
    mov x2, #0
    mov x3, #0
    mov x4, #0

    mov x16, #426       ; Darwin SYS___mac_getfsstat
    svc #0x80
    b.cc fail
    cmp x0, #45
    b.ne fail

    mov x16, #428       ; Darwin SYS_audit_session_self
    svc #0x80
    b.cs fail
    cbz x0, fail
    adrp x9, audit_session_port_name@PAGE
    add x9, x9, audit_session_port_name@PAGEOFF
    str w0, [x9]

    adrp x9, audit_session_port_name@PAGE
    add x9, x9, audit_session_port_name@PAGEOFF
    ldr w0, [x9]
    mov x16, #429       ; Darwin SYS_audit_session_join
    svc #0x80
    b.cs fail

    mov x0, x19
    adrp x1, fileport_name@PAGE
    add x1, x1, fileport_name@PAGEOFF
    mov x16, #430       ; Darwin SYS_fileport_makeport
    svc #0x80
    b.cs fail
    adrp x9, fileport_name@PAGE
    add x9, x9, fileport_name@PAGEOFF
    ldr w0, [x9]
    cbz x0, fail
    mov x16, #431       ; Darwin SYS_fileport_makefd
    svc #0x80
    b.cs fail
    mov x20, x0
    mov x16, #6
    svc #0x80
    b.cs fail

    mov x0, #0
    adrp x1, audit_session_port_out@PAGE
    add x1, x1, audit_session_port_out@PAGEOFF
    mov x16, #432       ; Darwin SYS_audit_session_port
    svc #0x80
    b.cs fail
    adrp x9, audit_session_port_out@PAGE
    add x9, x9, audit_session_port_out@PAGEOFF
    ldr w10, [x9]
    cbz w10, fail

    mov x0, #0
    mov x16, #429       ; Darwin SYS_audit_session_join invalid port
    svc #0x80
	b.cc fail
	cmp x0, #22
	b.ne fail

	mov x16, #20        ; Darwin SYS_getpid
	svc #0x80
	b.cs fail
    mov x16, #433       ; Darwin SYS_pid_suspend
    svc #0x80
    b.cs fail

    mov x0, #-1
    mov x16, #433       ; Darwin SYS_pid_suspend missing pid
    svc #0x80
    b.cc fail
    cmp x0, #3
    b.ne fail

    mov x16, #20        ; Darwin SYS_getpid
    svc #0x80
    b.cs fail
    mov x16, #434       ; Darwin SYS_pid_resume
    svc #0x80
    b.cs fail

    mov x0, #-1
    mov x16, #434       ; Darwin SYS_pid_resume missing pid
    svc #0x80
    b.cc fail
    cmp x0, #3
    b.ne fail

    mov x16, #20        ; Darwin SYS_getpid
    svc #0x80
    b.cs fail
    mov x16, #435       ; Darwin SYS_pid_hibernate
    svc #0x80
    b.cc fail
    cmp x0, #45
    b.ne fail

    mov x0, #-2
    mov x16, #435       ; Darwin SYS_pid_hibernate compressor sweep
    svc #0x80
    b.cs fail

    mov x0, #0
    adrp x1, kas_value400@PAGE
    add x1, x1, kas_value400@PAGEOFF
    adrp x2, kas_size400@PAGE
    add x2, x2, kas_size400@PAGEOFF
    mov x16, #439       ; Darwin SYS_kas_info
    svc #0x80
    b.cs fail
    adrp x9, kas_value400@PAGE
    add x9, x9, kas_value400@PAGEOFF
    ldr x10, [x9]
    cbnz x10, fail
    adrp x9, kas_size400@PAGE
    add x9, x9, kas_size400@PAGEOFF
    ldr x10, [x9]
    cmp x10, #8
    b.ne fail

    mov x0, #4
    adrp x1, kas_value400@PAGE
    add x1, x1, kas_value400@PAGEOFF
    adrp x2, kas_size400@PAGE
    add x2, x2, kas_size400@PAGEOFF
    mov x16, #439       ; Darwin SYS_kas_info invalid selector
    svc #0x80
    b.cc fail
    cmp x0, #22
    b.ne fail

    mov x0, #0
    adrp x1, kas_value400@PAGE
    add x1, x1, kas_value400@PAGEOFF
    mov x2, #0
	mov x16, #439       ; Darwin SYS_kas_info null size
	svc #0x80
	b.cc fail
	cmp x0, #14
	b.ne fail

    mov x0, #0x800
    mov x1, #5
    mov x2, #1
    mov x3, #0
	mov x16, #478
    svc #0x80
    b.cs fail
    cbz x0, fail

    mov x0, #0x800
    mov x1, #5
    mov x2, #8
    mov x3, #0
    mov x16, #478
    svc #0x80
    b.cc fail
    cmp x0, #22
    b.ne fail

    mov x0, #0x10
    mov x1, #0
    mov x2, #0
    mov x3, #0
    mov x16, #478
    svc #0x80
    b.cc fail
    cmp x0, #45
    b.ne fail

    mov x0, #0
    mov x1, #1
    mov x16, #496       ; Darwin SYS_mach_eventlink_signal
    svc #0x80
    b.cc fail
    cmp x0, #22
    b.ne fail

    mov x0, #0
    mov x1, #1
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x16, #497       ; Darwin SYS_mach_eventlink_wait_until
    svc #0x80
    b.cc fail
    cmp x0, #22
    b.ne fail

    mov x0, #0
    mov x1, #1
    mov x2, #1
    mov x3, #0
    mov x4, #0
    mov x5, #0
    mov x16, #498       ; Darwin SYS_mach_eventlink_signal_wait_until
    svc #0x80
    b.cc fail
    cmp x0, #22
    b.ne fail

    mov x0, #0
    mov x1, #0
    mov x2, #0
    mov x3, #0
    mov x16, #499
    svc #0x80
    b.cs fail

    mov x0, #0
    mov x1, #-2
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x5, #0
    mov x16, #518       ; Darwin SYS_fs_snapshot null name
	svc #0x80
	b.cc fail
	cmp x0, #14
	b.ne fail

    mov x0, #0
    mov x1, #-2
    adrp x2, snapshot_name400@PAGE
    add x2, x2, snapshot_name400@PAGEOFF
    mov x3, #0
    mov x4, #0
    mov x5, #0
    mov x16, #518       ; Darwin SYS_fs_snapshot
	svc #0x80
	b.cc fail
	cmp x0, #45
	b.ne fail

    adrp x0, mount_type400@PAGE
    add x0, x0, mount_type400@PAGEOFF
    mov x1, #-1
    mov x2, #0
    mov x3, #0
    mov x16, #526       ; Darwin SYS_fmount invalid fd
	svc #0x80
	b.cc fail
	cmp x0, #9
	b.ne fail

    adrp x0, mount_type400@PAGE
    add x0, x0, mount_type400@PAGEOFF
    mov x1, x19
    mov x2, #0
    mov x3, #0
    mov x16, #526       ; Darwin SYS_fmount
	svc #0x80
	b.cc fail
	cmp x0, #1
	b.ne fail

    adrp x0, missing_path400@PAGE
    add x0, x0, missing_path400@PAGEOFF
    adrp x1, dotpath400@PAGE
    add x1, x1, dotpath400@PAGEOFF
    mov x16, #537       ; Darwin SYS_pivot_root missing new root
	svc #0x80
	b.cc fail
	cmp x0, #2
	b.ne fail

    adrp x0, dotpath400@PAGE
    add x0, x0, dotpath400@PAGEOFF
    adrp x1, dotpath400@PAGE
    add x1, x1, dotpath400@PAGEOFF
    mov x16, #537       ; Darwin SYS_pivot_root
	svc #0x80
	b.cc fail
	cmp x0, #1
	b.ne fail

    mov x0, #-1
    adrp x1, dotpath400@PAGE
    add x1, x1, dotpath400@PAGEOFF
    mov x2, #0
    mov x3, #0
    mov x16, #549       ; Darwin SYS_graftdmg invalid fd
	svc #0x80
	b.cc fail
	cmp x0, #9
	b.ne fail

    mov x0, x19
    adrp x1, dotpath400@PAGE
    add x1, x1, dotpath400@PAGEOFF
    mov x2, #0
    mov x3, #0
    mov x16, #549       ; Darwin SYS_graftdmg
	svc #0x80
	b.cc fail
	cmp x0, #45
	b.ne fail

    adrp x0, missing_path400@PAGE
    add x0, x0, missing_path400@PAGEOFF
    mov x1, #0
    mov x16, #555       ; Darwin SYS_ungraftdmg missing path
	svc #0x80
	b.cc fail
	cmp x0, #2
	b.ne fail

    adrp x0, dotpath400@PAGE
    add x0, x0, dotpath400@PAGEOFF
    mov x1, #0
    mov x16, #555       ; Darwin SYS_ungraftdmg
	svc #0x80
	b.cc fail
	cmp x0, #45
	b.ne fail

    mov x0, #1
    adrp x1, iov@PAGE
    add x1, x1, iov@PAGEOFF
    mov x2, #2
    mov x16, #412
    svc #0x80
    b.cs fail

    mov x0, #1
    mov x1, #1
    mov x2, #0
    adrp x3, sockfds@PAGE
    add x3, x3, sockfds@PAGEOFF
    mov x16, #135
    svc #0x80
    b.cs fail
    adrp x9, sockfds@PAGE
    add x9, x9, sockfds@PAGEOFF
    ldr w22, [x9]
    ldr w23, [x9, #4]

    mov x0, x22
    adrp x1, sendmsg_hdr@PAGE
    add x1, x1, sendmsg_hdr@PAGEOFF
    mov x2, #0
    mov x16, #402
    svc #0x80
    b.cs fail
    cmp x0, #4
    b.ne fail

    mov x0, x23
    adrp x1, recvfrombuf@PAGE
    add x1, x1, recvfrombuf@PAGEOFF
    mov x2, #4
    mov x3, #0
    mov x4, #0
    mov x5, #0
    mov x16, #403
    svc #0x80
    b.cs fail
    cmp x0, #4
    b.ne fail
    adrp x9, recvfrombuf@PAGE
    add x9, x9, recvfrombuf@PAGEOFF
    ldrb w10, [x9]
    cmp w10, #'s'
    b.ne fail

    mov x0, x23
    adrp x1, sockreply@PAGE
    add x1, x1, sockreply@PAGEOFF
    mov x2, #4
    mov x16, #4
    svc #0x80
    b.cs fail
    cmp x0, #4
    b.ne fail

    mov x0, x22
    adrp x1, recvmsg_hdr@PAGE
    add x1, x1, recvmsg_hdr@PAGEOFF
    mov x2, #0
    mov x16, #401
    svc #0x80
    b.cs fail
    cmp x0, #4
    b.ne fail
    adrp x9, recvmsgbuf@PAGE
    add x9, x9, recvmsgbuf@PAGEOFF
    ldrb w10, [x9]
    cmp w10, #'p'
    b.ne fail

    mov x0, x22
    adrp x1, sockncan@PAGE
    add x1, x1, sockncan@PAGEOFF
    mov x2, #4
    mov x3, #0
    mov x4, #0
    mov x5, #0
    mov x16, #413
    svc #0x80
    b.cs fail
    cmp x0, #4
    b.ne fail

    mov x0, x23
    adrp x1, recvfrombuf@PAGE
    add x1, x1, recvfrombuf@PAGEOFF
    mov x2, #4
    mov x16, #3
    svc #0x80
    b.cs fail
    cmp x0, #4
    b.ne fail
    adrp x9, recvfrombuf@PAGE
    add x9, x9, recvfrombuf@PAGEOFF
    ldrb w10, [x9]
    cmp w10, #'n'
    b.ne fail

    mov x0, x22
    adrp x1, sendmsg_x_hdr@PAGE
    add x1, x1, sendmsg_x_hdr@PAGEOFF
    mov x2, #1
    mov x3, #0
    mov x16, #481
    svc #0x80
    b.cs fail
    cmp x0, #1
    b.ne fail

    mov x0, x23
    adrp x1, recvfrombuf@PAGE
    add x1, x1, recvfrombuf@PAGEOFF
    mov x2, #4
    mov x3, #0
    mov x4, #0
    mov x5, #0
    mov x16, #403
    svc #0x80
    b.cs fail
    cmp x0, #4
    b.ne fail
    adrp x9, recvfrombuf@PAGE
    add x9, x9, recvfrombuf@PAGEOFF
    ldrb w10, [x9]
    cmp w10, #'x'
    b.ne fail

    mov x0, x23
    adrp x1, sockreply@PAGE
    add x1, x1, sockreply@PAGEOFF
    mov x2, #4
    mov x16, #4
    svc #0x80
    b.cs fail
    cmp x0, #4
    b.ne fail

    mov x0, x22
    adrp x1, recvmsg_x_hdr@PAGE
    add x1, x1, recvmsg_x_hdr@PAGEOFF
    mov x2, #1
    mov x3, #0
    mov x16, #480
    svc #0x80
    b.cs fail
    cmp x0, #1
    b.ne fail
    adrp x9, recvmsg_x_hdr@PAGE
    add x9, x9, recvmsg_x_hdr@PAGEOFF
    ldr x10, [x9, #48]
    cmp x10, #4
    b.ne fail
    adrp x9, recvmsg_xbuf@PAGE
    add x9, x9, recvmsg_xbuf@PAGEOFF
    ldrb w10, [x9]
    cmp w10, #'p'
    b.ne fail

    mov x0, #0
    mov x1, #2
    mov x16, #436
    svc #0x80
    b.cs fail

    mov x0, #-1
    mov x1, #0
    mov x2, #0
    mov x16, #448
    svc #0x80
    b.cc fail
    cmp x0, #9
    b.ne fail

    mov x0, #-1
    mov x1, #0
    mov x16, #449
    svc #0x80
    b.cc fail
    cmp x0, #45
    b.ne fail

    mov x0, x22
    mov x16, #6
    svc #0x80
    b.cs fail
    mov x0, x23
    mov x16, #6
    svc #0x80
    b.cs fail

    adrp x0, sockpath400@PAGE
    add x0, x0, sockpath400@PAGEOFF
    mov x16, #10
    svc #0x80

    mov x0, #1
    mov x1, #1
    mov x2, #0
    mov x16, #97
    svc #0x80
    b.cs fail
    mov x24, x0

    mov x0, x24
    adrp x1, sock_addr400@PAGE
    add x1, x1, sock_addr400@PAGEOFF
    mov x2, #17
    mov x16, #104
    svc #0x80
    b.cs fail

    mov x0, x24
    mov x1, #4
    mov x16, #106
    svc #0x80
    b.cs fail

    mov x0, #1
    mov x1, #1
    mov x2, #0
    mov x3, #0
    mov x16, #450
    svc #0x80
    b.cs fail
    mov x25, x0

    mov x0, x25
    adrp x1, connectx_endpoints400@PAGE
    add x1, x1, connectx_endpoints400@PAGEOFF
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x5, #0
    mov x6, #0
    mov x7, #0
    mov x16, #447
    svc #0x80
    b.cs fail

    mov x0, x24
    adrp x1, accepted_addr400@PAGE
    add x1, x1, accepted_addr400@PAGEOFF
    adrp x2, accepted_len400@PAGE
    add x2, x2, accepted_len400@PAGEOFF
    mov x16, #404
    svc #0x80
    b.cs fail
    mov x26, x0

    mov x0, x26
    mov x16, #6
    svc #0x80
    b.cs fail
    mov x0, x25
    mov x16, #6
    svc #0x80
    b.cs fail

    mov x0, #1
    mov x1, #1
    mov x2, #0
    mov x16, #97
    svc #0x80
    b.cs fail
    mov x25, x0

    mov x0, x25
    adrp x1, sock_addr400@PAGE
    add x1, x1, sock_addr400@PAGEOFF
    mov x2, #17
    mov x16, #409
    svc #0x80
    b.cs fail

    mov x0, x24
    adrp x1, accepted_addr400@PAGE
    add x1, x1, accepted_addr400@PAGEOFF
    adrp x2, accepted_len400@PAGE
    add x2, x2, accepted_len400@PAGEOFF
    mov x16, #404
    svc #0x80
    b.cs fail
    mov x26, x0

    mov x0, x26
    mov x16, #6
    svc #0x80
    b.cs fail
    mov x0, x25
    mov x16, #6
    svc #0x80
    b.cs fail
    mov x0, x24
    mov x16, #6
    svc #0x80
    b.cs fail
    adrp x0, sockpath400@PAGE
    add x0, x0, sockpath400@PAGEOFF
    mov x16, #10
    svc #0x80
    b.cs fail

    mov x0, #1
    adrp x1, sigusr2_set400@PAGE
    add x1, x1, sigusr2_set400@PAGEOFF
    adrp x2, old_sigmask400@PAGE
    add x2, x2, old_sigmask400@PAGEOFF
    mov x16, #48
    svc #0x80
    b.cs fail

    mov x16, #20
    svc #0x80
    b.cs fail

    mov x1, #31
    mov x16, #37
    svc #0x80
    b.cs fail

    adrp x0, sigusr2_set400@PAGE
    add x0, x0, sigusr2_set400@PAGEOFF
    adrp x1, sigwait_out400@PAGE
    add x1, x1, sigwait_out400@PAGEOFF
    mov x16, #422
    svc #0x80
    b.cs fail
    adrp x9, sigwait_out400@PAGE
    add x9, x9, sigwait_out400@PAGEOFF
    ldr w10, [x9]
    cmp w10, #31
    b.ne fail

    mov x0, #3
    adrp x1, old_sigmask400@PAGE
    add x1, x1, old_sigmask400@PAGEOFF
    mov x2, #0
    mov x16, #48
    svc #0x80
    b.cs fail

    mov x0, #0
    mov x16, #410
    svc #0x80
    b.cc fail
    cmp x0, #4
    b.ne fail

    mov x0, #1
    mov x1, #0
    adrp x2, workloop_params400@PAGE
    add x2, x2, workloop_params400@PAGEOFF
    mov x3, #36
    mov x16, #530
    svc #0x80
    b.cs fail

    mov x0, #1
    mov x1, #0
    adrp x2, workloop_params400@PAGE
    add x2, x2, workloop_params400@PAGEOFF
    mov x3, #36
    mov x16, #530
    svc #0x80
    b.cc fail
    cmp x0, #17
    b.ne fail

    mov x0, #2
    mov x1, #0
    adrp x2, workloop_params400@PAGE
    add x2, x2, workloop_params400@PAGEOFF
    mov x3, #36
    mov x16, #530
    svc #0x80
    b.cs fail

    mov x0, #2
    mov x1, #0
    adrp x2, workloop_params400@PAGE
    add x2, x2, workloop_params400@PAGEOFF
    mov x3, #36
    mov x16, #530
    svc #0x80
    b.cc fail
    cmp x0, #22
    b.ne fail

    mov x0, #1
    mov x1, #0
    adrp x2, workloop_params400@PAGE
    add x2, x2, workloop_params400@PAGEOFF
    mov x3, #0
    mov x16, #530
    svc #0x80
    b.cc fail
    cmp x0, #22
    b.ne fail

    mov x0, #0x1234
    mov x16, #531       ; Darwin SYS___mach_bridge_remote_time
    svc #0x80
    b.cs fail
    mov x9, #0x1234
    cmp x0, x9
    b.ne fail

    mov x0, #1
    adrp x1, coalition_id400@PAGE
    add x1, x1, coalition_id400@PAGEOFF
    adrp x2, coalition_buf400@PAGE
    add x2, x2, coalition_buf400@PAGEOFF
    adrp x3, coalition_bufsize400@PAGE
    add x3, x3, coalition_bufsize400@PAGEOFF
    mov x16, #532
    svc #0x80
    b.cs fail

    mov x0, #0
    mov x1, #0
    adrp x2, log_buf400@PAGE
    add x2, x2, log_buf400@PAGEOFF
    mov x3, #16
    mov x16, #533
    svc #0x80
    b.cs fail

    mov x0, #0
    mov x1, #0
    mov x16, #535
    svc #0x80
    b.cs fail

    mov x16, #20        ; Darwin SYS_getpid
    svc #0x80
    b.cs fail
    mov x1, x0
    mov x0, #0
    adrp x2, task_port_out@PAGE
    add x2, x2, task_port_out@PAGEOFF
    mov x16, #538       ; Darwin SYS_task_inspect_for_pid
    svc #0x80
    b.cc fail
    cmp x0, #1
    b.ne fail
    adrp x9, task_port_out@PAGE
    add x9, x9, task_port_out@PAGEOFF
    ldr w10, [x9]
    cbnz w10, fail

    mov x16, #20        ; Darwin SYS_getpid
    svc #0x80
    b.cs fail
    mov x1, x0
    mov x0, #0
    adrp x2, task_port_out@PAGE
    add x2, x2, task_port_out@PAGEOFF
    mov x16, #539       ; Darwin SYS_task_read_for_pid
    svc #0x80
    b.cc fail
    cmp x0, #1
    b.ne fail

    mov x0, #0
    mov x1, #0
    adrp x2, necp_result400@PAGE
    add x2, x2, necp_result400@PAGEOFF
    mov x16, #460
    svc #0x80
    b.cs fail

    mov x0, #0
    mov x16, #501
    svc #0x80
    b.cs fail
    mov x24, x0

    mov x0, x24
    mov x1, #5
    mov x2, #0
    mov x3, #0
    adrp x4, necp_result400@PAGE
    add x4, x4, necp_result400@PAGEOFF
    mov x5, #512
    mov x16, #502
    svc #0x80
    b.cc fail
    cmp x0, #45
    b.ne fail

    mov x0, x24
    mov x16, #6
    svc #0x80
    b.cs fail

    mov x16, #503
    svc #0x80
    b.cc fail
    cmp x0, #45
    b.ne fail

    mov x16, #504
    svc #0x80
    b.cc fail
    cmp x0, #45
    b.ne fail

    mov x16, #505
    svc #0x80
    b.cc fail
    cmp x0, #45
    b.ne fail

    mov x16, #506
    svc #0x80
    b.cc fail
    cmp x0, #45
    b.ne fail

    mov x16, #507
    svc #0x80
    b.cc fail
    cmp x0, #45
    b.ne fail

    mov x16, #508
    svc #0x80
    b.cc fail
    cmp x0, #45
    b.ne fail

    mov x16, #509
    svc #0x80
    b.cc fail
    cmp x0, #45
    b.ne fail

    mov x16, #510
    svc #0x80
    b.cc fail
    cmp x0, #45
    b.ne fail

    mov x16, #511
    svc #0x80
    b.cc fail
    cmp x0, #45
    b.ne fail

    mov x16, #512
    svc #0x80
    b.cc fail
    cmp x0, #45
    b.ne fail

    mov x16, #513
    svc #0x80
    b.cc fail
    cmp x0, #45
    b.ne fail

    mov x16, #514
    svc #0x80
    b.cc fail
    cmp x0, #45
    b.ne fail

    mov x0, #0
    mov x16, #522
    svc #0x80
    b.cs fail
    mov x24, x0

    mov x0, x24
    mov x1, #5
    mov x2, #0
    mov x3, #0
    adrp x4, necp_result400@PAGE
    add x4, x4, necp_result400@PAGEOFF
    mov x5, #512
    mov x16, #523
    svc #0x80
    b.cc fail
    cmp x0, #45
    b.ne fail

    mov x0, x24
    mov x16, #6
    svc #0x80
    b.cs fail

    adrp x0, netqos400@PAGE
    add x0, x0, netqos400@PAGEOFF
    mov x1, #16
    mov x16, #525
    svc #0x80
    b.cs fail
    cbnz x0, fail

    mov x0, #3
    adrp x1, tracker_buf400@PAGE
    add x1, x1, tracker_buf400@PAGEOFF
    mov x2, #64
    mov x16, #546
    svc #0x80
    b.cs fail

    mov x0, #0
    mov x16, #547
    svc #0x80
    b.cs fail

    mov x0, #0
    mov x1, #0
    mov x2, #0
    mov x16, #548
    svc #0x80
    b.cs fail

    mov x0, #0
    mov x1, #0
    mov x2, #0
    mov x3, #0
    mov x16, #550
    svc #0x80
    b.cs fail

    mov x0, #0
    mov x1, #0
    mov x2, #0
    mov x3, #0
    mov x16, #552
    svc #0x80
    b.cs fail

    adrp x9, coalition_id400@PAGE
    add x9, x9, coalition_id400@PAGEOFF
    ldr x0, [x9]
    mov x1, #0
    mov x2, #0
    mov x16, #556
    svc #0x80
    b.cs fail

    adrp x9, coalition_id400@PAGE
    add x9, x9, coalition_id400@PAGEOFF
    ldr x0, [x9]
    mov x1, #0
    mov x16, #557
    svc #0x80
    b.cs fail
    cbnz x0, fail

    mov x0, x19
    mov x16, #6
    svc #0x80
    b.cs fail

    mov x0, #0
    mov x16, #1
    svc #0x80

fail:
    mov x0, #1
    mov x16, #1
    svc #0x80

.section __TEXT,__cstring
path:
    .asciz "syscall_data.txt"
linkpath:
    .asciz "syscall_range_400_plus.link"
rename_src:
    .asciz "range_400_rename_src.tmp"
rename_dst:
    .asciz "range_400_rename_dst.tmp"
rename_excl_src:
	.asciz "range_400_rename_excl_src.tmp"
clonefileat_dst400:
	.asciz "range_400_clonefileat.tmp"
fclonefileat_dst400:
	.asciz "range_400_fclonefileat.tmp"
guarded_path:
    .asciz "range_400_guarded.tmp"
guarded_dprotected_path:
    .asciz "range_400_guarded_dprotected.tmp"
dotpath400:
	.asciz "."
missing_path400:
	.asciz "range_400_missing"
mount_type400:
	.asciz "apfs"
snapshot_name400:
	.asciz "machgate_snapshot"
ok:
	.asciz "range400 ok\n"
sockmsg:
    .asciz "sock"
sockreply:
    .asciz "pong"
sockncan:
    .asciz "ncan"
sockxmsg:
    .asciz "xmsg"
guarded_msg1:
    .asciz "ABCD"
guarded_msg2:
    .asciz "xy"
guarded_msg3:
    .asciz "I"
guarded_iov_a:
    .asciz "EF"
guarded_iov_b:
    .asciz "GH"
sockpath400:
    .asciz "range_400.sock"
sem_name:
    .asciz "/machgate_range_400_sem"
sock_addr400:
    .byte 17
    .byte 1
    .asciz "range_400.sock"

.section __DATA,__data
.p2align 3
connectx_endpoints400:
    .long 0
    .long 0
    .quad 0
    .long 0
    .long 0
    .quad sock_addr400
    .long 17
    .long 0
iov:
    .quad ok
    .quad 9
    .quad readbuf
    .quad 4
guarded_iov:
    .quad guarded_iov_a
    .quad 2
    .quad guarded_iov_b
    .quad 2

readbuf:
    .space 8
guarded_readbuf:
    .space 16

entropy:
    .space 16

statbuf:
    .space 144

timeout:
    .quad 0
    .long 0
    .long 0
wait_status:
    .long 0
waitid_info:
    .space 128

linkbuf:
    .space 32

aio_read_cb:
    .space 80
aio_list:
    .quad 0
aio_readbuf:
    .space 8

ntptimebuf:
    .space 48

timexbuf:
    .space 136

sockfds:
    .space 8
sendmsg_hdr:
    .quad 0
    .long 0
    .long 0
    .quad send_iov
    .long 1
    .long 0
    .quad 0
    .long 0
    .long 0
recvmsg_hdr:
    .quad 0
    .long 0
    .long 0
    .quad recv_iov
    .long 1
    .long 0
    .quad 0
    .long 0
    .long 0
send_iov:
    .quad sockmsg
    .quad 4
recv_iov:
    .quad recvmsgbuf
    .quad 4
sendmsg_x_hdr:
    .quad 0
    .long 0
    .long 0
    .quad send_x_iov
    .long 1
    .long 0
    .quad 0
    .long 0
    .long 0
    .quad 0
recvmsg_x_hdr:
    .quad 0
    .long 0
    .long 0
    .quad recv_x_iov
    .long 1
    .long 0
    .quad 0
    .long 0
    .long 0
    .quad 0
send_x_iov:
    .quad sockxmsg
    .quad 4
recv_x_iov:
    .quad recvmsg_xbuf
    .quad 4
recvfrombuf:
    .space 8
recvmsgbuf:
    .space 8
recvmsg_xbuf:
    .space 8
sysv_send_msg:
    .quad 1
    .ascii "nmsg"
    .space 4
sysv_recv_msg:
    .space 16
accepted_addr400:
    .space 128
accepted_len400:
    .long 128

memlevel:
    .long 0

ulock_word:
    .long 0
proc_shortinfo_ext:
    .space 64
necp_result400:
    .space 512
netqos400:
    .quad 0
    .long 0
    .long 0
tracker_buf400:
	.space 64
csr_config400:
	.long 0
uuid400:
	.space 16
.p2align 3
sfi_time400:
	.quad 0
sfi_flags400:
	.long 0
.p2align 3
coalition_id400:
	.quad 0
coalition_buf400:
	.space 64
coalition_bufsize400:
	.quad 64
stackshot_config400:
	.space 16
microstackshot_buf400:
	.space 64
pgo_buf400:
	.space 64
	persona_id400:
		.long 0
	log_buf400:
		.space 16
	.p2align 3
	kas_value400:
		.quad 0x123456789abcdef0
	kas_size400:
		.quad 8
	sigusr2_set400:
		.long 0x40000000
old_sigmask400:
	.long 0
sigwait_out400:
    .long 0
.p2align 3
workloop_params400:
    .long 36
    .long 1
    .quad 0x400530
    .long 10
    .long 0
    .long 0
    .long 0
    .long 0
audit_session_port_name:
    .long 0
audit_session_port_out:
    .long 0
fileport_name:
    .long 0
task_port_out:
    .long 0
fdflags_save:
    .long 1
guard_a:
    .quad 0x1122334455667788
guard_b:
    .quad 0x8877665544332211
guard_bad:
    .quad 0x0102030405060708
mac_label400:
    .quad 8
    .quad mac_label400_buf
mac_label400_buf:
    .space 8
fsid_buf:
    .space 8
fileid_buf400:
	.space 8
fsobj_id400:
	.space 8
fsgetpath_buf:
    .space 4096
attrlist_file400:
	.hword 5
	.hword 0
	.long 0x8200000c
	.long 0
	.long 0
	.long 0x00000200
	.long 0
attrlist_bulk400:
	.hword 5
	.hword 0
	.long 0x80000001
	.long 0
	.long 0
	.long 0x00000200
	.long 0
attrlist_set_mode400:
	.hword 5
	.hword 0
	.long 0x00020000
	.long 0
	.long 0
	.long 0
	.long 0
mode_0600_400:
	.long 0600
attrlist_buf400:
	.space 512
attrlist_bulk_buf400:
	.space 1024
openbyid_buf400:
	.space 8
