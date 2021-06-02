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
  proc->p_euid = uid;
  proc->p_uid = uid;
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
  proc->p_egid = gid;
  proc->p_gid = gid;
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

pid_t
sys_getppid (void)
{
  return task_getppid ();
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
	  if (euid != proc->p_uid && euid != proc->p_euid)
	    return -EPERM;
	}
    }
  if (ruid != -1)
    proc->p_uid = ruid;
  if (euid != -1)
    proc->p_euid = euid;
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
	  if (egid != proc->p_gid && egid != proc->p_egid)
	    return -EPERM;
	}
    }
  if (rgid != -1)
    proc->p_gid = rgid;
  if (egid != -1)
    proc->p_egid = egid;
  return 0;
}
