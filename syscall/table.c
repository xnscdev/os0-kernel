/*************************************************************************
 * table.c -- This file is part of OS/0.                                 *
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

#include <sys/syscall.h>

void *syscall_table[NR_syscalls] = {
  [SYS_exit] = sys_exit,
  [SYS_fork] = sys_fork,
  [SYS_read] = sys_read,
  [SYS_write] = sys_write,
  [SYS_open] = sys_open,
  [SYS_close] = sys_close,
  [SYS_waitpid] = sys_waitpid,
  [SYS_creat] = sys_creat,
  [SYS_link] = sys_link,
  [SYS_unlink] = sys_unlink,
  [SYS_execve] = sys_execve,
  [SYS_chdir] = sys_chdir,
  [SYS_time] = sys_time,
  [SYS_mknod] = sys_mknod,
  [SYS_chmod] = sys_chmod,
  [SYS_lchown] = sys_lchown,
  [SYS_lseek] = sys_lseek,
  [SYS_getpid] = sys_getpid,
  [SYS_mount] = sys_mount,
  [SYS_umount] = sys_umount,
  [SYS_access] = sys_access,
  [SYS_kill] = sys_kill,
  [SYS_rename] = sys_rename,
  [SYS_mkdir] = sys_mkdir,
  [SYS_rmdir] = sys_rmdir,
  [SYS_dup] = sys_dup,
  [SYS_times] = sys_times,
  [SYS_brk] = sys_brk,
  [SYS_setgid] = sys_setgid,
  [SYS_getgid] = sys_getgid,
  [SYS_signal] = sys_signal,
  [SYS_geteuid] = sys_geteuid,
  [SYS_getegid] = sys_getegid,
  [SYS_ioctl] = sys_ioctl,
  [SYS_fcntl] = sys_fcntl,
  [SYS_dup2] = sys_dup2,
  [SYS_getppid] = sys_getppid,
  [SYS_sigaction] = sys_sigaction,
  [SYS_setreuid] = sys_setreuid,
  [SYS_setregid] = sys_setregid,
  [SYS_getrusage] = sys_getrusage,
  [SYS_gettimeofday] = sys_gettimeofday,
  [SYS_symlink] = sys_symlink,
  [SYS_readlink] = sys_readlink,
  [SYS_truncate] = sys_truncate,
  [SYS_ftruncate] = sys_ftruncate,
  [SYS_fchmod] = sys_fchmod,
  [SYS_fchown] = sys_fchown,
  [SYS_statvfs] = sys_statvfs,
  [SYS_stat] = sys_stat,
  [SYS_lstat] = sys_lstat,
  [SYS_fstat] = sys_fstat,
  [SYS_wait4] = sys_wait4,
  [SYS_sigprocmask] = sys_sigprocmask,
  [SYS_fchdir] = sys_fchdir,
  [SYS_nanosleep] = sys_nanosleep,
  [SYS_setresuid] = sys_setresuid,
  [SYS_getresuid] = sys_getresuid,
  [SYS_setresgid] = sys_setresgid,
  [SYS_getresgid] = sys_getresgid,
  [SYS_chown] = sys_chown,
  [SYS_setxattr] = sys_setxattr,
  [SYS_lsetxattr] = sys_lsetxattr,
  [SYS_fsetxattr] = sys_fsetxattr,
  [SYS_getxattr] = sys_getxattr,
  [SYS_lgetxattr] = sys_lgetxattr,
  [SYS_fgetxattr] = sys_fgetxattr,
  [SYS_listxattr] = sys_listxattr,
  [SYS_llistxattr] = sys_llistxattr,
  [SYS_flistxattr] = sys_flistxattr,
  [SYS_removexattr] = sys_removexattr,
  [SYS_lremovexattr] = sys_lremovexattr,
  [SYS_fremovexattr] = sys_fremovexattr,
  [SYS_openat] = sys_openat,
  [SYS_mkdirat] = sys_mkdirat,
  [SYS_mknodat] = sys_mknodat,
  [SYS_fchownat] = sys_fchownat,
  [SYS_unlinkat] = sys_unlinkat,
  [SYS_renameat] = sys_renameat,
  [SYS_linkat] = sys_linkat,
  [SYS_symlinkat] = sys_symlinkat,
  [SYS_readlinkat] = sys_readlinkat,
  [SYS_fchmodat] = sys_fchmodat,
  [SYS_faccessat] = sys_faccessat
};
