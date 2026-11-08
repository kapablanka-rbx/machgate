; darwin_range_200_399.s - representative raw Darwin syscalls in range 200-399
.section __TEXT,__text
.globl _main
.p2align 2
_main:
    adrp x0, path@PAGE
    add x0, x0, path@PAGEOFF
    mov x1, #0x602      ; Darwin O_RDWR | O_CREAT | O_TRUNC
    mov x2, #0644
    mov x16, #398       ; Darwin SYS_open_nocancel
    svc #0x80
    b.cs fail
    mov x19, x0

    mov x0, x19
    mov x1, #5
    mov x16, #201       ; Darwin SYS_ftruncate
    svc #0x80
    b.cs fail

    mov x0, x19
    adrp x1, message@PAGE
    add x1, x1, message@PAGEOFF
    mov x2, #5
    mov x16, #397       ; Darwin SYS_write_nocancel
    svc #0x80
    b.cs fail
    cmp x0, #5
    b.ne fail

    mov x0, x19
    adrp x1, statbuf2@PAGE
    add x1, x1, statbuf2@PAGEOFF
    mov x2, #0
    adrp x3, xsecurity_size@PAGE
    add x3, x3, xsecurity_size@PAGEOFF
    mov x16, #343       ; Darwin SYS_fstat64_extended
    svc #0x80
    b.cs fail
    adrp x9, xsecurity_size@PAGE
    add x9, x9, xsecurity_size@PAGEOFF
    ldr x10, [x9]
    cbnz x10, fail

    mov x0, x19
    mov x1, #-1
    mov x2, #-1
    mov x3, #0644
    mov x4, #0
    mov x16, #283       ; Darwin SYS_fchmod_extended
    svc #0x80
    b.cs fail

    mov x0, x19
    adrp x1, attrname@PAGE
    add x1, x1, attrname@PAGEOFF
    adrp x2, attrvalue@PAGE
    add x2, x2, attrvalue@PAGEOFF
    mov x3, #2
    mov x4, #0
    mov x5, #0
    mov x16, #237       ; Darwin SYS_fsetxattr
    svc #0x80
    b.cs fail

    mov x0, x19
    adrp x1, attrname@PAGE
    add x1, x1, attrname@PAGEOFF
    adrp x2, attrbuf@PAGE
    add x2, x2, attrbuf@PAGEOFF
    mov x3, #2
    mov x4, #0
    mov x5, #0
    mov x16, #235       ; Darwin SYS_fgetxattr
    svc #0x80
    b.cs fail
    cmp x0, #2
    b.ne fail
    adrp x9, attrbuf@PAGE
    add x9, x9, attrbuf@PAGEOFF
    ldrb w10, [x9]
    cmp w10, #'o'
    b.ne fail
    ldrb w10, [x9, #1]
    cmp w10, #'k'
    b.ne fail

    mov x0, x19
    adrp x1, attrname@PAGE
    add x1, x1, attrname@PAGEOFF
    mov x2, #0
    mov x16, #239       ; Darwin SYS_fremovexattr
    svc #0x80
    b.cs fail

    adrp x0, hw_ncpu_mib@PAGE
    add x0, x0, hw_ncpu_mib@PAGEOFF
    mov x1, #2
    adrp x2, sysctl_intbuf@PAGE
    add x2, x2, sysctl_intbuf@PAGEOFF
    adrp x3, sysctl_int_len@PAGE
    add x3, x3, sysctl_int_len@PAGEOFF
    mov x4, #0
    mov x5, #0
    mov x16, #202       ; Darwin SYS_sysctl
    svc #0x80
    b.cs fail
    adrp x9, sysctl_intbuf@PAGE
    add x9, x9, sysctl_intbuf@PAGEOFF
    ldr w10, [x9]
    cbz w10, fail

    adrp x0, hw_memsize_name@PAGE
    add x0, x0, hw_memsize_name@PAGEOFF
    mov x1, #10
    adrp x2, sysctl_u64buf@PAGE
    add x2, x2, sysctl_u64buf@PAGEOFF
    adrp x3, sysctl_u64_len@PAGE
    add x3, x3, sysctl_u64_len@PAGEOFF
    mov x4, #0
    mov x5, #0
    mov x16, #274       ; Darwin SYS_sys_sysctlbyname
    svc #0x80
    b.cs fail
    adrp x9, sysctl_u64buf@PAGE
    add x9, x9, sysctl_u64buf@PAGEOFF
    ldr x10, [x9]
    cbz x10, fail

    adrp x0, spawn_pid@PAGE
    add x0, x0, spawn_pid@PAGEOFF
    adrp x1, path@PAGE
    add x1, x1, path@PAGEOFF
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x16, #244       ; Darwin SYS_posix_spawn
    svc #0x80
    b.cc fail
    cmp x0, #45
    b.ne fail

    mov x0, #0          ; Darwin KAUTH_EXTLOOKUP_REGISTER
    mov x1, #0
    mov x16, #293       ; Darwin SYS_identitysvc
    svc #0x80
    b.cs fail

    mov x0, #8          ; Darwin KAUTH_GET_CACHE_SIZES
    adrp x1, identity_cache_sizes@PAGE
    add x1, x1, identity_cache_sizes@PAGEOFF
    mov x16, #293       ; Darwin SYS_identitysvc
    svc #0x80
    b.cs fail
    adrp x9, identity_cache_sizes@PAGE
    add x9, x9, identity_cache_sizes@PAGEOFF
    ldr w10, [x9]
    cbnz w10, fail
    ldr w10, [x9, #4]
    cbnz w10, fail

    mov x0, #2          ; Darwin KAUTH_EXTLOOKUP_WORKER
    adrp x1, identity_cache_sizes@PAGE
    add x1, x1, identity_cache_sizes@PAGEOFF
    mov x16, #293       ; Darwin SYS_identitysvc
    svc #0x80
    b.cc fail
    cmp x0, #45
    b.ne fail

    adrp x0, path@PAGE
    add x0, x0, path@PAGEOFF
    mov x1, #0
    mov x2, #0
    mov x3, #0
    mov x16, #380       ; Darwin SYS___mac_execve
    svc #0x80
    b.cc fail
    cmp x0, #45
    b.ne fail

    mov x16, #20        ; Darwin SYS_getpid
    svc #0x80
    b.cs fail
    mov x27, x0

    mov x0, #1
    mov x1, #1
    mov x2, #0
    mov x3, #0
    adrp x4, proc_pid_list@PAGE
    add x4, x4, proc_pid_list@PAGEOFF
    mov x5, #4
    mov x16, #336       ; Darwin SYS_proc_info
    svc #0x80
    b.cs fail
    cmp x0, #4
    b.ne fail
    adrp x9, proc_pid_list@PAGE
    add x9, x9, proc_pid_list@PAGEOFF
    ldr w10, [x9]
    cbz w10, fail

    mov x0, #2
    mov x1, x27
    mov x2, #13
    mov x3, #0
    adrp x4, proc_shortinfo@PAGE
    add x4, x4, proc_shortinfo@PAGEOFF
    mov x5, #64
    mov x16, #336       ; Darwin SYS_proc_info
    svc #0x80
    b.cs fail
    cmp x0, #64
    b.ne fail
    adrp x9, proc_shortinfo@PAGE
    add x9, x9, proc_shortinfo@PAGEOFF
    ldr w10, [x9]
    cmp x10, x27
    b.ne fail

    adrp x9, aio_write_cb@PAGE
    add x9, x9, aio_write_cb@PAGEOFF
    str w19, [x9]
    str xzr, [x9, #8]
    adrp x10, message@PAGE
    add x10, x10, message@PAGEOFF
    str x10, [x9, #16]
    mov x10, #5
    str x10, [x9, #24]
    str wzr, [x9, #32]
    str wzr, [x9, #72]

    mov x0, x9
    mov x16, #319       ; Darwin SYS_aio_write
    svc #0x80
    b.cs fail

    adrp x0, aio_write_cb@PAGE
    add x0, x0, aio_write_cb@PAGEOFF
    mov x16, #317       ; Darwin SYS_aio_error
    svc #0x80
    b.cs fail
    cbnz x0, fail

    adrp x0, aio_write_cb@PAGE
    add x0, x0, aio_write_cb@PAGEOFF
    mov x16, #314       ; Darwin SYS_aio_return
    svc #0x80
    b.cs fail
    cmp x0, #5
    b.ne fail

    adrp x9, aio_read_cb@PAGE
    add x9, x9, aio_read_cb@PAGEOFF
    str w19, [x9]
    str xzr, [x9, #8]
    adrp x10, aio_readbuf@PAGE
    add x10, x10, aio_readbuf@PAGEOFF
    str x10, [x9, #16]
    mov x10, #5
    str x10, [x9, #24]
    str wzr, [x9, #32]
    str wzr, [x9, #72]

    mov x0, x9
    mov x16, #318       ; Darwin SYS_aio_read
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
    mov x16, #315       ; Darwin SYS_aio_suspend
    svc #0x80
    b.cs fail

    adrp x0, aio_list@PAGE
    add x0, x0, aio_list@PAGEOFF
    mov x1, #1
    mov x2, #0
    mov x16, #421       ; Darwin SYS_aio_suspend_nocancel
    svc #0x80
    b.cs fail

    adrp x0, aio_read_cb@PAGE
    add x0, x0, aio_read_cb@PAGEOFF
    mov x16, #317       ; Darwin SYS_aio_error
    svc #0x80
    b.cs fail
    cbnz x0, fail

    adrp x0, aio_read_cb@PAGE
    add x0, x0, aio_read_cb@PAGEOFF
    mov x16, #314       ; Darwin SYS_aio_return
    svc #0x80
    b.cs fail
    cmp x0, #5
    b.ne fail
    adrp x9, aio_readbuf@PAGE
    add x9, x9, aio_readbuf@PAGEOFF
    ldrb w10, [x9]
    cmp w10, #'r'
    b.ne fail
    ldrb w10, [x9, #4]
    cmp w10, #'e'
    b.ne fail

    mov x0, #0
    adrp x1, aio_write_cb@PAGE
    add x1, x1, aio_write_cb@PAGEOFF
    mov x16, #313       ; Darwin SYS_aio_fsync
    svc #0x80
    b.cs fail

    mov x0, x19
    adrp x1, aio_write_cb@PAGE
    add x1, x1, aio_write_cb@PAGEOFF
    mov x16, #316       ; Darwin SYS_aio_cancel
    svc #0x80
    b.cs fail
    cmp x0, #1
    b.ne fail

    adrp x9, aio_lio_cb@PAGE
    add x9, x9, aio_lio_cb@PAGEOFF
    str w19, [x9]
    str xzr, [x9, #8]
    adrp x10, message@PAGE
    add x10, x10, message@PAGEOFF
    str x10, [x9, #16]
    mov x10, #5
    str x10, [x9, #24]
    str wzr, [x9, #32]
    mov w10, #2
    str w10, [x9, #72]
    adrp x10, aio_list@PAGE
    add x10, x10, aio_list@PAGEOFF
    str x9, [x10]
    mov x0, #2
    mov x1, x10
    mov x2, #1
    mov x3, #0
    mov x16, #320       ; Darwin SYS_lio_listio
    svc #0x80
    b.cs fail

    mov x0, x19
    mov x16, #399       ; Darwin SYS_close_nocancel
    svc #0x80
    b.cs fail

    adrp x0, protected_path@PAGE
    add x0, x0, protected_path@PAGEOFF
    mov x1, #0x601
    mov x2, #0
    mov x3, #0
    mov x4, #0644
    mov x16, #216       ; Darwin SYS_open_dprotected_np
    svc #0x80
    b.cs fail
    mov x20, x0

    mov x0, x20
    mov x16, #399       ; Darwin SYS_close_nocancel
    svc #0x80
    b.cs fail

    adrp x0, protected_path@PAGE
    add x0, x0, protected_path@PAGEOFF
    adrp x1, statbuf2@PAGE
    add x1, x1, statbuf2@PAGEOFF
    mov x2, #0
    adrp x3, xsecurity_size@PAGE
    add x3, x3, xsecurity_size@PAGEOFF
    mov x16, #341       ; Darwin SYS_stat64_extended
    svc #0x80
    b.cs fail

    adrp x0, protected_path@PAGE
    add x0, x0, protected_path@PAGEOFF
    mov x16, #226       ; Darwin SYS_delete
    svc #0x80
    b.cs fail

    adrp x0, path@PAGE
    add x0, x0, path@PAGEOFF
    adrp x1, statbuf@PAGE
    add x1, x1, statbuf@PAGEOFF
    mov x16, #338       ; Darwin SYS_stat64
    svc #0x80
    b.cs fail

    adrp x0, pollfds@PAGE
    add x0, x0, pollfds@PAGEOFF
    mov x1, #0
    mov x2, #0
    mov x16, #230       ; Darwin SYS_poll
    svc #0x80
    b.cs fail

    adrp x0, path@PAGE
    add x0, x0, path@PAGEOFF
    adrp x1, attrlist_file@PAGE
    add x1, x1, attrlist_file@PAGEOFF
    adrp x2, attrlist_buf@PAGE
    add x2, x2, attrlist_buf@PAGEOFF
    mov x3, #512
    mov x4, #0
    mov x16, #220       ; Darwin SYS_getattrlist
    svc #0x80
    b.cs fail
    adrp x9, attrlist_buf@PAGE
    add x9, x9, attrlist_buf@PAGEOFF
    ldr w10, [x9]
    cmp w10, #88
    b.lt fail
    ldr w10, [x9, #40]
    cmp w10, #1
    b.ne fail
    ldr x10, [x9, #80]
    cmp x10, #5
    b.ne fail
    adrp x11, fsid_buf@PAGE
    add x11, x11, fsid_buf@PAGEOFF
    ldr x10, [x9, #32]
    str x10, [x11]
    adrp x11, fileid_buf@PAGE
    add x11, x11, fileid_buf@PAGEOFF
    ldr x10, [x9, #72]
    str x10, [x11]

    adrp x0, path@PAGE
    add x0, x0, path@PAGEOFF
    mov x1, #0
    mov x2, #0
    mov x16, #398       ; Darwin SYS_open_nocancel
    svc #0x80
    b.cs fail
    mov x20, x0

    mov x0, x20
    adrp x1, attrlist_file@PAGE
    add x1, x1, attrlist_file@PAGEOFF
    adrp x2, attrlist_buf2@PAGE
    add x2, x2, attrlist_buf2@PAGEOFF
    mov x3, #512
    mov x4, #0
    mov x16, #228       ; Darwin SYS_fgetattrlist
    svc #0x80
    b.cs fail
    adrp x9, attrlist_buf2@PAGE
    add x9, x9, attrlist_buf2@PAGEOFF
    ldr x10, [x9, #80]
    cmp x10, #5
    b.ne fail

    adrp x0, path@PAGE
    add x0, x0, path@PAGEOFF
    adrp x1, attrlist_set_mode@PAGE
    add x1, x1, attrlist_set_mode@PAGEOFF
    adrp x2, mode_0600@PAGE
    add x2, x2, mode_0600@PAGEOFF
    mov x3, #4
    mov x4, #0
    mov x16, #221       ; Darwin SYS_setattrlist
    svc #0x80
    b.cs fail

    mov x0, x20
    adrp x1, attrlist_set_mode@PAGE
    add x1, x1, attrlist_set_mode@PAGEOFF
    adrp x2, mode_0644@PAGE
    add x2, x2, mode_0644@PAGEOFF
    mov x3, #4
    mov x4, #0
    mov x16, #229       ; Darwin SYS_fsetattrlist
    svc #0x80
    b.cs fail

    mov x0, x20
    mov x16, #399       ; Darwin SYS_close_nocancel
    svc #0x80
    b.cs fail

    adrp x0, root_path@PAGE
    add x0, x0, root_path@PAGEOFF
    mov x1, #0
    mov x2, #0
    mov x16, #398       ; Darwin SYS_open_nocancel
    svc #0x80
    b.cs fail
    mov x20, x0

    mov x0, x20
    adrp x1, attrlist_dir@PAGE
    add x1, x1, attrlist_dir@PAGEOFF
    adrp x2, dirattrbuf@PAGE
    add x2, x2, dirattrbuf@PAGEOFF
    mov x3, #4096
    adrp x4, dirattr_count@PAGE
    add x4, x4, dirattr_count@PAGEOFF
    adrp x5, dirattr_base@PAGE
    add x5, x5, dirattr_base@PAGEOFF
    adrp x6, dirattr_state@PAGE
    add x6, x6, dirattr_state@PAGEOFF
    mov x7, #0
    mov x16, #222       ; Darwin SYS_getdirentriesattr
    svc #0x80
    b.cs fail
    adrp x9, dirattr_count@PAGE
    add x9, x9, dirattr_count@PAGEOFF
    ldr w10, [x9]
    cbz w10, fail
    adrp x9, dirattrbuf@PAGE
    add x9, x9, dirattrbuf@PAGEOFF
    ldr w10, [x9]
    cbz w10, fail

    mov x0, x20
    mov x16, #399       ; Darwin SYS_close_nocancel
    svc #0x80
    b.cs fail

    adrp x0, path@PAGE
    add x0, x0, path@PAGEOFF
    adrp x1, copyfile_path@PAGE
    add x1, x1, copyfile_path@PAGEOFF
    mov x2, #0600
    mov x3, #1
    mov x16, #227       ; Darwin SYS_copyfile
    svc #0x80
    b.cs fail

    adrp x0, exchange_path_a@PAGE
    add x0, x0, exchange_path_a@PAGEOFF
    mov x1, #0x602
    mov x2, #0600
    mov x16, #398       ; Darwin SYS_open_nocancel
    svc #0x80
    b.cs fail
    mov x20, x0

    mov x0, x20
    adrp x1, exchange_left@PAGE
    add x1, x1, exchange_left@PAGEOFF
    mov x2, #5
    mov x16, #397       ; Darwin SYS_write_nocancel
    svc #0x80
    b.cs fail
    cmp x0, #5
    b.ne fail

    mov x0, x20
    mov x16, #399       ; Darwin SYS_close_nocancel
    svc #0x80
    b.cs fail

    adrp x0, exchange_path_b@PAGE
    add x0, x0, exchange_path_b@PAGEOFF
    mov x1, #0x602
    mov x2, #0600
    mov x16, #398       ; Darwin SYS_open_nocancel
    svc #0x80
    b.cs fail
    mov x20, x0

    mov x0, x20
    adrp x1, exchange_right@PAGE
    add x1, x1, exchange_right@PAGEOFF
    mov x2, #5
    mov x16, #397       ; Darwin SYS_write_nocancel
    svc #0x80
    b.cs fail
    cmp x0, #5
    b.ne fail

    mov x0, x20
    mov x16, #399       ; Darwin SYS_close_nocancel
    svc #0x80
    b.cs fail

    adrp x0, exchange_path_a@PAGE
    add x0, x0, exchange_path_a@PAGEOFF
    adrp x1, exchange_path_b@PAGE
    add x1, x1, exchange_path_b@PAGEOFF
    mov x2, #0
    mov x16, #223       ; Darwin SYS_exchangedata
    svc #0x80
    b.cs fail

    adrp x0, exchange_path_a@PAGE
    add x0, x0, exchange_path_a@PAGEOFF
    mov x1, #0
    mov x2, #0
    mov x16, #398       ; Darwin SYS_open_nocancel
    svc #0x80
    b.cs fail
    mov x20, x0

    mov x0, x20
    adrp x1, exchange_buf@PAGE
    add x1, x1, exchange_buf@PAGEOFF
    mov x2, #5
    mov x16, #396       ; Darwin SYS_read_nocancel
    svc #0x80
    b.cs fail
    cmp x0, #5
    b.ne fail

    mov x0, x20
    mov x16, #399       ; Darwin SYS_close_nocancel
    svc #0x80
    b.cs fail

    adrp x9, exchange_buf@PAGE
    add x9, x9, exchange_buf@PAGEOFF
    ldrb w10, [x9]
    cmp w10, #'r'
    b.ne fail
    ldrb w10, [x9, #4]
    cmp w10, #'t'
    b.ne fail

    adrp x0, exchange_path_a@PAGE
    add x0, x0, exchange_path_a@PAGEOFF
    mov x16, #205       ; Darwin SYS_undelete existing non-whiteout
    svc #0x80
    b.cc fail
    cmp x0, #17
    b.ne fail

    adrp x0, current_dir@PAGE
    add x0, x0, current_dir@PAGEOFF
    adrp x1, searchfs_block@PAGE
    add x1, x1, searchfs_block@PAGEOFF
    adrp x2, searchfs_match_count@PAGE
    add x2, x2, searchfs_match_count@PAGEOFF
    mov x3, #0
    mov x4, #9          ; SRCHFS_START | SRCHFS_MATCHFILES
    adrp x5, searchfs_state@PAGE
    add x5, x5, searchfs_state@PAGEOFF
    mov x16, #225       ; Darwin SYS_searchfs
    svc #0x80
    b.cs fail
    adrp x9, searchfs_match_count@PAGE
    add x9, x9, searchfs_match_count@PAGEOFF
    ldr x10, [x9]
    cmp x10, #1
    b.ne fail
    adrp x9, searchfs_result_buf@PAGE
    add x9, x9, searchfs_result_buf@PAGEOFF
    ldr w10, [x9]
    cbz w10, fail

    adrp x0, path@PAGE
    add x0, x0, path@PAGEOFF
    mov x1, #0
    mov x2, #0
    mov x3, #0
    mov x16, #242       ; Darwin SYS_fsctl
    svc #0x80
    b.cc fail
    cmp x0, #25
    b.ne fail

    adrp x0, path@PAGE
    add x0, x0, path@PAGEOFF
    mov x1, #0
    mov x2, #0
    mov x16, #398       ; Darwin SYS_open_nocancel
    svc #0x80
    b.cs fail
    mov x20, x0

    mov x0, x20
    mov x1, #0
    mov x2, #0
    mov x3, #0
    mov x16, #245       ; Darwin SYS_ffsctl
    svc #0x80
    b.cc fail
    cmp x0, #25
    b.ne fail

    mov x0, x20
    mov x16, #399       ; Darwin SYS_close_nocancel
    svc #0x80
    b.cs fail

    adrp x0, fhopen_handle@PAGE
    add x0, x0, fhopen_handle@PAGEOFF
    mov x1, #0x200      ; O_CREAT is invalid for fhopen
    mov x16, #248       ; Darwin SYS_fhopen
    svc #0x80
    b.cc fail
    cmp x0, #22
    b.ne fail

    adrp x0, fhopen_handle@PAGE
    add x0, x0, fhopen_handle@PAGEOFF
    mov x1, #0
    mov x16, #248       ; Darwin SYS_fhopen privileged unsupported path
    svc #0x80
    b.cc fail
    cmp x0, #45
    b.eq 1f
    cmp x0, #1
    b.ne fail
1:

    mov x0, #0
    mov x1, #0
    mov x2, #0
    mov x16, #243       ; Darwin SYS_initgroups empty-set subset
    svc #0x80
    b.cs fail

    mov x0, #1
    mov x1, #0
    mov x2, #0
    mov x16, #243       ; Darwin SYS_initgroups invalid group buffer
    svc #0x80
    b.cc fail
    cmp x0, #14
    b.ne fail

    mov x0, #1
    adrp x1, gidbuf@PAGE
    add x1, x1, gidbuf@PAGEOFF
    mov x2, #0
    mov x16, #243       ; Darwin SYS_initgroups non-empty unsupported
    svc #0x80
    b.cc fail
    cmp x0, #78
    b.ne fail

    adrp x0, minherit_region@PAGE
    add x0, x0, minherit_region@PAGEOFF
    mov x1, #4096
    mov x2, #1
    mov x16, #250       ; Darwin SYS_minherit
    svc #0x80
    b.cs fail
    cbnz x0, fail

    adrp x0, minherit_region@PAGE
    add x0, x0, minherit_region@PAGEOFF
    mov x1, #4096
    mov x2, #4
    mov x16, #250       ; Darwin SYS_minherit invalid inheritance
    svc #0x80
    b.cc fail
    cmp x0, #22
    b.ne fail

    mov x0, #3
    mov x16, #251       ; Darwin SYS_semsys invalid selector
    svc #0x80
    b.cc fail
    cmp x0, #22
    b.ne fail

    mov x0, #1          ; semget
    mov x1, #0
    mov x2, #1
    mov x3, #896        ; IPC_CREAT | 0600
    mov x16, #251       ; Darwin SYS_semsys
    svc #0x80
    b.cs fail
    mov x20, x0

    mov x0, #0          ; semctl
    mov x1, x20
    mov x2, #0
    mov x3, #0          ; IPC_RMID
    mov x4, #0
    mov x16, #251       ; Darwin SYS_semsys
    svc #0x80
    b.cs fail

    mov x0, #4
    mov x16, #252       ; Darwin SYS_msgsys invalid selector
    svc #0x80
    b.cc fail
    cmp x0, #22
    b.ne fail

    mov x0, #1          ; msgget
    mov x1, #0
    mov x2, #896        ; IPC_CREAT | 0600
    mov x16, #252       ; Darwin SYS_msgsys
    svc #0x80
    b.cs fail
    mov x20, x0

    mov x0, #0          ; msgctl
    mov x1, x20
    mov x2, #0          ; IPC_RMID
    mov x3, #0
    mov x16, #252       ; Darwin SYS_msgsys
    svc #0x80
    b.cs fail

    mov x0, #5
    mov x16, #253       ; Darwin SYS_shmsys invalid selector
    svc #0x80
    b.cc fail
    cmp x0, #22
    b.ne fail

    mov x0, #3          ; shmget
    mov x1, #0
    mov x2, #4096
    mov x3, #896        ; IPC_CREAT | 0600
    mov x16, #253       ; Darwin SYS_shmsys
    svc #0x80
    b.cs fail
    mov x20, x0

    mov x0, #4          ; shmctl
    mov x1, x20
    mov x2, #0          ; IPC_RMID
    mov x3, #0
    mov x16, #253       ; Darwin SYS_shmsys
    svc #0x80
    b.cs fail

    mov x0, #3          ; shmget
    mov x1, #0
    mov x2, #4096
    mov x3, #896        ; IPC_CREAT | 0600
    mov x16, #253       ; Darwin SYS_shmsys
    svc #0x80
    b.cs fail
    mov x20, x0

    mov x0, #1          ; oshmctl
    mov x1, x20
    mov x2, #0          ; IPC_RMID
    mov x3, #0
    mov x16, #253       ; Darwin SYS_shmsys
    svc #0x80
    b.cs fail

    adrp x0, access_entries@PAGE
    add x0, x0, access_entries@PAGEOFF
    adrp x1, access_entries_end@PAGE
    add x1, x1, access_entries_end@PAGEOFF
    sub x1, x1, x0
    adrp x2, access_results@PAGE
    add x2, x2, access_results@PAGEOFF
    mov x3, #-1
    mov x16, #284       ; Darwin SYS_access_extended
    svc #0x80
    b.cs fail
    adrp x9, access_results@PAGE
    add x9, x9, access_results@PAGEOFF
    ldr w10, [x9]
    cbnz w10, fail
    ldr w10, [x9, #4]
    cbnz w10, fail
    ldr w10, [x9, #8]
    cmp w10, #2
    b.ne fail

    adrp x0, access_entries@PAGE
    add x0, x0, access_entries@PAGEOFF
    mov x1, #17
    adrp x2, access_results@PAGE
    add x2, x2, access_results@PAGEOFF
    mov x3, #-1
    mov x16, #284       ; Darwin SYS_access_extended invalid size
    svc #0x80
    b.cc fail
    cmp x0, #22
    b.ne fail

    mov x16, #327       ; Darwin SYS_issetugid
    svc #0x80
    b.cs fail
    cbnz x0, fail

    adrp x0, shared_start@PAGE
    add x0, x0, shared_start@PAGEOFF
    mov x16, #294       ; Darwin SYS_shared_region_check_np
    svc #0x80
    b.cs fail
    adrp x9, shared_start@PAGE
    add x9, x9, shared_start@PAGEOFF
    ldr x10, [x9]
    cbnz x10, fail

    mov x0, #0
    mov x1, #0
    adrp x2, pages_reclaimed@PAGE
    add x2, x2, pages_reclaimed@PAGEOFF
    mov x16, #296       ; Darwin SYS_vm_pressure_monitor
    svc #0x80
    b.cs fail

    mov x0, #0
    mov x1, #0
    mov x16, #322       ; Darwin SYS_iopolicysys
    svc #0x80
    b.cs fail

    mov x0, #0
    mov x1, #0
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x5, #0
    mov x6, #0
    mov x16, #323       ; Darwin SYS_process_policy
    svc #0x80
    b.cs fail

    mov x0, #0
    mov x1, #0
    mov x2, #0
    mov x3, #0
    mov x16, #373       ; Darwin SYS_ledger
    svc #0x80
    b.cs fail

    mov x0, #1          ; Darwin SIG_BLOCK
    adrp x1, sigusr2_set@PAGE
    add x1, x1, sigusr2_set@PAGEOFF
    adrp x2, pthread_old_set@PAGE
    add x2, x2, pthread_old_set@PAGEOFF
    mov x16, #329       ; Darwin SYS___pthread_sigmask
    svc #0x80
    b.cs fail

    mov x0, #0
    mov x1, #0
    mov x16, #328       ; Darwin SYS___pthread_kill
    svc #0x80
    b.cs fail

    mov x16, #20        ; Darwin SYS_getpid
    svc #0x80
    b.cs fail

    mov x1, #31
    mov x16, #37        ; Darwin SYS_kill
    svc #0x80
    b.cs fail

    adrp x0, sigusr2_set@PAGE
    add x0, x0, sigusr2_set@PAGEOFF
    adrp x1, pthread_sigwait_out@PAGE
    add x1, x1, pthread_sigwait_out@PAGEOFF
    mov x16, #330       ; Darwin SYS___sigwait
    svc #0x80
    b.cs fail
    adrp x9, pthread_sigwait_out@PAGE
    add x9, x9, pthread_sigwait_out@PAGEOFF
    ldr w10, [x9]
    cmp w10, #31
    b.ne fail

    mov x0, #2          ; Darwin SIG_UNBLOCK
    adrp x1, sigusr2_set@PAGE
    add x1, x1, sigusr2_set@PAGEOFF
    mov x2, #0
    mov x16, #329       ; Darwin SYS___pthread_sigmask
    svc #0x80
    b.cs fail

    mov x0, #31
    mov x16, #331       ; Darwin SYS___disable_threadsignal
    svc #0x80
    b.cs fail

    mov x0, #0
    mov x16, #332       ; Darwin SYS___pthread_markcancel
    svc #0x80
    b.cs fail
    cbnz x0, fail

    mov x0, #0
    mov x16, #333       ; Darwin SYS___pthread_canceled
    svc #0x80
    b.cs fail
    cbnz x0, fail

    mov x0, #0
    mov x1, #0
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x5, #0
    mov x6, #0
    mov x16, #366       ; Darwin SYS_bsdthread_register
    svc #0x80
    b.cs fail
    cbnz x0, fail

    mov x16, #297       ; Darwin SYS_psynch_rw_longrdlock
    bl expect_psynch_rw_eagain

    mov x16, #298       ; Darwin SYS_psynch_rw_yieldwrlock
    bl expect_psynch_rw_success

    mov x16, #299       ; Darwin SYS_psynch_rw_downgrade
    bl expect_psynch_rw_success

    mov x16, #300       ; Darwin SYS_psynch_rw_upgrade
    bl expect_psynch_rw_success

    adrp x0, psynch_mutex@PAGE
    add x0, x0, psynch_mutex@PAGEOFF
    mov x1, #0x1111
    mov x2, #0x2222
    mov x3, #0x3333
    mov x4, #0
    mov x16, #301       ; Darwin SYS_psynch_mutexwait
    svc #0x80
    b.cc fail
    cmp x0, #35
    b.ne fail

    adrp x0, psynch_mutex@PAGE
    add x0, x0, psynch_mutex@PAGEOFF
    mov x1, #0x1111
    mov x2, #0x2222
    mov x3, #0x3333
    mov x4, #0
    mov x16, #302       ; Darwin SYS_psynch_mutexdrop
    svc #0x80
    b.cs fail
    cbnz x0, fail

    adrp x0, psynch_cond@PAGE
    add x0, x0, psynch_cond@PAGEOFF
    mov x1, #0x1111
    mov x2, #0x2222
    mov x3, #0
    adrp x4, psynch_mutex@PAGE
    add x4, x4, psynch_mutex@PAGEOFF
    mov x5, #0x3333
    mov x6, #0x4444
    mov x16, #303       ; Darwin SYS_psynch_cvbroad
    svc #0x80
    b.cs fail
    cbnz x0, fail

    adrp x0, psynch_cond@PAGE
    add x0, x0, psynch_cond@PAGEOFF
    mov x1, #0x1111
    mov x2, #0x2222
    mov x3, #0
    adrp x4, psynch_mutex@PAGE
    add x4, x4, psynch_mutex@PAGEOFF
    mov x5, #0x3333
    mov x6, #0x4444
    mov x7, #0
    mov x16, #304       ; Darwin SYS_psynch_cvsignal
    svc #0x80
    b.cs fail
    cbnz x0, fail

    adrp x0, psynch_cond@PAGE
    add x0, x0, psynch_cond@PAGEOFF
    mov x1, #0x1111
    mov x2, #0x2222
    adrp x3, psynch_mutex@PAGE
    add x3, x3, psynch_mutex@PAGEOFF
    mov x4, #0x3333
    mov x5, #0
    mov x6, #0
    mov x7, #0
    mov x16, #305       ; Darwin SYS_psynch_cvwait
    svc #0x80
    b.cc fail
    cmp x0, #60
    b.ne fail

    mov x16, #306       ; Darwin SYS_psynch_rw_rdlock
    bl expect_psynch_rw_eagain

    mov x16, #307       ; Darwin SYS_psynch_rw_wrlock
    bl expect_psynch_rw_eagain

    mov x16, #308       ; Darwin SYS_psynch_rw_unlock
    bl expect_psynch_rw_success

    mov x16, #309       ; Darwin SYS_psynch_rw_unlock2
    bl expect_psynch_rw_enotsup

    adrp x0, psynch_cond@PAGE
    add x0, x0, psynch_cond@PAGEOFF
    mov x1, #0x1111
    mov x2, #0x2222
    mov x3, #0x3333
    mov x4, #1
    mov x5, #0x4444
    mov x6, #0
    mov x16, #312       ; Darwin SYS_psynch_cvclrprepost
    svc #0x80
    b.cs fail
    cbnz x0, fail

    mov x0, #0
    mov x1, #0
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x16, #360       ; Darwin SYS_bsdthread_create
    svc #0x80
    b.cc fail
    cmp x0, #22
    b.ne fail

    mov x0, #0
    mov x1, #0
    mov x2, #0
    mov x3, #0
    mov x16, #361       ; Darwin SYS_bsdthread_terminate
    svc #0x80
    b.cc fail
    cmp x0, #22
    b.ne fail

    mov x16, #367       ; Darwin SYS_workq_open
    svc #0x80
    b.cs fail
    cbnz x0, fail

    mov x0, #0
    mov x1, #0
    mov x2, #0
    mov x3, #0
    mov x16, #368       ; Darwin SYS_workq_kernreturn
    svc #0x80
    b.cs fail
    cbnz x0, fail

    mov x16, #368       ; Darwin SYS_workq_kernreturn
    bl expect_workq_kernreturn_einval

    adrp x0, message@PAGE
    add x0, x0, message@PAGEOFF
    mov x1, #5
    mov x16, #203       ; Darwin SYS_mlock
    svc #0x80
    b.cs fail

    adrp x0, extdir@PAGE
    add x0, x0, extdir@PAGEOFF
    mov x1, #-1
    mov x2, #-1
    mov x3, #0700
    mov x4, #0
    mov x16, #292       ; Darwin SYS_mkdir_extended
    svc #0x80
    b.cs fail

    adrp x0, extdir@PAGE
    add x0, x0, extdir@PAGEOFF
    mov x16, #137       ; Darwin SYS_rmdir
    svc #0x80
    b.cs fail

    adrp x0, fifo_path@PAGE
    add x0, x0, fifo_path@PAGEOFF
    mov x1, #-1
    mov x2, #-1
    mov x3, #0600
    mov x4, #0
    mov x16, #291       ; Darwin SYS_mkfifo_extended
    svc #0x80
    b.cs fail

    adrp x0, fifo_path@PAGE
    add x0, x0, fifo_path@PAGEOFF
    mov x16, #226       ; Darwin SYS_delete
    svc #0x80
    b.cs fail

    adrp x0, uidbuf@PAGE
    add x0, x0, uidbuf@PAGEOFF
    adrp x1, gidbuf@PAGE
    add x1, x1, gidbuf@PAGEOFF
    mov x16, #286       ; Darwin SYS_gettid
    svc #0x80
    b.cs fail

    mov x0, #1234
    mov x1, #5678
    mov x16, #285       ; Darwin SYS_sys_settid
    svc #0x80
    b.cs fail

    adrp x0, uidbuf@PAGE
    add x0, x0, uidbuf@PAGEOFF
    adrp x1, gidbuf@PAGE
    add x1, x1, gidbuf@PAGEOFF
    mov x16, #286       ; Darwin SYS_gettid
    svc #0x80
    b.cs fail
    adrp x9, uidbuf@PAGE
    add x9, x9, uidbuf@PAGEOFF
    ldr w10, [x9]
    mov w11, #1234
    cmp w10, w11
    b.ne fail
    adrp x9, gidbuf@PAGE
    add x9, x9, gidbuf@PAGEOFF
    ldr w10, [x9]
    mov w11, #5678
    cmp w10, w11
    b.ne fail

    mov x0, #0
    mov x1, #0
    mov x16, #287       ; Darwin SYS_setsgroups
    svc #0x80
    b.cs fail

    mov x0, #0
    mov x1, #0
    mov x16, #289       ; Darwin SYS_setwgroups
    svc #0x80
    b.cs fail

    adrp x0, uuid_group_count@PAGE
    add x0, x0, uuid_group_count@PAGEOFF
    mov x1, #0
    mov x16, #288       ; Darwin SYS_getsgroups
    svc #0x80
    b.cs fail
    adrp x9, uuid_group_count@PAGE
    add x9, x9, uuid_group_count@PAGEOFF
    ldr w10, [x9]
    cbnz w10, fail

    mov w10, #99
    str w10, [x9]
    mov x0, x9
    mov x1, #0
    mov x16, #290       ; Darwin SYS_getwgroups
    svc #0x80
    b.cs fail
    ldr w10, [x9]
    cbnz w10, fail

    mov x0, #0
    mov x1, #0
    mov x16, #311       ; Darwin SYS_sys_settid_with_pid
    svc #0x80
    b.cs fail

    mov x0, #0
    mov x1, #0
    mov x2, #0
    mov x3, #0
    adrp x4, zero_timeout@PAGE
    add x4, x4, zero_timeout@PAGEOFF
    mov x5, #0
    mov x16, #395       ; Darwin SYS_pselect_nocancel
    svc #0x80
    b.cs fail
    cbnz x0, fail

    mov x16, #362       ; Darwin SYS_kqueue
    svc #0x80
    b.cs fail
    mov x26, x0

    mov x0, x26
    mov x1, #0
    mov x2, #0
    mov x3, #0
    mov x4, #0
    adrp x5, zero_timeout@PAGE
    add x5, x5, zero_timeout@PAGEOFF
    mov x16, #363       ; Darwin SYS_kevent
    svc #0x80
    b.cs fail
    cbnz x0, fail

    mov x0, x26
    mov x1, #0
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x5, #0
    mov x6, #0
    mov x7, #0
    mov x16, #374       ; Darwin SYS_kevent_qos
    svc #0x80
    b.cs fail
    cbnz x0, fail

    mov x0, x26
    mov x1, #0
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x5, #0
    mov x6, #0
    mov x7, #0
    mov x16, #375       ; Darwin SYS_kevent_id
    svc #0x80
    b.cs fail
    cbnz x0, fail

    mov x0, x26
    mov x1, #0
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x5, #0
    adrp x6, zero_timeout@PAGE
    add x6, x6, zero_timeout@PAGEOFF
    mov x16, #369       ; Darwin SYS_kevent64 zero-change subset
    svc #0x80
    b.cs fail
    cbnz x0, fail

    mov x0, x26
    mov x1, #0
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x5, #1
    mov x6, #0
    mov x16, #369       ; Darwin SYS_kevent64 flags unsupported
    svc #0x80
    b.cc fail
    cmp x0, #78
    b.ne fail

    mov x0, x26
    mov x16, #399       ; Darwin SYS_close_nocancel
    svc #0x80
    b.cs fail

    adrp x0, message@PAGE
    add x0, x0, message@PAGEOFF
    mov x1, #5
    mov x16, #204       ; Darwin SYS_munlock
    svc #0x80
    b.cs fail

    adrp x0, done@PAGE
    add x0, x0, done@PAGEOFF
    mov x1, #5
    mov x16, #200       ; Darwin SYS_truncate
    svc #0x80
    b.cs fail

    adrp x0, root_path@PAGE
    add x0, x0, root_path@PAGEOFF
    adrp x1, statfsbuf@PAGE
    add x1, x1, statfsbuf@PAGEOFF
    mov x16, #345       ; Darwin SYS_statfs64
    svc #0x80
    b.cs fail
    adrp x9, statfsbuf@PAGE
    add x9, x9, statfsbuf@PAGEOFF
    ldr w10, [x9]
    cbz w10, fail

    mov x0, #0
    mov x1, #0
    mov x2, #2
    mov x16, #347       ; Darwin SYS_getfsstat64
    svc #0x80
    b.cs fail
    cbz x0, fail

    adrp x0, root_path@PAGE
    add x0, x0, root_path@PAGEOFF
    mov x1, #0
    mov x2, #0
    mov x16, #398       ; Darwin SYS_open_nocancel
    svc #0x80
    b.cs fail
    mov x22, x0

    mov x0, x22
    adrp x1, direntbuf@PAGE
    add x1, x1, direntbuf@PAGEOFF
    mov x2, #4096
    adrp x3, dirpos@PAGE
    add x3, x3, dirpos@PAGEOFF
    mov x16, #344       ; Darwin SYS_getdirentries64
    svc #0x80
    b.cs fail
    cbz x0, fail
    adrp x9, direntbuf@PAGE
    add x9, x9, direntbuf@PAGEOFF
    ldrh w10, [x9, #16]
    cbz w10, fail
    ldrh w10, [x9, #18]
    cbz w10, fail
    adrp x9, dirpos@PAGE
    add x9, x9, dirpos@PAGEOFF
    ldr x10, [x9]
    cbz x10, fail

    mov x0, x22
    mov x16, #399       ; Darwin SYS_close_nocancel
    svc #0x80
    b.cs fail

    adrp x0, path@PAGE
    add x0, x0, path@PAGEOFF
    mov x1, #0
    mov x2, #0
    mov x16, #398       ; Darwin SYS_open_nocancel
    svc #0x80
    b.cs fail
    mov x21, x0

    mov x0, x21
    adrp x1, statfsbuf2@PAGE
    add x1, x1, statfsbuf2@PAGEOFF
    mov x16, #346       ; Darwin SYS_fstatfs64
    svc #0x80
    b.cs fail

    adrp x0, sendfile_path@PAGE
    add x0, x0, sendfile_path@PAGEOFF
    mov x1, #0x601
    mov x2, #0600
    mov x16, #398       ; Darwin SYS_open_nocancel
    svc #0x80
    b.cs fail
    mov x22, x0

    mov x0, x21
    mov x1, x22
    mov x2, #0
    adrp x3, sendfile_len@PAGE
    add x3, x3, sendfile_len@PAGEOFF
    mov x4, #0
    mov x5, #0
    mov x16, #337       ; Darwin SYS_sendfile
    svc #0x80
    b.cs fail
    cbnz x0, fail
    adrp x9, sendfile_len@PAGE
    add x9, x9, sendfile_len@PAGEOFF
    ldr x10, [x9]
    cmp x10, #5
    b.ne fail

    mov x0, x21
    mov x16, #399       ; Darwin SYS_close_nocancel
    svc #0x80
    b.cs fail

    mov x0, x22
    mov x16, #399       ; Darwin SYS_close_nocancel
    svc #0x80
    b.cs fail

    adrp x0, shm_name@PAGE
    add x0, x0, shm_name@PAGEOFF
    mov x1, #0x602      ; Darwin O_RDWR | O_CREAT | O_TRUNC
    mov x2, #0600
    mov x16, #266       ; Darwin SYS_shm_open
    svc #0x80
    b.cs fail
    mov x23, x0

    mov x0, x23
    adrp x1, message@PAGE
    add x1, x1, message@PAGEOFF
    mov x2, #5
    mov x16, #397       ; Darwin SYS_write_nocancel
    svc #0x80
    b.cs fail
    cmp x0, #5
    b.ne fail

    mov x0, x23
    mov x16, #399       ; Darwin SYS_close_nocancel
    svc #0x80
    b.cs fail

    adrp x0, shm_name@PAGE
    add x0, x0, shm_name@PAGEOFF
    mov x16, #267       ; Darwin SYS_shm_unlink
    svc #0x80
    b.cs fail

    adrp x0, sem_name@PAGE
    add x0, x0, sem_name@PAGEOFF
    mov x16, #270       ; Darwin SYS_sem_unlink
    svc #0x80

    adrp x0, sem_name@PAGE
    add x0, x0, sem_name@PAGEOFF
    mov x1, #0xA00      ; Darwin O_CREAT | O_EXCL
    mov x2, #0600
    mov x3, #1
    mov x16, #268       ; Darwin SYS_sem_open
    svc #0x80
    b.cs fail
    mov x26, x0

    mov x0, x26
    mov x16, #271       ; Darwin SYS_sem_wait
    svc #0x80
    b.cs fail

    mov x0, x26
    mov x16, #272       ; Darwin SYS_sem_trywait
    svc #0x80
    b.cc fail
    cmp x0, #35         ; Darwin EAGAIN
    b.ne fail

    mov x0, x26
    mov x16, #273       ; Darwin SYS_sem_post
    svc #0x80
    b.cs fail

    mov x0, x26
    mov x16, #272       ; Darwin SYS_sem_trywait
    svc #0x80
    b.cs fail

    mov x0, x26
    mov x16, #273       ; Darwin SYS_sem_post
    svc #0x80
    b.cs fail

    mov x0, x26
    mov x16, #420       ; Darwin SYS_sem_wait_nocancel
    svc #0x80
    b.cs fail

    mov x0, x26
    mov x16, #269       ; Darwin SYS_sem_close
    svc #0x80
    b.cs fail

    adrp x0, sem_name@PAGE
    add x0, x0, sem_name@PAGEOFF
    mov x1, #0
    mov x2, #0
    mov x3, #0
    mov x16, #268       ; Darwin SYS_sem_open
    svc #0x80
    b.cs fail
    mov x26, x0

    adrp x0, sem_name@PAGE
    add x0, x0, sem_name@PAGEOFF
    mov x16, #270       ; Darwin SYS_sem_unlink
    svc #0x80
    b.cs fail

    mov x0, x26
    mov x16, #269       ; Darwin SYS_sem_close
    svc #0x80
    b.cs fail

    adrp x0, sem_name@PAGE
    add x0, x0, sem_name@PAGEOFF
    mov x1, #0
    mov x2, #0
    mov x3, #0
    mov x16, #268       ; Darwin SYS_sem_open
    svc #0x80
    b.cc fail
    cmp x0, #2          ; Darwin ENOENT
    b.ne fail

    adrp x0, sem_name@PAGE
    add x0, x0, sem_name@PAGEOFF
    mov x1, #0xA00      ; Darwin O_CREAT | O_EXCL
    mov x2, #0600
    mov x3, #1
    mov x16, #268       ; Darwin SYS_sem_open
    svc #0x80
    b.cs fail
    mov x26, x0

    mov x0, x26
    mov x1, #0
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x5, #0
    mov x16, #334       ; Darwin SYS___semwait_signal
    svc #0x80
    b.cs fail
    cbnz x0, fail

    mov x0, x26
    mov x1, #0
    mov x2, #1
    mov x3, #1
    mov x4, #0
    mov x5, #0
    mov x16, #334       ; Darwin SYS___semwait_signal
    svc #0x80
    b.cc fail
    cmp x0, #35         ; Darwin EAGAIN
    b.ne fail

    mov x0, #0
    mov x1, x26
    mov x2, #1
    mov x3, #1
    mov x4, #0
    mov x5, #0
    mov x16, #334       ; Darwin SYS___semwait_signal
    svc #0x80
    b.cc fail
    cmp x0, #35         ; Darwin EAGAIN
    b.ne fail

    mov x0, x26
    mov x16, #272       ; Darwin SYS_sem_trywait
    svc #0x80
    b.cs fail

    adrp x0, sem_name@PAGE
    add x0, x0, sem_name@PAGEOFF
    mov x16, #270       ; Darwin SYS_sem_unlink
    svc #0x80
    b.cs fail

    mov x0, x26
    mov x16, #269       ; Darwin SYS_sem_close
    svc #0x80
    b.cs fail

    mov x0, #0
    mov x1, #0
    mov x2, #1
    mov x3, #1
    mov x4, #0
    mov x5, #0
    mov x16, #334       ; Darwin SYS___semwait_signal
    svc #0x80
    b.cc fail
    cmp x0, #35         ; Darwin EAGAIN
    b.ne fail

    mov x16, #334       ; Darwin SYS___semwait_signal
    bl expect_semwait_signal_einval

    mov x0, #0
    mov x1, #0
    mov x2, #1
    mov x3, #1
    mov x4, #0
    mov x5, #0
    mov x16, #423       ; Darwin SYS___semwait_signal_nocancel
    svc #0x80
    b.cc fail
    cmp x0, #35         ; Darwin EAGAIN
    b.ne fail

    mov x0, #0
    mov x1, #4096
    mov x2, #896        ; IPC_CREAT | 0600
    mov x16, #265       ; Darwin SYS_shmget
    svc #0x80
    b.cs fail
    mov x24, x0

    mov x0, x24
    mov x1, #0
    mov x2, #0
    mov x16, #262       ; Darwin SYS_shmat
    svc #0x80
    b.cs fail
    mov x25, x0
    mov w10, #'Z'
    strb w10, [x25]
    ldrb w11, [x25]
    cmp w11, #'Z'
    b.ne fail

    mov x0, x25
    mov x16, #264       ; Darwin SYS_shmdt
    svc #0x80
    b.cs fail

    mov x0, x24
    mov x1, #0          ; IPC_RMID
    mov x2, #0
    mov x16, #263       ; Darwin SYS_shmctl
    svc #0x80
    b.cs fail

    mov x0, #0
    mov x1, #0
    mov x16, #350       ; Darwin SYS_audit
    svc #0x80
    b.cs fail
    cbnz x0, fail

    mov x0, #0
    mov x1, #0
    mov x2, #0
    mov x16, #351       ; Darwin SYS_auditon
    svc #0x80
    b.cc fail
    cmp x0, #45
    b.ne fail

    adrp x0, audit_auid@PAGE
    add x0, x0, audit_auid@PAGEOFF
    mov x16, #353       ; Darwin SYS_getauid
    svc #0x80
    b.cs fail
    adrp x9, audit_auid@PAGE
    add x9, x9, audit_auid@PAGEOFF
    mov w10, #501
    str w10, [x9]
    mov x0, x9
    mov x16, #354       ; Darwin SYS_setauid
    svc #0x80
    b.cs fail

    adrp x0, audit_auid_readback@PAGE
    add x0, x0, audit_auid_readback@PAGEOFF
    mov x16, #353       ; Darwin SYS_getauid
    svc #0x80
    b.cs fail
    adrp x9, audit_auid_readback@PAGE
    add x9, x9, audit_auid_readback@PAGEOFF
    ldr w10, [x9]
    cmp w10, #501
    b.ne fail

    adrp x0, auditinfo_addr@PAGE
    add x0, x0, auditinfo_addr@PAGEOFF
    mov x1, #48
    mov x16, #357       ; Darwin SYS_getaudit_addr
    svc #0x80
    b.cs fail
    adrp x9, auditinfo_addr@PAGE
    add x9, x9, auditinfo_addr@PAGEOFF
    ldr w10, [x9]
    cmp w10, #501
    b.ne fail

    mov w10, #502
    str w10, [x9]
    mov x0, x9
    mov x1, #48
    mov x16, #358       ; Darwin SYS_setaudit_addr
    svc #0x80
    b.cs fail

    adrp x0, auditinfo_addr_readback@PAGE
    add x0, x0, auditinfo_addr_readback@PAGEOFF
    mov x1, #48
    mov x16, #357       ; Darwin SYS_getaudit_addr
    svc #0x80
    b.cs fail
    adrp x9, auditinfo_addr_readback@PAGE
    add x9, x9, auditinfo_addr_readback@PAGEOFF
    ldr w10, [x9]
    cmp w10, #502
    b.ne fail

    mov x0, #0
    mov x16, #359       ; Darwin SYS_auditctl
    svc #0x80
    b.cc fail
    cmp x0, #45
    b.ne fail

    mov x16, #381       ; Darwin SYS___mac_syscall
    svc #0x80
    b.cc fail
    cmp x0, #45
    b.ne fail

    adrp x0, path@PAGE
    add x0, x0, path@PAGEOFF
    adrp x1, mac_label@PAGE
    add x1, x1, mac_label@PAGEOFF
    mov x16, #382       ; Darwin SYS___mac_get_file
    svc #0x80
    b.cs fail
    adrp x9, mac_label_buf@PAGE
    add x9, x9, mac_label_buf@PAGEOFF
    ldrb w10, [x9]
    cbnz w10, fail

    adrp x0, path@PAGE
    add x0, x0, path@PAGEOFF
    adrp x1, mac_label@PAGE
    add x1, x1, mac_label@PAGEOFF
    mov x16, #383       ; Darwin SYS___mac_set_file
    svc #0x80
    b.cc fail
    cmp x0, #45
    b.ne fail

    adrp x0, path@PAGE
    add x0, x0, path@PAGEOFF
    adrp x1, mac_label@PAGE
    add x1, x1, mac_label@PAGEOFF
    mov x16, #384       ; Darwin SYS___mac_get_link
    svc #0x80
    b.cs fail

    adrp x0, path@PAGE
    add x0, x0, path@PAGEOFF
    adrp x1, mac_label@PAGE
    add x1, x1, mac_label@PAGEOFF
    mov x16, #385       ; Darwin SYS___mac_set_link
    svc #0x80
    b.cc fail
    cmp x0, #45
    b.ne fail

    adrp x0, mac_label@PAGE
    add x0, x0, mac_label@PAGEOFF
    mov x16, #386       ; Darwin SYS___mac_get_proc
    svc #0x80
    b.cs fail

    adrp x0, mac_label@PAGE
    add x0, x0, mac_label@PAGEOFF
    mov x16, #387       ; Darwin SYS___mac_set_proc
    svc #0x80
    b.cc fail
    cmp x0, #45
    b.ne fail

    mov x0, #1
    adrp x1, mac_label@PAGE
    add x1, x1, mac_label@PAGEOFF
    mov x16, #388       ; Darwin SYS___mac_get_fd
    svc #0x80
    b.cs fail

    mov x0, #1
    adrp x1, mac_label@PAGE
    add x1, x1, mac_label@PAGEOFF
    mov x16, #389       ; Darwin SYS___mac_set_fd
    svc #0x80
    b.cc fail
    cmp x0, #45
    b.ne fail

    mov x16, #20        ; Darwin SYS_getpid
    svc #0x80
    b.cs fail
    adrp x1, mac_label@PAGE
    add x1, x1, mac_label@PAGEOFF
    mov x16, #390       ; Darwin SYS___mac_get_pid
    svc #0x80
    b.cs fail

    adrp x0, fsgetpath_buf@PAGE
    add x0, x0, fsgetpath_buf@PAGEOFF
    mov x1, #256
    adrp x2, fsid_buf@PAGE
    add x2, x2, fsid_buf@PAGEOFF
    adrp x3, fileid_buf@PAGE
    add x3, x3, fileid_buf@PAGEOFF
    ldr x3, [x3]
    mov x4, #0
    mov x16, #217       ; Darwin SYS_fsgetpath_ext
    svc #0x80
    b.cs fail
    cbz x0, fail
    adrp x9, fsgetpath_buf@PAGE
    add x9, x9, fsgetpath_buf@PAGEOFF
    ldrb w10, [x9]
    cmp w10, #'/'
    b.ne fail

    mov x0, #1
    adrp x1, ok@PAGE
    add x1, x1, ok@PAGEOFF
    mov x2, #13
    mov x16, #397       ; Darwin SYS_write_nocancel
    svc #0x80
    b.cs fail

    mov x0, #0
    mov x16, #1
    svc #0x80
    b fail

expect_psynch_rw_eagain:
    adrp x0, psynch_rwlock@PAGE
    add x0, x0, psynch_rwlock@PAGEOFF
    mov w1, #0x123
    mov w2, #0x234
    mov w3, #0x345
    mov w4, #0x456
    svc #0x80
    b.cc fail
    cmp x0, #35
    b.ne fail
    cmp w1, #0x123
    b.ne fail
    cmp w2, #0x234
    b.ne fail
    cmp w3, #0x345
    b.ne fail
    cmp w4, #0x456
    b.ne fail
    ret

expect_psynch_rw_success:
    adrp x0, psynch_rwlock@PAGE
    add x0, x0, psynch_rwlock@PAGEOFF
    mov w1, #0x123
    mov w2, #0x234
    mov w3, #0x345
    mov w4, #0x456
    svc #0x80
    b.cs fail
    cbnz x0, fail
    cmp w1, #0x123
    b.ne fail
    cmp w2, #0x234
    b.ne fail
    cmp w3, #0x345
    b.ne fail
    cmp w4, #0x456
    b.ne fail
    ret

expect_psynch_rw_enotsup:
    adrp x0, psynch_rwlock@PAGE
    add x0, x0, psynch_rwlock@PAGEOFF
    mov w1, #0x123
    mov w2, #0x234
    mov w3, #0x345
    mov w4, #0x456
    svc #0x80
    b.cc fail
    cmp x0, #45
    b.ne fail
    cmp w1, #0x123
    b.ne fail
    cmp w2, #0x234
    b.ne fail
    cmp w3, #0x345
    b.ne fail
    cmp w4, #0x456
    b.ne fail
    ret

expect_workq_kernreturn_einval:
    mov w0, #1
    adrp x1, message@PAGE
    add x1, x1, message@PAGEOFF
    mov w2, #0x234
    mov w3, #0x345
    mov w4, #0x456
    svc #0x80
    b.cc fail
    cmp x0, #22
    b.ne fail
    adrp x9, message@PAGE
    add x9, x9, message@PAGEOFF
    cmp x1, x9
    b.ne fail
    cmp w2, #0x234
    b.ne fail
    cmp w3, #0x345
    b.ne fail
    cmp w4, #0x456
    b.ne fail
    ret

expect_semwait_signal_einval:
    mov w0, #1
    mov w1, #2
    mov w2, #1
    mov w3, #1
    mov w4, #0
    mov w5, #1
    svc #0x80
    b.cc fail
    cmp x0, #22
    b.ne fail
    cmp w1, #2
    b.ne fail
    cmp w2, #1
    b.ne fail
    cmp w3, #1
    b.ne fail
    cmp w4, #0
    b.ne fail
    cmp w5, #1
    b.ne fail
    ret

fail:
    mov x0, #1
    mov x16, #1
    svc #0x80

.section __TEXT,__cstring
path:
    .asciz "range_200_399.tmp"
done:
    .asciz "range_200_399.done"
protected_path:
    .asciz "range_200_399.protected"
extdir:
    .asciz "range_200_399.dir"
fifo_path:
    .asciz "range_200_399.fifo"
sendfile_path:
    .asciz "range_200_399.sendfile"
copyfile_path:
    .asciz "range_200_399.copyfile"
exchange_path_a:
    .asciz "range_200_399.exchange_a"
exchange_path_b:
    .asciz "range_200_399.exchange_b"
current_dir:
    .asciz "."
root_path:
    .asciz "/"
shm_name:
    .asciz "/machgate_range_200_399"
sem_name:
    .asciz "/machgate_range_200_399_sem"
message:
    .asciz "range"
ok:
    .asciz "range-200-ok\n"
attrname:
    .asciz "user.machgate"
attrvalue:
    .asciz "ok"
exchange_left:
    .asciz "left!"
exchange_right:
    .asciz "right"
hw_memsize_name:
    .asciz "hw.memsize"

.section __DATA,__data
.p2align 3
hw_ncpu_mib:
    .long 6
    .long 3
sysctl_int_len:
    .quad 4
sysctl_u64_len:
    .quad 8
sysctl_intbuf:
    .word 0
    .word 0
sysctl_u64buf:
    .quad 0
proc_pid_list:
    .word 0
    .word 0
proc_shortinfo:
    .space 64
statbuf:
    .space 144
statbuf2:
    .space 144
pollfds:
    .space 8
psynch_rwlock:
    .space 64
xsecurity_size:
    .quad 16
zero_timeout:
    .quad 0
    .quad 0
uidbuf:
    .word 0
gidbuf:
    .word 0
uuid_group_count:
    .word 99
audit_auid:
    .word 0
audit_auid_readback:
    .word 0
auditinfo_addr:
    .space 48
auditinfo_addr_readback:
    .space 48
mac_label:
    .quad 8
    .quad mac_label_buf
mac_label_buf:
    .space 8
attrbuf:
    .space 8
sendfile_len:
    .quad 5
statfsbuf:
    .space 2168
statfsbuf2:
    .space 2168
aio_write_cb:
    .space 80
aio_read_cb:
    .space 80
aio_lio_cb:
    .space 80
aio_list:
    .quad 0
aio_readbuf:
    .space 8
sigusr2_set:
    .long 0x40000000
pthread_old_set:
    .long 0
pthread_sigwait_out:
    .long 0
pages_reclaimed:
    .long 0
shared_start:
    .quad 0
    .p2align 12
minherit_region:
    .space 4096
dirpos:
    .quad 0
psynch_mutex:
    .space 64
psynch_cond:
    .space 64
direntbuf:
    .space 4096
attrlist_file:
    .hword 5
    .hword 0
    .long 0x8203840d
    .long 0
    .long 0
    .long 0x00000200
    .long 0
attrlist_set_mode:
    .hword 5
    .hword 0
    .long 0x00020000
    .long 0
    .long 0
    .long 0
    .long 0
attrlist_dir:
    .hword 5
    .hword 0
    .long 0x82000001
    .long 0
    .long 0
    .long 0
    .long 0
mode_0600:
    .long 0600
mode_0644:
    .long 0644
attrlist_buf:
    .space 512
attrlist_buf2:
    .space 512
dirattr_count:
    .long 8
dirattr_base:
    .quad 0
dirattr_state:
    .quad 0
dirattrbuf:
    .space 4096
fsid_buf:
    .space 8
fileid_buf:
    .quad 0
fsgetpath_buf:
    .space 256
    .p2align 2
access_entries:
    .long access_existing_path - access_entries
    .long 0
    .long 0
    .long 0
    .long 0
    .long 4
    .long 0
    .long 0
    .long access_missing_path - access_entries
    .long 0
    .long 0
    .long 0
access_existing_path:
    .asciz "range_200_399.tmp"
access_missing_path:
    .asciz "range_200_399.missing"
access_entries_end:
    .p2align 2
access_results:
    .space 12
spawn_pid:
    .long 0
identity_cache_sizes:
    .long 99
    .long 99
exchange_buf:
    .space 8
searchfs_match_count:
    .quad 0
searchfs_block:
    .quad attrlist_search_return
    .quad searchfs_result_buf
    .quad 512
    .quad 4
    .quad 0
    .quad 0
    .quad searchfs_name_param
    .quad searchfs_name_param_end - searchfs_name_param
    .quad 0
    .quad 0
    .hword 5
    .hword 0
    .long 0x00000001
    .long 0
    .long 0
    .long 0
    .long 0
attrlist_search_return:
    .hword 5
    .hword 0
    .long 0x00000001
    .long 0
    .long 0
    .long 0
    .long 0
searchfs_name_param:
    .long searchfs_name_param_end - searchfs_name_param
    .long 8
    .long searchfs_name_end - searchfs_name
searchfs_name:
    .asciz "range_200_399.exchange_a"
searchfs_name_end:
    .p2align 2
searchfs_name_param_end:
searchfs_result_buf:
    .space 512
searchfs_state:
    .space 64
fhopen_handle:
    .space 64
