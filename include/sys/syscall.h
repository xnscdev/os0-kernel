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
#include <sys/cdefs.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <stddef.h>

__BEGIN_DECLS

void sys_exit (int code);
int sys_fork (void);
int sys_read (int fd, void *buffer, size_t len);
int sys_write (int fd, void *buffer, size_t len);
int sys_open (const char *path, int flags, mode_t mode);
int sys_close (int fd);
int sys_creat (const char *path, mode_t mode);
int sys_link (const char *old, const char *new);
int sys_unlink (const char *path);
int sys_execve (const char *path, char *const *argv, char *const *envp);
int sys_chdir (const char *path);
int sys_mknod (const char *path, mode_t mode, dev_t dev);
int sys_chmod (const char *path, mode_t mode);
int sys_chown (const char *path, uid_t uid, gid_t gid);
off_t sys_lseek (int fd, off_t offset, int whence);
pid_t sys_getpid (void);
int sys_mount (const char *src, const char *dir, const char *type, int flags,
	       void *data);
int sys_umount (const char *dir);
int sys_access (const char *path, int mode);
int sys_rename (const char *old, const char *new);
int sys_mkdir (const char *path, mode_t mode);
int sys_rmdir (const char *path);
int sys_brk (void *ptr);
int sys_fcntl (int fd, int cmd, int arg);
int sys_sigaction (int sig, const struct sigaction *__restrict act,
		   struct sigaction *__restrict old);
int sys_gettimeofday (struct timeval *__restrict tv,
		      struct timezone *__restrict tz);
int sys_symlink (const char *old, const char *new);
int sys_readlink (const char *path, char *buffer, size_t len);
int sys_truncate (const char *path, off_t len);
int sys_ftruncate (int fd, off_t len);
int sys_fchmod (int fd, mode_t mode);
int sys_fchown (int fd, uid_t uid, gid_t gid);
int sys_statvfs (const char *path, struct statvfs *st);
int sys_stat (const char *path, struct stat *st);
int sys_fchdir (int fd);
int sys_setxattr (const char *path, const char *name, const void *value,
		  size_t len, int flags);
int sys_getxattr (const char *path, const char *name, void *value, size_t len);
int sys_listxattr (const char *path, char *buffer, size_t len);
int sys_removexattr (const char *path, const char *name);
int sys_openat (int fd, const char *path, int flags, mode_t mode);
int sys_mkdirat (int fd, const char *path, mode_t mode);
int sys_mknodat (int fd, const char *path, mode_t mode, dev_t dev);
int sys_fchownat (int fd, const char *path, uid_t uid, gid_t gid, int flags);
int sys_unlinkat (int fd, const char *path, int flags);
int sys_renameat (int oldfd, const char *old, int newfd, const char *new);
int sys_linkat (int oldfd, const char *old, int newfd, const char *new);
int sys_symlinkat (const char *old, int fd, const char *new);
int sys_readlinkat (int fd, const char *__restrict path,
		    char *__restrict buffer, size_t len);
int sys_fchmodat (int fd, const char *path, mode_t mode, int flags);
int sys_faccessat (int fd, const char *path, int mode, int flags);

__END_DECLS

#endif
