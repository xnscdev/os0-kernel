/*************************************************************************
 * id.c -- This file is part of OS/0.                                    *
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
#include <vm/heap.h>

pid_t
sys_getpid (void)
{
  return task_getpid ();
}

int
sys_setuid (uid_t uid)
{
  Process *proc = &process_table[task_getpid ()];
  if (proc->p_euid != 0 && proc->p_uid != uid)
    return -EPERM;
  proc->p_uid = uid;
  proc->p_euid = uid;
  proc->p_suid = uid;
  return 0;
}

uid_t
sys_getuid (void)
{
  return process_table[task_getpid ()].p_uid;
}

int
sys_setgid (gid_t gid)
{
  Process *proc = &process_table[task_getpid ()];
  if (proc->p_euid != 0 && proc->p_gid != gid)
    return -EPERM;
  proc->p_gid = gid;
  proc->p_egid = gid;
  proc->p_sgid = gid;
  return 0;
}

gid_t
sys_getgid (void)
{
  return process_table[task_getpid ()].p_gid;
}

uid_t
sys_geteuid (void)
{
  return process_table[task_getpid ()].p_euid;
}

gid_t
sys_getegid (void)
{
  return process_table[task_getpid ()].p_egid;
}

int
sys_setpgid (pid_t pid, pid_t pgid)
{
  if (pgid < 0)
    return -EINVAL;
  if (!process_valid (pid))
    return -ESRCH;
  process_table[pid == 0 ? task_getpid () : pid].p_pgid =
    pgid == 0 ? task_getpid () : pgid;
  return 0;
}

pid_t
sys_getppid (void)
{
  return task_getppid ();
}

pid_t
sys_getpgrp (void)
{
  return process_table[task_getpid ()].p_pgid;
}

pid_t
sys_setsid (void)
{
  pid_t pid = task_getpid ();
  Process *proc = &process_table[pid];
  int i;
  for (i = 0; i < PROCESS_LIMIT; i++)
    {
      if (i != pid && process_table[i].p_pgid == pid)
	return -EPERM; /* Calling process is a process group leader */
    }
  proc->p_pgid = pid;
  proc->p_sid = pid;
  return proc->p_sid;
}

int
sys_setreuid (uid_t ruid, uid_t euid)
{
  Process *proc = &process_table[task_getpid ()];
  if (proc->p_euid != 0)
    {
      if (ruid != -1)
	{
	  if (ruid != proc->p_uid && ruid != proc->p_euid)
	    return -EPERM;
	}
      if (euid != -1)
	{
	  if (euid != proc->p_uid && euid != proc->p_euid
	      && euid != proc->p_suid)
	    return -EPERM;
	}
    }
  if (euid != -1)
    {
      if (proc->p_uid != euid)
	proc->p_suid = euid;
      proc->p_euid = euid;
    }
  if (ruid != -1)
    {
      proc->p_uid = ruid;
      proc->p_suid = proc->p_euid;
    }
  return 0;
}

int
sys_setregid (gid_t rgid, gid_t egid)
{
  Process *proc = &process_table[task_getpid ()];
  if (proc->p_euid != 0)
    {
      if (rgid != -1)
	{
	  if (rgid != proc->p_gid && rgid != proc->p_egid)
	    return -EPERM;
	}
      if (egid != -1)
	{
	  if (egid != proc->p_gid && egid != proc->p_egid
	      && egid != proc->p_sgid)
	    return -EPERM;
	}
    }
  if (egid != -1)
    {
      if (proc->p_gid != egid)
	proc->p_sgid = egid;
      proc->p_egid = egid;
    }
  if (rgid != -1)
    {
      proc->p_gid = rgid;
      proc->p_sgid = proc->p_egid;
    }
  return 0;
}

pid_t
sys_getpgid (pid_t pid)
{
  if (!process_valid (pid))
    return -ESRCH;
  return process_table[pid == 0 ? task_getpid () : pid].p_pgid;
}

pid_t
sys_getsid (pid_t pid)
{
  if (!process_valid (pid))
    return -ESRCH;
  return process_table[pid].p_sid;
}

int
sys_setresuid (uid_t ruid, uid_t euid, uid_t suid)
{
  Process *proc = &process_table[task_getpid ()];
  if (proc->p_euid != 0)
    {
      if (ruid != -1)
	{
	  if (ruid != proc->p_uid && ruid != proc->p_euid
	      && ruid != proc->p_suid)
	    return -EPERM;
	}
      if (euid != -1)
	{
	  if (euid != proc->p_uid && euid != proc->p_euid
	      && euid != proc->p_suid)
	    return -EPERM;
	}
      if (suid != -1)
	{
	  if (suid != proc->p_uid && suid != proc->p_euid
	      && suid != proc->p_suid)
	    return -EPERM;
	}
    }
  if (ruid != -1)
    proc->p_uid = ruid;
  if (euid != -1)
    proc->p_euid = euid;
  if (suid != -1)
    proc->p_suid = suid;
  return 0;
}

int
sys_getresuid (uid_t *ruid, uid_t *euid, uid_t *suid)
{
  Process *proc = &process_table[task_getpid ()];
  if (ruid != NULL)
    *ruid = proc->p_uid;
  if (euid != NULL)
    *euid = proc->p_euid;
  if (suid != NULL)
    *suid = proc->p_suid;
  return 0;
}

int
sys_setresgid (gid_t rgid, gid_t egid, gid_t sgid)
{
  Process *proc = &process_table[task_getpid ()];
  if (proc->p_euid != 0)
    {
      if (rgid != -1)
	{
	  if (rgid != proc->p_gid && rgid != proc->p_egid
	      && rgid != proc->p_sgid)
	    return -EPERM;
	}
      if (egid != -1)
	{
	  if (egid != proc->p_gid && egid != proc->p_egid
	      && egid != proc->p_sgid)
	    return -EPERM;
	}
      if (sgid != -1)
	{
	  if (sgid != proc->p_gid && sgid != proc->p_egid
	      && sgid != proc->p_sgid)
	    return -EPERM;
	}
    }
  if (rgid != -1)
    proc->p_gid = rgid;
  if (egid != -1)
    proc->p_egid = egid;
  if (sgid != -1)
    proc->p_sgid = sgid;
  return 0;
}

int
sys_getresgid (gid_t *rgid, gid_t *egid, gid_t *sgid)
{
  Process *proc = &process_table[task_getpid ()];
  if (rgid != NULL)
    *rgid = proc->p_gid;
  if (egid != NULL)
    *egid = proc->p_egid;
  if (sgid != NULL)
    *sgid = proc->p_sgid;
  return 0;
}
