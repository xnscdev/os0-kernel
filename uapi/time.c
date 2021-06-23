/*************************************************************************
 * time.c -- This file is part of OS/0.                                  *
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
#include <sys/timer.h>

time_t
sys_time (time_t *t)
{
  return time (t);
}

unsigned int
sys_alarm (unsigned int seconds)
{
  struct itimerval old;
  struct itimerval new;
  new.it_interval.tv_sec = 0;
  new.it_interval.tv_usec = 0;
  new.it_value.tv_sec = seconds;
  new.it_value.tv_usec = 0;
  return sys_setitimer (ITIMER_REAL, &new, &old) < 0 ? 0 : old.it_value.tv_sec;
}

clock_t
sys_times (struct tms *tms)
{
  Process *proc = &process_table[task_getpid ()];
  tms->tms_utime = proc->p_rusage.ru_utime.tv_sec * CLOCKS_PER_SEC +
    proc->p_rusage.ru_utime.tv_usec * CLOCKS_PER_SEC / 1000000;
  tms->tms_stime = proc->p_rusage.ru_stime.tv_sec * CLOCKS_PER_SEC +
    proc->p_rusage.ru_stime.tv_usec * CLOCKS_PER_SEC / 1000000;
  tms->tms_cutime = proc->p_cusage.ru_utime.tv_sec * CLOCKS_PER_SEC +
    proc->p_cusage.ru_utime.tv_usec * CLOCKS_PER_SEC / 1000000;
  tms->tms_cstime = proc->p_cusage.ru_stime.tv_sec * CLOCKS_PER_SEC +
    proc->p_cusage.ru_stime.tv_usec * CLOCKS_PER_SEC / 1000000;
  return time (NULL) * CLOCKS_PER_SEC;
}

int
sys_gettimeofday (struct timeval *__restrict tv, struct timezone *__restrict tz)
{
  if (tv != NULL)
    {
      tv->tv_sec = time (NULL);
      tv->tv_usec = 0;
    }
  if (tz != NULL)
    {
      tz->tz_minuteswest = 0;
      tz->tz_dsttime = 0;
    }
  return 0;
}

int
sys_setitimer (int which, const struct itimerval *__restrict new,
	       struct itimerval *__restrict old)
{
  Process *proc = &process_table[task_getpid ()];
  if (which < 0 || which >= __NR_itimers)
    return -EINVAL;
  if (old != NULL)
    memcpy (old, &proc->p_itimers[which], sizeof (struct itimerval));
  memcpy (&proc->p_itimers[which], new, sizeof (struct itimerval));
  return 0;
}

int
sys_getitimer (int which, struct itimerval *curr)
{
  Process *proc = &process_table[task_getpid ()];
  if (which < 0 || which >= __NR_itimers)
    return -EINVAL;
  memcpy (curr, &proc->p_itimers[which], sizeof (struct itimerval));
  return 0;
}

int
sys_nanosleep (const struct timespec *req, struct timespec *rem)
{
  msleep (req->tv_sec * 1000 + req->tv_nsec / 1000000);
  return 0;
}
