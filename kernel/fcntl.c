/*************************************************************************
 * fcntl.c -- This file is part of OS/0.                                 *
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

#include <fs/vfs.h>
#include <libk/libk.h>
#include <sys/process.h>

static int
fcntl_dupfd (Process *proc, int fd, int arg, int cloexec)
{
  int nfd;
  if (arg < 0 || arg >= PROCESS_FILE_LIMIT)
    return -EINVAL;
  for (nfd = arg; nfd < PROCESS_FILE_LIMIT; nfd++)
    {
      if (proc->p_files[nfd] == NULL)
	break;
    }
  if (unlikely (nfd == PROCESS_FILE_LIMIT))
    return -EMFILE;
  proc->p_files[nfd] = proc->p_files[fd];
  vfs_ref_inode (proc->p_files[nfd]->pf_inode);
  return nfd;
}

int
fcntl (int fd, int cmd, int arg)
{
  Process *proc = &process_table[task_getpid ()];
  if (fd < 0 || fd >= PROCESS_FILE_LIMIT || proc->p_files[fd] == NULL)
    return -EBADF;
  switch (cmd)
    {
    case F_DUPFD:
      return fcntl_dupfd (proc, fd, arg, 0);
    case F_DUPFD_CLOEXEC:
      return fcntl_dupfd (proc, fd, arg, 1);
    case F_GETFD:
      return proc->p_files[fd]->pf_flags;
    case F_SETFD:
      proc->p_files[fd]->pf_flags = arg;
      return 0;
    case F_GETFL:
      return proc->p_files[fd]->pf_mode;
    case F_SETFL:
      proc->p_files[fd]->pf_mode = arg;
      return 0;
    default:
      return -EINVAL;
    }
}
