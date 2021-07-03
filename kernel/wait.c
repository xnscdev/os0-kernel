/*************************************************************************
 * wait.c -- This file is part of OS/0.                                  *
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
#include <sys/wait.h>

int exit_task;

static pid_t
wait_get_status (pid_t pid, int *status, struct rusage *usage)
{
  pid_t curr = task_getpid ();
  pid_t temp = exit_task;
  if (exit_task == 0)
    return 0; /* No process has terminated yet */

  switch (pid)
    {
    case -1:
    case 0:
      /* Check if the exited process is a child of the current process */
      while (temp != 0 && process_table[temp].p_task->t_ppid != curr)
	temp = process_table[temp].p_task->t_ppid;
      if (temp == 0)
	return 0;

      /* Make sure the exited process is in the same group as the caller if
	 pid is zero */
      if (pid != -1 && process_table[curr].p_pgid != process_table[temp].p_pgid)
	return 0;
      break;
    default:
      if (temp != pid)
	return 0;
    }

  if (usage != NULL)
    memcpy (usage, &process_table[temp].p_rusage, sizeof (struct rusage));
  *status = process_table[temp].p_waitstat;

  pid = temp;
  temp = process_table[temp].p_task->t_ppid;
  while (temp != 0)
    {
      process_table[temp].p_children--;
      temp = process_table[temp].p_task->t_ppid;
    }
  process_table[0].p_children--;
  return pid;
}

pid_t
wait4 (pid_t pid, int *status, int options, struct rusage *usage)
{
  __asm__ volatile ("sti");
  if (pid < -1)
    pid = -pid;
  if (process_table[task_getpid ()].p_children == 0)
    return -ECHILD;
  if (pid > 0 && !process_valid (pid))
    return -ESRCH;
  if (options & WNOHANG)
    return wait_get_status (pid, status, usage);
  while (1)
    {
      pid_t ret = wait_get_status (pid, status, usage);
      if (ret != 0)
	return ret;
    }
}
