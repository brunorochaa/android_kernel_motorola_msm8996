#ifndef _ASM_X86_64_IA32_UNISTD_H_
#define _ASM_X86_64_IA32_UNISTD_H_

/*
 * This file contains the system call numbers of the ia32 port,
 * this is for the kernel only.
 */

#define __NR_ia32_restart_syscall 0
#define __NR_ia32_exit		  1
#define __NR_ia32_fork		  2
#define __NR_ia32_read		  3
#define __NR_ia32_write		  4
#define __NR_ia32_open		  5
#define __NR_ia32_close		  6
#define __NR_ia32_waitpid		  7
#define __NR_ia32_creat		  8
#define __NR_ia32_link		  9
#define __NR_ia32_unlink		 10
#define __NR_ia32_execve		 11
#define __NR_ia32_chdir		 12
#define __NR_ia32_time		 13
#define __NR_ia32_mknod		 14
#define __NR_ia32_chmod		 15
#define __NR_ia32_lchown		 16
#define __NR_ia32_break		 17
#define __NR_ia32_oldstat		 18
#define __NR_ia32_lseek		 19
#define __NR_ia32_getpid		 20
#define __NR_ia32_mount		 21
#define __NR_ia32_umount		 22
#define __NR_ia32_setuid		 23
#define __NR_ia32_getuid		 24
#define __NR_ia32_stime		 25
#define __NR_ia32_ptrace		 26
#define __NR_ia32_alarm		 27
#define __NR_ia32_oldfstat		 28
#define __NR_ia32_pause		 29
#define __NR_ia32_utime		 30
#define __NR_ia32_stty		 31
#define __NR_ia32_gtty		 32
#define __NR_ia32_access		 33
#define __NR_ia32_nice		 34
#define __NR_ia32_ftime		 35
#define __NR_ia32_sync		 36
#define __NR_ia32_kill		 37
#define __NR_ia32_rename		 38
#define __NR_ia32_mkdir		 39
#define __NR_ia32_rmdir		 40
#define __NR_ia32_dup		 41
#define __NR_ia32_pipe		 42
#define __NR_ia32_times		 43
#define __NR_ia32_prof		 44
#define __NR_ia32_brk		 45
#define __NR_ia32_setgid		 46
#define __NR_ia32_getgid		 47
#define __NR_ia32_signal		 48
#define __NR_ia32_geteuid		 49
#define __NR_ia32_getegid		 50
#define __NR_ia32_acct		 51
#define __NR_ia32_umount2		 52
#define __NR_ia32_lock		 53
#define __NR_ia32_ioctl		 54
#define __NR_ia32_fcntl		 55
#define __NR_ia32_mpx		 56
#define __NR_ia32_setpgid		 57
#define __NR_ia32_ulimit		 58
#define __NR_ia32_oldolduname	 59
#define __NR_ia32_umask		 60
#define __NR_ia32_chroot		 61
#define __NR_ia32_ustat		 62
#define __NR_ia32_dup2		 63
#define __NR_ia32_getppid		 64
#define __NR_ia32_getpgrp		 65
#define __NR_ia32_setsid		 66
#define __NR_ia32_sigaction		 67
#define __NR_ia32_sgetmask		 68
#define __NR_ia32_ssetmask		 69
#define __NR_ia32_setreuid		 70
#define __NR_ia32_setregid		 71
#define __NR_ia32_sigsuspend		 72
#define __NR_ia32_sigpending		 73
#define __NR_ia32_sethostname	 74
#define __NR_ia32_setrlimit		 75
#define __NR_ia32_getrlimit		 76	/* Back compatible 2Gig limited rlimit */
#define __NR_ia32_getrusage		 77
#define __NR_ia32_gettimeofday	 78
#define __NR_ia32_settimeofday	 79
#define __NR_ia32_getgroups		 80
#define __NR_ia32_setgroups		 81
#define __NR_ia32_select		 82
#define __NR_ia32_symlink		 83
#define __NR_ia32_oldlstat		 84
#define __NR_ia32_readlink		 85
#define __NR_ia32_uselib		 86
#define __NR_ia32_swapon		 87
#define __NR_ia32_reboot		 88
#define __NR_ia32_readdir		 89
#define __NR_ia32_mmap		 90
#define __NR_ia32_munmap		 91
#define __NR_ia32_truncate		 92
#define __NR_ia32_ftruncate		 93
#define __NR_ia32_fchmod		 94
#define __NR_ia32_fchown		 95
#define __NR_ia32_getpriority	 96
#define __NR_ia32_setpriority	 97
#define __NR_ia32_profil		 98
#define __NR_ia32_statfs		 99
#define __NR_ia32_fstatfs		100
#define __NR_ia32_ioperm		101
#define __NR_ia32_socketcall		102
#define __NR_ia32_syslog		103
#define __NR_ia32_setitimer		104
#define __NR_ia32_getitimer		105
#define __NR_ia32_stat		106
#define __NR_ia32_lstat		107
#define __NR_ia32_fstat		108
#define __NR_ia32_olduname		109
#define __NR_ia32_iopl		110
#define __NR_ia32_vhangup		111
#define __NR_ia32_idle		112
#define __NR_ia32_vm86old		113
#define __NR_ia32_wait4		114
#define __NR_ia32_swapoff		115
#define __NR_ia32_sysinfo		116
#define __NR_ia32_ipc		117
#define __NR_ia32_fsync		118
#define __NR_ia32_sigreturn		119
#define __NR_ia32_clone		120
#define __NR_ia32_setdomainname	121
#define __NR_ia32_uname		122
#define __NR_ia32_modify_ldt		123
#define __NR_ia32_adjtimex		124
#define __NR_ia32_mprotect		125
#define __NR_ia32_sigprocmask	126
#define __NR_ia32_create_module	127
#define __NR_ia32_init_module	128
#define __NR_ia32_delete_module	129
#define __NR_ia32_get_kernel_syms	130
#define __NR_ia32_quotactl		131
#define __NR_ia32_getpgid		132
#define __NR_ia32_fchdir		133
#define __NR_ia32_bdflush		134
#define __NR_ia32_sysfs		135
#define __NR_ia32_personality	136
#define __NR_ia32_afs_syscall	137 /* Syscall for Andrew File System */
#define __NR_ia32_setfsuid		138
#define __NR_ia32_setfsgid		139
#define __NR_ia32__llseek		140
#define __NR_ia32_getdents		141
#define __NR_ia32__newselect		142
#define __NR_ia32_flock		143
#define __NR_ia32_msync		144
#define __NR_ia32_readv		145
#define __NR_ia32_writev		146
#define __NR_ia32_getsid		147
#define __NR_ia32_fdatasync		148
#define __NR_ia32__sysctl		149
#define __NR_ia32_mlock		150
#define __NR_ia32_munlock		151
#define __NR_ia32_mlockall		152
#define __NR_ia32_munlockall		153
#define __NR_ia32_sched_setparam		154
#define __NR_ia32_sched_getparam		155
#define __NR_ia32_sched_setscheduler		156
#define __NR_ia32_sched_getscheduler		157
#define __NR_ia32_sched_yield		158
#define __NR_ia32_sched_get_priority_max	159
#define __NR_ia32_sched_get_priority_min	160
#define __NR_ia32_sched_rr_get_interval	161
#define __NR_ia32_nanosleep		162
#define __NR_ia32_mremap		163
#define __NR_ia32_setresuid		164
#define __NR_ia32_getresuid		165
#define __NR_ia32_vm86		166
#define __NR_ia32_query_module	167
#define __NR_ia32_poll		168
#define __NR_ia32_nfsservctl		169
#define __NR_ia32_setresgid		170
#define __NR_ia32_getresgid		171
#define __NR_ia32_prctl              172
#define __NR_ia32_rt_sigreturn	173
#define __NR_ia32_rt_sigaction	174
#define __NR_ia32_rt_sigprocmask	175
#define __NR_ia32_rt_sigpending	176
#define __NR_ia32_rt_sigtimedwait	177
#define __NR_ia32_rt_sigqueueinfo	178
#define __NR_ia32_rt_sigsuspend	179
#define __NR_ia32_pread		180
#define __NR_ia32_pwrite		181
#define __NR_ia32_chown		182
#define __NR_ia32_getcwd		183
#define __NR_ia32_capget		184
#define __NR_ia32_capset		185
#define __NR_ia32_sigaltstack	186
#define __NR_ia32_sendfile		187
#define __NR_ia32_getpmsg		188	/* some people actually want streams */
#define __NR_ia32_putpmsg		189	/* some people actually want streams */
#define __NR_ia32_vfork		190
#define __NR_ia32_ugetrlimit		191	/* SuS compliant getrlimit */
#define __NR_ia32_mmap2		192
#define __NR_ia32_truncate64		193
#define __NR_ia32_ftruncate64	194
#define __NR_ia32_stat64		195
#define __NR_ia32_lstat64		196
#define __NR_ia32_fstat64		197
#define __NR_ia32_lchown32		198
#define __NR_ia32_getuid32		199
#define __NR_ia32_getgid32		200
#define __NR_ia32_geteuid32		201
#define __NR_ia32_getegid32		202
#define __NR_ia32_setreuid32		203
#define __NR_ia32_setregid32		204
#define __NR_ia32_getgroups32	205
#define __NR_ia32_setgroups32	206
#define __NR_ia32_fchown32		207
#define __NR_ia32_setresuid32	208
#define __NR_ia32_getresuid32	209
#define __NR_ia32_setresgid32	210
#define __NR_ia32_getresgid32	211
#define __NR_ia32_chown32		212
#define __NR_ia32_setuid32		213
#define __NR_ia32_setgid32		214
#define __NR_ia32_setfsuid32		215
#define __NR_ia32_setfsgid32		216
#define __NR_ia32_pivot_root		217
#define __NR_ia32_mincore		218
#define __NR_ia32_madvise		219
#define __NR_ia32_madvise1		219	/* delete when C lib stub is removed */
#define __NR_ia32_getdents64		220
#define __NR_ia32_fcntl64		221
#define __NR_ia32_tuxcall		222
#define __NR_ia32_security		223
#define __NR_ia32_gettid		224
#define __NR_ia32_readahead		225
#define __NR_ia32_setxattr		226
#define __NR_ia32_lsetxattr		227
#define __NR_ia32_fsetxattr		228
#define __NR_ia32_getxattr		229
#define __NR_ia32_lgetxattr		230
#define __NR_ia32_fgetxattr		231
#define __NR_ia32_listxattr		232
#define __NR_ia32_llistxattr		233
#define __NR_ia32_flistxattr		234
#define __NR_ia32_removexattr	235
#define __NR_ia32_lremovexattr	236
#define __NR_ia32_fremovexattr	237
#define __NR_ia32_tkill		238
#define __NR_ia32_sendfile64		239
#define __NR_ia32_futex		240
#define __NR_ia32_sched_setaffinity	241
#define __NR_ia32_sched_getaffinity	242
#define __NR_ia32_set_thread_area   243
#define __NR_ia32_get_thread_area	244
#define __NR_ia32_io_setup		245
#define __NR_ia32_io_destroy		246
#define __NR_ia32_io_getevents	247
#define __NR_ia32_io_submit		248
#define __NR_ia32_io_cancel		249
#define __NR_ia32_exit_group		252
#define __NR_ia32_lookup_dcookie	253
#define __NR_ia32_sys_epoll_create	254
#define __NR_ia32_sys_epoll_ctl	255
#define __NR_ia32_sys_epoll_wait	256
#define __NR_ia32_remap_file_pages	257
#define __NR_ia32_set_tid_address	258
#define __NR_ia32_timer_create		259
#define __NR_ia32_timer_settime	(__NR_ia32_timer_create+1)
#define __NR_ia32_timer_gettime	(__NR_ia32_timer_create+2)
#define __NR_ia32_timer_getoverrun	(__NR_ia32_timer_create+3)
#define __NR_ia32_timer_delete	(__NR_ia32_timer_create+4)
#define __NR_ia32_clock_settime	(__NR_ia32_timer_create+5)
#define __NR_ia32_clock_gettime	(__NR_ia32_timer_create+6)
#define __NR_ia32_clock_getres	(__NR_ia32_timer_create+7)
#define __NR_ia32_clock_nanosleep	(__NR_ia32_timer_create+8)
#define __NR_ia32_statfs64		268
#define __NR_ia32_fstatfs64		269
#define __NR_ia32_tgkill		270
#define __NR_ia32_utimes		271
#define __NR_ia32_fadvise64_64		272
#define __NR_ia32_vserver		273
#define __NR_ia32_mbind		274
#define __NR_ia32_get_mempolicy	275
#define __NR_ia32_set_mempolicy	276
#define __NR_ia32_mq_open 		277
#define __NR_ia32_mq_unlink		(__NR_ia32_mq_open+1)
#define __NR_ia32_mq_timedsend	(__NR_ia32_mq_open+2)
#define __NR_ia32_mq_timedreceive	(__NR_ia32_mq_open+3)
#define __NR_ia32_mq_notify		(__NR_ia32_mq_open+4)
#define __NR_ia32_mq_getsetattr	(__NR_ia32_mq_open+5)
#define __NR_ia32_kexec		283
#define __NR_ia32_waitid		284
/* #define __NR_sys_setaltroot	285 */
#define __NR_ia32_add_key		286
#define __NR_ia32_request_key	287
#define __NR_ia32_keyctl		288
#define __NR_ia32_ioprio_set		289
#define __NR_ia32_ioprio_get		290
#define __NR_ia32_inotify_init		291
#define __NR_ia32_inotify_add_watch	292
#define __NR_ia32_inotify_rm_watch	293
#define __NR_ia32_migrate_pages		294
#define __NR_ia32_openat		295
#define __NR_ia32_mkdirat		296
#define __NR_ia32_mknodat		297
#define __NR_ia32_fchownat		298
#define __NR_ia32_futimesat		299
#define __NR_ia32_newfstatat		300
#define __NR_ia32_unlinkat		301
#define __NR_ia32_renameat		302
#define __NR_ia32_linkat		303
#define __NR_ia32_symlinkat		304
#define __NR_ia32_readlinkat		305
#define __NR_ia32_fchmodat		306
#define __NR_ia32_faccessat		307
#define __NR_ia32_pselect6		308
#define __NR_ia32_ppoll			309
#define __NR_ia32_unshare		310

#define IA32_NR_syscalls 315	/* must be > than biggest syscall! */

#endif /* _ASM_X86_64_IA32_UNISTD_H_ */
