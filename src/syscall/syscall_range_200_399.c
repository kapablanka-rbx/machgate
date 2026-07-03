#include "syscall_range_200_399.h"
#include "execve_reexec.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/futex.h>
#include <mntent.h>
#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/ipc.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/time.h>
#include <sys/syscall.h>
#include <sys/xattr.h>
#include <time.h>
#include <unistd.h>

extern const char* __machgate_guest_executable_path;

#define DARWIN_SYS_truncate 200
#define DARWIN_SYS_ftruncate 201
#define DARWIN_SYS_sysctl 202
#define DARWIN_SYS_mlock 203
#define DARWIN_SYS_munlock 204
#define DARWIN_SYS_undelete 205
#define DARWIN_SYS_open_dprotected_np 216
#define DARWIN_SYS_fsgetpath_ext 217
#define DARWIN_SYS_openat_dprotected_np 218
#define DARWIN_SYS_getattrlist 220
#define DARWIN_SYS_setattrlist 221
#define DARWIN_SYS_getdirentriesattr 222
#define DARWIN_SYS_exchangedata 223
#define DARWIN_SYS_searchfs 225
#define DARWIN_SYS_delete 226
#define DARWIN_SYS_copyfile 227
#define DARWIN_SYS_fgetattrlist 228
#define DARWIN_SYS_fsetattrlist 229
#define DARWIN_SYS_poll 230
#define DARWIN_SYS_getxattr 234
#define DARWIN_SYS_fgetxattr 235
#define DARWIN_SYS_setxattr 236
#define DARWIN_SYS_fsetxattr 237
#define DARWIN_SYS_removexattr 238
#define DARWIN_SYS_fremovexattr 239
#define DARWIN_SYS_listxattr 240
#define DARWIN_SYS_flistxattr 241
#define DARWIN_SYS_fsctl 242
#define DARWIN_SYS_initgroups 243
#define DARWIN_SYS_posix_spawn 244
#define DARWIN_SYS_ffsctl 245
#define DARWIN_SYS_fhopen 248
#define DARWIN_SYS_minherit 250
#define DARWIN_SYS_semsys 251
#define DARWIN_SYS_msgsys 252
#define DARWIN_SYS_shmsys 253
#define DARWIN_SYS_semctl 254
#define DARWIN_SYS_semget 255
#define DARWIN_SYS_semop 256
#define DARWIN_SYS_msgctl 258
#define DARWIN_SYS_msgget 259
#define DARWIN_SYS_msgsnd 260
#define DARWIN_SYS_msgrcv 261
#define DARWIN_SYS_shmat 262
#define DARWIN_SYS_shmctl 263
#define DARWIN_SYS_shmdt 264
#define DARWIN_SYS_shmget 265
#define DARWIN_SYS_shm_open 266
#define DARWIN_SYS_shm_unlink 267
#define DARWIN_SYS_sem_open 268
#define DARWIN_SYS_sem_close 269
#define DARWIN_SYS_sem_unlink 270
#define DARWIN_SYS_sem_wait 271
#define DARWIN_SYS_sem_trywait 272
#define DARWIN_SYS_sem_post 273
#define DARWIN_SYS_sys_sysctlbyname 274
#define DARWIN_SYS_open_extended 277
#define DARWIN_SYS_umask_extended 278
#define DARWIN_SYS_stat_extended 279
#define DARWIN_SYS_lstat_extended 280
#define DARWIN_SYS_sys_fstat_extended 281
#define DARWIN_SYS_chmod_extended 282
#define DARWIN_SYS_fchmod_extended 283
#define DARWIN_SYS_access_extended 284
#define DARWIN_SYS_sys_settid 285
#define DARWIN_SYS_gettid 286
#define DARWIN_SYS_setsgroups 287
#define DARWIN_SYS_getsgroups 288
#define DARWIN_SYS_setwgroups 289
#define DARWIN_SYS_getwgroups 290
#define DARWIN_SYS_mkfifo_extended 291
#define DARWIN_SYS_mkdir_extended 292
#define DARWIN_SYS_identitysvc 293
#define DARWIN_SYS_shared_region_check_np 294
#define DARWIN_SYS_vm_pressure_monitor 296
#define DARWIN_SYS_psynch_rw_longrdlock 297
#define DARWIN_SYS_psynch_rw_yieldwrlock 298
#define DARWIN_SYS_psynch_rw_downgrade 299
#define DARWIN_SYS_psynch_rw_upgrade 300
#define DARWIN_SYS_psynch_mutexwait 301
#define DARWIN_SYS_psynch_mutexdrop 302
#define DARWIN_SYS_psynch_cvbroad 303
#define DARWIN_SYS_psynch_cvsignal 304
#define DARWIN_SYS_psynch_cvwait 305
#define DARWIN_SYS_psynch_rw_rdlock 306
#define DARWIN_SYS_psynch_rw_wrlock 307
#define DARWIN_SYS_psynch_rw_unlock 308
#define DARWIN_SYS_psynch_rw_unlock2 309
#define DARWIN_SYS_getsid 310
#define DARWIN_SYS_sys_settid_with_pid 311
#define DARWIN_SYS_psynch_cvclrprepost 312
#define DARWIN_SYS_aio_fsync 313
#define DARWIN_SYS_aio_return 314
#define DARWIN_SYS_aio_suspend 315
#define DARWIN_SYS_aio_cancel 316
#define DARWIN_SYS_aio_error 317
#define DARWIN_SYS_aio_read 318
#define DARWIN_SYS_aio_write 319
#define DARWIN_SYS_lio_listio 320
#define DARWIN_SYS_iopolicysys 322
#define DARWIN_SYS_process_policy 323
#define DARWIN_SYS_mlockall 324
#define DARWIN_SYS_munlockall 325
#define DARWIN_SYS_issetugid 327
#define DARWIN_SYS___pthread_kill 328
#define DARWIN_SYS___pthread_sigmask 329
#define DARWIN_SYS___sigwait 330
#define DARWIN_SYS___disable_threadsignal 331
#define DARWIN_SYS___pthread_markcancel 332
#define DARWIN_SYS___pthread_canceled 333
#define DARWIN_SYS___semwait_signal 334
#define DARWIN_SYS_proc_info 336
#define DARWIN_SYS_sendfile 337
#define DARWIN_SYS_stat64 338
#define DARWIN_SYS_fstat64 339
#define DARWIN_SYS_lstat64 340
#define DARWIN_SYS_stat64_extended 341
#define DARWIN_SYS_lstat64_extended 342
#define DARWIN_SYS_sys_fstat64_extended 343
#define DARWIN_SYS_getdirentries64 344
#define DARWIN_SYS___pthread_chdir 348
#define DARWIN_SYS___pthread_fchdir 349
#define DARWIN_SYS_statfs64 345
#define DARWIN_SYS_fstatfs64 346
#define DARWIN_SYS_getfsstat64 347
#define DARWIN_SYS_audit 350
#define DARWIN_SYS_auditon 351
#define DARWIN_SYS_getauid 353
#define DARWIN_SYS_setauid 354
#define DARWIN_SYS_getaudit_addr 357
#define DARWIN_SYS_setaudit_addr 358
#define DARWIN_SYS_auditctl 359
#define DARWIN_SYS_bsdthread_create 360
#define DARWIN_SYS_bsdthread_terminate 361
#define DARWIN_SYS_kqueue 362
#define DARWIN_SYS_kevent 363
#define DARWIN_SYS_lchown 364
#define DARWIN_SYS_bsdthread_register 366
#define DARWIN_SYS_workq_open 367
#define DARWIN_SYS_workq_kernreturn 368
#define DARWIN_SYS_kevent64 369
#define DARWIN_SYS_thread_selfid 372
#define DARWIN_SYS_ledger 373
#define DARWIN_SYS_kevent_qos 374
#define DARWIN_SYS_kevent_id 375
#define DARWIN_SYS___mac_execve 380
#define DARWIN_SYS___mac_syscall 381
#define DARWIN_SYS___mac_get_file 382
#define DARWIN_SYS___mac_set_file 383
#define DARWIN_SYS___mac_get_link 384
#define DARWIN_SYS___mac_set_link 385
#define DARWIN_SYS___mac_get_proc 386
#define DARWIN_SYS___mac_set_proc 387
#define DARWIN_SYS___mac_get_fd 388
#define DARWIN_SYS___mac_set_fd 389
#define DARWIN_SYS___mac_get_pid 390
#define DARWIN_SYS_pselect 394
#define DARWIN_SYS_pselect_nocancel 395
#define DARWIN_SYS_read_nocancel 396
#define DARWIN_SYS_write_nocancel 397
#define DARWIN_SYS_open_nocancel 398
#define DARWIN_SYS_close_nocancel 399

#define DARWIN_EPERM  1
#define DARWIN_ENOSYS 78
#define DARWIN_ENOMEM 12
#define DARWIN_EFAULT 14
#define DARWIN_EINVAL 22
#define DARWIN_ENOTSUP 45
#define SYSCALL_GATE_CARRY 0x20000000u

#define DARWIN_SIG_BLOCK   1
#define DARWIN_SIG_UNBLOCK 2
#define DARWIN_SIG_SETMASK 3

#define DARWIN_O_NONBLOCK  0x00000004
#define DARWIN_O_APPEND    0x00000008
#define DARWIN_O_NOFOLLOW  0x00000100
#define DARWIN_O_CREAT     0x00000200
#define DARWIN_O_TRUNC     0x00000400
#define DARWIN_O_EXCL      0x00000800
#define DARWIN_O_NOCTTY    0x00020000
#define DARWIN_O_DIRECTORY 0x00100000
#define DARWIN_O_CLOEXEC   0x01000000

#define DARWIN_MNT_RDONLY 0x00000001u
#define DARWIN_MNT_NOEXEC 0x00000004u
#define DARWIN_MNT_NOSUID 0x00000008u
#define DARWIN_MNT_NODEV  0x00000010u
#define DARWIN_MNT_LOCAL  0x00001000u

#define DARWIN_XATTR_NOFOLLOW 0x0001
#define DARWIN_XATTR_CREATE   0x0002
#define DARWIN_XATTR_REPLACE  0x0004

#define DARWIN_ACCESSX_MAX_DESCRIPTORS 100
#define DARWIN_ACCESSX_MAX_TABLESIZE   (16 * 1024)
#define DARWIN_ACCESSX_EXECUTE_OK      (1 << 0)
#define DARWIN_ACCESSX_WRITE_OK        (1 << 1)
#define DARWIN_ACCESSX_READ_OK         (1 << 2)
#define DARWIN_ACCESSX_READ_EXT_OK     (1 << 9)
#define DARWIN_ACCESSX_WRITE_EXT_OK    (1 << 10)
#define DARWIN_ACCESSX_EXECUTE_EXT_OK  (1 << 11)
#define DARWIN_ACCESSX_DELETE_OK       (1 << 12)
#define DARWIN_ACCESSX_APPEND_OK       (1 << 13)
#define DARWIN_ACCESSX_RMFILE_OK       (1 << 14)
#define DARWIN_ACCESSX_RATTR_OK        (1 << 15)
#define DARWIN_ACCESSX_WATTR_OK        (1 << 16)
#define DARWIN_ACCESSX_REXT_OK         (1 << 17)
#define DARWIN_ACCESSX_WEXT_OK         (1 << 18)
#define DARWIN_ACCESSX_RPERM_OK        (1 << 19)
#define DARWIN_ACCESSX_WPERM_OK        (1 << 20)
#define DARWIN_ACCESSX_CHOWN_OK        (1 << 21)

#define DARWIN_FSOPT_NOFOLLOW 0x00000001u
#define DARWIN_FSOPT_ATTR_CMN_EXTENDED 0x00000020u
#define DARWIN_FSOPT_NOFIRMLINKPATH 0x00000001u
#define DARWIN_FSOPT_ISREALFSID 0x00000002u

#define DARWIN_SRCHFS_START 0x00000001u
#define DARWIN_SRCHFS_MATCHPARTIALNAMES 0x00000002u
#define DARWIN_SRCHFS_MATCHDIRS 0x00000004u
#define DARWIN_SRCHFS_MATCHFILES 0x00000008u
#define DARWIN_SRCHFS_NEGATEPARAMS 0x80000000u
#define DARWIN_SRCHFS_VALIDOPTIONSMASK 0x800000ffu
#define DARWIN_SEARCHFS_MAX_SEARCHPARMS 4096u

#define DARWIN_VM_INHERIT_SHARE 0
#define DARWIN_VM_INHERIT_COPY 1
#define DARWIN_VM_INHERIT_NONE 2
#define DARWIN_VM_INHERIT_DONATE_COPY 3

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

#define DARWIN_COPYFILE_OVERWRITE 0x0001u
#define DARWIN_COPYFILE_IGNORE_MODE 0x0002u
#define DARWIN_COPYFILE_MASK \
	(DARWIN_COPYFILE_OVERWRITE | DARWIN_COPYFILE_IGNORE_MODE)

#define DARWIN_KAUTH_EXTLOOKUP_REGISTER   0
#define DARWIN_KAUTH_EXTLOOKUP_RESULT     (1 << 0)
#define DARWIN_KAUTH_EXTLOOKUP_WORKER     (1 << 1)
#define DARWIN_KAUTH_EXTLOOKUP_DEREGISTER (1 << 2)
#define DARWIN_KAUTH_GET_CACHE_SIZES      (1 << 3)
#define DARWIN_KAUTH_SET_CACHE_SIZES      (1 << 4)
#define DARWIN_KAUTH_CLEAR_CACHES         (1 << 5)

#define DARWIN_PSELECT_MAX_FDS 1024
#define DARWIN_MFSTYPENAMELEN 16
#define DARWIN_MAXPATHLEN 1024
#define MACHGATE_SEMAPHORE_MAX 64
#define MACHGATE_SEMAPHORE_NAME_MAX 255
#define MACHGATE_SEMAPHORE_VALUE_MAX 0x7fffffffu
#define DARWIN_AIO_TRACKED_REQUESTS 64
#define DARWIN_AIO_ALLDONE 1
#define DARWIN_LIO_NOP 0
#define DARWIN_LIO_READ 1
#define DARWIN_LIO_WRITE 2
#define DARWIN_LIO_NOWAIT 1
#define DARWIN_LIO_WAIT 2
#define DARWIN_DIRENT64_HEADER_SIZE 21
#define DARWIN_DIRENT64_ALIGN 8
#define MACHGATE_GETDENTS_BUFFER_SIZE 8192

#ifndef RENAME_EXCHANGE
#define RENAME_EXCHANGE (1u << 1)
#endif

#define DARWIN_CTL_KERN 1
#define DARWIN_CTL_HW   6

#define DARWIN_KERN_OSTYPE    1
#define DARWIN_KERN_OSRELEASE 2
#define DARWIN_KERN_OSREV     3
#define DARWIN_KERN_VERSION   4
#define DARWIN_KERN_ARGMAX    8
#define DARWIN_KERN_HOSTNAME  10

#define DARWIN_HW_MACHINE      1
#define DARWIN_HW_NCPU         3
#define DARWIN_HW_PHYSMEM      5
#define DARWIN_HW_PAGESIZE     7
#define DARWIN_HW_MACHINE_ARCH 12
#define DARWIN_HW_MEMSIZE      24
#define DARWIN_HW_AVAILCPU     25

#define DARWIN_PROC_FLAG_LP64 0x10u
#define DARWIN_PROC_STATUS_RUN 2u

#define DARWIN_PROC_ALL_PIDS 1
#define DARWIN_PROC_UID_ONLY 4
#define DARWIN_PROC_RUID_ONLY 5

#define DARWIN_PROC_INFO_CALL_LISTPIDS 1
#define DARWIN_PROC_INFO_CALL_PIDINFO  2

#define DARWIN_PROC_PIDTASKALLINFO    2
#define DARWIN_PROC_PIDTBSDINFO       3
#define DARWIN_PROC_PIDTASKINFO       4
#define DARWIN_PROC_PIDTHREADINFO     5
#define DARWIN_PROC_PIDLISTTHREADS    6
#define DARWIN_PROC_PIDPATHINFO       11
#define DARWIN_PROC_PIDT_SHORTBSDINFO 13
#define DARWIN_PROC_PIDTHREADID64INFO 15

#define DARWIN_MAXCOMLEN 16
#define DARWIN_MAXTHREADNAMESIZE 64

struct darwin_fsid {
	int32_t val[2];
};

struct darwin_aiocb {
	int32_t aio_fildes;
	int32_t __pad0;
	int64_t aio_offset;
	uint64_t aio_buf;
	uint64_t aio_nbytes;
	int32_t aio_reqprio;
	int32_t __pad1;
	uint8_t aio_sigevent[32];
	int32_t aio_lio_opcode;
	int32_t __pad2;
};

_Static_assert(sizeof(struct darwin_aiocb) == 80,
	"darwin_aiocb must be 80 bytes");

struct darwin_stat64 {
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

_Static_assert(sizeof(struct darwin_stat64) == 144,
	"darwin_stat64 must be 144 bytes");

struct darwin_statfs64 {
	uint32_t f_bsize;
	int32_t f_iosize;
	uint64_t f_blocks;
	uint64_t f_bfree;
	uint64_t f_bavail;
	uint64_t f_files;
	uint64_t f_ffree;
	struct darwin_fsid f_fsid;
	uint32_t f_owner;
	uint32_t f_type;
	uint32_t f_flags;
	uint32_t f_fssubtype;
	char f_fstypename[DARWIN_MFSTYPENAMELEN];
	char f_mntonname[DARWIN_MAXPATHLEN];
	char f_mntfromname[DARWIN_MAXPATHLEN];
	uint32_t f_flags_ext;
	uint32_t f_reserved[7];
};

_Static_assert(sizeof(struct darwin_statfs64) == 2168,
	"darwin_statfs64 must be 2168 bytes");

struct darwin_attrlist {
	uint16_t bitmapcount;
	uint16_t reserved;
	uint32_t commonattr;
	uint32_t volattr;
	uint32_t dirattr;
	uint32_t fileattr;
	uint32_t forkattr;
};

struct darwin_attribute_set {
	uint32_t commonattr;
	uint32_t volattr;
	uint32_t dirattr;
	uint32_t fileattr;
	uint32_t forkattr;
};

struct darwin_attrreference {
	int32_t attr_dataoffset;
	uint32_t attr_length;
};

struct darwin_attr_context {
	struct syscall_gate_state* state;
	const struct darwin_attrlist* attrlist;
	char* buffer;
	size_t capacity;
	size_t fixed_offset;
	size_t variable_offset;
	struct darwin_attribute_set returned;
};

struct darwin_fssearchblock {
	const struct darwin_attrlist* returnattrs;
	void* returnbuffer;
	size_t returnbuffersize;
	uint64_t maxmatches;
	struct timeval timelimit;
	void* searchparams1;
	size_t sizeofsearchparams1;
	void* searchparams2;
	size_t sizeofsearchparams2;
	struct darwin_attrlist searchattrs;
};

struct darwin_searchfs_context {
	struct syscall_gate_state* state;
	const struct darwin_fssearchblock* searchblock;
	char search_name[NAME_MAX + 1];
	uint32_t options;
	uint64_t matches;
	size_t buffer_offset;
	int stopped;
};

struct darwin_proc_bsdinfo {
	uint32_t pbi_flags;
	uint32_t pbi_status;
	uint32_t pbi_xstatus;
	uint32_t pbi_pid;
	uint32_t pbi_ppid;
	uint32_t pbi_uid;
	uint32_t pbi_gid;
	uint32_t pbi_ruid;
	uint32_t pbi_rgid;
	uint32_t pbi_svuid;
	uint32_t pbi_svgid;
	uint32_t rfu_1;
	char pbi_comm[DARWIN_MAXCOMLEN];
	char pbi_name[2 * DARWIN_MAXCOMLEN];
	uint32_t pbi_nfiles;
	uint32_t pbi_pgid;
	uint32_t pbi_pjobc;
	uint32_t e_tdev;
	uint32_t e_tpgid;
	int32_t pbi_nice;
	uint64_t pbi_start_tvsec;
	uint64_t pbi_start_tvusec;
};

struct darwin_proc_bsdshortinfo {
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

struct darwin_proc_taskinfo {
	uint64_t pti_virtual_size;
	uint64_t pti_resident_size;
	uint64_t pti_total_user;
	uint64_t pti_total_system;
	uint64_t pti_threads_user;
	uint64_t pti_threads_system;
	int32_t pti_policy;
	int32_t pti_faults;
	int32_t pti_pageins;
	int32_t pti_cow_faults;
	int32_t pti_messages_sent;
	int32_t pti_messages_received;
	int32_t pti_syscalls_mach;
	int32_t pti_syscalls_unix;
	int32_t pti_csw;
	int32_t pti_threadnum;
	int32_t pti_numrunning;
	int32_t pti_priority;
};

struct darwin_proc_taskallinfo {
	struct darwin_proc_bsdinfo pbsd;
	struct darwin_proc_taskinfo ptinfo;
};

struct darwin_proc_threadinfo {
	uint64_t pth_user_time;
	uint64_t pth_system_time;
	int32_t pth_cpu_usage;
	int32_t pth_policy;
	int32_t pth_run_state;
	int32_t pth_flags;
	int32_t pth_sleep_time;
	int32_t pth_curpri;
	int32_t pth_priority;
	int32_t pth_maxpriority;
	char pth_name[DARWIN_MAXTHREADNAMESIZE];
};

_Static_assert(sizeof(struct darwin_proc_bsdinfo) == 136,
	"darwin_proc_bsdinfo must be 136 bytes");
_Static_assert(sizeof(struct darwin_proc_bsdshortinfo) == 64,
	"darwin_proc_bsdshortinfo must be 64 bytes");
_Static_assert(sizeof(struct darwin_proc_taskinfo) == 96,
	"darwin_proc_taskinfo must be 96 bytes");
_Static_assert(sizeof(struct darwin_proc_taskallinfo) == 232,
	"darwin_proc_taskallinfo must be 232 bytes");
_Static_assert(sizeof(struct darwin_proc_threadinfo) == 112,
	"darwin_proc_threadinfo must be 112 bytes");

struct linux_dirent64_range_200_399 {
	uint64_t d_ino;
	int64_t d_off;
	unsigned short d_reclen;
	unsigned char d_type;
	char d_name[];
};

struct darwin_aio_completion {
	const struct darwin_aiocb* aiocb;
	int error;
	ssize_t result;
	int returned;
};

struct machgate_named_semaphore {
	int used;
	int unlinked;
	unsigned int value;
	unsigned int refs;
	char name[MACHGATE_SEMAPHORE_NAME_MAX + 1];
};

struct darwin_accessx_descriptor {
	uint32_t ad_name_offset;
	int32_t ad_flags;
	int32_t ad_pad[2];
};

struct darwin_auditinfo_addr_range_200_399 {
	uint32_t ai_auid;
	uint32_t ai_mask_success;
	uint32_t ai_mask_failure;
	int32_t ai_termid_port;
	uint32_t ai_termid_type;
	uint32_t ai_termid_addr[4];
	int32_t ai_asid;
	uint64_t ai_flags;
};

struct darwin_mac_label_ref_range_200_399 {
	uint64_t buffer_length;
	uint64_t buffer;
};

struct darwin_kauth_cache_sizes_range_200_399 {
	uint32_t group_size;
	uint32_t id_size;
};

_Static_assert(sizeof(struct darwin_auditinfo_addr_range_200_399) == 48,
	"darwin_auditinfo_addr_range_200_399 must be 48 bytes");

_Static_assert(sizeof(struct darwin_mac_label_ref_range_200_399) == 16,
	"darwin_mac_label_ref_range_200_399 must be 16 bytes");

_Static_assert(sizeof(struct darwin_kauth_cache_sizes_range_200_399) == 8,
	"darwin_kauth_cache_sizes_range_200_399 must be 8 bytes");
_Static_assert(sizeof(struct darwin_accessx_descriptor) == 16,
	"darwin_accessx_descriptor must be 16 bytes");

static struct darwin_aio_completion aio_completions[DARWIN_AIO_TRACKED_REQUESTS];
static struct machgate_named_semaphore __attribute__((unused))
	machgate_named_semaphores[MACHGATE_SEMAPHORE_MAX];
static struct darwin_auditinfo_addr_range_200_399 auditinfo_addr_state;
static int auditinfo_addr_state_initialized;
static int thread_credentials_overridden;
static uid_t thread_credential_uid;
static gid_t thread_credential_gid;

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

static void set_enosys(struct syscall_gate_state* state)
{
	set_failure(state, DARWIN_ENOSYS);
}

static int errno_to_darwin(int err)
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
	case EDEADLK: return 11;
	case ENOMEM: return 12;
	case EACCES: return 13;
	case EFAULT: return 14;
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
	case EAGAIN: return 35;
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
	case EPFNOSUPPORT: return 46;
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
	case ESHUTDOWN: return 58;
	case ETOOMANYREFS: return 59;
	case ETIMEDOUT: return 60;
	case ECONNREFUSED: return 61;
	case ELOOP: return 62;
	case ENAMETOOLONG: return 63;
	case EHOSTDOWN: return 64;
	case EHOSTUNREACH: return 65;
	case ENOTEMPTY: return 66;
	case EDQUOT: return 69;
	case ESTALE: return 70;
#ifdef EREMOTE
	case EREMOTE: return 71;
#endif
	case ENOLCK: return 77;
	case ENOSYS: return DARWIN_ENOSYS;
	case EOVERFLOW: return 84;
	case ECANCELED: return 89;
	case EIDRM: return 90;
	case ENOMSG: return 91;
	case EILSEQ: return 92;
#ifdef EBADMSG
	case EBADMSG: return 94;
#endif
#ifdef EMULTIHOP
	case EMULTIHOP: return 95;
#endif
#ifdef ENODATA
	case ENODATA: return 96;
#endif
#ifdef ENOLINK
	case ENOLINK: return 97;
#endif
#ifdef ENOSR
	case ENOSR: return 98;
#endif
#ifdef ENOSTR
	case ENOSTR: return 99;
#endif
#ifdef EPROTO
	case EPROTO: return 100;
#endif
#ifdef ETIME
	case ETIME: return 101;
#endif
#ifdef ENOTRECOVERABLE
	case ENOTRECOVERABLE: return 104;
#endif
#ifdef EOWNERDEAD
	case EOWNERDEAD: return 105;
#endif
	default: return err;
	}
}

static void finish_syscall_result(struct syscall_gate_state* state, long result)
{
	if (result < 0)
		set_failure(state, errno_to_darwin(errno ? errno : EIO));
	else
		set_success(state, (uint64_t)result);
}

static void handle_sysv_semctl(struct syscall_gate_state* state, uint64_t semid,
                               uint64_t semnum, uint64_t cmd, uint64_t arg)
{
	errno = 0;
	long result = syscall(SYS_semctl, (int)semid, (int)semnum, (int)cmd, arg);
	finish_syscall_result(state, result);
}

static void handle_sysv_semget(struct syscall_gate_state* state, uint64_t key,
                               uint64_t nsems, uint64_t flags)
{
	errno = 0;
	long result = syscall(SYS_semget, (key_t)key, (int)nsems, (int)flags);
	finish_syscall_result(state, result);
}

static void handle_sysv_semop(struct syscall_gate_state* state, uint64_t semid,
                              uint64_t sops, uint64_t nsops)
{
	errno = 0;
	long result = syscall(SYS_semop, (int)semid, (struct sembuf*)sops,
	                      (size_t)nsops);
	finish_syscall_result(state, result);
}

static void __attribute__((unused)) handle_semsys(struct syscall_gate_state* state)
{
	switch (state->x[0]) {
	case 0:
		handle_sysv_semctl(state, state->x[1], state->x[2], state->x[3],
		                   state->x[4]);
		return;
	case 1:
		handle_sysv_semget(state, state->x[1], state->x[2], state->x[3]);
		return;
	case 2:
		handle_sysv_semop(state, state->x[1], state->x[2], state->x[3]);
		return;
	default:
		set_failure(state, DARWIN_EINVAL);
		return;
	}
}

static void handle_sysv_msgctl(struct syscall_gate_state* state, uint64_t msqid,
                               uint64_t cmd, uint64_t buf)
{
	errno = 0;
	long result = syscall(SYS_msgctl, (int)msqid, (int)cmd, (void*)buf);
	finish_syscall_result(state, result);
}

static void handle_sysv_msgget(struct syscall_gate_state* state, uint64_t key,
                               uint64_t flags)
{
	errno = 0;
	long result = syscall(SYS_msgget, (key_t)key, (int)flags);
	finish_syscall_result(state, result);
}

static void handle_sysv_msgsnd(struct syscall_gate_state* state, uint64_t msqid,
                               uint64_t msgp, uint64_t msgsz, uint64_t flags)
{
	errno = 0;
	long result = syscall(SYS_msgsnd, (int)msqid, (const void*)msgp,
	                      (size_t)msgsz, (int)flags);
	finish_syscall_result(state, result);
}

static void handle_sysv_msgrcv(struct syscall_gate_state* state, uint64_t msqid,
                               uint64_t msgp, uint64_t msgsz, uint64_t msgtyp,
                               uint64_t flags)
{
	errno = 0;
	long result = syscall(SYS_msgrcv, (int)msqid, (void*)msgp, (size_t)msgsz,
	                      (long)msgtyp, (int)flags);
	finish_syscall_result(state, result);
}

static void __attribute__((unused)) handle_msgsys(struct syscall_gate_state* state)
{
	switch (state->x[0]) {
	case 0:
		handle_sysv_msgctl(state, state->x[1], state->x[2], state->x[3]);
		return;
	case 1:
		handle_sysv_msgget(state, state->x[1], state->x[2]);
		return;
	case 2:
		handle_sysv_msgsnd(state, state->x[1], state->x[2], state->x[3],
		                   state->x[4]);
		return;
	case 3:
		handle_sysv_msgrcv(state, state->x[1], state->x[2], state->x[3],
		                   state->x[4], state->x[5]);
		return;
	default:
		set_failure(state, DARWIN_EINVAL);
		return;
	}
}

static void handle_sysv_shmat(struct syscall_gate_state* state, uint64_t shmid,
                              uint64_t shmaddr, uint64_t flags)
{
	errno = 0;
	long result = syscall(SYS_shmat, (int)shmid, (void*)shmaddr, (int)flags);
	finish_syscall_result(state, result);
}

static void handle_sysv_shmctl(struct syscall_gate_state* state, uint64_t shmid,
                               uint64_t cmd, uint64_t buf)
{
	errno = 0;
	long result = syscall(SYS_shmctl, (int)shmid, (int)cmd, (void*)buf);
	finish_syscall_result(state, result);
}

static void handle_sysv_shmdt(struct syscall_gate_state* state, uint64_t shmaddr)
{
	errno = 0;
	long result = syscall(SYS_shmdt, (const void*)shmaddr);
	finish_syscall_result(state, result);
}

static void handle_sysv_shmget(struct syscall_gate_state* state, uint64_t key,
                               uint64_t size, uint64_t flags)
{
	errno = 0;
	long result = syscall(SYS_shmget, (key_t)key, (size_t)size, (int)flags);
	finish_syscall_result(state, result);
}

static void __attribute__((unused)) handle_shmsys(struct syscall_gate_state* state)
{
	switch (state->x[0]) {
	case 0:
		handle_sysv_shmat(state, state->x[1], state->x[2], state->x[3]);
		return;
	case 1:
		handle_sysv_shmctl(state, state->x[1], state->x[2], state->x[3]);
		return;
	case 2:
		handle_sysv_shmdt(state, state->x[1]);
		return;
	case 3:
		handle_sysv_shmget(state, state->x[1], state->x[2], state->x[3]);
		return;
	case 4:
		handle_sysv_shmctl(state, state->x[1], state->x[2], state->x[3]);
		return;
	default:
		set_failure(state, DARWIN_EINVAL);
		return;
	}
}

static int translate_accessx_flags(int darwin_flags)
{
	int result = 0;

	if (darwin_flags & (DARWIN_ACCESSX_READ_OK |
	                    DARWIN_ACCESSX_READ_EXT_OK |
	                    DARWIN_ACCESSX_RATTR_OK |
	                    DARWIN_ACCESSX_REXT_OK |
	                    DARWIN_ACCESSX_RPERM_OK))
		result |= R_OK;
	if (darwin_flags & (DARWIN_ACCESSX_WRITE_OK |
	                    DARWIN_ACCESSX_WRITE_EXT_OK |
	                    DARWIN_ACCESSX_DELETE_OK |
	                    DARWIN_ACCESSX_APPEND_OK |
	                    DARWIN_ACCESSX_RMFILE_OK |
	                    DARWIN_ACCESSX_WATTR_OK |
	                    DARWIN_ACCESSX_WEXT_OK |
	                    DARWIN_ACCESSX_WPERM_OK |
	                    DARWIN_ACCESSX_CHOWN_OK))
		result |= W_OK;
	if (darwin_flags & (DARWIN_ACCESSX_EXECUTE_OK |
	                    DARWIN_ACCESSX_EXECUTE_EXT_OK))
		result |= X_OK;

	return result;
}

static void __attribute__((unused)) handle_access_extended(struct syscall_gate_state* state)
{
	const struct darwin_accessx_descriptor* entries =
		(const struct darwin_accessx_descriptor*)state->x[0];
	size_t size = (size_t)state->x[1];
	int32_t* results = (int32_t*)state->x[2];
	const char* base = (const char*)entries;
	size_t desc_max;
	size_t desc_actual;

	if (!entries || !results) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	if (size > DARWIN_ACCESSX_MAX_TABLESIZE) {
		set_failure(state, DARWIN_ENOMEM);
		return;
	}

	if (size < sizeof(struct darwin_accessx_descriptor) + 2) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	desc_max = (size - 2) / sizeof(struct darwin_accessx_descriptor);
	desc_actual = desc_max;

	for (size_t index = 0; index < desc_actual; index++) {
		uint32_t offset = entries[index].ad_name_offset;
		size_t descriptor_index =
			offset / sizeof(struct darwin_accessx_descriptor);

		if (descriptor_index > desc_max ||
		    (descriptor_index != 0 && descriptor_index <= index)) {
			set_failure(state, DARWIN_EINVAL);
			return;
		}

		if (offset >= size) {
			set_failure(state, DARWIN_EINVAL);
			return;
		}

		if (descriptor_index == 0) {
			if (index == 0) {
				set_failure(state, DARWIN_EINVAL);
				return;
			}
			continue;
		}

		if (descriptor_index < desc_actual)
			desc_actual = descriptor_index;
	}

	if (desc_actual > DARWIN_ACCESSX_MAX_DESCRIPTORS) {
		set_failure(state, DARWIN_ENOMEM);
		return;
	}

	const char* current_path = NULL;
	for (size_t index = 0; index < desc_actual; index++) {
		uint32_t offset = entries[index].ad_name_offset;

		if (offset != 0) {
			size_t remaining = size - offset;
			current_path = base + offset;
			if (strnlen(current_path, remaining) == remaining) {
				set_failure(state, DARWIN_EINVAL);
				return;
			}
		}

		if (!current_path) {
			set_failure(state, DARWIN_EINVAL);
			return;
		}

		errno = 0;
		long result = syscall(SYS_faccessat, AT_FDCWD, current_path,
		                      translate_accessx_flags(entries[index].ad_flags),
		                      0);
		results[index] = result < 0 ? errno_to_darwin(errno ? errno : EIO) : 0;
	}

	set_success(state, 0);
}

static void init_auditinfo_addr_state(void)
{
	if (auditinfo_addr_state_initialized)
		return;

	memset(&auditinfo_addr_state, 0, sizeof(auditinfo_addr_state));
	auditinfo_addr_state.ai_auid = (uint32_t)syscall(SYS_getuid);
	auditinfo_addr_state.ai_asid = (int32_t)syscall(SYS_getpid);
	auditinfo_addr_state_initialized = 1;
}

static void handle_audit_record_compat(struct syscall_gate_state* state)
{
	int length = (int)state->x[1];

	if (length < 0) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	set_success(state, 0);
}

static void handle_getauid(struct syscall_gate_state* state)
{
	uint32_t* auid = (uint32_t*)state->x[0];

	if (!auid) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	init_auditinfo_addr_state();
	*auid = auditinfo_addr_state.ai_auid;
	set_success(state, 0);
}

static void handle_setauid(struct syscall_gate_state* state)
{
	const uint32_t* auid = (const uint32_t*)state->x[0];

	if (!auid) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	init_auditinfo_addr_state();
	auditinfo_addr_state.ai_auid = *auid;
	set_success(state, 0);
}

static void handle_getaudit_addr(struct syscall_gate_state* state)
{
	void* auditinfo_addr = (void*)state->x[0];
	int length = (int)state->x[1];

	if (!auditinfo_addr) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}
	if (length < (int)sizeof(auditinfo_addr_state)) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	init_auditinfo_addr_state();
	memcpy(auditinfo_addr, &auditinfo_addr_state, sizeof(auditinfo_addr_state));
	set_success(state, 0);
}

static void handle_setaudit_addr(struct syscall_gate_state* state)
{
	const void* auditinfo_addr = (const void*)state->x[0];
	int length = (int)state->x[1];

	if (!auditinfo_addr) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}
	if (length < (int)sizeof(auditinfo_addr_state)) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	memcpy(&auditinfo_addr_state, auditinfo_addr, sizeof(auditinfo_addr_state));
	auditinfo_addr_state_initialized = 1;
	set_success(state, 0);
}

static void handle_mac_get_label(struct syscall_gate_state* state,
                                 uint64_t mac_argument)
{
	struct darwin_mac_label_ref_range_200_399* mac_label =
		(struct darwin_mac_label_ref_range_200_399*)mac_argument;

	if (!mac_label || !mac_label->buffer || mac_label->buffer_length == 0) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	*(char*)(uintptr_t)mac_label->buffer = '\0';
	set_success(state, 0);
}

static int validate_fd_argument(int fd)
{
	errno = 0;
	return syscall(SYS_fcntl, fd, F_GETFD) >= 0;
}

static int host_cpu_count(void)
{
	long result = sysconf(_SC_NPROCESSORS_ONLN);
	if (result < 1)
		return 1;
	if (result > INT32_MAX)
		return INT32_MAX;
	return (int)result;
}

static uint64_t host_memory_size(void)
{
	long pages = sysconf(_SC_PHYS_PAGES);
	long page_size = sysconf(_SC_PAGESIZE);
	if (pages < 1 || page_size < 1)
		return 0;
	return (uint64_t)pages * (uint64_t)page_size;
}

static int host_page_size(void)
{
	long result = sysconf(_SC_PAGESIZE);
	if (result < 1)
		return 4096;
	if (result > INT32_MAX)
		return INT32_MAX;
	return (int)result;
}

static int sysctl_name_matches(const char* name, size_t name_length,
                               const char* expected)
{
	size_t expected_length = strlen(expected);
	return name && name_length == expected_length &&
	       memcmp(name, expected, expected_length) == 0;
}

static void finish_sysctl_value(struct syscall_gate_state* state,
                                const void* value, size_t value_size,
                                void* oldp, size_t* oldlenp)
{
	if (oldp && !oldlenp) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	if (oldlenp) {
		size_t capacity = *oldlenp;
		*oldlenp = value_size;
		if (oldp) {
			if (capacity < value_size) {
				set_failure(state, DARWIN_ENOMEM);
				return;
			}
			memcpy(oldp, value, value_size);
		}
	}

	set_success(state, 0);
}

static void finish_sysctl_int(struct syscall_gate_state* state, int value,
                              void* oldp, size_t* oldlenp)
{
	finish_sysctl_value(state, &value, sizeof(value), oldp, oldlenp);
}

static void finish_sysctl_u64(struct syscall_gate_state* state, uint64_t value,
                              void* oldp, size_t* oldlenp)
{
	finish_sysctl_value(state, &value, sizeof(value), oldp, oldlenp);
}

static void finish_sysctl_string(struct syscall_gate_state* state,
                                 const char* value, void* oldp,
                                 size_t* oldlenp)
{
	finish_sysctl_value(state, value, strlen(value) + 1, oldp, oldlenp);
}

static int reject_sysctl_write(struct syscall_gate_state* state, void* newp,
                               size_t newlen)
{
	if (!newp && newlen == 0)
		return 0;

	set_failure(state, DARWIN_EINVAL);
	return 1;
}

static void handle_hw_sysctl(struct syscall_gate_state* state, int selector,
                             void* oldp, size_t* oldlenp)
{
	switch (selector) {
	case DARWIN_HW_NCPU:
	case DARWIN_HW_AVAILCPU:
		finish_sysctl_int(state, host_cpu_count(), oldp, oldlenp);
		return;
	case DARWIN_HW_PHYSMEM:
	case DARWIN_HW_MEMSIZE:
		finish_sysctl_u64(state, host_memory_size(), oldp, oldlenp);
		return;
	case DARWIN_HW_PAGESIZE:
		finish_sysctl_int(state, host_page_size(), oldp, oldlenp);
		return;
	case DARWIN_HW_MACHINE:
	case DARWIN_HW_MACHINE_ARCH:
		finish_sysctl_string(state, "arm64", oldp, oldlenp);
		return;
	default:
		set_enosys(state);
		return;
	}
}

static void handle_kern_sysctl(struct syscall_gate_state* state, int selector,
                               void* oldp, size_t* oldlenp)
{
	switch (selector) {
	case DARWIN_KERN_OSTYPE:
		finish_sysctl_string(state, "Darwin", oldp, oldlenp);
		return;
	case DARWIN_KERN_OSRELEASE:
		finish_sysctl_string(state, "23.0.0", oldp, oldlenp);
		return;
	case DARWIN_KERN_OSREV:
		finish_sysctl_int(state, 199506, oldp, oldlenp);
		return;
	case DARWIN_KERN_VERSION:
		finish_sysctl_string(state, "Darwin Kernel Version 23.0.0: MachGate",
		                     oldp, oldlenp);
		return;
	case DARWIN_KERN_ARGMAX:
		finish_sysctl_int(state, 262144, oldp, oldlenp);
		return;
	case DARWIN_KERN_HOSTNAME:
		finish_sysctl_string(state, "machgate", oldp, oldlenp);
		return;
	default:
		set_enosys(state);
		return;
	}
}

static void handle_sysctl(struct syscall_gate_state* state)
{
	const int* name = (const int*)state->x[0];
	uint32_t name_length = (uint32_t)state->x[1];
	void* oldp = (void*)state->x[2];
	size_t* oldlenp = (size_t*)state->x[3];
	void* newp = (void*)state->x[4];
	size_t newlen = (size_t)state->x[5];

	if (reject_sysctl_write(state, newp, newlen))
		return;
	if (!name || name_length < 2) {
		set_failure(state, !name ? DARWIN_EFAULT : DARWIN_EINVAL);
		return;
	}

	switch (name[0]) {
	case DARWIN_CTL_HW:
		handle_hw_sysctl(state, name[1], oldp, oldlenp);
		return;
	case DARWIN_CTL_KERN:
		handle_kern_sysctl(state, name[1], oldp, oldlenp);
		return;
	default:
		set_enosys(state);
		return;
	}
}

static void handle_sysctlbyname(struct syscall_gate_state* state)
{
	const char* name = (const char*)state->x[0];
	size_t name_length = (size_t)state->x[1];
	void* oldp = (void*)state->x[2];
	size_t* oldlenp = (size_t*)state->x[3];
	void* newp = (void*)state->x[4];
	size_t newlen = (size_t)state->x[5];

	if (reject_sysctl_write(state, newp, newlen))
		return;
	if (!name) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	if (sysctl_name_matches(name, name_length, "hw.ncpu") ||
	    sysctl_name_matches(name, name_length, "hw.activecpu") ||
	    sysctl_name_matches(name, name_length, "hw.logicalcpu") ||
	    sysctl_name_matches(name, name_length, "hw.logicalcpu_max") ||
	    sysctl_name_matches(name, name_length, "hw.physicalcpu") ||
	    sysctl_name_matches(name, name_length, "hw.physicalcpu_max") ||
	    sysctl_name_matches(name, name_length, "hw.availcpu") ||
	    sysctl_name_matches(name, name_length, "machdep.cpu.core_count") ||
	    sysctl_name_matches(name, name_length, "machdep.cpu.thread_count")) {
		finish_sysctl_int(state, host_cpu_count(), oldp, oldlenp);
		return;
	}

	if (sysctl_name_matches(name, name_length, "hw.memsize") ||
	    sysctl_name_matches(name, name_length, "hw.physmem")) {
		finish_sysctl_u64(state, host_memory_size(), oldp, oldlenp);
		return;
	}

	if (sysctl_name_matches(name, name_length, "hw.pagesize")) {
		finish_sysctl_int(state, host_page_size(), oldp, oldlenp);
		return;
	}

	if (sysctl_name_matches(name, name_length, "hw.machine") ||
	    sysctl_name_matches(name, name_length, "hw.machinearch")) {
		finish_sysctl_string(state, "arm64", oldp, oldlenp);
		return;
	}

	if (sysctl_name_matches(name, name_length, "kern.ostype")) {
		finish_sysctl_string(state, "Darwin", oldp, oldlenp);
		return;
	}

	if (sysctl_name_matches(name, name_length, "kern.osrelease")) {
		finish_sysctl_string(state, "23.0.0", oldp, oldlenp);
		return;
	}

	if (sysctl_name_matches(name, name_length, "kern.version")) {
		finish_sysctl_string(state, "Darwin Kernel Version 23.0.0: MachGate",
		                     oldp, oldlenp);
		return;
	}

	if (sysctl_name_matches(name, name_length, "kern.argmax")) {
		finish_sysctl_int(state, 262144, oldp, oldlenp);
		return;
	}

	if (sysctl_name_matches(name, name_length, "kern.hostname")) {
		finish_sysctl_string(state, "machgate", oldp, oldlenp);
		return;
	}

	set_enosys(state);
}

static uid_t current_thread_uid(void)
{
	if (thread_credentials_overridden)
		return thread_credential_uid;
	return (uid_t)syscall(SYS_getuid);
}

static gid_t current_thread_gid(void)
{
	if (thread_credentials_overridden)
		return thread_credential_gid;
	return (gid_t)syscall(SYS_getgid);
}

static uint64_t timeval_to_microseconds(const struct timeval* value)
{
	return (uint64_t)value->tv_sec * 1000000u + (uint64_t)value->tv_usec;
}

static void read_proc_comm(char* buffer, size_t buffer_size)
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

static uint64_t read_statm_field(int index)
{
	char data[128];
	int fd = (int)syscall(SYS_openat, AT_FDCWD, "/proc/self/statm",
	                      O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return 0;

	ssize_t result = (ssize_t)syscall(SYS_read, fd, data, sizeof(data) - 1);
	syscall(SYS_close, fd);
	if (result <= 0)
		return 0;

	data[result] = '\0';
	char* cursor = data;
	for (int i = 0; i < index; i++) {
		while (*cursor && *cursor != ' ')
			cursor++;
		while (*cursor == ' ')
			cursor++;
	}

	if (!*cursor)
		return 0;

	return strtoull(cursor, NULL, 10) * (uint64_t)host_page_size();
}

static void fill_proc_bsdshortinfo(struct darwin_proc_bsdshortinfo* info,
                                   pid_t pid)
{
	memset(info, 0, sizeof(*info));
	info->pbsi_pid = (uint32_t)pid;
	info->pbsi_ppid = (uint32_t)syscall(SYS_getppid);
	info->pbsi_pgid = (uint32_t)syscall(SYS_getpgid, 0);
	info->pbsi_status = DARWIN_PROC_STATUS_RUN;
	info->pbsi_flags = DARWIN_PROC_FLAG_LP64;
	info->pbsi_uid = (uint32_t)current_thread_uid();
	info->pbsi_gid = (uint32_t)current_thread_gid();
	info->pbsi_ruid = (uint32_t)syscall(SYS_getuid);
	info->pbsi_rgid = (uint32_t)syscall(SYS_getgid);
	info->pbsi_svuid = info->pbsi_uid;
	info->pbsi_svgid = info->pbsi_gid;
	read_proc_comm(info->pbsi_comm, sizeof(info->pbsi_comm));
}

static void fill_proc_bsdinfo(struct darwin_proc_bsdinfo* info, pid_t pid)
{
	struct timespec now;
	memset(info, 0, sizeof(*info));
	info->pbi_flags = DARWIN_PROC_FLAG_LP64;
	info->pbi_status = DARWIN_PROC_STATUS_RUN;
	info->pbi_pid = (uint32_t)pid;
	info->pbi_ppid = (uint32_t)syscall(SYS_getppid);
	info->pbi_uid = (uint32_t)current_thread_uid();
	info->pbi_gid = (uint32_t)current_thread_gid();
	info->pbi_ruid = (uint32_t)syscall(SYS_getuid);
	info->pbi_rgid = (uint32_t)syscall(SYS_getgid);
	info->pbi_svuid = info->pbi_uid;
	info->pbi_svgid = info->pbi_gid;
	info->pbi_pgid = (uint32_t)syscall(SYS_getpgid, 0);
	info->pbi_pjobc = 1;
	read_proc_comm(info->pbi_comm, sizeof(info->pbi_comm));
	memcpy(info->pbi_name, info->pbi_comm, sizeof(info->pbi_comm));
	if (syscall(SYS_clock_gettime, CLOCK_MONOTONIC, &now) == 0) {
		info->pbi_start_tvsec = (uint64_t)now.tv_sec;
		info->pbi_start_tvusec = (uint64_t)(now.tv_nsec / 1000);
	}
}

static void fill_proc_taskinfo(struct darwin_proc_taskinfo* info)
{
	struct rusage usage;
	memset(info, 0, sizeof(*info));
	info->pti_virtual_size = read_statm_field(0);
	info->pti_resident_size = read_statm_field(1);
	if (syscall(SYS_getrusage, RUSAGE_SELF, &usage) == 0) {
		info->pti_total_user = timeval_to_microseconds(&usage.ru_utime);
		info->pti_total_system = timeval_to_microseconds(&usage.ru_stime);
		info->pti_threads_user = info->pti_total_user;
		info->pti_threads_system = info->pti_total_system;
		info->pti_faults = (int32_t)usage.ru_minflt;
		info->pti_pageins = (int32_t)usage.ru_majflt;
		info->pti_csw = (int32_t)(usage.ru_nvcsw + usage.ru_nivcsw);
	}
	info->pti_threadnum = 1;
	info->pti_numrunning = 1;
}

static void fill_proc_threadinfo(struct darwin_proc_threadinfo* info)
{
	struct rusage usage;
	memset(info, 0, sizeof(*info));
	if (syscall(SYS_getrusage, RUSAGE_THREAD, &usage) == 0) {
		info->pth_user_time = timeval_to_microseconds(&usage.ru_utime);
		info->pth_system_time = timeval_to_microseconds(&usage.ru_stime);
	}
	info->pth_run_state = DARWIN_PROC_STATUS_RUN;
	info->pth_curpri = 31;
	info->pth_priority = 31;
	info->pth_maxpriority = 63;
	read_proc_comm(info->pth_name, sizeof(info->pth_name));
}

static int proc_pid_is_supported(pid_t pid)
{
	pid_t self = (pid_t)syscall(SYS_getpid);
	return pid == 0 || pid == self;
}

static void finish_proc_copy(struct syscall_gate_state* state, void* buffer,
                             int32_t buffer_size, const void* value,
                             size_t value_size)
{
	if (!buffer) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}
	if (buffer_size < 0 || (size_t)buffer_size < value_size) {
		set_failure(state, DARWIN_ENOMEM);
		return;
	}

	memcpy(buffer, value, value_size);
	set_success(state, value_size);
}

static void handle_proc_listpids(struct syscall_gate_state* state,
                                 int32_t type, uint64_t typeinfo,
                                 void* buffer, int32_t buffer_size)
{
	pid_t self = (pid_t)syscall(SYS_getpid);

	if (type != DARWIN_PROC_ALL_PIDS && type != DARWIN_PROC_UID_ONLY &&
	    type != DARWIN_PROC_RUID_ONLY) {
		set_enosys(state);
		return;
	}

	if ((type == DARWIN_PROC_UID_ONLY || type == DARWIN_PROC_RUID_ONLY) &&
	    (uid_t)typeinfo != (uid_t)syscall(SYS_getuid)) {
		set_success(state, 0);
		return;
	}

	if (!buffer || buffer_size < (int32_t)sizeof(int32_t)) {
		set_success(state, sizeof(int32_t));
		return;
	}

	*(int32_t*)buffer = (int32_t)self;
	set_success(state, sizeof(int32_t));
}

static void handle_proc_pidinfo(struct syscall_gate_state* state, pid_t pid,
                                uint32_t flavor, uint64_t arg, void* buffer,
                                int32_t buffer_size)
{
	pid_t self = (pid_t)syscall(SYS_getpid);

	if (!proc_pid_is_supported(pid)) {
		set_success(state, 0);
		return;
	}
	if (pid == 0)
		pid = self;

	switch (flavor) {
	case DARWIN_PROC_PIDT_SHORTBSDINFO: {
		struct darwin_proc_bsdshortinfo info;
		fill_proc_bsdshortinfo(&info, pid);
		finish_proc_copy(state, buffer, buffer_size, &info, sizeof(info));
		return;
	}
	case DARWIN_PROC_PIDTBSDINFO: {
		struct darwin_proc_bsdinfo info;
		fill_proc_bsdinfo(&info, pid);
		finish_proc_copy(state, buffer, buffer_size, &info, sizeof(info));
		return;
	}
	case DARWIN_PROC_PIDTASKINFO: {
		struct darwin_proc_taskinfo info;
		fill_proc_taskinfo(&info);
		finish_proc_copy(state, buffer, buffer_size, &info, sizeof(info));
		return;
	}
	case DARWIN_PROC_PIDTASKALLINFO: {
		struct darwin_proc_taskallinfo info;
		fill_proc_bsdinfo(&info.pbsd, pid);
		fill_proc_taskinfo(&info.ptinfo);
		finish_proc_copy(state, buffer, buffer_size, &info, sizeof(info));
		return;
	}
	case DARWIN_PROC_PIDTHREADINFO:
	case DARWIN_PROC_PIDTHREADID64INFO: {
		struct darwin_proc_threadinfo info;
		(void)arg;
		fill_proc_threadinfo(&info);
		finish_proc_copy(state, buffer, buffer_size, &info, sizeof(info));
		return;
	}
	case DARWIN_PROC_PIDLISTTHREADS: {
		uint32_t thread_id[2];
		uint64_t tid = (uint64_t)syscall(SYS_gettid);
		thread_id[0] = (uint32_t)(tid >> 32);
		thread_id[1] = (uint32_t)tid;
		finish_proc_copy(state, buffer, buffer_size, thread_id,
		                 sizeof(thread_id));
		return;
	}
	case DARWIN_PROC_PIDPATHINFO: {
		char path[DARWIN_MAXPATHLEN];
		memset(path, 0, sizeof(path));
		if (pid == (pid_t)syscall(SYS_getpid) &&
		    __machgate_guest_executable_path &&
		    *__machgate_guest_executable_path) {
			strncpy(path, __machgate_guest_executable_path,
			        sizeof(path) - 1);
		} else {
			ssize_t result = (ssize_t)syscall(SYS_readlinkat, AT_FDCWD,
			                                  "/proc/self/exe", path,
			                                  sizeof(path) - 1);
			if (result < 0) {
				set_failure(state, errno_to_darwin(errno ? errno : EIO));
				return;
			}
		}
		finish_proc_copy(state, buffer, buffer_size, path, sizeof(path));
		return;
	}
	default:
		set_enosys(state);
		return;
	}
}

static void handle_proc_info(struct syscall_gate_state* state)
{
	int32_t callnum = (int32_t)state->x[0];
	int32_t pid = (int32_t)state->x[1];
	uint32_t flavor = (uint32_t)state->x[2];
	uint64_t arg = state->x[3];
	void* buffer = (void*)state->x[4];
	int32_t buffer_size = (int32_t)state->x[5];

	switch (callnum) {
	case DARWIN_PROC_INFO_CALL_LISTPIDS:
		handle_proc_listpids(state, pid, flavor, buffer, buffer_size);
		return;
	case DARWIN_PROC_INFO_CALL_PIDINFO:
		handle_proc_pidinfo(state, (pid_t)pid, flavor, arg, buffer,
		                    buffer_size);
		return;
	default:
		set_enosys(state);
		return;
	}
}

static void handle_sys_settid(struct syscall_gate_state* state)
{
	thread_credential_uid = (uid_t)state->x[0];
	thread_credential_gid = (gid_t)state->x[1];
	thread_credentials_overridden = 1;
	set_success(state, 0);
}

static void handle_gettid(struct syscall_gate_state* state)
{
	if (state->x[0])
		*(uid_t*)state->x[0] = current_thread_uid();
	if (state->x[1])
		*(gid_t*)state->x[1] = current_thread_gid();
	set_success(state, 0);
}

static void handle_empty_uuid_group_get(struct syscall_gate_state* state)
{
	uint32_t* count = (uint32_t*)state->x[0];

	if (!count) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	*count = 0;
	set_success(state, 0);
}

static void handle_empty_uuid_group_set(struct syscall_gate_state* state)
{
	if (state->x[0] != 0) {
		set_enosys(state);
		return;
	}

	set_success(state, 0);
}

static void handle_sys_settid_with_pid(struct syscall_gate_state* state)
{
	pid_t pid = (pid_t)state->x[0];
	pid_t self = (pid_t)syscall(SYS_getpid);

	if (pid != 0 && pid != self) {
		set_enosys(state);
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

static uint32_t linux_sigset_to_darwin(const sigset_t* linux_set)
{
	uint32_t result = 0;

	for (int linux_signal = 1; linux_signal < NSIG; linux_signal++) {
		if (sigismember(linux_set, linux_signal) != 1)
			continue;
		int darwin_signal = linux_signal_to_darwin(linux_signal);
		if (darwin_signal > 0)
			result |= 1u << (darwin_signal - 1);
	}

	return result;
}

static int darwin_sigprocmask_how_to_linux(int darwin_how)
{
	switch (darwin_how) {
	case DARWIN_SIG_BLOCK: return SIG_BLOCK;
	case DARWIN_SIG_UNBLOCK: return SIG_UNBLOCK;
	case DARWIN_SIG_SETMASK: return SIG_SETMASK;
	default: return -1;
	}
}

static void handle_pthread_kill(struct syscall_gate_state* state)
{
	int linux_signal = darwin_signal_to_linux((int)state->x[1]);
	pid_t target_tid = (pid_t)state->x[0];
	pid_t current_tid = (pid_t)syscall(SYS_gettid);

	if (linux_signal < 0) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	if (target_tid == 0)
		target_tid = current_tid;

	errno = 0;
	finish_syscall_result(state, syscall(SYS_tgkill, syscall(SYS_getpid),
	                                     target_tid, linux_signal));
}

static void handle_pthread_sigmask(struct syscall_gate_state* state)
{
	int linux_how = darwin_sigprocmask_how_to_linux((int)state->x[0]);
	const uint32_t* darwin_set = (const uint32_t*)state->x[1];
	uint32_t* darwin_old_set = (uint32_t*)state->x[2];
	sigset_t linux_set;
	sigset_t linux_old_set;
	sigset_t* linux_set_ptr = NULL;
	sigset_t* linux_old_set_ptr = darwin_old_set ? &linux_old_set : NULL;

	if (linux_how < 0 && darwin_set) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	if (darwin_set) {
		if (!darwin_sigset_to_linux(*darwin_set, &linux_set)) {
			set_failure(state, DARWIN_EINVAL);
			return;
		}
		linux_set_ptr = &linux_set;
	}

	errno = 0;
	if (sigprocmask(linux_how < 0 ? SIG_SETMASK : linux_how, linux_set_ptr,
	                linux_old_set_ptr) < 0) {
		set_failure(state, errno_to_darwin(errno ? errno : EIO));
		return;
	}

	if (darwin_old_set)
		*darwin_old_set = linux_sigset_to_darwin(&linux_old_set);

	set_success(state, 0);
}

static void handle_sigwait(struct syscall_gate_state* state)
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
		set_failure(state, errno_to_darwin(result));
		return;
	}

	*darwin_signal_out = (uint32_t)linux_signal_to_darwin(linux_signal);
	set_success(state, 0);
}

static struct darwin_aio_completion*
find_aio_completion(const struct darwin_aiocb* aiocb)
{
	for (size_t index = 0; index < DARWIN_AIO_TRACKED_REQUESTS; index++) {
		if (aio_completions[index].aiocb == aiocb)
			return &aio_completions[index];
	}

	return NULL;
}

static struct darwin_aio_completion*
allocate_aio_completion(const struct darwin_aiocb* aiocb)
{
	struct darwin_aio_completion* reusable = NULL;

	for (size_t index = 0; index < DARWIN_AIO_TRACKED_REQUESTS; index++) {
		if (aio_completions[index].aiocb == aiocb)
			return &aio_completions[index];
		if (!reusable && (!aio_completions[index].aiocb ||
		                  aio_completions[index].returned))
			reusable = &aio_completions[index];
	}

	if (!reusable) {
		errno = ENOMEM;
		return NULL;
	}

	reusable->aiocb = aiocb;
	reusable->error = 0;
	reusable->result = 0;
	reusable->returned = 0;
	return reusable;
}

static int complete_aio_operation(const struct darwin_aiocb* aiocb,
                                  int operation)
{
	struct darwin_aio_completion* completion = allocate_aio_completion(aiocb);
	long result;

	if (!completion)
		return -1;

	if (aiocb->aio_reqprio != 0) {
		completion->error = EINVAL;
		completion->result = -1;
		completion->returned = 0;
		errno = EINVAL;
		return -1;
	}

	errno = 0;
	switch (operation) {
	case DARWIN_LIO_READ:
		result = syscall(SYS_pread64, aiocb->aio_fildes,
		                 (void*)aiocb->aio_buf, (size_t)aiocb->aio_nbytes,
		                 (off_t)aiocb->aio_offset);
		break;
	case DARWIN_LIO_WRITE:
		result = syscall(SYS_pwrite64, aiocb->aio_fildes,
		                 (const void*)aiocb->aio_buf,
		                 (size_t)aiocb->aio_nbytes, (off_t)aiocb->aio_offset);
		break;
	default:
		errno = EINVAL;
		result = -1;
		break;
	}

	completion->returned = 0;
	if (result < 0) {
		completion->error = errno ? errno : EIO;
		completion->result = -1;
		return -1;
	}

	completion->error = 0;
	completion->result = (ssize_t)result;
	return 0;
}

static void record_aio_fsync(struct syscall_gate_state* state)
{
	struct darwin_aiocb* aiocb = (struct darwin_aiocb*)state->x[1];
	struct darwin_aio_completion* completion;

	if (!aiocb) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	completion = allocate_aio_completion(aiocb);
	if (!completion) {
		set_failure(state, errno_to_darwin(errno ? errno : EINVAL));
		return;
	}

	errno = 0;
	long result = syscall(SYS_fsync, aiocb->aio_fildes);
	completion->returned = 0;
	if (result < 0) {
		completion->error = errno ? errno : EIO;
		completion->result = -1;
		set_failure(state, errno_to_darwin(completion->error));
		return;
	}

	completion->error = 0;
	completion->result = 0;
	set_success(state, 0);
}

static void handle_aio_return(struct syscall_gate_state* state)
{
	struct darwin_aio_completion* completion =
		find_aio_completion((const struct darwin_aiocb*)state->x[0]);

	if (!completion || completion->returned) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	completion->returned = 1;
	if (completion->error) {
		set_failure(state, errno_to_darwin(completion->error));
		return;
	}

	set_success(state, (uint64_t)completion->result);
}

static void handle_aio_error(struct syscall_gate_state* state)
{
	struct darwin_aio_completion* completion =
		find_aio_completion((const struct darwin_aiocb*)state->x[0]);

	if (!completion || completion->returned) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	set_success(state, (uint64_t)errno_to_darwin(completion->error));
}

static void handle_aio_suspend(struct syscall_gate_state* state)
{
	const struct darwin_aiocb* const* list =
		(const struct darwin_aiocb* const*)state->x[0];
	int count = (int)state->x[1];

	if (!list || count < 0) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	for (int index = 0; index < count; index++) {
		const struct darwin_aiocb* aiocb = list[index];
		struct darwin_aio_completion* completion;

		if (!aiocb)
			continue;

		completion = find_aio_completion(aiocb);
		if (completion && !completion->returned) {
			set_success(state, 0);
			return;
		}
	}

	set_failure(state, DARWIN_EINVAL);
}

static void handle_aio_cancel(struct syscall_gate_state* state)
{
	struct darwin_aiocb* aiocb = (struct darwin_aiocb*)state->x[1];

	if (aiocb) {
		struct darwin_aio_completion* completion = find_aio_completion(aiocb);
		if (!completion) {
			set_failure(state, DARWIN_EINVAL);
			return;
		}
		set_success(state, DARWIN_AIO_ALLDONE);
		return;
	}

	set_success(state, DARWIN_AIO_ALLDONE);
}

static void handle_aio_transfer(struct syscall_gate_state* state, int operation)
{
	struct darwin_aiocb* aiocb = (struct darwin_aiocb*)state->x[0];

	if (!aiocb) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	if (complete_aio_operation(aiocb, operation) < 0) {
		set_failure(state, errno_to_darwin(errno ? errno : EIO));
		return;
	}

	set_success(state, 0);
}

static void handle_lio_listio(struct syscall_gate_state* state)
{
	int mode = (int)state->x[0];
	struct darwin_aiocb** list = (struct darwin_aiocb**)state->x[1];
	int count = (int)state->x[2];
	void* signal_event = (void*)state->x[3];

	if ((mode != DARWIN_LIO_WAIT && mode != DARWIN_LIO_NOWAIT) ||
	    !list || count < 0 || count > DARWIN_AIO_TRACKED_REQUESTS) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	if (mode == DARWIN_LIO_NOWAIT && signal_event) {
		set_enosys(state);
		return;
	}

	for (int index = 0; index < count; index++) {
		struct darwin_aiocb* aiocb = list[index];

		if (!aiocb || aiocb->aio_lio_opcode == DARWIN_LIO_NOP)
			continue;

		if (aiocb->aio_lio_opcode != DARWIN_LIO_READ &&
		    aiocb->aio_lio_opcode != DARWIN_LIO_WRITE) {
			set_failure(state, DARWIN_EINVAL);
			return;
		}

		if (complete_aio_operation(aiocb, aiocb->aio_lio_opcode) < 0) {
			set_failure(state, errno_to_darwin(errno ? errno : EIO));
			return;
		}
	}

	set_success(state, 0);
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
	if (darwin_flags & DARWIN_O_CLOEXEC) linux_flags |= O_CLOEXEC;

	return linux_flags;
}

static void linux_to_darwin_stat64(const struct stat* linux_stat,
                                   struct darwin_stat64* darwin_stat)
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

static void copy_bounded_string(char* dest, size_t dest_size, const char* src)
{
	if (!src)
		src = "";

	snprintf(dest, dest_size, "%s", src);
}

static uint32_t linux_mount_flags_to_darwin(uint64_t linux_flags)
{
	uint32_t darwin_flags = DARWIN_MNT_LOCAL;

#ifdef ST_RDONLY
	if (linux_flags & ST_RDONLY)
		darwin_flags |= DARWIN_MNT_RDONLY;
#endif
#ifdef ST_NOEXEC
	if (linux_flags & ST_NOEXEC)
		darwin_flags |= DARWIN_MNT_NOEXEC;
#endif
#ifdef ST_NOSUID
	if (linux_flags & ST_NOSUID)
		darwin_flags |= DARWIN_MNT_NOSUID;
#endif
#ifdef ST_NODEV
	if (linux_flags & ST_NODEV)
		darwin_flags |= DARWIN_MNT_NODEV;
#endif

	return darwin_flags;
}

static void linux_to_darwin_statfs64(const struct statfs* linux_statfs,
                                     struct darwin_statfs64* darwin_statfs,
                                     const char* fstype,
                                     const char* mntonname,
                                     const char* mntfromname)
{
	memset(darwin_statfs, 0, sizeof(*darwin_statfs));
	darwin_statfs->f_bsize = (uint32_t)linux_statfs->f_bsize;
	darwin_statfs->f_iosize = (int32_t)linux_statfs->f_bsize;
	darwin_statfs->f_blocks = linux_statfs->f_blocks;
	darwin_statfs->f_bfree = linux_statfs->f_bfree;
	darwin_statfs->f_bavail = linux_statfs->f_bavail;
	darwin_statfs->f_files = linux_statfs->f_files;
	darwin_statfs->f_ffree = linux_statfs->f_ffree;
	darwin_statfs->f_owner = (uint32_t)syscall(SYS_getuid);
	darwin_statfs->f_type = (uint32_t)linux_statfs->f_type;
	darwin_statfs->f_flags = linux_mount_flags_to_darwin(linux_statfs->f_flags);
	copy_bounded_string(darwin_statfs->f_fstypename,
	                    sizeof(darwin_statfs->f_fstypename), fstype);
	copy_bounded_string(darwin_statfs->f_mntonname,
	                    sizeof(darwin_statfs->f_mntonname), mntonname);
	copy_bounded_string(darwin_statfs->f_mntfromname,
	                    sizeof(darwin_statfs->f_mntfromname), mntfromname);
}

static void fill_statfs_mount_names(const char* path,
                                    const struct statfs* linux_statfs,
                                    struct darwin_statfs64* darwin_statfs)
{
	FILE* mounts = setmntent("/proc/self/mounts", "r");
	struct mntent* entry;
	const char* best_type = "";
	const char* best_dir = path ? path : "";
	const char* best_source = "";
	size_t best_len = 0;
	char best_type_buf[DARWIN_MFSTYPENAMELEN] = "";
	char best_dir_buf[DARWIN_MAXPATHLEN] = "";
	char best_source_buf[DARWIN_MAXPATHLEN] = "";

	if (!mounts) {
		linux_to_darwin_statfs64(linux_statfs, darwin_statfs, "", best_dir, "");
		return;
	}

	while ((entry = getmntent(mounts)) != NULL) {
		size_t len = strlen(entry->mnt_dir);
		if (!path || strncmp(path, entry->mnt_dir, len) != 0)
			continue;
		if (len > 1 && path[len] != '\0' && path[len] != '/')
			continue;
		if (len < best_len)
			continue;

		best_len = len;
		copy_bounded_string(best_type_buf, sizeof(best_type_buf), entry->mnt_type);
		copy_bounded_string(best_dir_buf, sizeof(best_dir_buf), entry->mnt_dir);
		copy_bounded_string(best_source_buf, sizeof(best_source_buf), entry->mnt_fsname);
		best_type = best_type_buf;
		best_dir = best_dir_buf;
		best_source = best_source_buf;
	}

	endmntent(mounts);
	linux_to_darwin_statfs64(linux_statfs, darwin_statfs, best_type, best_dir,
	                         best_source);
}

static void finish_statfs64_result(struct syscall_gate_state* state,
                                   long result,
                                   const struct statfs* linux_statfs,
                                   const char* path)
{
	if (result < 0) {
		set_failure(state, errno_to_darwin(errno ? errno : EIO));
		return;
	}

	fill_statfs_mount_names(path, linux_statfs,
	                        (struct darwin_statfs64*)state->x[1]);
	set_success(state, 0);
}

static void finish_stat64_result(struct syscall_gate_state* state,
                                 long result,
                                 const struct stat* linux_stat)
{
	if (result < 0) {
		set_failure(state, errno_to_darwin(errno ? errno : EIO));
		return;
	}

	linux_to_darwin_stat64(linux_stat, (struct darwin_stat64*)state->x[1]);
	set_success(state, 0);
}

static void finish_extended_stat64_result(struct syscall_gate_state* state,
                                          long result,
                                          const struct stat* linux_stat,
                                          uint64_t darwin_stat_addr,
                                          uint64_t xsecurity_size_addr)
{
	if (result < 0) {
		set_failure(state, errno_to_darwin(errno ? errno : EIO));
		return;
	}

	linux_to_darwin_stat64(linux_stat, (struct darwin_stat64*)darwin_stat_addr);
	if (xsecurity_size_addr)
		*(uint64_t*)xsecurity_size_addr = 0;
	set_success(state, 0);
}

static void finish_created_node(struct syscall_gate_state* state,
                                long result,
                                const char* path,
                                uid_t uid,
                                gid_t gid)
{
	if (result < 0) {
		set_failure(state, errno_to_darwin(errno ? errno : EIO));
		return;
	}

	if (uid == (uid_t)-1 && gid == (gid_t)-1) {
		set_success(state, 0);
		return;
	}

	errno = 0;
	result = syscall(SYS_fchownat, AT_FDCWD, path, uid, gid, 0);
	finish_syscall_result(state, result);
}

static void finish_open_extended_result(struct syscall_gate_state* state,
                                        long result,
                                        uid_t uid,
                                        gid_t gid)
{
	if (result < 0) {
		set_failure(state, errno_to_darwin(errno ? errno : EIO));
		return;
	}

	if (uid == (uid_t)-1 && gid == (gid_t)-1) {
		set_success(state, (uint64_t)result);
		return;
	}

	errno = 0;
	long chown_result = syscall(SYS_fchown, (int)result, uid, gid);
	if (chown_result < 0) {
		int saved_errno = errno ? errno : EIO;
		syscall(SYS_close, (int)result);
		set_failure(state, errno_to_darwin(saved_errno));
		return;
	}

	set_success(state, (uint64_t)result);
}

static size_t align_attr_offset(size_t value)
{
	return (value + 3u) & ~(size_t)3u;
}

static const char* path_basename(const char* path)
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

static int darwin_attr_validate(struct syscall_gate_state* state,
                                const struct darwin_attrlist* attrlist,
                                uint32_t options, int for_set)
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

static size_t darwin_attr_fixed_size(const struct darwin_attrlist* attrlist)
{
	size_t result = sizeof(uint32_t);

	if (attrlist->commonattr & DARWIN_ATTR_CMN_RETURNED_ATTRS)
		result += sizeof(struct darwin_attribute_set);
	if (attrlist->commonattr & DARWIN_ATTR_CMN_NAME)
		result += sizeof(struct darwin_attrreference);
	if (attrlist->commonattr & DARWIN_ATTR_CMN_DEVID)
		result += sizeof(int32_t);
	if (attrlist->commonattr & DARWIN_ATTR_CMN_FSID)
		result += sizeof(struct darwin_fsid);
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
		result += sizeof(struct darwin_attrreference);
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

	return align_attr_offset(result);
}

static int darwin_attr_write_bytes(struct darwin_attr_context* context,
                                   const void* value, size_t size)
{
	if (context->fixed_offset + size > context->capacity) {
		set_failure(context->state, DARWIN_ENOMEM);
		return 0;
	}

	memcpy(context->buffer + context->fixed_offset, value, size);
	context->fixed_offset = align_attr_offset(context->fixed_offset + size);
	return 1;
}

static int darwin_attr_write_u32(struct darwin_attr_context* context,
                                 uint32_t value)
{
	return darwin_attr_write_bytes(context, &value, sizeof(value));
}

static int darwin_attr_write_u64(struct darwin_attr_context* context,
                                 uint64_t value)
{
	return darwin_attr_write_bytes(context, &value, sizeof(value));
}

static int darwin_attr_write_ref(struct darwin_attr_context* context,
                                 const char* value)
{
	struct darwin_attrreference reference;
	size_t length = strlen(value) + 1;

	context->variable_offset = align_attr_offset(context->variable_offset);
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
		align_attr_offset(context->fixed_offset + sizeof(reference));
	context->variable_offset = align_attr_offset(context->variable_offset + length);
	return 1;
}

static uint32_t darwin_file_type(mode_t mode)
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

static uint32_t darwin_user_access(const struct stat* linux_stat)
{
	uint32_t result = 0;
	uid_t uid = current_thread_uid();
	gid_t gid = current_thread_gid();
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

static uint32_t count_directory_entries_at(int dirfd, const char* name)
{
	int fd = (int)syscall(SYS_openat, dirfd, name,
	                      O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	char buffer[MACHGATE_GETDENTS_BUFFER_SIZE];
	uint32_t result = 0;

	if (fd < 0)
		return 0;

	for (;;) {
		long read_result = syscall(SYS_getdents64, fd, buffer,
		                           sizeof(buffer));
		if (read_result <= 0)
			break;

		for (long offset = 0; offset < read_result;) {
			const struct linux_dirent64_range_200_399* entry =
				(const struct linux_dirent64_range_200_399*)(buffer + offset);
			if (strcmp(entry->d_name, ".") != 0 &&
			    strcmp(entry->d_name, "..") != 0)
				result++;
			offset += entry->d_reclen;
		}
	}

	syscall(SYS_close, fd);
	return result;
}

static int darwin_attr_write_stat(struct darwin_attr_context* context,
                                  const struct stat* linux_stat,
                                  const char* name,
                                  const char* full_path,
                                  int dirfd)
{
	struct darwin_fsid fsid;
	struct darwin_attribute_set returned;
	uint64_t allocated_size = (uint64_t)linux_stat->st_blocks * 512u;

	memset(&returned, 0, sizeof(returned));
	returned.commonattr = context->attrlist->commonattr;
	returned.dirattr = context->attrlist->dirattr;
	returned.fileattr = context->attrlist->fileattr;
	context->returned = returned;

	if (context->attrlist->commonattr & DARWIN_ATTR_CMN_RETURNED_ATTRS &&
	    !darwin_attr_write_bytes(context, &returned, sizeof(returned)))
		return 0;
	if (context->attrlist->commonattr & DARWIN_ATTR_CMN_NAME &&
	    !darwin_attr_write_ref(context, name))
		return 0;
	if (context->attrlist->commonattr & DARWIN_ATTR_CMN_DEVID &&
	    !darwin_attr_write_u32(context, (uint32_t)linux_stat->st_dev))
		return 0;
	if (context->attrlist->commonattr & DARWIN_ATTR_CMN_FSID) {
		fsid.val[0] = (int32_t)linux_stat->st_dev;
		fsid.val[1] = 0;
		if (!darwin_attr_write_bytes(context, &fsid, sizeof(fsid)))
			return 0;
	}
	if (context->attrlist->commonattr & DARWIN_ATTR_CMN_OBJTYPE &&
	    !darwin_attr_write_u32(context, darwin_file_type(linux_stat->st_mode)))
		return 0;
	if (context->attrlist->commonattr & DARWIN_ATTR_CMN_OBJTAG &&
	    !darwin_attr_write_u32(context, 0))
		return 0;
	if (context->attrlist->commonattr & DARWIN_ATTR_CMN_CRTIME &&
	    !darwin_attr_write_bytes(context, &linux_stat->st_ctim,
	                             sizeof(linux_stat->st_ctim)))
		return 0;
	if (context->attrlist->commonattr & DARWIN_ATTR_CMN_MODTIME &&
	    !darwin_attr_write_bytes(context, &linux_stat->st_mtim,
	                             sizeof(linux_stat->st_mtim)))
		return 0;
	if (context->attrlist->commonattr & DARWIN_ATTR_CMN_CHGTIME &&
	    !darwin_attr_write_bytes(context, &linux_stat->st_ctim,
	                             sizeof(linux_stat->st_ctim)))
		return 0;
	if (context->attrlist->commonattr & DARWIN_ATTR_CMN_ACCTIME &&
	    !darwin_attr_write_bytes(context, &linux_stat->st_atim,
	                             sizeof(linux_stat->st_atim)))
		return 0;
	if (context->attrlist->commonattr & DARWIN_ATTR_CMN_OWNERID &&
	    !darwin_attr_write_u32(context, (uint32_t)linux_stat->st_uid))
		return 0;
	if (context->attrlist->commonattr & DARWIN_ATTR_CMN_GRPID &&
	    !darwin_attr_write_u32(context, (uint32_t)linux_stat->st_gid))
		return 0;
	if (context->attrlist->commonattr & DARWIN_ATTR_CMN_ACCESSMASK &&
	    !darwin_attr_write_u32(context, (uint32_t)(linux_stat->st_mode & 07777)))
		return 0;
	if (context->attrlist->commonattr & DARWIN_ATTR_CMN_FLAGS &&
	    !darwin_attr_write_u32(context, 0))
		return 0;
	if (context->attrlist->commonattr & DARWIN_ATTR_CMN_USERACCESS &&
	    !darwin_attr_write_u32(context, darwin_user_access(linux_stat)))
		return 0;
	if (context->attrlist->commonattr & DARWIN_ATTR_CMN_FILEID &&
	    !darwin_attr_write_u64(context, (uint64_t)linux_stat->st_ino))
		return 0;
	if (context->attrlist->commonattr & DARWIN_ATTR_CMN_FULLPATH &&
	    !darwin_attr_write_ref(context, full_path ? full_path : name))
		return 0;
	if (context->attrlist->dirattr & DARWIN_ATTR_DIR_LINKCOUNT &&
	    !darwin_attr_write_u32(context, (uint32_t)linux_stat->st_nlink))
		return 0;
	if (context->attrlist->dirattr & DARWIN_ATTR_DIR_ENTRYCOUNT &&
	    !darwin_attr_write_u32(context, count_directory_entries_at(dirfd, name)))
		return 0;
	if (context->attrlist->dirattr & DARWIN_ATTR_DIR_IOBLOCKSIZE &&
	    !darwin_attr_write_u32(context, (uint32_t)linux_stat->st_blksize))
		return 0;
	if (context->attrlist->dirattr & DARWIN_ATTR_DIR_DATALENGTH &&
	    !darwin_attr_write_u64(context, (uint64_t)linux_stat->st_size))
		return 0;
	if (context->attrlist->fileattr & DARWIN_ATTR_FILE_LINKCOUNT &&
	    !darwin_attr_write_u32(context, (uint32_t)linux_stat->st_nlink))
		return 0;
	if (context->attrlist->fileattr & DARWIN_ATTR_FILE_TOTALSIZE &&
	    !darwin_attr_write_u64(context, (uint64_t)linux_stat->st_size))
		return 0;
	if (context->attrlist->fileattr & DARWIN_ATTR_FILE_ALLOCSIZE &&
	    !darwin_attr_write_u64(context, allocated_size))
		return 0;
	if (context->attrlist->fileattr & DARWIN_ATTR_FILE_IOBLOCKSIZE &&
	    !darwin_attr_write_u32(context, (uint32_t)linux_stat->st_blksize))
		return 0;
	if (context->attrlist->fileattr & DARWIN_ATTR_FILE_DATALENGTH &&
	    !darwin_attr_write_u64(context, (uint64_t)linux_stat->st_size))
		return 0;
	if (context->attrlist->fileattr & DARWIN_ATTR_FILE_DATAALLOCSIZE &&
	    !darwin_attr_write_u64(context, allocated_size))
		return 0;

	return 1;
}

static int build_absolute_path(const char* path, char* buffer, size_t buffer_size)
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

static void finish_getattrlist_result(struct syscall_gate_state* state,
                                      const struct darwin_attrlist* attrlist,
                                      char* buffer, size_t buffer_size,
                                      const struct stat* linux_stat,
                                      const char* name,
                                      const char* full_path,
                                      int dirfd)
{
	struct darwin_attr_context context;
	size_t fixed_size = darwin_attr_fixed_size(attrlist);
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

	if (!darwin_attr_write_stat(&context, linux_stat, name, full_path, dirfd))
		return;

	final_size = (uint32_t)context.variable_offset;
	memcpy(buffer, &final_size, sizeof(final_size));
	set_success(state, 0);
}

static void handle_getattrlist_path(struct syscall_gate_state* state)
{
	const char* path = (const char*)state->x[0];
	const struct darwin_attrlist* attrlist =
		(const struct darwin_attrlist*)state->x[1];
	char* buffer = (char*)state->x[2];
	size_t buffer_size = (size_t)state->x[3];
	uint32_t options = (uint32_t)state->x[4];
	struct stat linux_stat;
	char full_path[4096];
	int flags = (options & DARWIN_FSOPT_NOFOLLOW) ? AT_SYMLINK_NOFOLLOW : 0;

	if (!darwin_attr_validate(state, attrlist, options, 0))
		return;

	errno = 0;
	long result = syscall(SYS_newfstatat, AT_FDCWD, path, &linux_stat, flags);
	if (result < 0) {
		set_failure(state, errno_to_darwin(errno ? errno : EIO));
		return;
	}

	if (!build_absolute_path(path, full_path, sizeof(full_path)))
		snprintf(full_path, sizeof(full_path), "%s", path ? path : "");

	finish_getattrlist_result(state, attrlist, buffer, buffer_size,
	                          &linux_stat, path_basename(path), full_path,
	                          AT_FDCWD);
}

static void handle_fgetattrlist(struct syscall_gate_state* state)
{
	int fd = (int)state->x[0];
	const struct darwin_attrlist* attrlist =
		(const struct darwin_attrlist*)state->x[1];
	char* buffer = (char*)state->x[2];
	size_t buffer_size = (size_t)state->x[3];
	uint32_t options = (uint32_t)state->x[4];
	struct stat linux_stat;
	char proc_path[64];
	char full_path[4096];
	ssize_t path_size;

	if (!darwin_attr_validate(state, attrlist, options, 0))
		return;

	errno = 0;
	long result = syscall(SYS_fstat, fd, &linux_stat);
	if (result < 0) {
		set_failure(state, errno_to_darwin(errno ? errno : EIO));
		return;
	}

	snprintf(proc_path, sizeof(proc_path), "/proc/self/fd/%d", fd);
	errno = 0;
	path_size = (ssize_t)syscall(SYS_readlinkat, AT_FDCWD, proc_path,
	                             full_path, sizeof(full_path) - 1);
	if (path_size < 0) {
		snprintf(full_path, sizeof(full_path), "fd/%d", fd);
	} else {
		full_path[path_size] = '\0';
	}

	finish_getattrlist_result(state, attrlist, buffer, buffer_size,
	                          &linux_stat, path_basename(full_path),
	                          full_path, AT_FDCWD);
}

static const char* consume_setattr_timespec(const char* cursor,
                                            struct timespec* value)
{
	memcpy(value, cursor, sizeof(*value));
	return cursor + align_attr_offset(sizeof(*value));
}

static void apply_setattr_result(struct syscall_gate_state* state,
                                 long result)
{
	if (result < 0)
		set_failure(state, errno_to_darwin(errno ? errno : EIO));
	else
		set_success(state, 0);
}

static void handle_setattrlist_path(struct syscall_gate_state* state)
{
	const char* path = (const char*)state->x[0];
	const struct darwin_attrlist* attrlist =
		(const struct darwin_attrlist*)state->x[1];
	const char* cursor = (const char*)state->x[2];
	uint32_t options = (uint32_t)state->x[4];
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
	if (!darwin_attr_validate(state, attrlist, options, 1))
		return;

	times[0].tv_nsec = UTIME_OMIT;
	times[1].tv_nsec = UTIME_OMIT;
	if (attrlist->commonattr & DARWIN_ATTR_CMN_MODTIME) {
		cursor = consume_setattr_timespec(cursor, &times[1]);
		set_mtime = 1;
	}
	if (attrlist->commonattr & DARWIN_ATTR_CMN_ACCTIME) {
		cursor = consume_setattr_timespec(cursor, &times[0]);
		set_atime = 1;
	}
	if (attrlist->commonattr & DARWIN_ATTR_CMN_OWNERID) {
		memcpy(&uid, cursor, sizeof(uint32_t));
		cursor += align_attr_offset(sizeof(uint32_t));
	}
	if (attrlist->commonattr & DARWIN_ATTR_CMN_GRPID) {
		memcpy(&gid, cursor, sizeof(uint32_t));
		cursor += align_attr_offset(sizeof(uint32_t));
	}
	if (attrlist->commonattr & DARWIN_ATTR_CMN_ACCESSMASK) {
		uint32_t access_mode;
		memcpy(&access_mode, cursor, sizeof(access_mode));
		mode = (mode_t)access_mode;
	}

	if (uid != (uid_t)-1 || gid != (gid_t)-1) {
		errno = 0;
		long result = syscall(SYS_fchownat, AT_FDCWD, path, uid, gid,
		                      (options & DARWIN_FSOPT_NOFOLLOW) ?
		                      AT_SYMLINK_NOFOLLOW : 0);
		if (result < 0) {
			apply_setattr_result(state, result);
			return;
		}
	}

	if (mode != (mode_t)-1) {
		errno = 0;
		long result = syscall(SYS_fchmodat, AT_FDCWD, path, mode, 0);
		if (result < 0) {
			apply_setattr_result(state, result);
			return;
		}
	}

	if (set_atime || set_mtime) {
		errno = 0;
		apply_setattr_result(state,
		                     syscall(SYS_utimensat, AT_FDCWD, path, times,
		                             (options & DARWIN_FSOPT_NOFOLLOW) ?
		                             AT_SYMLINK_NOFOLLOW : 0));
		return;
	}

	set_success(state, 0);
}

static void handle_fsetattrlist(struct syscall_gate_state* state)
{
	int fd = (int)state->x[0];
	const struct darwin_attrlist* attrlist =
		(const struct darwin_attrlist*)state->x[1];
	const char* cursor = (const char*)state->x[2];
	uint32_t options = (uint32_t)state->x[4];
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
	if (!darwin_attr_validate(state, attrlist, options, 1))
		return;

	times[0].tv_nsec = UTIME_OMIT;
	times[1].tv_nsec = UTIME_OMIT;
	if (attrlist->commonattr & DARWIN_ATTR_CMN_MODTIME) {
		cursor = consume_setattr_timespec(cursor, &times[1]);
		set_mtime = 1;
	}
	if (attrlist->commonattr & DARWIN_ATTR_CMN_ACCTIME) {
		cursor = consume_setattr_timespec(cursor, &times[0]);
		set_atime = 1;
	}
	if (attrlist->commonattr & DARWIN_ATTR_CMN_OWNERID) {
		memcpy(&uid, cursor, sizeof(uint32_t));
		cursor += align_attr_offset(sizeof(uint32_t));
	}
	if (attrlist->commonattr & DARWIN_ATTR_CMN_GRPID) {
		memcpy(&gid, cursor, sizeof(uint32_t));
		cursor += align_attr_offset(sizeof(uint32_t));
	}
	if (attrlist->commonattr & DARWIN_ATTR_CMN_ACCESSMASK) {
		uint32_t access_mode;
		memcpy(&access_mode, cursor, sizeof(access_mode));
		mode = (mode_t)access_mode;
	}

	if (uid != (uid_t)-1 || gid != (gid_t)-1) {
		errno = 0;
		long result = syscall(SYS_fchown, fd, uid, gid);
		if (result < 0) {
			apply_setattr_result(state, result);
			return;
		}
	}

	if (mode != (mode_t)-1) {
		errno = 0;
		long result = syscall(SYS_fchmod, fd, mode);
		if (result < 0) {
			apply_setattr_result(state, result);
			return;
		}
	}

	if (set_atime || set_mtime) {
		errno = 0;
		apply_setattr_result(state, syscall(SYS_utimensat, fd, "", times,
		                                    AT_EMPTY_PATH));
		return;
	}

	set_success(state, 0);
}

static void handle_getdirentriesattr(struct syscall_gate_state* state)
{
	int fd = (int)state->x[0];
	const struct darwin_attrlist* attrlist =
		(const struct darwin_attrlist*)state->x[1];
	char* buffer = (char*)state->x[2];
	size_t buffer_size = (size_t)state->x[3];
	uint32_t* count_ptr = (uint32_t*)state->x[4];
	uint64_t* base_ptr = (uint64_t*)state->x[5];
	uint64_t* newstate_ptr = (uint64_t*)state->x[6];
	uint32_t options = (uint32_t)state->x[7];
	char linux_buffer[MACHGATE_GETDENTS_BUFFER_SIZE];
	uint32_t max_count = count_ptr && *count_ptr ? *count_ptr : UINT32_MAX;
	uint32_t count = 0;
	size_t offset = 0;
	int64_t last_position = 0;

	if (!buffer && buffer_size) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}
	if (!darwin_attr_validate(state, attrlist, options, 0))
		return;

	errno = 0;
	long result = syscall(SYS_getdents64, fd, linux_buffer,
	                      sizeof(linux_buffer));
	if (result < 0) {
		set_failure(state, errno_to_darwin(errno ? errno : EIO));
		return;
	}

	for (long linux_offset = 0; linux_offset < result && count < max_count;) {
		const struct linux_dirent64_range_200_399* entry =
			(const struct linux_dirent64_range_200_399*)(linux_buffer +
			                                             linux_offset);
		struct stat linux_stat;
		struct darwin_attr_context context;
		size_t fixed_size = darwin_attr_fixed_size(attrlist);
		uint32_t entry_size;

		linux_offset += entry->d_reclen;
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
			continue;

		errno = 0;
		if (syscall(SYS_newfstatat, fd, entry->d_name, &linux_stat,
		            AT_SYMLINK_NOFOLLOW) < 0)
			continue;

		if (offset + fixed_size > buffer_size)
			break;

		memset(&context, 0, sizeof(context));
		context.state = state;
		context.attrlist = attrlist;
		context.buffer = buffer + offset;
		context.capacity = buffer_size - offset;
		context.fixed_offset = sizeof(uint32_t);
		context.variable_offset = fixed_size;

		if (!darwin_attr_write_stat(&context, &linux_stat, entry->d_name,
		                            entry->d_name, fd))
			return;

		entry_size = (uint32_t)context.variable_offset;
		memcpy(buffer + offset, &entry_size, sizeof(entry_size));
		offset += align_attr_offset(entry_size);
		last_position = entry->d_off;
		count++;
	}

	if (count_ptr)
		*count_ptr = count;
	if (base_ptr)
		*base_ptr = (uint64_t)last_position;
	if (newstate_ptr)
		*newstate_ptr = 0;
	set_success(state, 0);
}

static void handle_copyfile(struct syscall_gate_state* state)
{
	const char* from = (const char*)state->x[0];
	const char* to = (const char*)state->x[1];
	mode_t mode = (mode_t)state->x[2];
	uint32_t flags = (uint32_t)state->x[3];
	int in_fd;
	int out_fd;
	int out_flags = O_WRONLY | O_CREAT | O_CLOEXEC;
	char buffer[16384];

	if (flags & ~DARWIN_COPYFILE_MASK) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	if (flags & DARWIN_COPYFILE_OVERWRITE)
		out_flags |= O_TRUNC;
	else
		out_flags |= O_EXCL;

	errno = 0;
	in_fd = (int)syscall(SYS_openat, AT_FDCWD, from, O_RDONLY | O_CLOEXEC);
	if (in_fd < 0) {
		set_failure(state, errno_to_darwin(errno ? errno : EIO));
		return;
	}

	errno = 0;
	out_fd = (int)syscall(SYS_openat, AT_FDCWD, to, out_flags, mode);
	if (out_fd < 0) {
		int saved_errno = errno ? errno : EIO;
		syscall(SYS_close, in_fd);
		set_failure(state, errno_to_darwin(saved_errno));
		return;
	}

	for (;;) {
		errno = 0;
		long read_result = syscall(SYS_read, in_fd, buffer, sizeof(buffer));
		if (read_result < 0) {
			int saved_errno = errno ? errno : EIO;
			syscall(SYS_close, in_fd);
			syscall(SYS_close, out_fd);
			set_failure(state, errno_to_darwin(saved_errno));
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
				int saved_errno = errno ? errno : EIO;
				syscall(SYS_close, in_fd);
				syscall(SYS_close, out_fd);
				set_failure(state, errno_to_darwin(saved_errno));
				return;
			}
			cursor += write_result;
			remaining -= write_result;
		}
	}

	syscall(SYS_close, in_fd);
	errno = 0;
	long close_result = syscall(SYS_close, out_fd);
	finish_syscall_result(state, close_result);
}

static int fsgetpath_match(const struct stat* linux_stat,
                           const struct darwin_fsid* fsid,
                           uint64_t objid)
{
	return (int32_t)linux_stat->st_dev == fsid->val[0] &&
	       linux_stat->st_ino == objid;
}

static int fsgetpath_search_dir(int dirfd, const char* display_path,
                                const struct darwin_fsid* fsid,
                                uint64_t objid, char* result,
                                size_t result_size, int depth)
{
	char linux_buffer[MACHGATE_GETDENTS_BUFFER_SIZE];

	if (depth > 8)
		return 0;

	for (;;) {
		long read_result = syscall(SYS_getdents64, dirfd, linux_buffer,
		                           sizeof(linux_buffer));
		if (read_result <= 0)
			return 0;

		for (long offset = 0; offset < read_result;) {
			const struct linux_dirent64_range_200_399* entry =
				(const struct linux_dirent64_range_200_399*)(linux_buffer +
				                                             offset);
			struct stat linux_stat;
			char child_path[4096];

			offset += entry->d_reclen;
			if (strcmp(entry->d_name, ".") == 0 ||
			    strcmp(entry->d_name, "..") == 0)
				continue;

			if (display_path[0] && strcmp(display_path, ".") != 0)
				snprintf(child_path, sizeof(child_path), "%s/%s", display_path,
				         entry->d_name);
			else
				snprintf(child_path, sizeof(child_path), "%s", entry->d_name);

			errno = 0;
			if (syscall(SYS_newfstatat, dirfd, entry->d_name, &linux_stat,
			            AT_SYMLINK_NOFOLLOW) < 0)
				continue;

			if (fsgetpath_match(&linux_stat, fsid, objid)) {
				if (!build_absolute_path(child_path, result, result_size))
					snprintf(result, result_size, "%s", child_path);
				return 1;
			}

			if (S_ISDIR(linux_stat.st_mode)) {
				int child_fd = (int)syscall(SYS_openat, dirfd, entry->d_name,
				                            O_RDONLY | O_DIRECTORY |
				                            O_CLOEXEC | O_NOFOLLOW);
				if (child_fd < 0)
					continue;
				int found = fsgetpath_search_dir(child_fd, child_path, fsid,
				                                 objid, result, result_size,
				                                 depth + 1);
				syscall(SYS_close, child_fd);
				if (found)
					return 1;
			}
		}
	}
}

static void handle_fsgetpath_ext(struct syscall_gate_state* state)
{
	char* buffer = (char*)state->x[0];
	size_t buffer_size = (size_t)state->x[1];
	const struct darwin_fsid* fsid = (const struct darwin_fsid*)state->x[2];
	uint64_t objid = state->x[3];
	uint32_t options = (uint32_t)state->x[4];
	char path[4096];
	int fd;

	if (!buffer || !fsid) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}
	if (buffer_size == 0 || buffer_size > sizeof(path) ||
	    (options & ~(DARWIN_FSOPT_NOFIRMLINKPATH | DARWIN_FSOPT_ISREALFSID))) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	fd = (int)syscall(SYS_openat, AT_FDCWD, ".",
	                  O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (fd < 0) {
		set_failure(state, errno_to_darwin(errno ? errno : EIO));
		return;
	}

	memset(path, 0, sizeof(path));
	if (!fsgetpath_search_dir(fd, ".", fsid, objid, path, sizeof(path), 0)) {
		syscall(SYS_close, fd);
		set_failure(state, errno_to_darwin(ENOENT));
		return;
	}
	syscall(SYS_close, fd);

	size_t length = strlen(path) + 1;
	if (length > buffer_size) {
		set_failure(state, DARWIN_ENOMEM);
		return;
	}

	memcpy(buffer, path, length);
	set_success(state, length);
}

static void handle_undelete(struct syscall_gate_state* state)
{
	const char* path = (const char*)state->x[0];
	struct stat linux_stat;

	if (!path) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	errno = 0;
	long result = syscall(SYS_newfstatat, AT_FDCWD, path, &linux_stat,
	                      AT_SYMLINK_NOFOLLOW);
	if (result == 0) {
		set_failure(state, errno_to_darwin(EEXIST));
		return;
	}

	set_failure(state, errno_to_darwin(errno ? errno : ENOENT));
}

static void handle_exchangedata(struct syscall_gate_state* state)
{
	const char* path1 = (const char*)state->x[0];
	const char* path2 = (const char*)state->x[1];
	uint32_t options = (uint32_t)state->x[2];
	struct stat stat1;
	struct stat stat2;
	int stat_flags = 0;

	if (!path1 || !path2) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}
	if (options & ~DARWIN_FSOPT_NOFOLLOW) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}
	if (options & DARWIN_FSOPT_NOFOLLOW)
		stat_flags = AT_SYMLINK_NOFOLLOW;

	errno = 0;
	if (syscall(SYS_newfstatat, AT_FDCWD, path1, &stat1, stat_flags) < 0) {
		set_failure(state, errno_to_darwin(errno ? errno : EIO));
		return;
	}
	errno = 0;
	if (syscall(SYS_newfstatat, AT_FDCWD, path2, &stat2, stat_flags) < 0) {
		set_failure(state, errno_to_darwin(errno ? errno : EIO));
		return;
	}
	if (stat1.st_dev == stat2.st_dev && stat1.st_ino == stat2.st_ino) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}
	if (!S_ISREG(stat1.st_mode) || !S_ISREG(stat2.st_mode)) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

#ifdef SYS_renameat2
	errno = 0;
	long result = syscall(SYS_renameat2, AT_FDCWD, path1, AT_FDCWD, path2,
	                      RENAME_EXCHANGE);
	if (result < 0 && (errno == ENOSYS || errno == EINVAL || errno == ENOTSUP)) {
		set_failure(state, DARWIN_ENOTSUP);
		return;
	}
	finish_syscall_result(state, result);
#else
	set_failure(state, DARWIN_ENOTSUP);
#endif
}

static int searchfs_read_name_param(struct syscall_gate_state* state,
                                    const struct darwin_fssearchblock* searchblock,
                                    char* name, size_t name_size)
{
	const char* params = (const char*)searchblock->searchparams1;
	uint32_t buffer_length;
	struct darwin_attrreference reference;
	size_t string_offset;
	size_t string_length;

	if (!params) {
		set_failure(state, DARWIN_EFAULT);
		return 0;
	}
	if (searchblock->sizeofsearchparams1 > DARWIN_SEARCHFS_MAX_SEARCHPARMS ||
	    searchblock->sizeofsearchparams2 > DARWIN_SEARCHFS_MAX_SEARCHPARMS ||
	    searchblock->sizeofsearchparams1 <
	    sizeof(uint32_t) + sizeof(reference)) {
		set_failure(state, DARWIN_EINVAL);
		return 0;
	}

	memcpy(&buffer_length, params, sizeof(buffer_length));
	memcpy(&reference, params + sizeof(buffer_length), sizeof(reference));
	if (buffer_length > searchblock->sizeofsearchparams1 ||
	    reference.attr_dataoffset < 0) {
		set_failure(state, DARWIN_EINVAL);
		return 0;
	}

	string_offset = sizeof(buffer_length) + (size_t)reference.attr_dataoffset;
	string_length = reference.attr_length;
	if (string_length == 0 || string_offset > searchblock->sizeofsearchparams1 ||
	    string_length > searchblock->sizeofsearchparams1 - string_offset ||
	    string_length > name_size) {
		set_failure(state, DARWIN_EINVAL);
		return 0;
	}

	memcpy(name, params + string_offset, string_length);
	name[name_size - 1] = '\0';
	return 1;
}

static int searchfs_validate(struct syscall_gate_state* state,
                             const struct darwin_fssearchblock* searchblock,
                             uint32_t options)
{
	if (!searchblock) {
		set_failure(state, DARWIN_EFAULT);
		return 0;
	}
	if (!darwin_attr_validate(state, searchblock->returnattrs, 0, 0))
		return 0;
	if (searchblock->searchattrs.bitmapcount != DARWIN_ATTR_BIT_MAP_COUNT ||
	    searchblock->searchattrs.reserved != 0 ||
	    searchblock->searchattrs.commonattr != DARWIN_ATTR_CMN_NAME ||
	    searchblock->searchattrs.volattr || searchblock->searchattrs.dirattr ||
	    searchblock->searchattrs.fileattr || searchblock->searchattrs.forkattr) {
		set_failure(state, DARWIN_EINVAL);
		return 0;
	}
	if (options & ~DARWIN_SRCHFS_VALIDOPTIONSMASK) {
		set_failure(state, DARWIN_EINVAL);
		return 0;
	}
	if (!(options & DARWIN_SRCHFS_START)) {
		set_failure(state, errno_to_darwin(EBUSY));
		return 0;
	}
	if (!(options & (DARWIN_SRCHFS_MATCHDIRS | DARWIN_SRCHFS_MATCHFILES))) {
		set_failure(state, DARWIN_EINVAL);
		return 0;
	}
	if (!searchblock->returnbuffer && searchblock->returnbuffersize) {
		set_failure(state, DARWIN_EFAULT);
		return 0;
	}
	return 1;
}

static int searchfs_name_matches(const struct darwin_searchfs_context* context,
                                 const char* name)
{
	int result;

	if (context->options & DARWIN_SRCHFS_MATCHPARTIALNAMES)
		result = strstr(name, context->search_name) != NULL;
	else
		result = strcmp(name, context->search_name) == 0;

	if (context->options & DARWIN_SRCHFS_NEGATEPARAMS)
		result = !result;

	return result;
}

static int searchfs_append_match(struct darwin_searchfs_context* context,
                                 const struct stat* linux_stat,
                                 const char* name, int dirfd)
{
	struct darwin_attr_context attr_context;
	size_t fixed_size = darwin_attr_fixed_size(context->searchblock->returnattrs);
	uint32_t entry_size;

	if (context->matches >= context->searchblock->maxmatches) {
		context->stopped = 1;
		return 0;
	}
	if (context->buffer_offset + fixed_size >
	    context->searchblock->returnbuffersize) {
		set_failure(context->state, errno_to_darwin(ENOBUFS));
		context->stopped = 1;
		return 0;
	}

	memset(&attr_context, 0, sizeof(attr_context));
	attr_context.state = context->state;
	attr_context.attrlist = context->searchblock->returnattrs;
	attr_context.buffer = (char*)context->searchblock->returnbuffer +
	                      context->buffer_offset;
	attr_context.capacity = context->searchblock->returnbuffersize -
	                        context->buffer_offset;
	attr_context.fixed_offset = sizeof(uint32_t);
	attr_context.variable_offset = fixed_size;

	if (!darwin_attr_write_stat(&attr_context, linux_stat, name, name, dirfd)) {
		context->stopped = 1;
		return 0;
	}

	entry_size = (uint32_t)attr_context.variable_offset;
	memcpy(attr_context.buffer, &entry_size, sizeof(entry_size));
	context->buffer_offset += align_attr_offset(entry_size);
	context->matches++;
	return 1;
}

static void searchfs_scan_dir(struct darwin_searchfs_context* context,
                              int dirfd, int depth)
{
	char linux_buffer[MACHGATE_GETDENTS_BUFFER_SIZE];

	if (depth > 8 || context->stopped)
		return;

	for (;;) {
		errno = 0;
		long read_result = syscall(SYS_getdents64, dirfd, linux_buffer,
		                           sizeof(linux_buffer));
		if (read_result <= 0)
			return;

		for (long offset = 0; offset < read_result;) {
			const struct linux_dirent64_range_200_399* entry =
				(const struct linux_dirent64_range_200_399*)(linux_buffer +
				                                             offset);
			struct stat linux_stat;
			int is_directory;
			int type_matches;

			offset += entry->d_reclen;
			if (strcmp(entry->d_name, ".") == 0 ||
			    strcmp(entry->d_name, "..") == 0)
				continue;

			errno = 0;
			if (syscall(SYS_newfstatat, dirfd, entry->d_name, &linux_stat,
			            AT_SYMLINK_NOFOLLOW) < 0)
				continue;

			is_directory = S_ISDIR(linux_stat.st_mode);
			type_matches = is_directory ?
				!!(context->options & DARWIN_SRCHFS_MATCHDIRS) :
				!!(context->options & DARWIN_SRCHFS_MATCHFILES);
			if (type_matches && searchfs_name_matches(context, entry->d_name) &&
			    !searchfs_append_match(context, &linux_stat, entry->d_name,
			                           dirfd))
				return;

			if (is_directory) {
				int child_fd = (int)syscall(SYS_openat, dirfd, entry->d_name,
				                            O_RDONLY | O_DIRECTORY |
				                            O_CLOEXEC | O_NOFOLLOW);
				if (child_fd < 0)
					continue;
				searchfs_scan_dir(context, child_fd, depth + 1);
				syscall(SYS_close, child_fd);
				if (context->stopped)
					return;
			}
		}
	}
}

static void handle_searchfs(struct syscall_gate_state* state)
{
	const char* path = (const char*)state->x[0];
	const struct darwin_fssearchblock* searchblock =
		(const struct darwin_fssearchblock*)state->x[1];
	uint64_t* num_matches = (uint64_t*)state->x[2];
	uint32_t options = (uint32_t)state->x[4];
	struct darwin_searchfs_context context;
	int fd;

	if (!path || !searchblock || !num_matches) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}
	if (!searchfs_validate(state, searchblock, options))
		return;

	memset(&context, 0, sizeof(context));
	context.state = state;
	context.searchblock = searchblock;
	context.options = options;
	if (!searchfs_read_name_param(state, searchblock, context.search_name,
	                              sizeof(context.search_name)))
		return;

	errno = 0;
	fd = (int)syscall(SYS_openat, AT_FDCWD, path,
	                  O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (fd < 0) {
		set_failure(state, errno_to_darwin(errno ? errno : EIO));
		return;
	}

	state->nzcv &= ~SYSCALL_GATE_CARRY;
	searchfs_scan_dir(&context, fd, 0);
	syscall(SYS_close, fd);
	*num_matches = context.matches;
	if (state->nzcv & SYSCALL_GATE_CARRY)
		return;

	set_success(state, 0);
}

static void handle_fhopen(struct syscall_gate_state* state)
{
	const void* file_handle = (const void*)state->x[0];
	int darwin_flags = (int)state->x[1];

	if (!file_handle) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}
	if (darwin_flags & DARWIN_O_CREAT) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}
	if ((uid_t)syscall(SYS_geteuid) != 0) {
		set_failure(state, DARWIN_EPERM);
		return;
	}

	set_failure(state, DARWIN_ENOTSUP);
}

static void handle_minherit(struct syscall_gate_state* state)
{
	uintptr_t address = (uintptr_t)state->x[0];
	uint64_t size = state->x[1];
	uint64_t inheritance = state->x[2];
	uint64_t page_size = (uint64_t)host_page_size();

	if (inheritance > DARWIN_VM_INHERIT_DONATE_COPY) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}
	if (size == 0) {
		set_success(state, 0);
		return;
	}
	if (!address || address % page_size != 0) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	set_success(state, 0);
}

static void handle_fsctl_path(struct syscall_gate_state* state)
{
	const char* path = (const char*)state->x[0];
	unsigned long cmd = (unsigned long)state->x[1];
	void* data = (void*)state->x[2];
	uint32_t options = (uint32_t)state->x[3];
	int open_flags = O_RDONLY | O_CLOEXEC;
	int fd;

	if (options & ~DARWIN_FSOPT_NOFOLLOW) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	if (options & DARWIN_FSOPT_NOFOLLOW)
		open_flags |= O_NOFOLLOW;

	errno = 0;
	fd = (int)syscall(SYS_openat, AT_FDCWD, path, open_flags);
	if (fd < 0) {
		set_failure(state, errno_to_darwin(errno ? errno : EIO));
		return;
	}

	errno = 0;
	long result = syscall(SYS_ioctl, fd, cmd, data);
	int saved_errno = errno;
	syscall(SYS_close, fd);
	if (result < 0)
		set_failure(state, errno_to_darwin(saved_errno ? saved_errno : EIO));
	else
		set_success(state, 0);
}

static void handle_ffsctl(struct syscall_gate_state* state)
{
	errno = 0;
	long result = syscall(SYS_ioctl, (int)state->x[0],
	                      (unsigned long)state->x[1], (void*)state->x[2]);
	if (result < 0)
		set_failure(state, errno_to_darwin(errno ? errno : EIO));
	else
		set_success(state, 0);
}

static int make_posix_shm_path(const char* name, char* path, size_t path_size)
{
	if (!name || name[0] != '/' || name[1] == '\0' || strchr(name + 1, '/')) {
		errno = EINVAL;
		return 0;
	}

	int result = snprintf(path, path_size, "/dev/shm/%s", name + 1);
	if (result < 0 || (size_t)result >= path_size) {
		errno = ENAMETOOLONG;
		return 0;
	}

	return 1;
}

static int valid_posix_name(const char* name)
{
	return name && name[0] == '/' && name[1] != '\0' &&
	       strlen(name) <= MACHGATE_SEMAPHORE_NAME_MAX &&
	       !strchr(name + 1, '/');
}

static struct machgate_named_semaphore*
find_named_semaphore(const char* name)
{
	for (size_t i = 0; i < MACHGATE_SEMAPHORE_MAX; i++) {
		struct machgate_named_semaphore* sem = &machgate_named_semaphores[i];
		if (sem->used && !sem->unlinked && strcmp(sem->name, name) == 0)
			return sem;
	}

	return NULL;
}

static struct machgate_named_semaphore*
find_free_named_semaphore(void)
{
	for (size_t i = 0; i < MACHGATE_SEMAPHORE_MAX; i++) {
		if (!machgate_named_semaphores[i].used)
			return &machgate_named_semaphores[i];
	}

	return NULL;
}

static struct machgate_named_semaphore*
semaphore_from_handle(uint64_t handle)
{
	for (size_t i = 0; i < MACHGATE_SEMAPHORE_MAX; i++) {
		struct machgate_named_semaphore* sem = &machgate_named_semaphores[i];
		if (sem->used && (uint64_t)(uintptr_t)sem == handle)
			return sem;
	}

	return NULL;
}

static void release_named_semaphore(struct machgate_named_semaphore* sem)
{
	if (sem->refs > 0)
		sem->refs--;

	if (sem->refs == 0 && sem->unlinked)
		memset(sem, 0, sizeof(*sem));
}

static int named_semaphore_wait_nonblocking(struct machgate_named_semaphore* sem)
{
	if (sem->value == 0)
		return EAGAIN;

	sem->value--;
	return 0;
}

static int named_semaphore_post(struct machgate_named_semaphore* sem)
{
	if (sem->value == MACHGATE_SEMAPHORE_VALUE_MAX)
		return EOVERFLOW;

	sem->value++;
	return 0;
}

static void __attribute__((unused)) handle_sem_open(struct syscall_gate_state* state)
{
	const char* name = (const char*)state->x[0];
	int flags = (int)state->x[1];
	unsigned int value;
	struct machgate_named_semaphore* sem;

	if (!valid_posix_name(name)) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	sem = find_named_semaphore(name);
	if (sem) {
		if ((flags & DARWIN_O_CREAT) && (flags & DARWIN_O_EXCL)) {
			set_failure(state, errno_to_darwin(EEXIST));
			return;
		}
		sem->refs++;
		set_success(state, (uint64_t)(uintptr_t)sem);
		return;
	}

	if (!(flags & DARWIN_O_CREAT)) {
		set_failure(state, errno_to_darwin(ENOENT));
		return;
	}

	if (state->x[3] > MACHGATE_SEMAPHORE_VALUE_MAX) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}
	value = (unsigned int)state->x[3];

	sem = find_free_named_semaphore();
	if (!sem) {
		set_failure(state, errno_to_darwin(ENOSPC));
		return;
	}

	memset(sem, 0, sizeof(*sem));
	sem->used = 1;
	sem->value = value;
	sem->refs = 1;
	snprintf(sem->name, sizeof(sem->name), "%s", name);
	set_success(state, (uint64_t)(uintptr_t)sem);
}

static void __attribute__((unused)) handle_sem_close(struct syscall_gate_state* state)
{
	struct machgate_named_semaphore* sem = semaphore_from_handle(state->x[0]);

	if (!sem) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	release_named_semaphore(sem);
	set_success(state, 0);
}

static void __attribute__((unused)) handle_sem_unlink(struct syscall_gate_state* state)
{
	const char* name = (const char*)state->x[0];
	struct machgate_named_semaphore* sem;

	if (!valid_posix_name(name)) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	sem = find_named_semaphore(name);
	if (!sem) {
		set_failure(state, errno_to_darwin(ENOENT));
		return;
	}

	sem->unlinked = 1;
	if (sem->refs == 0)
		memset(sem, 0, sizeof(*sem));
	set_success(state, 0);
}

static void __attribute__((unused)) handle_sem_wait(struct syscall_gate_state* state)
{
	struct machgate_named_semaphore* sem = semaphore_from_handle(state->x[0]);
	int result;

	if (!sem) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	result = named_semaphore_wait_nonblocking(sem);
	if (result != 0) {
		set_failure(state, errno_to_darwin(result));
		return;
	}

	set_success(state, 0);
}

static void __attribute__((unused)) handle_sem_trywait(struct syscall_gate_state* state)
{
	handle_sem_wait(state);
}

static void __attribute__((unused)) handle_sem_post(struct syscall_gate_state* state)
{
	struct machgate_named_semaphore* sem = semaphore_from_handle(state->x[0]);
	int result;

	if (!sem) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	result = named_semaphore_post(sem);
	if (result != 0) {
		set_failure(state, errno_to_darwin(result));
		return;
	}

	set_success(state, 0);
}

static void __attribute__((unused)) handle_semwait_signal(struct syscall_gate_state* state)
{
	struct machgate_named_semaphore* wait_sem = NULL;
	struct machgate_named_semaphore* signal_sem = NULL;
	int timeout = (int)state->x[2];
	int relative = (int)state->x[3];
	int64_t tv_sec = (int64_t)state->x[4];
	int32_t tv_nsec = (int32_t)state->x[5];
	int result;

	if (tv_sec < 0 || tv_nsec < 0 || tv_nsec >= 1000000000) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	if (state->x[0]) {
		wait_sem = semaphore_from_handle(state->x[0]);
		if (!wait_sem) {
			set_failure(state, DARWIN_EINVAL);
			return;
		}
	}

	if (state->x[1]) {
		signal_sem = semaphore_from_handle(state->x[1]);
		if (!signal_sem) {
			set_failure(state, DARWIN_EINVAL);
			return;
		}

		result = named_semaphore_post(signal_sem);
		if (result != 0) {
			set_failure(state, errno_to_darwin(result));
			return;
		}
	}

	if (!wait_sem) {
		if (timeout) {
			set_failure(state, errno_to_darwin(EAGAIN));
			return;
		}

		set_failure(state, DARWIN_EINVAL);
		return;
	}

	result = named_semaphore_wait_nonblocking(wait_sem);
	if (result != 0) {
		if (timeout || relative) {
			set_failure(state, errno_to_darwin(EAGAIN));
			return;
		}

		set_failure(state, errno_to_darwin(result));
		return;
	}

	set_success(state, 0);
}

static void handle_psynch_wait_probe(struct syscall_gate_state* state)
{
	uint32_t* address = (uint32_t*)state->x[0];
	uint32_t expected = (uint32_t)state->x[1];
	struct timespec timeout = {0, 0};

	if (!address) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	if (*address != expected) {
		set_failure(state, errno_to_darwin(EAGAIN));
		return;
	}

#ifdef SYS_futex
	errno = 0;
	long result = syscall(SYS_futex, address, FUTEX_WAIT_PRIVATE, expected,
	                      &timeout, NULL, 0);
	if (result < 0) {
		int err = errno ? errno : EIO;
		if (err == ETIMEDOUT)
			err = EAGAIN;
		set_failure(state, errno_to_darwin(err));
		return;
	}

	set_success(state, 0);
#else
	set_enosys(state);
#endif
}

static void handle_psynch_wake(struct syscall_gate_state* state, int wake_all)
{
	uint32_t* address = (uint32_t*)state->x[0];
	int wake_count = wake_all ? INT_MAX : 1;

	if (!address) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

#ifdef SYS_futex
	errno = 0;
	long result = syscall(SYS_futex, address, FUTEX_WAKE_PRIVATE, wake_count,
	                      NULL, NULL, 0);
	finish_syscall_result(state, result);
#else
	set_enosys(state);
#endif
}

static void handle_psynch_yieldwrlock(struct syscall_gate_state* state)
{
#ifdef SYS_sched_yield
	errno = 0;
	long result = syscall(SYS_sched_yield);
	finish_syscall_result(state, result);
#else
	set_success(state, 0);
#endif
}

static void handle_bsdthread_create(struct syscall_gate_state* state)
{
	if (!state->x[0] || !state->x[2] || !state->x[3]) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	set_enosys(state);
}

static void handle_bsdthread_terminate(struct syscall_gate_state* state)
{
	if (!state->x[0] && !state->x[1] && !state->x[2] && !state->x[3]) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	set_enosys(state);
}

static void handle_workq_kernreturn(struct syscall_gate_state* state)
{
	if (state->x[0] == 0) {
		set_success(state, 0);
		return;
	}

	set_failure(state, DARWIN_EINVAL);
}

static void handle_getfsstat64(struct syscall_gate_state* state)
{
	struct darwin_statfs64* darwin_statfs = (struct darwin_statfs64*)state->x[0];
	int bufsize = (int)state->x[1];
	FILE* mounts = setmntent("/proc/self/mounts", "r");
	struct mntent* entry;
	int count = 0;
	int max_entries = 0;

	if (bufsize < 0) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	if (!mounts) {
		set_failure(state, errno_to_darwin(errno ? errno : EIO));
		return;
	}

	if (darwin_statfs && bufsize > 0)
		max_entries = bufsize / (int)sizeof(*darwin_statfs);

	while ((entry = getmntent(mounts)) != NULL) {
		if (darwin_statfs && count < max_entries) {
			struct statfs linux_statfs;
			errno = 0;
			long result = syscall(SYS_statfs, entry->mnt_dir, &linux_statfs);
			if (result == 0) {
				linux_to_darwin_statfs64(&linux_statfs, &darwin_statfs[count],
				                         entry->mnt_type, entry->mnt_dir,
				                         entry->mnt_fsname);
				count++;
			}
			continue;
		}

		count++;
	}

	endmntent(mounts);
	set_success(state, (uint64_t)count);
}

static void handle_sendfile(struct syscall_gate_state* state)
{
	int input_fd = (int)state->x[0];
	int output_fd = (int)state->x[1];
	off_t offset = (off_t)state->x[2];
	off_t* length = (off_t*)state->x[3];
	void* headers = (void*)state->x[4];
	int flags = (int)state->x[5];

	if (!length) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	if (headers || flags) {
		set_enosys(state);
		return;
	}

	errno = 0;
	long result = syscall(SYS_sendfile, output_fd, input_fd, &offset,
	                      (size_t)*length);
	if (result < 0) {
		set_failure(state, errno_to_darwin(errno ? errno : EIO));
		return;
	}

	*length = result;
	set_success(state, 0);
}

static size_t align_darwin_dirent64(size_t size)
{
	return (size + DARWIN_DIRENT64_ALIGN - 1) &
	       ~(size_t)(DARWIN_DIRENT64_ALIGN - 1);
}

static int append_darwin_dirent64(char* darwin_buffer,
                                  size_t darwin_buffer_size,
                                  size_t* darwin_offset,
                                  const struct linux_dirent64_range_200_399* linux_entry)
{
	size_t name_len = strnlen(linux_entry->d_name,
	                          linux_entry->d_reclen -
	                          offsetof(struct linux_dirent64_range_200_399, d_name));
	size_t record_len = align_darwin_dirent64(DARWIN_DIRENT64_HEADER_SIZE +
	                                          name_len + 1);

	if (*darwin_offset + record_len > darwin_buffer_size)
		return 0;

	char* record = darwin_buffer + *darwin_offset;
	memset(record, 0, record_len);
	memcpy(record, &linux_entry->d_ino, sizeof(uint64_t));
	memcpy(record + 8, &linux_entry->d_off, sizeof(int64_t));
	uint16_t darwin_record_len = (uint16_t)record_len;
	uint16_t darwin_name_len = (uint16_t)name_len;
	memcpy(record + 16, &darwin_record_len, sizeof(darwin_record_len));
	memcpy(record + 18, &darwin_name_len, sizeof(darwin_name_len));
	record[20] = (char)linux_entry->d_type;
	memcpy(record + DARWIN_DIRENT64_HEADER_SIZE, linux_entry->d_name,
	       name_len + 1);

	*darwin_offset += record_len;
	return 1;
}

static void handle_getdirentries64(struct syscall_gate_state* state)
{
	int fd = (int)state->x[0];
	char* darwin_buffer = (char*)state->x[1];
	size_t darwin_buffer_size = (size_t)state->x[2];
	int64_t* darwin_position = (int64_t*)state->x[3];
	char linux_buffer[MACHGATE_GETDENTS_BUFFER_SIZE];
	size_t linux_buffer_size = darwin_buffer_size;

	if (!darwin_buffer && darwin_buffer_size) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	if (linux_buffer_size > sizeof(linux_buffer))
		linux_buffer_size = sizeof(linux_buffer);

	errno = 0;
	long result = syscall(SYS_getdents64, fd, linux_buffer,
	                      linux_buffer_size);
	if (result < 0) {
		set_failure(state, errno_to_darwin(errno ? errno : EIO));
		return;
	}

	size_t darwin_offset = 0;
	int64_t last_position = 0;
	for (long linux_offset = 0; linux_offset < result;) {
		const struct linux_dirent64_range_200_399* linux_entry =
			(const struct linux_dirent64_range_200_399*)(linux_buffer +
			                                             linux_offset);
		if (!append_darwin_dirent64(darwin_buffer, darwin_buffer_size,
		                            &darwin_offset, linux_entry)) {
			set_failure(state, DARWIN_EINVAL);
			return;
		}

		last_position = linux_entry->d_off;
		linux_offset += linux_entry->d_reclen;
	}

	if (darwin_position)
		*darwin_position = last_position;

	set_success(state, (uint64_t)darwin_offset);
}

static int translate_xattr_options(uint32_t darwin_options, int* linux_flags)
{
	uint32_t supported = DARWIN_XATTR_NOFOLLOW | DARWIN_XATTR_CREATE |
	                     DARWIN_XATTR_REPLACE;

	if (darwin_options & ~supported)
		return 0;

	*linux_flags = 0;
	if (darwin_options & DARWIN_XATTR_CREATE)
		*linux_flags |= XATTR_CREATE;
	if (darwin_options & DARWIN_XATTR_REPLACE)
		*linux_flags |= XATTR_REPLACE;

	return 1;
}

static void finish_xattr_result(struct syscall_gate_state* state,
                                long result)
{
	if (result < 0)
		set_failure(state, errno_to_darwin(errno ? errno : EIO));
	else
		set_success(state, (uint64_t)result);
}

static void darwin_fdset_to_linux(const uint32_t* darwin_fdset,
                                  uint64_t* linux_fdset,
                                  int fd_count)
{
	int linux_word_count = (fd_count + 63) / 64;

	memset(linux_fdset, 0, (size_t)linux_word_count * sizeof(*linux_fdset));
	for (int fd = 0; fd < fd_count; fd++) {
		if (darwin_fdset[fd / 32] & (1u << (fd % 32)))
			linux_fdset[fd / 64] |= 1ull << (fd % 64);
	}
}

static void linux_fdset_to_darwin(const uint64_t* linux_fdset,
                                  uint32_t* darwin_fdset,
                                  int fd_count)
{
	int darwin_word_count = (fd_count + 31) / 32;

	memset(darwin_fdset, 0, (size_t)darwin_word_count * sizeof(*darwin_fdset));
	for (int fd = 0; fd < fd_count; fd++) {
		if (linux_fdset[fd / 64] & (1ull << (fd % 64)))
			darwin_fdset[fd / 32] |= 1u << (fd % 32);
	}
}

static void handle_pselect(struct syscall_gate_state* state)
{
	int fd_count = (int)state->x[0];
	uint32_t* darwin_readfds = (uint32_t*)state->x[1];
	uint32_t* darwin_writefds = (uint32_t*)state->x[2];
	uint32_t* darwin_exceptfds = (uint32_t*)state->x[3];
	struct timespec* timeout = (struct timespec*)state->x[4];
	void* sigmask = (void*)state->x[5];
	uint64_t linux_readfds[DARWIN_PSELECT_MAX_FDS / 64];
	uint64_t linux_writefds[DARWIN_PSELECT_MAX_FDS / 64];
	uint64_t linux_exceptfds[DARWIN_PSELECT_MAX_FDS / 64];
	void* readfds_arg = NULL;
	void* writefds_arg = NULL;
	void* exceptfds_arg = NULL;

	if (fd_count < 0 || fd_count > DARWIN_PSELECT_MAX_FDS) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	if (sigmask) {
		set_enosys(state);
		return;
	}

	if (darwin_readfds) {
		darwin_fdset_to_linux(darwin_readfds, linux_readfds, fd_count);
		readfds_arg = linux_readfds;
	}
	if (darwin_writefds) {
		darwin_fdset_to_linux(darwin_writefds, linux_writefds, fd_count);
		writefds_arg = linux_writefds;
	}
	if (darwin_exceptfds) {
		darwin_fdset_to_linux(darwin_exceptfds, linux_exceptfds, fd_count);
		exceptfds_arg = linux_exceptfds;
	}

	errno = 0;
	long result = syscall(SYS_pselect6, fd_count, readfds_arg, writefds_arg,
	                      exceptfds_arg, timeout, NULL);
	if (result < 0) {
		set_failure(state, errno_to_darwin(errno ? errno : EIO));
		return;
	}

	if (darwin_readfds)
		linux_fdset_to_darwin(linux_readfds, darwin_readfds, fd_count);
	if (darwin_writefds)
		linux_fdset_to_darwin(linux_writefds, darwin_writefds, fd_count);
	if (darwin_exceptfds)
		linux_fdset_to_darwin(linux_exceptfds, darwin_exceptfds, fd_count);

	set_success(state, (uint64_t)result);
}

static void handle_initgroups(struct syscall_gate_state* state)
{
	uint32_t gidset_size = (uint32_t)state->x[0];
	const uint32_t* gidset = (const uint32_t*)state->x[1];

	if (gidset_size == 0) {
		set_success(state, 0);
		return;
	}

	if (!gidset) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	set_enosys(state);
}

static void handle_posix_spawn(struct syscall_gate_state* state)
{
	const char* path = (const char*)state->x[1];
	struct stat linux_stat;

	if (getenv("MACHGATE_TRACE_SYSCALL") || getenv("MACHGATE_TRACE_SHIM")) {
		fprintf(stderr, "machgate: posix_spawn unsupported path='%s'\n",
		        path ? path : "(null)");
	}

	if (!path) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	errno = 0;
	long result = syscall(SYS_newfstatat, AT_FDCWD, path, &linux_stat, 0);
	if (result < 0 && errno == EFAULT) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	set_failure(state, DARWIN_ENOTSUP);
}

static void handle_identitysvc(struct syscall_gate_state* state)
{
	uint32_t opcode = (uint32_t)state->x[0];
	struct darwin_kauth_cache_sizes_range_200_399* cache_sizes =
	    (struct darwin_kauth_cache_sizes_range_200_399*)state->x[1];
	uint32_t known_bits = DARWIN_KAUTH_EXTLOOKUP_RESULT |
	                      DARWIN_KAUTH_EXTLOOKUP_WORKER |
	                      DARWIN_KAUTH_EXTLOOKUP_DEREGISTER |
	                      DARWIN_KAUTH_GET_CACHE_SIZES |
	                      DARWIN_KAUTH_SET_CACHE_SIZES |
	                      DARWIN_KAUTH_CLEAR_CACHES;

	if (opcode == DARWIN_KAUTH_EXTLOOKUP_REGISTER ||
	    opcode == DARWIN_KAUTH_EXTLOOKUP_DEREGISTER ||
	    opcode == DARWIN_KAUTH_CLEAR_CACHES) {
		set_success(state, 0);
		return;
	}

	if (opcode & ~known_bits) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	if ((opcode & (DARWIN_KAUTH_GET_CACHE_SIZES |
	               DARWIN_KAUTH_SET_CACHE_SIZES |
	               DARWIN_KAUTH_EXTLOOKUP_RESULT |
	               DARWIN_KAUTH_EXTLOOKUP_WORKER)) &&
	    !cache_sizes) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	if (opcode & DARWIN_KAUTH_GET_CACHE_SIZES) {
		cache_sizes->group_size = 0;
		cache_sizes->id_size = 0;
		set_success(state, 0);
		return;
	}

	if (opcode & DARWIN_KAUTH_SET_CACHE_SIZES) {
		set_success(state, 0);
		return;
	}

	set_failure(state, DARWIN_ENOTSUP);
}

static void handle_mac_execve(struct syscall_gate_state* state)
{
	if (getenv("MACHGATE_TRACE_SYSCALL") || getenv("MACHGATE_TRACE_SHIM")) {
		fprintf(stderr, "machgate: __mac_execve path='%s' mac_label=%p\n",
		        state->x[0] ? (const char*)state->x[0] : "(null)",
		        (void*)state->x[3]);
	}

	if (!state->x[0]) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	int result = machgate_execve_macho_guest((const char*)state->x[0],
	                                        (char* const*)state->x[1],
	                                        (char* const*)state->x[2]);
	set_failure(state, result);
}

static int is_eventfd_backed_queue(int fd)
{
	char path[64];
	char target[64];
	int printed = snprintf(path, sizeof(path), "/proc/self/fd/%d", fd);

	if (fd < 0) {
		errno = EBADF;
		return 0;
	}

	if (printed < 0 || (size_t)printed >= sizeof(path)) {
		errno = EBADF;
		return 0;
	}

	errno = 0;
	long result = syscall(SYS_readlinkat, AT_FDCWD, path, target,
	                      sizeof(target) - 1);
	if (result < 0)
		return 0;

	target[result] = '\0';
	if (strcmp(target, "anon_inode:[eventfd]") != 0) {
		errno = EBADF;
		return 0;
	}

	return 1;
}

static void handle_kevent_noop_wait(struct syscall_gate_state* state,
                                    int fd,
                                    uint64_t changelist,
                                    int nchanges,
                                    uint64_t eventlist,
                                    int nevents)
{
	if (nchanges < 0 || nevents < 0) {
		set_failure(state, DARWIN_EINVAL);
		return;
	}

	if (!is_eventfd_backed_queue(fd)) {
		set_failure(state, errno_to_darwin(errno ? errno : EBADF));
		return;
	}

	if (nchanges != 0 || changelist || nevents != 0 || eventlist) {
		set_enosys(state);
		return;
	}

	set_success(state, 0);
}

static void handle_shared_region_check(struct syscall_gate_state* state)
{
	uint64_t* start_address = (uint64_t*)state->x[0];

	if (!start_address) {
		set_failure(state, DARWIN_EFAULT);
		return;
	}

	*start_address = 0;
	set_success(state, 0);
}

static void handle_vm_pressure_monitor(struct syscall_gate_state* state)
{
	uint32_t* pages_reclaimed = (uint32_t*)state->x[2];

	if (pages_reclaimed)
		*pages_reclaimed = 0;

	if (state->x[0] || state->x[1]) {
		set_enosys(state);
		return;
	}

	set_success(state, 0);
}

int syscall_range_200_399_dispatch(struct syscall_gate_state* state)
{
	if (state->x[16] < 200 || state->x[16] > 399)
		return 0;

	switch (state->x[16]) {
	case DARWIN_SYS_truncate: {
		errno = 0;
		long result = syscall(SYS_truncate, (const char*)state->x[0],
		                      (off_t)state->x[1]);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_ftruncate: {
		errno = 0;
		long result = syscall(SYS_ftruncate, (int)state->x[0],
		                      (off_t)state->x[1]);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_sysctl:
		handle_sysctl(state);
		return 1;
	case DARWIN_SYS_mlock: {
		errno = 0;
		long result = syscall(SYS_mlock, (void*)state->x[0],
		                      (size_t)state->x[1]);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_munlock: {
		errno = 0;
		long result = syscall(SYS_munlock, (void*)state->x[0],
		                      (size_t)state->x[1]);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_undelete:
		handle_undelete(state);
		return 1;
	case DARWIN_SYS_open_dprotected_np: {
		errno = 0;
		long result = syscall(SYS_openat, AT_FDCWD,
		                      (const char*)state->x[0],
		                      translate_open_flags((int)state->x[1]),
		                      (mode_t)state->x[4]);
		finish_syscall_result(state, result);
		return 1;
	}
		case DARWIN_SYS_openat_dprotected_np: {
			errno = 0;
			long result = syscall(SYS_openat, (int)state->x[0],
			                      (const char*)state->x[1],
			                      translate_open_flags((int)state->x[2]),
			                      (mode_t)state->x[5]);
			finish_syscall_result(state, result);
			return 1;
		}
		case DARWIN_SYS_exchangedata:
			handle_exchangedata(state);
			return 1;
		case DARWIN_SYS_searchfs:
			handle_searchfs(state);
			return 1;
		case DARWIN_SYS_fhopen:
			handle_fhopen(state);
			return 1;
	case DARWIN_SYS_fsgetpath_ext:
		handle_fsgetpath_ext(state);
		return 1;
	case DARWIN_SYS_getattrlist:
		handle_getattrlist_path(state);
		return 1;
	case DARWIN_SYS_setattrlist:
		handle_setattrlist_path(state);
		return 1;
	case DARWIN_SYS_getdirentriesattr:
		handle_getdirentriesattr(state);
		return 1;
	case DARWIN_SYS_copyfile:
		handle_copyfile(state);
		return 1;
	case DARWIN_SYS_fgetattrlist:
		handle_fgetattrlist(state);
		return 1;
	case DARWIN_SYS_fsetattrlist:
		handle_fsetattrlist(state);
		return 1;
	case DARWIN_SYS_fsctl:
		handle_fsctl_path(state);
		return 1;
	case DARWIN_SYS_ffsctl:
		handle_ffsctl(state);
		return 1;
		case DARWIN_SYS_initgroups:
			handle_initgroups(state);
			return 1;
	case DARWIN_SYS_posix_spawn:
		handle_posix_spawn(state);
		return 1;
		case DARWIN_SYS_minherit:
			handle_minherit(state);
			return 1;
			case DARWIN_SYS_delete: {
				errno = 0;
			long result = syscall(SYS_unlinkat, AT_FDCWD,
			                      (const char*)state->x[0], 0);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_poll: {
		errno = 0;
		long result = raw_poll((struct pollfd*)state->x[0],
		                       (nfds_t)state->x[1], (int)state->x[2]);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_getxattr: {
		if (state->x[4] != 0) {
			set_failure(state, DARWIN_EINVAL);
			return 1;
		}
		int linux_flags = 0;
		if (!translate_xattr_options((uint32_t)state->x[5], &linux_flags)) {
			set_failure(state, DARWIN_EINVAL);
			return 1;
		}
		errno = 0;
		long result;
		if ((uint32_t)state->x[5] & DARWIN_XATTR_NOFOLLOW)
			result = syscall(SYS_lgetxattr, (const char*)state->x[0],
			                 (const char*)state->x[1], (void*)state->x[2],
			                 (size_t)state->x[3]);
		else
			result = syscall(SYS_getxattr, (const char*)state->x[0],
			                 (const char*)state->x[1], (void*)state->x[2],
			                 (size_t)state->x[3]);
		(void)linux_flags;
		finish_xattr_result(state, result);
		return 1;
	}
	case DARWIN_SYS_fgetxattr: {
		if (state->x[4] != 0) {
			set_failure(state, DARWIN_EINVAL);
			return 1;
		}
		int linux_flags = 0;
		if (!translate_xattr_options((uint32_t)state->x[5], &linux_flags)) {
			set_failure(state, DARWIN_EINVAL);
			return 1;
		}
		errno = 0;
		long result = syscall(SYS_fgetxattr, (int)state->x[0],
		                      (const char*)state->x[1], (void*)state->x[2],
		                      (size_t)state->x[3]);
		(void)linux_flags;
		finish_xattr_result(state, result);
		return 1;
	}
	case DARWIN_SYS_setxattr: {
		if (state->x[4] != 0) {
			set_failure(state, DARWIN_EINVAL);
			return 1;
		}
		int linux_flags = 0;
		if (!translate_xattr_options((uint32_t)state->x[5], &linux_flags)) {
			set_failure(state, DARWIN_EINVAL);
			return 1;
		}
		errno = 0;
		long result;
		if ((uint32_t)state->x[5] & DARWIN_XATTR_NOFOLLOW)
			result = syscall(SYS_lsetxattr, (const char*)state->x[0],
			                 (const char*)state->x[1],
			                 (const void*)state->x[2], (size_t)state->x[3],
			                 linux_flags);
		else
			result = syscall(SYS_setxattr, (const char*)state->x[0],
			                 (const char*)state->x[1],
			                 (const void*)state->x[2], (size_t)state->x[3],
			                 linux_flags);
		finish_xattr_result(state, result);
		return 1;
	}
	case DARWIN_SYS_fsetxattr: {
		if (state->x[4] != 0) {
			set_failure(state, DARWIN_EINVAL);
			return 1;
		}
		int linux_flags = 0;
		if (!translate_xattr_options((uint32_t)state->x[5], &linux_flags)) {
			set_failure(state, DARWIN_EINVAL);
			return 1;
		}
		errno = 0;
		long result = syscall(SYS_fsetxattr, (int)state->x[0],
		                      (const char*)state->x[1],
		                      (const void*)state->x[2], (size_t)state->x[3],
		                      linux_flags);
		finish_xattr_result(state, result);
		return 1;
	}
	case DARWIN_SYS_removexattr: {
		int linux_flags = 0;
		if (!translate_xattr_options((uint32_t)state->x[2], &linux_flags)) {
			set_failure(state, DARWIN_EINVAL);
			return 1;
		}
		errno = 0;
		long result;
		if ((uint32_t)state->x[2] & DARWIN_XATTR_NOFOLLOW)
			result = syscall(SYS_lremovexattr, (const char*)state->x[0],
			                 (const char*)state->x[1]);
		else
			result = syscall(SYS_removexattr, (const char*)state->x[0],
			                 (const char*)state->x[1]);
		(void)linux_flags;
		finish_xattr_result(state, result);
		return 1;
	}
	case DARWIN_SYS_fremovexattr: {
		int linux_flags = 0;
		if (!translate_xattr_options((uint32_t)state->x[2], &linux_flags)) {
			set_failure(state, DARWIN_EINVAL);
			return 1;
		}
		errno = 0;
		long result = syscall(SYS_fremovexattr, (int)state->x[0],
		                      (const char*)state->x[1]);
		(void)linux_flags;
		finish_xattr_result(state, result);
		return 1;
	}
	case DARWIN_SYS_listxattr: {
		int linux_flags = 0;
		if (!translate_xattr_options((uint32_t)state->x[3], &linux_flags)) {
			set_failure(state, DARWIN_EINVAL);
			return 1;
		}
		errno = 0;
		long result;
		if ((uint32_t)state->x[3] & DARWIN_XATTR_NOFOLLOW)
			result = syscall(SYS_llistxattr, (const char*)state->x[0],
			                 (char*)state->x[1], (size_t)state->x[2]);
		else
			result = syscall(SYS_listxattr, (const char*)state->x[0],
			                 (char*)state->x[1], (size_t)state->x[2]);
		(void)linux_flags;
		finish_xattr_result(state, result);
		return 1;
	}
	case DARWIN_SYS_flistxattr: {
		int linux_flags = 0;
		if (!translate_xattr_options((uint32_t)state->x[3], &linux_flags)) {
			set_failure(state, DARWIN_EINVAL);
			return 1;
		}
		errno = 0;
		long result = syscall(SYS_flistxattr, (int)state->x[0],
		                      (char*)state->x[1], (size_t)state->x[2]);
		(void)linux_flags;
		finish_xattr_result(state, result);
		return 1;
	}
	case DARWIN_SYS_semsys:
		handle_semsys(state);
		return 1;
	case DARWIN_SYS_msgsys:
		handle_msgsys(state);
		return 1;
	case DARWIN_SYS_shmsys:
		handle_shmsys(state);
		return 1;
	case DARWIN_SYS_semctl:
		handle_sysv_semctl(state, state->x[0], state->x[1], state->x[2],
		                   state->x[3]);
		return 1;
	case DARWIN_SYS_semget:
		handle_sysv_semget(state, state->x[0], state->x[1], state->x[2]);
		return 1;
	case DARWIN_SYS_semop:
		handle_sysv_semop(state, state->x[0], state->x[1], state->x[2]);
		return 1;
	case DARWIN_SYS_msgctl:
		handle_sysv_msgctl(state, state->x[0], state->x[1], state->x[2]);
		return 1;
	case DARWIN_SYS_msgget:
		handle_sysv_msgget(state, state->x[0], state->x[1]);
		return 1;
	case DARWIN_SYS_msgsnd:
		handle_sysv_msgsnd(state, state->x[0], state->x[1], state->x[2],
		                   state->x[3]);
		return 1;
	case DARWIN_SYS_msgrcv:
		handle_sysv_msgrcv(state, state->x[0], state->x[1], state->x[2],
		                   state->x[3], state->x[4]);
		return 1;
	case DARWIN_SYS_shmat:
		handle_sysv_shmat(state, state->x[0], state->x[1], state->x[2]);
		return 1;
	case DARWIN_SYS_shmctl:
		handle_sysv_shmctl(state, state->x[0], state->x[1], state->x[2]);
		return 1;
	case DARWIN_SYS_shmdt:
		handle_sysv_shmdt(state, state->x[0]);
		return 1;
	case DARWIN_SYS_shmget:
		handle_sysv_shmget(state, state->x[0], state->x[1], state->x[2]);
		return 1;
	case DARWIN_SYS_shm_open: {
		char shm_path[256];
		if (!make_posix_shm_path((const char*)state->x[0], shm_path,
		                         sizeof(shm_path))) {
			set_failure(state, errno_to_darwin(errno ? errno : EIO));
			return 1;
		}
		errno = 0;
		long result = syscall(SYS_openat, AT_FDCWD, shm_path,
		                      translate_open_flags((int)state->x[1]),
		                      (mode_t)state->x[2]);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_shm_unlink: {
		char shm_path[256];
		if (!make_posix_shm_path((const char*)state->x[0], shm_path,
		                         sizeof(shm_path))) {
			set_failure(state, errno_to_darwin(errno ? errno : EIO));
			return 1;
		}
		errno = 0;
		long result = syscall(SYS_unlinkat, AT_FDCWD, shm_path, 0);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_sem_open:
		handle_sem_open(state);
		return 1;
	case DARWIN_SYS_sem_close:
		handle_sem_close(state);
		return 1;
	case DARWIN_SYS_sem_unlink:
		handle_sem_unlink(state);
		return 1;
	case DARWIN_SYS_sem_wait:
		handle_sem_wait(state);
		return 1;
	case DARWIN_SYS_sem_trywait:
		handle_sem_trywait(state);
		return 1;
	case DARWIN_SYS_sem_post:
		handle_sem_post(state);
		return 1;
	case DARWIN_SYS_sys_sysctlbyname:
		handle_sysctlbyname(state);
		return 1;
	case DARWIN_SYS_open_extended: {
		errno = 0;
		long result = syscall(SYS_openat, AT_FDCWD,
		                      (const char*)state->x[0],
		                      translate_open_flags((int)state->x[1]),
		                      (mode_t)state->x[4]);
		finish_open_extended_result(state, result, (uid_t)state->x[2],
		                            (gid_t)state->x[3]);
		return 1;
	}
	case DARWIN_SYS_umask_extended: {
		errno = 0;
		long result = syscall(SYS_umask, (mode_t)state->x[0]);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_stat_extended: {
		struct stat linux_stat;
		errno = 0;
		long result = syscall(SYS_newfstatat, AT_FDCWD,
		                      (const char*)state->x[0], &linux_stat, 0);
		finish_extended_stat64_result(state, result, &linux_stat, state->x[1],
		                              state->x[3]);
		return 1;
	}
	case DARWIN_SYS_lstat_extended: {
		struct stat linux_stat;
		errno = 0;
		long result = syscall(SYS_newfstatat, AT_FDCWD,
		                      (const char*)state->x[0], &linux_stat,
		                      AT_SYMLINK_NOFOLLOW);
		finish_extended_stat64_result(state, result, &linux_stat, state->x[1],
		                              state->x[3]);
		return 1;
	}
	case DARWIN_SYS_sys_fstat_extended: {
		struct stat linux_stat;
		errno = 0;
		long result = syscall(SYS_fstat, (int)state->x[0], &linux_stat);
		finish_extended_stat64_result(state, result, &linux_stat, state->x[1],
		                              state->x[3]);
		return 1;
	}
	case DARWIN_SYS_chmod_extended: {
		errno = 0;
		long result = syscall(SYS_fchownat, AT_FDCWD,
		                      (const char*)state->x[0], (uid_t)state->x[1],
		                      (gid_t)state->x[2], 0);
		if (result < 0 && !((uid_t)state->x[1] == (uid_t)-1 &&
		                    (gid_t)state->x[2] == (gid_t)-1)) {
			finish_syscall_result(state, result);
			return 1;
		}
		errno = 0;
		result = syscall(SYS_fchmodat, AT_FDCWD, (const char*)state->x[0],
		                 (mode_t)state->x[3], 0);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_fchmod_extended: {
		errno = 0;
		long result = syscall(SYS_fchown, (int)state->x[0],
		                      (uid_t)state->x[1], (gid_t)state->x[2]);
		if (result < 0 && !((uid_t)state->x[1] == (uid_t)-1 &&
		                    (gid_t)state->x[2] == (gid_t)-1)) {
			finish_syscall_result(state, result);
			return 1;
		}
		errno = 0;
			result = syscall(SYS_fchmod, (int)state->x[0], (mode_t)state->x[3]);
			finish_syscall_result(state, result);
			return 1;
		}
		case DARWIN_SYS_access_extended:
			handle_access_extended(state);
			return 1;
		case DARWIN_SYS_sys_settid:
			handle_sys_settid(state);
			return 1;
	case DARWIN_SYS_gettid:
		handle_gettid(state);
		return 1;
	case DARWIN_SYS_setsgroups:
	case DARWIN_SYS_setwgroups:
		handle_empty_uuid_group_set(state);
		return 1;
	case DARWIN_SYS_getsgroups:
	case DARWIN_SYS_getwgroups:
		handle_empty_uuid_group_get(state);
		return 1;
	case DARWIN_SYS_mkfifo_extended: {
		errno = 0;
		long result = syscall(SYS_mknodat, AT_FDCWD,
		                      (const char*)state->x[0],
		                      S_IFIFO | (mode_t)state->x[3], 0);
		finish_created_node(state, result, (const char*)state->x[0],
		                    (uid_t)state->x[1], (gid_t)state->x[2]);
		return 1;
	}
	case DARWIN_SYS_mkdir_extended: {
		errno = 0;
		long result = syscall(SYS_mkdirat, AT_FDCWD,
		                      (const char*)state->x[0],
		                      (mode_t)state->x[3]);
		finish_created_node(state, result, (const char*)state->x[0],
		                    (uid_t)state->x[1], (gid_t)state->x[2]);
		return 1;
	}
	case DARWIN_SYS_shared_region_check_np:
		handle_shared_region_check(state);
		return 1;
	case DARWIN_SYS_identitysvc:
		handle_identitysvc(state);
		return 1;
	case DARWIN_SYS_vm_pressure_monitor:
		handle_vm_pressure_monitor(state);
		return 1;
	case DARWIN_SYS_psynch_rw_longrdlock:
	case DARWIN_SYS_psynch_mutexwait:
	case DARWIN_SYS_psynch_cvwait:
	case DARWIN_SYS_psynch_rw_rdlock:
	case DARWIN_SYS_psynch_rw_wrlock:
		handle_psynch_wait_probe(state);
		return 1;
	case DARWIN_SYS_psynch_rw_yieldwrlock:
		handle_psynch_yieldwrlock(state);
		return 1;
	case DARWIN_SYS_psynch_mutexdrop:
	case DARWIN_SYS_psynch_cvsignal:
	case DARWIN_SYS_psynch_rw_unlock:
		handle_psynch_wake(state, 0);
		return 1;
	case DARWIN_SYS_psynch_cvbroad:
	case DARWIN_SYS_psynch_cvclrprepost:
		handle_psynch_wake(state, 1);
		return 1;
	case DARWIN_SYS_psynch_rw_downgrade:
	case DARWIN_SYS_psynch_rw_upgrade:
		set_success(state, 0);
		return 1;
	case DARWIN_SYS_psynch_rw_unlock2:
		set_failure(state, DARWIN_ENOTSUP);
		return 1;
	case DARWIN_SYS_getsid: {
		errno = 0;
		long result = syscall(SYS_getsid, (pid_t)state->x[0]);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_sys_settid_with_pid:
		handle_sys_settid_with_pid(state);
		return 1;
	case DARWIN_SYS_aio_fsync:
		record_aio_fsync(state);
		return 1;
	case DARWIN_SYS_aio_return:
		handle_aio_return(state);
		return 1;
	case DARWIN_SYS_aio_suspend:
		handle_aio_suspend(state);
		return 1;
	case DARWIN_SYS_aio_cancel:
		handle_aio_cancel(state);
		return 1;
	case DARWIN_SYS_aio_error:
		handle_aio_error(state);
		return 1;
	case DARWIN_SYS_aio_read:
		handle_aio_transfer(state, DARWIN_LIO_READ);
		return 1;
	case DARWIN_SYS_aio_write:
		handle_aio_transfer(state, DARWIN_LIO_WRITE);
		return 1;
	case DARWIN_SYS_lio_listio:
		handle_lio_listio(state);
		return 1;
	case DARWIN_SYS_iopolicysys:
	case DARWIN_SYS_process_policy:
		set_success(state, 0);
		return 1;
	case DARWIN_SYS_mlockall: {
		errno = 0;
		long result = syscall(SYS_mlockall, (int)state->x[0]);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_munlockall: {
		errno = 0;
		long result = syscall(SYS_munlockall);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_issetugid:
		set_success(state, 0);
		return 1;
	case DARWIN_SYS___pthread_kill:
		handle_pthread_kill(state);
		return 1;
	case DARWIN_SYS___pthread_sigmask:
		handle_pthread_sigmask(state);
		return 1;
	case DARWIN_SYS___sigwait:
		handle_sigwait(state);
		return 1;
	case DARWIN_SYS___disable_threadsignal:
		set_success(state, 0);
		return 1;
	case DARWIN_SYS___pthread_markcancel:
	case DARWIN_SYS___pthread_canceled:
		set_success(state, 0);
		return 1;
	case DARWIN_SYS___semwait_signal:
		handle_semwait_signal(state);
		return 1;
	case DARWIN_SYS_proc_info:
		handle_proc_info(state);
		return 1;
	case DARWIN_SYS_sendfile:
		handle_sendfile(state);
		return 1;
	case DARWIN_SYS_stat64: {
		struct stat linux_stat;
		errno = 0;
		long result = syscall(SYS_newfstatat, AT_FDCWD,
		                      (const char*)state->x[0], &linux_stat, 0);
		finish_stat64_result(state, result, &linux_stat);
		return 1;
	}
	case DARWIN_SYS_fstat64: {
		struct stat linux_stat;
		errno = 0;
		long result = syscall(SYS_fstat, (int)state->x[0], &linux_stat);
		finish_stat64_result(state, result, &linux_stat);
		return 1;
	}
	case DARWIN_SYS_lstat64: {
		struct stat linux_stat;
		errno = 0;
		long result = syscall(SYS_newfstatat, AT_FDCWD,
		                      (const char*)state->x[0], &linux_stat,
		                      AT_SYMLINK_NOFOLLOW);
		finish_stat64_result(state, result, &linux_stat);
		return 1;
	}
	case DARWIN_SYS_stat64_extended: {
		struct stat linux_stat;
		errno = 0;
		long result = syscall(SYS_newfstatat, AT_FDCWD,
		                      (const char*)state->x[0], &linux_stat, 0);
		finish_extended_stat64_result(state, result, &linux_stat, state->x[1],
		                              state->x[3]);
		return 1;
	}
	case DARWIN_SYS_lstat64_extended: {
		struct stat linux_stat;
		errno = 0;
		long result = syscall(SYS_newfstatat, AT_FDCWD,
		                      (const char*)state->x[0], &linux_stat,
		                      AT_SYMLINK_NOFOLLOW);
		finish_extended_stat64_result(state, result, &linux_stat, state->x[1],
		                              state->x[3]);
		return 1;
	}
	case DARWIN_SYS_sys_fstat64_extended: {
		struct stat linux_stat;
		errno = 0;
		long result = syscall(SYS_fstat, (int)state->x[0], &linux_stat);
		finish_extended_stat64_result(state, result, &linux_stat, state->x[1],
		                              state->x[3]);
		return 1;
	}
	case DARWIN_SYS_statfs64: {
		struct statfs linux_statfs;
		errno = 0;
		long result = syscall(SYS_statfs, (const char*)state->x[0],
		                      &linux_statfs);
		finish_statfs64_result(state, result, &linux_statfs,
		                       (const char*)state->x[0]);
		return 1;
	}
	case DARWIN_SYS_fstatfs64: {
		struct statfs linux_statfs;
		errno = 0;
		long result = syscall(SYS_fstatfs, (int)state->x[0], &linux_statfs);
		finish_statfs64_result(state, result, &linux_statfs, NULL);
		return 1;
	}
	case DARWIN_SYS_getfsstat64:
		handle_getfsstat64(state);
		return 1;
	case DARWIN_SYS_getdirentries64:
		handle_getdirentries64(state);
		return 1;
	case DARWIN_SYS___pthread_chdir: {
		errno = 0;
		long result = syscall(SYS_chdir, (const char*)state->x[0]);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS___pthread_fchdir: {
		errno = 0;
		long result = syscall(SYS_fchdir, (int)state->x[0]);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_audit:
		handle_audit_record_compat(state);
		return 1;
	case DARWIN_SYS_auditon:
		set_failure(state, DARWIN_ENOTSUP);
		return 1;
	case DARWIN_SYS_getauid:
		handle_getauid(state);
		return 1;
	case DARWIN_SYS_setauid:
		handle_setauid(state);
		return 1;
	case DARWIN_SYS_getaudit_addr:
		handle_getaudit_addr(state);
		return 1;
	case DARWIN_SYS_setaudit_addr:
		handle_setaudit_addr(state);
		return 1;
	case DARWIN_SYS_auditctl:
		set_failure(state, DARWIN_ENOTSUP);
		return 1;
	case DARWIN_SYS_bsdthread_create:
		handle_bsdthread_create(state);
		return 1;
	case DARWIN_SYS_bsdthread_terminate:
		handle_bsdthread_terminate(state);
		return 1;
	case DARWIN_SYS_kqueue: {
		errno = 0;
		long result = syscall(SYS_eventfd2, 0, EFD_CLOEXEC);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_kevent:
		handle_kevent_noop_wait(state, (int)state->x[0], state->x[1],
		                        (int)state->x[2], state->x[3],
		                        (int)state->x[4]);
		return 1;
	case DARWIN_SYS_lchown: {
		errno = 0;
		long result = syscall(SYS_fchownat, AT_FDCWD,
		                      (const char*)state->x[0],
		                      (uid_t)state->x[1], (gid_t)state->x[2],
		                      AT_SYMLINK_NOFOLLOW);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_bsdthread_register:
		set_success(state, 0);
		return 1;
	case DARWIN_SYS_workq_open:
		set_success(state, 0);
		return 1;
	case DARWIN_SYS_workq_kernreturn:
		handle_workq_kernreturn(state);
		return 1;
	case DARWIN_SYS_kevent64:
		if (state->x[5]) {
			set_enosys(state);
			return 1;
		}
		handle_kevent_noop_wait(state, (int)state->x[0], state->x[1],
		                        (int)state->x[2], state->x[3],
		                        (int)state->x[4]);
		return 1;
	case DARWIN_SYS_thread_selfid: {
		errno = 0;
		long result = syscall(SYS_gettid);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_ledger:
		set_success(state, 0);
		return 1;
	case DARWIN_SYS_kevent_qos:
		if (state->x[5] || state->x[6] || state->x[7]) {
			set_enosys(state);
			return 1;
		}
		handle_kevent_noop_wait(state, (int)state->x[0], state->x[1],
		                        (int)state->x[2], state->x[3],
		                        (int)state->x[4]);
		return 1;
	case DARWIN_SYS_kevent_id:
		if (state->x[5] || state->x[6] || state->x[7]) {
			set_enosys(state);
			return 1;
		}
		handle_kevent_noop_wait(state, (int)state->x[0], state->x[1],
		                        (int)state->x[2], state->x[3],
		                        (int)state->x[4]);
		return 1;
	case DARWIN_SYS___mac_execve:
		handle_mac_execve(state);
		return 1;
	case DARWIN_SYS___mac_syscall:
		set_failure(state, DARWIN_ENOTSUP);
		return 1;
	case DARWIN_SYS___mac_get_file:
		if (!state->x[0]) {
			set_failure(state, DARWIN_EFAULT);
			return 1;
		}
		handle_mac_get_label(state, state->x[1]);
		return 1;
	case DARWIN_SYS___mac_set_file:
		set_failure(state, DARWIN_ENOTSUP);
		return 1;
	case DARWIN_SYS___mac_get_link:
		if (!state->x[0]) {
			set_failure(state, DARWIN_EFAULT);
			return 1;
		}
		handle_mac_get_label(state, state->x[1]);
		return 1;
	case DARWIN_SYS___mac_set_link:
		set_failure(state, DARWIN_ENOTSUP);
		return 1;
	case DARWIN_SYS___mac_get_proc:
		handle_mac_get_label(state, state->x[0]);
		return 1;
	case DARWIN_SYS___mac_set_proc:
		set_failure(state, DARWIN_ENOTSUP);
		return 1;
	case DARWIN_SYS___mac_get_fd:
		if (!validate_fd_argument((int)state->x[0])) {
			set_failure(state, errno_to_darwin(errno ? errno : EBADF));
			return 1;
		}
		handle_mac_get_label(state, state->x[1]);
		return 1;
	case DARWIN_SYS___mac_set_fd:
		set_failure(state, DARWIN_ENOTSUP);
		return 1;
	case DARWIN_SYS___mac_get_pid:
		if ((pid_t)state->x[0] != (pid_t)syscall(SYS_getpid)) {
			set_failure(state, errno_to_darwin(ESRCH));
			return 1;
		}
		handle_mac_get_label(state, state->x[1]);
		return 1;
	case DARWIN_SYS_pselect:
	case DARWIN_SYS_pselect_nocancel:
		handle_pselect(state);
		return 1;
	case DARWIN_SYS_read_nocancel: {
		errno = 0;
		long result = syscall(SYS_read, (int)state->x[0],
		                      (void*)state->x[1], (size_t)state->x[2]);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_write_nocancel: {
		long result = machgate_syscall_write_no_sigpipe((int)state->x[0],
		                                                (const void*)state->x[1],
		                                                (size_t)state->x[2]);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_open_nocancel: {
		errno = 0;
		long result = syscall(SYS_openat, AT_FDCWD,
		                      (const char*)state->x[0],
		                      translate_open_flags((int)state->x[1]),
		                      (mode_t)state->x[2]);
		finish_syscall_result(state, result);
		return 1;
	}
	case DARWIN_SYS_close_nocancel: {
		errno = 0;
		long result = syscall(SYS_close, (int)state->x[0]);
		finish_syscall_result(state, result);
		return 1;
	}
	default:
		set_enosys(state);
		return 1;
	}
}
