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

#include <sys/cdefs.h>
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
#define SIGCLD    SIGCHLD
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
#define SIGUNUSED SIGSYS

#define NR_signals 48
#define SIGRTMIN   32
#define SIGRTMAX   NR_signals

/* Signal handler flags */

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

/* Reason for signal generation */

#define SI_USER    0x100
#define SI_KERNEL  0x101
#define SI_QUEUE   0x102
#define SI_TIMER   0x103
#define SI_ASYNCIO 0x104
#define SI_TKILL   0x105

#define ILL_ILLOPC  0x200
#define ILL_ILLOPN  0x201
#define ILL_ILLADDR 0x202
#define ILL_ILLTRP  0x203
#define ILL_PRVOPC  0x204
#define ILL_PRVREG  0x205
#define ILL_COPROC  0x206
#define ILL_BADSTK  0x207

#define FPE_INTDIV 0x300
#define FPE_INTOVF 0x301
#define FPE_FLTDIV 0x302
#define FPE_FLTOVF 0x303
#define FPE_FLTUND 0x304
#define FPE_FLTRES 0x305
#define FPE_FLTINV 0x306
#define FPE_FLTSUB 0x307

#define SEGV_MAPERR 0x400
#define SEGV_ACCERR 0x401

#define BUS_ADRALN 0x500
#define BUS_ADRERR 0x501
#define BUS_OBJERR 0x502

#define TRAP_BRKPT 0x600
#define TRAP_TRACE 0x601

#define CLD_EXITED    0x700
#define CLD_KILLED    0x701
#define CLD_DUMPED    0x702
#define CLD_TRAPPED   0x703
#define CLD_STOPPED   0x704
#define CLD_CONTINUED 0x705

#define POLL_IN  0x800
#define POLL_OUT 0x801
#define POLL_MSG 0x802
#define POLL_ERR 0x803
#define POLL_PRI 0x804
#define POLL_HUP 0x805

/* sigprocmask(2) actions */

#define SIG_BLOCK   0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

/* Signal handler special values */

#define SIG_DFL ((sig_t) 0)
#define SIG_IGN ((sig_t) 1)
#define SIG_ERR ((sig_t) -1)

typedef void (*sig_t) (int);
typedef int sig_atomic_t;

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

struct sigaction
{
  sig_t sa_handler;
  void (*sa_sigaction) (int, siginfo_t *, void *);
  unsigned long sa_flags;
  sigset_t sa_mask;
};

__BEGIN_DECLS

extern const char *const sys_signames[NR_signals];

int sigemptyset (sigset_t *set);
int sigfillset (sigset_t *set);
int sigaddset (sigset_t *set, int sig);
int sigdelset (sigset_t *set, int sig);
int sigismember (const sigset_t *set, int sig);

__END_DECLS

#endif
