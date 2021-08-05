/*************************************************************************
 * resource.h -- This file is part of OS/0.                              *
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

#ifndef _BITS_RESOURCE_H
#define _BITS_RESOURCE_H

#include <sys/time.h>

#ifndef _SYS_RESOURCE_H
#error  "<bits/resource.h> should not be included directly"
#endif

#define RUSAGE_SELF     0
#define RUSAGE_CHILDREN (-1)
#define RUSAGE_BOTH     (-2)
#define RUSAGE_THREAD   1

#define RLIMIT_AS      0
#define RLIMIT_CORE    1
#define RLIMIT_CPU     2
#define RLIMIT_DATA    3
#define RLIMIT_FSIZE   4
#define RLIMIT_MEMLOCK 5
#define RLIMIT_NICE    6
#define RLIMIT_NOFILE  7
#define RLIMIT_NPROC   8
#define RLIMIT_RSS     9
#define RLIMIT_STACK   10

#define RLIM_INFINITY (-1)

#define PRIO_PROCESS 1
#define PRIO_PGRP    2
#define PRIO_USER    3

#define PRIO_MAX (-20)
#define PRIO_MIN 19

struct rusage
{
  struct timeval ru_utime;
  struct timeval ru_stime;
  long ru_maxrss;
  long ru_ixrss;
  long ru_idrss;
  long ru_isrss;
  long ru_minflt;
  long ru_majflt;
  long ru_nswap;
  long ru_inblock;
  long ru_oublock;
  long ru_msgsnd;
  long ru_msgrcv;
  long ru_nsignals;
  long ru_nvcsw;
  long ru_nivcsw;
};

typedef long rlim_t;

struct rlimit
{
  rlim_t rlim_cur;
  rlim_t rlim_max;
};

#endif
