/*************************************************************************
 * systab.c -- This file is part of OS/0.                                *
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
  NULL,
  sys_exit,
  sys_fork,
  sys_read,
  sys_write,
  sys_open,
  sys_close,
  sys_waitpid,
  sys_creat,
  sys_link,
  sys_unlink,
  sys_execve,
  sys_chdir,
  NULL,
  sys_mknod,
  sys_chmod,
  sys_chown,
  NULL,
  NULL,
  sys_lseek,
  sys_getpid,
  sys_mount,
  sys_umount,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  sys_access,
  NULL,
  NULL,
  NULL,
  sys_kill,
  sys_rename,
  sys_mkdir,
  sys_rmdir,
  NULL,
  NULL,
  NULL,
  NULL,
  sys_brk,
  NULL,
  NULL,
  sys_signal,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  sys_ioctl,
  sys_fcntl,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  sys_gettimeofday,
  NULL,
  NULL,
  NULL,
  NULL,
  sys_symlink,
  sys_readlink,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  sys_truncate,
  sys_ftruncate,
  sys_fchmod,
  sys_fchown,
  NULL,
  NULL,
  NULL,
  sys_statvfs,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  sys_stat,
  NULL,
  sys_fstat,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  sys_wait4,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  sys_fchdir,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  sys_setxattr,
  NULL,
  NULL,
  sys_getxattr,
  NULL,
  NULL,
  sys_listxattr,
  NULL,
  NULL,
  sys_removexattr,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  sys_openat,
  sys_mkdirat,
  sys_mknodat,
  sys_fchownat,
  NULL,
  NULL,
  sys_unlinkat,
  sys_renameat,
  sys_linkat,
  sys_symlinkat,
  sys_readlinkat,
  sys_fchmodat,
  sys_faccessat
};
