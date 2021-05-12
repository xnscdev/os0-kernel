/*************************************************************************
 * signal.h -- This file is part of OS/0.                                *
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

#ifndef _SYS_SIGNAL_H
#define _SYS_SIGNAL_H

#include <sys/types.h>
#include <limits.h>

#define SIGHUP    1
#define SIGINT    2
#define SIGQUIT   3
#define SIGILL    4
#define SIGTRAP   5
#define SIGABRT   6
#define SIGIOT    6
#define SIGBUS    7
#define SIGFPE    8
#define SIGKILL   9
#define SIGUSR1   10
#define SIGSEGV   11
#define SIGUSR2   12
#define SIGPIPE   13
#define SIGALRM   14
#define SIGTERM   15
#define SIGSTKFLT 16
#define SIGCHLD   17
#define SIGCONT   18
#define SIGSTOP   19
#define SIGTSTP   20
#define SIGTTIN   21
#define SIGTTOU   22
#define SIGURG    23
#define SIGXCPU   24
#define SIGXFSZ   25
#define SIGVTALRM 26
#define SIGPROF   27
#define SIGWINCH  28
#define SIGIO     29
#define SIGPOLL   SIGIO
#define SIGPWR    30
#define SIGSYS    31
#define SIGUNUSED 31

#define NR_signals 48
#define SIGRTMIN   32
#define SIGRTMAX   NR_signals

#define SA_NOCLDSTOP   0x00000001
#define SA_NOCLDWAIT   0x00000002
#define SA_SIGINFO     0x00000004
#define SA_UNSUPPORTED 0x00000400
#define SA_ONSTACK     0x08000000
#define SA_RESTART     0x10000000
#define SA_NODEFER     0x40000000
#define SA_RESETHAND   0x80000000
#define SA_NOMASK      SA_NODEFER
#define SA_ONESHOT     SA_RESETHAND

#define SIG_BLOCK   0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

#define SIG_DFL ((sighandler_t) 0)
#define SIG_IGN ((sighandler_t) 1)
#define SIG_ERR ((sighandler_t) -1)

typedef void (*sighandler_t) (int);

typedef struct
{
  char sig[NR_signals / CHAR_BIT];
} sigset_t;

union sigval
{
  int sival_int;
  void *sival_ptr;
};

typedef struct
{
  int si_signo;
  int si_code;
  union sigval si_value;
  int si_errno;
  pid_t si_pid;
  uid_t si_uid;
  void *si_addr;
  int si_status;
  int si_band;
} siginfo_t;

union __sighandler
{
  sighandler_t sa_handler;
  void (*sa_sigaction) (int, siginfo_t *, void *);
};

struct sigaction
{
  union __sighandler __sa_handler;
  unsigned long sa_flags;
  sigset_t sa_mask;
};

#endif
