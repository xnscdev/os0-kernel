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

extern int exit_task;

pid_t
wait4 (pid_t pid, int *status, int options, struct rusage *usage)
{
  Process *proc;
  __asm__ volatile ("sti");
  if (pid == 0 || pid == -1)
    return -ENOSYS;
  if (pid < 0)
    pid = -pid;
  if (!process_valid (pid))
    return -ESRCH;
  proc = &process_table[pid];
  if (!proc->p_term && (options & WNOHANG))
    return 0;
  proc->p_refcnt++;
  while (!proc->p_term)
    ;
  *status = proc->p_waitstat;
  if (--proc->p_refcnt == 0)
    exit_task = pid;
  if (usage != NULL)
    memcpy (usage, &proc->p_rusage, sizeof (struct rusage));
  return pid;
}
