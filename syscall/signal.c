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

int
sys_kill (pid_t pid, int sig)
{
  int exit = sig == SIGKILL;
  struct sigaction *sigaction;
  if (pid == 0 || pid == -1)
    return -ENOSYS;
  if (pid < 0)
    pid = -pid;
  if (pid >= PROCESS_LIMIT)
    return -EINVAL;
  if (sig < 0 || sig >= NR_signals)
    return -EINVAL;
  sigaction = &process_table[pid].p_sigactions[sig];

  if (sig == SIGFPE || sig == SIGILL || sig == SIGSEGV || sig == SIGBUS
      || sig == SIGABRT || sig == SIGTRAP || sig == SIGSYS)
    {
      if (!(sigaction->sa_flags & SA_SIGINFO)
	  && sigaction->sa_handler == SIG_DFL)
	exit = 1; /* Default action is to terminate process */
    }

  if (exit)
    {
      process_table[pid].p_term = 1;
      process_table[pid].p_waitstat = sig;
      return 0;
    }

  /* FIXME This needs to be called on the process being signaled */
  if (sigaction->sa_flags & SA_SIGINFO)
    ; /* TODO Call sigaction->sa_sigaction */
  else
    sigaction->sa_handler (sig);
  return 0;
}

sighandler_t
sys_signal (int sig, sighandler_t func)
{
  struct sigaction old;
  struct sigaction act;
  if (sig < 0 || sig >= NR_signals || sig == SIGKILL || sig == SIGSTOP)
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
  if (sig < 0 || sig >= NR_signals || sig == SIGKILL || sig == SIGSTOP)
    return -EINVAL;
  proc = &process_table[task_getpid ()];
  if (old != NULL)
    memcpy (old, &proc->p_sigactions[sig], sizeof (struct sigaction));
  if (act != NULL)
    memcpy (&proc->p_sigactions[sig], act, sizeof (struct sigaction));
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
