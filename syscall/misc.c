/*************************************************************************
 * misc.c -- This file is part of OS/0.                                  *
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

#include <libk/libk.h>
#include <sys/process.h>
#include <sys/syscall.h>
#include <sys/wait.h>

extern int exit_task;

void
sys_exit (int code)
{
  pid_t pid = task_getpid ();
  if (pid == 0)
    panic ("Attempted to exit from kernel task");
  process_table[pid].p_term = 1;
  process_table[pid].p_waitstat = (code & 0xff) << 8;
  if (process_table[pid].p_refcnt == 0)
    exit_task = pid;
}

pid_t
sys_waitpid (pid_t pid, int *status, int options)
{
  return wait4 (pid, status, options, NULL);
}

int
sys_execve (const char *path, char *const *argv, char *const *envp)
{
  DynamicLinkInfo dlinfo;
  uint32_t eip;
  VFSInode *inode;
  int ret = vfs_open_file (&inode, path, 1);
  if (ret != 0)
    return ret;
  memset (&dlinfo, 0, sizeof (DynamicLinkInfo));
  ret = process_exec (inode, &eip, &dlinfo);
  vfs_unref_inode (inode);
  if (ret != 0)
    return ret;
  task_exec (eip, argv, envp, &dlinfo);
}

int
sys_dup (int fd)
{
  return fcntl (fd, F_DUPFD, 0);
}

int
sys_brk (void *ptr)
{
  if (sys_getpid () == 0)
    return -EINVAL;
  return process_set_break ((uint32_t) ptr);
}

int
sys_ioctl (int fd, unsigned long req, void *data)
{
  return ioctl (fd, req, data);
}

int
sys_fcntl (int fd, int cmd, int arg)
{
  return fcntl (fd, cmd, arg);
}

int
sys_dup2 (int fd1, int fd2)
{
  sys_close (fd2);
  return fcntl (fd1, F_DUPFD, fd2);
}

int
sys_getrusage (int who, struct rusage *usage)
{
  Process *proc = &process_table[task_getpid ()];
  switch (who)
    {
    case RUSAGE_SELF:
    case RUSAGE_THREAD:
      memcpy (usage, &proc->p_rusage, sizeof (struct rusage));
      break;
    case RUSAGE_CHILDREN:
      memcpy (usage, &proc->p_cusage, sizeof (struct rusage));
      break;
    case RUSAGE_BOTH:
      process_add_rusage (usage, proc);
      break;
    default:
      return -EINVAL;
    }
  return 0;
}

pid_t
sys_wait4 (pid_t pid, int *status, int options, struct rusage *usage)
{
  return wait4 (pid, status, options, usage);
}
