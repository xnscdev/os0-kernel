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

#include <bits/mman.h>
#include <bits/mount.h>
#include <bits/random.h>
#include <fs/pipe.h>
#include <libk/libk.h>
#include <sys/acpi.h>
#include <sys/process.h>
#include <sys/reboot.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <vm/heap.h>

#ifdef ARCH_I386
#include <i386/features.h>
#include <cpuid.h>
#endif

extern int exit_task;

void
sys_exit (int code)
{
  pid_t pid = task_getpid ();
  pid_t ppid = process_table[pid].p_task->t_ppid;
  if (pid == 0)
    panic ("Attempted to exit from kernel task");
  process_table[pid].p_term = 1;
  process_table[pid].p_waitstat = (code & 0xff) << 8;
  exit_task = pid;
  if (ppid != 0)
    process_send_signal (ppid, SIGCHLD);
}

int
sys_fork (void)
{
  int ret;
  task_switch_enabled = 0;
  ret = task_fork (1);
  task_switch_enabled = 1;
  return ret;
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
  char **nenvp;
  int ret = vfs_open_file (&inode, path, 1);
  if (ret != 0)
    return ret;
  if (inode->vi_sb->sb_mntflags & MS_NOEXEC)
    {
      vfs_unref_inode (inode);
      return -EACCES;
    }
  memset (&dlinfo, 0, sizeof (DynamicLinkInfo));
  DISABLE_TASK_SWITCH;
  ret = process_exec (inode, &eip, argv, envp, &dlinfo);
  vfs_unref_inode (inode);
  if (ret != 0)
    return ret;
  nenvp = envp == NULL ? NULL : ((char ***) EXEC_DATA_VADDR)[1];
  task_exec (eip, ((char ***) EXEC_DATA_VADDR)[0], nenvp, &dlinfo);
}

int
sys_nice (int inc)
{
  Process *proc = &process_table[task_getpid ()];
  int prio = proc->p_task->t_priority + inc;
  if (proc->p_euid != 0 && inc < 0)
    return -EPERM - 20;
  if (prio < PRIO_MIN)
    prio = PRIO_MIN;
  if (prio > PRIO_MAX)
    prio = PRIO_MAX;
  proc->p_task->t_priority = prio;
  return prio;
}

int
sys_dup (int fd)
{
  return fcntl (fd, F_DUPFD, 0);
}

int
sys_pipe (int fd[2])
{
  Process *proc = &process_table[task_getpid ()];
  VFSInode *read_inode;
  VFSInode *write_inode;
  int read_fd;
  int write_fd;
  Pipe *pipe;
  int ret;

  /* Allocate pipe inodes and structure */
  read_inode = vfs_alloc_inode (&pipe_sb);
  write_inode = vfs_alloc_inode (&pipe_sb);
  pipe = kmalloc (sizeof (Pipe));
  if (unlikely (read_inode == NULL || write_inode == NULL || pipe == NULL))
    {
      ret = -ENOMEM;
      goto err;
    }
  write_inode->vi_flags |= PIPE_WRITE_END;
  read_inode->vi_private = pipe;
  write_inode->vi_private = pipe;

  /* Allocate memory for pipe */
  pipe->p_data = sys_mmap (NULL, PIPE_LENGTH, PROT_READ | PROT_WRITE,
			   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (unlikely ((uintptr_t) pipe->p_data >= UINTPTR_MAX - __NR_errno))
    {
      ret = UINTPTR_MAX - (uintptr_t) pipe->p_data;
      goto err;
    }

  /* Allocate file descriptors */
  read_fd = process_alloc_fd (proc, 0);
  if (unlikely (read_fd < 0))
    {
      sys_munmap (pipe->p_data, PIPE_LENGTH);
      ret = -ENFILE;
      goto err;
    }
  write_fd = process_alloc_fd (proc, 0);
  if (unlikely (write_fd < 0))
    {
      process_free_fd (proc, read_fd);
      sys_munmap (pipe->p_data, PIPE_LENGTH);
      ret = -ENFILE;
    }

  /* Fill file descriptors */
  proc->p_files[read_fd]->pf_inode = read_inode;
  proc->p_files[read_fd]->pf_dir = NULL;
  proc->p_files[read_fd]->pf_path = NULL;
  proc->p_files[read_fd]->pf_mode = O_RDONLY;
  proc->p_files[read_fd]->pf_flags = 0;
  proc->p_files[read_fd]->pf_offset = 0;
  fd[0] = read_fd;

  proc->p_files[write_fd]->pf_inode = write_inode;
  proc->p_files[write_fd]->pf_dir = NULL;
  proc->p_files[write_fd]->pf_path = NULL;
  proc->p_files[write_fd]->pf_mode = O_WRONLY;
  proc->p_files[write_fd]->pf_flags = 0;
  proc->p_files[write_fd]->pf_offset = 0;
  fd[1] = write_fd;
  return 0;

 err:
  vfs_unref_inode (read_inode);
  vfs_unref_inode (write_inode);
  kfree (pipe);
  return ret;
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

int
sys_reboot (int cmd)
{
  uid_t uid = process_table[task_getpid ()].p_euid;
  if (uid != 0)
    return -EPERM;
  switch (cmd)
    {
    case RB_HALT_SYSTEM:
      printk ("System halted.\n");
      return acpi_shutdown ();
    case RB_POWER_OFF:
      printk ("Power down.\n");
      return acpi_shutdown ();
    case RB_AUTOBOOT:
      printk ("Restarting system.\n");
      acpi_reset ();
    default:
      return -EINVAL;
    }
}

int
sys_getpriority (int which, id_t who)
{
  pid_t pid = who != 0 ? who : task_getpid ();
  uid_t uid = who != 0 ? who : process_table[task_getpid ()].p_uid;
  int lowest = 20;
  int i;
  switch (which)
    {
    case PRIO_PROCESS:
      if (!process_valid (pid))
	return -ESRCH - 20;
      return process_table[pid].p_task->t_priority;
    case PRIO_PGRP:
      if (!process_valid (pid))
	return -ESRCH - 20;
      for (i = 0; i < PROCESS_LIMIT; i++)
	{
	  if (process_table[i].p_task != NULL && process_table[i].p_pgid == pid)
	    {
	      int p = process_table[i].p_task->t_priority;
	      if (p < lowest)
		lowest = p;
	    }
	}
      return lowest;
    case PRIO_USER:
      for (i = 0; i < PROCESS_LIMIT; i++)
	{
	  if (process_table[i].p_task != NULL && process_table[i].p_uid == uid)
	    {
	      int p = process_table[i].p_task->t_priority;
	      if (p < lowest)
		lowest = p;
	    }
	}
      return lowest > 19 ? -ESRCH - 20 : lowest;
    default:
      return -EINVAL - 20;
    }
}

int
sys_setpriority (int which, id_t who, int prio)
{
  Process *proc = &process_table[task_getpid ()];
  pid_t pid = who != 0 ? who : proc->p_task->t_pid;
  uid_t uid = who != 0 ? who : proc->p_uid;
  int super = proc->p_euid == 0;
  int i;
  if (prio < PRIO_MIN)
    prio = PRIO_MIN;
  if (prio > PRIO_MAX)
    prio = PRIO_MAX;
  switch (which)
    {
    case PRIO_PROCESS:
      if (!process_valid (pid))
	return -ESRCH;
      if (!super)
	{
	  if (prio < process_table[pid].p_task->t_priority)
	    return -EPERM;
	  if (process_table[pid].p_euid != proc->p_uid
	      && process_table[pid].p_euid != proc->p_euid)
	    return -EPERM;
	}
      process_table[pid].p_task->t_priority = prio;
      return 0;
    case PRIO_PGRP:
      if (!process_valid (pid))
	return -ESRCH;
      if (!super)
	{
	  if (prio < process_table[pid].p_task->t_priority)
	    return -EPERM;
	  if (process_table[pid].p_euid != proc->p_uid
	      && process_table[pid].p_euid != proc->p_euid)
	    return -EPERM;
	}
      for (i = 0; i < PROCESS_LIMIT; i++)
	{
	  if (process_table[i].p_task != NULL && process_table[i].p_pgid == pid)
	    process_table[i].p_task->t_priority = prio;
	}
      return 0;
    case PRIO_USER:
      if (!super)
	{
	  if (prio < process_table[pid].p_task->t_priority)
	    return -EPERM;
	  if (proc->p_uid != uid && proc->p_euid != uid)
	    return -EPERM;
	}
      for (i = 0; i < PROCESS_LIMIT; i++)
	{
	  if (process_table[i].p_task != NULL && process_table[i].p_uid == uid)
	    process_table[i].p_task->t_priority = prio;
	}
      return 0;
    default:
      return -EINVAL;
    }
}

pid_t
sys_wait4 (pid_t pid, int *status, int options, struct rusage *usage)
{
  return wait4 (pid, status, options, usage);
}

int
sys_uname (struct utsname *name)
{
  strcpy (name->sysname, "OS/0");
  strcpy (name->nodename, "os0_system"); /* Dummy value */
  strcpy (name->release, VERSION);
  strcpy (name->version, "Kernel version " VERSION " built " __DATE__);
  strcpy (name->machine, ARCH_STRING);
  return 0;
}

int
sys_vfork (void)
{
  int ret;
  task_switch_enabled = 0;
  ret = task_fork (0);
  task_switch_enabled = 1;
  return ret;
}

ssize_t
sys_getrandom (void *buffer, size_t len, unsigned int flags)
{
  if (flags & GRND_RANDOM)
    {
#ifdef ARCH_I386
#define NEXT_RANDOM if (flags & GRND_NONBLOCK)	\
	{					\
	  if (cpu_random (&n) != 0)		\
	    return -EAGAIN;			\
	}					\
      else					\
	{					\
	  while (1)				\
	    {					\
	      if (cpu_random (&n) != 0)		\
		break;				\
	    }					\
	}

      /* Use RDRAND if supported */
      if (cpu_features_ecx & bit_RDRND)
	{
	  unsigned long *ptr = buffer;
	  unsigned long n;
	  size_t i;
	  for (i = 0; i < (len & ~(sizeof (unsigned long) - 1));
	       i += sizeof (unsigned long))
	    {
	      NEXT_RANDOM;
	      *ptr++ = n;
	    }
	  if (i < len)
	    {
	      NEXT_RANDOM;
	      memcpy (buffer + i, &n, len - i);
	    }
	  return len;
	}
#endif
    }
  get_entropy (buffer, len);
  return len;
}
