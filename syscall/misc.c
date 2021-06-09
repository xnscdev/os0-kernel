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
  uint32_t eip;
  VFSInode *inode;
  int ret = vfs_open_file (&inode, path, 1);
  if (ret != 0)
    return ret;
  ret = process_exec (inode, &eip);
  vfs_unref_inode (inode);
  if (ret != 0)
    return ret;
  task_exec (eip, argv, envp);
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
      usage->ru_utime.tv_sec += proc->p_rusage.ru_utime.tv_sec +
	proc->p_cusage.ru_utime.tv_sec;
      usage->ru_utime.tv_usec += proc->p_rusage.ru_utime.tv_usec +
	proc->p_cusage.ru_utime.tv_usec;
      usage->ru_utime.tv_sec += usage->ru_utime.tv_usec / 1000000;
      usage->ru_utime.tv_usec %= 1000000;
      usage->ru_stime.tv_sec += proc->p_rusage.ru_stime.tv_sec +
	proc->p_cusage.ru_stime.tv_sec;
      usage->ru_stime.tv_usec += proc->p_rusage.ru_stime.tv_usec +
	proc->p_cusage.ru_stime.tv_usec;
      usage->ru_stime.tv_sec += usage->ru_stime.tv_usec / 1000000;
      usage->ru_stime.tv_usec %= 1000000;
      usage->ru_maxrss += proc->p_rusage.ru_maxrss + proc->p_cusage.ru_maxrss;
      usage->ru_ixrss += proc->p_rusage.ru_ixrss + proc->p_cusage.ru_ixrss;
      usage->ru_idrss += proc->p_rusage.ru_idrss + proc->p_cusage.ru_idrss;
      usage->ru_isrss += proc->p_rusage.ru_isrss + proc->p_cusage.ru_isrss;
      usage->ru_minflt += proc->p_rusage.ru_minflt + proc->p_cusage.ru_minflt;
      usage->ru_majflt += proc->p_rusage.ru_majflt + proc->p_cusage.ru_majflt;
      usage->ru_nswap += proc->p_rusage.ru_nswap + proc->p_cusage.ru_nswap;
      usage->ru_inblock += proc->p_rusage.ru_inblock +
	proc->p_cusage.ru_inblock;
      usage->ru_oublock += proc->p_rusage.ru_oublock +
	proc->p_cusage.ru_oublock;
      usage->ru_msgsnd += proc->p_rusage.ru_msgsnd + proc->p_cusage.ru_msgsnd;
      usage->ru_msgrcv += proc->p_rusage.ru_msgrcv + proc->p_cusage.ru_msgrcv;
      usage->ru_nsignals += proc->p_rusage.ru_nsignals +
	proc->p_cusage.ru_nsignals;
      usage->ru_nvcsw += proc->p_rusage.ru_nvcsw + proc->p_cusage.ru_nvcsw;
      usage->ru_nivcsw += proc->p_rusage.ru_nivcsw + proc->p_cusage.ru_nivcsw;
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
