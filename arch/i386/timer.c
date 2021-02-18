/*************************************************************************
 * timer.c -- This file is part of OS/0.                                 *
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

#include <i386/timer.h>
#include <sys/io.h>
#include <sys/timer.h>
#include <sys/types.h>

extern time_t rtc_time;

static long tick;
static uint32_t timer_freq;

void
timer_tick (void)
{
  tick++;
  if (tick % timer_freq == 0)
    rtc_time++;
}

void
timer_set_freq (uint32_t freq)
{
  uint32_t div = TIMER_FREQ / freq;
  outb (0x36, TIMER_PORT_COMMAND);
  outb ((unsigned char) (div & 0xff), TIMER_PORT_CHANNEL0);
  outb ((unsigned char) ((div >> 8) & 0xff), TIMER_PORT_CHANNEL0);
  timer_freq = freq;
}

uint32_t
timer_get_freq (void)
{
  return timer_freq;
}

void
msleep (uint32_t ms)
{
  tick = 0;
  while (tick < ms)
    ;
}

long
timer_poll (void)
{
  return tick;
}
