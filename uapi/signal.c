/*************************************************************************
 * signal.c -- This file is part of OS/0.                                *
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

static int
__sys_killpg (pid_t pgid, int sig)
{
  int i;
  for (i = 1; i < PROCESS_LIMIT; i++)
    {
      if (process_table[i].p_pgid == pgid)
	{
	  int ret = sys_kill (i, sig);
	  if (ret < 0)
	    return ret;
	}
    }
  return 0;
}

int
sys_kill (pid_t pid, int sig)
{
  siginfo_t *info;
  pid_t currpid = task_getpid ();
  if (sig < 0 || sig >= NSIG)
    return -EINVAL;
  if (pid == -1)
    return -ENOSYS; /* TODO Send signal to all non-system processes */
  else if (pid <= 0)
    return __sys_killpg (-pid, sig);
  if (!process_valid (pid))
    return -ESRCH;
  if (process_table[currpid].p_euid != 0
      && process_table[currpid].p_euid != process_table[pid].p_euid)
    return -EPERM;

  /* Fill siginfo */
  info = &process_table[pid].p_siginfo;
  info->si_signo = sig;
  info->si_code = SI_USER;
  info->si_pid = currpid;
  info->si_uid = process_table[currpid].p_uid;

  return process_send_signal (pid, sig);
}

int
sys_pause (void)
{
  Process *proc = &process_table[task_getpid ()];
  proc->p_pause = 1;
  while (proc->p_pause)
    ;
  return -EINTR;
}

sig_t
sys_signal (int sig, sig_t func)
{
  struct sigaction old;
  struct sigaction act;
  if (sig < 0 || sig >= NSIG || sig == SIGKILL || sig == SIGSTOP)
    return SIG_ERR;
  act.sa_handler = func;
  act.sa_sigaction = NULL;
  act.sa_flags = 0;
  memset (&act.sa_mask, 0, sizeof (sigset_t));
  if (sys_sigaction (sig, &act, &old) == -1)
    return SIG_ERR;
  return old.sa_handler;
}

int
sys_sigaction (int sig, const struct sigaction *__restrict act,
	       struct sigaction *__restrict old)
{
  Process *proc;
  if (sig < 0 || sig >= NSIG || sig == SIGKILL || sig == SIGSTOP)
    return -EINVAL;
  proc = &process_table[task_getpid ()];
  if (old != NULL)
    memcpy (old, &proc->p_sigactions[sig], sizeof (struct sigaction));
  if (act != NULL)
    memcpy (&proc->p_sigactions[sig], act, sizeof (struct sigaction));
  return 0;
}

int
sys_sigsuspend (const sigset_t *mask)
{
  Process *proc = &process_table[task_getpid ()];
  sigset_t save;
  memcpy (&save, &proc->p_sigblocked, sizeof (sigset_t));
  memcpy (&proc->p_sigblocked, mask, sizeof (sigset_t));
  proc->p_pause = 1;
  while (proc->p_pause)
    ;
  memcpy (&proc->p_sigblocked, &save, sizeof (sigset_t));
  return -EINTR;
}

int
sys_sigpending (sigset_t *set)
{
  memcpy (set, &process_table[task_getpid ()].p_sigpending, sizeof (sigset_t));
  return 0;
}

int
sys_sigprocmask (int how, const sigset_t *__restrict set,
		 sigset_t *__restrict old)
{
  Process *proc = &process_table[task_getpid ()];
  int i;
  if (old != NULL)
    memcpy (old, &proc->p_sigblocked, sizeof (sigset_t));
  if (set != NULL)
    {
      switch (how)
	{
	case SIG_BLOCK:
	  for (i = 0; i < sizeof (sigset_t); i++)
	    proc->p_sigblocked.sig[i] |= set->sig[i];
	  break;
	case SIG_UNBLOCK:
	  for (i = 0; i < sizeof (sigset_t); i++)
	    proc->p_sigblocked.sig[i] &= ~set->sig[i];
	  break;
	case SIG_SETMASK:
	  memcpy (&proc->p_sigblocked, set, sizeof (sigset_t));
	  break;
	default:
	  return -EINVAL;
	}
    }
  return 0;
}
