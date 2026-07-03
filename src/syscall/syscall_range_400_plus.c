#include "syscall_range_400_plus.h"

#include "syscall_range_200_399.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <limits.h>
#include <linux/futex.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/random.h>
#include <sys/resource.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysinfo.h>
#include <sys/timex.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define DARWIN_SYS_wait4_nocancel       400
#define DARWIN_SYS_recvmsg_nocancel     401
#define DARWIN_SYS_sendmsg_nocancel     402
#define DARWIN_SYS_recvfrom_nocancel    403
#define DARWIN_SYS_accept_nocancel      404
#define DARWIN_SYS_msync_nocancel       405
#define DARWIN_SYS_fcntl_nocancel       406
#define DARWIN_SYS_select_nocancel      407
#define DARWIN_SYS_fsync_nocancel       408
#define DARWIN_SYS_connect_nocancel     409
#define DARWIN_SYS_sigsuspend_nocancel  410
#define DARWIN_SYS_readv_nocancel       411
#define DARWIN_SYS_writev_nocancel      412
#define DARWIN_SYS_sendto_nocancel      413
#define DARWIN_SYS_pread_nocancel       414
#define DARWIN_SYS_pwrite_nocancel      415
#define DARWIN_SYS_waitid_nocancel      416
#define DARWIN_SYS_poll_nocancel        417
#define DARWIN_SYS_msgsnd_nocancel      418
#define DARWIN_SYS_msgrcv_nocancel      419
#define DARWIN_SYS_sem_wait_nocancel    420
#define DARWIN_SYS_aio_suspend_nocancel 421
#define DARWIN_SYS___sigwait_nocancel   422
#define DARWIN_SYS___semwait_signal_nocancel 423
#define DARWIN_SYS___mac_mount          424
#define DARWIN_SYS___mac_get_mount      425
#define DARWIN_SYS___mac_getfsstat      426
#define DARWIN_SYS_fsgetpath            427
#define DARWIN_SYS_audit_session_self   428
#define DARWIN_SYS_audit_session_join   429
#define DARWIN_SYS_sys_fileport_makeport 430
#define DARWIN_SYS_sys_fileport_makefd  431
#define DARWIN_SYS_audit_session_port   432
#define DARWIN_SYS_pid_suspend          433
#define DARWIN_SYS_pid_resume           434
#define DARWIN_SYS_pid_hibernate        435
#define DARWIN_SYS_pid_shutdown_sockets 436
#define DARWIN_SYS_kas_info             439
#define DARWIN_SYS_memorystatus_control 440
#define DARWIN_SYS_guarded_open_np      441
#define DARWIN_SYS_guarded_close_np     442
#define DARWIN_SYS_guarded_kqueue_np    443
#define DARWIN_SYS_change_fdguard_np    444
#define DARWIN_SYS_usrctl               445
#define DARWIN_SYS_proc_rlimit_control  446
#define DARWIN_SYS_connectx             447
#define DARWIN_SYS_disconnectx          448
#define DARWIN_SYS_peeloff              449
#define DARWIN_SYS_socket_delegate      450
#define DARWIN_SYS_telemetry            451
#define DARWIN_SYS_proc_uuid_policy     452
#define DARWIN_SYS_memorystatus_get_level 453
#define DARWIN_SYS_system_override      454
#define DARWIN_SYS_vfs_purge            455
#define DARWIN_SYS_sfi_ctl              456
#define DARWIN_SYS_sfi_pidctl           457
#define DARWIN_SYS_coalition            458
#define DARWIN_SYS_coalition_info       459
#define DARWIN_SYS_necp_match_policy    460
#define DARWIN_SYS_getattrlistbulk      461
#define DARWIN_SYS_clonefileat          462
#define DARWIN_SYS_openat               463
#define DARWIN_SYS_openat_nocancel      464
#define DARWIN_SYS_renameat             465
#define DARWIN_SYS_faccessat            466
#define DARWIN_SYS_fchmodat             467
#define DARWIN_SYS_fchownat             468
#define DARWIN_SYS_fstatat              469
#define DARWIN_SYS_fstatat64            470
#define DARWIN_SYS_linkat               471
#define DARWIN_SYS_unlinkat             472
#define DARWIN_SYS_readlinkat           473
#define DARWIN_SYS_symlinkat            474
#define DARWIN_SYS_mkdirat              475
#define DARWIN_SYS_getattrlistat        476
#define DARWIN_SYS_proc_trace_log       477
#define DARWIN_SYS_bsdthread_ctl        478
#define DARWIN_SYS_openbyid_np          479
#define DARWIN_SYS_recvmsg_x            480
#define DARWIN_SYS_sendmsg_x            481
#define DARWIN_SYS_thread_selfusage     482
#define DARWIN_SYS_csrctl               483
#define DARWIN_SYS_guarded_open_dprotected_np 484
#define DARWIN_SYS_guarded_write_np     485
#define DARWIN_SYS_guarded_pwrite_np    486
#define DARWIN_SYS_guarded_writev_np    487
#define DARWIN_SYS_renameatx_np         488
#define DARWIN_SYS_mremap_encrypted     489
#define DARWIN_SYS_netagent_trigger     490
#define DARWIN_SYS_stack_snapshot_with_config 491
#define DARWIN_SYS_microstackshot       492
#define DARWIN_SYS_grab_pgo_data        493
#define DARWIN_SYS_persona              494
#define DARWIN_SYS_mach_eventlink_signal 496
#define DARWIN_SYS_mach_eventlink_wait_until 497
#define DARWIN_SYS_mach_eventlink_signal_wait_until 498
#define DARWIN_SYS_work_interval_ctl    499
#define DARWIN_SYS_getentropy           500
#define DARWIN_SYS_necp_open            501
#define DARWIN_SYS_necp_client_action   502
#define DARWIN_SYS___nexus_open         503
#define DARWIN_SYS___nexus_register     504
#define DARWIN_SYS___nexus_deregister   505
#define DARWIN_SYS___nexus_create       506
#define DARWIN_SYS___nexus_destroy      507
#define DARWIN_SYS___nexus_get_opt      508
#define DARWIN_SYS___nexus_set_opt      509
#define DARWIN_SYS___channel_open       510
#define DARWIN_SYS___channel_get_info   511
#define DARWIN_SYS___channel_sync       512
#define DARWIN_SYS___channel_get_opt    513
#define DARWIN_SYS___channel_set_opt    514
#define DARWIN_SYS_sys_ulock_wait       515
#define DARWIN_SYS_sys_ulock_wake       516
#define DARWIN_SYS_fclonefileat         517

struct darwin_timeval_range_400_plus {
	int64_t tv_sec;
	int32_t tv_usec;
	int32_t pad;
};

struct darwin_rusage_range_400_plus {
	struct darwin_timeval_range_400_plus ru_utime;
	struct darwin_timeval_range_400_plus ru_stime;
	int64_t ru_maxrss;
	int64_t ru_ixrss;
	int64_t ru_idrss;
	int64_t ru_isrss;
	int64_t ru_minflt;
	int64_t ru_majflt;
	int64_t ru_nswap;
	int64_t ru_inblock;
	int64_t ru_oublock;
	int64_t ru_msgsnd;
	int64_t ru_msgrcv;
	int64_t ru_nsignals;
	int64_t ru_nvcsw;
	int64_t ru_nivcsw;
};
#define DARWIN_SYS_fs_snapshot          518
#define DARWIN_SYS_terminate_with_payload 520
#define DARWIN_SYS_abort_with_payload   521
#define DARWIN_SYS_necp_session_open    522
#define DARWIN_SYS_necp_session_action  523
#define DARWIN_SYS_setattrlistat        524
#define DARWIN_SYS_net_qos_guideline    525
#define DARWIN_SYS_fmount               526
#define DARWIN_SYS_ntp_adjtime          527
#define DARWIN_SYS_ntp_gettime          528
#define DARWIN_SYS_os_fault_with_payload 529
#define DARWIN_SYS_kqueue_workloop_ctl  530
#define DARWIN_SYS___mach_bridge_remote_time 531
#define DARWIN_SYS_coalition_ledger     532
#define DARWIN_SYS_log_data             533
#define DARWIN_SYS_memorystatus_available_memory 534
#define DARWIN_SYS_objc_bp_assist_cfg_np 535
#define DARWIN_SYS_shared_region_map_and_slide_2_np 536
#define DARWIN_SYS_pivot_root           537
#define DARWIN_SYS_task_inspect_for_pid 538
#define DARWIN_SYS_task_read_for_pid    539
#define DARWIN_SYS_preadv               540
#define DARWIN_SYS_pwritev              541
#define DARWIN_SYS_preadv_nocancel      542
#define DARWIN_SYS_pwritev_nocancel     543
#define DARWIN_SYS_sys_ulock_wait2      544
#define DARWIN_SYS_proc_info_extended_id 545
#define DARWIN_SYS_tracker_action       546
#define DARWIN_SYS_debug_syscall_reject 547
#define DARWIN_SYS_sys_debug_syscall_reject_config 548
#define DARWIN_SYS_graftdmg             549
#define DARWIN_SYS_map_with_linking_np  550
#define DARWIN_SYS_freadlink            551
#define DARWIN_SYS_sys_record_system_event 552
#define DARWIN_SYS_mkfifoat             553
#define DARWIN_SYS_mknodat              554
#define DARWIN_SYS_ungraftdmg           555
#define DARWIN_SYS_sys_coalition_policy_set 556
#define DARWIN_SYS_sys_coalition_policy_get 557

#define DARWIN_EPERM  1
#define DARWIN_ENOENT 2
#define DARWIN_ENOSYS 78
#define DARWIN_EINTR  4
#define DARWIN_EIO    5
#define DARWIN_EBADF  9
#define DARWIN_ENOMEM 12
#define DARWIN_EFAULT 14
#define DARWIN_ESRCH  3
#define DARWIN_EINVAL 22
#define DARWIN_ENOTDIR 20
#define DARWIN_ENOTSUP 45
#define DARWIN_EEXIST 17

#define SYSCALL_GATE_CARRY 0x20000000u

#define DARWIN_SIG_BLOCK   1
#define DARWIN_SIG_UNBLOCK 2
#define DARWIN_SIG_SETMASK 3

#define DARWIN_UL_COMPARE_AND_WAIT          1
#define DARWIN_UL_UNFAIR_LOCK               2
#define DARWIN_UL_COMPARE_AND_WAIT_SHARED   3
#define DARWIN_UL_UNFAIR_LOCK64_SHARED      4
#define DARWIN_UL_COMPARE_AND_WAIT64        5
#define DARWIN_UL_COMPARE_AND_WAIT64_SHARED 6
#define DARWIN_UL_OPCODE_MASK               0x000000ffu
#define DARWIN_ULF_WAKE_ALL                 0x00000100u
#define DARWIN_ULF_DEADLINE                 0x02000000u

#define DARWIN_MEMORYSTATUS_CMD_GET_PRESSURE_STATUS     4
#define DARWIN_MEMORYSTATUS_CMD_GET_PROCESS_IS_MANAGED  17
#define DARWIN_MEMORYSTATUS_CMD_GET_PROCESS_IS_FREEZABLE 19
#define DARWIN_MEMORYSTATUS_CMD_GET_AGGRESSIVE_JETSAM_LENIENT_MODE 21
#define DARWIN_MEMORYSTATUS_CMD_GET_PROCESS_IS_FROZEN   24

#define DARWIN_KAS_INFO_MAX_SELECTOR 4

#define DARWIN_RLIMIT_WAKEUPS_MONITOR    0x1
#define DARWIN_RLIMIT_CPU_USAGE_MONITOR  0x2
#define DARWIN_RLIMIT_THREAD_CPULIMITS   0x3
#define DARWIN_RLIMIT_FOOTPRINT_INTERVAL 0x4
#define DARWIN_FOOTPRINT_INTERVAL_RESET  0x1

#define DARWIN_BSDTHREAD_CTL_SET_QOS 0x10
#define DARWIN_BSDTHREAD_CTL_GET_QOS 0x20
#define DARWIN_BSDTHREAD_CTL_QOS_OVERRIDE_START 0x40
#define DARWIN_BSDTHREAD_CTL_QOS_OVERRIDE_END 0x80
#define DARWIN_BSDTHREAD_CTL_SET_SELF 0x100
#define DARWIN_BSDTHREAD_CTL_QOS_OVERRIDE_RESET 0x200
#define DARWIN_BSDTHREAD_CTL_QOS_OVERRIDE_DISPATCH 0x400
#define DARWIN_BSDTHREAD_CTL_QOS_DISPATCH_ASYNCHRONOUS_OVERRIDE_ADD 0x401
#define DARWIN_BSDTHREAD_CTL_QOS_DISPATCH_ASYNCHRONOUS_OVERRIDE_RESET 0x402
#define DARWIN_BSDTHREAD_CTL_QOS_MAX_PARALLELISM 0x800
#define DARWIN_BSDTHREAD_CTL_WORKQ_ALLOW_KILL 0x1000
#define DARWIN_BSDTHREAD_CTL_DISPATCH_APPLY_ATTR 0x2000
#define DARWIN_BSDTHREAD_CTL_WORKQ_ALLOW_SIGMASK 0x4000
#define DARWIN_QOS_PARALLELISM_COUNT_LOGICAL 0x1
#define DARWIN_QOS_PARALLELISM_REALTIME 0x2
#define DARWIN_QOS_PARALLELISM_CLUSTER_SHARED_RSRC 0x4
#define DARWIN_THREAD_QOS_UNSPECIFIED 0
#define DARWIN_THREAD_QOS_LAST 7

#define DARWIN_KQ_WORKLOOP_CREATE 0x01
#define DARWIN_KQ_WORKLOOP_DESTROY 0x02
#define DARWIN_KQ_WORKLOOP_CREATE_SCHED_PRI 0x01
#define DARWIN_KQ_WORKLOOP_CREATE_SCHED_POL 0x02
#define DARWIN_KQ_WORKLOOP_CREATE_CPU_PERCENT 0x04
#define DARWIN_KQ_WORKLOOP_CREATE_WORK_INTERVAL 0x08
#define DARWIN_KQ_WORKLOOP_CREATE_WITH_BOUND_THREAD 0x10
#define DARWIN_KQ_WORKLOOP_CREATE_SUPPORTED_FLAGS \
	(DARWIN_KQ_WORKLOOP_CREATE_SCHED_PRI | \
	 DARWIN_KQ_WORKLOOP_CREATE_SCHED_POL | \
	 DARWIN_KQ_WORKLOOP_CREATE_CPU_PERCENT | \
	 DARWIN_KQ_WORKLOOP_CREATE_WORK_INTERVAL | \
	 DARWIN_KQ_WORKLOOP_CREATE_WITH_BOUND_THREAD)
#define DARWIN_POLICY_TIMESHARE 1
#define DARWIN_POLICY_RR 2
#define DARWIN_POLICY_FIFO 4

#define DARWIN_CSR_SYSCALL_CHECK             0
#define DARWIN_CSR_SYSCALL_GET_ACTIVE_CONFIG 1
#define DARWIN_CSR_VALID_FLAGS               0x1fffu

#define DARWIN_COALITION_ID_SELF       2ull
#define DARWIN_COALITION_OP_CREATE     1
#define DARWIN_COALITION_OP_TERMINATE  2
#define DARWIN_COALITION_OP_REAP       3
#define DARWIN_COALITION_INFO_RESOURCE_USAGE 1
#define DARWIN_COALITION_INFO_SET_NAME       2
#define DARWIN_COALITION_INFO_SET_EFFICIENCY 3
#define DARWIN_COALITION_INFO_GET_DEBUG_INFO 4
#define DARWIN_COALITION_LEDGER_SET_LOGICAL_WRITES_LIMIT 1

#define DARWIN_PERSONA_ID_NONE       ((uint32_t)-1)
#define DARWIN_PERSONA_TYPE_DEFAULT  5
#define DARWIN_PERSONA_OP_ALLOC      1
#define DARWIN_PERSONA_OP_PALLOC     2
#define DARWIN_PERSONA_OP_DEALLOC    3
#define DARWIN_PERSONA_OP_GET        4
#define DARWIN_PERSONA_OP_INFO       5
#define DARWIN_PERSONA_OP_PIDINFO    6
#define DARWIN_PERSONA_OP_FIND       7
#define DARWIN_PERSONA_OP_GETPATH    8
#define DARWIN_PERSONA_OP_FIND_BY_TYPE 9
#define DARWIN_PERSONA_NAME_SIZE     256
#define DARWIN_PERSONA_GROUPS        16

#define DARWIN_AT_FDCWD                -2
#define DARWIN_AT_EACCESS              0x0010
#define DARWIN_AT_SYMLINK_NOFOLLOW     0x0020
#define DARWIN_AT_SYMLINK_FOLLOW       0x0040
#define DARWIN_AT_REMOVEDIR            0x0080
#define DARWIN_AT_SYMLINK_NOFOLLOW_ANY 0x0800

#define DARWIN_FSOPT_NOFOLLOW          0x00000001u
#define DARWIN_FSOPT_ATTR_CMN_EXTENDED 0x00000020u
#define DARWIN_FSOPT_NOFIRMLINKPATH    0x00000001u
#define DARWIN_FSOPT_ISREALFSID        0x00000002u

#define DARWIN_ATTR_BIT_MAP_COUNT 5
#define DARWIN_ATTR_CMN_NAME 0x00000001u
#define DARWIN_ATTR_CMN_DEVID 0x00000002u
#define DARWIN_ATTR_CMN_FSID 0x00000004u
#define DARWIN_ATTR_CMN_OBJTYPE 0x00000008u
#define DARWIN_ATTR_CMN_OBJTAG 0x00000010u
#define DARWIN_ATTR_CMN_CRTIME 0x00000200u
#define DARWIN_ATTR_CMN_MODTIME 0x00000400u
#define DARWIN_ATTR_CMN_CHGTIME 0x00000800u
#define DARWIN_ATTR_CMN_ACCTIME 0x00001000u
#define DARWIN_ATTR_CMN_OWNERID 0x00008000u
#define DARWIN_ATTR_CMN_GRPID 0x00010000u
#define DARWIN_ATTR_CMN_ACCESSMASK 0x00020000u
#define DARWIN_ATTR_CMN_FLAGS 0x00040000u
#define DARWIN_ATTR_CMN_USERACCESS 0x00200000u
#define DARWIN_ATTR_CMN_FILEID 0x02000000u
#define DARWIN_ATTR_CMN_FULLPATH 0x08000000u
#define DARWIN_ATTR_CMN_RETURNED_ATTRS 0x80000000u
#define DARWIN_ATTR_DIR_LINKCOUNT 0x00000001u
#define DARWIN_ATTR_DIR_ENTRYCOUNT 0x00000002u
#define DARWIN_ATTR_DIR_IOBLOCKSIZE 0x00000010u
#define DARWIN_ATTR_DIR_DATALENGTH 0x00000020u
#define DARWIN_ATTR_FILE_LINKCOUNT 0x00000001u
#define DARWIN_ATTR_FILE_TOTALSIZE 0x00000002u
#define DARWIN_ATTR_FILE_ALLOCSIZE 0x00000004u
#define DARWIN_ATTR_FILE_IOBLOCKSIZE 0x00000008u
#define DARWIN_ATTR_FILE_DATALENGTH 0x00000200u
#define DARWIN_ATTR_FILE_DATAALLOCSIZE 0x00000400u
#define DARWIN_ATTR_CMN_SUPPORTED \
	(DARWIN_ATTR_CMN_NAME | DARWIN_ATTR_CMN_DEVID | \
	 DARWIN_ATTR_CMN_FSID | DARWIN_ATTR_CMN_OBJTYPE | \
	 DARWIN_ATTR_CMN_OBJTAG | DARWIN_ATTR_CMN_CRTIME | \
	 DARWIN_ATTR_CMN_MODTIME | DARWIN_ATTR_CMN_CHGTIME | \
	 DARWIN_ATTR_CMN_ACCTIME | DARWIN_ATTR_CMN_OWNERID | \
	 DARWIN_ATTR_CMN_GRPID | DARWIN_ATTR_CMN_ACCESSMASK | \
	 DARWIN_ATTR_CMN_FLAGS | DARWIN_ATTR_CMN_USERACCESS | \
	 DARWIN_ATTR_CMN_FILEID | DARWIN_ATTR_CMN_FULLPATH | \
	 DARWIN_ATTR_CMN_RETURNED_ATTRS)
#define DARWIN_ATTR_CMN_SET_SUPPORTED \
	(DARWIN_ATTR_CMN_MODTIME | DARWIN_ATTR_CMN_ACCTIME | \
	 DARWIN_ATTR_CMN_OWNERID | DARWIN_ATTR_CMN_GRPID | \
	 DARWIN_ATTR_CMN_ACCESSMASK)
#define DARWIN_ATTR_DIR_SUPPORTED \
	(DARWIN_ATTR_DIR_LINKCOUNT | DARWIN_ATTR_DIR_ENTRYCOUNT | \
	 DARWIN_ATTR_DIR_IOBLOCKSIZE | DARWIN_ATTR_DIR_DATALENGTH)
#define DARWIN_ATTR_FILE_SUPPORTED \
	(DARWIN_ATTR_FILE_LINKCOUNT | DARWIN_ATTR_FILE_TOTALSIZE | \
	 DARWIN_ATTR_FILE_ALLOCSIZE | DARWIN_ATTR_FILE_IOBLOCKSIZE | \
	 DARWIN_ATTR_FILE_DATALENGTH | DARWIN_ATTR_FILE_DATAALLOCSIZE)
#define DARWIN_ATTR_BULK_REQUIRED \
	(DARWIN_ATTR_CMN_NAME | DARWIN_ATTR_CMN_RETURNED_ATTRS)

#define DARWIN_CLONE_NOFOLLOW        0x0001u
#define DARWIN_CLONE_NOOWNERCOPY     0x0002u
#define DARWIN_CLONE_ACL             0x0004u
#define DARWIN_CLONE_NOFOLLOW_ANY    0x0008u
#define DARWIN_CLONE_RESOLVE_BENEATH 0x0010u
#define DARWIN_CLONE_SUPPORTED_FLAGS \
	(DARWIN_CLONE_NOFOLLOW | DARWIN_CLONE_NOOWNERCOPY | DARWIN_CLONE_ACL)

#define DARWIN_RENAME_SECLUDE 0x00000001u
#define DARWIN_RENAME_SWAP    0x00000002u
#define DARWIN_RENAME_EXCL    0x00000004u

#define LINUX_RENAME_NOREPLACE 0x00000001u
#define LINUX_RENAME_EXCHANGE  0x00000002u

#define DARWIN_O_NONBLOCK  0x00000004
#define DARWIN_O_APPEND    0x00000008
#define DARWIN_O_NOFOLLOW  0x00000100
#define DARWIN_O_CREAT     0x00000200
#define DARWIN_O_TRUNC     0x00000400
#define DARWIN_O_EXCL      0x00000800
#define DARWIN_O_NOCTTY    0x00020000
#define DARWIN_O_DIRECTORY 0x00100000
#define DARWIN_O_SYMLINK   0x00200000
#define DARWIN_O_DSYNC     0x00400000
#define DARWIN_O_CLOEXEC   0x01000000

#define DARWIN_F_GETOWN    5
#define DARWIN_F_SETOWN    6
#define DARWIN_F_GETLK     7
#define DARWIN_F_SETLK     8
#define DARWIN_F_SETLKW    9
#define DARWIN_F_FULLFSYNC 51
#define DARWIN_F_DUPFD_CLOEXEC 67

#define DARWIN_FD_CLOEXEC 1
#define DARWIN_FD_CLOFORK 2

#define DARWIN_FD_SETSIZE 1024
#define DARWIN_NFDBITS   32

#define DARWIN_AF_UNIX   1
#define DARWIN_AF_INET   2
#define DARWIN_AF_INET6 30

#define DARWIN_SYS_RANGE_200_SEM_WAIT        271
#define DARWIN_SYS_RANGE_200_AIO_SUSPEND     315
#define DARWIN_SYS_RANGE_200_SEMWAIT_SIGNAL  334
#define DARWIN_SYS_RANGE_200_MSGSND          260
#define DARWIN_SYS_RANGE_200_MSGRCV          261

#define DARWIN_GUARD_CLOSE       (1u << 0)
#define DARWIN_GUARD_DUP         (1u << 1)
#define DARWIN_GUARD_SOCKET_IPC  (1u << 2)
#define DARWIN_GUARD_FILEPORT    (1u << 3)
#define DARWIN_GUARD_WRITE       (1u << 4)
#define DARWIN_GUARD_REQUIRED    DARWIN_GUARD_DUP
#define DARWIN_GUARD_ALL         (DARWIN_GUARD_CLOSE | \
                                  DARWIN_GUARD_DUP | \
                                  DARWIN_GUARD_SOCKET_IPC | \
                                  DARWIN_GUARD_FILEPORT | \
                                  DARWIN_GUARD_WRITE)

#define DARWIN_O_DP_GETRAWENCRYPTED   0x0001
#define DARWIN_O_DP_GETRAWUNENCRYPTED 0x0002

#define MACHGATE_FD_GUARD_MAX 1024

#define DARWIN_PROC_FLAG_LP64 0x10u
#define DARWIN_PROC_STATUS_RUN 2u
#define DARWIN_PROC_INFO_CALL_PIDINFO 2
#define DARWIN_PROC_PIDT_SHORTBSDINFO 13
#define DARWIN_MAXCOMLEN 16
#define MACHGATE_AUDIT_SESSION_PORT 0x41530001u
#define MACHGATE_FILEPORT_BASE 0x4f000000u
#define MACHGATE_FILEPORT_MAX 64
#define MACHGATE_KQUEUE_WORKLOOP_MAX 64

struct darwin_mac_label_ref_range_400_plus {
	uint64_t buffer_length;
	uint64_t buffer;
};

struct machgate_fileport_entry {
	uint32_t port_name;
	int fd;
};

struct machgate_fd_guard_entry {
	int fd;
	uint64_t guard;
	uint32_t guardflags;
	int fdflags;
	int in_use;
};

struct machgate_kqueue_workloop_entry {
	uint64_t id;
	int fd;
	int in_use;
};

struct darwin_kqueue_workloop_params_range_400_plus {
	int32_t version;
	int32_t flags;
	uint64_t id;
	int32_t sched_pri;
	int32_t sched_pol;
	int32_t cpu_percent;
	int32_t cpu_refillms;
	uint32_t wi_port;
} __attribute__((packed));

struct darwin_ntptimeval_range_400_plus {
	struct timespec time;
	int64_t maxerror;
	int64_t esterror;
	int64_t tai;
	int32_t time_state;
	int32_t __pad;
};

struct darwin_timex_range_400_plus {
	uint32_t modes;
	uint32_t __pad0;
	int64_t offset;
	int64_t freq;
	int64_t maxerror;
	int64_t esterror;
	int32_t status;
	int32_t __pad1;
	int64_t constant;
	int64_t precision;
	int64_t tolerance;
	int64_t ppsfreq;
	int64_t jitter;
	int32_t shift;
	int32_t __pad2;
	int64_t stabil;
	int64_t jitcnt;
	int64_t calcnt;
	int64_t errcnt;
	int64_t stbcnt;
};

struct darwin_stat_range_400_plus {
	int32_t         st_dev;
	uint16_t        st_mode;
	uint16_t        st_nlink;
	uint64_t        st_ino;
	uint32_t        st_uid;
	uint32_t        st_gid;
	int32_t         st_rdev;
	int32_t         __pad0;
	struct timespec st_atimespec;
	struct timespec st_mtimespec;
	struct timespec st_ctimespec;
	struct timespec st_birthtimespec;
	int64_t         st_size;
	int64_t         st_blocks;
	int32_t         st_blksize;
	uint32_t        st_flags;
	uint32_t        st_gen;
	int32_t         st_lspare;
	int64_t         st_qspare[2];
};

struct darwin_fsid_range_400_plus {
	int32_t val[2];
};

struct darwin_fsobj_id_range_400_plus {
	uint32_t fid_objno;
	uint32_t fid_generation;
};

struct darwin_attrlist_range_400_plus {
	uint16_t bitmapcount;
	uint16_t reserved;
	uint32_t commonattr;
	uint32_t volattr;
	uint32_t dirattr;
	uint32_t fileattr;
	uint32_t forkattr;
};

struct darwin_attribute_set_range_400_plus {
	uint32_t commonattr;
	uint32_t volattr;
	uint32_t dirattr;
	uint32_t fileattr;
	uint32_t forkattr;
};

struct darwin_attrreference_range_400_plus {
	int32_t attr_dataoffset;
	uint32_t attr_length;
};

struct darwin_attr_context_range_400_plus {
	struct syscall_gate_state* state;
	const struct darwin_attrlist_range_400_plus* attrlist;
	char* buffer;
	size_t capacity;
	size_t fixed_offset;
	size_t variable_offset;
};

struct darwin_msghdr_range_400_plus {
	void* msg_name;
	uint32_t msg_namelen;
	struct iovec* msg_iov;
	int32_t msg_iovlen;
	void* msg_control;
	uint32_t msg_controllen;
	int32_t msg_flags;
};

struct darwin_msghdr_x_range_400_plus {
	void* msg_name;
	uint32_t msg_namelen;
	struct iovec* msg_iov;
	int32_t msg_iovlen;
	void* msg_control;
	uint32_t msg_controllen;
	int32_t msg_flags;
	uint64_t msg_datalen;
};

struct darwin_sa_endpoints_range_400_plus {
	uint32_t sae_srcif;
	uint32_t __pad0;
	void* sae_srcaddr;
	uint32_t sae_srcaddrlen;
	uint32_t __pad1;
	void* sae_dstaddr;
	uint32_t sae_dstaddrlen;
	uint32_t __pad2;
};

struct darwin_net_qos_param_range_400_plus {
	uint64_t nq_transfer_size;
	uint32_t nq_flags;
	uint32_t nq_unused;
};

struct darwin_proc_bsdshortinfo_range_400_plus {
	uint32_t pbsi_pid;
	uint32_t pbsi_ppid;
	uint32_t pbsi_pgid;
	uint32_t pbsi_status;
	char pbsi_comm[DARWIN_MAXCOMLEN];
	uint32_t pbsi_flags;
	uint32_t pbsi_uid;
	uint32_t pbsi_gid;
	uint32_t pbsi_ruid;
	uint32_t pbsi_rgid;
	uint32_t pbsi_svuid;
	uint32_t pbsi_svgid;
	uint32_t pbsi_rfu;
};

struct darwin_kpersona_info_range_400_plus {
	uint32_t persona_info_version;
	uint32_t persona_id;
	int32_t persona_type;
	uint32_t persona_gid;
	uint32_t persona_ngroups;
	uint32_t persona_groups[DARWIN_PERSONA_GROUPS];
	uint32_t persona_gmuid;
	char persona_name[DARWIN_PERSONA_NAME_SIZE];
	uint32_t persona_uid;
} __attribute__((packed));

struct linux_dirent64_range_400_plus {
	uint64_t d_ino;
	int64_t d_off;
	unsigned short d_reclen;
	unsigned char d_type;
	char d_name[];
};

_Static_assert(sizeof(struct darwin_stat_range_400_plus) == 144,
	"darwin_stat_range_400_plus must be 144 bytes");

_Static_assert(sizeof(struct darwin_fsid_range_400_plus) == 8,
	"darwin_fsid_range_400_plus must be 8 bytes");

_Static_assert(sizeof(struct darwin_fsobj_id_range_400_plus) == 8,
	"darwin_fsobj_id_range_400_plus must be 8 bytes");

_Static_assert(sizeof(struct darwin_attrlist_range_400_plus) == 24,
	"darwin_attrlist_range_400_plus must be 24 bytes");

_Static_assert(sizeof(struct darwin_attribute_set_range_400_plus) == 20,
	"darwin_attribute_set_range_400_plus must be 20 bytes");

_Static_assert(sizeof(struct darwin_attrreference_range_400_plus) == 8,
	"darwin_attrreference_range_400_plus must be 8 bytes");

_Static_assert(sizeof(struct darwin_msghdr_range_400_plus) == 48,
	"darwin_msghdr_range_400_plus must be 48 bytes");

_Static_assert(sizeof(struct darwin_msghdr_x_range_400_plus) == 56,
	"darwin_msghdr_x_range_400_plus must be 56 bytes");

_Static_assert(sizeof(struct darwin_sa_endpoints_range_400_plus) == 40,
	"darwin_sa_endpoints_range_400_plus must be 40 bytes");

_Static_assert(sizeof(struct darwin_net_qos_param_range_400_plus) == 16,
	"darwin_net_qos_param_range_400_plus must be 16 bytes");

_Static_assert(sizeof(struct darwin_proc_bsdshortinfo_range_400_plus) == 64,
	"darwin_proc_bsdshortinfo_range_400_plus must be 64 bytes");

_Static_assert(sizeof(struct darwin_kpersona_info_range_400_plus) == 348,
	"darwin_kpersona_info_range_400_plus must be 348 bytes");

_Static_assert(sizeof(struct darwin_timeval_range_400_plus) == 16,
	"darwin_timeval_range_400_plus must be 16 bytes");

_Static_assert(sizeof(struct darwin_ntptimeval_range_400_plus) == 48,
	"darwin_ntptimeval_range_400_plus must be 48 bytes");

_Static_assert(sizeof(struct darwin_timex_range_400_plus) == 136,
	"darwin_timex_range_400_plus must be 136 bytes");

_Static_assert(sizeof(struct darwin_mac_label_ref_range_400_plus) == 16,
	"darwin_mac_label_ref_range_400_plus must be 16 bytes");

_Static_assert(sizeof(struct darwin_kqueue_workloop_params_range_400_plus) == 36,
	"darwin_kqueue_workloop_params_range_400_plus must be 36 bytes");

static struct machgate_fileport_entry fileport_entries[MACHGATE_FILEPORT_MAX];
static struct machgate_fd_guard_entry fd_guard_entries[MACHGATE_FD_GUARD_MAX];
static struct machgate_kqueue_workloop_entry
	kqueue_workloop_entries[MACHGATE_KQUEUE_WORKLOOP_MAX];

static void set_success(struct syscall_gate_state* state, uint64_t result)
{
	state->x[0] = result;
	state->nzcv &= ~SYSCALL_GATE_CARRY;
}

static void set_failure(struct syscall_gate_state* state, int err)
{
	state->x[0] = (uint64_t)err;
	state->nzcv |= SYSCALL_GATE_CARRY;
}

static int linux_errno_to_darwin(int err)
{
	switch (err) {
	case EPERM: return 1;
	case ENOENT: return 2;
	case ESRCH: return 3;
	case EINTR: return 4;
	case EIO: return 5;
	case ENXIO: return 6;
	case E2BIG: return 7;
	case ENOEXEC: return 8;
	case EBADF: return 9;
	case ECHILD: return 10;
	case EAGAIN: return 35;
	case EDEADLK: return 11;
	case ENOMEM: return 12;
	case EACCES: return 13;
	case EFAULT: return 14;
#ifdef ENOTBLK
	case ENOTBLK: return 15;
#endif
	case EBUSY: return 16;
	case EEXIST: return 17;
	case EXDEV: return 18;
	case ENODEV: return 19;
	case ENOTDIR: return 20;
	case EISDIR: return 21;
	case EINVAL: return 22;
	case ENFILE: return 23;
	case EMFILE: return 24;
	case ENOTTY: return 25;
	case ETXTBSY: return 26;
	case EFBIG: return 27;
	case ENOSPC: return 28;
	case ESPIPE: return 29;
	case EROFS: return 30;
	case EMLINK: return 31;
	case EPIPE: return 32;
	case EDOM: return 33;
	case ERANGE: return 34;
	case EINPROGRESS: return 36;
	case EALREADY: return 37;
	case ENOTSOCK: return 38;
	case EDESTADDRREQ: return 39;
	case EMSGSIZE: return 40;
	case EPROTOTYPE: return 41;
	case ENOPROTOOPT: return 42;
	case EPROTONOSUPPORT: return 43;
#ifdef ESOCKTNOSUPPORT
	case ESOCKTNOSUPPORT: return 44;
#endif
	case ENOTSUP: return 45;
#if defined(EOPNOTSUPP) && EOPNOTSUPP != ENOTSUP
	case EOPNOTSUPP: return 45;
#endif
#ifdef EPFNOSUPPORT
	case EPFNOSUPPORT: return 46;
#endif
	case EAFNOSUPPORT: return 47;
	case EADDRINUSE: return 48;
	case EADDRNOTAVAIL: return 49;
	case ENETDOWN: return 50;
	case ENETUNREACH: return 51;
	case ENETRESET: return 52;
	case ECONNABORTED: return 53;
	case ECONNRESET: return 54;
	case ENOBUFS: return 55;
	case EISCONN: return 56;
	case ENOTCONN: return 57;
#ifdef ESHUTDOWN
	case ESHUTDOWN: return 58;
#endif
#ifdef ETOOMANYREFS
	case ETOOMANYREFS: return 59;
#endif
	case ETIMEDOUT: return 60;
	case ECONNREFUSED: return 61;
	case ELOOP: return 62;
	case ENAMETOOLONG: return 63;
#ifdef EHOSTDOWN
	case EHOSTDOWN: return 64;
#endif
	case EHOSTUNREACH: return 65;
	case ENOTEMPTY: return 66;
#ifdef EPROCLIM
	case EPROCLIM: return 67;
#endif
#ifdef EUSERS
	case EUSERS: return 68;
#endif
	case EDQUOT: return 69;
	case ESTALE: return 70;
#ifdef EREMOTE
	case EREMOTE: return 71;
#endif
#ifdef EBADRPC
	case EBADRPC: return 72;
#endif
#ifdef ERPCMISMATCH
	case ERPCMISMATCH: return 73;
#endif
#ifdef EPROGUNAVAIL
	case EPROGUNAVAIL: return 74;
#endif
#ifdef EPROGMISMATCH
	case EPROGMISMATCH: return 75;
#endif
#ifdef EPROCUNAVAIL
	case EPROCUNAVAIL: return 76;
#endif
	case ENOLCK: return 77;
	case ENOSYS: return DARWIN_ENOSYS;
	default: return DARWIN_EIO;
	}
}

static void set_errno_failure(struct syscall_gate_state* state)
{
	set_failure(state, linux_errno_to_darwin(errno ? errno : EIO));
}

static int fd_is_valid(int fd);
static int translate_open_flags(int darwin_flags);

static void finish_syscall_result(struct syscall_gate_state* state, long result)
{
	if (result < 0)
		set_errno_failure(state);
	else
		set_success(state, (uint64_t)result);
}

static void set_unsupported_feature(struct syscall_gate_state* state)
{
	set_failure(state, DARWIN_ENOTSUP);
}

static int fd_guard_flags_are_valid(uint32_t guardflags)
{
	return (guardflags & DARWIN_GUARD_REQUIRED) == DARWIN_GUARD_REQUIRED &&
	       (guardflags & ~DARWIN_GUARD_ALL) == 0;
}

static uint64_t read_guard_value(const uint64_t* guard)
{
	if (!guard)
		return 0;
	return *guard;
}

static struct machgate_fd_guard_entry* fd_guard_entry_for_fd(int fd)
{
	for (int index = 0; index < MACHGATE_FD_GUARD_MAX; index++) {
		if (fd_guard_entries[index].in_use &&
		    fd_guard_entries[index].fd == fd)
			return &fd_guard_entries[index];
	}

	return NULL;
}

static void fd_guard_remove(int fd)
{
	struct machgate_fd_guard_entry* entry = fd_guard_entry_for_fd(fd);

	if (entry)
		memset(entry, 0, sizeof(*entry));
}

static int fd_guard_current_flags(int fd, int guardflags)
{
	int fdflags = 0;

	errno = 0;
	long result = syscall(SYS_fcntl, fd, F_GETFD);
	if (result >= 0 && (result & FD_CLOEXEC))
		fdflags |= DARWIN_FD_CLOEXEC;
	if (guardflags & DARWIN_GUARD_CLOSE)
		fdflags |= DARWIN_FD_CLOFORK;

	return fdflags;
}

static int fd_guard_set_linux_cloexec(int fd, int darwin_fdflags)
{
	errno = 0;
	long result = syscall(SYS_fcntl, fd, F_GETFD);
	if (result < 0)
		return 0;

	int linux_flags = (int)result;
	if (darwin_fdflags & DARWIN_FD_CLOEXEC)
		linux_flags |= FD_CLOEXEC;
	else
		linux_flags &= ~FD_CLOEXEC;

	errno = 0;
	return syscall(SYS_fcntl, fd, F_SETFD, linux_flags) == 0;
}

static int fd_guard_register(int fd, uint64_t guard, uint32_t guardflags)
{
	struct machgate_fd_guard_entry* free_entry = NULL;

	fd_guard_remove(fd);

	for (int index = 0; index < MACHGATE_FD_GUARD_MAX; index++) {
		if (!fd_guard_entries[index].in_use) {
			free_entry = &fd_guard_entries[index];
			break;
		}
	}

	if (!free_entry)
		return 0;

	free_entry->fd = fd;
	free_entry->guard = guard;
	free_entry->guardflags = guardflags;
	free_entry->fdflags = fd_guard_current_flags(fd, guardflags);
	free_entry->in_use = 1;
	return 1;
}

static int fd_guard_validate(struct syscall_gate_state* state, int fd,
                             const uint64_t* guard,
                             struct machgate_fd_guard_entry** entry_out)
{
	if (!guard) {
		set_failure(state, DARWIN_EFAULT);
		return 0;
	}

	struct machgate_fd_guard_entry* entry = fd_guard_entry_for_fd(fd);
	if (!entry) {
		if (!fd_is_valid(fd))
			set_failure(state, DARWIN_EBADF);
		else
			set_failure(state, DARWIN_EINVAL);
		return 0;
	}

	if (read_guard_value(guard) != entry->guard) {
		set_failure(state, DARWIN_EPERM);
		return 0;
	}

	if (entry_out)
		*entry_out = entry;
	return 1;
}

static int fd_guard_prepare_new(struct syscall_gate_state* state,
                                const uint64_t* guard,
                                uint32_t guardflags,
                                uint64_t* guard_out)
{
	if (!guard) {
		set_failure(state, DARWIN_EFAULT);
		return 0;
	}

	if (!fd_guard_flags_are_valid(guardflags)) {
		set_failure(state, DARWIN_EINVAL);
		return 0;
	}

	*guard_out = read_guard_value(guard);
	if (*guard_out == 0) {
		set_failure(state, DARWIN_EINVAL);
		return 0;
	}

	return 1;
}

static void dispatch_guarded_open(struct syscall_gate_state* state)
{
	uint64_t guard;
	int flags = (int)state->x[3];

	if ((flags & DARWIN_O_CLOEXEC) == 0) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	if (!fd_guard_prepare_new(state, (const uint64_t*)state->x[1],
	                          (uint32_t)state->x[2], &guard))
		return;

	errno = 0;
	long result = syscall(SYS_openat, AT_FDCWD, (const char*)state->x[0],
	                      translate_open_flags(flags) | O_CLOEXEC,
	                      (mode_t)state->x[4]);
	if (result < 0) {
		set_errno_failure(state);
		return;
	}

	if (!fd_guard_register((int)result, guard, (uint32_t)state->x[2])) {
		syscall(SYS_close, (int)result);
		set_failure(state, DARWIN_ENOMEM);
		return;
	}

	set_success(state, (uint64_t)result);
}

static void dispatch_guarded_open_dprotected(struct syscall_gate_state* state)
{
	uint64_t guard;
	int flags = (int)state->x[3];
	int dpflags = (int)state->x[5];

	if ((flags & DARWIN_O_CLOEXEC) == 0) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	if ((dpflags & (DARWIN_O_DP_GETRAWENCRYPTED |
	                DARWIN_O_DP_GETRAWUNENCRYPTED)) &&
	    (flags & (O_WRONLY | O_RDWR))) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	if (!fd_guard_prepare_new(state, (const uint64_t*)state->x[1],
	                          (uint32_t)state->x[2], &guard))
		return;

	errno = 0;
	long result = syscall(SYS_openat, AT_FDCWD, (const char*)state->x[0],
	                      translate_open_flags(flags) | O_CLOEXEC,
	                      (mode_t)state->x[6]);
	if (result < 0) {
		set_errno_failure(state);
		return;
	}

	if (!fd_guard_register((int)result, guard, (uint32_t)state->x[2])) {
		syscall(SYS_close, (int)result);
		set_failure(state, DARWIN_ENOMEM);
		return;
	}

	set_success(state, (uint64_t)result);
}

static void dispatch_guarded_kqueue(struct syscall_gate_state* state)
{
	uint64_t guard;

	if (!fd_guard_prepare_new(state, (const uint64_t*)state->x[0],
	                          (uint32_t)state->x[1], &guard))
		return;

	errno = 0;
	long result = syscall(SYS_eventfd2, 0, EFD_CLOEXEC);
	if (result < 0) {
		set_errno_failure(state);
		return;
	}

	if (!fd_guard_register((int)result, guard, (uint32_t)state->x[1])) {
		syscall(SYS_close, (int)result);
		set_failure(state, DARWIN_ENOMEM);
		return;
	}

	set_success(state, (uint64_t)result);
}

static void dispatch_guarded_close(struct syscall_gate_state* state)
{
	int fd = (int)state->x[0];

	if (!fd_guard_validate(state, fd, (const uint64_t*)state->x[1], NULL))
		return;

	errno = 0;
	long result = syscall(SYS_close, fd);
	if (result < 0) {
		set_errno_failure(state);
		return;
	}

	fd_guard_remove(fd);
	set_success(state, 0);
}

static void dispatch_change_fdguard(struct syscall_gate_state* state)
{
	int fd = (int)state->x[0];
	const uint64_t* old_guard_ptr = (const uint64_t*)state->x[1];
	uint32_t old_flags = (uint32_t)state->x[2];
	const uint64_t* new_guard_ptr = (const uint64_t*)state->x[3];
	uint32_t new_flags = (uint32_t)state->x[4];
	int* fdflagsp = (int*)state->x[5];
	struct machgate_fd_guard_entry* entry = fd_guard_entry_for_fd(fd);
	int requested_fdflags = 0;

	if (!fd_is_valid(fd)) {
		set_failure(state, DARWIN_EBADF);
		return;
	}

	if (fdflagsp)
		requested_fdflags = *fdflagsp;

	if (fdflagsp) {
		int old_fdflags = entry ? entry->fdflags :
		                  fd_guard_current_flags(fd, 0);
		*fdflagsp = old_fdflags;
	}

	if (entry) {
		if (!old_guard_ptr || old_flags == 0) {
			set_failure(state, DARWIN_EINVAL);
			return;
		}
		if (read_guard_value(old_guard_ptr) == 0) {
			set_failure(state, DARWIN_EPERM);
			return;
		}
		if (read_guard_value(old_guard_ptr) != entry->guard ||
		    old_flags != entry->guardflags) {
			set_failure(state, DARWIN_EPERM);
			return;
		}
	} else if (old_guard_ptr || old_flags != 0) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	if (new_guard_ptr) {
		uint64_t new_guard = read_guard_value(new_guard_ptr);
		if (new_guard == 0 || !fd_guard_flags_are_valid(new_flags)) {
			set_failure(state, DARWIN_EINVAL);
			return;
		}
		if (!entry && !fd_guard_set_linux_cloexec(fd, DARWIN_FD_CLOEXEC)) {
			set_errno_failure(state);
			return;
		}
		if (!fd_guard_register(fd, new_guard, new_flags)) {
			set_failure(state, DARWIN_ENOMEM);
			return;
		}
		entry = fd_guard_entry_for_fd(fd);
		if (entry)
			entry->fdflags = DARWIN_FD_CLOEXEC |
			                 ((new_flags & DARWIN_GUARD_CLOSE) ||
			                  (requested_fdflags & DARWIN_FD_CLOFORK) ?
			                  DARWIN_FD_CLOFORK : 0);
		set_success(state, 0);
		return;
	}

	if (new_flags != 0 || !entry) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	fd_guard_remove(fd);
	if (!fd_guard_set_linux_cloexec(fd, requested_fdflags)) {
		set_errno_failure(state);
		return;
	}
	set_success(state, 0);
}

static void dispatch_guarded_write(struct syscall_gate_state* state)
{
	if (!fd_guard_validate(state, (int)state->x[0],
	                       (const uint64_t*)state->x[1], NULL))
		return;

	long result = machgate_syscall_write_no_sigpipe((int)state->x[0],
	                                                (const void*)state->x[2],
	                                                (size_t)state->x[3]);
	finish_syscall_result(state, result);
}

static void dispatch_guarded_pwrite(struct syscall_gate_state* state)
{
	if (!fd_guard_validate(state, (int)state->x[0],
	                       (const uint64_t*)state->x[1], NULL))
		return;

	errno = 0;
	long result = syscall(SYS_pwrite64, (int)state->x[0],
	                      (const void*)state->x[2],
	                      (size_t)state->x[3], (off_t)state->x[4]);
	finish_syscall_result(state, result);
}

static void dispatch_guarded_writev(struct syscall_gate_state* state)
{
	if (!fd_guard_validate(state, (int)state->x[0],
	                       (const uint64_t*)state->x[1], NULL))
		return;

	long result = machgate_syscall_writev_no_sigpipe((int)state->x[0],
	                                                 (const struct iovec*)state->x[2],
	                                                 (int)state->x[3]);
	finish_syscall_result(state, result);
}

static void dispatch_mac_get_label_range_400_plus(struct syscall_gate_state* state,
                                                  uint64_t mac_argument)
{
	struct darwin_mac_label_ref_range_400_plus* mac_label =
		(struct darwin_mac_label_ref_range_400_plus*)mac_argument;

	if (!mac_label || !mac_label->buffer || mac_label->buffer_length == 0) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	*(char*)(uintptr_t)mac_label->buffer = '\0';
	set_success(state, 0);
}

static void dispatch_audit_session_self(struct syscall_gate_state* state)
{
	set_success(state, MACHGATE_AUDIT_SESSION_PORT);
}

static void dispatch_audit_session_join(struct syscall_gate_state* state)
{
	uint32_t port_name = (uint32_t)state->x[0];

	if (port_name != MACHGATE_AUDIT_SESSION_PORT) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	set_success(state, 0);
}

static void dispatch_audit_session_port(struct syscall_gate_state* state)
{
	uint32_t* port_name = (uint32_t*)state->x[1];

	if (!port_name) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	*port_name = MACHGATE_AUDIT_SESSION_PORT;
	set_success(state, 0);
}

static int fileport_index_for_port(uint32_t port_name)
{
	for (int index = 0; index < MACHGATE_FILEPORT_MAX; index++) {
		if (fileport_entries[index].port_name == port_name)
			return index;
	}

	return -1;
}

static void dispatch_fileport_makeport(struct syscall_gate_state* state)
{
	uint32_t* port_name = (uint32_t*)state->x[1];

	if (!port_name) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	errno = 0;
	long dup_result = syscall(SYS_fcntl, (int)state->x[0], F_DUPFD_CLOEXEC, 0);
	if (dup_result < 0) {
		set_errno_failure(state);
		return;
	}

	for (int index = 0; index < MACHGATE_FILEPORT_MAX; index++) {
		if (fileport_entries[index].port_name != 0)
			continue;

		fileport_entries[index].fd = (int)dup_result;
		fileport_entries[index].port_name = MACHGATE_FILEPORT_BASE +
		                                    (uint32_t)index + 1u;
		*port_name = fileport_entries[index].port_name;
		set_success(state, 0);
		return;
	}

	syscall(SYS_close, (int)dup_result);
	set_failure(state, DARWIN_ENOMEM);
}

static void dispatch_fileport_makefd(struct syscall_gate_state* state)
{
	uint32_t port_name = (uint32_t)state->x[0];
	int index = fileport_index_for_port(port_name);

	if (index < 0) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	errno = 0;
	long result = syscall(SYS_fcntl, fileport_entries[index].fd,
	                      F_DUPFD_CLOEXEC, 0);
	finish_syscall_result(state, result);
}

static void dispatch_eventlink_missing_port(struct syscall_gate_state* state)
{
	set_failure(state, DARWIN_EINVAL);
}

static void dispatch_task_port_request(struct syscall_gate_state* state)
{
	uint32_t* port_name = (uint32_t*)state->x[2];

	if (!port_name) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	*port_name = 0;
	set_failure(state, DARWIN_EPERM);
}

static void read_proc_comm_range_400_plus(char* buffer, size_t buffer_size)
{
	if (!buffer || buffer_size == 0)
		return;

	memset(buffer, 0, buffer_size);
	int fd = (int)syscall(SYS_openat, AT_FDCWD, "/proc/self/comm",
	                      O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		strncpy(buffer, "machgate", buffer_size - 1);
		return;
	}

	ssize_t result = (ssize_t)syscall(SYS_read, fd, buffer, buffer_size - 1);
	syscall(SYS_close, fd);
	if (result <= 0) {
		strncpy(buffer, "machgate", buffer_size - 1);
		return;
	}

	for (ssize_t i = 0; i < result; i++) {
		if (buffer[i] == '\n') {
			buffer[i] = '\0';
			return;
		}
	}
}

static int proc_pid_is_supported_range_400_plus(pid_t pid)
{
	pid_t self = (pid_t)syscall(SYS_getpid);
	return pid == 0 || pid == self;
}

static void fill_proc_bsdshortinfo_range_400_plus(
	struct darwin_proc_bsdshortinfo_range_400_plus* info,
	pid_t pid)
{
	memset(info, 0, sizeof(*info));
	info->pbsi_pid = (uint32_t)pid;
	info->pbsi_ppid = (uint32_t)syscall(SYS_getppid);
	info->pbsi_pgid = (uint32_t)syscall(SYS_getpgid, 0);
	info->pbsi_status = DARWIN_PROC_STATUS_RUN;
	info->pbsi_flags = DARWIN_PROC_FLAG_LP64;
	info->pbsi_uid = (uint32_t)syscall(SYS_getuid);
	info->pbsi_gid = (uint32_t)syscall(SYS_getgid);
	info->pbsi_ruid = info->pbsi_uid;
	info->pbsi_rgid = info->pbsi_gid;
	info->pbsi_svuid = info->pbsi_uid;
	info->pbsi_svgid = info->pbsi_gid;
	read_proc_comm_range_400_plus(info->pbsi_comm, sizeof(info->pbsi_comm));
}

static void dispatch_proc_info_extended_id(struct syscall_gate_state* state)
{
	int32_t callnum = (int32_t)state->x[0];
	pid_t pid = (pid_t)(int32_t)state->x[1];
	uint32_t flavor = (uint32_t)state->x[2];
	uint32_t flags = (uint32_t)state->x[3];
	void* buffer = (void*)state->x[6];
	int32_t buffer_size = (int32_t)state->x[7];
	pid_t self = (pid_t)syscall(SYS_getpid);

	if (flags != 0 || callnum != DARWIN_PROC_INFO_CALL_PIDINFO ||
	    flavor != DARWIN_PROC_PIDT_SHORTBSDINFO) {
		set_failure(state, DARWIN_ENOSYS);
		return;
	}

	if (!proc_pid_is_supported_range_400_plus(pid)) {
		set_success(state, 0);
		return;
	}
	if (pid == 0)
		pid = self;

	if (!buffer) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}
	if (buffer_size < (int32_t)
	    sizeof(struct darwin_proc_bsdshortinfo_range_400_plus)) {
		set_failure(state, DARWIN_ENOMEM);
		return;
	}

	struct darwin_proc_bsdshortinfo_range_400_plus info;
	fill_proc_bsdshortinfo_range_400_plus(&info, pid);
	memcpy(buffer, &info, sizeof(info));
	set_success(state, sizeof(info));
}

static long raw_poll(struct pollfd* fds, nfds_t nfds, int timeout_ms)
{
#ifdef SYS_poll
	return syscall(SYS_poll, fds, nfds, timeout_ms);
#else
	struct timespec timeout;
	struct timespec* timeout_ptr = NULL;

	if (timeout_ms >= 0) {
		timeout.tv_sec = timeout_ms / 1000;
		timeout.tv_nsec = (long)(timeout_ms % 1000) * 1000000L;
		timeout_ptr = &timeout;
	}

	return syscall(SYS_ppoll, fds, nfds, timeout_ptr, NULL, 0);
#endif
}

static int translate_socket_domain(int darwin_domain, int* linux_domain)
{
	switch (darwin_domain) {
	case DARWIN_AF_UNIX:
		*linux_domain = AF_UNIX;
		return 1;
	case DARWIN_AF_INET:
		*linux_domain = AF_INET;
		return 1;
	case DARWIN_AF_INET6:
		*linux_domain = AF_INET6;
		return 1;
	default:
		return 0;
	}
}

static int translate_sockaddr(const void* darwin_addr, uint64_t darwin_len,
                              struct sockaddr_storage* linux_addr,
                              socklen_t* linux_len)
{
	const unsigned char* raw = (const unsigned char*)darwin_addr;
	int linux_domain;

	if (!darwin_addr) {
		*linux_len = 0;
		return 1;
	}

	if (darwin_len < 2 || darwin_len > sizeof(*linux_addr)) {
		errno = EINVAL;
		return 0;
	}

	if (!translate_socket_domain(raw[1], &linux_domain)) {
		errno = EAFNOSUPPORT;
		return 0;
	}

	memset(linux_addr, 0, sizeof(*linux_addr));
	switch (raw[1]) {
	case DARWIN_AF_UNIX: {
		struct sockaddr_un* unix_addr = (struct sockaddr_un*)linux_addr;
		size_t path_len = darwin_len - 2;
		if (path_len > sizeof(unix_addr->sun_path)) {
			errno = ENAMETOOLONG;
			return 0;
		}
		unix_addr->sun_family = AF_UNIX;
		memcpy(unix_addr->sun_path, raw + 2, path_len);
		*linux_len = (socklen_t)(offsetof(struct sockaddr_un, sun_path) +
		                         path_len);
		return 1;
	}
	case DARWIN_AF_INET: {
		struct sockaddr_in* inet_addr = (struct sockaddr_in*)linux_addr;
		if (darwin_len < 16) {
			errno = EINVAL;
			return 0;
		}
		inet_addr->sin_family = AF_INET;
		memcpy(&inet_addr->sin_port, raw + 2, 2);
		memcpy(&inet_addr->sin_addr, raw + 4, 4);
		*linux_len = sizeof(*inet_addr);
		return 1;
	}
	case DARWIN_AF_INET6: {
		struct sockaddr_in6* inet6_addr = (struct sockaddr_in6*)linux_addr;
		if (darwin_len < 28) {
			errno = EINVAL;
			return 0;
		}
		inet6_addr->sin6_family = AF_INET6;
		memcpy(&inet6_addr->sin6_port, raw + 2, 2);
		memcpy(&inet6_addr->sin6_flowinfo, raw + 4, 4);
		memcpy(&inet6_addr->sin6_addr, raw + 8, 16);
		memcpy(&inet6_addr->sin6_scope_id, raw + 24, 4);
		*linux_len = sizeof(*inet6_addr);
		return 1;
	}
	default:
		errno = EAFNOSUPPORT;
		return 0;
	}
}

static int linux_sockaddr_to_darwin(const struct sockaddr_storage* linux_addr,
                                    socklen_t linux_len, void* darwin_addr,
                                    socklen_t* darwin_len)
{
	unsigned char* raw = (unsigned char*)darwin_addr;
	socklen_t capacity;

	if (!darwin_addr || !darwin_len)
		return 1;

	capacity = *darwin_len;
	memset(raw, 0, capacity);
	switch (linux_addr->ss_family) {
	case AF_UNIX: {
		const struct sockaddr_un* unix_addr =
			(const struct sockaddr_un*)linux_addr;
		socklen_t path_len = 0;
		if (linux_len > offsetof(struct sockaddr_un, sun_path))
			path_len = linux_len - offsetof(struct sockaddr_un, sun_path);
		if (path_len + 2 > capacity || path_len + 2 > UINT8_MAX) {
			errno = EINVAL;
			return 0;
		}
		raw[0] = (unsigned char)(path_len + 2);
		raw[1] = DARWIN_AF_UNIX;
		memcpy(raw + 2, unix_addr->sun_path, path_len);
		*darwin_len = path_len + 2;
		return 1;
	}
	case AF_INET: {
		const struct sockaddr_in* inet_addr =
			(const struct sockaddr_in*)linux_addr;
		if (capacity < 16) {
			errno = EINVAL;
			return 0;
		}
		raw[0] = 16;
		raw[1] = DARWIN_AF_INET;
		memcpy(raw + 2, &inet_addr->sin_port, 2);
		memcpy(raw + 4, &inet_addr->sin_addr, 4);
		*darwin_len = 16;
		return 1;
	}
	case AF_INET6: {
		const struct sockaddr_in6* inet6_addr =
			(const struct sockaddr_in6*)linux_addr;
		if (capacity < 28) {
			errno = EINVAL;
			return 0;
		}
		raw[0] = 28;
		raw[1] = DARWIN_AF_INET6;
		memcpy(raw + 2, &inet6_addr->sin6_port, 2);
		memcpy(raw + 4, &inet6_addr->sin6_flowinfo, 4);
		memcpy(raw + 8, &inet6_addr->sin6_addr, 16);
		memcpy(raw + 24, &inet6_addr->sin6_scope_id, 4);
		*darwin_len = 28;
		return 1;
	}
	default:
		errno = EAFNOSUPPORT;
		return 0;
	}
}

static int prepare_linux_msghdr(
	const struct darwin_msghdr_range_400_plus* darwin_msg,
	struct msghdr* linux_msg,
	struct sockaddr_storage* linux_name,
	socklen_t* linux_name_len)
{
	if (!darwin_msg) {
		errno = EFAULT;
		return 0;
	}

	if (darwin_msg->msg_control || darwin_msg->msg_controllen) {
		errno = ENOSYS;
		return 0;
	}

	memset(linux_msg, 0, sizeof(*linux_msg));
	linux_msg->msg_iov = darwin_msg->msg_iov;
	linux_msg->msg_iovlen = (size_t)darwin_msg->msg_iovlen;
	linux_msg->msg_flags = darwin_msg->msg_flags;

	if (darwin_msg->msg_name) {
		if (!translate_sockaddr(darwin_msg->msg_name, darwin_msg->msg_namelen,
		                        linux_name, linux_name_len))
			return 0;
		linux_msg->msg_name = linux_name;
		linux_msg->msg_namelen = *linux_name_len;
	}

	return 1;
}

static void dispatch_sendmsg(struct syscall_gate_state* state)
{
	const struct darwin_msghdr_range_400_plus* darwin_msg =
		(const struct darwin_msghdr_range_400_plus*)state->x[1];
	struct msghdr linux_msg;
	struct sockaddr_storage linux_name;
	socklen_t linux_name_len = 0;

	if (!prepare_linux_msghdr(darwin_msg, &linux_msg, &linux_name,
	                          &linux_name_len)) {
		set_errno_failure(state);
		return;
	}

	errno = 0;
	finish_syscall_result(state, syscall(SYS_sendmsg, (int)state->x[0],
	                                     &linux_msg, (int)state->x[2]));
}

static void dispatch_recvmsg(struct syscall_gate_state* state)
{
	struct darwin_msghdr_range_400_plus* darwin_msg =
		(struct darwin_msghdr_range_400_plus*)state->x[1];
	struct msghdr linux_msg;
	struct sockaddr_storage linux_name;
	socklen_t linux_name_len = sizeof(linux_name);

	if (!darwin_msg) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	if (darwin_msg->msg_control || darwin_msg->msg_controllen) {
		set_failure(state, DARWIN_ENOSYS);
		return;
	}

	memset(&linux_msg, 0, sizeof(linux_msg));
	linux_msg.msg_iov = darwin_msg->msg_iov;
	linux_msg.msg_iovlen = (size_t)darwin_msg->msg_iovlen;
	if (darwin_msg->msg_name) {
		linux_msg.msg_name = &linux_name;
		linux_msg.msg_namelen = linux_name_len;
	}

	errno = 0;
	long result = syscall(SYS_recvmsg, (int)state->x[0], &linux_msg,
	                      (int)state->x[2]);
	if (result < 0) {
		finish_syscall_result(state, result);
		return;
	}

	darwin_msg->msg_flags = linux_msg.msg_flags;
	if (darwin_msg->msg_name) {
		socklen_t darwin_len = darwin_msg->msg_namelen;
		if (!linux_sockaddr_to_darwin(&linux_name, linux_msg.msg_namelen,
		                              darwin_msg->msg_name, &darwin_len)) {
			set_errno_failure(state);
			return;
		}
		darwin_msg->msg_namelen = darwin_len;
	}

	set_success(state, (uint64_t)result);
}

static int prepare_linux_msghdr_x(
	const struct darwin_msghdr_x_range_400_plus* darwin_msg,
	struct msghdr* linux_msg,
	struct sockaddr_storage* linux_name,
	socklen_t* linux_name_len)
{
	if (!darwin_msg) {
		errno = EFAULT;
		return 0;
	}

	if (darwin_msg->msg_control || darwin_msg->msg_controllen) {
		errno = ENOSYS;
		return 0;
	}

	memset(linux_msg, 0, sizeof(*linux_msg));
	linux_msg->msg_iov = darwin_msg->msg_iov;
	linux_msg->msg_iovlen = (size_t)darwin_msg->msg_iovlen;
	linux_msg->msg_flags = darwin_msg->msg_flags;

	if (darwin_msg->msg_name) {
		if (!translate_sockaddr(darwin_msg->msg_name, darwin_msg->msg_namelen,
		                        linux_name, linux_name_len))
			return 0;
		linux_msg->msg_name = linux_name;
		linux_msg->msg_namelen = *linux_name_len;
	}

	return 1;
}

static void dispatch_sendmsg_x(struct syscall_gate_state* state)
{
	struct darwin_msghdr_x_range_400_plus* darwin_msg =
		(struct darwin_msghdr_x_range_400_plus*)state->x[1];
	unsigned int count = (unsigned int)state->x[2];
	unsigned int sent = 0;

	if (!darwin_msg && count != 0) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	for (unsigned int i = 0; i < count; i++) {
		struct msghdr linux_msg;
		struct sockaddr_storage linux_name;
		socklen_t linux_name_len = 0;

		if (!prepare_linux_msghdr_x(&darwin_msg[i], &linux_msg, &linux_name,
		                            &linux_name_len)) {
			if (sent)
				set_success(state, sent);
			else
				set_errno_failure(state);
			return;
		}

		errno = 0;
		long result = syscall(SYS_sendmsg, (int)state->x[0], &linux_msg,
		                      (int)state->x[3]);
		if (result < 0) {
			if (sent)
				set_success(state, sent);
			else
				finish_syscall_result(state, result);
			return;
		}
		sent++;
	}

	set_success(state, sent);
}

static void dispatch_recvmsg_x(struct syscall_gate_state* state)
{
	struct darwin_msghdr_x_range_400_plus* darwin_msg =
		(struct darwin_msghdr_x_range_400_plus*)state->x[1];
	unsigned int count = (unsigned int)state->x[2];
	unsigned int received = 0;

	for (unsigned int i = 0; i < count; i++) {
		struct msghdr linux_msg;
		struct sockaddr_storage linux_name;
		socklen_t linux_name_len = sizeof(linux_name);
		int flags = (int)state->x[3];

		if (!darwin_msg) {
			if (received)
				set_success(state, received);
			else
				set_failure(state, DARWIN_EFAULT);
			return;
		}

		if (darwin_msg[i].msg_control || darwin_msg[i].msg_controllen) {
			if (received)
				set_success(state, received);
			else
				set_failure(state, DARWIN_ENOSYS);
			return;
		}

		memset(&linux_msg, 0, sizeof(linux_msg));
		linux_msg.msg_iov = darwin_msg[i].msg_iov;
		linux_msg.msg_iovlen = (size_t)darwin_msg[i].msg_iovlen;
		if (darwin_msg[i].msg_name) {
			linux_msg.msg_name = &linux_name;
			linux_msg.msg_namelen = linux_name_len;
		}

		if (received)
			flags |= MSG_DONTWAIT;

		errno = 0;
		long result = syscall(SYS_recvmsg, (int)state->x[0], &linux_msg,
		                      flags);
		if (result < 0) {
			if (received)
				set_success(state, received);
			else
				finish_syscall_result(state, result);
			return;
		}

		darwin_msg[i].msg_flags = linux_msg.msg_flags;
		darwin_msg[i].msg_datalen = (uint64_t)result;
		if (darwin_msg[i].msg_name) {
			socklen_t darwin_len = darwin_msg[i].msg_namelen;
			if (!linux_sockaddr_to_darwin(&linux_name, linux_msg.msg_namelen,
			                              darwin_msg[i].msg_name,
			                              &darwin_len)) {
				if (received)
					set_success(state, received);
				else
					set_errno_failure(state);
				return;
			}
			darwin_msg[i].msg_namelen = darwin_len;
		}
		received++;
	}

	set_success(state, received);
}

static void dispatch_recvfrom(struct syscall_gate_state* state)
{
	struct sockaddr_storage linux_name;
	socklen_t linux_name_len = sizeof(linux_name);
	socklen_t* darwin_len_ptr = (socklen_t*)state->x[5];

	errno = 0;
	long result = syscall(SYS_recvfrom, (int)state->x[0], (void*)state->x[1],
	                      (size_t)state->x[2], (int)state->x[3],
	                      state->x[4] ? (struct sockaddr*)&linux_name : NULL,
	                      state->x[4] ? &linux_name_len : NULL);
	if (result < 0) {
		finish_syscall_result(state, result);
		return;
	}

	if (state->x[4] && darwin_len_ptr) {
		socklen_t darwin_len = *darwin_len_ptr;
		if (!linux_sockaddr_to_darwin(&linux_name, linux_name_len,
		                              (void*)state->x[4], &darwin_len)) {
			set_errno_failure(state);
			return;
		}
		*darwin_len_ptr = darwin_len;
	}

	set_success(state, (uint64_t)result);
}

static void dispatch_sendto(struct syscall_gate_state* state)
{
	struct sockaddr_storage linux_addr;
	struct sockaddr_storage* linux_addr_ptr = NULL;
	socklen_t linux_len = 0;

	if (state->x[4]) {
		if (!translate_sockaddr((const void*)state->x[4], state->x[5],
		                        &linux_addr, &linux_len)) {
			set_errno_failure(state);
			return;
		}
		linux_addr_ptr = &linux_addr;
	}

	errno = 0;
	long result = syscall(SYS_sendto, (int)state->x[0],
	                      (const void*)state->x[1], (size_t)state->x[2],
	                      (int)state->x[3],
	                      (const struct sockaddr*)linux_addr_ptr, linux_len);
	finish_syscall_result(state, result);
}

static void finish_socket_name_result(struct syscall_gate_state* state,
                                      long result,
                                      const struct sockaddr_storage* linux_name,
                                      socklen_t linux_name_len)
{
	socklen_t* darwin_len_ptr = (socklen_t*)state->x[2];

	if (result < 0) {
		finish_syscall_result(state, result);
		return;
	}

	if (state->x[1] && darwin_len_ptr) {
		socklen_t darwin_len = *darwin_len_ptr;
		if (!linux_sockaddr_to_darwin(linux_name, linux_name_len,
		                              (void*)state->x[1], &darwin_len)) {
			set_errno_failure(state);
			return;
		}
		*darwin_len_ptr = darwin_len;
	}

	set_success(state, (uint64_t)result);
}

static void dispatch_accept(struct syscall_gate_state* state)
{
	struct sockaddr_storage linux_name;
	socklen_t linux_name_len = sizeof(linux_name);

	errno = 0;
	long result = syscall(SYS_accept, (int)state->x[0],
	                      state->x[1] ? (struct sockaddr*)&linux_name : NULL,
	                      state->x[1] ? &linux_name_len : NULL);
	finish_socket_name_result(state, result, &linux_name, linux_name_len);
}

static void dispatch_connect(struct syscall_gate_state* state)
{
	struct sockaddr_storage linux_name;
	socklen_t linux_name_len;

	if (!translate_sockaddr((const void*)state->x[1], state->x[2],
	                        &linux_name, &linux_name_len)) {
		set_errno_failure(state);
		return;
	}

	errno = 0;
	finish_syscall_result(state, syscall(SYS_connect, (int)state->x[0],
	                                     (const struct sockaddr*)&linux_name,
	                                     linux_name_len));
}

static void dispatch_connectx(struct syscall_gate_state* state)
{
	const struct darwin_sa_endpoints_range_400_plus* endpoints =
		(const struct darwin_sa_endpoints_range_400_plus*)state->x[1];
	uint32_t associd = (uint32_t)state->x[2];
	uint32_t flags = (uint32_t)state->x[3];
	const struct iovec* iov = (const struct iovec*)state->x[4];
	unsigned int iovcnt = (unsigned int)state->x[5];
	size_t* len = (size_t*)state->x[6];
	void* connid = (void*)state->x[7];
	struct sockaddr_storage linux_name;
	socklen_t linux_name_len;

	if (!endpoints) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	if (associd != 0 || connid || (flags & ~0x7u)) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	if (endpoints->sae_srcif != 0) {
		set_failure(state, DARWIN_ENOTSUP);
		return;
	}

	if (iovcnt != 0 && (!iov || !len)) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	if (endpoints->sae_srcaddr) {
		if (!translate_sockaddr(endpoints->sae_srcaddr,
		                        endpoints->sae_srcaddrlen,
		                        &linux_name, &linux_name_len)) {
			set_errno_failure(state);
			return;
		}

		errno = 0;
		long result = syscall(SYS_bind, (int)state->x[0],
		                      (const struct sockaddr*)&linux_name,
		                      linux_name_len);
		if (result < 0) {
			finish_syscall_result(state, result);
			return;
		}
	}

	if (!endpoints->sae_dstaddr) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	if (!translate_sockaddr(endpoints->sae_dstaddr, endpoints->sae_dstaddrlen,
	                        &linux_name, &linux_name_len)) {
		set_errno_failure(state);
		return;
	}

	errno = 0;
	long result = syscall(SYS_connect, (int)state->x[0],
	                      (const struct sockaddr*)&linux_name,
	                      linux_name_len);
	if (result < 0) {
		finish_syscall_result(state, result);
		return;
	}

	if (len)
		*len = 0;

	if (iovcnt != 0) {
		result = machgate_syscall_writev_no_sigpipe((int)state->x[0], iov,
		                                            (int)iovcnt);
		if (result < 0) {
			finish_syscall_result(state, result);
			return;
		}
		*len = (size_t)result;
	}

	set_success(state, 0);
}

static void dispatch_disconnectx(struct syscall_gate_state* state)
{
	uint32_t associd = (uint32_t)state->x[1];
	uint32_t connid = (uint32_t)state->x[2];
	struct sockaddr linux_name;

	if (associd != 0 || connid != 0) {
		set_failure(state, DARWIN_ENOTSUP);
		return;
	}

	memset(&linux_name, 0, sizeof(linux_name));
	linux_name.sa_family = AF_UNSPEC;
	errno = 0;
	long result = syscall(SYS_connect, (int)state->x[0], &linux_name,
	                      sizeof(linux_name.sa_family));
	finish_syscall_result(state, result);
}

static void dispatch_socket_delegate(struct syscall_gate_state* state)
{
	int linux_domain;
	pid_t epid = (pid_t)(int32_t)state->x[3];
	pid_t self = (pid_t)syscall(SYS_getpid);

	if (epid != 0 && epid != self) {
		set_failure(state, DARWIN_ESRCH);
		return;
	}

	if (!translate_socket_domain((int)state->x[0], &linux_domain)) {
		set_failure(state, linux_errno_to_darwin(EAFNOSUPPORT));
		return;
	}

	errno = 0;
	finish_syscall_result(state, syscall(SYS_socket, linux_domain,
	                                     (int)state->x[1],
	                                     (int)state->x[2]));
}

static void dispatch_pid_shutdown_sockets(struct syscall_gate_state* state)
{
	pid_t pid = (pid_t)(int32_t)state->x[0];
	int level = (int)state->x[1];
	pid_t self = (pid_t)syscall(SYS_getpid);

	if (level < 0 || level > SHUT_RDWR) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	if (pid != 0 && pid != self) {
		set_failure(state, DARWIN_ESRCH);
		return;
	}

	for (int fd = 0; fd < 1024; fd++) {
		errno = 0;
		long result = syscall(SYS_shutdown, fd, level);
		if (result < 0 && errno != EBADF && errno != ENOTSOCK &&
		    errno != ENOTCONN)
			continue;
	}

	set_success(state, 0);
}

static int open_dummy_control_fd(void)
{
	return (int)syscall(SYS_openat, AT_FDCWD, "/dev/null", O_RDWR | O_CLOEXEC);
}

static int fd_is_valid(int fd)
{
	errno = 0;
	return syscall(SYS_fcntl, fd, F_GETFD) >= 0;
}

static void dispatch_dummy_control_open(struct syscall_gate_state* state)
{
	if (state->x[0] & ~0x7ull) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	errno = 0;
	finish_syscall_result(state, open_dummy_control_fd());
}

static void dispatch_necp_match_policy(struct syscall_gate_state* state)
{
	void* returned_result = (void*)state->x[2];

	if (!returned_result) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	memset(returned_result, 0, 512);
	set_success(state, 0);
}

static void dispatch_necp_client_action(struct syscall_gate_state* state)
{
	if (!fd_is_valid((int)state->x[0])) {
		set_failure(state, DARWIN_EBADF);
		return;
	}

	set_failure(state, DARWIN_ENOTSUP);
}

static void dispatch_necp_session_action(struct syscall_gate_state* state)
{
	if (!fd_is_valid((int)state->x[0])) {
		set_failure(state, DARWIN_EBADF);
		return;
	}

	set_failure(state, DARWIN_ENOTSUP);
}

static void dispatch_net_qos_guideline(struct syscall_gate_state* state)
{
	struct darwin_net_qos_param_range_400_plus* param =
		(struct darwin_net_qos_param_range_400_plus*)state->x[0];
	uint32_t param_len = (uint32_t)state->x[1];

	if (!param ||
	    param_len < sizeof(struct darwin_net_qos_param_range_400_plus)) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	set_success(state, 0);
}

static void dispatch_tracker_action(struct syscall_gate_state* state)
{
	uint32_t action = (uint32_t)state->x[0];
	void* buffer = (void*)state->x[1];
	size_t buffer_size = (size_t)state->x[2];

	if (action == 0 || action >= 4) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	if (buffer && buffer_size)
		memset(buffer, 0, buffer_size);

	set_success(state, 0);
}

static int translate_dirfd(int darwin_fd)
{
	if (darwin_fd == DARWIN_AT_FDCWD)
		return AT_FDCWD;
	return darwin_fd;
}

static int should_hide_proc_exe_symlink(const char* path)
{
	const char* cursor;

	if (!path)
		return 0;
	if (strcmp(path, "/proc/self/exe") == 0)
		return 0;
	if (strncmp(path, "/proc/", 6) != 0)
		return 0;

	cursor = path + 6;
	if (*cursor < '0' || *cursor > '9')
		return 0;
	while (*cursor >= '0' && *cursor <= '9')
		cursor++;
	return strcmp(cursor, "/exe") == 0;
}

static int translate_open_flags(int darwin_flags)
{
	int linux_flags = darwin_flags & O_ACCMODE;

	if (darwin_flags & DARWIN_O_NONBLOCK) linux_flags |= O_NONBLOCK;
	if (darwin_flags & DARWIN_O_APPEND) linux_flags |= O_APPEND;
	if (darwin_flags & DARWIN_O_NOFOLLOW) linux_flags |= O_NOFOLLOW;
	if (darwin_flags & DARWIN_O_CREAT) linux_flags |= O_CREAT;
	if (darwin_flags & DARWIN_O_TRUNC) linux_flags |= O_TRUNC;
	if (darwin_flags & DARWIN_O_EXCL) linux_flags |= O_EXCL;
	if (darwin_flags & DARWIN_O_NOCTTY) linux_flags |= O_NOCTTY;
	if (darwin_flags & DARWIN_O_DIRECTORY) linux_flags |= O_DIRECTORY;
	if (darwin_flags & DARWIN_O_DSYNC) linux_flags |= O_DSYNC;
	if (darwin_flags & DARWIN_O_CLOEXEC) linux_flags |= O_CLOEXEC;
	if (darwin_flags & DARWIN_O_SYMLINK) linux_flags |= O_PATH | O_NOFOLLOW;

	return linux_flags;
}

static int translate_open_flags_to_darwin(int linux_flags)
{
	int darwin_flags = linux_flags & O_ACCMODE;

	if (linux_flags & O_NONBLOCK) darwin_flags |= DARWIN_O_NONBLOCK;
	if (linux_flags & O_APPEND) darwin_flags |= DARWIN_O_APPEND;
	if (linux_flags & O_NOFOLLOW) darwin_flags |= DARWIN_O_NOFOLLOW;
	if (linux_flags & O_CREAT) darwin_flags |= DARWIN_O_CREAT;
	if (linux_flags & O_TRUNC) darwin_flags |= DARWIN_O_TRUNC;
	if (linux_flags & O_EXCL) darwin_flags |= DARWIN_O_EXCL;
	if (linux_flags & O_NOCTTY) darwin_flags |= DARWIN_O_NOCTTY;
	if (linux_flags & O_DIRECTORY) darwin_flags |= DARWIN_O_DIRECTORY;
	if (linux_flags & O_DSYNC) darwin_flags |= DARWIN_O_DSYNC;
	if (linux_flags & O_CLOEXEC) darwin_flags |= DARWIN_O_CLOEXEC;

	return darwin_flags;
}

static size_t align_attr_offset_range_400_plus(size_t value)
{
	return (value + 3u) & ~(size_t)3u;
}

static const char* path_basename_range_400_plus(const char* path)
{
	const char* slash;

	if (!path || !*path)
		return "";

	slash = strrchr(path, '/');
	if (!slash)
		return path;
	if (!slash[1])
		return slash;
	return slash + 1;
}

static int attrlist_validate_range_400_plus(struct syscall_gate_state* state,
                                            const struct darwin_attrlist_range_400_plus* attrlist,
                                            uint32_t options,
                                            int for_set)
{
	uint32_t supported_options = DARWIN_FSOPT_NOFOLLOW |
	                             DARWIN_FSOPT_ATTR_CMN_EXTENDED;
	uint32_t common_supported = for_set ? DARWIN_ATTR_CMN_SET_SUPPORTED :
	                            DARWIN_ATTR_CMN_SUPPORTED;

	if (!attrlist) {
		set_failure(state, DARWIN_EFAULT);
		return 0;
	}

	if (attrlist->bitmapcount != DARWIN_ATTR_BIT_MAP_COUNT ||
	    attrlist->reserved != 0) {
		set_failure(state, DARWIN_EINVAL);
		return 0;
	}

	if (options & ~supported_options) {
		set_failure(state, DARWIN_EINVAL);
		return 0;
	}

	if ((attrlist->commonattr & ~common_supported) ||
	    attrlist->volattr || attrlist->forkattr ||
	    (attrlist->dirattr & ~DARWIN_ATTR_DIR_SUPPORTED) ||
	    (attrlist->fileattr & ~DARWIN_ATTR_FILE_SUPPORTED)) {
		set_failure(state, DARWIN_EINVAL);
		return 0;
	}

	if (for_set && (attrlist->dirattr || attrlist->fileattr)) {
		set_failure(state, DARWIN_EINVAL);
		return 0;
	}

	return 1;
}

static size_t attrlist_fixed_size_range_400_plus(
	const struct darwin_attrlist_range_400_plus* attrlist)
{
	size_t result = sizeof(uint32_t);

	if (attrlist->commonattr & DARWIN_ATTR_CMN_RETURNED_ATTRS)
		result += sizeof(struct darwin_attribute_set_range_400_plus);
	if (attrlist->commonattr & DARWIN_ATTR_CMN_NAME)
		result += sizeof(struct darwin_attrreference_range_400_plus);
	if (attrlist->commonattr & DARWIN_ATTR_CMN_DEVID)
		result += sizeof(int32_t);
	if (attrlist->commonattr & DARWIN_ATTR_CMN_FSID)
		result += sizeof(struct darwin_fsid_range_400_plus);
	if (attrlist->commonattr & DARWIN_ATTR_CMN_OBJTYPE)
		result += sizeof(uint32_t);
	if (attrlist->commonattr & DARWIN_ATTR_CMN_OBJTAG)
		result += sizeof(uint32_t);
	if (attrlist->commonattr & DARWIN_ATTR_CMN_CRTIME)
		result += sizeof(struct timespec);
	if (attrlist->commonattr & DARWIN_ATTR_CMN_MODTIME)
		result += sizeof(struct timespec);
	if (attrlist->commonattr & DARWIN_ATTR_CMN_CHGTIME)
		result += sizeof(struct timespec);
	if (attrlist->commonattr & DARWIN_ATTR_CMN_ACCTIME)
		result += sizeof(struct timespec);
	if (attrlist->commonattr & DARWIN_ATTR_CMN_OWNERID)
		result += sizeof(uint32_t);
	if (attrlist->commonattr & DARWIN_ATTR_CMN_GRPID)
		result += sizeof(uint32_t);
	if (attrlist->commonattr & DARWIN_ATTR_CMN_ACCESSMASK)
		result += sizeof(uint32_t);
	if (attrlist->commonattr & DARWIN_ATTR_CMN_FLAGS)
		result += sizeof(uint32_t);
	if (attrlist->commonattr & DARWIN_ATTR_CMN_USERACCESS)
		result += sizeof(uint32_t);
	if (attrlist->commonattr & DARWIN_ATTR_CMN_FILEID)
		result += sizeof(uint64_t);
	if (attrlist->commonattr & DARWIN_ATTR_CMN_FULLPATH)
		result += sizeof(struct darwin_attrreference_range_400_plus);
	if (attrlist->dirattr & DARWIN_ATTR_DIR_LINKCOUNT)
		result += sizeof(uint32_t);
	if (attrlist->dirattr & DARWIN_ATTR_DIR_ENTRYCOUNT)
		result += sizeof(uint32_t);
	if (attrlist->dirattr & DARWIN_ATTR_DIR_IOBLOCKSIZE)
		result += sizeof(uint32_t);
	if (attrlist->dirattr & DARWIN_ATTR_DIR_DATALENGTH)
		result += sizeof(uint64_t);
	if (attrlist->fileattr & DARWIN_ATTR_FILE_LINKCOUNT)
		result += sizeof(uint32_t);
	if (attrlist->fileattr & DARWIN_ATTR_FILE_TOTALSIZE)
		result += sizeof(uint64_t);
	if (attrlist->fileattr & DARWIN_ATTR_FILE_ALLOCSIZE)
		result += sizeof(uint64_t);
	if (attrlist->fileattr & DARWIN_ATTR_FILE_IOBLOCKSIZE)
		result += sizeof(uint32_t);
	if (attrlist->fileattr & DARWIN_ATTR_FILE_DATALENGTH)
		result += sizeof(uint64_t);
	if (attrlist->fileattr & DARWIN_ATTR_FILE_DATAALLOCSIZE)
		result += sizeof(uint64_t);

	return align_attr_offset_range_400_plus(result);
}

static int attrlist_write_bytes_range_400_plus(
	struct darwin_attr_context_range_400_plus* context,
	const void* value, size_t size)
{
	if (context->fixed_offset + size > context->capacity) {
		set_failure(context->state, DARWIN_ENOMEM);
		return 0;
	}

	memcpy(context->buffer + context->fixed_offset, value, size);
	context->fixed_offset =
		align_attr_offset_range_400_plus(context->fixed_offset + size);
	return 1;
}

static int attrlist_write_u32_range_400_plus(
	struct darwin_attr_context_range_400_plus* context, uint32_t value)
{
	return attrlist_write_bytes_range_400_plus(context, &value,
	                                          sizeof(value));
}

static int attrlist_write_u64_range_400_plus(
	struct darwin_attr_context_range_400_plus* context, uint64_t value)
{
	return attrlist_write_bytes_range_400_plus(context, &value,
	                                          sizeof(value));
}

static int attrlist_write_ref_range_400_plus(
	struct darwin_attr_context_range_400_plus* context, const char* value)
{
	struct darwin_attrreference_range_400_plus reference;
	size_t length = strlen(value) + 1;

	context->variable_offset =
		align_attr_offset_range_400_plus(context->variable_offset);
	if (context->fixed_offset + sizeof(reference) > context->capacity ||
	    context->variable_offset + length > context->capacity) {
		set_failure(context->state, DARWIN_ENOMEM);
		return 0;
	}

	reference.attr_dataoffset =
		(int32_t)(context->variable_offset - context->fixed_offset);
	reference.attr_length = (uint32_t)length;
	memcpy(context->buffer + context->fixed_offset, &reference,
	       sizeof(reference));
	memcpy(context->buffer + context->variable_offset, value, length);
	context->fixed_offset =
		align_attr_offset_range_400_plus(context->fixed_offset +
		                                 sizeof(reference));
	context->variable_offset =
		align_attr_offset_range_400_plus(context->variable_offset + length);
	return 1;
}

static uint32_t darwin_file_type_range_400_plus(mode_t mode)
{
	if (S_ISREG(mode)) return 1;
	if (S_ISDIR(mode)) return 2;
	if (S_ISBLK(mode)) return 3;
	if (S_ISCHR(mode)) return 4;
	if (S_ISLNK(mode)) return 5;
	if (S_ISSOCK(mode)) return 6;
	if (S_ISFIFO(mode)) return 7;
	return 0;
}

static uint32_t darwin_user_access_range_400_plus(const struct stat* linux_stat)
{
	uint32_t result = 0;
	uid_t uid = (uid_t)syscall(SYS_getuid);
	gid_t gid = (gid_t)syscall(SYS_getgid);
	mode_t mode = linux_stat->st_mode;

	if (uid == 0)
		return 7;

	if (uid == linux_stat->st_uid) {
		if (mode & S_IRUSR) result |= 4;
		if (mode & S_IWUSR) result |= 2;
		if (mode & S_IXUSR) result |= 1;
		return result;
	}

	if (gid == linux_stat->st_gid) {
		if (mode & S_IRGRP) result |= 4;
		if (mode & S_IWGRP) result |= 2;
		if (mode & S_IXGRP) result |= 1;
		return result;
	}

	if (mode & S_IROTH) result |= 4;
	if (mode & S_IWOTH) result |= 2;
	if (mode & S_IXOTH) result |= 1;
	return result;
}

static uint32_t count_directory_entries_at_range_400_plus(int dirfd,
                                                          const char* name)
{
	int fd = (int)syscall(SYS_openat, dirfd, name,
	                      O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	char buffer[8192];
	uint32_t result = 0;

	if (fd < 0)
		return 0;

	for (;;) {
		long read_result = syscall(SYS_getdents64, fd, buffer,
		                           sizeof(buffer));
		if (read_result <= 0)
			break;

		for (long offset = 0; offset < read_result;) {
			const struct linux_dirent64_range_400_plus* entry =
				(const struct linux_dirent64_range_400_plus*)(buffer + offset);
			if (strcmp(entry->d_name, ".") != 0 &&
			    strcmp(entry->d_name, "..") != 0)
				result++;
			offset += entry->d_reclen;
		}
	}

	syscall(SYS_close, fd);
	return result;
}

static int attrlist_write_stat_range_400_plus(
	struct darwin_attr_context_range_400_plus* context,
	const struct stat* linux_stat,
	const char* name,
	const char* full_path,
	int dirfd)
{
	struct darwin_fsid_range_400_plus fsid;
	struct darwin_attribute_set_range_400_plus returned;
	uint64_t allocated_size = (uint64_t)linux_stat->st_blocks * 512u;

	memset(&returned, 0, sizeof(returned));
	returned.commonattr = context->attrlist->commonattr;
	returned.dirattr = context->attrlist->dirattr;
	returned.fileattr = context->attrlist->fileattr;

	if (context->attrlist->commonattr & DARWIN_ATTR_CMN_RETURNED_ATTRS &&
	    !attrlist_write_bytes_range_400_plus(context, &returned,
	                                        sizeof(returned)))
		return 0;
	if (context->attrlist->commonattr & DARWIN_ATTR_CMN_NAME &&
	    !attrlist_write_ref_range_400_plus(context, name))
		return 0;
	if (context->attrlist->commonattr & DARWIN_ATTR_CMN_DEVID &&
	    !attrlist_write_u32_range_400_plus(context,
	                                      (uint32_t)linux_stat->st_dev))
		return 0;
	if (context->attrlist->commonattr & DARWIN_ATTR_CMN_FSID) {
		fsid.val[0] = (int32_t)linux_stat->st_dev;
		fsid.val[1] = 0;
		if (!attrlist_write_bytes_range_400_plus(context, &fsid,
		                                        sizeof(fsid)))
			return 0;
	}
	if (context->attrlist->commonattr & DARWIN_ATTR_CMN_OBJTYPE &&
	    !attrlist_write_u32_range_400_plus(context,
	                                      darwin_file_type_range_400_plus(
		                                      linux_stat->st_mode)))
		return 0;
	if (context->attrlist->commonattr & DARWIN_ATTR_CMN_OBJTAG &&
	    !attrlist_write_u32_range_400_plus(context, 0))
		return 0;
	if (context->attrlist->commonattr & DARWIN_ATTR_CMN_CRTIME &&
	    !attrlist_write_bytes_range_400_plus(context, &linux_stat->st_ctim,
	                                        sizeof(linux_stat->st_ctim)))
		return 0;
	if (context->attrlist->commonattr & DARWIN_ATTR_CMN_MODTIME &&
	    !attrlist_write_bytes_range_400_plus(context, &linux_stat->st_mtim,
	                                        sizeof(linux_stat->st_mtim)))
		return 0;
	if (context->attrlist->commonattr & DARWIN_ATTR_CMN_CHGTIME &&
	    !attrlist_write_bytes_range_400_plus(context, &linux_stat->st_ctim,
	                                        sizeof(linux_stat->st_ctim)))
		return 0;
	if (context->attrlist->commonattr & DARWIN_ATTR_CMN_ACCTIME &&
	    !attrlist_write_bytes_range_400_plus(context, &linux_stat->st_atim,
	                                        sizeof(linux_stat->st_atim)))
		return 0;
	if (context->attrlist->commonattr & DARWIN_ATTR_CMN_OWNERID &&
	    !attrlist_write_u32_range_400_plus(context,
	                                      (uint32_t)linux_stat->st_uid))
		return 0;
	if (context->attrlist->commonattr & DARWIN_ATTR_CMN_GRPID &&
	    !attrlist_write_u32_range_400_plus(context,
	                                      (uint32_t)linux_stat->st_gid))
		return 0;
	if (context->attrlist->commonattr & DARWIN_ATTR_CMN_ACCESSMASK &&
	    !attrlist_write_u32_range_400_plus(context,
	                                      (uint32_t)(linux_stat->st_mode &
	                                                 07777)))
		return 0;
	if (context->attrlist->commonattr & DARWIN_ATTR_CMN_FLAGS &&
	    !attrlist_write_u32_range_400_plus(context, 0))
		return 0;
	if (context->attrlist->commonattr & DARWIN_ATTR_CMN_USERACCESS &&
	    !attrlist_write_u32_range_400_plus(context,
	                                      darwin_user_access_range_400_plus(
		                                      linux_stat)))
		return 0;
	if (context->attrlist->commonattr & DARWIN_ATTR_CMN_FILEID &&
	    !attrlist_write_u64_range_400_plus(context, (uint64_t)linux_stat->st_ino))
		return 0;
	if (context->attrlist->commonattr & DARWIN_ATTR_CMN_FULLPATH &&
	    !attrlist_write_ref_range_400_plus(context, full_path ? full_path : name))
		return 0;
	if (context->attrlist->dirattr & DARWIN_ATTR_DIR_LINKCOUNT &&
	    !attrlist_write_u32_range_400_plus(context, (uint32_t)linux_stat->st_nlink))
		return 0;
	if (context->attrlist->dirattr & DARWIN_ATTR_DIR_ENTRYCOUNT &&
	    !attrlist_write_u32_range_400_plus(
		    context, count_directory_entries_at_range_400_plus(dirfd, name)))
		return 0;
	if (context->attrlist->dirattr & DARWIN_ATTR_DIR_IOBLOCKSIZE &&
	    !attrlist_write_u32_range_400_plus(context,
	                                      (uint32_t)linux_stat->st_blksize))
		return 0;
	if (context->attrlist->dirattr & DARWIN_ATTR_DIR_DATALENGTH &&
	    !attrlist_write_u64_range_400_plus(context, (uint64_t)linux_stat->st_size))
		return 0;
	if (context->attrlist->fileattr & DARWIN_ATTR_FILE_LINKCOUNT &&
	    !attrlist_write_u32_range_400_plus(context, (uint32_t)linux_stat->st_nlink))
		return 0;
	if (context->attrlist->fileattr & DARWIN_ATTR_FILE_TOTALSIZE &&
	    !attrlist_write_u64_range_400_plus(context, (uint64_t)linux_stat->st_size))
		return 0;
	if (context->attrlist->fileattr & DARWIN_ATTR_FILE_ALLOCSIZE &&
	    !attrlist_write_u64_range_400_plus(context, allocated_size))
		return 0;
	if (context->attrlist->fileattr & DARWIN_ATTR_FILE_IOBLOCKSIZE &&
	    !attrlist_write_u32_range_400_plus(context,
	                                      (uint32_t)linux_stat->st_blksize))
		return 0;
	if (context->attrlist->fileattr & DARWIN_ATTR_FILE_DATALENGTH &&
	    !attrlist_write_u64_range_400_plus(context, (uint64_t)linux_stat->st_size))
		return 0;
	if (context->attrlist->fileattr & DARWIN_ATTR_FILE_DATAALLOCSIZE &&
	    !attrlist_write_u64_range_400_plus(context, allocated_size))
		return 0;

	return 1;
}

static int build_absolute_path_range_400_plus(const char* path, char* buffer,
                                              size_t buffer_size)
{
	if (!path || !buffer || buffer_size == 0)
		return 0;

	if (path[0] == '/') {
		int result = snprintf(buffer, buffer_size, "%s", path);
		return result >= 0 && (size_t)result < buffer_size;
	}

	errno = 0;
	long result = syscall(SYS_getcwd, buffer, buffer_size);
	if (result < 0)
		return 0;

	size_t length = strlen(buffer);
	int printed = snprintf(buffer + length, buffer_size - length, "/%s", path);
	return printed >= 0 && (size_t)printed < buffer_size - length;
}

static void finish_getattrlist_result_range_400_plus(
	struct syscall_gate_state* state,
	const struct darwin_attrlist_range_400_plus* attrlist,
	char* buffer,
	size_t buffer_size,
	const struct stat* linux_stat,
	const char* name,
	const char* full_path,
	int dirfd)
{
	struct darwin_attr_context_range_400_plus context;
	size_t fixed_size = attrlist_fixed_size_range_400_plus(attrlist);
	uint32_t final_size;

	if (!buffer || buffer_size < sizeof(uint32_t)) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	memset(&context, 0, sizeof(context));
	context.state = state;
	context.attrlist = attrlist;
	context.buffer = buffer;
	context.capacity = buffer_size;
	context.fixed_offset = sizeof(uint32_t);
	context.variable_offset = fixed_size;

	if (fixed_size > buffer_size) {
		set_failure(state, DARWIN_ENOMEM);
		return;
	}

	if (!attrlist_write_stat_range_400_plus(&context, linux_stat, name,
	                                       full_path, dirfd))
		return;

	final_size = (uint32_t)context.variable_offset;
	memcpy(buffer, &final_size, sizeof(final_size));
	set_success(state, 0);
}

static void dispatch_getattrlistat(struct syscall_gate_state* state)
{
	int dirfd = translate_dirfd((int)state->x[0]);
	const char* path = (const char*)state->x[1];
	const struct darwin_attrlist_range_400_plus* attrlist =
		(const struct darwin_attrlist_range_400_plus*)state->x[2];
	char* buffer = (char*)state->x[3];
	size_t buffer_size = (size_t)state->x[4];
	uint32_t options = (uint32_t)state->x[5];
	struct stat linux_stat;
	char full_path[4096];
	int flags = (options & DARWIN_FSOPT_NOFOLLOW) ? AT_SYMLINK_NOFOLLOW : 0;

	if (!attrlist_validate_range_400_plus(state, attrlist, options, 0))
		return;

	errno = 0;
	long result = syscall(SYS_newfstatat, dirfd, path, &linux_stat, flags);
	if (result < 0) {
		set_errno_failure(state);
		return;
	}

	if (!build_absolute_path_range_400_plus(path, full_path, sizeof(full_path)))
		snprintf(full_path, sizeof(full_path), "%s", path ? path : "");

	finish_getattrlist_result_range_400_plus(
		state, attrlist, buffer, buffer_size, &linux_stat,
		path_basename_range_400_plus(path), full_path, dirfd);
}

static const char* consume_setattr_timespec_range_400_plus(
	const char* cursor,
	struct timespec* value)
{
	memcpy(value, cursor, sizeof(*value));
	return cursor + align_attr_offset_range_400_plus(sizeof(*value));
}

static void apply_setattr_result_range_400_plus(struct syscall_gate_state* state,
                                                long result)
{
	if (result < 0)
		set_errno_failure(state);
	else
		set_success(state, 0);
}

static void dispatch_setattrlistat(struct syscall_gate_state* state)
{
	int dirfd = translate_dirfd((int)state->x[0]);
	const char* path = (const char*)state->x[1];
	const struct darwin_attrlist_range_400_plus* attrlist =
		(const struct darwin_attrlist_range_400_plus*)state->x[2];
	const char* cursor = (const char*)state->x[3];
	uint32_t options = (uint32_t)state->x[5];
	uid_t uid = (uid_t)-1;
	gid_t gid = (gid_t)-1;
	mode_t mode = (mode_t)-1;
	struct timespec times[2];
	int set_atime = 0;
	int set_mtime = 0;

	if (!cursor) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}
	if (!attrlist_validate_range_400_plus(state, attrlist, options, 1))
		return;

	times[0].tv_nsec = UTIME_OMIT;
	times[1].tv_nsec = UTIME_OMIT;
	if (attrlist->commonattr & DARWIN_ATTR_CMN_MODTIME) {
		cursor = consume_setattr_timespec_range_400_plus(cursor, &times[1]);
		set_mtime = 1;
	}
	if (attrlist->commonattr & DARWIN_ATTR_CMN_ACCTIME) {
		cursor = consume_setattr_timespec_range_400_plus(cursor, &times[0]);
		set_atime = 1;
	}
	if (attrlist->commonattr & DARWIN_ATTR_CMN_OWNERID) {
		memcpy(&uid, cursor, sizeof(uint32_t));
		cursor += align_attr_offset_range_400_plus(sizeof(uint32_t));
	}
	if (attrlist->commonattr & DARWIN_ATTR_CMN_GRPID) {
		memcpy(&gid, cursor, sizeof(uint32_t));
		cursor += align_attr_offset_range_400_plus(sizeof(uint32_t));
	}
	if (attrlist->commonattr & DARWIN_ATTR_CMN_ACCESSMASK) {
		uint32_t access_mode;
		memcpy(&access_mode, cursor, sizeof(access_mode));
		mode = (mode_t)access_mode;
	}

	if (uid != (uid_t)-1 || gid != (gid_t)-1) {
		errno = 0;
		long result = syscall(SYS_fchownat, dirfd, path, uid, gid,
		                      (options & DARWIN_FSOPT_NOFOLLOW) ?
		                      AT_SYMLINK_NOFOLLOW : 0);
		if (result < 0) {
			apply_setattr_result_range_400_plus(state, result);
			return;
		}
	}

	if (mode != (mode_t)-1) {
		errno = 0;
		long result = syscall(SYS_fchmodat, dirfd, path, mode, 0);
		if (result < 0) {
			apply_setattr_result_range_400_plus(state, result);
			return;
		}
	}

	if (set_atime || set_mtime) {
		errno = 0;
		apply_setattr_result_range_400_plus(
			state, syscall(SYS_utimensat, dirfd, path, times,
			               (options & DARWIN_FSOPT_NOFOLLOW) ?
			               AT_SYMLINK_NOFOLLOW : 0));
		return;
	}

	set_success(state, 0);
}

static void dispatch_getattrlistbulk(struct syscall_gate_state* state)
{
	int fd = (int)state->x[0];
	const struct darwin_attrlist_range_400_plus* attrlist =
		(const struct darwin_attrlist_range_400_plus*)state->x[1];
	char* buffer = (char*)state->x[2];
	size_t buffer_size = (size_t)state->x[3];
	uint32_t options = (uint32_t)state->x[4];
	char linux_buffer[8192];
	uint32_t count = 0;
	size_t offset = 0;

	if (!buffer && buffer_size) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}
	if (!attrlist_validate_range_400_plus(state, attrlist, options, 0))
		return;
	if ((attrlist->commonattr & DARWIN_ATTR_BULK_REQUIRED) !=
	    DARWIN_ATTR_BULK_REQUIRED) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	errno = 0;
	long result = syscall(SYS_getdents64, fd, linux_buffer,
	                      sizeof(linux_buffer));
	if (result < 0) {
		set_errno_failure(state);
		return;
	}

	for (long linux_offset = 0; linux_offset < result;) {
		const struct linux_dirent64_range_400_plus* entry =
			(const struct linux_dirent64_range_400_plus*)(linux_buffer +
			                                              linux_offset);
		struct stat linux_stat;
		struct darwin_attr_context_range_400_plus context;
		size_t fixed_size = attrlist_fixed_size_range_400_plus(attrlist);
		size_t variable_size = 0;
		uint32_t entry_size;

		linux_offset += entry->d_reclen;
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
			continue;

		errno = 0;
		if (syscall(SYS_newfstatat, fd, entry->d_name, &linux_stat,
		            AT_SYMLINK_NOFOLLOW) < 0)
			continue;

		if (attrlist->commonattr & DARWIN_ATTR_CMN_NAME)
			variable_size = align_attr_offset_range_400_plus(
				strlen(entry->d_name) + 1);

		if (offset + fixed_size + variable_size > buffer_size)
			break;

		memset(&context, 0, sizeof(context));
		context.state = state;
		context.attrlist = attrlist;
		context.buffer = buffer + offset;
		context.capacity = buffer_size - offset;
		context.fixed_offset = sizeof(uint32_t);
		context.variable_offset = fixed_size;

		if (!attrlist_write_stat_range_400_plus(&context, &linux_stat,
		                                       entry->d_name, entry->d_name,
		                                       fd)) {
			if (count > 0)
				set_success(state, count);
			return;
		}

		entry_size = (uint32_t)context.variable_offset;
		memcpy(buffer + offset, &entry_size, sizeof(entry_size));
		offset += align_attr_offset_range_400_plus(entry_size);
		count++;
	}

	set_success(state, count);
}

static int fsgetpath_match_range_400_plus(
	const struct stat* linux_stat,
	const struct darwin_fsid_range_400_plus* fsid,
	uint64_t objid)
{
	return (int32_t)linux_stat->st_dev == fsid->val[0] &&
	       (linux_stat->st_ino == objid ||
	        (uint32_t)linux_stat->st_ino == (uint32_t)objid);
}

static int fsgetpath_search_dir_range_400_plus(
	int dirfd,
	const char* display_path,
	const struct darwin_fsid_range_400_plus* fsid,
	uint64_t objid,
	char* result,
	size_t result_size,
	int depth)
{
	char linux_buffer[8192];

	if (depth > 8)
		return 0;

	for (;;) {
		long read_result = syscall(SYS_getdents64, dirfd, linux_buffer,
		                           sizeof(linux_buffer));
		if (read_result <= 0)
			return 0;

		for (long offset = 0; offset < read_result;) {
			const struct linux_dirent64_range_400_plus* entry =
				(const struct linux_dirent64_range_400_plus*)(linux_buffer +
				                                              offset);
			struct stat linux_stat;
			char child_path[4096];

			offset += entry->d_reclen;
			if (strcmp(entry->d_name, ".") == 0 ||
			    strcmp(entry->d_name, "..") == 0)
				continue;

			if (display_path[0] && strcmp(display_path, ".") != 0)
				snprintf(child_path, sizeof(child_path), "%s/%s",
				         display_path, entry->d_name);
			else
				snprintf(child_path, sizeof(child_path), "%s", entry->d_name);

			errno = 0;
			if (syscall(SYS_newfstatat, dirfd, entry->d_name, &linux_stat,
			            AT_SYMLINK_NOFOLLOW) < 0)
				continue;

			if (fsgetpath_match_range_400_plus(&linux_stat, fsid, objid)) {
				if (!build_absolute_path_range_400_plus(child_path, result,
				                                       result_size))
					snprintf(result, result_size, "%s", child_path);
				return 1;
			}

			if (S_ISDIR(linux_stat.st_mode)) {
				int child_fd = (int)syscall(SYS_openat, dirfd, entry->d_name,
				                            O_RDONLY | O_DIRECTORY |
				                            O_CLOEXEC | O_NOFOLLOW);
				if (child_fd < 0)
					continue;
				int found = fsgetpath_search_dir_range_400_plus(
					child_fd, child_path, fsid, objid, result, result_size,
					depth + 1);
				syscall(SYS_close, child_fd);
				if (found)
					return 1;
			}
		}
	}
}

static int fsgetpath_find_range_400_plus(
	const struct darwin_fsid_range_400_plus* fsid,
	uint64_t objid,
	char* path,
	size_t path_size)
{
	int fd = (int)syscall(SYS_openat, AT_FDCWD, ".",
	                      O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	int found;

	if (fd < 0)
		return -1;

	memset(path, 0, path_size);
	found = fsgetpath_search_dir_range_400_plus(fd, ".", fsid, objid,
	                                           path, path_size, 0);
	syscall(SYS_close, fd);
	if (!found) {
		errno = ENOENT;
		return -1;
	}

	return 0;
}

static void dispatch_fsgetpath(struct syscall_gate_state* state)
{
	char* buffer = (char*)state->x[0];
	size_t buffer_size = (size_t)state->x[1];
	const struct darwin_fsid_range_400_plus* fsid =
		(const struct darwin_fsid_range_400_plus*)state->x[2];
	uint64_t objid = state->x[3];
	char path[4096];
	size_t length;

	if (!buffer || !fsid) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}
	if (buffer_size == 0 || buffer_size > sizeof(path)) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	errno = 0;
	if (fsgetpath_find_range_400_plus(fsid, objid, path, sizeof(path)) < 0) {
		set_errno_failure(state);
		return;
	}

	length = strlen(path) + 1;
	if (length > buffer_size) {
		set_failure(state, DARWIN_ENOMEM);
		return;
	}

	memcpy(buffer, path, length);
	set_success(state, length);
}

static void dispatch_openbyid_np(struct syscall_gate_state* state)
{
	const struct darwin_fsid_range_400_plus* fsid =
		(const struct darwin_fsid_range_400_plus*)state->x[0];
	const struct darwin_fsobj_id_range_400_plus* objid =
		(const struct darwin_fsobj_id_range_400_plus*)state->x[1];
	char path[4096];
	uint64_t inode;

	if (!fsid || !objid) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	inode = objid->fid_objno;
	errno = 0;
	if (fsgetpath_find_range_400_plus(fsid, inode, path, sizeof(path)) < 0) {
		set_errno_failure(state);
		return;
	}

	errno = 0;
	long result = syscall(SYS_openat, AT_FDCWD, path,
	                      translate_open_flags((int)state->x[2]));
	finish_syscall_result(state, result);
}

static void clonefile_cleanup_failure(struct syscall_gate_state* state,
                                      int out_fd,
                                      int dst_dirfd,
                                      const char* dst,
                                      int saved_errno)
{
	if (out_fd >= 0)
		syscall(SYS_close, out_fd);
	if (dst)
		syscall(SYS_unlinkat, dst_dirfd, dst, 0);
	set_failure(state, linux_errno_to_darwin(saved_errno));
}

static void finish_clone_copy_range_400_plus(struct syscall_gate_state* state,
                                             int in_fd,
                                             int out_fd,
                                             int dst_dirfd,
                                             const char* dst)
{
	char buffer[16384];
	off_t offset = 0;

	for (;;) {
		errno = 0;
		long read_result = syscall(SYS_pread64, in_fd, buffer,
		                           sizeof(buffer), offset);
		if (read_result < 0) {
			clonefile_cleanup_failure(state, out_fd, dst_dirfd, dst,
			                          errno ? errno : EIO);
			return;
		}
		if (read_result == 0)
			break;

		char* cursor = buffer;
		long remaining = read_result;
		while (remaining > 0) {
			long write_result = machgate_syscall_write_no_sigpipe(out_fd,
			                                                      cursor,
			                                                      (size_t)remaining);
			if (write_result < 0) {
				clonefile_cleanup_failure(state, out_fd, dst_dirfd, dst,
				                          errno ? errno : EIO);
				return;
			}
			cursor += write_result;
			remaining -= write_result;
		}
		offset += read_result;
	}

	errno = 0;
	long close_result = syscall(SYS_close, out_fd);
	if (close_result < 0) {
		clonefile_cleanup_failure(state, -1, dst_dirfd, dst,
		                          errno ? errno : EIO);
		return;
	}

	set_success(state, 0);
}

static void dispatch_clone_from_fd_range_400_plus(
	struct syscall_gate_state* state,
	int in_fd,
	int dst_dirfd,
	const char* dst,
	uint32_t flags)
{
	struct stat linux_stat;
	int out_fd;
	mode_t mode;

	if (!dst) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}
	if (flags & ~DARWIN_CLONE_SUPPORTED_FLAGS) {
		set_failure(state, DARWIN_ENOTSUP);
		return;
	}

	errno = 0;
	if (syscall(SYS_fstat, in_fd, &linux_stat) < 0) {
		set_errno_failure(state);
		return;
	}

	mode = (mode_t)(linux_stat.st_mode & 07777);
	if (S_ISREG(linux_stat.st_mode))
		mode &= ~(S_ISUID | S_ISGID);

	errno = 0;
	out_fd = (int)syscall(SYS_openat, dst_dirfd, dst,
	                      O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC,
	                      mode);
	if (out_fd < 0) {
		set_errno_failure(state);
		return;
	}

	errno = 0;
	if (syscall(SYS_ioctl, out_fd, FICLONE, in_fd) == 0) {
		errno = 0;
		long close_result = syscall(SYS_close, out_fd);
		finish_syscall_result(state, close_result);
		return;
	}

	finish_clone_copy_range_400_plus(state, in_fd, out_fd, dst_dirfd, dst);
}

static void dispatch_clonefileat(struct syscall_gate_state* state)
{
	int src_dirfd = translate_dirfd((int)state->x[0]);
	const char* src = (const char*)state->x[1];
	int dst_dirfd = translate_dirfd((int)state->x[2]);
	const char* dst = (const char*)state->x[3];
	uint32_t flags = (uint32_t)state->x[4];
	int open_flags = O_RDONLY | O_CLOEXEC;
	int in_fd;

	if (!src) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	if (flags & DARWIN_CLONE_NOFOLLOW)
		open_flags |= O_NOFOLLOW;

	errno = 0;
	in_fd = (int)syscall(SYS_openat, src_dirfd, src, open_flags);
	if (in_fd < 0) {
		set_errno_failure(state);
		return;
	}

	dispatch_clone_from_fd_range_400_plus(state, in_fd, dst_dirfd, dst, flags);
	syscall(SYS_close, in_fd);
}

static void dispatch_fclonefileat(struct syscall_gate_state* state)
{
	dispatch_clone_from_fd_range_400_plus(state, (int)state->x[0],
	                                     translate_dirfd((int)state->x[1]),
	                                     (const char*)state->x[2],
	                                     (uint32_t)state->x[3]);
}

static long raw_fcntl(int fd, int darwin_cmd, uint64_t arg)
{
	int linux_cmd;

	switch (darwin_cmd) {
	case F_DUPFD:
	case F_GETFD:
	case F_SETFD:
	case F_GETFL:
	case F_SETFL:
		linux_cmd = darwin_cmd;
		break;
	case DARWIN_F_GETOWN:
		linux_cmd = F_GETOWN;
		break;
	case DARWIN_F_SETOWN:
		linux_cmd = F_SETOWN;
		break;
	case DARWIN_F_GETLK:
		linux_cmd = F_GETLK;
		break;
	case DARWIN_F_SETLK:
		linux_cmd = F_SETLK;
		break;
	case DARWIN_F_SETLKW:
		linux_cmd = F_SETLKW;
		break;
	case DARWIN_F_DUPFD_CLOEXEC:
		linux_cmd = F_DUPFD_CLOEXEC;
		break;
	case DARWIN_F_FULLFSYNC:
		return syscall(SYS_fsync, fd);
	default:
		errno = ENOSYS;
		return -1;
	}

	if (darwin_cmd == F_SETFL)
		arg = (uint64_t)translate_open_flags((int)arg);

	long result = syscall(SYS_fcntl, fd, linux_cmd, arg);
	if (darwin_cmd == F_GETFL && result >= 0)
		result = translate_open_flags_to_darwin((int)result);
	return result;
}

static int translate_at_flags(int darwin_flags)
{
	int linux_flags = 0;

	if (darwin_flags & DARWIN_AT_SYMLINK_NOFOLLOW)
		linux_flags |= AT_SYMLINK_NOFOLLOW;
	if (darwin_flags & DARWIN_AT_REMOVEDIR)
		linux_flags |= AT_REMOVEDIR;
	if (darwin_flags & DARWIN_AT_EACCESS)
		linux_flags |= AT_EACCESS;

	return linux_flags;
}

static int translate_linkat_flags(int darwin_flags)
{
	int linux_flags = 0;

	if (darwin_flags & DARWIN_AT_SYMLINK_FOLLOW)
		linux_flags |= AT_SYMLINK_FOLLOW;

	return linux_flags;
}

static int translate_renameatx_flags(uint32_t darwin_flags, unsigned int* linux_flags)
{
	if (darwin_flags & ~(DARWIN_RENAME_SECLUDE |
	                     DARWIN_RENAME_SWAP |
	                     DARWIN_RENAME_EXCL)) {
		errno = EINVAL;
		return 0;
	}

	if ((darwin_flags & DARWIN_RENAME_SWAP) &&
	    (darwin_flags & DARWIN_RENAME_EXCL)) {
		errno = EINVAL;
		return 0;
	}

	if (darwin_flags & DARWIN_RENAME_SECLUDE) {
		errno = ENOSYS;
		return 0;
	}

	*linux_flags = 0;
	if (darwin_flags & DARWIN_RENAME_SWAP)
		*linux_flags |= LINUX_RENAME_EXCHANGE;
	if (darwin_flags & DARWIN_RENAME_EXCL)
		*linux_flags |= LINUX_RENAME_NOREPLACE;

	return 1;
}

static int has_unsupported_at_flags(int darwin_flags)
{
	return (darwin_flags & DARWIN_AT_SYMLINK_NOFOLLOW_ANY) != 0;
}

static void linux_to_darwin_stat(const struct stat* linux_stat,
                                 struct darwin_stat_range_400_plus* darwin_stat)
{
	memset(darwin_stat, 0, sizeof(*darwin_stat));
	darwin_stat->st_dev = (int32_t)linux_stat->st_dev;
	darwin_stat->st_mode = (uint16_t)linux_stat->st_mode;
	darwin_stat->st_nlink = (uint16_t)linux_stat->st_nlink;
	darwin_stat->st_ino = linux_stat->st_ino;
	darwin_stat->st_uid = linux_stat->st_uid;
	darwin_stat->st_gid = linux_stat->st_gid;
	darwin_stat->st_rdev = (int32_t)linux_stat->st_rdev;
	darwin_stat->st_atimespec = linux_stat->st_atim;
	darwin_stat->st_mtimespec = linux_stat->st_mtim;
	darwin_stat->st_ctimespec = linux_stat->st_ctim;
	darwin_stat->st_birthtimespec = linux_stat->st_ctim;
	darwin_stat->st_size = linux_stat->st_size;
	darwin_stat->st_blocks = linux_stat->st_blocks;
	darwin_stat->st_blksize = linux_stat->st_blksize;
}

static long sys_newfstatat(int fd, const char* path, struct stat* statbuf,
                           int flags)
{
#ifdef SYS_newfstatat
	return syscall(SYS_newfstatat, fd, path, statbuf, flags);
#else
	return syscall(SYS_fstatat64, fd, path, statbuf, flags);
#endif
}

static int path_exists_for_namespace_mutation(struct syscall_gate_state* state,
                                              const char* path,
                                              int require_directory)
{
	struct stat linux_stat;

	if (!path) {
		set_failure(state, DARWIN_EFAULT);
		return 0;
	}

	errno = 0;
	long result = sys_newfstatat(AT_FDCWD, path, &linux_stat, 0);
	if (result < 0) {
		set_errno_failure(state);
		return 0;
	}

	if (require_directory && !S_ISDIR(linux_stat.st_mode)) {
		set_failure(state, DARWIN_ENOTDIR);
		return 0;
	}

	return 1;
}

static void dispatch_mac_mount(struct syscall_gate_state* state)
{
	if (!state->x[0] || !state->x[4]) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	if (!path_exists_for_namespace_mutation(state, (const char*)state->x[1], 0))
		return;

	set_failure(state, DARWIN_ENOTSUP);
}

static void dispatch_fs_snapshot(struct syscall_gate_state* state)
{
	int dirfd = translate_dirfd((int)state->x[1]);

	if (dirfd != AT_FDCWD && !fd_is_valid(dirfd)) {
		set_failure(state, DARWIN_EBADF);
		return;
	}

	if (!state->x[2]) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	set_failure(state, DARWIN_ENOTSUP);
}

static void dispatch_fmount(struct syscall_gate_state* state)
{
	if (!state->x[0]) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	if (!fd_is_valid((int)state->x[1])) {
		set_failure(state, DARWIN_EBADF);
		return;
	}

	set_failure(state, DARWIN_EPERM);
}

static void dispatch_pivot_root(struct syscall_gate_state* state)
{
	if (!path_exists_for_namespace_mutation(state, (const char*)state->x[0], 1))
		return;
	if (!path_exists_for_namespace_mutation(state, (const char*)state->x[1], 1))
		return;

	set_failure(state, DARWIN_EPERM);
}

static void dispatch_graftdmg(struct syscall_gate_state* state)
{
	if (!fd_is_valid((int)state->x[0])) {
		set_failure(state, DARWIN_EBADF);
		return;
	}

	if (!path_exists_for_namespace_mutation(state, (const char*)state->x[1], 1))
		return;

	set_failure(state, DARWIN_ENOTSUP);
}

static void dispatch_ungraftdmg(struct syscall_gate_state* state)
{
	if (!path_exists_for_namespace_mutation(state, (const char*)state->x[0], 1))
		return;

	set_failure(state, DARWIN_ENOTSUP);
}

static int darwin_fdset_to_linux(int nfds, const uint32_t* darwin_set,
                                 fd_set* linux_set)
{
	FD_ZERO(linux_set);

	if (!darwin_set)
		return 0;

	for (int fd = 0; fd < nfds; fd++) {
		uint32_t mask = (uint32_t)1u << (fd % DARWIN_NFDBITS);
		if (darwin_set[fd / DARWIN_NFDBITS] & mask)
			FD_SET(fd, linux_set);
	}

	return 0;
}

static void linux_fdset_to_darwin(int nfds, const fd_set* linux_set,
                                  uint32_t* darwin_set)
{
	if (!darwin_set)
		return;

	size_t words = ((size_t)nfds + DARWIN_NFDBITS - 1) / DARWIN_NFDBITS;
	memset(darwin_set, 0, words * sizeof(*darwin_set));

	for (int fd = 0; fd < nfds; fd++) {
		if (FD_ISSET(fd, linux_set))
			darwin_set[fd / DARWIN_NFDBITS] |=
				(uint32_t)1u << (fd % DARWIN_NFDBITS);
	}
}

static long raw_select(int nfds, fd_set* readfds, fd_set* writefds,
                       fd_set* exceptfds,
                       struct darwin_timeval_range_400_plus* darwin_timeout)
{
#ifdef SYS_select
	struct timeval timeout;
	struct timeval* timeout_ptr = NULL;

	if (darwin_timeout) {
		timeout.tv_sec = (time_t)darwin_timeout->tv_sec;
		timeout.tv_usec = (suseconds_t)darwin_timeout->tv_usec;
		timeout_ptr = &timeout;
	}

	long result = syscall(SYS_select, nfds, readfds, writefds, exceptfds,
	                      timeout_ptr);
	if (result >= 0 && darwin_timeout) {
		darwin_timeout->tv_sec = (int64_t)timeout.tv_sec;
		darwin_timeout->tv_usec = (int32_t)timeout.tv_usec;
		darwin_timeout->pad = 0;
	}
	return result;
#else
	struct timespec timeout;
	struct timespec* timeout_ptr = NULL;

	if (darwin_timeout) {
		timeout.tv_sec = (time_t)darwin_timeout->tv_sec;
		timeout.tv_nsec = (long)darwin_timeout->tv_usec * 1000L;
		timeout_ptr = &timeout;
	}

	return syscall(SYS_pselect6, nfds, readfds, writefds, exceptfds,
	               timeout_ptr, NULL);
#endif
}

static void dispatch_select(struct syscall_gate_state* state)
{
	int nfds = (int)state->x[0];
	uint32_t* darwin_readfds = (uint32_t*)state->x[1];
	uint32_t* darwin_writefds = (uint32_t*)state->x[2];
	uint32_t* darwin_exceptfds = (uint32_t*)state->x[3];
	fd_set readfds;
	fd_set writefds;
	fd_set exceptfds;

	if (nfds < 0 || nfds > FD_SETSIZE || nfds > DARWIN_FD_SETSIZE) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	darwin_fdset_to_linux(nfds, darwin_readfds, &readfds);
	darwin_fdset_to_linux(nfds, darwin_writefds, &writefds);
	darwin_fdset_to_linux(nfds, darwin_exceptfds, &exceptfds);

	errno = 0;
	long result = raw_select(nfds,
	                         darwin_readfds ? &readfds : NULL,
	                         darwin_writefds ? &writefds : NULL,
	                         darwin_exceptfds ? &exceptfds : NULL,
	                         (struct darwin_timeval_range_400_plus*)state->x[4]);
	if (result < 0) {
		set_errno_failure(state);
		return;
	}

	linux_fdset_to_darwin(nfds, &readfds, darwin_readfds);
	linux_fdset_to_darwin(nfds, &writefds, darwin_writefds);
	linux_fdset_to_darwin(nfds, &exceptfds, darwin_exceptfds);
	set_success(state, (uint64_t)result);
}

static void dispatch_fstatat(struct syscall_gate_state* state)
{
	struct stat linux_stat;
	int flags = (int)state->x[3];

	if (has_unsupported_at_flags(flags)) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	errno = 0;
	long result = sys_newfstatat(translate_dirfd((int)state->x[0]),
	                            (const char*)state->x[1], &linux_stat,
	                            translate_at_flags(flags));
	if (result < 0) {
		set_errno_failure(state);
		return;
	}

	linux_to_darwin_stat(&linux_stat,
	                     (struct darwin_stat_range_400_plus*)state->x[2]);
	set_success(state, 0);
}

static void linux_timex_to_darwin(const struct timex* linux_timex,
                                  struct darwin_timex_range_400_plus* darwin_timex)
{
	darwin_timex->modes = linux_timex->modes;
	darwin_timex->__pad0 = 0;
	darwin_timex->offset = linux_timex->offset;
	darwin_timex->freq = linux_timex->freq;
	darwin_timex->maxerror = linux_timex->maxerror;
	darwin_timex->esterror = linux_timex->esterror;
	darwin_timex->status = linux_timex->status;
	darwin_timex->__pad1 = 0;
	darwin_timex->constant = linux_timex->constant;
	darwin_timex->precision = linux_timex->precision;
	darwin_timex->tolerance = linux_timex->tolerance;
	darwin_timex->ppsfreq = linux_timex->ppsfreq;
	darwin_timex->jitter = linux_timex->jitter;
	darwin_timex->shift = linux_timex->shift;
	darwin_timex->__pad2 = 0;
	darwin_timex->stabil = linux_timex->stabil;
	darwin_timex->jitcnt = linux_timex->jitcnt;
	darwin_timex->calcnt = linux_timex->calcnt;
	darwin_timex->errcnt = linux_timex->errcnt;
	darwin_timex->stbcnt = linux_timex->stbcnt;
}

static void darwin_timex_to_linux(const struct darwin_timex_range_400_plus* darwin_timex,
                                  struct timex* linux_timex)
{
	memset(linux_timex, 0, sizeof(*linux_timex));
	linux_timex->modes = darwin_timex->modes;
	linux_timex->offset = darwin_timex->offset;
	linux_timex->freq = darwin_timex->freq;
	linux_timex->maxerror = darwin_timex->maxerror;
	linux_timex->esterror = darwin_timex->esterror;
	linux_timex->status = darwin_timex->status;
	linux_timex->constant = darwin_timex->constant;
	linux_timex->precision = darwin_timex->precision;
	linux_timex->tolerance = darwin_timex->tolerance;
	linux_timex->ppsfreq = darwin_timex->ppsfreq;
	linux_timex->jitter = darwin_timex->jitter;
	linux_timex->shift = darwin_timex->shift;
	linux_timex->stabil = darwin_timex->stabil;
	linux_timex->jitcnt = darwin_timex->jitcnt;
	linux_timex->calcnt = darwin_timex->calcnt;
	linux_timex->errcnt = darwin_timex->errcnt;
	linux_timex->stbcnt = darwin_timex->stbcnt;
}

static void dispatch_ntp_adjtime(struct syscall_gate_state* state)
{
	struct darwin_timex_range_400_plus* darwin_timex =
		(struct darwin_timex_range_400_plus*)state->x[0];
	struct timex linux_timex;

	darwin_timex_to_linux(darwin_timex, &linux_timex);

	errno = 0;
	long result = syscall(SYS_adjtimex, &linux_timex);
	if (result < 0) {
		set_errno_failure(state);
		return;
	}

	linux_timex_to_darwin(&linux_timex, darwin_timex);
	set_success(state, (uint64_t)result);
}

static void dispatch_ntp_gettime(struct syscall_gate_state* state)
{
	struct darwin_ntptimeval_range_400_plus* darwin_ntp =
		(struct darwin_ntptimeval_range_400_plus*)state->x[0];
	struct timex linux_timex;

	memset(&linux_timex, 0, sizeof(linux_timex));
	errno = 0;
	long result = syscall(SYS_adjtimex, &linux_timex);
	if (result < 0) {
		set_errno_failure(state);
		return;
	}

	darwin_ntp->time.tv_sec = linux_timex.time.tv_sec;
	darwin_ntp->time.tv_nsec = linux_timex.time.tv_usec * 1000L;
	darwin_ntp->maxerror = linux_timex.maxerror;
	darwin_ntp->esterror = linux_timex.esterror;
	darwin_ntp->tai = linux_timex.tai;
	darwin_ntp->time_state = (int32_t)result;
	darwin_ntp->__pad = 0;
	set_success(state, (uint64_t)result);
}

static void dispatch_getentropy(struct syscall_gate_state* state)
{
	void* buffer = (void*)state->x[0];
	size_t size = (size_t)state->x[1];
	size_t done = 0;

	if (size > 256) {
		set_failure(state, DARWIN_EIO);
		return;
	}

	while (done < size) {
		errno = 0;
		long result = syscall(SYS_getrandom, (char*)buffer + done,
		                      size - done, 0);
		if (result < 0) {
			if (errno == EINTR)
				continue;
			set_errno_failure(state);
			return;
		}
		done += (size_t)result;
	}

	set_success(state, 0);
}

static void set_deferred_enosys(struct syscall_gate_state* state);

static uint64_t sysinfo_bytes(unsigned long value, unsigned int unit)
{
	if (unit != 0 && value > UINT64_MAX / unit)
		return UINT64_MAX;

	return (uint64_t)value * (uint64_t)unit;
}

static void dispatch_memorystatus_available_memory(struct syscall_gate_state* state)
{
	struct sysinfo info;

	memset(&info, 0, sizeof(info));
	errno = 0;
	long result = syscall(SYS_sysinfo, &info);
	if (result < 0) {
		set_errno_failure(state);
		return;
	}

	uint64_t free_bytes = sysinfo_bytes(info.freeram, info.mem_unit);
	uint64_t buffer_bytes = sysinfo_bytes(info.bufferram, info.mem_unit);
	if (UINT64_MAX - free_bytes < buffer_bytes)
		set_success(state, UINT64_MAX);
	else
		set_success(state, free_bytes + buffer_bytes);
}

static void dispatch_memorystatus_get_level(struct syscall_gate_state* state)
{
	uint32_t* level = (uint32_t*)state->x[0];

	if (!level) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	*level = 0;
	set_success(state, 0);
}

static void dispatch_memorystatus_control(struct syscall_gate_state* state)
{
	uint32_t command = (uint32_t)state->x[0];
	void* buffer = (void*)state->x[3];
	size_t buffer_size = (size_t)state->x[4];

	switch (command) {
	case DARWIN_MEMORYSTATUS_CMD_GET_PRESSURE_STATUS:
		set_success(state, 0);
		return;
	case DARWIN_MEMORYSTATUS_CMD_GET_PROCESS_IS_MANAGED:
	case DARWIN_MEMORYSTATUS_CMD_GET_PROCESS_IS_FREEZABLE:
	case DARWIN_MEMORYSTATUS_CMD_GET_AGGRESSIVE_JETSAM_LENIENT_MODE:
	case DARWIN_MEMORYSTATUS_CMD_GET_PROCESS_IS_FROZEN:
		if (!buffer || buffer_size < sizeof(uint32_t)) {
			set_failure(state, DARWIN_EINVAL);
			return;
		}
		*(uint32_t*)buffer = 0;
		set_success(state, 0);
		return;
	default:
		set_deferred_enosys(state);
		return;
	}
}

static int ulock_is_32bit_operation(uint32_t operation)
{
	switch (operation & DARWIN_UL_OPCODE_MASK) {
	case DARWIN_UL_COMPARE_AND_WAIT:
	case DARWIN_UL_UNFAIR_LOCK:
	case DARWIN_UL_COMPARE_AND_WAIT_SHARED:
		return 1;
	default:
		return 0;
	}
}

static int ulock_is_64bit_operation(uint32_t operation)
{
	switch (operation & DARWIN_UL_OPCODE_MASK) {
	case DARWIN_UL_UNFAIR_LOCK64_SHARED:
	case DARWIN_UL_COMPARE_AND_WAIT64:
	case DARWIN_UL_COMPARE_AND_WAIT64_SHARED:
		return 1;
	default:
		return 0;
	}
}

static void microseconds_to_timespec(uint32_t timeout_us,
                                     struct timespec* timeout)
{
	timeout->tv_sec = timeout_us / 1000000u;
	timeout->tv_nsec = (long)(timeout_us % 1000000u) * 1000L;
}

static void nanoseconds_to_timespec(uint64_t timeout_ns,
                                    struct timespec* timeout)
{
	timeout->tv_sec = (time_t)(timeout_ns / 1000000000ull);
	timeout->tv_nsec = (long)(timeout_ns % 1000000000ull);
}

static void dispatch_ulock_wait(struct syscall_gate_state* state)
{
	uint32_t operation = (uint32_t)state->x[0];
	void* address = (void*)state->x[1];
	uint64_t value = state->x[2];
	uint32_t timeout_us = (uint32_t)state->x[3];
	struct timespec timeout;
	struct timespec* timeout_ptr = NULL;

	if (!address) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	if (ulock_is_64bit_operation(operation)) {
		if (*(uint64_t*)address != value) {
			set_failure(state, linux_errno_to_darwin(EAGAIN));
			return;
		}
		set_deferred_enosys(state);
		return;
	}

	if (!ulock_is_32bit_operation(operation)) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	if (timeout_us != 0) {
		microseconds_to_timespec(timeout_us, &timeout);
		timeout_ptr = &timeout;
	}

	errno = 0;
#ifdef SYS_futex
	long result = syscall(SYS_futex, (uint32_t*)address, FUTEX_WAIT_PRIVATE,
	                      (uint32_t)value, timeout_ptr, NULL, 0);
	if (result < 0) {
		set_errno_failure(state);
		return;
	}
	set_success(state, 0);
#else
	set_deferred_enosys(state);
#endif
}

static void dispatch_ulock_wait2(struct syscall_gate_state* state)
{
	uint32_t operation = (uint32_t)state->x[0];
	void* address = (void*)state->x[1];
	uint64_t value = state->x[2];
	uint64_t timeout_ns = state->x[3];
	uint64_t value2 = state->x[4];
	struct timespec timeout;
	struct timespec* timeout_ptr = NULL;

	if (operation & DARWIN_ULF_DEADLINE) {
		set_deferred_enosys(state);
		return;
	}

	if (value2 != 0) {
		set_deferred_enosys(state);
		return;
	}

	if (!address) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	if (ulock_is_64bit_operation(operation)) {
		if (*(uint64_t*)address != value) {
			set_failure(state, linux_errno_to_darwin(EAGAIN));
			return;
		}
		set_deferred_enosys(state);
		return;
	}

	if (!ulock_is_32bit_operation(operation)) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	if (timeout_ns != 0) {
		nanoseconds_to_timespec(timeout_ns, &timeout);
		timeout_ptr = &timeout;
	}

	errno = 0;
#ifdef SYS_futex
	long result = syscall(SYS_futex, (uint32_t*)address, FUTEX_WAIT_PRIVATE,
	                      (uint32_t)value, timeout_ptr, NULL, 0);
	if (result < 0) {
		set_errno_failure(state);
		return;
	}
	set_success(state, 0);
#else
	set_deferred_enosys(state);
#endif
}

static void dispatch_ulock_wake(struct syscall_gate_state* state)
{
	uint32_t operation = (uint32_t)state->x[0];
	void* address = (void*)state->x[1];
	int wake_count = (operation & DARWIN_ULF_WAKE_ALL) ? INT_MAX : 1;

	if (!address) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	if (!ulock_is_32bit_operation(operation) &&
	    !ulock_is_64bit_operation(operation)) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	errno = 0;
#ifdef SYS_futex
	long result = syscall(SYS_futex, (uint32_t*)address, FUTEX_WAKE_PRIVATE,
	                      wake_count, NULL, NULL, 0);
	if (result < 0) {
		set_errno_failure(state);
		return;
	}
	set_success(state, 0);
#else
	set_deferred_enosys(state);
#endif
}

static void dispatch_shared_region_map_and_slide(struct syscall_gate_state* state)
{
	if (state->x[0] == 0 && state->x[2] == 0) {
		set_success(state, 0);
		return;
	}

	set_deferred_enosys(state);
}

static void zero_guest_buffer(void* buffer, size_t buffer_size)
{
	if (buffer && buffer_size)
		memset(buffer, 0, buffer_size);
}

static void dispatch_success_noop(struct syscall_gate_state* state)
{
	set_success(state, 0);
}

static int pid_is_self_or_current_process(int pid)
{
	return pid == -1 || pid == getpid();
}

static void dispatch_usrctl(struct syscall_gate_state* state)
{
	(void)state->x[0];
	set_failure(state, DARWIN_EPERM);
}

static void dispatch_proc_rlimit_control(struct syscall_gate_state* state)
{
	int pid = (int)state->x[0];
	int flavor = (int)state->x[1];
	uint64_t argument = state->x[2];

	if (!pid_is_self_or_current_process(pid)) {
		set_failure(state, DARWIN_ESRCH);
		return;
	}

	switch (flavor) {
	case DARWIN_RLIMIT_WAKEUPS_MONITOR:
	case DARWIN_RLIMIT_CPU_USAGE_MONITOR:
		set_success(state, 0);
		return;
	case DARWIN_RLIMIT_THREAD_CPULIMITS: {
		uint32_t flags = (uint32_t)argument;
		uint8_t percent = (uint8_t)(flags & 0xffu);

		if (pid != -1 || percent == 0 || percent >= 100) {
			set_failure(state, DARWIN_EINVAL);
			return;
		}

		set_success(state, 0);
		return;
	}
	case DARWIN_RLIMIT_FOOTPRINT_INTERVAL:
		if (((uint32_t)argument & DARWIN_FOOTPRINT_INTERVAL_RESET) == 0) {
			set_failure(state, DARWIN_EINVAL);
			return;
		}

		set_success(state, 0);
		return;
	default:
		set_failure(state, DARWIN_EINVAL);
		return;
	}
}

static uint64_t host_online_cpu_count(void)
{
	unsigned long affinity_mask[16];
	uint64_t result = 0;

	memset(affinity_mask, 0, sizeof(affinity_mask));
	errno = 0;
	long syscall_result = syscall(SYS_sched_getaffinity, 0,
	                              sizeof(affinity_mask), affinity_mask);
	if (syscall_result < 0)
		return 1;

	for (size_t index = 0; index < sizeof(affinity_mask) /
	                            sizeof(affinity_mask[0]); index++) {
		unsigned long value = affinity_mask[index];
		while (value) {
			result += value & 1ul;
			value >>= 1;
		}
	}

	return result ? result : 1;
}

static void dispatch_bsdthread_ctl(struct syscall_gate_state* state)
{
	uint64_t command = state->x[0];
	uint64_t argument1 = state->x[1];
	uint64_t argument2 = state->x[2];
	uint64_t argument3 = state->x[3];

	switch (command) {
	case DARWIN_BSDTHREAD_CTL_SET_QOS:
	case DARWIN_BSDTHREAD_CTL_QOS_DISPATCH_ASYNCHRONOUS_OVERRIDE_ADD:
	case DARWIN_BSDTHREAD_CTL_QOS_DISPATCH_ASYNCHRONOUS_OVERRIDE_RESET:
		set_failure(state, DARWIN_ENOTSUP);
		return;
	case DARWIN_BSDTHREAD_CTL_GET_QOS:
		set_success(state, 0);
		return;
	case DARWIN_BSDTHREAD_CTL_QOS_OVERRIDE_START:
	case DARWIN_BSDTHREAD_CTL_QOS_OVERRIDE_DISPATCH:
	case DARWIN_BSDTHREAD_CTL_QOS_OVERRIDE_RESET:
	case DARWIN_BSDTHREAD_CTL_SET_SELF:
		set_success(state, 0);
		return;
	case DARWIN_BSDTHREAD_CTL_QOS_OVERRIDE_END:
		if (argument3 != 0) {
			set_failure(state, DARWIN_EINVAL);
			return;
		}

		set_success(state, 0);
		return;
	case DARWIN_BSDTHREAD_CTL_QOS_MAX_PARALLELISM: {
		uint64_t supported_flags =
			DARWIN_QOS_PARALLELISM_COUNT_LOGICAL |
			DARWIN_QOS_PARALLELISM_REALTIME |
			DARWIN_QOS_PARALLELISM_CLUSTER_SHARED_RSRC;

		if (argument3 != 0 || (argument2 & ~supported_flags) != 0) {
			set_failure(state, DARWIN_EINVAL);
			return;
		}
		if (argument2 & DARWIN_QOS_PARALLELISM_CLUSTER_SHARED_RSRC) {
			set_failure(state, DARWIN_ENOTSUP);
			return;
		}
		if (argument2 & DARWIN_QOS_PARALLELISM_REALTIME) {
			if (argument1 != 0) {
				set_failure(state, DARWIN_EINVAL);
				return;
			}
		} else if (argument1 == DARWIN_THREAD_QOS_UNSPECIFIED ||
		           argument1 >= DARWIN_THREAD_QOS_LAST) {
			set_failure(state, DARWIN_EINVAL);
			return;
		}

		set_success(state, host_online_cpu_count());
		return;
	}
	case DARWIN_BSDTHREAD_CTL_WORKQ_ALLOW_KILL:
		if (argument2 != 0 || argument3 != 0) {
			set_failure(state, DARWIN_EINVAL);
			return;
		}

		set_success(state, 0);
		return;
	case DARWIN_BSDTHREAD_CTL_DISPATCH_APPLY_ATTR:
		if (argument1 != 1 && argument1 != 2) {
			set_failure(state, DARWIN_EINVAL);
			return;
		}

		set_success(state, 0);
		return;
	case DARWIN_BSDTHREAD_CTL_WORKQ_ALLOW_SIGMASK:
		set_success(state, 0);
		return;
	default:
		set_failure(state, DARWIN_EINVAL);
		return;
	}
}

static struct machgate_kqueue_workloop_entry* kqueue_workloop_find(uint64_t id)
{
	for (int index = 0; index < MACHGATE_KQUEUE_WORKLOOP_MAX; index++) {
		if (kqueue_workloop_entries[index].in_use &&
		    kqueue_workloop_entries[index].id == id)
			return &kqueue_workloop_entries[index];
	}

	return NULL;
}

static struct machgate_kqueue_workloop_entry* kqueue_workloop_free_entry(void)
{
	for (int index = 0; index < MACHGATE_KQUEUE_WORKLOOP_MAX; index++) {
		if (!kqueue_workloop_entries[index].in_use)
			return &kqueue_workloop_entries[index];
	}

	return NULL;
}

static int darwin_policy_is_valid(int policy)
{
	return policy == DARWIN_POLICY_TIMESHARE ||
	       policy == DARWIN_POLICY_RR ||
	       policy == DARWIN_POLICY_FIFO;
}

static int kqueue_workloop_params_are_valid(
	const struct darwin_kqueue_workloop_params_range_400_plus* params)
{
	if (params->flags == 0)
		return DARWIN_EINVAL;
	if (params->flags & ~DARWIN_KQ_WORKLOOP_CREATE_SUPPORTED_FLAGS)
		return DARWIN_EINVAL;
	if ((params->flags & DARWIN_KQ_WORKLOOP_CREATE_SCHED_PRI) &&
	    (params->sched_pri < 1 || params->sched_pri > 63))
		return DARWIN_EINVAL;
	if ((params->flags & DARWIN_KQ_WORKLOOP_CREATE_SCHED_POL) &&
	    !darwin_policy_is_valid(params->sched_pol))
		return DARWIN_EINVAL;
	if ((params->flags & DARWIN_KQ_WORKLOOP_CREATE_CPU_PERCENT) &&
	    (params->cpu_percent <= 0 || params->cpu_percent > 100 ||
	     params->cpu_refillms <= 0 || params->cpu_refillms > 0x00ffffff))
		return DARWIN_EINVAL;
	if (params->flags & (DARWIN_KQ_WORKLOOP_CREATE_WORK_INTERVAL |
	                     DARWIN_KQ_WORKLOOP_CREATE_WITH_BOUND_THREAD))
		return DARWIN_ENOTSUP;

	return 0;
}

static void dispatch_kqueue_workloop_ctl(struct syscall_gate_state* state)
{
	uint64_t command = state->x[0];
	const struct darwin_kqueue_workloop_params_range_400_plus* params =
		(const struct darwin_kqueue_workloop_params_range_400_plus*)state->x[2];
	size_t size = (size_t)state->x[3];

	if (size < sizeof(int32_t) || !params) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}
	if (params->version != (int32_t)size) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	switch (command) {
	case DARWIN_KQ_WORKLOOP_CREATE: {
		int params_error = kqueue_workloop_params_are_valid(params);
		if (params_error != 0) {
			set_failure(state, params_error);
			return;
		}
		if (kqueue_workloop_find(params->id)) {
			set_failure(state, DARWIN_EEXIST);
			return;
		}

		struct machgate_kqueue_workloop_entry* entry =
			kqueue_workloop_free_entry();
		if (!entry) {
			set_failure(state, DARWIN_ENOMEM);
			return;
		}

		errno = 0;
		long fd = syscall(SYS_eventfd2, 0, EFD_CLOEXEC);
		if (fd < 0) {
			set_errno_failure(state);
			return;
		}

		entry->id = params->id;
		entry->fd = (int)fd;
		entry->in_use = 1;
		set_success(state, 0);
		return;
	}
	case DARWIN_KQ_WORKLOOP_DESTROY: {
		struct machgate_kqueue_workloop_entry* entry =
			kqueue_workloop_find(params->id);
		if (!entry) {
			set_failure(state, DARWIN_EINVAL);
			return;
		}

		syscall(SYS_close, entry->fd);
		memset(entry, 0, sizeof(*entry));
		set_success(state, 0);
		return;
	}
	default:
		set_failure(state, DARWIN_EINVAL);
		return;
	}
}

static void dispatch_proc_uuid_policy(struct syscall_gate_state* state)
{
	if (state->x[2] && !state->x[1]) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	set_success(state, 0);
}

static void dispatch_sfi_ctl(struct syscall_gate_state* state)
{
	uint64_t* out_time = (uint64_t*)state->x[3];

	if (out_time)
		*out_time = state->x[2];

	set_success(state, 0);
}

static void dispatch_sfi_pidctl(struct syscall_gate_state* state)
{
	pid_t pid = (pid_t)(int32_t)state->x[1];
	pid_t self = (pid_t)syscall(SYS_getpid);
	uint32_t* out_flags = (uint32_t*)state->x[3];

	if (pid != 0 && pid != self) {
		set_failure(state, DARWIN_ESRCH);
		return;
	}

	if (out_flags)
		*out_flags = 0;

	set_success(state, 0);
}

static void dispatch_coalition(struct syscall_gate_state* state)
{
	uint32_t operation = (uint32_t)state->x[0];
	uint64_t* coalition_id = (uint64_t*)state->x[1];

	if (!coalition_id) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	switch (operation) {
	case DARWIN_COALITION_OP_CREATE:
		*coalition_id = DARWIN_COALITION_ID_SELF;
		set_success(state, 0);
		return;
	case DARWIN_COALITION_OP_TERMINATE:
	case DARWIN_COALITION_OP_REAP:
		if (*coalition_id != 0 && *coalition_id != DARWIN_COALITION_ID_SELF) {
			set_failure(state, DARWIN_ESRCH);
			return;
		}
		set_success(state, 0);
		return;
	default:
		set_failure(state, DARWIN_EINVAL);
		return;
	}
}

static void dispatch_coalition_info(struct syscall_gate_state* state)
{
	uint32_t flavor = (uint32_t)state->x[0];
	uint64_t* coalition_id = (uint64_t*)state->x[1];
	void* buffer = (void*)state->x[2];
	size_t* buffer_size = (size_t*)state->x[3];

	if (!coalition_id) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	if (*coalition_id != 0 && *coalition_id != DARWIN_COALITION_ID_SELF) {
		set_failure(state, DARWIN_ESRCH);
		return;
	}

	switch (flavor) {
	case DARWIN_COALITION_INFO_RESOURCE_USAGE:
	case DARWIN_COALITION_INFO_GET_DEBUG_INFO:
		if (!buffer_size) {
			set_failure(state, DARWIN_EFAULT);
			return;
		}
		zero_guest_buffer(buffer, *buffer_size);
		set_success(state, 0);
		return;
	case DARWIN_COALITION_INFO_SET_NAME:
	case DARWIN_COALITION_INFO_SET_EFFICIENCY:
		set_success(state, 0);
		return;
	default:
		set_failure(state, DARWIN_EINVAL);
		return;
	}
}

static void dispatch_proc_trace_log(struct syscall_gate_state* state)
{
	pid_t pid = (pid_t)(int32_t)state->x[0];
	pid_t self = (pid_t)syscall(SYS_getpid);

	if (pid != 0 && pid != self) {
		set_failure(state, DARWIN_ESRCH);
		return;
	}

	set_success(state, 0);
}

static void dispatch_csrctl(struct syscall_gate_state* state)
{
	uint32_t operation = (uint32_t)state->x[0];
	uint32_t* config = (uint32_t*)state->x[1];
	size_t config_size = (size_t)state->x[2];

	if (!config || config_size < sizeof(uint32_t)) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	switch (operation) {
	case DARWIN_CSR_SYSCALL_CHECK:
		set_success(state, 0);
		return;
	case DARWIN_CSR_SYSCALL_GET_ACTIVE_CONFIG:
		*config = DARWIN_CSR_VALID_FLAGS;
		set_success(state, 0);
		return;
	default:
		set_failure(state, DARWIN_EINVAL);
		return;
	}
}

static void dispatch_mremap_encrypted(struct syscall_gate_state* state)
{
	if (state->x[1] != 0 && !state->x[0]) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	set_success(state, 0);
}

static void dispatch_netagent_trigger(struct syscall_gate_state* state)
{
	size_t uuid_length = (size_t)state->x[1];

	if (uuid_length != 0 && !state->x[0]) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	if (uuid_length != 0 && uuid_length != 16) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	set_success(state, 0);
}

static void dispatch_microstackshot(struct syscall_gate_state* state)
{
	void* trace_buffer = (void*)state->x[0];
	size_t trace_buffer_size = (size_t)(uint32_t)state->x[1];

	if (trace_buffer_size != 0 && !trace_buffer) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	zero_guest_buffer(trace_buffer, trace_buffer_size);
	set_success(state, 0);
}

static void dispatch_grab_pgo_data(struct syscall_gate_state* state)
{
	void* buffer = (void*)state->x[2];
	ssize_t buffer_size = (ssize_t)state->x[3];

	if (buffer_size < 0) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	if (buffer_size != 0 && !buffer) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	zero_guest_buffer(buffer, (size_t)buffer_size);
	set_success(state, 0);
}

static void fill_persona_info_range_400_plus(
	struct darwin_kpersona_info_range_400_plus* info)
{
	uint32_t uid = (uint32_t)syscall(SYS_getuid);
	uint32_t gid = (uint32_t)syscall(SYS_getgid);

	memset(info, 0, sizeof(*info));
	info->persona_info_version = 2;
	info->persona_id = uid;
	info->persona_type = DARWIN_PERSONA_TYPE_DEFAULT;
	info->persona_gid = gid;
	info->persona_gmuid = uid;
	info->persona_uid = uid;
	read_proc_comm_range_400_plus(info->persona_name,
	                              sizeof(info->persona_name));
}

static void dispatch_persona(struct syscall_gate_state* state)
{
	uint32_t operation = (uint32_t)state->x[0];
	struct darwin_kpersona_info_range_400_plus* info =
		(struct darwin_kpersona_info_range_400_plus*)state->x[2];
	uint32_t* persona_id = (uint32_t*)state->x[3];
	size_t* persona_id_count = (size_t*)state->x[4];
	char* path = (char*)state->x[5];
	uint32_t uid = (uint32_t)syscall(SYS_getuid);

	switch (operation) {
	case DARWIN_PERSONA_OP_ALLOC:
	case DARWIN_PERSONA_OP_PALLOC:
	case DARWIN_PERSONA_OP_DEALLOC:
		set_failure(state, DARWIN_EPERM);
		return;
	case DARWIN_PERSONA_OP_GET:
		if (!persona_id) {
			set_failure(state, DARWIN_EFAULT);
			return;
		}
		*persona_id = uid;
		set_success(state, 0);
		return;
	case DARWIN_PERSONA_OP_INFO:
	case DARWIN_PERSONA_OP_PIDINFO:
		if (!info) {
			set_failure(state, DARWIN_EFAULT);
			return;
		}
		fill_persona_info_range_400_plus(info);
		set_success(state, 0);
		return;
	case DARWIN_PERSONA_OP_FIND:
	case DARWIN_PERSONA_OP_FIND_BY_TYPE:
		if (!persona_id_count) {
			set_failure(state, DARWIN_EFAULT);
			return;
		}
		if (persona_id && *persona_id_count > 0)
			*persona_id = uid;
		*persona_id_count = 1;
		set_success(state, 1);
		return;
	case DARWIN_PERSONA_OP_GETPATH:
		if (!path) {
			set_failure(state, DARWIN_EFAULT);
			return;
		}
		path[0] = '\0';
		set_success(state, 0);
		return;
	default:
		set_failure(state, DARWIN_EINVAL);
		return;
	}
}

static void dispatch_work_interval_ctl(struct syscall_gate_state* state)
{
	if (state->x[3] != 0 && !state->x[2]) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	set_success(state, 0);
}

static void dispatch_coalition_ledger(struct syscall_gate_state* state)
{
	uint32_t operation = (uint32_t)state->x[0];
	uint64_t* coalition_id = (uint64_t*)state->x[1];
	void* buffer = (void*)state->x[2];
	size_t* buffer_size = (size_t*)state->x[3];

	if (!coalition_id) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	if (*coalition_id != 0 && *coalition_id != DARWIN_COALITION_ID_SELF) {
		set_failure(state, DARWIN_ESRCH);
		return;
	}

	if (operation != DARWIN_COALITION_LEDGER_SET_LOGICAL_WRITES_LIMIT) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	if (buffer_size)
		zero_guest_buffer(buffer, *buffer_size);

	set_success(state, 0);
}

static void dispatch_log_data(struct syscall_gate_state* state)
{
	void* buffer = (void*)state->x[2];
	size_t buffer_size = (size_t)(uint32_t)state->x[3];

	if (buffer_size != 0 && !buffer) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	set_success(state, 0);
}

static void dispatch_map_with_linking_np(struct syscall_gate_state* state)
{
	if (state->x[1] == 0) {
		set_success(state, 0);
		return;
	}

	set_failure(state, DARWIN_ENOTSUP);
}

static void dispatch_record_system_event(struct syscall_gate_state* state)
{
	set_success(state, 0);
}

static void dispatch_coalition_policy_set(struct syscall_gate_state* state)
{
	uint64_t coalition_id = state->x[0];

	if (coalition_id != 0 && coalition_id != DARWIN_COALITION_ID_SELF) {
		set_failure(state, DARWIN_ESRCH);
		return;
	}

	set_success(state, 0);
}

static void dispatch_coalition_policy_get(struct syscall_gate_state* state)
{
	uint64_t coalition_id = state->x[0];

	if (coalition_id != 0 && coalition_id != DARWIN_COALITION_ID_SELF) {
		set_failure(state, DARWIN_ESRCH);
		return;
	}

	set_success(state, 0);
}

static int darwin_signal_to_linux(int darwin_signal)
{
	switch (darwin_signal) {
	case 0: return 0;
	case 1: return SIGHUP;
	case 2: return SIGINT;
	case 3: return SIGQUIT;
	case 4: return SIGILL;
	case 5: return SIGTRAP;
	case 6: return SIGABRT;
	case 8: return SIGFPE;
	case 9: return SIGKILL;
	case 10: return SIGBUS;
	case 11: return SIGSEGV;
	case 12: return SIGSYS;
	case 13: return SIGPIPE;
	case 14: return SIGALRM;
	case 15: return SIGTERM;
	case 16: return SIGURG;
	case 17: return SIGSTOP;
	case 18: return SIGTSTP;
	case 19: return SIGCONT;
	case 20: return SIGCHLD;
	case 21: return SIGTTIN;
	case 22: return SIGTTOU;
	case 23: return SIGIO;
	case 24: return SIGXCPU;
	case 25: return SIGXFSZ;
	case 26: return SIGVTALRM;
	case 27: return SIGPROF;
	case 28: return SIGWINCH;
	case 30: return SIGUSR1;
	case 31: return SIGUSR2;
	default: return -1;
	}
}

static int linux_signal_to_darwin(int linux_signal)
{
	switch (linux_signal) {
	case SIGHUP: return 1;
	case SIGINT: return 2;
	case SIGQUIT: return 3;
	case SIGILL: return 4;
	case SIGTRAP: return 5;
	case SIGABRT: return 6;
	case SIGFPE: return 8;
	case SIGKILL: return 9;
	case SIGBUS: return 10;
	case SIGSEGV: return 11;
	case SIGSYS: return 12;
	case SIGPIPE: return 13;
	case SIGALRM: return 14;
	case SIGTERM: return 15;
	case SIGURG: return 16;
	case SIGSTOP: return 17;
	case SIGTSTP: return 18;
	case SIGCONT: return 19;
	case SIGCHLD: return 20;
	case SIGTTIN: return 21;
	case SIGTTOU: return 22;
	case SIGIO: return 23;
	case SIGXCPU: return 24;
	case SIGXFSZ: return 25;
	case SIGVTALRM: return 26;
	case SIGPROF: return 27;
	case SIGWINCH: return 28;
	case SIGUSR1: return 30;
	case SIGUSR2: return 31;
	default: return 0;
	}
}

static int darwin_sigset_to_linux(uint32_t darwin_set, sigset_t* linux_set)
{
	if (sigemptyset(linux_set) < 0)
		return 0;

	for (int darwin_signal = 1; darwin_signal <= 31; darwin_signal++) {
		if (!(darwin_set & (1u << (darwin_signal - 1))))
			continue;

		int linux_signal = darwin_signal_to_linux(darwin_signal);
		if (linux_signal <= 0)
			return 0;
		if (sigaddset(linux_set, linux_signal) < 0)
			return 0;
	}

	return 1;
}

static void dispatch_sigwait_nocancel(struct syscall_gate_state* state)
{
	const uint32_t* darwin_set = (const uint32_t*)state->x[0];
	uint32_t* darwin_signal_out = (uint32_t*)state->x[1];
	sigset_t linux_set;
	int linux_signal = 0;

	if (!darwin_set || !darwin_signal_out) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	if (!darwin_sigset_to_linux(*darwin_set, &linux_set)) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	int result = sigwait(&linux_set, &linux_signal);
	if (result != 0) {
		set_failure(state, linux_errno_to_darwin(result));
		return;
	}

	*darwin_signal_out = (uint32_t)linux_signal_to_darwin(linux_signal);
	set_success(state, 0);
}

static void dispatch_sigsuspend_nocancel(struct syscall_gate_state* state)
{
	set_failure(state, DARWIN_EINTR);
}

static int pid_is_current_process(pid_t pid)
{
	pid_t self = (pid_t)syscall(SYS_getpid);

	return pid == 0 || pid == self;
}

static void dispatch_pid_suspend(struct syscall_gate_state* state)
{
	pid_t pid = (pid_t)(int32_t)state->x[0];

	if (!pid_is_current_process(pid)) {
		set_failure(state, DARWIN_ESRCH);
		return;
	}

	set_success(state, 0);
}

static void dispatch_pid_resume(struct syscall_gate_state* state)
{
	pid_t pid = (pid_t)(int32_t)state->x[0];

	if (!pid_is_current_process(pid)) {
		set_failure(state, DARWIN_ESRCH);
		return;
	}

	set_success(state, 0);
}

static void dispatch_pid_hibernate(struct syscall_gate_state* state)
{
	pid_t pid = (pid_t)(int32_t)state->x[0];

	if (pid == -2) {
		set_success(state, 0);
		return;
	}

	if (!pid_is_current_process(pid)) {
		set_failure(state, DARWIN_ESRCH);
		return;
	}

	set_failure(state, DARWIN_ENOTSUP);
}

static void dispatch_kas_info(struct syscall_gate_state* state)
{
	int selector = (int)state->x[0];
	uint64_t* value = (uint64_t*)state->x[1];
	size_t* size = (size_t*)state->x[2];

	if (!value || !size) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	if (selector < 0 || selector >= DARWIN_KAS_INFO_MAX_SELECTOR) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	if (*size < sizeof(uint64_t)) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	*value = 0;
	*size = sizeof(uint64_t);
	set_success(state, 0);
}

static void set_deferred_enosys(struct syscall_gate_state* state)
{
	set_failure(state, DARWIN_ENOSYS);
}

static void dispatch_terminate_with_payload(struct syscall_gate_state* state)
{
	int pid = (int)state->x[0];

	if (pid <= 0) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	if (pid == getpid()) {
		syscall(SYS_exit_group, 137);
		_exit(137);
	}

	errno = 0;
	long result = syscall(SYS_kill, pid, SIGKILL);
	finish_syscall_result(state, result);
}

static void dispatch_range_200_alias(struct syscall_gate_state* state,
                                     uint64_t target_syscall)
{
	uint64_t original_syscall = state->x[16];

	state->x[16] = target_syscall;
	syscall_range_200_399_dispatch(state);
	state->x[16] = original_syscall;
}

static void dispatch_wait4_single_process(struct syscall_gate_state* state)
{
	if (!((int)state->x[2] & WNOHANG)) {
		set_deferred_enosys(state);
		return;
	}

	set_failure(state, 10);
}

static void dispatch_waitid_single_process(struct syscall_gate_state* state)
{
	if (!((int)state->x[3] & WNOHANG)) {
		set_deferred_enosys(state);
		return;
	}

	set_failure(state, 10);
}

int syscall_range_400_plus_dispatch(struct syscall_gate_state* state)
{
	switch (state->x[16]) {
	case DARWIN_SYS_wait4_nocancel:
		dispatch_wait4_single_process(state);
		return 1;
	case DARWIN_SYS_waitid_nocancel:
		dispatch_waitid_single_process(state);
		return 1;
	case DARWIN_SYS_recvmsg_nocancel:
		dispatch_recvmsg(state);
		return 1;
	case DARWIN_SYS_sendmsg_nocancel:
		dispatch_sendmsg(state);
		return 1;
	case DARWIN_SYS_recvfrom_nocancel:
		dispatch_recvfrom(state);
		return 1;
	case DARWIN_SYS_accept_nocancel:
		dispatch_accept(state);
		return 1;
	case DARWIN_SYS_msync_nocancel: {
		errno = 0;
		long result = syscall(SYS_msync, (void*)state->x[0],
		                      (size_t)state->x[1], (int)state->x[2]);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_fcntl_nocancel: {
		errno = 0;
		long result = raw_fcntl((int)state->x[0], (int)state->x[1],
		                        state->x[2]);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_select_nocancel:
		dispatch_select(state);
		return 1;
	case DARWIN_SYS_fsync_nocancel: {
		errno = 0;
		long result = syscall(SYS_fsync, (int)state->x[0]);
		finish_syscall_result(state, result);
		return 1;
	}
		case DARWIN_SYS_connect_nocancel:
			dispatch_connect(state);
			return 1;
		case DARWIN_SYS_sigsuspend_nocancel:
			dispatch_sigsuspend_nocancel(state);
			return 1;
	case DARWIN_SYS_readv_nocancel: {
		errno = 0;
		long result = syscall(SYS_readv, (int)state->x[0],
		                      (const struct iovec*)state->x[1],
		                      (int)state->x[2]);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_writev_nocancel: {
		long result = machgate_syscall_writev_no_sigpipe((int)state->x[0],
		                                                 (const struct iovec*)state->x[1],
		                                                 (int)state->x[2]);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_sendto_nocancel:
		dispatch_sendto(state);
		return 1;
	case DARWIN_SYS_pread_nocancel: {
		errno = 0;
		long result = syscall(SYS_pread64, (int)state->x[0],
		                      (void*)state->x[1],
		                      (size_t)state->x[2],
		                      (off_t)state->x[3]);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_pwrite_nocancel: {
		errno = 0;
		long result = syscall(SYS_pwrite64, (int)state->x[0],
		                      (const void*)state->x[1],
		                      (size_t)state->x[2],
		                      (off_t)state->x[3]);
		finish_syscall_result(state, result);
		return 1;
	}
		case DARWIN_SYS_poll_nocancel: {
			errno = 0;
			long result = raw_poll((struct pollfd*)state->x[0],
			                       (nfds_t)state->x[1], (int)state->x[2]);
			finish_syscall_result(state, result);
			return 1;
		}
	case DARWIN_SYS_msgsnd_nocancel:
		dispatch_range_200_alias(state, DARWIN_SYS_RANGE_200_MSGSND);
		return 1;
	case DARWIN_SYS_msgrcv_nocancel:
		dispatch_range_200_alias(state, DARWIN_SYS_RANGE_200_MSGRCV);
		return 1;
	case DARWIN_SYS_sem_wait_nocancel:
		dispatch_range_200_alias(state, DARWIN_SYS_RANGE_200_SEM_WAIT);
		return 1;
	case DARWIN_SYS_aio_suspend_nocancel:
		dispatch_range_200_alias(state, DARWIN_SYS_RANGE_200_AIO_SUSPEND);
		return 1;
	case DARWIN_SYS___sigwait_nocancel:
		dispatch_sigwait_nocancel(state);
		return 1;
	case DARWIN_SYS___semwait_signal_nocancel:
		dispatch_range_200_alias(state, DARWIN_SYS_RANGE_200_SEMWAIT_SIGNAL);
		return 1;
	case DARWIN_SYS___mac_mount:
		dispatch_mac_mount(state);
		return 1;
	case DARWIN_SYS___mac_get_mount:
		dispatch_mac_get_label_range_400_plus(state, state->x[1]);
		return 1;
	case DARWIN_SYS___mac_getfsstat:
		set_unsupported_feature(state);
		return 1;
	case DARWIN_SYS_fsgetpath:
		dispatch_fsgetpath(state);
		return 1;
	case DARWIN_SYS_audit_session_self:
		dispatch_audit_session_self(state);
		return 1;
	case DARWIN_SYS_audit_session_join:
		dispatch_audit_session_join(state);
		return 1;
	case DARWIN_SYS_sys_fileport_makeport:
		dispatch_fileport_makeport(state);
		return 1;
	case DARWIN_SYS_sys_fileport_makefd:
		dispatch_fileport_makefd(state);
		return 1;
		case DARWIN_SYS_audit_session_port:
			dispatch_audit_session_port(state);
			return 1;
		case DARWIN_SYS_pid_suspend:
			dispatch_pid_suspend(state);
			return 1;
		case DARWIN_SYS_pid_resume:
			dispatch_pid_resume(state);
			return 1;
		case DARWIN_SYS_pid_hibernate:
			dispatch_pid_hibernate(state);
			return 1;
		case DARWIN_SYS_pid_shutdown_sockets:
			dispatch_pid_shutdown_sockets(state);
			return 1;
		case DARWIN_SYS_kas_info:
			dispatch_kas_info(state);
			return 1;
	case DARWIN_SYS_memorystatus_control:
		dispatch_memorystatus_control(state);
		return 1;
	case DARWIN_SYS_guarded_open_np:
		dispatch_guarded_open(state);
		return 1;
	case DARWIN_SYS_guarded_close_np:
		dispatch_guarded_close(state);
		return 1;
	case DARWIN_SYS_guarded_kqueue_np:
		dispatch_guarded_kqueue(state);
		return 1;
	case DARWIN_SYS_change_fdguard_np:
		dispatch_change_fdguard(state);
		return 1;
	case DARWIN_SYS_usrctl:
		dispatch_usrctl(state);
		return 1;
	case DARWIN_SYS_proc_rlimit_control:
		dispatch_proc_rlimit_control(state);
		return 1;
	case DARWIN_SYS_memorystatus_get_level:
		dispatch_memorystatus_get_level(state);
		return 1;
	case DARWIN_SYS_connectx:
		dispatch_connectx(state);
		return 1;
	case DARWIN_SYS_disconnectx:
		dispatch_disconnectx(state);
		return 1;
	case DARWIN_SYS_peeloff:
		set_failure(state, DARWIN_ENOTSUP);
		return 1;
	case DARWIN_SYS_socket_delegate:
		dispatch_socket_delegate(state);
		return 1;
	case DARWIN_SYS_telemetry:
	case DARWIN_SYS_system_override:
	case DARWIN_SYS_vfs_purge:
		dispatch_success_noop(state);
		return 1;
	case DARWIN_SYS_proc_uuid_policy:
		dispatch_proc_uuid_policy(state);
		return 1;
	case DARWIN_SYS_sfi_ctl:
		dispatch_sfi_ctl(state);
		return 1;
	case DARWIN_SYS_sfi_pidctl:
		dispatch_sfi_pidctl(state);
		return 1;
	case DARWIN_SYS_coalition:
		dispatch_coalition(state);
		return 1;
	case DARWIN_SYS_coalition_info:
		dispatch_coalition_info(state);
		return 1;
	case DARWIN_SYS_necp_match_policy:
		dispatch_necp_match_policy(state);
		return 1;
		case DARWIN_SYS_bsdthread_ctl:
			dispatch_bsdthread_ctl(state);
			return 1;
		case DARWIN_SYS_getattrlistbulk:
			dispatch_getattrlistbulk(state);
			return 1;
		case DARWIN_SYS_clonefileat:
			dispatch_clonefileat(state);
			return 1;
		case DARWIN_SYS_getattrlistat:
			dispatch_getattrlistat(state);
			return 1;
		case DARWIN_SYS_openbyid_np:
			dispatch_openbyid_np(state);
			return 1;
		case DARWIN_SYS_fclonefileat:
			dispatch_fclonefileat(state);
			return 1;
		case DARWIN_SYS_setattrlistat:
			dispatch_setattrlistat(state);
			return 1;
		case DARWIN_SYS_proc_trace_log:
			dispatch_proc_trace_log(state);
			return 1;
		case DARWIN_SYS_csrctl:
			dispatch_csrctl(state);
			return 1;
		case DARWIN_SYS_mremap_encrypted:
			dispatch_mremap_encrypted(state);
			return 1;
		case DARWIN_SYS_netagent_trigger:
			dispatch_netagent_trigger(state);
			return 1;
		case DARWIN_SYS_stack_snapshot_with_config:
			dispatch_success_noop(state);
			return 1;
		case DARWIN_SYS_microstackshot:
			dispatch_microstackshot(state);
			return 1;
		case DARWIN_SYS_grab_pgo_data:
			dispatch_grab_pgo_data(state);
			return 1;
		case DARWIN_SYS_persona:
			dispatch_persona(state);
			return 1;
		case DARWIN_SYS_fs_snapshot:
			dispatch_fs_snapshot(state);
			return 1;
		case DARWIN_SYS_fmount:
			dispatch_fmount(state);
			return 1;
	case DARWIN_SYS_guarded_open_dprotected_np:
		dispatch_guarded_open_dprotected(state);
		return 1;
	case DARWIN_SYS_guarded_write_np:
		dispatch_guarded_write(state);
		return 1;
	case DARWIN_SYS_guarded_pwrite_np:
		dispatch_guarded_pwrite(state);
		return 1;
	case DARWIN_SYS_guarded_writev_np:
		dispatch_guarded_writev(state);
		return 1;
		case DARWIN_SYS_openat:
		case DARWIN_SYS_openat_nocancel: {
			errno = 0;
		long result = syscall(SYS_openat, translate_dirfd((int)state->x[0]),
		                      (const char*)state->x[1],
		                      translate_open_flags((int)state->x[2]),
		                      (mode_t)state->x[3]);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_renameat: {
		errno = 0;
		long result = syscall(SYS_renameat, translate_dirfd((int)state->x[0]),
		                      (const char*)state->x[1],
		                      translate_dirfd((int)state->x[2]),
		                      (const char*)state->x[3]);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_renameatx_np: {
		unsigned int linux_flags;
		if (!translate_renameatx_flags((uint32_t)state->x[4], &linux_flags)) {
			set_failure(state, linux_errno_to_darwin(errno));
			return 1;
		}

		errno = 0;
		long result;
		if (linux_flags == 0) {
			result = syscall(SYS_renameat,
			                 translate_dirfd((int)state->x[0]),
			                 (const char*)state->x[1],
			                 translate_dirfd((int)state->x[2]),
			                 (const char*)state->x[3]);
		} else {
#ifdef SYS_renameat2
			result = syscall(SYS_renameat2,
			                 translate_dirfd((int)state->x[0]),
			                 (const char*)state->x[1],
			                 translate_dirfd((int)state->x[2]),
			                 (const char*)state->x[3],
			                 linux_flags);
#else
			errno = ENOSYS;
			result = -1;
#endif
		}
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_faccessat: {
		int flags = (int)state->x[3];
		if (has_unsupported_at_flags(flags)) {
			set_failure(state, DARWIN_EINVAL);
			return 1;
		}
		errno = 0;
		long result = syscall(SYS_faccessat, translate_dirfd((int)state->x[0]),
		                      (const char*)state->x[1], (int)state->x[2],
		                      translate_at_flags(flags));
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_fchmodat: {
		if (state->x[3] != 0) {
			set_failure(state, DARWIN_EINVAL);
			return 1;
		}
		errno = 0;
		long result = syscall(SYS_fchmodat, translate_dirfd((int)state->x[0]),
		                      (const char*)state->x[1], (mode_t)state->x[2]);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_fchownat: {
		int flags = (int)state->x[4];
		if (has_unsupported_at_flags(flags)) {
			set_failure(state, DARWIN_EINVAL);
			return 1;
		}
		errno = 0;
		long result = syscall(SYS_fchownat, translate_dirfd((int)state->x[0]),
		                      (const char*)state->x[1], (uid_t)state->x[2],
		                      (gid_t)state->x[3], translate_at_flags(flags));
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_fstatat:
	case DARWIN_SYS_fstatat64:
		dispatch_fstatat(state);
		return 1;
	case DARWIN_SYS_linkat: {
		errno = 0;
		long result = syscall(SYS_linkat, translate_dirfd((int)state->x[0]),
		                      (const char*)state->x[1],
		                      translate_dirfd((int)state->x[2]),
		                      (const char*)state->x[3],
		                      translate_linkat_flags((int)state->x[4]));
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_unlinkat: {
		int flags = (int)state->x[2];
		if (flags & ~(DARWIN_AT_REMOVEDIR)) {
			set_failure(state, DARWIN_EINVAL);
			return 1;
		}
		errno = 0;
		long result = syscall(SYS_unlinkat, translate_dirfd((int)state->x[0]),
		                      (const char*)state->x[1],
		                      translate_at_flags(flags));
		finish_syscall_result(state, result);
		return 1;
	}
		case DARWIN_SYS_readlinkat: {
			if (should_hide_proc_exe_symlink((const char*)state->x[1])) {
				set_failure(state, DARWIN_ENOENT);
				return 1;
			}
			errno = 0;
			long result = syscall(SYS_readlinkat, translate_dirfd((int)state->x[0]),
			                      (const char*)state->x[1], (char*)state->x[2],
		                      (size_t)state->x[3]);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_symlinkat: {
		errno = 0;
		long result = syscall(SYS_symlinkat, (const char*)state->x[0],
		                      translate_dirfd((int)state->x[1]),
		                      (const char*)state->x[2]);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_mkdirat: {
		errno = 0;
		long result = syscall(SYS_mkdirat, translate_dirfd((int)state->x[0]),
		                      (const char*)state->x[1], (mode_t)state->x[2]);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_recvmsg_x:
		dispatch_recvmsg_x(state);
		return 1;
	case DARWIN_SYS_sendmsg_x:
		dispatch_sendmsg_x(state);
		return 1;
	case DARWIN_SYS_mach_eventlink_signal:
	case DARWIN_SYS_mach_eventlink_wait_until:
	case DARWIN_SYS_mach_eventlink_signal_wait_until:
		dispatch_eventlink_missing_port(state);
		return 1;
	case DARWIN_SYS_work_interval_ctl:
		dispatch_work_interval_ctl(state);
		return 1;
	case DARWIN_SYS_thread_selfusage:
		set_success(state, 0);
		return 1;
	case DARWIN_SYS_getentropy:
		dispatch_getentropy(state);
		return 1;
	case DARWIN_SYS_necp_open:
		dispatch_dummy_control_open(state);
		return 1;
	case DARWIN_SYS_necp_client_action:
		dispatch_necp_client_action(state);
		return 1;
	case DARWIN_SYS___nexus_open:
	case DARWIN_SYS___nexus_register:
	case DARWIN_SYS___nexus_deregister:
	case DARWIN_SYS___nexus_create:
	case DARWIN_SYS___nexus_destroy:
	case DARWIN_SYS___nexus_get_opt:
	case DARWIN_SYS___nexus_set_opt:
	case DARWIN_SYS___channel_open:
	case DARWIN_SYS___channel_get_info:
	case DARWIN_SYS___channel_sync:
	case DARWIN_SYS___channel_get_opt:
	case DARWIN_SYS___channel_set_opt:
		set_failure(state, DARWIN_ENOTSUP);
		return 1;
	case DARWIN_SYS_sys_ulock_wait:
		dispatch_ulock_wait(state);
		return 1;
	case DARWIN_SYS_sys_ulock_wake:
		dispatch_ulock_wake(state);
		return 1;
	case DARWIN_SYS_terminate_with_payload:
		dispatch_terminate_with_payload(state);
		return 1;
	case DARWIN_SYS_abort_with_payload:
		syscall(SYS_exit_group, 134);
		_exit(134);
	case DARWIN_SYS_necp_session_open:
		dispatch_dummy_control_open(state);
		return 1;
	case DARWIN_SYS_necp_session_action:
		dispatch_necp_session_action(state);
		return 1;
	case DARWIN_SYS_net_qos_guideline:
		dispatch_net_qos_guideline(state);
		return 1;
	case DARWIN_SYS_ntp_adjtime:
		dispatch_ntp_adjtime(state);
		return 1;
	case DARWIN_SYS_ntp_gettime:
		dispatch_ntp_gettime(state);
		return 1;
	case DARWIN_SYS_os_fault_with_payload:
		set_success(state, 0);
		return 1;
	case DARWIN_SYS_kqueue_workloop_ctl:
		dispatch_kqueue_workloop_ctl(state);
		return 1;
	case DARWIN_SYS___mach_bridge_remote_time:
		set_success(state, state->x[0]);
		return 1;
	case DARWIN_SYS_coalition_ledger:
		dispatch_coalition_ledger(state);
		return 1;
	case DARWIN_SYS_log_data:
		dispatch_log_data(state);
		return 1;
	case DARWIN_SYS_objc_bp_assist_cfg_np:
		dispatch_success_noop(state);
		return 1;
	case DARWIN_SYS_memorystatus_available_memory:
		dispatch_memorystatus_available_memory(state);
		return 1;
	case DARWIN_SYS_shared_region_map_and_slide_2_np:
		dispatch_shared_region_map_and_slide(state);
		return 1;
	case DARWIN_SYS_pivot_root:
		dispatch_pivot_root(state);
		return 1;
	case DARWIN_SYS_task_inspect_for_pid:
	case DARWIN_SYS_task_read_for_pid:
		dispatch_task_port_request(state);
		return 1;
	case DARWIN_SYS_preadv:
	case DARWIN_SYS_preadv_nocancel: {
		errno = 0;
		long result = syscall(SYS_preadv, (int)state->x[0],
		                      (const struct iovec*)state->x[1],
		                      (int)state->x[2], (off_t)state->x[3]);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_pwritev:
	case DARWIN_SYS_pwritev_nocancel: {
		errno = 0;
		long result = syscall(SYS_pwritev, (int)state->x[0],
		                      (const struct iovec*)state->x[1],
		                      (int)state->x[2], (off_t)state->x[3]);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_sys_ulock_wait2:
		dispatch_ulock_wait2(state);
		return 1;
	case DARWIN_SYS_proc_info_extended_id:
		dispatch_proc_info_extended_id(state);
		return 1;
	case DARWIN_SYS_tracker_action:
		dispatch_tracker_action(state);
		return 1;
	case DARWIN_SYS_debug_syscall_reject:
	case DARWIN_SYS_sys_debug_syscall_reject_config:
		dispatch_success_noop(state);
		return 1;
	case DARWIN_SYS_graftdmg:
		dispatch_graftdmg(state);
		return 1;
	case DARWIN_SYS_map_with_linking_np:
		dispatch_map_with_linking_np(state);
		return 1;
	case DARWIN_SYS_sys_record_system_event:
		dispatch_record_system_event(state);
		return 1;
	case DARWIN_SYS_freadlink: {
		errno = 0;
		long result = syscall(SYS_readlinkat, (int)state->x[0], "",
		                      (char*)state->x[1], (size_t)state->x[2]);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_mkfifoat: {
		errno = 0;
		long result = syscall(SYS_mknodat, translate_dirfd((int)state->x[0]),
		                      (const char*)state->x[1],
		                      (mode_t)(state->x[2] | S_IFIFO), 0);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_mknodat: {
		errno = 0;
		long result = syscall(SYS_mknodat, translate_dirfd((int)state->x[0]),
		                      (const char*)state->x[1], (mode_t)state->x[2],
		                      (dev_t)state->x[3]);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_ungraftdmg:
		dispatch_ungraftdmg(state);
		return 1;
	case DARWIN_SYS_sys_coalition_policy_set:
		dispatch_coalition_policy_set(state);
		return 1;
	case DARWIN_SYS_sys_coalition_policy_get:
		dispatch_coalition_policy_get(state);
		return 1;
	default:
		if (state->x[16] >= DARWIN_SYS_wait4_nocancel && state->x[16] <= 557) {
			set_deferred_enosys(state);
			return 1;
		}
		return 0;
	}
}
