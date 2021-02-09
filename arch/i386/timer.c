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

static uint32_t tick;
static int record;

void
timer_tick (void)
{
  if (record)
    tick++;
}

void
timer_set_freq (uint32_t freq)
{
  uint32_t div = TIMER_FREQ / freq;
  outb (0x36, TIMER_PORT_COMMAND);
  outb ((unsigned char) (div & 0xff), TIMER_PORT_CHANNEL0);
  outb ((unsigned char) ((div >> 8) & 0xff), TIMER_PORT_CHANNEL0);
}

void
msleep (uint32_t ms)
{
  record = 1;
  tick = 0;
  while (tick < ms)
    ;
  record = 0;
}
