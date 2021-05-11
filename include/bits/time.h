/*************************************************************************
 * time.h -- This file is part of OS/0.                                  *
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

#ifndef _BITS_TIME_H
#define _BITS_TIME_H

#include <sys/types.h>

#ifndef _SYS_TIME_H
#error  "<bits/time.h> should not be included directly"
#endif

struct timespec
{
  time_t tv_sec;
  long tv_nsec;
};

struct itimerspec
{
  struct timespec it_interval;
  struct timespec it_value;
};

struct timeval
{
  time_t tv_sec;
  suseconds_t tv_usec;
};

struct itimerval
{
  struct timeval it_interval;
  struct timeval it_value;
};

struct timezone
{
  int tz_minuteswest;
  int tz_dsttime;
};

#endif
