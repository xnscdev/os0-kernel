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

#include <bits/signal.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <limits.h>

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
  char sig[NSIG / CHAR_BIT];
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

extern const char *const sys_siglist[NSIG];
extern const char *const sys_signame[NSIG];

int sigemptyset (sigset_t *set);
int sigfillset (sigset_t *set);
int sigaddset (sigset_t *set, int sig);
int sigdelset (sigset_t *set, int sig);
int sigismember (const sigset_t *set, int sig);

__END_DECLS

#endif
