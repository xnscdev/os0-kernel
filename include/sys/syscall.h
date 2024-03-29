/*************************************************************************
 * syscall.h -- This file is part of OS/0.                               *
 * Copyright (C) 2021 XNSC                                               *
 *                                                                       *
 * OS/0 is free software: you can redistribute it and/or modify          *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation, either version 3 of the License, or     *
 * (at your option) any later version.                                   *
 *                                                                       *
 * OS/0 is distributed in the hope that it will be useful,               *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the          *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have received a copy of the GNU General Public License     *
 * along with OS/0. If not, see <https://www.gnu.org/licenses/>.         *
 *************************************************************************/

#ifndef _SYS_SYSCALL_H
#define _SYS_SYSCALL_H

#include <bits/syscall.h>
#include <bits/uio.h>
#include <bits/utime.h>
#include <bits/utsname.h>
#include <fs/vfs.h>
#include <sys/cdefs.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/resource.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <dirent.h>

typedef struct
{
  unsigned int d_curr_count;
  unsigned int d_max_count;
  off64_t d_curr_offset;
  off64_t d_min_offset;
  struct dirent *d_dirp;
} DirIterContext;

__BEGIN_DECLS

void sys_exit (int code);
int sys_fork (void);
ssize_t sys_read (int fd, void *buffer, size_t len);
ssize_t sys_write (int fd, const void *buffer, size_t len);
int sys_open (const char *path, int flags, mode_t mode);
int sys_close (int fd);
pid_t sys_waitpid (pid_t pid, int *status, int options);
int sys_creat (const char *path, mode_t mode);
int sys_link (const char *old, const char *new);
int sys_unlink (const char *path);
int sys_execve (const char *path, char *const *argv, char *const *envp);
int sys_chdir (const char *path);
time_t sys_time (time_t *t);
int sys_mknod (const char *path, mode_t mode, dev_t dev);
int sys_chmod (const char *path, mode_t mode);
int sys_lchown (const char *path, uid_t uid, gid_t gid);
off_t sys_lseek (int fd, off_t offset, int whence);
pid_t sys_getpid (void);
int sys_mount (const char *src, const char *dir, const char *type,
	       unsigned long flags, const void *data);
int sys_umount (const char *dir);
int sys_setuid (uid_t uid);
uid_t sys_getuid (void);
unsigned int sys_alarm (unsigned int seconds);
int sys_pause (void);
int sys_utime (const char *path, const struct utimbuf *times);
int sys_access (const char *path, int mode);
int sys_nice (int inc);
void sys_sync (void);
int sys_kill (pid_t pid, int sig);
int sys_rename (const char *old, const char *new);
int sys_mkdir (const char *path, mode_t mode);
int sys_rmdir (const char *path);
int sys_dup (int fd);
int sys_pipe (int fd[2]);
clock_t sys_times (struct tms *tms);
int sys_brk (void *ptr);
int sys_setgid (gid_t gid);
gid_t sys_getgid (void);
sig_t sys_signal (int sig, sig_t func);
uid_t sys_geteuid (void);
gid_t sys_getegid (void);
int sys_ioctl (int fd, unsigned long req, void *data);
int sys_fcntl (int fd, int cmd, int arg);
int sys_setpgid (pid_t pid, pid_t pgid);
mode_t sys_umask (mode_t mask);
int sys_dup2 (int fd1, int fd2);
pid_t sys_getppid (void);
pid_t sys_getpgrp (void);
pid_t sys_setsid (void);
int sys_sigaction (int sig, const struct sigaction *__restrict act,
		   struct sigaction *__restrict old);
int sys_setreuid (uid_t ruid, uid_t euid);
int sys_setregid (gid_t rgid, gid_t egid);
int sys_sigsuspend (const sigset_t *mask);
int sys_sigpending (sigset_t *set);
int sys_setrlimit (int resource, const struct rlimit *rlim);
int sys_getrlimit (int resource, struct rlimit *rlim);
int sys_getrusage (int who, struct rusage *usage);
int sys_gettimeofday (struct timeval *__restrict tv,
		      struct timezone *__restrict tz);
int sys_getgroups (int size, gid_t list[]);
int sys_setgroups (size_t size, const gid_t *list);
int sys_symlink (const char *old, const char *new);
int sys_readlink (const char *path, char *buffer, size_t len);
int sys_reboot (int cmd);
void *sys_mmap (void *addr, size_t len, int prot, int flags, int fd,
		off_t offset);
int sys_munmap (void *addr, size_t len);
int sys_truncate (const char *path, off_t len);
int sys_ftruncate (int fd, off_t len);
int sys_fchmod (int fd, mode_t mode);
int sys_fchown (int fd, uid_t uid, gid_t gid);
int sys_getpriority (int which, id_t who);
int sys_setpriority (int which, id_t who, int prio);
int sys_statfs (const char *path, struct statfs *st);
int sys_fstatfs (int fd, struct statfs *st);
int sys_setitimer (int which, const struct itimerval *__restrict new,
		   struct itimerval *__restrict old);
int sys_getitimer (int which, struct itimerval *curr);
int sys_stat (const char *path, struct stat *st);
int sys_lstat (const char *path, struct stat *st);
int sys_fstat (int fd, struct stat *st);
pid_t sys_wait4 (pid_t pid, int *status, int options, struct rusage *usage);
int sys_fsync (int fd);
int sys_uname (struct utsname *name);
int sys_mprotect (void *addr, size_t len, int prot);
int sys_sigprocmask (int how, const sigset_t *__restrict set,
		     sigset_t *__restrict old);
pid_t sys_getpgid (pid_t pid);
int sys_fchdir (int fd);
int sys__llseek (int fd, unsigned long offset_high, unsigned long offset_low,
		 off64_t *result, int whence);
int sys_getdents (int fd, struct dirent *dirp, unsigned int count);
ssize_t sys_readv (int fd, const struct iovec *vec, int vlen);
ssize_t sys_writev (int fd, const struct iovec *vec, int vlen);
pid_t sys_getsid (pid_t pid);
int sys_nanosleep (const struct timespec *req, struct timespec *rem);
int sys_setresuid (uid_t ruid, uid_t euid, uid_t suid);
int sys_getresuid (uid_t *ruid, uid_t *euid, uid_t *suid);
int sys_setresgid (gid_t rgid, gid_t egid, gid_t sgid);
int sys_getresgid (gid_t *rgid, gid_t *egid, gid_t *sgid);
ssize_t sys_pread64 (int fd, void *buffer, size_t len, off64_t offset);
ssize_t sys_pwrite64 (int fd, const void *buffer, size_t len, off64_t offset);
int sys_chown (const char *path, uid_t uid, gid_t gid);
int sys_getcwd (char *buffer, size_t len);
int sys_vfork (void);
int sys_truncate64 (const char *path, off64_t len);
int sys_ftruncate64 (int fd, off64_t len);
int sys_stat64 (const char *path, struct stat64 *st);
int sys_lstat64 (const char *path, struct stat64 *st);
int sys_fstat64 (int fd, struct stat64 *st);
int sys_setxattr (const char *path, const char *name, const void *value,
		  size_t len, int flags);
int sys_lsetxattr (const char *path, const char *name, const void *value,
		   size_t len, int flags);
int sys_fsetxattr (int fd, const char *name, const void *value, size_t len,
		   int flags);
int sys_getxattr (const char *path, const char *name, void *value, size_t len);
int sys_lgetxattr (const char *path, const char *name, void *value, size_t len);
int sys_fgetxattr (int fd, const char *name, void *value, size_t len);
int sys_listxattr (const char *path, char *buffer, size_t len);
int sys_llistxattr (const char *path, char *buffer, size_t len);
int sys_flistxattr (int fd, char *buffer, size_t len);
int sys_removexattr (const char *path, const char *name);
int sys_lremovexattr (const char *path, const char *name);
int sys_fremovexattr (int fd, const char *name);
int sys_clock_getres (clockid_t id, struct timespec *tp);
int sys_clock_gettime (clockid_t id, struct timespec *tp);
int sys_statfs64 (const char *path, struct statfs64 *st);
int sys_fstatfs64 (int fd, struct statfs64 *st);
int sys_utimes (const char *path, const struct timeval times[2]);
int sys_openat (int fd, const char *path, int flags, mode_t mode);
int sys_mkdirat (int fd, const char *path, mode_t mode);
int sys_mknodat (int fd, const char *path, mode_t mode, dev_t dev);
int sys_fchownat (int fd, const char *path, uid_t uid, gid_t gid, int flags);
int sys_futimesat (int fd, const char *path, const struct timeval times[2]);
int sys_fstatat64 (int fd, const char *path, struct stat64 *st, int flags);
int sys_unlinkat (int fd, const char *path, int flags);
int sys_renameat (int oldfd, const char *old, int newfd, const char *new);
int sys_linkat (int oldfd, const char *old, int newfd, const char *new,
		int flags);
int sys_symlinkat (const char *old, int fd, const char *new);
int sys_readlinkat (int fd, const char *__restrict path,
		    char *__restrict buffer, size_t len);
int sys_fchmodat (int fd, const char *path, mode_t mode, int flags);
int sys_faccessat (int fd, const char *path, int mode, int flags);
int sys_utimensat (int fd, const char *path, const struct timespec times[2],
		   int flags);
ssize_t sys_getrandom (void *buffer, size_t len, unsigned int flags);

/* Utility functions */

int sys_path_sep (const char *path, VFSInode **dir, char **name);
VFSInode *inode_from_fd (int fd);
int sys_read_dir_entry (const char *name, size_t namelen, ino64_t ino,
			unsigned char type, off64_t offset, void *private);

__END_DECLS

#endif
